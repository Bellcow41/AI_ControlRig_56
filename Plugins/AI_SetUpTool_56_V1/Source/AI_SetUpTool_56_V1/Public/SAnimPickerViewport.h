#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UControlRigBlueprint;

// ★ 컨트롤러 마커 데이터 (PDI로 직접 그리기용) ★
struct FControllerMarkerData
{
	FName ControlName;
	FTransform Transform;
	FVector BoxExtent;
	FLinearColor Color;
	FVector ShapeScale;          // Control Rig의 Shape 스케일
	FName ShapeName;             // Shape 모양 (Box, Sphere, Circle 등)
	bool bUseShapeScale = false; // Shape 스케일 사용 여부
};

// ============================================================================
// Anim Picker 3D Viewport Client
// ============================================================================
class FAnimPickerViewportClient : public FEditorViewportClient
{
public:
	FAnimPickerViewportClient(FAdvancedPreviewScene* InPreviewScene);
	virtual ~FAnimPickerViewportClient() override;

	// FEditorViewportClient overrides
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;
	virtual FLinearColor GetBackgroundColor() const override { return FLinearColor(0.18f, 0.18f, 0.2f); }  // 회색 배경
	virtual bool ShouldOrbitCamera() const override { return true; }
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual void TrackingStarted(const FInputEventState& InInputState, bool bIsDragging, bool bNudge) override;
	
	// 호버 상태 설정
	void SetHoveredController(FName InName, int32 InMouseX, int32 InMouseY);
	
	// 메쉬 설정
	void SetSkeletalMesh(USkeletalMesh* InMesh);
	void ClearMesh();
	
	// 컨트롤러 마커 설정
	void SetControllerMarkers(const TArray<TPair<FName, FTransform>>& Controllers, const TMap<FName, FLinearColor>& Colors);
	void SetControllerMarkersWithShapeInfo(const TArray<TPair<FName, FTransform>>& Controllers, const TMap<FName, FLinearColor>& Colors, const TMap<FName, FVector>& ShapeScales, const TMap<FName, FName>& ShapeNames);
	void ClearControllerMarkers();
	
	// 선택 상태 업데이트
	void SetSelectedControllers(const TSet<FName>& Selected);
	
	// 카메라 조작
	void FocusOnMesh();
	
	// 호버된 컨트롤러 찾기
	FName GetHoveredController(int32 MouseX, int32 MouseY);
	
	// 클릭 콜백
	DECLARE_DELEGATE_TwoParams(FOnMarkerClicked, FName /*ControlName*/, bool /*bCtrlDown*/);
	FOnMarkerClicked OnMarkerClicked;

private:
	FAdvancedPreviewScene* PreviewScene;
	USkeletalMeshComponent* PreviewMeshComponent;
	
	// ★ PDI 방식: 마커 데이터만 저장 (StaticMeshComponent 사용 안 함) ★
	TArray<FControllerMarkerData> ControllerData;
	TSet<FName> SelectedControllers;
	
	// 호버 상태
	FName HoveredControllerName;
	int32 HoveredMouseX;
	int32 HoveredMouseY;
};

// ============================================================================
// Anim Picker 3D Viewport Widget
// ============================================================================
class SAnimPickerViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SAnimPickerViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAnimPickerViewport() override;

	// SEditorViewport overrides
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	
	// 마우스 호버 시 툴팁 표시
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	// 메쉬 설정
	void SetSkeletalMesh(USkeletalMesh* InMesh);
	void ClearMesh();
	
	// 컨트롤러 마커 설정
	void SetControllerMarkers(const TArray<TPair<FName, FTransform>>& Controllers, const TMap<FName, FLinearColor>& Colors);
	void SetControllerMarkersWithShapeInfo(const TArray<TPair<FName, FTransform>>& Controllers, const TMap<FName, FLinearColor>& Colors, const TMap<FName, FVector>& ShapeScales, const TMap<FName, FName>& ShapeNames);
	void ClearControllerMarkers();
	
	// 선택 상태 업데이트
	void SetSelectedControllers(const TSet<FName>& Selected);
	
	// 클릭 콜백
	DECLARE_DELEGATE_TwoParams(FOnControllerClicked, FName /*ControlName*/, bool /*bCtrlDown*/);
	FOnControllerClicked OnControllerClicked;

private:
	void HandleMarkerClicked(FName ControlName, bool bCtrlDown);
	void UpdateHoverTooltip(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FAnimPickerViewportClient> ViewportClient;
	FName LastHoveredController;
};
