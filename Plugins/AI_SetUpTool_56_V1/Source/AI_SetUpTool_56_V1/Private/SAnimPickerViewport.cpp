#include "SAnimPickerViewport.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"

// ============================================================================
// FAnimPickerViewportClient Implementation
// ============================================================================

FAnimPickerViewportClient::FAnimPickerViewportClient(FAdvancedPreviewScene* InPreviewScene)
	: FEditorViewportClient(nullptr, InPreviewScene)
	, PreviewScene(InPreviewScene)
	, PreviewMeshComponent(nullptr)
	, HoveredControllerName(NAME_None)
	, HoveredMouseX(0)
	, HoveredMouseY(0)
{
	// 카메라 초기 설정
	SetViewLocation(FVector(0, 200, 100));
	SetViewRotation(FRotator(-15, -90, 0));
	
	// 뷰포트 설정
	SetRealtime(true);
	SetShowStats(false);
	
	// 그리드 표시 설정
	DrawHelper.bDrawGrid = true;
	DrawHelper.GridColorAxis = FColor(70, 70, 70);
	DrawHelper.GridColorMajor = FColor(40, 40, 40);
	DrawHelper.GridColorMinor = FColor(20, 20, 20);
}

FAnimPickerViewportClient::~FAnimPickerViewportClient()
{
	ControllerData.Empty();
	SelectedControllers.Empty();
	
	if (PreviewScene)
	{
		if (PreviewMeshComponent && IsValid(PreviewMeshComponent))
		{
			PreviewScene->RemoveComponent(PreviewMeshComponent);
		}
		PreviewMeshComponent = nullptr;
	}
	
	PreviewScene = nullptr;
}

void FAnimPickerViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	
	if (PreviewScene)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
	
	// ★★★ 매 틱마다 마우스 호버 체크 (OnMouseMove가 안 불릴 수 있어서) ★★★
	if (Viewport && ControllerData.Num() > 0)
	{
		FIntPoint MousePos;
		Viewport->GetMousePos(MousePos);
		
		// 뷰포트 내에 있는지 확인
		FIntPoint ViewportSize = Viewport->GetSizeXY();
		if (MousePos.X >= 0 && MousePos.X < ViewportSize.X && 
			MousePos.Y >= 0 && MousePos.Y < ViewportSize.Y)
		{
			FName NewHovered = GetHoveredController(MousePos.X, MousePos.Y);
			if (NewHovered != HoveredControllerName)
			{
				HoveredControllerName = NewHovered;
				HoveredMouseX = MousePos.X;
				HoveredMouseY = MousePos.Y;
				Invalidate();
			}
			else if (NewHovered != NAME_None)
			{
				// 같은 컨트롤러 위에 있어도 마우스 위치 업데이트 (툴팁 위치용)
				HoveredMouseX = MousePos.X;
				HoveredMouseY = MousePos.Y;
			}
		}
		else
		{
			// 뷰포트 밖이면 호버 해제
			if (HoveredControllerName != NAME_None)
			{
				HoveredControllerName = NAME_None;
				Invalidate();
			}
		}
	}
}

// ★ 헬퍼: 박스 와이어프레임 그리기 ★
static void DrawWireframeBox(FPrimitiveDrawInterface* PDI, const FVector& Location, const FVector& Extent, const FQuat& Rotation, const FColor& Color, float Thickness)
{
	FVector Corners[8];
	FVector LocalCorners[8] = {
		FVector(-Extent.X, -Extent.Y, -Extent.Z),
		FVector(+Extent.X, -Extent.Y, -Extent.Z),
		FVector(+Extent.X, +Extent.Y, -Extent.Z),
		FVector(-Extent.X, +Extent.Y, -Extent.Z),
		FVector(-Extent.X, -Extent.Y, +Extent.Z),
		FVector(+Extent.X, -Extent.Y, +Extent.Z),
		FVector(+Extent.X, +Extent.Y, +Extent.Z),
		FVector(-Extent.X, +Extent.Y, +Extent.Z)
	};
	
	for (int32 i = 0; i < 8; i++)
	{
		Corners[i] = Location + Rotation.RotateVector(LocalCorners[i]);
	}
	
	// 12개 엣지
	PDI->DrawLine(Corners[0], Corners[1], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[1], Corners[2], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[2], Corners[3], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[3], Corners[0], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[4], Corners[5], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[5], Corners[6], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[6], Corners[7], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[7], Corners[4], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[0], Corners[4], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[1], Corners[5], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[2], Corners[6], Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Corners[3], Corners[7], Color, SDPG_Foreground, Thickness);
}

// ★ 헬퍼: 원/구 와이어프레임 그리기 ★
static void DrawWireframeSphere(FPrimitiveDrawInterface* PDI, const FVector& Location, float Radius, const FQuat& Rotation, const FColor& Color, float Thickness, int32 Segments = 16)
{
	// XY 평면 원
	for (int32 i = 0; i < Segments; i++)
	{
		float Angle1 = (float)i / Segments * 2.0f * PI;
		float Angle2 = (float)(i + 1) / Segments * 2.0f * PI;
		FVector P1 = Location + Rotation.RotateVector(FVector(FMath::Cos(Angle1) * Radius, FMath::Sin(Angle1) * Radius, 0));
		FVector P2 = Location + Rotation.RotateVector(FVector(FMath::Cos(Angle2) * Radius, FMath::Sin(Angle2) * Radius, 0));
		PDI->DrawLine(P1, P2, Color, SDPG_Foreground, Thickness);
	}
	// XZ 평면 원
	for (int32 i = 0; i < Segments; i++)
	{
		float Angle1 = (float)i / Segments * 2.0f * PI;
		float Angle2 = (float)(i + 1) / Segments * 2.0f * PI;
		FVector P1 = Location + Rotation.RotateVector(FVector(FMath::Cos(Angle1) * Radius, 0, FMath::Sin(Angle1) * Radius));
		FVector P2 = Location + Rotation.RotateVector(FVector(FMath::Cos(Angle2) * Radius, 0, FMath::Sin(Angle2) * Radius));
		PDI->DrawLine(P1, P2, Color, SDPG_Foreground, Thickness);
	}
	// YZ 평면 원
	for (int32 i = 0; i < Segments; i++)
	{
		float Angle1 = (float)i / Segments * 2.0f * PI;
		float Angle2 = (float)(i + 1) / Segments * 2.0f * PI;
		FVector P1 = Location + Rotation.RotateVector(FVector(0, FMath::Cos(Angle1) * Radius, FMath::Sin(Angle1) * Radius));
		FVector P2 = Location + Rotation.RotateVector(FVector(0, FMath::Cos(Angle2) * Radius, FMath::Sin(Angle2) * Radius));
		PDI->DrawLine(P1, P2, Color, SDPG_Foreground, Thickness);
	}
}

// ★ 헬퍼: 원 (Circle) 그리기 - 하나의 평면 원만 ★
static void DrawWireframeCircle(FPrimitiveDrawInterface* PDI, const FVector& Location, float Radius, const FQuat& Rotation, const FColor& Color, float Thickness, int32 Segments = 24)
{
	for (int32 i = 0; i < Segments; i++)
	{
		float Angle1 = (float)i / Segments * 2.0f * PI;
		float Angle2 = (float)(i + 1) / Segments * 2.0f * PI;
		FVector P1 = Location + Rotation.RotateVector(FVector(FMath::Cos(Angle1) * Radius, FMath::Sin(Angle1) * Radius, 0));
		FVector P2 = Location + Rotation.RotateVector(FVector(FMath::Cos(Angle2) * Radius, FMath::Sin(Angle2) * Radius, 0));
		PDI->DrawLine(P1, P2, Color, SDPG_Foreground, Thickness);
	}
}

// ★ 헬퍼: 다이아몬드/마름모 그리기 (FK/IK 스위치용) ★
static void DrawWireframeDiamond(FPrimitiveDrawInterface* PDI, const FVector& Location, float Size, const FQuat& Rotation, const FColor& Color, float Thickness)
{
	FVector Top = Location + Rotation.RotateVector(FVector(0, 0, Size));
	FVector Bottom = Location + Rotation.RotateVector(FVector(0, 0, -Size));
	FVector Front = Location + Rotation.RotateVector(FVector(Size, 0, 0));
	FVector Back = Location + Rotation.RotateVector(FVector(-Size, 0, 0));
	FVector Left = Location + Rotation.RotateVector(FVector(0, -Size, 0));
	FVector Right = Location + Rotation.RotateVector(FVector(0, Size, 0));
	
	// 상단 피라미드
	PDI->DrawLine(Top, Front, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Top, Back, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Top, Left, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Top, Right, Color, SDPG_Foreground, Thickness);
	// 하단 피라미드
	PDI->DrawLine(Bottom, Front, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Back, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Left, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Right, Color, SDPG_Foreground, Thickness);
	// 중간 사각형
	PDI->DrawLine(Front, Right, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Right, Back, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Back, Left, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Left, Front, Color, SDPG_Foreground, Thickness);
}

// ★ PDI로 Shape 모양별 그리기 ★
void FAnimPickerViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);
	
	if (!PDI || ControllerData.Num() == 0)
	{
		return;
	}
	
	// 모든 컨트롤러 그리기
	for (const auto& Data : ControllerData)
	{
		FVector Location = Data.Transform.GetLocation();
		FVector Extent = Data.BoxExtent;
		FQuat Rotation = Data.Transform.GetRotation();
		FString ShapeStr = Data.ShapeName.ToString().ToLower();
		
		FColor DrawColor = Data.Color.ToFColor(true);
		bool bIsSelected = SelectedControllers.Contains(Data.ControlName);
		bool bIsHovered = (Data.ControlName == HoveredControllerName);
		
		// Shape 두께 결정
		float Thickness = 1.0f;
		if (ShapeStr.Contains(TEXT("thick")))
		{
			Thickness = 2.5f;
		}
		else if (ShapeStr.Contains(TEXT("solid")))
		{
			Thickness = 2.0f;
		}
		
		// ★★★ Shape 모양별 그리기 ★★★
		FString CtrlNameLower = Data.ControlName.ToString().ToLower();
		
		if (CtrlNameLower.Contains(TEXT("switch")) || ShapeStr.Contains(TEXT("gizmo")))
		{
			// FK/IK 스위치 & Gizmo: 다이아몬드 모양 (눈에 띄게!)
			float Size = FMath::Max3(Extent.X, Extent.Y, Extent.Z) * 1.5f;
			DrawWireframeDiamond(PDI, Location, Size, Rotation, DrawColor, Thickness + 1.5f);
		}
		else if (ShapeStr.Contains(TEXT("sphere")))
		{
			// 구
			float Radius = FMath::Max3(Extent.X, Extent.Y, Extent.Z);
			DrawWireframeSphere(PDI, Location, Radius, Rotation, DrawColor, Thickness, 16);
		}
		else if (ShapeStr.Contains(TEXT("circle")) || ShapeStr.Contains(TEXT("hexagon")) || ShapeStr.Contains(TEXT("octagon")))
		{
			// 원/육각형/팔각형 -> 원으로 표시
			float Radius = FMath::Max(Extent.X, Extent.Y);
			DrawWireframeCircle(PDI, Location, Radius, Rotation, DrawColor, Thickness, 24);
		}
		else if (ShapeStr.Contains(TEXT("arrow")))
		{
			// 화살표 -> 다이아몬드로 표시
			float Size = FMath::Max3(Extent.X, Extent.Y, Extent.Z);
			DrawWireframeDiamond(PDI, Location, Size, Rotation, DrawColor, Thickness);
		}
		else
		{
			// 박스 (Box, Square, RoundedSquare, Default 등)
			DrawWireframeBox(PDI, Location, Extent, Rotation, DrawColor, Thickness);
		}
		
		// ★ 선택/호버 시 아웃라인 추가 ★
		if (bIsSelected || bIsHovered)
		{
			FColor OutlineColor = bIsSelected ? FColor(255, 165, 0) : FColor(255, 255, 100);
			float OutlineThickness = 2.5f;
			FVector OutlineExtent = Extent * 1.08f;
			
			if (CtrlNameLower.Contains(TEXT("switch")) || ShapeStr.Contains(TEXT("gizmo")))
			{
				float Size = FMath::Max3(OutlineExtent.X, OutlineExtent.Y, OutlineExtent.Z) * 1.5f;
				DrawWireframeDiamond(PDI, Location, Size, Rotation, OutlineColor, OutlineThickness);
			}
			else if (ShapeStr.Contains(TEXT("sphere")))
			{
				float Radius = FMath::Max3(OutlineExtent.X, OutlineExtent.Y, OutlineExtent.Z);
				DrawWireframeSphere(PDI, Location, Radius, Rotation, OutlineColor, OutlineThickness, 16);
			}
			else if (ShapeStr.Contains(TEXT("circle")) || ShapeStr.Contains(TEXT("hexagon")) || ShapeStr.Contains(TEXT("octagon")))
			{
				float Radius = FMath::Max(OutlineExtent.X, OutlineExtent.Y);
				DrawWireframeCircle(PDI, Location, Radius, Rotation, OutlineColor, OutlineThickness, 24);
			}
			else if (ShapeStr.Contains(TEXT("arrow")))
			{
				float Size = FMath::Max3(OutlineExtent.X, OutlineExtent.Y, OutlineExtent.Z);
				DrawWireframeDiamond(PDI, Location, Size, Rotation, OutlineColor, OutlineThickness);
			}
			else
			{
				DrawWireframeBox(PDI, Location, OutlineExtent, Rotation, OutlineColor, OutlineThickness);
			}
		}
	}
}

void FAnimPickerViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);
	
	// ★ 호버된 컨트롤러 툴팁 표시 ★
	if (HoveredControllerName != NAME_None)
	{
		FString TooltipText = HoveredControllerName.ToString();
		
		UFont* Font = GEngine->GetSmallFont();
		if (Font)
		{
			float BoxX = FMath::Clamp((float)HoveredMouseX + 15.0f, 10.0f, InViewport.GetSizeXY().X - 200.0f);
			float BoxY = FMath::Clamp((float)HoveredMouseY - 10.0f, 10.0f, InViewport.GetSizeXY().Y - 30.0f);
			float Padding = 6.0f;
			float TextWidth = TooltipText.Len() * 8.0f;
			float TextHeight = 16.0f;
			
			// 배경 (반투명 검정)
			FCanvasTileItem BackgroundTile(
				FVector2D(BoxX - Padding, BoxY - Padding),
				FVector2D(TextWidth + Padding * 2, TextHeight + Padding * 2),
				FLinearColor(0.0f, 0.0f, 0.0f, 0.9f)
			);
			Canvas.DrawItem(BackgroundTile);
			
			// 테두리
			FCanvasTileItem BorderTile(
				FVector2D(BoxX - Padding - 1, BoxY - Padding - 1),
				FVector2D(TextWidth + Padding * 2 + 2, TextHeight + Padding * 2 + 2),
				FLinearColor(1.0f, 1.0f, 0.3f, 1.0f)
			);
			BorderTile.BlendMode = SE_BLEND_Translucent;
			// Canvas.DrawItem(BorderTile);  // 테두리는 선택적
			
			// 텍스트 (밝은 노랑)
			FCanvasTextItem TextItem(
				FVector2D(BoxX, BoxY),
				FText::FromString(TooltipText),
				Font,
				FLinearColor(1.0f, 1.0f, 0.5f)
			);
			TextItem.Scale = FVector2D(1.2f, 1.2f);
			Canvas.DrawItem(TextItem);
		}
	}
}

void FAnimPickerViewportClient::SetHoveredController(FName InName, int32 InMouseX, int32 InMouseY)
{
	if (HoveredControllerName != InName)
	{
		HoveredControllerName = InName;
		HoveredMouseX = InMouseX;
		HoveredMouseY = InMouseY;
		Invalidate();
	}
	else
	{
		HoveredMouseX = InMouseX;
		HoveredMouseY = InMouseY;
	}
}

void FAnimPickerViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	FName ClickedController = GetHoveredController(HitX, HitY);
	
	if (ClickedController != NAME_None)
	{
		bool bCtrlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();
		
		if (OnMarkerClicked.IsBound())
		{
			OnMarkerClicked.Execute(ClickedController, bCtrlDown);
		}
		
		UE_LOG(LogTemp, Log, TEXT("[AnimPicker 3D] Clicked: %s (Ctrl=%d)"), *ClickedController.ToString(), bCtrlDown ? 1 : 0);
		Invalidate();
		return;
	}
	
	FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
}

void FAnimPickerViewportClient::TrackingStarted(const FInputEventState& InInputState, bool bIsDragging, bool bNudge)
{
	FEditorViewportClient::TrackingStarted(InInputState, bIsDragging, bNudge);
}

FName FAnimPickerViewportClient::GetHoveredController(int32 MouseX, int32 MouseY)
{
	if (!PreviewScene || ControllerData.Num() == 0 || !Viewport)
	{
		return NAME_None;
	}
	
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Viewport,
		GetScene(),
		EngineShowFlags)
		.SetRealtimeUpdate(true));
	
	FSceneView* View = CalcSceneView(&ViewFamily);
	if (!View)
	{
		return NAME_None;
	}
	
	float MinDist = 40.0f;  // 40픽셀 이내
	FName ClosestController = NAME_None;
	
	for (const auto& Data : ControllerData)
	{
		FVector WorldPos = Data.Transform.GetLocation();
		FVector2D ScreenPos;
		
		if (View->WorldToPixel(WorldPos, ScreenPos))
		{
			float Dist = FVector2D::Distance(ScreenPos, FVector2D(MouseX, MouseY));
			if (Dist < MinDist)
			{
				MinDist = Dist;
				ClosestController = Data.ControlName;
			}
		}
	}
	
	return ClosestController;
}

void FAnimPickerViewportClient::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	ClearMesh();
	
	if (!InMesh || !PreviewScene)
	{
		return;
	}
	
	PreviewMeshComponent = NewObject<USkeletalMeshComponent>();
	PreviewMeshComponent->SetSkeletalMesh(InMesh);
	PreviewMeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	
	// ★ 콜리전 완전 비활성화 - 피커 선택 방해 안 함 ★
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMeshComponent->SetCastShadow(false);
	PreviewMeshComponent->SetRenderCustomDepth(false);
	
	// 어두운 회색 머티리얼 (배경용)
	UMaterial* BaseMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	if (BaseMat)
	{
		for (int32 i = 0; i < PreviewMeshComponent->GetNumMaterials(); i++)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, PreviewMeshComponent);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.12f, 0.12f, 0.15f));
				DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
				DynMat->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
				PreviewMeshComponent->SetMaterial(i, DynMat);
			}
		}
	}
	
	PreviewScene->AddComponent(PreviewMeshComponent, FTransform::Identity);
	FocusOnMesh();
	
	UE_LOG(LogTemp, Log, TEXT("[AnimPicker 3D] Set skeletal mesh (collision disabled)"));
}

void FAnimPickerViewportClient::ClearMesh()
{
	if (PreviewMeshComponent && IsValid(PreviewMeshComponent) && PreviewScene)
	{
		PreviewScene->RemoveComponent(PreviewMeshComponent);
	}
	PreviewMeshComponent = nullptr;
}

void FAnimPickerViewportClient::SetControllerMarkers(const TArray<TPair<FName, FTransform>>& Controllers, const TMap<FName, FLinearColor>& Colors)
{
	ClearControllerMarkers();
	
	// 메쉬 높이 기준 스케일 계산
	float GlobalScale = 1.0f;
	if (PreviewMeshComponent && IsValid(PreviewMeshComponent))
	{
		FBoxSphereBounds Bounds = PreviewMeshComponent->Bounds;
		float MeshHeight = Bounds.BoxExtent.Z * 2.0f;
		GlobalScale = MeshHeight / 180.0f;
		GlobalScale = FMath::Clamp(GlobalScale, 0.5f, 3.0f);
	}
	
	for (const auto& Controller : Controllers)
	{
		const FName& ControlName = Controller.Key;
		FTransform Transform = Controller.Value;
		FString CtrlStr = ControlName.ToString().ToLower();
		
		FControllerMarkerData Data;
		Data.ControlName = ControlName;
		Data.Transform = Transform;
		
		// ★ 본 타입별 박스 크기 설정 ★
		FVector BoxExtent(3.0f, 3.0f, 3.0f);
		FLinearColor Color = FLinearColor(0.5f, 0.5f, 0.5f);
		
		// 머리
		if (CtrlStr.Contains(TEXT("head")))
		{
			BoxExtent = FVector(6.0f, 6.0f, 7.0f);
			Color = FLinearColor(1.0f, 0.9f, 0.3f);
		}
		// 목
		else if (CtrlStr.Contains(TEXT("neck")))
		{
			BoxExtent = FVector(2.5f, 2.5f, 4.0f);
			Color = FLinearColor(0.6f, 0.9f, 0.6f);
		}
		// 쇄골
		else if (CtrlStr.Contains(TEXT("clavicle")))
		{
			BoxExtent = FVector(5.0f, 2.5f, 2.5f);
			Color = FLinearColor(0.3f, 0.7f, 0.3f);
		}
		// 상완
		else if (CtrlStr.Contains(TEXT("upperarm")))
		{
			BoxExtent = FVector(8.0f, 3.0f, 3.0f);
			Color = CtrlStr.Contains(TEXT("_l")) ? FLinearColor(0.3f, 0.5f, 1.0f) : FLinearColor(1.0f, 0.4f, 0.3f);
		}
		// 전완
		else if (CtrlStr.Contains(TEXT("lowerarm")) || CtrlStr.Contains(TEXT("forearm")))
		{
			BoxExtent = FVector(7.0f, 2.5f, 2.5f);
			Color = CtrlStr.Contains(TEXT("_l")) ? FLinearColor(0.4f, 0.6f, 1.0f) : FLinearColor(1.0f, 0.5f, 0.4f);
		}
		// 손
		else if (CtrlStr.Contains(TEXT("hand")) && !CtrlStr.Contains(TEXT("thumb")) && !CtrlStr.Contains(TEXT("index")) && !CtrlStr.Contains(TEXT("middle")) && !CtrlStr.Contains(TEXT("ring")) && !CtrlStr.Contains(TEXT("pinky")))
		{
			BoxExtent = FVector(3.5f, 4.5f, 1.5f);
			Color = CtrlStr.Contains(TEXT("_l")) ? FLinearColor(0.5f, 0.7f, 1.0f) : FLinearColor(1.0f, 0.6f, 0.5f);
		}
		// 척추
		else if (CtrlStr.Contains(TEXT("spine")))
		{
			BoxExtent = FVector(4.0f, 8.0f, 5.0f);
			Color = FLinearColor(0.3f, 0.9f, 0.9f);
		}
		// 골반
		else if (CtrlStr.Contains(TEXT("pelvis")) || CtrlStr.Contains(TEXT("hips")))
		{
			BoxExtent = FVector(4.0f, 10.0f, 5.0f);
			Color = FLinearColor(1.0f, 0.85f, 0.3f);
		}
		// 허벅지
		else if (CtrlStr.Contains(TEXT("thigh")))
		{
			BoxExtent = FVector(3.5f, 3.5f, 12.0f);
			Color = CtrlStr.Contains(TEXT("_l")) ? FLinearColor(0.3f, 0.5f, 1.0f) : FLinearColor(1.0f, 0.4f, 0.3f);
		}
		// 정강이
		else if (CtrlStr.Contains(TEXT("calf")) || CtrlStr.Contains(TEXT("shin")))
		{
			BoxExtent = FVector(2.5f, 2.5f, 10.0f);
			Color = CtrlStr.Contains(TEXT("_l")) ? FLinearColor(0.4f, 0.6f, 1.0f) : FLinearColor(1.0f, 0.5f, 0.4f);
		}
		// 발
		else if (CtrlStr.Contains(TEXT("foot")) && !CtrlStr.Contains(TEXT("toe")))
		{
			BoxExtent = FVector(6.0f, 3.0f, 2.0f);
			Color = CtrlStr.Contains(TEXT("_l")) ? FLinearColor(0.5f, 0.7f, 1.0f) : FLinearColor(1.0f, 0.6f, 0.5f);
		}
		// 손가락
		else if (CtrlStr.Contains(TEXT("thumb")) || CtrlStr.Contains(TEXT("index")) || CtrlStr.Contains(TEXT("middle")) || CtrlStr.Contains(TEXT("ring")) || CtrlStr.Contains(TEXT("pinky")))
		{
			BoxExtent = FVector(0.6f, 0.6f, 1.2f);
			Color = CtrlStr.Contains(TEXT("_l")) ? FLinearColor(0.6f, 0.8f, 1.0f) : FLinearColor(1.0f, 0.7f, 0.6f);
		}
		// 발가락/볼
		else if (CtrlStr.Contains(TEXT("toe")) || CtrlStr.Contains(TEXT("ball")))
		{
			BoxExtent = FVector(0.8f, 1.2f, 0.6f);
			Color = CtrlStr.Contains(TEXT("_l")) ? FLinearColor(0.6f, 0.8f, 1.0f) : FLinearColor(1.0f, 0.7f, 0.6f);
		}
		// IK
		else if (CtrlStr.Contains(TEXT("_ik")) || CtrlStr.Contains(TEXT("ik_")))
		{
			BoxExtent = FVector(3.0f, 3.0f, 3.0f);
			Color = FLinearColor(1.0f, 1.0f, 0.3f);
		}
		// PV (Pole Vector)
		else if (CtrlStr.Contains(TEXT("_pv")) || CtrlStr.Contains(TEXT("pv_")))
		{
			BoxExtent = FVector(2.0f, 2.0f, 2.0f);
			Color = FLinearColor(1.0f, 0.5f, 1.0f);
		}
		// 기타 (세컨더리 등)
		else
		{
			BoxExtent = FVector(2.0f, 2.0f, 2.0f);
			if (Colors.Contains(ControlName))
			{
				Color = Colors[ControlName];
			}
			else
			{
				Color = FLinearColor(0.7f, 0.5f, 0.8f);  // 보라
			}
		}
		
		// 글로벌 스케일 적용
		Data.BoxExtent = BoxExtent * GlobalScale;
		Data.Color = Color;
		
		ControllerData.Add(Data);
	}
	
	// ★ 최초 로드 시에만 FocusOnMesh 호출 ★
	Invalidate();
	
	UE_LOG(LogTemp, Log, TEXT("[AnimPicker 3D] Created %d controller markers (PDI)"), ControllerData.Num());
}

// ★★★ Control Rig Shape 정보를 사용해서 마커 생성 ★★★
void FAnimPickerViewportClient::SetControllerMarkersWithShapeInfo(const TArray<TPair<FName, FTransform>>& Controllers, const TMap<FName, FLinearColor>& Colors, const TMap<FName, FVector>& ShapeScales, const TMap<FName, FName>& ShapeNames)
{
	ClearControllerMarkers();
	
	for (const auto& Controller : Controllers)
	{
		const FName& ControlName = Controller.Key;
		FTransform Transform = Controller.Value;
		FString CtrlStr = ControlName.ToString().ToLower();
		
		FControllerMarkerData Data;
		Data.ControlName = ControlName;
		Data.Transform = Transform;
		
		// ★ Control Rig의 Shape 스케일 가져오기 ★
		FVector ShapeScale = FVector(1.0f);
		if (ShapeScales.Contains(ControlName))
		{
			ShapeScale = ShapeScales[ControlName];
			Data.bUseShapeScale = true;
			Data.ShapeScale = ShapeScale;
		}
		
		// ★ Control Rig의 Shape 이름 가져오기 ★
		if (ShapeNames.Contains(ControlName))
		{
			Data.ShapeName = ShapeNames[ControlName];
		}
		else
		{
			Data.ShapeName = FName(TEXT("Box_Thin"));  // 기본값
		}
		
		// ★★★ ShapeScale 값을 실제 크기로 변환 ★★★
		// Control Rig ShapeTransform의 Scale3D 값 사용
		const float ScaleMultiplier = 2.5f;  // 곱셈 계수
		FVector BoxExtent = ShapeScale * ScaleMultiplier;
		
		// 최소/최대 크기 제한
		BoxExtent.X = FMath::Clamp(BoxExtent.X, 1.0f, 25.0f);
		BoxExtent.Y = FMath::Clamp(BoxExtent.Y, 1.0f, 25.0f);
		BoxExtent.Z = FMath::Clamp(BoxExtent.Z, 1.0f, 25.0f);
		
		Data.BoxExtent = BoxExtent;
		
		UE_LOG(LogTemp, Warning, TEXT("[3D Marker] %s: Shape=%s, Scale=(%.2f,%.2f,%.2f) -> Extent=(%.2f,%.2f,%.2f)"),
			*ControlName.ToString(), *Data.ShapeName.ToString(), ShapeScale.X, ShapeScale.Y, ShapeScale.Z, BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
		
		// ★ Control Rig의 원본 색상 사용 ★
		if (Colors.Contains(ControlName))
		{
			Data.Color = Colors[ControlName];
		}
		else
		{
			Data.Color = FLinearColor(0.7f, 0.7f, 0.7f);  // 기본 회색
		}
		
		ControllerData.Add(Data);
		
		// 디버그 로그 (처음 10개)
		static int32 DebugCount = 0;
		if (DebugCount < 10)
		{
			FVector Loc = Transform.GetLocation();
			UE_LOG(LogTemp, Warning, TEXT("[AnimPicker 3D] '%s': Pos=(%.1f,%.1f,%.1f) ShapeScale=(%.2f,%.2f,%.2f) Color=(%.2f,%.2f,%.2f)"),
				*ControlName.ToString(),
				Loc.X, Loc.Y, Loc.Z,
				ShapeScale.X, ShapeScale.Y, ShapeScale.Z,
				Data.Color.R, Data.Color.G, Data.Color.B);
			DebugCount++;
		}
	}
	
	// 최초 로드 시 포커스
	static bool bFirstFocus = true;
	if (bFirstFocus && ControllerData.Num() > 0)
	{
		FocusOnMesh();
		bFirstFocus = false;
	}
	
	Invalidate();
	
	UE_LOG(LogTemp, Log, TEXT("[AnimPicker 3D] Created %d controller markers (using Control Rig Shape scale directly)"), ControllerData.Num());
}

void FAnimPickerViewportClient::ClearControllerMarkers()
{
	ControllerData.Empty();
}

void FAnimPickerViewportClient::SetSelectedControllers(const TSet<FName>& Selected)
{
	SelectedControllers = Selected;
	Invalidate();
}

void FAnimPickerViewportClient::FocusOnMesh()
{
	if (PreviewMeshComponent && IsValid(PreviewMeshComponent))
	{
		FBoxSphereBounds Bounds = PreviewMeshComponent->Bounds;
		FVector Center = Bounds.Origin;
		float Radius = Bounds.SphereRadius;
		
		SetViewLocation(Center + FVector(0, Radius * 2.0f, Radius * 0.4f));
		SetLookAtLocation(Center);
	}
	else if (ControllerData.Num() > 0)
	{
		FBox BoundingBox(ForceInit);
		for (const auto& Data : ControllerData)
		{
			BoundingBox += Data.Transform.GetLocation();
		}
		
		FVector Center = BoundingBox.GetCenter();
		float Radius = FVector::Dist(BoundingBox.Min, BoundingBox.Max) * 0.5f;
		Radius = FMath::Max(Radius, 50.0f);
		
		SetViewLocation(Center + FVector(0, Radius * 2.0f, Radius * 0.4f));
		SetLookAtLocation(Center);
	}
}

// ============================================================================
// SAnimPickerViewport Implementation
// ============================================================================

void SAnimPickerViewport::Construct(const FArguments& InArgs)
{
	FPreviewScene::ConstructionValues CVS;
	CVS.bDefaultLighting = true;
	CVS.LightBrightness = 3.0f;  // 밝게
	
	PreviewScene = MakeShared<FAdvancedPreviewScene>(CVS);
	PreviewScene->SetFloorVisibility(false);
	PreviewScene->SetEnvironmentVisibility(false);
	PreviewScene->SetSkyBrightness(0.5f);
	
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SAnimPickerViewport::~SAnimPickerViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->ClearMesh();
		ViewportClient->ClearControllerMarkers();
		ViewportClient.Reset();
	}
	
	PreviewScene.Reset();
}

TSharedRef<FEditorViewportClient> SAnimPickerViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FAnimPickerViewportClient(PreviewScene.Get()));
	ViewportClient->OnMarkerClicked.BindRaw(this, &SAnimPickerViewport::HandleMarkerClicked);
	
	return ViewportClient.ToSharedRef();
}

void SAnimPickerViewport::HandleMarkerClicked(FName ControlName, bool bCtrlDown)
{
	if (OnControllerClicked.IsBound())
	{
		OnControllerClicked.Execute(ControlName, bCtrlDown);
	}
}

void SAnimPickerViewport::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetSkeletalMesh(InMesh);
	}
}

void SAnimPickerViewport::ClearMesh()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->ClearMesh();
	}
}

void SAnimPickerViewport::SetControllerMarkers(const TArray<TPair<FName, FTransform>>& Controllers, const TMap<FName, FLinearColor>& Colors)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetControllerMarkers(Controllers, Colors);
	}
}

void SAnimPickerViewport::SetControllerMarkersWithShapeInfo(const TArray<TPair<FName, FTransform>>& Controllers, const TMap<FName, FLinearColor>& Colors, const TMap<FName, FVector>& ShapeScales, const TMap<FName, FName>& ShapeNames)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetControllerMarkersWithShapeInfo(Controllers, Colors, ShapeScales, ShapeNames);
	}
}

void SAnimPickerViewport::ClearControllerMarkers()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->ClearControllerMarkers();
	}
}

void SAnimPickerViewport::SetSelectedControllers(const TSet<FName>& Selected)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetSelectedControllers(Selected);
	}
}

FReply SAnimPickerViewport::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = SEditorViewport::OnMouseMove(MyGeometry, MouseEvent);
	
	UpdateHoverTooltip(MyGeometry, MouseEvent);
	
	return Reply;
}

void SAnimPickerViewport::UpdateHoverTooltip(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!ViewportClient.IsValid())
	{
		return;
	}
	
	FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	int32 MouseX = FMath::RoundToInt(LocalPos.X);
	int32 MouseY = FMath::RoundToInt(LocalPos.Y);
	
	FName HoveredController = ViewportClient->GetHoveredController(MouseX, MouseY);
	
	// ★ 툴팁 상태 변경 시에만 로그 ★
	if (HoveredController != LastHoveredController)
	{
		if (HoveredController != NAME_None)
		{
			UE_LOG(LogTemp, Log, TEXT("[AnimPicker] Hover: %s at (%d, %d)"), *HoveredController.ToString(), MouseX, MouseY);
		}
	}
	
	ViewportClient->SetHoveredController(HoveredController, MouseX, MouseY);
	LastHoveredController = HoveredController;
}

