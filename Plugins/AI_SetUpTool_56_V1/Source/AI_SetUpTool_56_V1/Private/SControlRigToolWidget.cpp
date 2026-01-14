#include "SControlRigToolWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/Application/SlateApplication.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "AssetToolsModule.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "BonePose.h"
#include "Animation/AnimTypes.h"
#include "Animation/AttributesRuntime.h"
#include "Rendering/SkeletalMeshRenderData.h"
// UE5 Control Rig
#include "ControlRig.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintFactory.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMFunctions/RigVMDispatch_Array.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "MeshUtilitiesCommon.h"
#include "MeshUtilitiesEngine.h"
#include "UObject/SavePackage.h"
// IK Rig
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigController.h"
// IK Retargeter
#include "Retargeter/IKRetargeter.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "DesktopPlatformModule.h"
#include "Widgets/Colors/SColorPicker.h"
// Control Rig Edit Mode (시퀀서 연동용)
#include "EditMode/ControlRigEditMode.h"
#include "EditorModeManager.h"
// Anim Picker 3D Viewport
#include "SAnimPickerViewport.h"

// Physics Asset
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

// AnimBlueprint 생성용
#include "Modules/ModuleManager.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ObjectTools.h"
#include "EdGraphUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_Root.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "K2Node_VariableGet.h"
#include "AnimGraphNode_Base.h"  // FAnimGraphNodePropertyBinding, UAnimGraphNodeBinding

#define LOCTEXT_NAMESPACE "SControlRigToolWidget"

// ============================================================================
// 디버그 결과 팝업 (복사 버튼 포함)
// ============================================================================
static void ShowDebugPopup(const FString& Title, const FString& Content)
{
	TSharedRef<SWindow> DebugWindow = SNew(SWindow)
		.Title(FText::FromString(Title))
		.ClientSize(FVector2D(600, 400))
		.SupportsMinimize(false)
		.SupportsMaximize(false);
	
	TSharedPtr<SMultiLineEditableTextBox> TextBox;
	FString ContentCopy = Content; // 캡처용 복사본
	
	DebugWindow->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(5)
		[
			SAssignNew(TextBox, SMultiLineEditableTextBox)
			.Text(FText::FromString(Content))
			.IsReadOnly(true)
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("📋 Copy All")))
				.OnClicked_Lambda([ContentCopy]() -> FReply
				{
					FPlatformApplicationMisc::ClipboardCopy(*ContentCopy);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Close")))
				.OnClicked_Lambda([DebugWindow]() -> FReply
				{
					DebugWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		]
	);
	
	FSlateApplication::Get().AddWindow(DebugWindow);
}

// ============================================================================
// 제로 뼈구조 정의 (UE5 표준 본) - 소문자로 비교
// ============================================================================
const TSet<FString> SControlRigToolWidget::ZeroBones = {
	// 루트/몸통
	TEXT("root"), TEXT("pelvis"),
	TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05"),
	TEXT("neck_01"), TEXT("neck_02"), TEXT("head"),
	// 왼쪽 팔
	TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
	// 오른쪽 팔
	TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
	// 왼쪽 다리
	TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"),
	// 오른쪽 다리
	TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r"),
	// 왼쪽 손가락
	TEXT("thumb_01_l"), TEXT("thumb_02_l"), TEXT("thumb_03_l"),
	TEXT("index_01_l"), TEXT("index_02_l"), TEXT("index_03_l"),
	TEXT("middle_01_l"), TEXT("middle_02_l"), TEXT("middle_03_l"),
	TEXT("ring_01_l"), TEXT("ring_02_l"), TEXT("ring_03_l"),
	TEXT("pinky_01_l"), TEXT("pinky_02_l"), TEXT("pinky_03_l"),
	// 오른쪽 손가락
	TEXT("thumb_01_r"), TEXT("thumb_02_r"), TEXT("thumb_03_r"),
	TEXT("index_01_r"), TEXT("index_02_r"), TEXT("index_03_r"),
	TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r"),
	TEXT("ring_01_r"), TEXT("ring_02_r"), TEXT("ring_03_r"),
	TEXT("pinky_01_r"), TEXT("pinky_02_r"), TEXT("pinky_03_r"),
	// IK 본 (템플릿 전용)
	TEXT("ik_foot_root"), TEXT("ik_foot_l"), TEXT("ik_foot_r"),
	TEXT("ik_hand_root"), TEXT("ik_hand_gun"), TEXT("ik_hand_l"), TEXT("ik_hand_r"),
	TEXT("heel_l"), TEXT("heel_r"), TEXT("tip_l"), TEXT("tip_r")
};

// ============================================================================
// 액세서리 키워드 (컨트롤러 생성 O) - 의상, 머리카락, 무기 등
// ============================================================================
const TArray<FString> SControlRigToolWidget::AccessoryKeywords = {
	// 의상/망토
	TEXT("skirt"), TEXT("cape"), TEXT("cloak"), TEXT("cloth"), TEXT("ribbon"), TEXT("tassel"),
	TEXT("coat"), TEXT("jacket"), TEXT("robe"), TEXT("scarf"), TEXT("collar_cloth"), TEXT("sleeve"),
	// 머리카락
	TEXT("hair"), TEXT("ponytail"), TEXT("pigtail"), TEXT("braid"), TEXT("longhair"), TEXT("bangs"),
	// 가슴/물리
	TEXT("breast"), TEXT("boob"), TEXT("chest_physics"),
	// 무기/악세서리
	TEXT("weapon"), TEXT("sword"), TEXT("shield"), TEXT("bow"), TEXT("quiver"),
	TEXT("earring"), TEXT("necklace"), TEXT("accessory"), TEXT("ornament"),
	TEXT("belt"), TEXT("pouch"), TEXT("bag"),
	// 기타 부속
	TEXT("tail"), TEXT("wing"), TEXT("antenna"),
	// 일반 접두사
	TEXT("bone_acc"), TEXT("_acc_"), TEXT("acc_"), TEXT("bone_"),
	// 얼굴 관련 (세컨더리로 생성)
	TEXT("eye"), TEXT("jaw"), TEXT("mouth"), TEXT("eyebrow"), TEXT("eyelid"), TEXT("brow"),
	TEXT("tongue"), TEXT("teeth"), TEXT("lip"), TEXT("nose"), TEXT("cheek"), TEXT("ear_"),
	TEXT("facial"), TEXT("face_")
};

// ============================================================================
// 헬퍼 키워드 (컨트롤러 생성 X) - 트위스트, 보정, IK 등
// 주의: 너무 일반적인 키워드는 제외 (얼굴 본 등에 영향)
// ============================================================================
const TArray<FString> SControlRigToolWidget::HelperKeywords = {
	// 트위스트/롤 본
	TEXT("twist"), TEXT("roll"), TEXT("_tw"),
	// 보정 본
	TEXT("corrective"), TEXT("blend"),
	// 더미/종단 본
	TEXT("nub"), TEXT("dummy"),
	// IK 관련
	TEXT("ik_"), TEXT("_ik"), TEXT("ikgoal"), TEXT("ikpole"),
	// 컨트롤/가이드 (이미 컨트롤러인 것)
	TEXT("ctrl"), TEXT("control"), TEXT("helper"), TEXT("guide"),
	// 보조 본
	TEXT("lookat"), TEXT("aim_"), TEXT("target"),
	TEXT("aux_"), TEXT("add_"),
	TEXT("_dm_"), TEXT("_ph_"),
	TEXT("metacarpal"), TEXT("attach"),
	TEXT("interpolate"), TEXT("driver")
};

// ============================================================================
// 본 분류 함수들
// ============================================================================
bool SControlRigToolWidget::IsZeroBone(const FString& BoneName) const
{
	// LastBoneMapping에 매핑된 본인지 확인 (소스 본)
	for (const auto& Pair : LastBoneMapping)
	{
		if (Pair.Value.ToString().Equals(BoneName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	
	// ZeroBones 세트에 있는지 확인
	FString LowerName = BoneName.ToLower();
	return ZeroBones.Contains(LowerName);
}

bool SControlRigToolWidget::IsAccessoryBone(const FString& BoneName) const
{
	FString LowerName = BoneName.ToLower();
	
	// 먼저 헬퍼 본인지 확인 (헬퍼가 우선)
	if (IsHelperBone(BoneName))
	{
		return false;
	}
	
	// 액세서리 키워드 포함 여부
	for (const FString& Keyword : AccessoryKeywords)
	{
		if (LowerName.Contains(Keyword))
		{
			return true;
		}
	}
	
	return false;
}

bool SControlRigToolWidget::IsHelperBone(const FString& BoneName) const
{
	FString LowerName = BoneName.ToLower();
	
	for (const FString& Keyword : HelperKeywords)
	{
		if (LowerName.Contains(Keyword))
		{
			return true;
		}
	}
	
	return false;
}

// ============================================================================
// SAnimPicker2DPanel 구현 - 마우스 줌/패닝 지원
// ============================================================================
void SAnimPicker2DPanel::Construct(const FArguments& InArgs)
{
	OwnerWidget = InArgs._OwnerWidget;
	ZoomScale = 1.0f;
	bIsDragging = false;
	
	ChildSlot
	[
		SAssignNew(PickerOverlay, SOverlay)
	];
}

FReply SAnimPicker2DPanel::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (OwnerWidget)
	{
		// 마우스 휠로 줌
		float Delta = MouseEvent.GetWheelDelta() * 0.1f;
		ZoomScale = FMath::Clamp(ZoomScale + Delta, 0.5f, 2.5f);
		OwnerWidget->AnimPicker2DZoomScale = ZoomScale;
		OwnerWidget->UpdateAnimPicker2DView();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAnimPicker2DPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Ctrl + 우클릭 또는 Ctrl + 좌클릭으로 드래그 줌 시작
	if (MouseEvent.IsControlDown() && (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton || MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton))
	{
		bIsDragging = true;
		LastMousePos = MouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SAnimPicker2DPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsDragging)
	{
		bIsDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SAnimPicker2DPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsDragging && OwnerWidget)
	{
		FVector2D CurrentPos = MouseEvent.GetScreenSpacePosition();
		FVector2D Delta = CurrentPos - LastMousePos;
		LastMousePos = CurrentPos;
		
		// 가로 드래그로 줌 조절 (오른쪽 = 확대, 왼쪽 = 축소)
		float ZoomDelta = Delta.X * 0.005f;
		ZoomScale = FMath::Clamp(ZoomScale + ZoomDelta, 0.5f, 2.5f);
		OwnerWidget->AnimPicker2DZoomScale = ZoomScale;
		OwnerWidget->UpdateAnimPicker2DView();
		
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SControlRigToolWidget::Construct(const FArguments& InArgs)
{
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(24);
	LoadAssetData();

	// 프로페셔널 색상 팔레트
	const FLinearColor HeaderBgColor(0.02f, 0.02f, 0.025f, 1.0f);     // 거의 검정
	const FLinearColor HeaderAccent(0.0f, 0.47f, 0.84f, 1.0f);        // 언리얼 블루
	const FLinearColor CardBgColor(0.04f, 0.04f, 0.05f, 1.0f);        // 다크 카드
	const FLinearColor SectionHeaderColor(0.08f, 0.08f, 0.1f, 1.0f);  // 섹션 헤더
	const FLinearColor TextMuted(0.5f, 0.5f, 0.55f, 1.0f);            // 흐린 텍스트
	const FLinearColor BorderColor(0.15f, 0.15f, 0.18f, 1.0f);        // 테두리

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.015f, 0.015f, 0.02f, 1.0f))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			
			// ========== 헤더 ==========
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.BorderBackgroundColor(HeaderBgColor)
				.Padding(FMargin(16, 14))
				[
					SNew(SHorizontalBox)
					// 아이콘
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
					[
						SNew(SBox)
						.WidthOverride(32)
						.HeightOverride(32)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("ControlRig.RigUnit"))
							.ColorAndOpacity(HeaderAccent)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "AI Character Setup Tool"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						.ColorAndOpacity(FLinearColor::White)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Subtitle", "Control Rig, IK Rig & Physics Setup"))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							.ColorAndOpacity(TextMuted)
						]
					]
					// 버전 뱃지
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.0f, 0.35f, 0.6f, 0.3f))
						.Padding(FMargin(8, 3))
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("v3.0")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
							.ColorAndOpacity(HeaderAccent)
						]
					]
					// 전체 새로고침 버튼
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton")
						.ToolTipText(LOCTEXT("ResetAll_Tooltip", "Reset All - Clear all selections and return to initial state"))
						.OnClicked(this, &SControlRigToolWidget::OnResetAllClicked)
						.ContentPadding(FMargin(6, 4))
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("Icons.Refresh"))
							.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f))
							.DesiredSizeOverride(FVector2D(16, 16))
						]
					]
				]
			]
			
			// 구분선
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(HeaderAccent)
				.Padding(0)
				[
					SNew(SBox).HeightOverride(2)
				]
			]
			
			// ========== 탭 바 ==========
			+ SVerticalBox::Slot().AutoHeight()
			[
				CreateTabBar()
			]
			
			// ========== 탭 콘텐츠 ==========
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(12)
			[
				SAssignNew(TabContentSwitcher, SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return CurrentTabIndex; })
				
				// Tab 0: Control Rig
				+ SWidgetSwitcher::Slot()
				[
					CreateControlRigTab()
				]
				
				// Tab 1: Control Rig Sec (세컨더리 전용)
				+ SWidgetSwitcher::Slot()
				[
					CreateControlRigSecTab()
				]
				
				// Tab 2: IK Rig
				+ SWidgetSwitcher::Slot()
				[
					CreateIKRigTab()
				]
				
				// Tab 3: Kawaii Physics
				+ SWidgetSwitcher::Slot()
				[
					CreateKawaiiPhysicsTab()
				]
				
				// Tab 4: Physics Asset
				+ SWidgetSwitcher::Slot()
				[
					CreatePhysicsAssetTab()
				]
				
				// Tab 5: Anim Picker
				+ SWidgetSwitcher::Slot()
				[
					CreateAnimPickerTab()
				]
			]
		]
	];

	UpdateTemplateThumbnail();
	UpdateMeshThumbnail();
	UpdateIKTemplateThumbnail();
	UpdateIKMeshThumbnail();
	SetStatus(FString::Printf(TEXT("Loaded: %d templates, %d meshes"), ControlRigs.Num(), SkeletalMeshes.Num()));
}

// ============================================================================
// 탭 바 생성
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::CreateTabBar()
{
	auto MakeTabButton = [this](int32 TabIndex, const FText& Label, const FSlateBrush* Icon) -> TSharedRef<SWidget>
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor_Lambda([this, TabIndex]() 
			{ 
				if (CurrentTabIndex == TabIndex)
					return FLinearColor(0.08f, 0.08f, 0.1f, 1.0f);  // 활성 탭 - 밝은 배경
				return FLinearColor(0.02f, 0.02f, 0.025f, 1.0f);    // 비활성 탭 - 어두운 배경
			})
			.Padding(0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton")
					.ContentPadding(FMargin(20, 12))
					.OnClicked_Lambda([this, TabIndex]() -> FReply
					{
						OnTabChanged(TabIndex);
						return FReply::Handled();
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(Icon)
							.ColorAndOpacity_Lambda([this, TabIndex]() 
							{ 
								return CurrentTabIndex == TabIndex ? FLinearColor(0.3f, 0.7f, 1.0f) : FLinearColor(0.4f, 0.4f, 0.45f); 
							})
							.DesiredSizeOverride(FVector2D(16, 16))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Label)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity_Lambda([this, TabIndex]() 
							{ 
								return CurrentTabIndex == TabIndex ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.55f); 
							})
						]
					]
				]
				// 활성 탭 하단 강조선
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([this, TabIndex]() 
					{ 
						return CurrentTabIndex == TabIndex ? FLinearColor(0.0f, 0.5f, 1.0f, 1.0f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f); 
					})
					.Padding(0)
					[
						SNew(SBox).HeightOverride(3)
					]
				]
			];
	};
	
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.015f, 0.015f, 0.02f, 1.0f))
			.Padding(FMargin(8, 0, 8, 0))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeTabButton(0, LOCTEXT("Tab_ControlRig", "Control Rig"), FAppStyle::GetBrush("ControlRig.RigUnit"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0)
				[
					MakeTabButton(1, LOCTEXT("Tab_ControlRigSec", "Control Rig Sec"), FAppStyle::GetBrush("ControlRig.RigUnit"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0)
				[
					MakeTabButton(2, LOCTEXT("Tab_IKRig", "IK Rig"), FAppStyle::GetBrush("ClassIcon.IKRigDefinition"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0)
				[
					MakeTabButton(3, LOCTEXT("Tab_KawaiiPhysics", "Kawaii Physics"), FAppStyle::GetBrush("PhysicsAssetEditor.Tabs.Profiles"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0)
				[
					MakeTabButton(4, LOCTEXT("Tab_PhysicsAsset", "Physics Asset"), FAppStyle::GetBrush("PhysicsAssetEditor.Tabs.Body"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0)
				[
					MakeTabButton(5, LOCTEXT("Tab_AnimPicker", "Anim Picker"), FAppStyle::GetBrush("Sequencer.Tracks.ControlRig"))
				]
				// 나머지 공간 채우기
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f, 1.0f))
				]
			]
		]
		// 탭 바 하단 구분선
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f))
			.Padding(0)
			[
				SNew(SBox).HeightOverride(1)
			]
		];
}

void SControlRigToolWidget::OnTabChanged(int32 NewTabIndex)
{
	CurrentTabIndex = NewTabIndex;
	// TabContentSwitcher는 WidgetIndex_Lambda로 자동 업데이트됨
	
	// Kawaii Physics 탭으로 전환 시 Secondary 데이터 가져오기
	if (NewTabIndex == 3)
	{
		TransferSecondaryDataToKawaii();
	}
}

// ============================================================================
// 전체 새로고침 - 모든 상태 초기화
// ============================================================================
FReply SControlRigToolWidget::OnResetAllClicked()
{
	// Control Rig 탭 초기화
	SelectedTemplate.Reset();
	SelectedMesh.Reset();
	if (TemplateComboBox.IsValid())
	{
		TemplateComboBox->ClearSelection();
	}
	if (MeshComboBox.IsValid())
	{
		MeshComboBox->ClearSelection();
	}
	LastBoneMapping.Empty();
	BoneDisplayList.Empty();
	CachedMesh.Reset();
	PendingControlRig.Reset();
	CurrentStep = EControlRigWorkflowStep::Step1_Setup;
	if (OutputNameBox.IsValid())
	{
		OutputNameBox->SetText(FText::GetEmpty());
	}
	if (OutputFolderBox.IsValid())
	{
		OutputFolderBox->SetText(FText::FromString(DefaultOutputFolder));
	}
	if (MappingResultBox.IsValid())
	{
		MappingResultBox->ClearChildren();
	}
	UpdateTemplateThumbnail();
	UpdateMeshThumbnail();
	UpdateWorkflowUI();
	
	// IK Rig 탭 초기화
	SelectedIKRigTemplate.Reset();
	SelectedIKMesh.Reset();
	if (IKRigTemplateComboBox.IsValid())
	{
		IKRigTemplateComboBox->ClearSelection();
	}
	if (IKMeshComboBox.IsValid())
	{
		IKMeshComboBox->ClearSelection();
	}
	IKBoneMapping.Empty();
	if (IKOutputNameBox.IsValid())
	{
		IKOutputNameBox->SetText(FText::GetEmpty());
	}
	if (IKOutputFolderBox.IsValid())
	{
		IKOutputFolderBox->SetText(FText::FromString(IKDefaultOutputFolder));
	}
	if (IKMappingResultBox.IsValid())
	{
		IKMappingResultBox->ClearChildren();
	}
	UpdateIKTemplateThumbnail();
	UpdateIKMeshThumbnail();
	
	// IK Retargeter 초기화
	SelectedRetargeterSource.Reset();
	SelectedRetargeterTarget.Reset();
	if (RetargeterSourceComboBox.IsValid())
	{
		RetargeterSourceComboBox->ClearSelection();
	}
	if (RetargeterTargetComboBox.IsValid())
	{
		RetargeterTargetComboBox->ClearSelection();
	}
	UpdateRetargeterSourceThumbnail();
	UpdateRetargeterTargetThumbnail();
	
	// Kawaii Physics 탭 초기화
	SelectedKawaiiMesh.Reset();
	if (KawaiiMeshComboBox.IsValid())
	{
		KawaiiMeshComboBox->ClearSelection();
	}
	KawaiiTags.Empty();
	KawaiiBoneDisplayList.Empty();
	SelectedKawaiiTagIndex = INDEX_NONE;
	if (KawaiiOutputNameBox.IsValid())
	{
		KawaiiOutputNameBox->SetText(FText::GetEmpty());
	}
	if (KawaiiOutputFolderBox.IsValid())
	{
		KawaiiOutputFolderBox->SetText(FText::FromString(KawaiiDefaultOutputFolder));
	}
	UpdateKawaiiMeshThumbnail();
	UpdateKawaiiBoneTreeUI();
	UpdateKawaiiTagListUI();
	
	// Physics Asset 탭 초기화
	SelectedPhysAssetMesh.Reset();
	if (PhysAssetMeshComboBox.IsValid())
	{
		PhysAssetMeshComboBox->ClearSelection();
	}
	PhysAssetBoneMapping.Empty();
	PhysAssetMainBones.Empty();
	if (PhysAssetOutputNameBox.IsValid())
	{
		PhysAssetOutputNameBox->SetText(FText::GetEmpty());
	}
	if (PhysAssetOutputFolderBox.IsValid())
	{
		PhysAssetOutputFolderBox->SetText(FText::FromString(PhysAssetDefaultOutputFolder));
	}
	UpdatePhysAssetMeshThumbnail();
	UpdatePhysAssetBoneListUI();
	
	// 탭을 Control Rig으로 리셋
	CurrentTabIndex = 0;
	
	SetStatus(TEXT("All states have been reset."));
	
	return FReply::Handled();
}

FSlateColor SControlRigToolWidget::GetTabButtonColor(int32 TabIndex) const
{
	return CurrentTabIndex == TabIndex ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor(0.5f, 0.5f, 0.55f));
}

EVisibility SControlRigToolWidget::GetTabContentVisibility(int32 TabIndex) const
{
	return CurrentTabIndex == TabIndex ? EVisibility::Visible : EVisibility::Collapsed;
}

// ============================================================================
// Control Rig 탭 콘텐츠
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::CreateControlRigTab()
{
	const FLinearColor CardBgColor(0.04f, 0.04f, 0.05f, 1.0f);
	
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			
			// ===== 템플릿 섹션 =====
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(CardBgColor)
						.Padding(10)
						[
							CreateTemplateSection()
						]
					]
					
					// ===== 메시 섹션 =====
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(CardBgColor)
						.Padding(10)
						[
							CreateMeshSection()
						]
					]
					
					// ===== 출력 섹션 =====
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(CardBgColor)
						.Padding(10)
						[
							CreateOutputSection()
						]
					]
					
					// ===== 버튼 섹션 =====
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)
					[
						CreateButtonSection()
					]
					
					// ===== 상태 바 =====
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.03f, 0.03f, 0.04f, 1.0f))
						.Padding(FMargin(12, 8))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.Info"))
								.ColorAndOpacity(FLinearColor(0.4f, 0.6f, 0.9f, 1.0f))
								.DesiredSizeOverride(FVector2D(14, 14))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SAssignNew(StatusText, STextBlock)
								.Text(LOCTEXT("Ready", "Ready - Select template and mesh to begin"))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								.ColorAndOpacity(FLinearColor(0.6f, 0.65f, 0.7f, 1.0f))
							]
						]
					]
					
					// ===== 매핑 결과 =====
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 6)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("ClassIcon.SkeletalMesh"))
							.ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.3f, 1.0f))
							.DesiredSizeOverride(FVector2D(14, 14))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Results", "Bone Mapping Results"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
						]
					]
					
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.025f, 0.025f, 0.03f, 1.0f))
						.Padding(10)
						[
							SNew(SBox).MinDesiredHeight(80).MaxDesiredHeight(140)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(MappingResultBox, SVerticalBox)
								]
							]
						]
					]
					
					// ===== 본 선택 UI (Step 2: 세컨더리/헬퍼 선택) =====
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 6)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("LevelEditor.Tabs.Viewports"))
							.ColorAndOpacity(FLinearColor(0.3f, 0.8f, 0.5f, 1.0f))
							.DesiredSizeOverride(FVector2D(14, 14))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BoneSelection", "Secondary Bone Selection"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BoneSelectionHint", "Check = Create Controller"))
							.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
							.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f, 1.0f))
						]
					]
					
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.03f, 0.03f, 0.05f, 1.0f))
						.Padding(8)
						[
							SNew(SBox).MinDesiredHeight(150).MaxDesiredHeight(300)
							[
								SAssignNew(BoneScrollBox, SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(BoneSelectionBox, SVerticalBox)
								]
							]
						]
					]
					
					// ===== 최종 Create Control Rig 버튼 (초록색) =====
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 15, 0, 5)
					[
						SAssignNew(FinalCreateButton, SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
						.ContentPadding(FMargin(20, 14))
						.HAlign(HAlign_Center)
						.IsEnabled(false)  // Body Rig 생성 후 활성화
						.OnClicked(this, &SControlRigToolWidget::OnCreateFinalControlRigClicked)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("\U00002728")))  // ✨
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
							]
							+ SHorizontalBox::Slot().Padding(12, 0, 0, 0).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("FinalCreate", "3. Create Final Control Rig"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							]
						]
					]
					
					// ===== Secondary Only Control Rig 버튼 =====
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 5)
					[
						SAssignNew(SecondaryOnlyButton, SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Primary")
						.ContentPadding(FMargin(20, 14))
						.HAlign(HAlign_Center)
						.IsEnabled(false)  // 본 매핑 후 활성화
						.OnClicked(this, &SControlRigToolWidget::OnCreateSecondaryOnlyControlRigClicked)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("\U0001F3A8")))  // 🎨
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
							]
							+ SHorizontalBox::Slot().Padding(12, 0, 0, 0).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SecondaryOnly", "Create Secondary Only Control Rig"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							]
						]
					]
		];
}

// ============================================================================
// IK Rig 탭 콘텐츠
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::CreateIKRigTab()
{
	const FLinearColor CardBgColor(0.04f, 0.04f, 0.05f, 1.0f);
	const FLinearColor SectionHeaderColor(0.08f, 0.08f, 0.1f, 1.0f);
	const FLinearColor TextMuted(0.5f, 0.5f, 0.55f, 1.0f);
	
	// IK Rig 템플릿 로드
	LoadIKRigTemplates();
	
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			
			// ===== 템플릿 IK Rig 섹션 =====
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(CardBgColor)
				.Padding(10)
				[
					SNew(SVerticalBox)
					// 섹션 헤더
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("ClassIcon.IKRigDefinition"))
							.ColorAndOpacity(FLinearColor(0.9f, 0.5f, 0.2f, 1.0f))
							.DesiredSizeOverride(FVector2D(14, 14))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("IKTemplateHeader", "Template IK Rig"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
						]
						]
					// 썸네일 + 드롭다운 + 화살표 버튼
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
						[
							SAssignNew(IKTemplateThumbnailBox, SBox)
							.WidthOverride(ThumbnailSize)
							.HeightOverride(ThumbnailSize)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SAssignNew(IKRigTemplateComboBox, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&IKRigTemplateOptions)
							.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateIKRigTemplateWidget)
							.OnSelectionChanged(this, &SControlRigToolWidget::OnIKRigTemplateSelectionChanged)
							[
								SNew(STextBlock)
								.Text(this, &SControlRigToolWidget::GetSelectedIKRigTemplateName)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0).VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ContentPadding(FMargin(4))
							.OnClicked(this, &SControlRigToolWidget::OnUseSelectedIKTemplateClicked)
							.ToolTipText(LOCTEXT("UseSelectedIKTemplate", "Use selected IK Rig from Content Browser"))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("\U00002B05")))  // ⬅
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
							]
						]
					]
				]
			]
			
			// ===== 스켈레탈 메쉬 섹션 =====
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(CardBgColor)
				.Padding(10)
				[
					SNew(SVerticalBox)
					// 섹션 헤더
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("ClassIcon.SkeletalMesh"))
							.ColorAndOpacity(FLinearColor(0.3f, 0.7f, 0.9f, 1.0f))
							.DesiredSizeOverride(FVector2D(14, 14))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("IKMeshHeader", "Target Skeletal Mesh"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
						]
						]
					// 썸네일 + 드롭다운 + 화살표 버튼
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
						[
							SAssignNew(IKMeshThumbnailBox, SBox)
							.WidthOverride(ThumbnailSize)
							.HeightOverride(ThumbnailSize)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SAssignNew(IKMeshComboBox, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&MeshOptions)
							.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateIKMeshWidget)
							.OnSelectionChanged(this, &SControlRigToolWidget::OnIKMeshSelectionChanged)
							[
								SNew(STextBlock)
								.Text(this, &SControlRigToolWidget::GetSelectedIKMeshName)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0).VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ContentPadding(FMargin(4))
							.OnClicked(this, &SControlRigToolWidget::OnUseSelectedIKMeshClicked)
							.ToolTipText(LOCTEXT("UseSelectedIKMesh", "Use selected Skeletal Mesh from Content Browser"))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("\U00002B05")))  // ⬅
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
							]
						]
					]
				]
			]
			
			// ===== Make T-Pose 버튼 =====
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
				.ContentPadding(FMargin(16, 10))
				.HAlign(HAlign_Center)
				.OnClicked(this, &SControlRigToolWidget::OnMakeTPoseClicked)
				.IsEnabled_Lambda([this]() { return SelectedIKMesh.IsValid(); })
				.ToolTipText(LOCTEXT("MakeTPoseTooltip", "Create a 1-frame T-Pose animation sequence from the selected Skeletal Mesh"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("\U0001F9CD")))  // 🧍
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MakeTPose", "Make T-Pose"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					]
				]
			]
			
			// ===== 출력 섹션 =====
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(CardBgColor)
				.Padding(10)
				[
					SNew(SVerticalBox)
					// 섹션 헤더
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("Icons.Save"))
							.ColorAndOpacity(FLinearColor(0.4f, 0.8f, 0.4f, 1.0f))
							.DesiredSizeOverride(FVector2D(14, 14))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("IKOutputHeader", "Output Settings"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
						]
					]
					// 이름 입력
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SBox).WidthOverride(60)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("IKName", "Name"))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								.ColorAndOpacity(TextMuted)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(IKOutputNameBox, SEditableTextBox)
							.HintText(LOCTEXT("IKNameHint", "IK_MeshName"))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
					// 경로 입력
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SBox).WidthOverride(60)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("IKFolder", "Folder"))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								.ColorAndOpacity(TextMuted)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(IKOutputFolderBox, SEditableTextBox)
							.Text(FText::FromString(IKDefaultOutputFolder))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ContentPadding(4)
							.OnClicked(this, &SControlRigToolWidget::OnIKBrowseFolderClicked)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.FolderOpen"))
								.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.65f, 1.0f))
								.DesiredSizeOverride(FVector2D(14, 14))
							]
						]
					]
				]
			]
			
			// ===== AI Bone Mapping 버튼 섹션 =====
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Primary")
					.ContentPadding(FMargin(16, 10))
					.HAlign(HAlign_Center)
					.OnClicked(this, &SControlRigToolWidget::OnIKAIBoneMappingClicked)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("Icons.Visible"))
							.ColorAndOpacity(FLinearColor::White)
							.DesiredSizeOverride(FVector2D(14, 14))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("IKAIMapping", "AI Bone Mapping"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
					.ContentPadding(FMargin(16, 10))
					.OnClicked(this, &SControlRigToolWidget::OnIKApproveMappingClicked)
					.IsEnabled_Lambda([this]() { return IKBoneMapping.Num() > 0; })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("Icons.Check"))
							.ColorAndOpacity(FLinearColor::White)
							.DesiredSizeOverride(FVector2D(14, 14))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("IKApprove", "Approve"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]
					]
				]
			]
			
			// ===== 상태 바 =====
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.03f, 0.03f, 0.04f, 1.0f))
				.Padding(FMargin(12, 8))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Info"))
						.ColorAndOpacity(FLinearColor(0.4f, 0.6f, 0.9f, 1.0f))
						.DesiredSizeOverride(FVector2D(14, 14))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SAssignNew(IKStatusText, STextBlock)
						.Text(LOCTEXT("IKReady", "Ready - Select template and mesh"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FLinearColor(0.6f, 0.65f, 0.7f, 1.0f))
					]
				]
			]
			
			// ===== 매핑 결과 =====
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("ClassIcon.SkeletalMesh"))
					.ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.3f, 1.0f))
					.DesiredSizeOverride(FVector2D(14, 14))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("IKResults", "Bone Mapping Results"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ContentPadding(FMargin(4))
					.OnClicked_Lambda([this]() -> FReply
					{
						// IKBoneMapping을 문자열로 변환해서 클립보드에 복사
						FString Result;
						for (const auto& Pair : IKBoneMapping)
						{
							Result += FString::Printf(TEXT("%s -> %s\n"), *Pair.Key.ToString(), *Pair.Value.ToString());
						}
						FPlatformApplicationMisc::ClipboardCopy(*Result);
						SetIKStatus(TEXT("Mapping copied to clipboard!"));
						return FReply::Handled();
					})
					.IsEnabled_Lambda([this]() { return IKBoneMapping.Num() > 0; })
					.ToolTipText(LOCTEXT("CopyMapping", "Copy mapping to clipboard"))
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("📋")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.025f, 0.025f, 0.03f, 1.0f))
				.Padding(10)
				[
					SNew(SBox).MinDesiredHeight(80).MaxDesiredHeight(140)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(IKMappingResultBox, SVerticalBox)
						]
					]
				]
			]
			
			// ===== Create IK Rig 버튼 =====
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 15, 0, 5)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
				.ContentPadding(FMargin(20, 14))
				.HAlign(HAlign_Center)
				.OnClicked(this, &SControlRigToolWidget::OnCreateIKRigClicked)
				.IsEnabled_Lambda([this]() { return IKBoneMapping.Num() > 0 && SelectedIKRigTemplate.IsValid() && SelectedIKMesh.IsValid(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("\U00002728")))  // ✨
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
					]
					+ SHorizontalBox::Slot().Padding(12, 0, 0, 0).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateIKRig", "Create IK Rig"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					]
				]
			]
			
			// ===================================================================
			// IK Retargeter 생성 섹션
			// ===================================================================
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 30, 0, 0)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.05f, 1.0f))
				.Padding(16)
				[
					SNew(SVerticalBox)
					
					// 섹션 타이틀
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("ClassIcon.IKRetargeter"))
							.ColorAndOpacity(FLinearColor(0.9f, 0.6f, 0.2f, 1.0f))
							.DesiredSizeOverride(FVector2D(18, 18))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("IKRetargeterSection", "IK Retargeter"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(FLinearColor(0.9f, 0.6f, 0.2f, 1.0f))
						]
					]
					
					// Source IK Rig
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SourceIKRig", "Source IK Rig"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(RetargeterSourceComboBox, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&RetargeterSourceOptions)
							.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateRetargeterSourceWidget)
							.OnSelectionChanged(this, &SControlRigToolWidget::OnRetargeterSourceSelectionChanged)
							[
								SNew(STextBlock)
								.Text(this, &SControlRigToolWidget::GetSelectedRetargeterSourceName)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
						[
							SAssignNew(RetargeterSourceThumbnailBox, SBox)
							.WidthOverride(48)
							.HeightOverride(48)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "FlatButton")
							.ContentPadding(FMargin(4))
							.ToolTipText(LOCTEXT("UseSelectedSource", "Use Selected in Content Browser"))
							.OnClicked(this, &SControlRigToolWidget::OnUseSelectedRetargeterSourceClicked)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("\U00002B05")))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
							]
						]
					]
					
					// Target IK Rig
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TargetIKRig", "Target IK Rig"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(RetargeterTargetComboBox, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&RetargeterTargetOptions)
							.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateRetargeterTargetWidget)
							.OnSelectionChanged(this, &SControlRigToolWidget::OnRetargeterTargetSelectionChanged)
							[
								SNew(STextBlock)
								.Text(this, &SControlRigToolWidget::GetSelectedRetargeterTargetName)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
						[
							SAssignNew(RetargeterTargetThumbnailBox, SBox)
							.WidthOverride(48)
							.HeightOverride(48)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "FlatButton")
							.ContentPadding(FMargin(4))
							.ToolTipText(LOCTEXT("UseSelectedTarget", "Use Selected in Content Browser"))
							.OnClicked(this, &SControlRigToolWidget::OnUseSelectedRetargeterTargetClicked)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("\U00002B05")))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
							]
						]
					]
					
					// Output Name
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RetargeterOutputName", "Output Name"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
					[
						SAssignNew(RetargeterOutputNameBox, SEditableTextBox)
						.Text(FText::FromString(TEXT("NewIKRetargeter")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					]
					
					// Output Folder
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RetargeterOutputFolder", "Output Folder"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(RetargeterOutputFolderBox, SEditableTextBox)
							.Text(FText::FromString(RetargeterDefaultOutputFolder))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ContentPadding(4)
							.OnClicked(this, &SControlRigToolWidget::OnRetargeterBrowseFolderClicked)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.FolderOpen"))
								.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.65f, 1.0f))
								.DesiredSizeOverride(FVector2D(14, 14))
							]
						]
					]
					
					// Create IK Retargeter 버튼
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Primary")
						.ContentPadding(FMargin(16, 12))
						.HAlign(HAlign_Center)
						.OnClicked(this, &SControlRigToolWidget::OnCreateIKRetargeterClicked)
						.IsEnabled_Lambda([this]() { return SelectedRetargeterSource.IsValid() && SelectedRetargeterTarget.IsValid(); })
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("\U0001F504")))  // 🔄
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
							]
							+ SHorizontalBox::Slot().Padding(10, 0, 0, 0).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("CreateIKRetargeter", "Create IK Retargeter"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
								.ColorAndOpacity(FLinearColor::White)
							]
						]
					]
				]
			]
		];
	
	// IK Retargeter 옵션 로드
	LoadRetargeterIKRigs();
}

// ============================================================================
// Kawaii Physics 탭 콘텐츠
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::CreateKawaiiPhysicsTab()
{
	const FLinearColor TextMuted(0.55f, 0.55f, 0.6f, 1.0f);
	const FLinearColor TextBright(0.9f, 0.9f, 0.95f, 1.0f);
	const FLinearColor AccentPink(1.0f, 0.5f, 0.8f, 1.0f);
	
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			
			// ========== 섹션 헤더 (더 크게) ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("PhysicsAssetEditor.Tabs.Profiles"))
					.ColorAndOpacity(AccentPink)
					.DesiredSizeOverride(FVector2D(24, 24))  // 더 큰 아이콘
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KawaiiTitle", "Kawaii Physics Setup"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))  // 더 큰 글씨
					.ColorAndOpacity(TextBright)
				]
			]
			
			// ========== Skeletal Mesh 선택 (더 두껍게) ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("ClassIcon.SkeletalMesh"))
					.ColorAndOpacity(AccentPink)
					.DesiredSizeOverride(FVector2D(18, 18))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KawaiiMeshLabel", "Skeletal Mesh"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))  // 더 큰 글씨
					.ColorAndOpacity(TextBright)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
			[
				SNew(SHorizontalBox)
				// 썸네일
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
				[
					SAssignNew(KawaiiMeshThumbnailBox, SBox)
					.WidthOverride(ThumbnailSize)
					.HeightOverride(ThumbnailSize)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.06f, 0.06f, 0.08f, 1.0f))
					]
				]
				// 드롭다운 (더 두꺼운 높이)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredHeight(36.0f)  // 더 두꺼운 드롭다운
					[
						SAssignNew(KawaiiMeshComboBox, SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&MeshOptions)
						.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateKawaiiMeshWidget)
						.OnSelectionChanged(this, &SControlRigToolWidget::OnKawaiiMeshSelectionChanged)
						.Content()
						[
							SNew(STextBlock)
							.Text(this, &SControlRigToolWidget::GetSelectedKawaiiMeshName)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))  // 더 큰 글씨
						]
					]
				]
				// 화살표 버튼 (더 크고 선명하게)
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "Button")
					.ToolTipText(LOCTEXT("UseSelectedKawaiiMesh", "Use selected asset from Content Browser"))
					.OnClicked(this, &SControlRigToolWidget::OnUseSelectedKawaiiMeshClicked)
					.ContentPadding(FMargin(10, 8))  // 더 큰 패딩
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.ChevronLeft"))
						.ColorAndOpacity(AccentPink)
						.DesiredSizeOverride(FVector2D(20, 20))  // 더 큰 아이콘
					]
				]
			]
			
			// ========== 스켈레톤 트리 (더 크게) ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Persona.SkeletonTree"))
					.ColorAndOpacity(AccentPink)
					.DesiredSizeOverride(FVector2D(18, 18))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KawaiiBoneTree", "Skeleton Structure"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					.ColorAndOpacity(TextBright)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().MaxHeight(350.0f).Padding(0, 0, 0, 12)  // 더 큰 높이
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.05f, 1.0f))
				.Padding(8)  // 더 큰 패딩
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(KawaiiBoneTreeBox, SVerticalBox)
					]
				]
			]
			
			// ========== 태그 목록 (더 크게) ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Label"))
					.ColorAndOpacity(AccentPink)
					.DesiredSizeOverride(FVector2D(18, 18))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KawaiiTags", "Tags"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					.ColorAndOpacity(TextBright)
				]
				// 태그 추가 버튼 (더 크게)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "Button")
					.ToolTipText(LOCTEXT("AddTag", "Add new tag"))
					.OnClicked(this, &SControlRigToolWidget::OnAddKawaiiTagClicked)
					.ContentPadding(FMargin(8, 4))
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Plus"))
						.ColorAndOpacity(FLinearColor(0.4f, 0.9f, 0.4f, 1.0f))
						.DesiredSizeOverride(FVector2D(18, 18))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().MaxHeight(180.0f).Padding(0, 0, 0, 16)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.05f, 1.0f))
				.Padding(8)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(KawaiiTagListBox, SVerticalBox)
					]
				]
			]
			
			// ========== 출력 설정 (더 크게) ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Save"))
					.ColorAndOpacity(AccentPink)
					.DesiredSizeOverride(FVector2D(18, 18))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KawaiiOutputSettings", "Output Settings"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					.ColorAndOpacity(TextBright)
				]
			]
			// Output Folder
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
				[
					SNew(SBox)
					.MinDesiredWidth(90.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("KawaiiOutputFolder", "Output Folder"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
						.ColorAndOpacity(TextMuted)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredHeight(32.0f)
					[
						SAssignNew(KawaiiOutputFolderBox, SEditableTextBox)
						.Text(FText::FromString(KawaiiDefaultOutputFolder))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "Button")
					.ToolTipText(LOCTEXT("BrowseKawaiiFolder", "Browse folder"))
					.OnClicked(this, &SControlRigToolWidget::OnKawaiiBrowseFolderClicked)
					.ContentPadding(FMargin(10, 6))
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.FolderOpen"))
						.ColorAndOpacity(AccentPink)
						.DesiredSizeOverride(FVector2D(18, 18))
					]
				]
			]
			// Output Name
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
				[
					SNew(SBox)
					.MinDesiredWidth(90.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("KawaiiOutputName", "Output Name"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
						.ColorAndOpacity(TextMuted)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredHeight(32.0f)
					[
						SAssignNew(KawaiiOutputNameBox, SEditableTextBox)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
						.HintText(LOCTEXT("KawaiiNameHint", "ABP_{MeshName}_Kawaii"))
					]
				]
			]
			
			// ========== 생성 버튼 (더 크게) ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(32, 14))  // 더 큰 버튼
				.OnClicked(this, &SControlRigToolWidget::OnCreateKawaiiAnimBPClicked)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Plus"))
						.ColorAndOpacity(FLinearColor::White)
						.DesiredSizeOverride(FVector2D(20, 20))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateKawaiiAnimBP", "Create Kawaii AnimBP"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))  // 더 큰 글씨
					]
				]
			]
			
			// ========== 상태 표시 ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
			[
				SAssignNew(KawaiiStatusText, STextBlock)
				.Text(LOCTEXT("KawaiiReady", "Select a Skeletal Mesh to begin"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				.ColorAndOpacity(TextMuted)
				.AutoWrapText(true)
			]
		];
}

SControlRigToolWidget::~SControlRigToolWidget()
{
	ThumbnailPool.Reset();
}

TSharedRef<SWidget> SControlRigToolWidget::CreateTemplateSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("ControlRig.RigUnit"))
				.ColorAndOpacity(FLinearColor(0.3f, 0.7f, 1.0f, 1.0f))
				.DesiredSizeOverride(FVector2D(14, 14))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TemplateLabel", "Template Control Rig"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			// 썸네일
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
			[
				SAssignNew(TemplateThumbnailBox, SBox)
				.WidthOverride(ThumbnailSize)
				.HeightOverride(ThumbnailSize)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(2)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("CR")))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.6f))
					]
				]
			]
			// 드롭다운
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SAssignNew(TemplateComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&TemplateOptions)
				.OnSelectionChanged(this, &SControlRigToolWidget::OnTemplateSelectionChanged)
				.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateTemplateWidget)
				[
					SNew(STextBlock)
					.Text(this, &SControlRigToolWidget::GetSelectedTemplateName)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
			]
			// 화살표 버튼 (선택된 에셋 사용)
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0).VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(4))
				.OnClicked(this, &SControlRigToolWidget::OnUseSelectedTemplateClicked)
				.ToolTipText(LOCTEXT("UseSelectedTemplate", "Use selected Control Rig from Content Browser"))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("\U00002B05")))  // ⬅
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				]
			]
		];
}

TSharedRef<SWidget> SControlRigToolWidget::CreateMeshSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("ClassIcon.SkeletalMesh"))
				.ColorAndOpacity(FLinearColor(0.9f, 0.6f, 0.3f, 1.0f))
				.DesiredSizeOverride(FVector2D(14, 14))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MeshLabel", "Target Skeletal Mesh"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			// 썸네일
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
			[
				SAssignNew(MeshThumbnailBox, SBox)
				.WidthOverride(ThumbnailSize)
				.HeightOverride(ThumbnailSize)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(2)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("SK")))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.6f))
					]
				]
			]
			// 드롭다운
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SAssignNew(MeshComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&MeshOptions)
				.OnSelectionChanged(this, &SControlRigToolWidget::OnMeshSelectionChanged)
				.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateMeshWidget)
				[
					SNew(STextBlock)
					.Text(this, &SControlRigToolWidget::GetSelectedMeshName)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
			]
			// 화살표 버튼
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0).VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(4))
				.OnClicked(this, &SControlRigToolWidget::OnUseSelectedMeshClicked)
				.ToolTipText(LOCTEXT("UseSelectedMesh", "Use selected Skeletal Mesh from Content Browser"))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("\U00002B05")))  // ⬅
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				]
			]
		];
}

TSharedRef<SWidget> SControlRigToolWidget::CreateOutputSection()
{
	return SNew(SVerticalBox)
		// 섹션 헤더
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Save"))
				.ColorAndOpacity(FLinearColor(0.5f, 0.8f, 0.5f, 1.0f))
				.DesiredSizeOverride(FVector2D(14, 14))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OutputLabel", "Output Settings"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
			]
		]
		// 출력 이름
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OutputNameLabel", "Name"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.7f))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SAssignNew(OutputNameBox, SEditableTextBox)
			.Text(FText::FromString(TEXT("CTR_NewRig_Rig")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		]
		// 출력 폴더
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OutputFolderLabel", "Folder"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.7f))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SAssignNew(OutputFolderBox, SEditableTextBox)
				.Text(FText::FromString(DefaultOutputFolder))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(4))
				.OnClicked(this, &SControlRigToolWidget::OnBrowseFolderClicked)
				.ToolTipText(LOCTEXT("BrowseFolder", "Browse folder"))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("\U0001F4C2")))  // 📂
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
				]
			]
		];
}

TSharedRef<SWidget> SControlRigToolWidget::CreateButtonSection()
{
	return SNew(SVerticalBox)
		// 첫 번째 줄: Refresh + AI Mapping
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Button")
				.ContentPadding(FMargin(14, 8))
				.OnClicked(this, &SControlRigToolWidget::OnRefreshClicked)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Refresh"))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f))
						.DesiredSizeOverride(FVector2D(14, 14))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Refresh", "Refresh"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.ContentPadding(FMargin(20, 10))
				.HAlign(HAlign_Center)
				.OnClicked(this, &SControlRigToolWidget::OnAIBoneMappingClicked)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Toolbar.Settings"))
						.ColorAndOpacity(FLinearColor::White)
						.DesiredSizeOverride(FVector2D(16, 16))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AIMapping", "AI Bone Mapping"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
				]
			]
		]
		// 두 번째 줄: Create Body Control Rig (Step 1)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SAssignNew(BodyRigButton, SButton)
			.ButtonStyle(FAppStyle::Get(), "Button")
			.ContentPadding(FMargin(20, 12))
			.HAlign(HAlign_Center)
			.OnClicked(this, &SControlRigToolWidget::OnCreateBodyControlRigClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("ControlRig.RigUnit"))
					.ColorAndOpacity(FLinearColor(0.3f, 0.7f, 1.0f, 1.0f))
					.DesiredSizeOverride(FVector2D(18, 18))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CreateBodyRig", "1. Create Body Control Rig"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				]
			]
		]
		// 세 번째 줄: Approve Mapping
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "Button")
			.ContentPadding(FMargin(14, 8))
			.HAlign(HAlign_Center)
			.OnClicked(this, &SControlRigToolWidget::OnApproveMappingClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Check"))
					.ColorAndOpacity(FLinearColor(0.4f, 0.85f, 0.4f, 1.0f))
					.DesiredSizeOverride(FVector2D(14, 14))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ApproveMapping", "Approve Mapping (Save for Training)"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f))
				]
			]
		];
}

void SControlRigToolWidget::LoadAssetData()
{
	ControlRigs.Empty();
	SkeletalMeshes.Empty();
	TemplateOptions.Empty();
	MeshOptions.Empty();

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AR = ARM.Get();

	// Control Rig
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/ControlRigDeveloper"), TEXT("ControlRigBlueprint")));
		Filter.bRecursiveClasses = true;
		Filter.bRecursivePaths = true;
		TArray<FAssetData> Assets;
		AR.GetAssets(Filter, Assets);
		for (const FAssetData& A : Assets)
		{
			FAssetInfo Info;
			Info.Name = A.AssetName.ToString();
			Info.Path = A.PackageName.ToString();
			ControlRigs.Add(Info);
		}
	}

	// Skeletal Mesh
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("SkeletalMesh")));
		Filter.bRecursiveClasses = true;
		Filter.bRecursivePaths = true;
		TArray<FAssetData> Assets;
		AR.GetAssets(Filter, Assets);
		for (const FAssetData& A : Assets)
		{
			FAssetInfo Info;
			Info.Name = A.AssetName.ToString();
			Info.Path = A.PackageName.ToString();
			SkeletalMeshes.Add(Info);
		}
	}

	// 정렬
	ControlRigs.Sort([](const FAssetInfo& A, const FAssetInfo& B) { return A.Name < B.Name; });
	SkeletalMeshes.Sort([](const FAssetInfo& A, const FAssetInfo& B) { return A.Name < B.Name; });

	// 옵션 생성
	for (const FAssetInfo& Info : ControlRigs)
		TemplateOptions.Add(MakeShared<FString>(Info.Name));
	for (const FAssetInfo& Info : SkeletalMeshes)
		MeshOptions.Add(MakeShared<FString>(Info.Name));

	// 기본 선택 (Template 포함된 것)
	for (int32 i = 0; i < ControlRigs.Num(); ++i)
	{
		if (ControlRigs[i].Name.Contains(TEXT("Template")))
		{
			SelectedTemplate = TemplateOptions[i];
			break;
		}
	}
}

void SControlRigToolWidget::RefreshData()
{
	LoadAssetData();
	if (TemplateComboBox.IsValid()) TemplateComboBox->RefreshOptions();
	if (MeshComboBox.IsValid()) MeshComboBox->RefreshOptions();
	UpdateTemplateThumbnail();
	UpdateMeshThumbnail();
	SetStatus(FString::Printf(TEXT("Refreshed: %d templates, %d meshes"), ControlRigs.Num(), SkeletalMeshes.Num()));
}

void SControlRigToolWidget::UpdateTemplateThumbnail()
{
	if (!TemplateThumbnailBox.IsValid() || !ThumbnailPool.IsValid()) return;
	FString Path = GetSelectedTemplatePath();
	if (Path.IsEmpty())
	{
		TemplateThumbnailBox->SetContent(
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			[SNew(STextBlock).Text(FText::FromString(TEXT("CR"))).Justification(ETextJustify::Center)]
		);
		return;
	}
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetData = AR.GetAssetByObjectPath(FSoftObjectPath(Path + TEXT(".") + FPaths::GetBaseFilename(Path)));
	TemplateThumbnail = MakeShared<FAssetThumbnail>(AssetData, ThumbnailSize, ThumbnailSize, ThumbnailPool.ToSharedRef());
	TemplateThumbnailBox->SetContent(
		SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder")).Padding(2)
		[TemplateThumbnail->MakeThumbnailWidget()]
	);
}

void SControlRigToolWidget::UpdateMeshThumbnail()
{
	if (!MeshThumbnailBox.IsValid() || !ThumbnailPool.IsValid()) return;
	FString Path = GetSelectedMeshPath();
	if (Path.IsEmpty())
	{
		MeshThumbnailBox->SetContent(
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			[SNew(STextBlock).Text(FText::FromString(TEXT("SK"))).Justification(ETextJustify::Center)]
		);
		return;
	}
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetData = AR.GetAssetByObjectPath(FSoftObjectPath(Path + TEXT(".") + FPaths::GetBaseFilename(Path)));
	MeshThumbnail = MakeShared<FAssetThumbnail>(AssetData, ThumbnailSize, ThumbnailSize, ThumbnailPool.ToSharedRef());
	MeshThumbnailBox->SetContent(
		SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder")).Padding(2)
		[MeshThumbnail->MakeThumbnailWidget()]
	);
}

FString SControlRigToolWidget::GetSelectedTemplatePath() const
{
	if (!SelectedTemplate.IsValid()) return FString();
	for (const FAssetInfo& Info : ControlRigs)
		if (Info.Name == *SelectedTemplate) return Info.Path;
	return FString();
}

FString SControlRigToolWidget::GetSelectedMeshPath() const
{
	if (!SelectedMesh.IsValid()) return FString();
	for (const FAssetInfo& Info : SkeletalMeshes)
		if (Info.Name == *SelectedMesh) return Info.Path;
	return FString();
}

TSharedRef<SWidget> SControlRigToolWidget::OnGenerateTemplateWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(*InItem));
}

void SControlRigToolWidget::OnTemplateSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type)
{
	SelectedTemplate = NewValue;
	UpdateTemplateThumbnail();
}

FText SControlRigToolWidget::GetSelectedTemplateName() const
{
	return SelectedTemplate.IsValid() ? FText::FromString(*SelectedTemplate) : LOCTEXT("SelectTemplate", "Select...");
}

TSharedRef<SWidget> SControlRigToolWidget::OnGenerateMeshWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(*InItem));
}

void SControlRigToolWidget::OnMeshSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type)
{
	SelectedMesh = NewValue;
	UpdateMeshThumbnail();
	if (SelectedMesh.IsValid() && OutputNameBox.IsValid())
		OutputNameBox->SetText(FText::FromString(FString::Printf(TEXT("CTR_%s_Rig"), **SelectedMesh)));
}

FText SControlRigToolWidget::GetSelectedMeshName() const
{
	return SelectedMesh.IsValid() ? FText::FromString(*SelectedMesh) : LOCTEXT("SelectMesh", "Select...");
}

FReply SControlRigToolWidget::OnRefreshClicked()
{
	RefreshData();
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnUseSelectedTemplateClicked()
{
	TArray<FAssetData> Selected;
	GEditor->GetContentBrowserSelections(Selected);
	for (const FAssetData& A : Selected)
	{
		if (A.AssetClassPath.GetAssetName() == TEXT("ControlRigBlueprint"))
		{
			FString Path = A.PackageName.ToString();
			for (int32 i = 0; i < ControlRigs.Num(); ++i)
			{
				if (ControlRigs[i].Path == Path)
				{
					SelectedTemplate = TemplateOptions[i];
					if (TemplateComboBox.IsValid()) TemplateComboBox->SetSelectedItem(SelectedTemplate);
					UpdateTemplateThumbnail();
					SetStatus(FString::Printf(TEXT("Template: %s"), *A.AssetName.ToString()));
					return FReply::Handled();
				}
			}
		}
	}
	SetStatus(TEXT("Select a Control Rig in Content Browser"));
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnUseSelectedMeshClicked()
{
	TArray<FAssetData> Selected;
	GEditor->GetContentBrowserSelections(Selected);
	for (const FAssetData& A : Selected)
	{
		if (A.AssetClassPath.GetAssetName() == TEXT("SkeletalMesh"))
		{
			FString Path = A.PackageName.ToString();
			for (int32 i = 0; i < SkeletalMeshes.Num(); ++i)
			{
				if (SkeletalMeshes[i].Path == Path)
				{
					SelectedMesh = MeshOptions[i];
					if (MeshComboBox.IsValid()) MeshComboBox->SetSelectedItem(SelectedMesh);
					UpdateMeshThumbnail();
					if (OutputNameBox.IsValid())
						OutputNameBox->SetText(FText::FromString(FString::Printf(TEXT("CTR_%s_Rig"), *A.AssetName.ToString())));
					SetStatus(FString::Printf(TEXT("Mesh: %s"), *A.AssetName.ToString()));
					return FReply::Handled();
				}
			}
		}
	}
	SetStatus(TEXT("Select a Skeletal Mesh in Content Browser"));
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnBrowseFolderClicked()
{
	FContentBrowserModule& CBM = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FPathPickerConfig Config;
	Config.DefaultPath = OutputFolderBox->GetText().ToString();
	Config.OnPathSelected = FOnPathSelected::CreateLambda([this](const FString& Path)
	{
		if (OutputFolderBox.IsValid()) OutputFolderBox->SetText(FText::FromString(Path));
	});

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("SelectFolder", "Select Output Folder"))
		.ClientSize(FVector2D(400, 500));

	Window->SetContent(
		SNew(SBorder).Padding(8)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[CBM.Get().CreatePathPicker(Config)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
			[
				SNew(SButton).Text(LOCTEXT("OK", "OK")).HAlign(HAlign_Center)
				.OnClicked_Lambda([Window]() { Window->RequestDestroyWindow(); return FReply::Handled(); })
			]
		]
	);
	FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow());
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnAIBoneMappingClicked()
{
	RequestAIBoneMapping();
	return FReply::Handled();
}

void SControlRigToolWidget::RequestAIBoneMapping()
{
	FString MeshPath = GetSelectedMeshPath();
	if (MeshPath.IsEmpty())
	{
		SetStatus(TEXT("ERROR: Select a mesh first"));
		return;
	}

	USkeletalMesh* Mesh = Cast<USkeletalMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
	if (!Mesh)
	{
		SetStatus(TEXT("ERROR: Failed to load mesh"));
		return;
	}
	CachedMesh = Mesh;
	SetStatus(TEXT("Requesting AI mapping..."));

	const FReferenceSkeleton& Skel = Mesh->GetRefSkeleton();
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Bones;

	for (int32 i = 0; i < Skel.GetNum(); i++)
	{
		const FMeshBoneInfo& Info = Skel.GetRefBoneInfo()[i];
		TSharedPtr<FJsonObject> Bone = MakeShared<FJsonObject>();
		Bone->SetStringField("name", Info.Name.ToString());
		if (Info.ParentIndex >= 0)
			Bone->SetStringField("parent", Skel.GetBoneName(Info.ParentIndex).ToString());
		TArray<TSharedPtr<FJsonValue>> Children;
		for (int32 j = 0; j < Skel.GetNum(); j++)
			if (Skel.GetRefBoneInfo()[j].ParentIndex == i)
				Children.Add(MakeShared<FJsonValueString>(Skel.GetBoneName(j).ToString()));
		Bone->SetArrayField("children", Children);
		Bones.Add(MakeShared<FJsonValueObject>(Bone));
	}
	Root->SetArrayField("bones", Bones);
	Root->SetBoolField("use_ai", true);

	FString Body;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root.ToSharedRef(), W);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(TEXT("http://localhost:8000/predict"));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(Body);
	Req->SetTimeout(120.0f);

	Req->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Res, bool Ok)
	{
		if (!Ok || !Res.IsValid()) { SetStatus(TEXT("ERROR: Server connection failed")); return; }
		TSharedPtr<FJsonObject> J;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Res->GetContentAsString());
		if (!FJsonSerializer::Deserialize(R, J)) { SetStatus(TEXT("ERROR: Parse failed")); return; }

		LastBoneMapping.Empty();
		const TSharedPtr<FJsonObject>* Map;
		if (J->TryGetObjectField(TEXT("mapping"), Map))
		{
			for (const auto& P : (*Map)->Values)
			{
				FString V;
				if (P.Value->TryGetString(V))
					LastBoneMapping.Add(FName(*P.Key), FName(*V));
			}
		}
		SetStatus(FString::Printf(TEXT("SUCCESS: %d mappings"), LastBoneMapping.Num()));
		DisplayMappingResults();
		
		// 본 매핑 완료 후 본 선택 UI 표시 및 세컨더리 버튼 활성화
		BuildBoneDisplayList();
		UpdateBoneSelectionUI();
		if (SecondaryOnlyButton.IsValid())
		{
			SecondaryOnlyButton->SetEnabled(true);
		}
	});
	Req->ProcessRequest();
}

// ============================================================================
// Step 1: Body Control Rig 생성 (저장 안 함, 본 선택 UI 표시)
// ============================================================================
FReply SControlRigToolWidget::OnCreateBodyControlRigClicked()
{
	if (CreateBodyControlRig())
	{
		SetStatus(TEXT("Body Control Rig ready! Select secondary bones below."));
		// 본 선택 UI 표시
		BuildBoneDisplayList();
		UpdateBoneSelectionUI();
		// 최종 버튼 활성화
		if (FinalCreateButton.IsValid())
		{
			FinalCreateButton->SetEnabled(true);
		}
		// 세컨더리 전용 버튼도 활성화
		if (SecondaryOnlyButton.IsValid())
		{
			SecondaryOnlyButton->SetEnabled(true);
		}
		CurrentStep = EControlRigWorkflowStep::Step3_SelectBones;
	}
	return FReply::Handled();
}

bool SControlRigToolWidget::CreateBodyControlRig()
{
	if (LastBoneMapping.Num() == 0)
	{
		SetStatus(TEXT("ERROR: Run AI Bone Mapping first"));
		return false;
	}
	
	FString TemplatePath = GetSelectedTemplatePath();
	FString MeshPath = GetSelectedMeshPath();
	
	if (TemplatePath.IsEmpty())
	{
		SetStatus(TEXT("ERROR: Select a template"));
		return false;
	}
	if (MeshPath.IsEmpty())
	{
		SetStatus(TEXT("ERROR: Select a mesh"));
		return false;
	}

	FString OutputName = OutputNameBox->GetText().ToString();
	FString OutputFolder = OutputFolderBox->GetText().ToString();
	PendingOutputPath = OutputFolder / OutputName;

	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Creating Body Rig: %s"), *PendingOutputPath);

	// 1. 메시 로드
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
	if (!Mesh) { SetStatus(TEXT("ERROR: Failed to load mesh")); return false; }
	CachedMesh = Mesh;

	// 2. 템플릿 확인
	if (!UEditorAssetLibrary::DoesAssetExist(TemplatePath))
	{
		SetStatus(TEXT("ERROR: Template not found"));
		return false;
	}

	// 3. 기존 에셋 삭제 + 템플릿 복제
	if (UEditorAssetLibrary::DoesAssetExist(PendingOutputPath))
	{
		UEditorAssetLibrary::DeleteAsset(PendingOutputPath);
	}

	UObject* Duplicated = UEditorAssetLibrary::DuplicateAsset(TemplatePath, PendingOutputPath);
	if (!Duplicated) { SetStatus(TEXT("ERROR: Duplication failed")); return false; }

	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Template duplicated"));

	// 4. Control Rig 로드
	UControlRigBlueprint* Rig = Cast<UControlRigBlueprint>(UEditorAssetLibrary::LoadAsset(PendingOutputPath));
	if (!Rig) { SetStatus(TEXT("ERROR: Failed to load Control Rig")); return false; }
	
	PendingControlRig = Rig;

	URigHierarchyController* HC = Rig->GetHierarchyController();
	URigHierarchy* Hierarchy = Rig->Hierarchy;

	// 5. 기존 본 삭제 (역순) - 단, 템플릿 전용 본은 유지
	TSet<FString> PreserveBones = {
		TEXT("heel_l"), TEXT("heel_r"), 
		TEXT("tip_l"), TEXT("tip_r"),
		TEXT("ik_foot_l"), TEXT("ik_foot_r"),
		TEXT("ik_hand_l"), TEXT("ik_hand_r"),
		TEXT("ik_foot_root"), TEXT("ik_hand_root"), TEXT("ik_hand_gun")
	};
	
	TArray<FRigBoneElement*> OldBones = Hierarchy->GetBones();
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Removing old bones (preserving template-only bones)"));
	int32 RemovedCount = 0;
	for (int32 i = OldBones.Num() - 1; i >= 0; --i)
	{
		FString BoneName = OldBones[i]->GetKey().Name.ToString().ToLower();
		if (!PreserveBones.Contains(BoneName))
		{
			HC->RemoveElement(OldBones[i]->GetKey(), false, false);
			RemovedCount++;
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("  Preserved: %s"), *OldBones[i]->GetKey().Name.ToString());
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Removed %d bones, preserved %d"), RemovedCount, OldBones.Num() - RemovedCount);

	// 6. 메시 교체 + 새 본 임포트
	Rig->SetPreviewMesh(Mesh, true);

	USkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton) { SetStatus(TEXT("ERROR: No skeleton")); return false; }

	TArray<FRigElementKey> ImportedBones = HC->ImportBones(
		Skeleton,
		NAME_None,
		true,   // bReplaceExistingBones
		false,  // bRemoveObsoleteBones
		false,  // bSelectBones
		true,   // bCreateNulls
		false   // bUseCustomNameSetting
	);
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Imported %d bones"), ImportedBones.Num());

	// 6.5. IK 본들만 올바른 부모에 연결 (heel_l, tip_l 등은 템플릿 원본대로 유지)
	TMap<FString, FString> IKBoneParents = {
		{TEXT("ik_foot_l"), TEXT("ik_foot_root")},
		{TEXT("ik_foot_r"), TEXT("ik_foot_root")},
		{TEXT("ik_hand_l"), TEXT("ik_hand_root")},
		{TEXT("ik_hand_r"), TEXT("ik_hand_root")},
	};

	for (const auto& Pair : IKBoneParents)
	{
		FString BoneName = Pair.Key;
		FString LogicalParent = Pair.Value;
		
		FRigElementKey BoneKey(FName(*BoneName), ERigElementType::Bone);
		FRigElementKey ParentKey(FName(*LogicalParent), ERigElementType::Bone);
		
		// IK 본이 있고 부모도 있으면 연결
		if (Hierarchy->Contains(BoneKey) && Hierarchy->Contains(ParentKey))
		{
			HC->SetParent(BoneKey, ParentKey, true);
			UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] IK bone connected: %s -> %s"), *BoneName, *LogicalParent);
		}
	}

	// 7. 본 참조 리매핑 (LastBoneMapping 사용)
	RemapBoneReferences(Rig);
	
	// 8. 바디 컨트롤러 오토스케일 적용 (메쉬 크기에 맞게)
	ApplyAutoScaleToBodyControls(Rig, Mesh);
	
	// 저장은 아직 안 함 - 세컨더리 선택 후 최종 버튼에서 저장
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Body Rig ready (not saved yet)"));
	
	return true;
}

// ============================================================================
// Step 2: 최종 Control Rig 생성 (세컨더리 추가 + 저장)
// ============================================================================
FReply SControlRigToolWidget::OnCreateFinalControlRigClicked()
{
	if (CreateFinalControlRig())
	{
		SetStatus(TEXT("Control Rig created and saved!"));
		
		// 컨트롤릭 생성 성공 = 매핑 성공이므로 자동으로 AI 학습 데이터 전송
		if (LastBoneMapping.Num() > 0)
		{
			SendApproveRequest();
			UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Auto-approved mapping after Control Rig creation"));
		}
	}
	return FReply::Handled();
}

bool SControlRigToolWidget::CreateFinalControlRig()
{
	if (!PendingControlRig.IsValid())
	{
		SetStatus(TEXT("ERROR: Create Body Control Rig first"));
		return false;
	}
	
	if (!CachedMesh.IsValid())
	{
		SetStatus(TEXT("ERROR: Mesh not found"));
		return false;
	}
	
	UControlRigBlueprint* Rig = PendingControlRig.Get();
	USkeletalMesh* Mesh = CachedMesh.Get();
	
	// 사용자가 선택한 세컨더리 본으로 컨트롤러 생성
	CreateSecondaryControlsFromSelection(Rig, Mesh);

	// 저장
	Rig->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(PendingOutputPath);

	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Saved: %s"), *PendingOutputPath);

	// 상태 업데이트
	CurrentStep = EControlRigWorkflowStep::Step4_Complete;
	PendingControlRig.Reset();

	// 결과 다이얼로그
	FString Msg = FString::Printf(TEXT("Control Rig Created!\n\nPath: %s\nCore Mappings: %d\nSecondary Controls: %d"), 
		*PendingOutputPath, LastBoneMapping.Num(), LastSecondaryControlCount);
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Msg));
	return true;
}

// ============================================================================
// 세컨더리 전용 Control Rig 생성 (템플릿 없이)
// Head, Hair, Armor 등 부분 메쉬용
// ============================================================================
FReply SControlRigToolWidget::OnCreateSecondaryOnlyControlRigClicked()
{
	if (CreateSecondaryOnlyControlRig())
		SetStatus(TEXT("Secondary Only Control Rig created!"));
	return FReply::Handled();
}

bool SControlRigToolWidget::CreateSecondaryOnlyControlRig()
{
	// 1. 메쉬 확인
	if (!CachedMesh.IsValid())
	{
		SetStatus(TEXT("ERROR: Select a mesh first and run AI Bone Mapping"));
		return false;
	}
	
	USkeletalMesh* Mesh = CachedMesh.Get();
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// 2. 세컨더리로 선택된 본 수집
	TArray<FName> SelectedSecondaryBones;
	for (const FBoneDisplayInfo& Info : BoneDisplayList)
	{
		if (Info.Classification == EBoneClassification::Secondary)
		{
			SelectedSecondaryBones.Add(Info.BoneName);
		}
	}
	
	if (SelectedSecondaryBones.Num() == 0)
	{
		SetStatus(TEXT("ERROR: No secondary bones selected"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[SecondaryOnly] Creating Control Rig with %d secondary bones"), SelectedSecondaryBones.Num());
	
	// 3. 출력 경로 설정
	FString OutputName = OutputNameBox->GetText().ToString() + TEXT("_Secondary");
	FString OutputFolder = OutputFolderBox->GetText().ToString();
	FString OutputPath = OutputFolder / OutputName;
	
	// 기존 에셋 삭제
	if (UEditorAssetLibrary::DoesAssetExist(OutputPath))
	{
		UEditorAssetLibrary::DeleteAsset(OutputPath);
	}
	
	// 4. 새 Control Rig Blueprint 생성 (템플릿 없이)
	UControlRigBlueprintFactory* Factory = NewObject<UControlRigBlueprintFactory>();
	Factory->ParentClass = UControlRig::StaticClass();
	
	FString PackageName = OutputPath;
	FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
	
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		SetStatus(TEXT("ERROR: Failed to create package"));
		return false;
	}
	
	UControlRigBlueprint* NewRig = Cast<UControlRigBlueprint>(
		Factory->FactoryCreateNew(
			UControlRigBlueprint::StaticClass(),
			Package,
			FName(*AssetName),
			RF_Public | RF_Standalone,
			nullptr,
			GWarn
		)
	);
	
	if (!NewRig)
	{
		SetStatus(TEXT("ERROR: Failed to create Control Rig Blueprint"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[SecondaryOnly] Created new Control Rig: %s"), *OutputPath);
	
	// 5. Preview Mesh 설정 및 본 임포트
	NewRig->SetPreviewMesh(Mesh, true);
	
	URigHierarchyController* HC = NewRig->GetHierarchyController();
	URigHierarchy* Hierarchy = NewRig->Hierarchy;
	
	if (!HC || !Hierarchy)
	{
		SetStatus(TEXT("ERROR: Failed to get hierarchy controller"));
		return false;
	}
	
	USkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton)
	{
		SetStatus(TEXT("ERROR: No skeleton"));
		return false;
	}
	
	// 본 임포트
	TArray<FRigElementKey> ImportedBones = HC->ImportBones(
		Skeleton,
		NAME_None,
		true,   // bReplaceExistingBones
		false,  // bRemoveObsoleteBones
		false,  // bSelectBones
		true,   // bCreateNulls
		false   // bUseCustomNameSetting
	);
	UE_LOG(LogTemp, Log, TEXT("[SecondaryOnly] Imported %d bones"), ImportedBones.Num());
	
	// 6. 버텍스 기반 Shape Info 계산
	CalculateBoneShapeInfos(Mesh);
	
	// 7. Space별로 세컨더리 본 그룹화
	TMap<FName, TArray<FName>> ChainsBySpace;
	for (const FName& BoneName : SelectedSecondaryBones)
	{
		FName SpaceParent = FindZeroBoneParent(BoneName, RefSkel);
		if (SpaceParent.IsNone())
		{
			SpaceParent = FName(TEXT("root"));
		}
		ChainsBySpace.FindOrAdd(SpaceParent).Add(BoneName);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[SecondaryOnly] Grouped into %d spaces"), ChainsBySpace.Num());
	
	// 8. Space 및 Control 생성
	LastSecondaryControlCount = 0;
	for (const auto& Pair : ChainsBySpace)
	{
		FName SpaceParentName = Pair.Key;
		const TArray<FName>& ChainBones = Pair.Value;
		
		// Space Null 생성
		FString SpaceNameStr = SpaceParentName.ToString() + TEXT("_space");
		FName SpaceFName(*SpaceNameStr);
		
		// Space 트랜스폼 (부모 본 위치)
		FTransform SpaceTransform = FTransform::Identity;
		int32 SpaceBoneIdx = RefSkel.FindBoneIndex(SpaceParentName);
		if (SpaceBoneIdx != INDEX_NONE)
		{
			SpaceTransform = RefSkel.GetRefBonePose()[SpaceBoneIdx];
		}
		
		CreateSpaceNull(HC, SpaceFName, SpaceTransform);
		
		// 각 본에 컨트롤러 생성
		CreateChainControls(HC, Hierarchy, SpaceFName, ChainBones, RefSkel);
		
		UE_LOG(LogTemp, Log, TEXT("[SecondaryOnly] Created space '%s' with %d controls"), *SpaceNameStr, ChainBones.Num());
	}
	
	// 9. AI 함수 노드 연결 (AI_Setup, AI_Forward, AI_Backward)
	ConnectSecondaryFunctionNodes(NewRig, ChainsBySpace);
	
	// 10. 컴파일 및 저장
	FBlueprintEditorUtils::MarkBlueprintAsModified(NewRig);
	NewRig->MarkPackageDirty();
	
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewRig, *FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()), SaveArgs);
	
	UE_LOG(LogTemp, Log, TEXT("[SecondaryOnly] Saved: %s"), *OutputPath);
	
	// 결과 다이얼로그
	FString Msg = FString::Printf(TEXT("Secondary Only Control Rig Created!\n\nPath: %s\nSpaces: %d\nSecondary Controls: %d"), 
		*OutputPath, ChainsBySpace.Num(), LastSecondaryControlCount);
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Msg));
	
	return true;
}

// ============================================================================
// 세컨더리 본에서 가장 가까운 "Space 대상" 부모 찾기
// Space 대상: 제로본 + root + bip001 등 최상위 본
// 반환값: Space 이름으로 사용할 본 이름 (예: head, pelvis, bip001, root 등)
// ============================================================================
FName SControlRigToolWidget::FindZeroBoneParent(const FName& BoneName, const FReferenceSkeleton& RefSkel) const
{
	int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE) return NAME_None;
	
	// 부모를 따라 올라가면서 Space 대상 본을 찾음
	int32 ParentIndex = RefSkel.GetParentIndex(BoneIndex);
	while (ParentIndex != INDEX_NONE)
	{
		FName ParentName = RefSkel.GetBoneName(ParentIndex);
		FString ParentNameStr = ParentName.ToString();
		FString LowerName = ParentNameStr.ToLower();
		
		// 1. 제로본 (UE5 표준 본)인지 확인 - UE5 표준 이름으로 반환
		if (IsZeroBone(ParentNameStr))
		{
			// LastBoneMapping에서 UE5 표준 이름 찾기 (source -> target)
			for (const auto& Pair : LastBoneMapping)
			{
				if (Pair.Value.ToString().Equals(ParentNameStr, ESearchCase::IgnoreCase))
				{
					return Pair.Key;
				}
			}
			// 매핑 없으면 원래 이름 (소문자로 정규화)
			return FName(*LowerName);
		}
		
		// 2. bip001 계열 본 - "bip001"로 Space 생성
		if (LowerName.Contains(TEXT("bip001")) || LowerName.Contains(TEXT("bip_001")) ||
			LowerName.Contains(TEXT("bip-001")) || LowerName.Equals(TEXT("bip")))
		{
			return FName(TEXT("bip001"));
		}
		
		// 3. root 본 - "root"로 Space 생성
		if (LowerName.Equals(TEXT("root")) || LowerName.Contains(TEXT("armature")))
		{
			return FName(TEXT("root"));
		}
		
		ParentIndex = RefSkel.GetParentIndex(ParentIndex);
	}
	
	// 최상위까지 갔는데 못 찾으면 root로
	return FName(TEXT("root"));
}

// ============================================================================
// 세컨더리 체인 수집: Space별로 그룹화
// OutChainsBySpace: SpaceName -> 해당 Space 아래에 속할 본들 (체인 순서)
// ============================================================================
// ============================================================================
// 스킨 웨이트가 있는 본인지 확인 (ActiveBoneIndices 사용)
// ============================================================================
bool SControlRigToolWidget::HasSkinWeight(USkeletalMesh* Mesh, const FName& BoneName) const
{
	if (!Mesh) return false;
	
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE) return false;
	
	// 스킨 웨이트 데이터 확인 (LOD 0 기준)
	if (Mesh->GetResourceForRendering() && Mesh->GetResourceForRendering()->LODRenderData.Num() > 0)
	{
		const FSkeletalMeshLODRenderData& LODData = Mesh->GetResourceForRendering()->LODRenderData[0];
		
		// ActiveBoneIndices에 포함되어 있어야 실제 스킨 웨이트가 있음
		// (RequiredBones는 계층상 필요한 본이고, ActiveBoneIndices는 실제 스킨된 본)
		for (int32 i = 0; i < LODData.ActiveBoneIndices.Num(); ++i)
		{
			if (LODData.ActiveBoneIndices[i] == BoneIndex)
			{
				return true;
			}
		}
	}
	
	return false;
}

void SControlRigToolWidget::BuildSecondaryChains(USkeletalMesh* Mesh, TMap<FName, TArray<FName>>& OutChainsBySpace)
{
	OutChainsBySpace.Empty();
	if (!Mesh) return;
	
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// 1. 모든 세컨더리 본 수집
	// 조건: 제로본 아님 + 헬퍼본 아님 + 스킨 웨이트 있음
	TArray<FName> SecondaryBones;
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		FName BoneName = RefSkel.GetBoneName(i);
		FString BoneNameStr = BoneName.ToString();
		
		// 제로본은 제외 (이미 템플릿에 있음)
		if (IsZeroBone(BoneNameStr))
		{
			continue;
		}
		
		// 헬퍼본은 제외 (twist, corrective 등)
		if (IsHelperBone(BoneNameStr))
		{
			UE_LOG(LogTemp, Verbose, TEXT("  [SKIP Helper] %s"), *BoneNameStr);
			continue;
		}
		
		// 스킨 웨이트가 없으면 제외 (비어있는 본)
		if (!HasSkinWeight(Mesh, BoneName))
		{
			UE_LOG(LogTemp, Verbose, TEXT("  [SKIP No Skin] %s"), *BoneNameStr);
			continue;
		}
		
		// 조건을 만족하면 세컨더리 본으로 수집
		SecondaryBones.Add(BoneName);
		UE_LOG(LogTemp, Verbose, TEXT("  [Secondary] %s"), *BoneNameStr);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Collected %d secondary bones"), SecondaryBones.Num());
	
	// 2. 각 세컨더리 본의 제로본 부모 찾아서 그룹화
	for (const FName& BoneName : SecondaryBones)
	{
		FName ZeroParent = FindZeroBoneParent(BoneName, RefSkel);
		if (ZeroParent.IsNone())
		{
			// 제로본 부모가 없으면 root_space에 넣음
			ZeroParent = FName(TEXT("root"));
		}
		
		FName SpaceName = FName(*FString::Printf(TEXT("%s_space"), *ZeroParent.ToString()));
		OutChainsBySpace.FindOrAdd(SpaceName).Add(BoneName);
	}
	
	// 3. 각 Space 내 본들을 계층 순서로 정렬 (부모가 먼저)
	for (auto& Pair : OutChainsBySpace)
	{
		TArray<FName>& Bones = Pair.Value;
		
		// 인덱스 기준 정렬 (본 계층 순서)
		Bones.Sort([&RefSkel](const FName& A, const FName& B)
		{
			return RefSkel.FindBoneIndex(A) < RefSkel.FindBoneIndex(B);
		});
	}
}

// ============================================================================
// Space(Null) 생성 - body_offset_ctrl 밑에 생성
// ============================================================================
void SControlRigToolWidget::CreateSpaceNull(URigHierarchyController* HC, const FName& SpaceName, const FTransform& Transform)
{
	if (!HC) return;
	
	URigHierarchy* Hierarchy = HC->GetHierarchy();
	if (!Hierarchy) return;
	
	FRigElementKey SpaceKey(SpaceName, ERigElementType::Null);
	if (Hierarchy->Contains(SpaceKey))
	{
		UE_LOG(LogTemp, Log, TEXT("  Space already exists: %s"), *SpaceName.ToString());
		return;
	}
	
	// body_offset_ctrl 밑에 생성
	FRigElementKey ParentKey(FName(TEXT("body_offset_ctrl")), ERigElementType::Control);
	
	// body_offset_ctrl이 없으면 최상위에 생성
	if (!Hierarchy->Contains(ParentKey))
	{
		UE_LOG(LogTemp, Warning, TEXT("  body_offset_ctrl not found, creating Space at root level"));
		ParentKey = FRigElementKey(); // 빈 키 = 최상위
	}
	
	FRigElementKey NewSpaceKey = HC->AddNull(
		SpaceName,
		ParentKey,
		Transform,
		false  // bSetupUndo
	);
	
	if (NewSpaceKey.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("  Created Space (Null): %s"), *SpaceName.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("  Failed to create Space: %s"), *SpaceName.ToString());
	}
}

// ============================================================================
// 체인 컨트롤러 생성 - Space 아래에 체인 구조로 컨트롤러 생성
// ============================================================================
void SControlRigToolWidget::CreateChainControls(URigHierarchyController* HC, URigHierarchy* Hierarchy,
	const FName& SpaceName, const TArray<FName>& ChainBones, const FReferenceSkeleton& RefSkel)
{
	if (!HC || !Hierarchy || ChainBones.Num() == 0) return;
	
	// 각 본의 부모 본 -> 컨트롤 매핑 (체인 구조 유지용)
	TMap<FName, FName> BoneToControlMap;
	
	for (const FName& BoneName : ChainBones)
	{
		FString BoneNameStr = BoneName.ToString();
		FString ControlName = FString::Printf(TEXT("%s_ctrl"), *BoneNameStr);
		FName ControlFName(*ControlName);
		
		FRigElementKey ControlKey(ControlFName, ERigElementType::Control);
		if (Hierarchy->Contains(ControlKey))
		{
			BoneToControlMap.Add(BoneName, ControlFName);
			continue;
		}
		
		// 부모 결정: 
		// 1. 스켈레톤에서 부모 본을 찾음
		// 2. 부모 본의 컨트롤러가 있으면 그것을 부모로
		// 3. 없으면 Space를 부모로
		int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		int32 ParentBoneIndex = RefSkel.GetParentIndex(BoneIndex);
		
		FRigElementKey ParentKey;
		
		if (ParentBoneIndex != INDEX_NONE)
		{
			FName ParentBoneName = RefSkel.GetBoneName(ParentBoneIndex);
			
			// 부모 본이 액세서리 본이고, 해당 컨트롤러가 있으면 그것을 부모로
			if (FName* ParentControlName = BoneToControlMap.Find(ParentBoneName))
			{
				ParentKey = FRigElementKey(*ParentControlName, ERigElementType::Control);
			}
		}
		
		// 부모 컨트롤러가 없으면 Space를 부모로
		if (!ParentKey.IsValid())
		{
			ParentKey = FRigElementKey(SpaceName, ERigElementType::Null);
		}
		
		// 본의 트랜스폼 가져오기
		FRigElementKey BoneKey(BoneName, ERigElementType::Bone);
		FTransform BoneTransform = FTransform::Identity;
		if (Hierarchy->Contains(BoneKey))
		{
			BoneTransform = Hierarchy->GetGlobalTransform(BoneKey);
		}
		
		// 컨트롤러 설정
		FRigControlSettings ControlSettings;
		ControlSettings.ControlType = ERigControlType::Transform;
		ControlSettings.DisplayName = BoneName;
		ControlSettings.AnimationType = ERigControlAnimationType::AnimationControl;
		
		// Shape 설정: Box_Solid, 작은 스케일, 노멀 방향 오프셋
		FTransform ShapeTransform = FTransform::Identity;
		FBoneShapeInfo ShapeInfo = GetBoneShapeInfo(BoneName);
		
		// 모든 세컨더리 컨트롤러: Box_Solid, 스케일 0.2, 메쉬에서 적당히 떨어뜨림
		ControlSettings.ShapeName = FName(TEXT("Box_Solid"));
		
		// 노멀 방향으로 오프셋 (메쉬에서 조금 떨어뜨림)
		float OffsetDistance = 5.0f;  // 메쉬에서 적당히 떨어뜨림
		FVector NormalOffset = ShapeInfo.AverageNormal * OffsetDistance;
		ShapeTransform.SetLocation(NormalOffset);
		
		// 스케일: XYZ 모두 0.2 (큐브)
		ShapeTransform.SetScale3D(FVector(0.2f, 0.2f, 0.2f));
		
		UE_LOG(LogTemp, Log, TEXT("    %s: Default, Scale=0.05, Normal=(%.2f, %.2f, %.2f), Offset=(%.1f, %.1f, %.1f)"), 
			*BoneNameStr, 
			ShapeInfo.AverageNormal.X, ShapeInfo.AverageNormal.Y, ShapeInfo.AverageNormal.Z,
			NormalOffset.X, NormalOffset.Y, NormalOffset.Z);
		
		// 컨트롤러 추가
		FRigElementKey NewControlKey = HC->AddControl(
			ControlFName,
			ParentKey,
			ControlSettings,
			FRigControlValue::Make<FTransform>(FTransform::Identity),
			FTransform::Identity,  // Offset Transform
			ShapeTransform,        // Shape Transform
			false                  // bSetupUndo
		);
		
		if (NewControlKey.IsValid())
		{
			BoneToControlMap.Add(BoneName, ControlFName);
			LastSecondaryControlCount++;
			UE_LOG(LogTemp, Log, TEXT("    %s_ctrl (parent: %s)"), *BoneNameStr, *ParentKey.Name.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("    Failed to create: %s_ctrl"), *BoneNameStr);
		}
	}
}

// CreateSecondaryControls는 CreateSecondaryControlsFromSelection으로 대체됨

void SControlRigToolWidget::RemapBoneReferences(UControlRigBlueprint* Rig)
{
	if (!Rig || LastBoneMapping.Num() == 0) return;

	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Remapping bone references..."));

	// LastBoneMapping: target(UE5 표준) -> source(실제 메시 본)
	// 템플릿의 노드들은 target 본 이름을 사용 중 (예: thigh_l)
	// ERigElementType::Bone인 경우만 source 본 이름으로 변경
	// ERigElementType::Control인 경우는 변경하지 않음!

	URigVMController* VMController = Rig->GetController();
	if (!VMController) return;

	URigVMGraph* Graph = VMController->GetGraph();
	if (!Graph) return;

	int32 RemappedCount = 0;

	// 먼저 모든 핀 값 형식 로깅 (디버깅용)
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] === Scanning all pins ==="));
	for (URigVMNode* Node : Graph->GetNodes())
	{
		if (!Node) continue;

		for (URigVMPin* Pin : Node->GetPins())
		{
			if (!Pin) continue;

			FString PinName = Pin->GetName();
			FString DefaultValue = Pin->GetDefaultValue();

			if (DefaultValue.IsEmpty()) continue;

			// 매핑 테이블의 본 이름을 포함하는 핀만 로깅
			for (const auto& Pair : LastBoneMapping)
			{
				if (DefaultValue.Contains(Pair.Key.ToString()))
				{
					UE_LOG(LogTemp, Log, TEXT("  [DEBUG] Node=%s, Pin=%s, Value=%s"), 
						*Node->GetName(), *PinName, *DefaultValue.Left(200));
					break;
				}
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] === End scan ==="));

	for (URigVMNode* Node : Graph->GetNodes())
	{
		if (!Node) continue;

		for (URigVMPin* Pin : Node->GetPins())
		{
			if (!Pin) continue;

			FString PinName = Pin->GetName();
			FString DefaultValue = Pin->GetDefaultValue();

			if (DefaultValue.IsEmpty()) continue;

			// Control 타입이면 스킵 (컨트롤러 참조는 변경하지 않음)
			if (DefaultValue.Contains(TEXT("Type=Control")) ||
				DefaultValue.Contains(TEXT("ERigElementType::Control")))
			{
				continue;
			}

			// 매핑 테이블에서 찾기
			FString NewValue = DefaultValue;
			bool AnyRemapped = false;

			for (const auto& Pair : LastBoneMapping)
			{
				FString TargetBone = Pair.Key.ToString();  // UE5 표준 (템플릿에서 사용)
				FString SourceBone = Pair.Value.ToString(); // 실제 메시 본

				// 다양한 패턴으로 매칭
				TArray<TPair<FString, FString>> Patterns = {
					{FString::Printf(TEXT("Name=\"%s\""), *TargetBone), FString::Printf(TEXT("Name=\"%s\""), *SourceBone)},
					{FString::Printf(TEXT("Name=%s,"), *TargetBone), FString::Printf(TEXT("Name=%s,"), *SourceBone)},
					{FString::Printf(TEXT("Name=%s)"), *TargetBone), FString::Printf(TEXT("Name=%s)"), *SourceBone)},
					{FString::Printf(TEXT("\"Name\":\"%s\""), *TargetBone), FString::Printf(TEXT("\"Name\":\"%s\""), *SourceBone)},
					{FString::Printf(TEXT("Bone(%s)"), *TargetBone), FString::Printf(TEXT("Bone(%s)"), *SourceBone)},
				};

				for (const auto& P : Patterns)
				{
					if (NewValue.Contains(P.Key))
					{
						NewValue = NewValue.Replace(*P.Key, *P.Value);
						AnyRemapped = true;
						UE_LOG(LogTemp, Log, TEXT("  [%s.%s] %s -> %s"), *Node->GetName(), *PinName, *TargetBone, *SourceBone);
					}
				}
			}

			if (AnyRemapped)
			{
				VMController->SetPinDefaultValue(Pin->GetPinPath(), NewValue, true, false, false);
				RemappedCount++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Remapped %d bone references"), RemappedCount);
}

void SControlRigToolWidget::SetStatus(const FString& Message)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(Message.Contains(TEXT("ERROR")) ? FLinearColor::Red : FLinearColor::White);
	}
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] %s"), *Message);
}

void SControlRigToolWidget::DisplayMappingResults()
{
	if (!MappingResultBox.IsValid()) return;
	MappingResultBox->ClearChildren();
	TArray<FName> Keys;
	LastBoneMapping.GetKeys(Keys);
	Keys.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	for (const FName& K : Keys)
	{
		MappingResultBox->AddSlot().AutoHeight().Padding(2)
		[
			SNew(STextBlock).Text(FText::FromString(
				FString::Printf(TEXT("%s <- %s"), *K.ToString(), *LastBoneMapping[K].ToString())))
		];
	}
}

FReply SControlRigToolWidget::OnApproveMappingClicked()
{
	if (LastBoneMapping.Num() == 0)
	{
		SetStatus(TEXT("ERROR: No mapping to approve. Run AI Bone Mapping first."));
		return FReply::Handled();
	}
	
	SendApproveRequest();
	return FReply::Handled();
}

void SControlRigToolWidget::SendApproveRequest()
{
	FString MeshPath = GetSelectedMeshPath();
	if (MeshPath.IsEmpty())
	{
		SetStatus(TEXT("ERROR: No mesh selected"));
		return;
	}
	
	// 메시 로드
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		SetStatus(TEXT("ERROR: Failed to load mesh"));
		return;
	}
	
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// JSON 요청 생성
	TSharedPtr<FJsonObject> RequestObj = MakeShared<FJsonObject>();
	RequestObj->SetStringField(TEXT("skeleton_name"), Mesh->GetName());
	
	// 본 정보 배열
	TArray<TSharedPtr<FJsonValue>> BonesArray;
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		TSharedPtr<FJsonObject> BoneObj = MakeShared<FJsonObject>();
		FName BoneName = RefSkel.GetBoneName(i);
		int32 ParentIdx = RefSkel.GetParentIndex(i);
		
		BoneObj->SetStringField(TEXT("name"), BoneName.ToString());
		
		if (ParentIdx >= 0)
		{
			BoneObj->SetStringField(TEXT("parent"), RefSkel.GetBoneName(ParentIdx).ToString());
		}
		else
		{
			BoneObj->SetField(TEXT("parent"), MakeShared<FJsonValueNull>());
		}
		
		// 자식 본 수집
		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		for (int32 j = 0; j < RefSkel.GetNum(); ++j)
		{
			if (RefSkel.GetParentIndex(j) == i)
			{
				ChildrenArray.Add(MakeShared<FJsonValueString>(RefSkel.GetBoneName(j).ToString()));
			}
		}
		BoneObj->SetArrayField(TEXT("children"), ChildrenArray);
		
		BonesArray.Add(MakeShared<FJsonValueObject>(BoneObj));
	}
	RequestObj->SetArrayField(TEXT("bones"), BonesArray);
	
	// 매핑 정보 (역전: ue5_bone -> source_bone)
	TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();
	for (const auto& Pair : LastBoneMapping)
	{
		MappingObj->SetStringField(Pair.Key.ToString(), Pair.Value.ToString());
	}
	RequestObj->SetObjectField(TEXT("mapping"), MappingObj);
	
	// JSON 문자열로 변환
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);
	
	// HTTP 요청 전송
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("http://localhost:8000/approve"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);
	
	HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (!bWasSuccessful || !Response.IsValid())
		{
			SetStatus(TEXT("ERROR: Failed to send approve request"));
			return;
		}
		
		TSharedPtr<FJsonObject> JsonResponse;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		
		if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
		{
			int32 TotalSamples = JsonResponse->GetIntegerField(TEXT("total_samples"));
			bool AutoTrain = JsonResponse->GetBoolField(TEXT("auto_train_triggered"));
			FString Message = JsonResponse->GetStringField(TEXT("message"));
			
			if (AutoTrain)
			{
				SetStatus(FString::Printf(TEXT("APPROVED! %s (Auto-training started!)"), *Message));
			}
			else
			{
				SetStatus(FString::Printf(TEXT("APPROVED! %s"), *Message));
			}
			
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
				FString::Printf(TEXT("Mapping Approved!\n\nTotal Samples: %d\n%s"), TotalSamples, *Message)));
		}
		else
		{
			SetStatus(TEXT("ERROR: Invalid response from server"));
		}
	});
	
	HttpRequest->ProcessRequest();
	SetStatus(TEXT("Sending approve request..."));
}

// ============================================================================
// 분류 피드백 함수들 (AI 학습용)
// ============================================================================
void SControlRigToolWidget::SendClassificationFeedback(const FString& BoneName, const FString& Classification)
{
	// JSON 요청 생성
	TSharedPtr<FJsonObject> RequestObj = MakeShared<FJsonObject>();
	RequestObj->SetStringField(TEXT("bone_name"), BoneName);
	RequestObj->SetStringField(TEXT("classification"), Classification);
	
	// 부모/자식 정보 추가 (캐시된 메시에서)
	if (CachedMesh.IsValid())
	{
		const FReferenceSkeleton& RefSkel = CachedMesh->GetRefSkeleton();
		int32 BoneIndex = RefSkel.FindBoneIndex(FName(*BoneName));
		if (BoneIndex != INDEX_NONE)
		{
			int32 ParentIndex = RefSkel.GetParentIndex(BoneIndex);
			if (ParentIndex != INDEX_NONE)
			{
				RequestObj->SetStringField(TEXT("parent"), RefSkel.GetBoneName(ParentIndex).ToString());
			}
			
			// 자식 본 수집
			TArray<TSharedPtr<FJsonValue>> ChildrenArray;
			for (int32 i = 0; i < RefSkel.GetNum(); ++i)
			{
				if (RefSkel.GetParentIndex(i) == BoneIndex)
				{
					ChildrenArray.Add(MakeShared<FJsonValueString>(RefSkel.GetBoneName(i).ToString()));
				}
			}
			RequestObj->SetArrayField(TEXT("children"), ChildrenArray);
		}
		RequestObj->SetStringField(TEXT("skeleton_name"), CachedMesh->GetName());
	}
	
	// JSON 문자열로 변환
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);
	
	// HTTP 요청 전송
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("http://localhost:8000/classify_feedback"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);
	
	HttpRequest->OnProcessRequestComplete().BindLambda([this, BoneName, Classification](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (bWasSuccessful && Response.IsValid())
		{
			TSharedPtr<FJsonObject> JsonResponse;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (FJsonSerializer::Deserialize(Reader, JsonResponse))
			{
				FString Message = JsonResponse->GetStringField(TEXT("message"));
				SetStatus(FString::Printf(TEXT("Feedback: %s -> %s. %s"), *BoneName, *Classification, *Message));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ControlRigTool] Failed to send classification feedback"));
		}
	});
	
	HttpRequest->ProcessRequest();
}

// ============================================================================
// 본 선택 UI 함수들
// ============================================================================
void SControlRigToolWidget::BuildBoneDisplayList()
{
	BoneDisplayList.Empty();
	
	if (!CachedMesh.IsValid()) return;
	
	USkeletalMesh* Mesh = CachedMesh.Get();
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// 각 본의 깊이 계산을 위한 함수
	auto GetBoneDepth = [&RefSkel](int32 BoneIndex) -> int32 {
		int32 Depth = 0;
		int32 ParentIndex = RefSkel.GetParentIndex(BoneIndex);
		while (ParentIndex != INDEX_NONE)
		{
			Depth++;
			ParentIndex = RefSkel.GetParentIndex(ParentIndex);
		}
		return Depth;
	};
	
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		FBoneDisplayInfo Info;
		Info.BoneName = RefSkel.GetBoneName(i);
		Info.BoneIndex = i;
		Info.ParentIndex = RefSkel.GetParentIndex(i);
		Info.Depth = GetBoneDepth(i);
		
		FString BoneNameStr = Info.BoneName.ToString();
		
		// 제로본 여부
		Info.bIsZeroBone = IsZeroBone(BoneNameStr);
		
		// 스킨 웨이트 여부
		Info.bHasSkinWeight = HasSkinWeight(Mesh, Info.BoneName);
		
		// 기본 분류: 제로본 아니고, 헬퍼 아니고, 스킨 있으면 Secondary
		if (!Info.bIsZeroBone && !IsHelperBone(BoneNameStr) && Info.bHasSkinWeight)
		{
			Info.Classification = EBoneClassification::Secondary;
		}
		else
		{
			Info.Classification = EBoneClassification::Helper;
		}
		
		BoneDisplayList.Add(Info);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Built bone display list: %d bones"), BoneDisplayList.Num());
}

void SControlRigToolWidget::UpdateBoneSelectionUI()
{
	if (!BoneSelectionBox.IsValid()) return;
	BoneSelectionBox->ClearChildren();
	
	// 통계
	int32 ZeroCount = 0, NoSkinCount = 0, SecondaryCount = 0, WeaponCount = 0;
	for (const FBoneDisplayInfo& Info : BoneDisplayList)
	{
		if (Info.bIsZeroBone) ZeroCount++;
		else if (!Info.bHasSkinWeight) NoSkinCount++;
		else if (Info.Classification == EBoneClassification::Secondary) SecondaryCount++;
		else if (Info.Classification == EBoneClassification::Weapon) WeaponCount++;
	}
	
	// 헤더
	BoneSelectionBox->AddSlot().AutoHeight().Padding(4)
	[
		SNew(STextBlock)
		.Text(FText::FromString(FString::Printf(TEXT("Total: %d | Zero: %d | No Skin: %d | Secondary: %d | Weapon: %d"), 
			BoneDisplayList.Num(), ZeroCount, NoSkinCount, SecondaryCount, WeaponCount)))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
	];
	
	// 색상 범례
	BoneSelectionBox->AddSlot().AutoHeight().Padding(4, 0, 4, 8)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("\U0001F534 Zero")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FLinearColor(1.0f, 0.3f, 0.3f))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("\U000026AA NoSkin")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("H=Helper")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("S=Secondary")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FLinearColor(0.3f, 1.0f, 0.3f))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("W=Weapon")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.2f))
		]
	];
	
	// 본 목록
	for (int32 i = 0; i < BoneDisplayList.Num(); ++i)
	{
		BoneSelectionBox->AddSlot().AutoHeight()
		[
			CreateBoneRow(i)
		];
	}
}

TSharedRef<SWidget> SControlRigToolWidget::CreateBoneRow(int32 Index)
{
	FBoneDisplayInfo& Info = BoneDisplayList[Index];
	
	// 색상 결정
	FLinearColor TextColor;
	FLinearColor RowBgColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // 기본: 투명
	
	// 3단계 배경색: 양옆(SideBg) - 그라데이션(FadeBg) - 가운데(CenterBg)
	FLinearColor SideBgColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // 양옆
	FLinearColor FadeBgColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // 그라데이션 (중간)
	FLinearColor CenterBgColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // 가운데 (메뉴+본이름)
	
	// 양쪽(Side+Fade) = 같은 색, 가운데(Center) = 더 투명
	// 전체적으로 더 투명하게 (alpha 줄임)
	if (Info.bIsZeroBone)
	{
		TextColor = FLinearColor(1.0f, 0.4f, 0.4f);  // 빨강 (제로본)
		SideBgColor = FLinearColor(1.0f, 0.3f, 0.3f, 0.12f);   // 양쪽 (같은색)
		FadeBgColor = FLinearColor(1.0f, 0.3f, 0.3f, 0.12f);   // 페이드도 양쪽과 동일
		CenterBgColor = FLinearColor(1.0f, 0.3f, 0.3f, 0.03f); // 가운데 더 투명
	}
	else if (!Info.bHasSkinWeight)
	{
		TextColor = FLinearColor(0.4f, 0.4f, 0.4f);  // 진한 회색 (스킨 없음)
		SideBgColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.08f);
		FadeBgColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.08f);
		CenterBgColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.02f);
	}
	else
	{
		// 분류에 따른 색상
		switch (Info.Classification)
		{
		case EBoneClassification::Helper:
			TextColor = FLinearColor(0.3f, 0.5f, 1.0f);  // 파란색 (Helper/X 선택)
			SideBgColor = FLinearColor(0.2f, 0.4f, 0.9f, 0.12f);   // 양쪽 같은색
			FadeBgColor = FLinearColor(0.2f, 0.4f, 0.9f, 0.12f);
			CenterBgColor = FLinearColor(0.2f, 0.4f, 0.9f, 0.03f); // 가운데 더 투명
			break;
		case EBoneClassification::Secondary:
			TextColor = FLinearColor(0.2f, 1.0f, 0.2f);  // 형광 초록
			SideBgColor = FLinearColor(0.1f, 0.8f, 0.1f, 0.12f);
			FadeBgColor = FLinearColor(0.1f, 0.8f, 0.1f, 0.12f);
			CenterBgColor = FLinearColor(0.1f, 0.8f, 0.1f, 0.03f);
			break;
		case EBoneClassification::Weapon:
			TextColor = FLinearColor(1.0f, 0.8f, 0.0f);  // 쨍한 노랑/주황
			SideBgColor = FLinearColor(0.9f, 0.7f, 0.0f, 0.12f);
			FadeBgColor = FLinearColor(0.9f, 0.7f, 0.0f, 0.12f);
			CenterBgColor = FLinearColor(0.9f, 0.7f, 0.0f, 0.03f);
			break;
		default:
			TextColor = FLinearColor(0.8f, 0.8f, 0.8f);  // 흰색
			break;
		}
	}
	
	// RowBgColor는 더 이상 사용 안함
	RowBgColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	
	// 들여쓰기
	float Indent = Info.Depth * 12.0f;
	
	// 라디오 버튼 활성화 여부 (제로본과 스킨없는 본은 비활성화)
	bool bCanSelect = !Info.bIsZeroBone && Info.bHasSkinWeight;
	
	// 3영역 구조: [좌측: 들여쓰기] [가운데: 메뉴+본이름] [우측: 나머지]
	return SNew(SHorizontalBox)
		// 1. 좌측 - 들여쓰기 영역
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(SideBgColor)
			.Padding(FMargin(0, 2))
			[
				SNew(SBox).WidthOverride(FMath::Max(Indent, 0.0f))
			]
		]
		// 2. 가운데 (연한 배경) - 메뉴 + 본이름
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(CenterBgColor)
			.Padding(FMargin(2, 2))
			[
				SNew(SHorizontalBox)
		// Helper 라디오 버튼 (H)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0).VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(Info.Classification == EBoneClassification::Helper ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.IsEnabled(bCanSelect)
			.OnCheckStateChanged_Lambda([this, Index](ECheckBoxState NewState) {
				if (NewState == ECheckBoxState::Checked)
					OnBoneClassificationChanged(EBoneClassification::Helper, Index);
			})
			.ToolTipText(FText::FromString(TEXT("Helper - No controller")))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0).VAlign(VAlign_Center)
		[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("X")))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
		.ColorAndOpacity(FLinearColor(0.3f, 0.5f, 1.0f))  // 파란색
		]
		// Secondary 라디오 버튼 (S)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0).VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(Info.Classification == EBoneClassification::Secondary ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.IsEnabled(bCanSelect)
			.OnCheckStateChanged_Lambda([this, Index](ECheckBoxState NewState) {
				if (NewState == ECheckBoxState::Checked)
					OnBoneClassificationChanged(EBoneClassification::Secondary, Index);
			})
			.ToolTipText(FText::FromString(TEXT("Secondary - Standard controller")))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("S")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
			.ColorAndOpacity(FLinearColor(0.0f, 1.0f, 0.3f))
		]
		// Weapon 라디오 버튼 (W)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0).VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(Info.Classification == EBoneClassification::Weapon ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.IsEnabled(bCanSelect)
			.OnCheckStateChanged_Lambda([this, Index](ECheckBoxState NewState) {
				if (NewState == ECheckBoxState::Checked)
					OnBoneClassificationChanged(EBoneClassification::Weapon, Index);
			})
			.ToolTipText(FText::FromString(TEXT("Weapon - Weapon controller")))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("W")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
			.ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.2f))
		]
		// 본 이름
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(2, 1))
			.OnClicked_Lambda([this, Index]() -> FReply {
				FModifierKeysState Modifiers = FSlateApplication::Get().GetModifierKeys();
				OnBoneRowClicked(Index, Modifiers);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(Info.BoneName.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(TextColor)
			]
		]  // SButton Slot 끝
		]  // 가운데 SHorizontalBox 끝
	]  // 가운데 SBorder 끝
	// 4. 우측 - 남은 공간 전부 채우기 (본이름 옆부터 끝까지)
	+ SHorizontalBox::Slot().FillWidth(1.0f)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(SideBgColor)
		.Padding(FMargin(0, 2))
	];  // 전체 SHorizontalBox 끝
}

void SControlRigToolWidget::OnBoneClassificationChanged(EBoneClassification NewClassification, int32 BoneIndex)
{
	if (BoneIndex >= 0 && BoneIndex < BoneDisplayList.Num())
	{
		BoneDisplayList[BoneIndex].Classification = NewClassification;
		
		// 피드백 전송 (AI 학습용)
		FString ClassificationStr;
		switch (NewClassification)
		{
		case EBoneClassification::Secondary:
			ClassificationStr = TEXT("secondary");
			break;
		case EBoneClassification::Weapon:
			ClassificationStr = TEXT("weapon");
			break;
		default:
			ClassificationStr = TEXT("helper");
			break;
		}
		SendClassificationFeedback(BoneDisplayList[BoneIndex].BoneName.ToString(), ClassificationStr);
		
		// UI 갱신 (라디오 버튼 상태 반영)
		UpdateBoneSelectionUI();
	}
}

void SControlRigToolWidget::OnBoneRowClicked(int32 BoneIndex, const FModifierKeysState& ModifierKeys)
{
	if (BoneIndex < 0 || BoneIndex >= BoneDisplayList.Num()) return;
	
	FBoneDisplayInfo& ClickedInfo = BoneDisplayList[BoneIndex];
	
	// 제로본이나 스킨없는 본은 무시
	if (ClickedInfo.bIsZeroBone || !ClickedInfo.bHasSkinWeight) return;
	
	// Shift 클릭: 범위 선택 (같은 분류로 설정)
	if (ModifierKeys.IsShiftDown() && LastSelectedBoneIndex != INDEX_NONE)
	{
		int32 Start = FMath::Min(LastSelectedBoneIndex, BoneIndex);
		int32 End = FMath::Max(LastSelectedBoneIndex, BoneIndex);
		
		// 현재 클릭한 본의 분류로 범위 내 모든 본 설정
		EBoneClassification NewClassification = ClickedInfo.Classification;
		
		for (int32 i = Start; i <= End; ++i)
		{
			FBoneDisplayInfo& Info = BoneDisplayList[i];
			if (!Info.bIsZeroBone && Info.bHasSkinWeight)
			{
				Info.Classification = NewClassification;
			}
		}
		
		// UI 갱신
		UpdateBoneSelectionUI();
	}
	else
	{
		// 일반 클릭: 분류 순환 (Helper -> Secondary -> Weapon -> Helper)
		switch (ClickedInfo.Classification)
		{
		case EBoneClassification::Helper:
			ClickedInfo.Classification = EBoneClassification::Secondary;
			break;
		case EBoneClassification::Secondary:
			ClickedInfo.Classification = EBoneClassification::Weapon;
			break;
		case EBoneClassification::Weapon:
			ClickedInfo.Classification = EBoneClassification::Helper;
			break;
		}
		UpdateBoneSelectionUI();
	}
	
	LastSelectedBoneIndex = BoneIndex;
}

// ============================================================================
// 사용자 선택 기반 세컨더리 컨트롤러 생성
// ============================================================================
void SControlRigToolWidget::CreateSecondaryControlsFromSelection(UControlRigBlueprint* Rig, USkeletalMesh* Mesh)
{
	if (!Rig || !Mesh) return;
	
	LastSecondaryControlCount = 0;
	
	URigHierarchyController* HC = Rig->GetHierarchyController();
	URigHierarchy* Hierarchy = Rig->Hierarchy;
	
	if (!HC || !Hierarchy) return;
	
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// 버텍스 기반 Shape Info 계산 (스케일 + 오프셋)
	CalculateBoneShapeInfos(Mesh);
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] === Creating Secondary Controls from Selection ==="));
	
	// 사용자가 선택한 세컨더리 본 수집 (Weapon 제외)
	TArray<FName> SelectedBones;
	for (const FBoneDisplayInfo& Info : BoneDisplayList)
	{
		if (Info.Classification == EBoneClassification::Secondary)
		{
			SelectedBones.Add(Info.BoneName);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Selected %d secondary bones"), SelectedBones.Num());
	
	// Space별로 그룹화
	TMap<FName, TArray<FName>> ChainsBySpace;
	for (const FName& BoneName : SelectedBones)
	{
		FName SpaceParent = FindZeroBoneParent(BoneName, RefSkel);
		if (SpaceParent.IsNone())
		{
			SpaceParent = FName(TEXT("root"));
		}
		ChainsBySpace.FindOrAdd(SpaceParent).Add(BoneName);
	}
	
	// Space 및 Control 생성
	for (const auto& Pair : ChainsBySpace)
	{
		FName SpaceParentName = Pair.Key;
		const TArray<FName>& ChainBones = Pair.Value;
		
		// Space Null 생성
		FString SpaceNameStr = SpaceParentName.ToString() + TEXT("_space");
		FName SpaceFName(*SpaceNameStr);
		
		// Space 트랜스폼 (부모 본 위치)
		FTransform SpaceTransform = FTransform::Identity;
		
		// 매핑에서 실제 본 이름 찾기
		if (const FName* MappedBone = LastBoneMapping.Find(SpaceParentName))
		{
			int32 BoneIdx = RefSkel.FindBoneIndex(*MappedBone);
			if (BoneIdx != INDEX_NONE)
			{
				SpaceTransform = RefSkel.GetRefBonePose()[BoneIdx];
			}
		}
		
		CreateSpaceNull(HC, SpaceFName, SpaceTransform);
		
		// 각 본에 컨트롤러 생성
		CreateChainControls(HC, Hierarchy, SpaceFName, ChainBones, RefSkel);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Created %d secondary controls"), LastSecondaryControlCount);
	
	// AI 함수 노드 연결 (AI_Setup, AI_Forward, AI_Backward)
	ConnectSecondaryFunctionNodes(Rig, ChainsBySpace);
	
	// Weapon 본 처리
	CreateWeaponControlsFromSelection(Rig, Mesh);
}

void SControlRigToolWidget::UpdateWorkflowUI()
{
	// 현재 단계에 따라 UI 요소 활성화/비활성화
	switch (CurrentStep)
	{
	case EControlRigWorkflowStep::Step1_Setup:
		if (BodyRigButton.IsValid()) BodyRigButton->SetEnabled(true);
		if (FinalCreateButton.IsValid()) FinalCreateButton->SetEnabled(false);
		break;
		
	case EControlRigWorkflowStep::Step2_BodyRig:
	case EControlRigWorkflowStep::Step3_SelectBones:
		if (BodyRigButton.IsValid()) BodyRigButton->SetEnabled(false);
		if (FinalCreateButton.IsValid()) FinalCreateButton->SetEnabled(true);
		break;
		
	case EControlRigWorkflowStep::Step4_Complete:
		if (BodyRigButton.IsValid()) BodyRigButton->SetEnabled(true);
		if (FinalCreateButton.IsValid()) FinalCreateButton->SetEnabled(false);
		break;
	}
}

// ============================================================================
// RigVM 함수 노드 연결 (AI_Setup, AI_Forward, AI_Backward)
// 세컨더리 노드: Neck 관련 노드 뒤에 가로(X 방향)로 배치
// ============================================================================
void SControlRigToolWidget::ConnectSecondaryFunctionNodes(UControlRigBlueprint* Rig, 
	const TMap<FName, TArray<FName>>& ChainsBySpace)
{
	FString DebugInfo;
	
	if (!Rig)
	{
		DebugInfo = TEXT("ERROR: Control Rig Blueprint is NULL!");
		ShowDebugPopup(TEXT("RigVM Function Node Debug"), DebugInfo);
		return;
	}
	
	if (ChainsBySpace.Num() == 0)
	{
		DebugInfo = TEXT("No secondary bones selected!\n\nChainsBySpace is empty.");
		ShowDebugPopup(TEXT("RigVM Function Node Debug"), DebugInfo);
		return;
	}
	
	TArray<URigVMGraph*> AllGraphs = Rig->GetAllModels();
	URigVMGraph* MainGraph = nullptr;
	
	for (URigVMGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName().Equals(TEXT("RigVMModel")))
		{
			MainGraph = Graph;
			break;
		}
	}
	
	if (!MainGraph)
	{
		DebugInfo = TEXT("[ERROR] Main graph not found!");
		ShowDebugPopup(TEXT("RigVM Function Node Debug"), DebugInfo);
		return;
	}
	
	URigVMController* Controller = Rig->GetController(MainGraph);
	if (!Controller)
	{
		DebugInfo = TEXT("[ERROR] Controller not found!");
		ShowDebugPopup(TEXT("RigVM Function Node Debug"), DebugInfo);
		return;
	}
	
	DebugInfo += TEXT("=== Secondary Function Nodes (Neck 뒤) ===\n\n");
	
	// 템플릿의 빈 함수 노드 찾기 및 위치 저장 후 삭제
	FVector2D SetupStartPos(800.0f, 500.0f);
	FVector2D ForwardStartPos(1400.0f, 500.0f);
	FVector2D BackwardStartPos(2000.0f, 500.0f);
	
	URigVMNode* NeckSetupPrev = nullptr;  // 연결할 이전 노드 (Neck 노드)
	URigVMNode* NeckForwardPrev = nullptr;
	URigVMNode* NeckBackwardPrev = nullptr;
	
	// 빈 AI_Setup, AI_Forward, AI_Backward 노드 찾아서 위치 저장 및 삭제
	TArray<URigVMNode*> NodesToRemove;
	for (URigVMNode* Node : MainGraph->GetNodes())
	{
		FString NodeName = Node->GetName();
		
		// AI_Setup (정확히 일치하거나 숫자 suffix만 있는 것 - 빈 템플릿 함수)
		if (NodeName.Equals(TEXT("AI_Setup")) || 
			(NodeName.StartsWith(TEXT("AI_Setup")) && !NodeName.Contains(TEXT("Weapon"))))
		{
			// 이 노드에 연결된 이전 노드 찾기
			for (URigVMPin* Pin : Node->GetPins())
			{
				if (Pin->GetName().Contains(TEXT("Execute")))
				{
					for (URigVMLink* Link : Pin->GetLinks())
					{
						URigVMPin* OtherPin = Link->GetSourcePin();
						if (OtherPin && OtherPin->GetNode() != Node)
						{
							URigVMNode* PrevNode = OtherPin->GetNode();
							// Neck 관련 노드인지 확인
							if (PrevNode->GetName().Contains(TEXT("Neck")) || 
								PrevNode->GetName().Contains(TEXT("Setup")))
							{
								NeckSetupPrev = PrevNode;
							}
						}
					}
				}
			}
			SetupStartPos = Node->GetPosition();
			NodesToRemove.Add(Node);
			DebugInfo += FString::Printf(TEXT("Found empty AI_Setup at (%.0f, %.0f)\n"), SetupStartPos.X, SetupStartPos.Y);
		}
		
		// AI_Forward (Weapon 제외)
		if (NodeName.Equals(TEXT("AI_Forward")) || 
			(NodeName.StartsWith(TEXT("AI_Forward")) && !NodeName.Contains(TEXT("Weapon"))))
		{
			for (URigVMPin* Pin : Node->GetPins())
			{
				if (Pin->GetName().Contains(TEXT("Execute")))
				{
					for (URigVMLink* Link : Pin->GetLinks())
					{
						URigVMPin* OtherPin = Link->GetSourcePin();
						if (OtherPin && OtherPin->GetNode() != Node)
						{
							URigVMNode* PrevNode = OtherPin->GetNode();
							if (PrevNode->GetName().Contains(TEXT("Neck")) || 
								PrevNode->GetName().Contains(TEXT("Forward")))
							{
								NeckForwardPrev = PrevNode;
							}
						}
					}
				}
			}
			ForwardStartPos = Node->GetPosition();
			NodesToRemove.Add(Node);
			DebugInfo += FString::Printf(TEXT("Found empty AI_Forward at (%.0f, %.0f)\n"), ForwardStartPos.X, ForwardStartPos.Y);
		}
		
		// AI_Backward (Weapon 제외)
		if (NodeName.Equals(TEXT("AI_Backward")) || 
			(NodeName.StartsWith(TEXT("AI_Backward")) && !NodeName.Contains(TEXT("Weapon"))))
		{
			for (URigVMPin* Pin : Node->GetPins())
			{
				if (Pin->GetName().Contains(TEXT("Execute")))
				{
					for (URigVMLink* Link : Pin->GetLinks())
					{
						URigVMPin* OtherPin = Link->GetSourcePin();
						if (OtherPin && OtherPin->GetNode() != Node)
						{
							URigVMNode* PrevNode = OtherPin->GetNode();
							if (PrevNode->GetName().Contains(TEXT("Neck")) || 
								PrevNode->GetName().Contains(TEXT("Backward")))
							{
								NeckBackwardPrev = PrevNode;
							}
						}
					}
				}
			}
			BackwardStartPos = Node->GetPosition();
			NodesToRemove.Add(Node);
			DebugInfo += FString::Printf(TEXT("Found empty AI_Backward at (%.0f, %.0f)\n"), BackwardStartPos.X, BackwardStartPos.Y);
		}
	}
	
	// 빈 노드 삭제
	for (URigVMNode* Node : NodesToRemove)
	{
		DebugInfo += FString::Printf(TEXT("Removing empty node: %s\n"), *Node->GetName());
		Controller->RemoveNode(Node, false, false);
	}
	
	DebugInfo += TEXT("\n");
	
	// 노드 간격 (가로 방향)
	const float XSpacing = 400.0f;
	
	float SetupX = SetupStartPos.X;
	float ForwardX = ForwardStartPos.X;
	float BackwardX = BackwardStartPos.X;
	
	URigVMNode* LastSetupNode = NeckSetupPrev;
	URigVMNode* LastForwardNode = NeckForwardPrev;
	URigVMNode* LastBackwardNode = NeckBackwardPrev;
	
	// 각 Space에 대해 함수 노드 추가 (가로 방향)
	int32 SpaceIndex = 0;
	for (const auto& Pair : ChainsBySpace)
	{
		FName SpaceParentName = Pair.Key;
		const TArray<FName>& ChainBones = Pair.Value;
		
		FString SpaceNameStr = SpaceParentName.ToString() + TEXT("_space");
		FName SpaceName(*SpaceNameStr);
		
		TArray<FName> ControlNames;
		for (const FName& BoneName : ChainBones)
		{
			ControlNames.Add(FName(*(BoneName.ToString() + TEXT("_ctrl"))));
		}
		
		FName ActualBoneName = LastBoneMapping.FindRef(SpaceParentName);
		if (ActualBoneName.IsNone()) ActualBoneName = SpaceParentName;
		
		DebugInfo += FString::Printf(TEXT("--- Space: %s ---\n"), *SpaceNameStr);
		
		// AI_Setup (가로 배치)
		{
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Setup"), 
				FVector2D(SetupX + SpaceIndex * XSpacing, SetupStartPos.Y), DebugInfo);
			if (FuncNode)
			{
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				
				if (LastSetupNode)
				{
					// Execute 연결 시도 (여러 핀 이름)
					bool bLinked = Controller->AddLink(LastSetupNode->GetName() + TEXT(".Execute"), FuncNode->GetName() + TEXT(".Execute"), false);
					if (!bLinked)
						bLinked = Controller->AddLink(LastSetupNode->GetName() + TEXT(".ExecuteContext"), FuncNode->GetName() + TEXT(".ExecuteContext"), false);
					DebugInfo += FString::Printf(TEXT("  Setup: %s -> %s (%s)\n"), *LastSetupNode->GetName(), *FuncNode->GetName(), bLinked ? TEXT("OK") : TEXT("FAIL"));
				}
				LastSetupNode = FuncNode;
			}
		}
		
		// AI_Forward (가로 배치)
		{
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Forward"), 
				FVector2D(ForwardX + SpaceIndex * XSpacing, ForwardStartPos.Y), DebugInfo);
			if (FuncNode)
			{
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				
				if (LastForwardNode)
				{
					bool bLinked = Controller->AddLink(LastForwardNode->GetName() + TEXT(".Execute"), FuncNode->GetName() + TEXT(".Execute"), false);
					if (!bLinked)
						bLinked = Controller->AddLink(LastForwardNode->GetName() + TEXT(".ExecuteContext"), FuncNode->GetName() + TEXT(".ExecuteContext"), false);
					DebugInfo += FString::Printf(TEXT("  Forward: %s -> %s (%s)\n"), *LastForwardNode->GetName(), *FuncNode->GetName(), bLinked ? TEXT("OK") : TEXT("FAIL"));
				}
				LastForwardNode = FuncNode;
			}
		}
		
		// AI_Backward (가로 배치)
		{
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Backward"), 
				FVector2D(BackwardX + SpaceIndex * XSpacing, BackwardStartPos.Y), DebugInfo);
			if (FuncNode)
			{
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				
				if (LastBackwardNode)
				{
					bool bLinked = Controller->AddLink(LastBackwardNode->GetName() + TEXT(".Execute"), FuncNode->GetName() + TEXT(".Execute"), false);
					if (!bLinked)
						bLinked = Controller->AddLink(LastBackwardNode->GetName() + TEXT(".ExecuteContext"), FuncNode->GetName() + TEXT(".ExecuteContext"), false);
					DebugInfo += FString::Printf(TEXT("  Backward: %s -> %s (%s)\n"), *LastBackwardNode->GetName(), *FuncNode->GetName(), bLinked ? TEXT("OK") : TEXT("FAIL"));
				}
				LastBackwardNode = FuncNode;
			}
		}
		
		SpaceIndex++;
		DebugInfo += TEXT("\n");
	}
	
	DebugInfo += FString::Printf(TEXT("\n=== Result: %d spaces processed ===\n"), ChainsBySpace.Num());
	ShowDebugPopup(TEXT("Secondary Function Debug"), DebugInfo);
}

URigVMNode* SControlRigToolWidget::FindLastAIFunctionNode(URigVMGraph* Graph, const FString& FunctionPrefix)
{
	if (!Graph) return nullptr;
	
	URigVMNode* LastNode = nullptr;
	float MaxY = -FLT_MAX;
	
	for (URigVMNode* Node : Graph->GetNodes())
	{
		FString NodeName = Node->GetName();
		if (NodeName.Contains(FunctionPrefix))
		{
			FVector2D Pos = Node->GetPosition();
			if (Pos.Y > MaxY)
			{
				MaxY = Pos.Y;
				LastNode = Node;
			}
		}
	}
	
	return LastNode;
}

URigVMNode* SControlRigToolWidget::AddFunctionReferenceNode(URigVMController* Controller, 
	const FString& FunctionName, const FVector2D& Position, FString& OutDebugInfo)
{
	if (!Controller)
	{
		OutDebugInfo += TEXT("    [ERROR] Controller is null!\n");
		return nullptr;
	}
	
	URigVMGraph* Graph = Controller->GetGraph();
	if (!Graph)
	{
		OutDebugInfo += TEXT("    [ERROR] Graph is null!\n");
		return nullptr;
	}
	
	// 그래프의 블루프린트 접근
	UObject* Outer = Graph->GetOuter();
	while (Outer && !Cast<URigVMBlueprint>(Outer))
	{
		Outer = Outer->GetOuter();
	}
	
	URigVMBlueprint* Blueprint = Cast<URigVMBlueprint>(Outer);
	if (!Blueprint) 
	{
		OutDebugInfo += TEXT("    [ERROR] Failed to find blueprint for graph!\n");
		return nullptr;
	}
	
	// 로컬 함수 라이브러리에서 함수 찾기
	URigVMFunctionLibrary* FunctionLibrary = Blueprint->GetLocalFunctionLibrary();
	if (!FunctionLibrary)
	{
		OutDebugInfo += TEXT("    [ERROR] No local function library found!\n");
		return nullptr;
	}
	
	// 모든 함수 로깅 (한번만)
	static bool bLoggedFunctions = false;
	if (!bLoggedFunctions)
	{
		OutDebugInfo += TEXT("\n  Available functions in library:\n");
		for (URigVMLibraryNode* LibNode : FunctionLibrary->GetFunctions())
		{
			if (LibNode)
			{
				OutDebugInfo += FString::Printf(TEXT("    - %s\n"), *LibNode->GetName());
			}
		}
		bLoggedFunctions = true;
	}
	
	// 함수 찾기
	URigVMLibraryNode* FunctionNode = nullptr;
	for (URigVMLibraryNode* LibNode : FunctionLibrary->GetFunctions())
	{
		if (LibNode && LibNode->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionNode = LibNode;
			break;
		}
	}
	
	if (!FunctionNode)
	{
		OutDebugInfo += FString::Printf(TEXT("    [ERROR] Function not found: %s\n"), *FunctionName);
		return nullptr;
	}
	
	// 함수 참조 노드 추가
	URigVMNode* NewNode = Controller->AddFunctionReferenceNode(
		FunctionNode,
		Position,
		FString(),  // 자동 이름
		false,      // bSetupUndoRedo
		false       // bPrintPythonCommand
	);
	
	if (NewNode)
	{
		return NewNode;
	}
	
	OutDebugInfo += FString::Printf(TEXT("    [ERROR] AddFunctionReferenceNode failed for: %s\n"), *FunctionName);
	return nullptr;
}

void SControlRigToolWidget::SetFunctionNodePins(URigVMController* Controller, URigVMNode* FuncNode,
	const FName& BoneName, const FName& SpaceName, 
	const TArray<FName>& Bones, const TArray<FName>& Controls)
{
	if (!Controller || !FuncNode) return;
	
	FString NodeName = FuncNode->GetName();
	FVector2D NodePos = FuncNode->GetPosition();
	
	// bone 핀 설정 (단일 본) - FRigElementKey 형식
	FString BoneValue = FString::Printf(TEXT("(Type=Bone,Name=\"%s\")"), *BoneName.ToString());
	Controller->SetPinDefaultValue(NodeName + TEXT(".bone"), BoneValue, true, false, false);
	
	// space 핀 설정 (Null) - FRigElementKey 형식
	FString SpaceValue = FString::Printf(TEXT("(Type=Null,Name=\"%s\")"), *SpaceName.ToString());
	Controller->SetPinDefaultValue(NodeName + TEXT(".space"), SpaceValue, true, false, false);
	
	// ItemArray 노드 (Make Array) 생성 - 함수 노드 하단부에 배치
	FName ArrayMakeNotation = FRigVMDispatch_ArrayMake().GetTemplateNotation();
	
	// Bones ItemArray 생성 (함수 노드 아래)
	URigVMTemplateNode* BonesArrayNode = Controller->AddTemplateNode(
		ArrayMakeNotation,
		FVector2D(NodePos.X - 100.0f, NodePos.Y + 180.0f),
		FString(),
		false, false
	);
	
	if (BonesArrayNode)
	{
		FString ArrayNodeName = BonesArrayNode->GetName();
		FString ValuesPath = ArrayNodeName + TEXT(".Values");
		
		Controller->AddLink(
			ArrayNodeName + TEXT(".Array"),
			NodeName + TEXT(".bones"),
			false
		);
		
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			FString BoneElementValue = FString::Printf(TEXT("(Type=Bone,Name=\"%s\")"), *Bones[i].ToString());
			Controller->InsertArrayPin(ValuesPath, i, BoneElementValue, false, false);
		}
		
		Controller->SetPinExpansion(ValuesPath, false, false);
	}
	
	// Ctrls ItemArray 생성 (Bones 아래)
	URigVMTemplateNode* CtrlsArrayNode = Controller->AddTemplateNode(
		ArrayMakeNotation,
		FVector2D(NodePos.X - 100.0f, NodePos.Y + 280.0f),
		FString(),
		false, false
	);
	
	if (CtrlsArrayNode)
	{
		FString ArrayNodeName = CtrlsArrayNode->GetName();
		FString ValuesPath = ArrayNodeName + TEXT(".Values");
		
		Controller->AddLink(
			ArrayNodeName + TEXT(".Array"),
			NodeName + TEXT(".ctrls"),
			false
		);
		
		for (int32 i = 0; i < Controls.Num(); ++i)
		{
			FString CtrlElementValue = FString::Printf(TEXT("(Type=Control,Name=\"%s\")"), *Controls[i].ToString());
			Controller->InsertArrayPin(ValuesPath, i, CtrlElementValue, false, false);
		}
		
		Controller->SetPinExpansion(ValuesPath, false, false);
	}
	
	UE_LOG(LogTemp, Log, TEXT("    Set pins for %s: bone=%s, space=%s, %d bones, %d ctrls"),
		*NodeName, *BoneName.ToString(), *SpaceName.ToString(), Bones.Num(), Controls.Num());
}

// ============================================================================
// Weapon 본 처리 함수들
// ============================================================================
void SControlRigToolWidget::CreateWeaponControlsFromSelection(UControlRigBlueprint* Rig, USkeletalMesh* Mesh)
{
	if (!Rig || !Mesh) return;
	
	URigHierarchyController* HC = Rig->GetHierarchyController();
	URigHierarchy* Hierarchy = Rig->Hierarchy;
	
	if (!HC || !Hierarchy) return;
	
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// Weapon 본 수집 (L/R 구분)
	TArray<FName> WeaponBonesL, WeaponBonesR;
	
	for (const FBoneDisplayInfo& Info : BoneDisplayList)
	{
		if (Info.Classification == EBoneClassification::Weapon)
		{
			FString BoneNameLower = Info.BoneName.ToString().ToLower();
			
			// L/R 판별 (이름에 _l, _r, left, right 등 포함)
			if (BoneNameLower.Contains(TEXT("_l")) || BoneNameLower.Contains(TEXT("left")) ||
				BoneNameLower.Contains(TEXT("-l")) || BoneNameLower.EndsWith(TEXT("l")))
			{
				WeaponBonesL.Add(Info.BoneName);
			}
			else if (BoneNameLower.Contains(TEXT("_r")) || BoneNameLower.Contains(TEXT("right")) ||
					 BoneNameLower.Contains(TEXT("-r")) || BoneNameLower.EndsWith(TEXT("r")))
			{
				WeaponBonesR.Add(Info.BoneName);
			}
			else
			{
				// L/R 구분 없으면 기본적으로 L로 처리
				WeaponBonesL.Add(Info.BoneName);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Weapon bones - L: %d, R: %d"), WeaponBonesL.Num(), WeaponBonesR.Num());
	
	// Weapon L 처리
	if (WeaponBonesL.Num() > 0)
	{
		CreateWeaponSpaceAndControls(HC, Hierarchy, true, WeaponBonesL, RefSkel);
	}
	
	// Weapon R 처리
	if (WeaponBonesR.Num() > 0)
	{
		CreateWeaponSpaceAndControls(HC, Hierarchy, false, WeaponBonesR, RefSkel);
	}
}

void SControlRigToolWidget::CreateWeaponSpaceAndControls(URigHierarchyController* HC, URigHierarchy* Hierarchy,
	bool bIsLeft, const TArray<FName>& WeaponBones, const FReferenceSkeleton& RefSkel)
{
	if (!HC || !Hierarchy || WeaponBones.Num() == 0) return;
	
	// Space 이름: Weapon_l_space 또는 Weapon_r_space
	FString SpaceNameStr = bIsLeft ? TEXT("Weapon_l_space") : TEXT("Weapon_r_space");
	FName SpaceName(*SpaceNameStr);
	
	// global_ctrl 밑에 Space 생성
	FRigElementKey GlobalCtrlKey(FName(TEXT("global_ctrl")), ERigElementType::Control);
	FRigElementKey ParentKey = Hierarchy->Contains(GlobalCtrlKey) ? GlobalCtrlKey : FRigElementKey();
	
	// Space가 이미 있는지 확인
	FRigElementKey SpaceKey(SpaceName, ERigElementType::Null);
	if (!Hierarchy->Contains(SpaceKey))
	{
		FRigElementKey NewSpaceKey = HC->AddNull(
			SpaceName,
			ParentKey,
			FTransform::Identity,
			false
		);
		UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Created Weapon Space: %s (parent: %s)"), 
			*SpaceNameStr, ParentKey.IsValid() ? *ParentKey.Name.ToString() : TEXT("root"));
	}
	
	// 본들을 계층 순서로 정렬
	TArray<FName> SortedBones = WeaponBones;
	SortedBones.Sort([&RefSkel](const FName& A, const FName& B) {
		return RefSkel.FindBoneIndex(A) < RefSkel.FindBoneIndex(B);
	});
	
	// ========== 무기 전체 버텍스 바운딩 박스 계산 ==========
	// 모든 웨폰 본의 버텍스를 합쳐서 무기 전체 크기 계산
	TArray<FBoneVertInfo> BoneVertInfos;
	FString MeshPath = GetSelectedMeshPath();
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *MeshPath));
	if (Mesh)
	{
		FMeshUtilitiesEngine::CalcBoneVertInfos(Mesh, BoneVertInfos, true);
	}
	
	FBox TotalWeaponBox(ForceInit);
	for (const FName& BoneName : SortedBones)
	{
		int32 BoneIdx = RefSkel.FindBoneIndex(BoneName);
		if (BoneIdx != INDEX_NONE && BoneIdx < BoneVertInfos.Num())
		{
			const FBoneVertInfo& Info = BoneVertInfos[BoneIdx];
			
			// 본의 월드 트랜스폼 가져오기
			FTransform BoneTransform = FTransform::Identity;
			if (BoneIdx < RefSkel.GetRefBonePose().Num())
			{
				BoneTransform = RefSkel.GetRefBonePose()[BoneIdx];
			}
			
			for (const FVector3f& LocalPos : Info.Positions)
			{
				// 로컬 → 월드 변환 (대략적)
				FVector WorldPos = BoneTransform.TransformPosition(FVector(LocalPos));
				TotalWeaponBox += WorldPos;
			}
		}
	}
	
	// 무기 전체 크기에서 XYZ 각각 스케일 계산 (직육면체)
	FVector WeaponSize = TotalWeaponBox.GetSize();
	FVector WeaponScale = WeaponSize / 50.0f;  // 무기 크기에 맞게 XYZ 각각
	// 최소/최대 클램프
	WeaponScale.X = FMath::Clamp(WeaponScale.X, 0.3f, 15.0f);
	WeaponScale.Y = FMath::Clamp(WeaponScale.Y, 0.3f, 15.0f);
	WeaponScale.Z = FMath::Clamp(WeaponScale.Z, 0.3f, 15.0f);
	
	UE_LOG(LogTemp, Log, TEXT("  Weapon Total BBox: %.1f x %.1f x %.1f -> Scale: %.2f x %.2f x %.2f"), 
		WeaponSize.X, WeaponSize.Y, WeaponSize.Z, WeaponScale.X, WeaponScale.Y, WeaponScale.Z);
	
	// 컨트롤러 생성
	TMap<FName, FName> BoneToControlMap;
	FName LastControlName;
	
	for (const FName& BoneName : SortedBones)
	{
		FString ControlNameStr = BoneName.ToString() + TEXT("_ctrl");
		FName ControlName(*ControlNameStr);
		
		FRigElementKey ControlKey(ControlName, ERigElementType::Control);
		if (Hierarchy->Contains(ControlKey))
		{
			BoneToControlMap.Add(BoneName, ControlName);
			LastControlName = ControlName;
			continue;
		}
		
		// 부모 결정
		int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		int32 ParentBoneIndex = RefSkel.GetParentIndex(BoneIndex);
		
		FRigElementKey ControlParentKey;
		
		if (ParentBoneIndex != INDEX_NONE)
		{
			FName ParentBoneName = RefSkel.GetBoneName(ParentBoneIndex);
			if (FName* ParentControlName = BoneToControlMap.Find(ParentBoneName))
			{
				ControlParentKey = FRigElementKey(*ParentControlName, ERigElementType::Control);
			}
		}
		
		if (!ControlParentKey.IsValid())
		{
			ControlParentKey = FRigElementKey(SpaceName, ERigElementType::Null);
		}
		
		// 컨트롤러 설정
		FRigControlSettings ControlSettings;
		ControlSettings.ControlType = ERigControlType::Transform;
		ControlSettings.DisplayName = BoneName;
		ControlSettings.AnimationType = ERigControlAnimationType::AnimationControl;
		
		// Weapon: Box_Thick, 무기 전체 크기로 스케일 (모든 컨트롤러 동일 크기)
		ControlSettings.ShapeName = FName(TEXT("Box_Thick"));
		
		FTransform ShapeTransform = FTransform::Identity;
		ShapeTransform.SetScale3D(WeaponScale);  // XYZ 각각 다른 직육면체
		
		UE_LOG(LogTemp, Log, TEXT("  Weapon: %s, Shape=Box_Thick, Scale=%.2f x %.2f x %.2f"), 
			*BoneName.ToString(), WeaponScale.X, WeaponScale.Y, WeaponScale.Z);
		
		FRigElementKey NewControlKey = HC->AddControl(
			ControlName,
			ControlParentKey,
			ControlSettings,
			FRigControlValue::Make<FTransform>(FTransform::Identity),
			FTransform::Identity,
			ShapeTransform,
			false
		);
		
		if (NewControlKey.IsValid())
		{
			BoneToControlMap.Add(BoneName, ControlName);
			LastControlName = ControlName;
			LastSecondaryControlCount++;
			UE_LOG(LogTemp, Log, TEXT("  Created Weapon control: %s (scale: %.2f x %.2f x %.2f)"), *BoneName.ToString(), WeaponScale.X, WeaponScale.Y, WeaponScale.Z);
		}
	}
	
	// 마지막 컨트롤러에 Animation Channel (Bool) "world" 추가
	if (!LastControlName.IsNone())
	{
		FName ChannelName(TEXT("world"));
		FRigElementKey LastControlKey(LastControlName, ERigElementType::Control);
		
		// Animation Channel 설정
		FRigControlSettings ChannelSettings;
		ChannelSettings.ControlType = ERigControlType::Bool;
		ChannelSettings.DisplayName = FName(TEXT("world"));
		ChannelSettings.AnimationType = ERigControlAnimationType::AnimationChannel;
		
		FRigElementKey ChannelKey = HC->AddControl(
			ChannelName,
			LastControlKey,
			ChannelSettings,
			FRigControlValue::Make<bool>(false),
			FTransform::Identity,
			FTransform::Identity,
			false
		);
		
		if (ChannelKey.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("  Created Animation Channel 'world' under %s"), *LastControlName.ToString());
		}
	}
	
	// 컨트롤러 이름 배열 생성
	TArray<FName> WeaponCtrls;
	for (const FName& BoneName : SortedBones)
	{
		if (FName* CtrlName = BoneToControlMap.Find(BoneName))
		{
			WeaponCtrls.Add(*CtrlName);
		}
	}
	
	// Weapon 함수 노드 연결
	if (PendingControlRig.IsValid())
	{
		ConnectWeaponFunctionNodes(PendingControlRig.Get(), bIsLeft, SpaceName, SortedBones, WeaponCtrls);
	}
}

void SControlRigToolWidget::ConnectWeaponFunctionNodes(UControlRigBlueprint* Rig, 
	bool bIsLeft, const FName& WeaponSpaceName, const TArray<FName>& WeaponBones, const TArray<FName>& WeaponCtrls)
{
	if (!Rig || WeaponBones.Num() == 0) return;
	
	FString DebugInfo;
	DebugInfo += FString::Printf(TEXT("=== Weapon Function Nodes (%s) ===\n"), bIsLeft ? TEXT("Left") : TEXT("Right"));
	
	TArray<URigVMGraph*> AllGraphs = Rig->GetAllModels();
	URigVMGraph* MainGraph = nullptr;
	
	for (URigVMGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName().Equals(TEXT("RigVMModel")))
		{
			MainGraph = Graph;
			break;
		}
	}
	
	if (!MainGraph)
	{
		DebugInfo += TEXT("[ERROR] Main graph not found!\n");
		ShowDebugPopup(TEXT("Weapon Function Debug"), DebugInfo);
		return;
	}
	
	URigVMController* Controller = Rig->GetController(MainGraph);
	if (!Controller)
	{
		DebugInfo += TEXT("[ERROR] Controller not found!\n");
		ShowDebugPopup(TEXT("Weapon Function Debug"), DebugInfo);
		return;
	}
	
	// Hand 본 이름
	FName HandBoneName = bIsLeft ? FName(TEXT("hand_l")) : FName(TEXT("hand_r"));
	FName ActualHandBone = LastBoneMapping.FindRef(HandBoneName);
	if (ActualHandBone.IsNone()) ActualHandBone = HandBoneName;
	
	// 체인의 마지막 컨트롤러 이름 (Get Bool Channel용)
	FString LastCtrlName = WeaponCtrls.Num() > 0 ? WeaponCtrls.Last().ToString() : TEXT("None");
	
	// 빈 Weapon 함수 노드 찾아서 위치 저장 후 삭제 (L을 처리할 때만)
	FVector2D SetupStartPos(1500.0f, 600.0f);
	FVector2D ForwardStartPos(2500.0f, 600.0f);
	FVector2D BackwardStartPos(3500.0f, 600.0f);
	
	URigVMNode* FingerSetupPrev = nullptr;
	URigVMNode* FingerForwardPrev = nullptr;
	URigVMNode* FingerBackwardPrev = nullptr;
	
	if (bIsLeft)  // L을 처리할 때만 빈 노드 삭제
	{
		TArray<URigVMNode*> NodesToRemove;
		
		for (URigVMNode* Node : MainGraph->GetNodes())
		{
			FString NodeName = Node->GetName();
			
			// AI_Setup_Weapon (빈 템플릿)
			if (NodeName.Equals(TEXT("AI_Setup_Weapon")) || 
				(NodeName.StartsWith(TEXT("AI_Setup_Weapon")) && NodeName.Len() < 20))
			{
				// 이전에 연결된 Finger 노드 찾기
				for (URigVMPin* Pin : Node->GetPins())
				{
					if (Pin->GetName().Contains(TEXT("Execute")))
					{
						for (URigVMLink* Link : Pin->GetLinks())
						{
							URigVMPin* OtherPin = Link->GetSourcePin();
							if (OtherPin && OtherPin->GetNode() != Node)
							{
								URigVMNode* PrevNode = OtherPin->GetNode();
								if (PrevNode->GetName().Contains(TEXT("Finger")))
								{
									FingerSetupPrev = PrevNode;
								}
							}
						}
					}
				}
				SetupStartPos = Node->GetPosition();
				NodesToRemove.Add(Node);
				DebugInfo += FString::Printf(TEXT("Found empty AI_Setup_Weapon at (%.0f, %.0f)\n"), SetupStartPos.X, SetupStartPos.Y);
			}
			
			// AI_Forward_Weapon
			if (NodeName.Equals(TEXT("AI_Forward_Weapon")) || 
				(NodeName.StartsWith(TEXT("AI_Forward_Weapon")) && NodeName.Len() < 22))
			{
				for (URigVMPin* Pin : Node->GetPins())
				{
					if (Pin->GetName().Contains(TEXT("Execute")))
					{
						for (URigVMLink* Link : Pin->GetLinks())
						{
							URigVMPin* OtherPin = Link->GetSourcePin();
							if (OtherPin && OtherPin->GetNode() != Node)
							{
								URigVMNode* PrevNode = OtherPin->GetNode();
								if (PrevNode->GetName().Contains(TEXT("Finger")))
								{
									FingerForwardPrev = PrevNode;
								}
							}
						}
					}
				}
				ForwardStartPos = Node->GetPosition();
				NodesToRemove.Add(Node);
				DebugInfo += FString::Printf(TEXT("Found empty AI_Forward_Weapon at (%.0f, %.0f)\n"), ForwardStartPos.X, ForwardStartPos.Y);
			}
			
			// AI_Backward_Weapon
			if (NodeName.Equals(TEXT("AI_Backward_Weapon")) || 
				(NodeName.StartsWith(TEXT("AI_Backward_Weapon")) && NodeName.Len() < 23))
			{
				for (URigVMPin* Pin : Node->GetPins())
				{
					if (Pin->GetName().Contains(TEXT("Execute")))
					{
						for (URigVMLink* Link : Pin->GetLinks())
						{
							URigVMPin* OtherPin = Link->GetSourcePin();
							if (OtherPin && OtherPin->GetNode() != Node)
							{
								URigVMNode* PrevNode = OtherPin->GetNode();
								if (PrevNode->GetName().Contains(TEXT("Finger")))
								{
									FingerBackwardPrev = PrevNode;
								}
							}
						}
					}
				}
				BackwardStartPos = Node->GetPosition();
				NodesToRemove.Add(Node);
				DebugInfo += FString::Printf(TEXT("Found empty AI_Backward_Weapon at (%.0f, %.0f)\n"), BackwardStartPos.X, BackwardStartPos.Y);
			}
		}
		
		// 빈 노드 삭제
		for (URigVMNode* Node : NodesToRemove)
		{
			DebugInfo += FString::Printf(TEXT("Removing: %s\n"), *Node->GetName());
			Controller->RemoveNode(Node, false, false);
		}
	}
	
	// 이미 만든 Weapon 노드 찾기 (R 처리 시)
	URigVMNode* PrevWeaponSetup = nullptr;
	URigVMNode* PrevWeaponForward = nullptr;
	URigVMNode* PrevWeaponBackward = nullptr;
	
	if (!bIsLeft)
	{
		for (URigVMNode* Node : MainGraph->GetNodes())
		{
			FString NodeName = Node->GetName();
			if (NodeName.Contains(TEXT("AI_Setup_Weapon")))
			{
				if (!PrevWeaponSetup || Node->GetPosition().X > PrevWeaponSetup->GetPosition().X)
				{
					PrevWeaponSetup = Node;
					SetupStartPos = Node->GetPosition();
				}
			}
			if (NodeName.Contains(TEXT("AI_Forward_Weapon")))
			{
				if (!PrevWeaponForward || Node->GetPosition().X > PrevWeaponForward->GetPosition().X)
				{
					PrevWeaponForward = Node;
					ForwardStartPos = Node->GetPosition();
				}
			}
			if (NodeName.Contains(TEXT("AI_Backward_Weapon")))
			{
				if (!PrevWeaponBackward || Node->GetPosition().X > PrevWeaponBackward->GetPosition().X)
				{
					PrevWeaponBackward = Node;
					BackwardStartPos = Node->GetPosition();
				}
			}
		}
	}
	
	// 노드 간격 (가로 방향)
	const float XSpacing = 400.0f;
	
	float SetupX = bIsLeft ? SetupStartPos.X : SetupStartPos.X + XSpacing;
	float ForwardX = bIsLeft ? ForwardStartPos.X : ForwardStartPos.X + XSpacing;
	float BackwardX = bIsLeft ? BackwardStartPos.X : BackwardStartPos.X + XSpacing;
	
	DebugInfo += FString::Printf(TEXT("\nHand bone: %s\n"), *ActualHandBone.ToString());
	DebugInfo += FString::Printf(TEXT("Weapon space: %s\n"), *WeaponSpaceName.ToString());
	DebugInfo += FString::Printf(TEXT("Last ctrl (for GetBool): %s\n\n"), *LastCtrlName);
	
	// Execute 연결 헬퍼
	auto TryLink = [Controller](URigVMNode* From, URigVMNode* To, FString& Dbg) -> bool {
		if (!From || !To) return false;
		bool ok = Controller->AddLink(From->GetName() + TEXT(".Execute"), To->GetName() + TEXT(".Execute"), false);
		if (!ok) ok = Controller->AddLink(From->GetName() + TEXT(".ExecuteContext"), To->GetName() + TEXT(".ExecuteContext"), false);
		Dbg += FString::Printf(TEXT("  Link: %s -> %s (%s)\n"), *From->GetName(), *To->GetName(), ok ? TEXT("OK") : TEXT("FAIL"));
		return ok;
	};
	
	// AI_Setup_Weapon
	URigVMNode* SetupNode = AddFunctionReferenceNode(Controller, TEXT("AI_Setup_Weapon"), 
		FVector2D(SetupX, SetupStartPos.Y), DebugInfo);
	if (SetupNode)
	{
		FString NodeName = SetupNode->GetName();
		FVector2D NodePos = SetupNode->GetPosition();
		
		Controller->SetPinDefaultValue(NodeName + TEXT(".Handbone"), 
			FString::Printf(TEXT("(Type=Bone,Name=\"%s\")"), *ActualHandBone.ToString()), true, false, false);
		Controller->SetPinDefaultValue(NodeName + TEXT(".Wp_space"), 
			FString::Printf(TEXT("(Type=Null,Name=\"%s\")"), *WeaponSpaceName.ToString()), true, false, false);
		
		FName ArrayMakeNotation = FRigVMDispatch_ArrayMake().GetTemplateNotation();
		
		// 함수 노드 하단부에 배치
		URigVMTemplateNode* BonesArr = Controller->AddTemplateNode(ArrayMakeNotation, 
			FVector2D(NodePos.X - 80.0f, NodePos.Y + 160.0f), FString(), false, false);
		if (BonesArr)
		{
			Controller->AddLink(BonesArr->GetName() + TEXT(".Array"), NodeName + TEXT(".Bone"), false);
			for (int32 i = 0; i < WeaponBones.Num(); ++i)
				Controller->InsertArrayPin(BonesArr->GetName() + TEXT(".Values"), i, 
					FString::Printf(TEXT("(Type=Bone,Name=\"%s\")"), *WeaponBones[i].ToString()), false, false);
			Controller->SetPinExpansion(BonesArr->GetName() + TEXT(".Values"), false, false);
		}
		
		URigVMTemplateNode* CtrlsArr = Controller->AddTemplateNode(ArrayMakeNotation, 
			FVector2D(NodePos.X - 80.0f, NodePos.Y + 260.0f), FString(), false, false);
		if (CtrlsArr)
		{
			Controller->AddLink(CtrlsArr->GetName() + TEXT(".Array"), NodeName + TEXT(".Ctrl"), false);
			for (int32 i = 0; i < WeaponCtrls.Num(); ++i)
				Controller->InsertArrayPin(CtrlsArr->GetName() + TEXT(".Values"), i, 
					FString::Printf(TEXT("(Type=Control,Name=\"%s\")"), *WeaponCtrls[i].ToString()), false, false);
			Controller->SetPinExpansion(CtrlsArr->GetName() + TEXT(".Values"), false, false);
		}
		
		URigVMNode* PrevNode = bIsLeft ? FingerSetupPrev : PrevWeaponSetup;
		TryLink(PrevNode, SetupNode, DebugInfo);
		
		DebugInfo += FString::Printf(TEXT("Created AI_Setup_Weapon at (%.0f, %.0f)\n"), NodePos.X, NodePos.Y);
	}
	
	// AI_Forward_Weapon
	URigVMNode* ForwardNode = AddFunctionReferenceNode(Controller, TEXT("AI_Forward_Weapon"), 
		FVector2D(ForwardX, ForwardStartPos.Y), DebugInfo);
	if (ForwardNode)
	{
		FString NodeName = ForwardNode->GetName();
		FVector2D NodePos = ForwardNode->GetPosition();
		
		Controller->SetPinDefaultValue(NodeName + TEXT(".handbone"), 
			FString::Printf(TEXT("(Type=Bone,Name=\"%s\")"), *ActualHandBone.ToString()), true, false, false);
		Controller->SetPinDefaultValue(NodeName + TEXT(".rootbone"), TEXT("(Type=Bone,Name=\"Root\")"), true, false, false);
		Controller->SetPinDefaultValue(NodeName + TEXT(".wp_space"), 
			FString::Printf(TEXT("(Type=Null,Name=\"%s\")"), *WeaponSpaceName.ToString()), true, false, false);
		
		FName ArrayMakeNotation = FRigVMDispatch_ArrayMake().GetTemplateNotation();
		
		// 함수 노드 하단부에 배치
		URigVMTemplateNode* BonesArr = Controller->AddTemplateNode(ArrayMakeNotation, 
			FVector2D(NodePos.X - 80.0f, NodePos.Y + 200.0f), FString(), false, false);
		if (BonesArr)
		{
			Controller->AddLink(BonesArr->GetName() + TEXT(".Array"), NodeName + TEXT(".bone"), false);
			for (int32 i = 0; i < WeaponBones.Num(); ++i)
				Controller->InsertArrayPin(BonesArr->GetName() + TEXT(".Values"), i, 
					FString::Printf(TEXT("(Type=Bone,Name=\"%s\")"), *WeaponBones[i].ToString()), false, false);
			Controller->SetPinExpansion(BonesArr->GetName() + TEXT(".Values"), false, false);
		}
		
		URigVMTemplateNode* CtrlsArr = Controller->AddTemplateNode(ArrayMakeNotation, 
			FVector2D(NodePos.X - 80.0f, NodePos.Y + 300.0f), FString(), false, false);
		if (CtrlsArr)
		{
			Controller->AddLink(CtrlsArr->GetName() + TEXT(".Array"), NodeName + TEXT(".ctrl"), false);
			for (int32 i = 0; i < WeaponCtrls.Num(); ++i)
				Controller->InsertArrayPin(CtrlsArr->GetName() + TEXT(".Values"), i, 
					FString::Printf(TEXT("(Type=Control,Name=\"%s\")"), *WeaponCtrls[i].ToString()), false, false);
			Controller->SetPinExpansion(CtrlsArr->GetName() + TEXT(".Values"), false, false);
		}
		
		// Get Bool Channel (체인의 마지막 컨트롤러 사용)
		// Control과 Channel 핀은 FName 타입이므로 이름만 설정
		UScriptStruct* GetBoolStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ControlRig.RigUnit_GetBoolAnimationChannel"));
		if (!GetBoolStruct) GetBoolStruct = LoadObject<UScriptStruct>(nullptr, TEXT("/Script/ControlRig.RigUnit_GetBoolAnimationChannel"));
		
		if (GetBoolStruct && !LastCtrlName.Equals(TEXT("None")))
		{
			URigVMNode* GetBoolNode = Controller->AddUnitNode(GetBoolStruct, TEXT("Execute"),
				FVector2D(NodePos.X - 150.0f, NodePos.Y + 420.0f), FString(), false, false);
			if (GetBoolNode)
			{
				// Control 핀: FName 타입이므로 이름만 설정 (FRigElementKey 형식 아님)
				Controller->SetPinDefaultValue(GetBoolNode->GetName() + TEXT(".Control"), LastCtrlName, true, false, false);
				Controller->SetPinDefaultValue(GetBoolNode->GetName() + TEXT(".Channel"), TEXT("world"), true, false, false);
				Controller->AddLink(GetBoolNode->GetName() + TEXT(".Value"), NodeName + TEXT(".world"), false);
				DebugInfo += FString::Printf(TEXT("  GetBool: Control=%s, Channel=world\n"), *LastCtrlName);
			}
		}
		
		URigVMNode* PrevNode = bIsLeft ? FingerForwardPrev : PrevWeaponForward;
		TryLink(PrevNode, ForwardNode, DebugInfo);
		
		DebugInfo += FString::Printf(TEXT("Created AI_Forward_Weapon at (%.0f, %.0f)\n"), NodePos.X, NodePos.Y);
	}
	
	// AI_Backward_Weapon
	URigVMNode* BackwardNode = AddFunctionReferenceNode(Controller, TEXT("AI_Backward_Weapon"), 
		FVector2D(BackwardX, BackwardStartPos.Y), DebugInfo);
	if (BackwardNode)
	{
		FString NodeName = BackwardNode->GetName();
		FVector2D NodePos = BackwardNode->GetPosition();
		
		Controller->SetPinDefaultValue(NodeName + TEXT(".handbone"), 
			FString::Printf(TEXT("(Type=Bone,Name=\"%s\")"), *ActualHandBone.ToString()), true, false, false);
		Controller->SetPinDefaultValue(NodeName + TEXT(".wp_space"), 
			FString::Printf(TEXT("(Type=Null,Name=\"%s\")"), *WeaponSpaceName.ToString()), true, false, false);
		
		FName ArrayMakeNotation = FRigVMDispatch_ArrayMake().GetTemplateNotation();
		
		// 함수 노드 하단부에 배치
		URigVMTemplateNode* BonesArr = Controller->AddTemplateNode(ArrayMakeNotation, 
			FVector2D(NodePos.X - 80.0f, NodePos.Y + 160.0f), FString(), false, false);
		if (BonesArr)
		{
			Controller->AddLink(BonesArr->GetName() + TEXT(".Array"), NodeName + TEXT(".bones"), false);
			for (int32 i = 0; i < WeaponBones.Num(); ++i)
				Controller->InsertArrayPin(BonesArr->GetName() + TEXT(".Values"), i, 
					FString::Printf(TEXT("(Type=Bone,Name=\"%s\")"), *WeaponBones[i].ToString()), false, false);
			Controller->SetPinExpansion(BonesArr->GetName() + TEXT(".Values"), false, false);
		}
		
		URigVMTemplateNode* CtrlsArr = Controller->AddTemplateNode(ArrayMakeNotation, 
			FVector2D(NodePos.X - 80.0f, NodePos.Y + 260.0f), FString(), false, false);
		if (CtrlsArr)
		{
			Controller->AddLink(CtrlsArr->GetName() + TEXT(".Array"), NodeName + TEXT(".ctrls"), false);
			for (int32 i = 0; i < WeaponCtrls.Num(); ++i)
				Controller->InsertArrayPin(CtrlsArr->GetName() + TEXT(".Values"), i, 
					FString::Printf(TEXT("(Type=Control,Name=\"%s\")"), *WeaponCtrls[i].ToString()), false, false);
			Controller->SetPinExpansion(CtrlsArr->GetName() + TEXT(".Values"), false, false);
		}
		
		URigVMNode* PrevNode = bIsLeft ? FingerBackwardPrev : PrevWeaponBackward;
		TryLink(PrevNode, BackwardNode, DebugInfo);
		
		DebugInfo += FString::Printf(TEXT("Created AI_Backward_Weapon at (%.0f, %.0f)\n"), NodePos.X, NodePos.Y);
	}
	
	DebugInfo += TEXT("\n=== Complete ===\n");
	ShowDebugPopup(TEXT("Weapon Function Debug"), DebugInfo);
}

// ============================================================================
// 본별 버텍스 기반 Shape Info 계산 (스케일 + 오프셋)
// 컨트롤러가 메쉬 바깥으로 나오도록 위치와 크기를 계산
// ============================================================================
void SControlRigToolWidget::CalculateBoneShapeInfos(USkeletalMesh* Mesh)
{
	BoneShapeInfoMap.Empty();
	
	if (!Mesh) return;
	
	// 본별 버텍스 정보 계산
	TArray<FBoneVertInfo> BoneVertInfos;
	FMeshUtilitiesEngine::CalcBoneVertInfos(Mesh, BoneVertInfos, true);
	
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// 스케일 설정 - 버텍스 바운딩 박스 기준
	// BoxSize 100 -> Scale 1.0 정도가 되도록
	constexpr float ScaleDivisor = 100.0f;   // 이 값으로 나눔
	constexpr float MinScale = 0.15f;
	constexpr float MaxScale = 5.0f;
	constexpr float OffsetMargin = 1.3f;  // 오프셋 마진 (바깥으로 더 밀어냄)
	
	FString DebugLog = TEXT("=== Auto Shape Info Calculation ===\n");
	
	for (int32 BoneIdx = 0; BoneIdx < RefSkel.GetRawBoneNum(); ++BoneIdx)
	{
		FName BoneName = RefSkel.GetBoneName(BoneIdx);
		FBoneShapeInfo ShapeInfo;
		
		if (BoneIdx >= BoneVertInfos.Num() || BoneVertInfos[BoneIdx].Positions.Num() == 0)
		{
			// 버텍스 없음 (스킨 웨이트 없는 본) - 기본값
			ShapeInfo.Scale = FVector(0.3f, 0.3f, 0.3f);
			ShapeInfo.Offset = FVector::ZeroVector;
			ShapeInfo.AverageNormal = FVector(0.0f, 0.0f, 1.0f);  // 기본 Z축
			BoneShapeInfoMap.Add(BoneName, ShapeInfo);
			continue;
		}
		
		const FBoneVertInfo& Info = BoneVertInfos[BoneIdx];
		
		// 버텍스들의 중심 계산 (본 로컬 스페이스)
		FVector VertexCenter = FVector::ZeroVector;
		for (const FVector3f& Pos : Info.Positions)
		{
			VertexCenter += FVector(Pos);
		}
		VertexCenter /= Info.Positions.Num();
		
		// 버텍스 노멀 평균 계산 (메쉬 표면의 바깥 방향)
		FVector AverageNormal = FVector::ZeroVector;
		if (Info.Normals.Num() > 0)
		{
			for (const FVector3f& Normal : Info.Normals)
			{
				AverageNormal += FVector(Normal);
			}
			AverageNormal.Normalize();
		}
		else
		{
			// 노멀이 없으면 Z축 사용
			AverageNormal = FVector(0.0f, 0.0f, 1.0f);
		}
		ShapeInfo.AverageNormal = AverageNormal;
		
		// 버텍스들의 바운딩 박스 계산
		FBox BoneBox(ForceInit);
		float MaxDistFromCenter = 0.0f;
		FVector FurthestDirection = FVector::ZeroVector;
		
		for (const FVector3f& Pos : Info.Positions)
		{
			FVector VPos(Pos);
			BoneBox += VPos;
			
			// 본 원점에서 가장 먼 버텍스 방향 찾기
			float Dist = VPos.Size();
			if (Dist > MaxDistFromCenter)
			{
				MaxDistFromCenter = Dist;
				FurthestDirection = VPos.GetSafeNormal();
			}
		}
		
		// 박스 크기에서 XYZ 각각 스케일 계산 (직육면체)
		FVector BoxSize = BoneBox.GetSize();
		
		// 스케일: BoxSize / 100 정도가 되도록 (BoxSize 100 -> Scale 1.0)
		ShapeInfo.Scale.X = FMath::Clamp(BoxSize.X / ScaleDivisor, MinScale, MaxScale);
		ShapeInfo.Scale.Y = FMath::Clamp(BoxSize.Y / ScaleDivisor, MinScale, MaxScale);
		ShapeInfo.Scale.Z = FMath::Clamp(BoxSize.Z / ScaleDivisor, MinScale, MaxScale);
		
		// 오프셋: 버텍스 중심 방향으로 가장 먼 거리 + 마진
		// 이렇게 하면 컨트롤러가 메쉬 표면 바깥에 위치
		if (VertexCenter.Size() > 1.0f)
		{
			// 버텍스 중심 방향으로 이동
			FVector CenterDirection = VertexCenter.GetSafeNormal();
			float CenterDist = VertexCenter.Size();
			
			// 바운딩 박스의 중심 방향 반경 계산
			float BoxRadius = (BoxSize * 0.5f).Size();
			
			// 오프셋 = 중심 방향으로 (중심까지 거리 + 반경의 일부) 이동
			ShapeInfo.Offset = CenterDirection * (CenterDist + BoxRadius * 0.5f) * OffsetMargin;
		}
		else
		{
			// 버텍스가 본 원점 근처에 있으면 가장 먼 방향으로 이동
			ShapeInfo.Offset = FurthestDirection * MaxDistFromCenter * OffsetMargin;
		}
		
		BoneShapeInfoMap.Add(BoneName, ShapeInfo);
		
		DebugLog += FString::Printf(TEXT("  %s: Verts=%d, Scale=(%.2f, %.2f, %.2f), Offset=(%.1f, %.1f, %.1f)\n"),
			*BoneName.ToString(), Info.Positions.Num(), 
			ShapeInfo.Scale.X, ShapeInfo.Scale.Y, ShapeInfo.Scale.Z,
			ShapeInfo.Offset.X, ShapeInfo.Offset.Y, ShapeInfo.Offset.Z);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Calculated shape infos for %d bones"), BoneShapeInfoMap.Num());
	
	// 디버그 팝업 제거 - Body Controls Update 팝업에서 확인
	// ShowDebugPopup(TEXT("Auto Shape Info Debug"), DebugLog);
}

SControlRigToolWidget::FBoneShapeInfo SControlRigToolWidget::GetBoneShapeInfo(const FName& BoneName) const
{
	if (const FBoneShapeInfo* Info = BoneShapeInfoMap.Find(BoneName))
	{
		return *Info;
	}
	FBoneShapeInfo Default;
	Default.Scale = FVector(0.3f, 0.3f, 0.3f);
	Default.Offset = FVector::ZeroVector;
	return Default;
}

// ============================================================================
// 바디 컨트롤러 오토스케일 적용 (템플릿에서 복사된 컨트롤러들)
// Shape 모양은 유지, 크기를 메쉬에 맞게 조정
// ============================================================================
void SControlRigToolWidget::ApplyAutoScaleToBodyControls(UControlRigBlueprint* Rig, USkeletalMesh* Mesh)
{
	if (!Rig || !Mesh) return;
	
	URigHierarchy* Hierarchy = Rig->Hierarchy;
	URigHierarchyController* HC = Rig->GetHierarchyController();
	
	if (!Hierarchy || !HC) return;
	
	// 먼저 버텍스 기반 Shape Info 계산
	CalculateBoneShapeInfos(Mesh);
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] === Applying Auto Shape to Body Controls ==="));
	
	int32 UpdatedCount = 0;
	FString DebugLog = TEXT("=== Body Controls Update ===\n");
	
	// 컨트롤러 키 목록 먼저 수집 (순회 중 수정 방지)
	TArray<FRigElementKey> ControlKeys;
	Hierarchy->ForEach<FRigControlElement>([&](FRigControlElement* ControlElement) -> bool
	{
		if (ControlElement)
		{
			ControlKeys.Add(ControlElement->GetKey());
		}
		return true;
	});
	
	// 수집된 키로 설정 변경
	for (const FRigElementKey& ControlKey : ControlKeys)
	{
		FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(ControlKey);
		if (!ControlElement) continue;
		
		FName ControlName = ControlElement->GetFName();
		FString ControlNameStr = ControlName.ToString();
		
		// 컨트롤러 이름에서 본 이름 추출 (예: pelvis_ctrl -> pelvis)
		FString BoneNameStr = ControlNameStr;
		if (BoneNameStr.EndsWith(TEXT("_ctrl")))
		{
			BoneNameStr = BoneNameStr.LeftChop(5);  // "_ctrl" 제거
		}
		else if (BoneNameStr.EndsWith(TEXT("_ik")))
		{
			BoneNameStr = BoneNameStr.LeftChop(3);  // "_ik" 제거
		}
		else if (BoneNameStr.EndsWith(TEXT("_fk")))
		{
			BoneNameStr = BoneNameStr.LeftChop(3);  // "_fk" 제거
		}
		
		FName TemplateBoneName(*BoneNameStr);
		
		// 템플릿 본 이름 → 실제 메쉬 본 이름으로 매핑
		// LastBoneMapping은 target(실제메쉬본) -> source(템플릿본) 구조
		// 역으로 찾아야 함
		FName MeshBoneName = TemplateBoneName;  // 기본값
		for (const auto& Mapping : LastBoneMapping)
		{
			if (Mapping.Value == TemplateBoneName)
			{
				MeshBoneName = Mapping.Key;  // 실제 메쉬 본 이름
				break;
			}
		}
		
		// 해당 본의 Shape Info 가져오기 (실제 메쉬 본 이름으로)
		FBoneShapeInfo ShapeInfo = GetBoneShapeInfo(MeshBoneName);
		
		// 현재 Settings 복사
		FRigControlSettings NewSettings = ControlElement->Settings;
		FVector OldScale = NewSettings.ShapeTransform.GetScale3D();
		
		// ShapeTransform 스케일만 변경 (XYZ 각각)
		NewSettings.ShapeTransform.SetScale3D(ShapeInfo.Scale);
		
		// SetControlSettings API로 적용
		bool bSuccess = HC->SetControlSettings(ControlKey, NewSettings, false);
		
		if (bSuccess)
		{
			UpdatedCount++;
			DebugLog += FString::Printf(TEXT("  %s: (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f) [%s -> %s] ✓\n"), 
				*ControlNameStr, 
				OldScale.X, OldScale.Y, OldScale.Z,
				ShapeInfo.Scale.X, ShapeInfo.Scale.Y, ShapeInfo.Scale.Z,
				*BoneNameStr, *MeshBoneName.ToString());
		}
		else
		{
			DebugLog += FString::Printf(TEXT("  %s: FAILED [%s -> %s]\n"), 
				*ControlNameStr, *BoneNameStr, *MeshBoneName.ToString());
		}
	}
	
	
	// 블루프린트 더티 마킹
	Rig->MarkPackageDirty();
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Updated %d body controls with auto shape"), UpdatedCount);
	ShowDebugPopup(TEXT("Body Controls Update"), DebugLog);
}

// ============================================================================
// IK Rig Generation
// ============================================================================

TSharedRef<SWidget> SControlRigToolWidget::CreateIKRigSection()
{
	// IK Rig 템플릿 로드
	LoadIKRigTemplates();
	
	return SNew(SVerticalBox)
		// 섹션 헤더
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("ClassIcon.IKRigDefinition"))
				.ColorAndOpacity(FLinearColor(0.9f, 0.5f, 0.2f, 1.0f))
				.DesiredSizeOverride(FVector2D(14, 14))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("IKRigSection", "IK Rig Generation"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f))
			]
		]
		
		// Template IK Rig 드롭다운
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("IKRigTemplate", "Template IK Rig:"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SAssignNew(IKRigTemplateComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&IKRigTemplateOptions)
				.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateIKRigTemplateWidget)
				.OnSelectionChanged(this, &SControlRigToolWidget::OnIKRigTemplateSelectionChanged)
				[
					SNew(STextBlock)
					.Text(this, &SControlRigToolWidget::GetSelectedIKRigTemplateName)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
			]
		]
		
		// Create IK Rig 버튼
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "FlatButton.Primary")
			.ContentPadding(FMargin(16, 10))
			.HAlign(HAlign_Center)
			.OnClicked(this, &SControlRigToolWidget::OnCreateIKRigClicked)
			.IsEnabled_Lambda([this]() { return LastBoneMapping.Num() > 0 && SelectedIKRigTemplate.IsValid(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Plus"))
					.ColorAndOpacity(FLinearColor::White)
					.DesiredSizeOverride(FVector2D(14, 14))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CreateIKRig", "Create IK Rig"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(FLinearColor::White)
				]
			]
		];
}

void SControlRigToolWidget::LoadIKRigTemplates()
{
	IKRigTemplateOptions.Empty();
	
	// Asset Registry에서 IK Rig 에셋 검색
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	
	FARFilter Filter;
	Filter.ClassPaths.Add(UIKRigDefinition::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	
	TArray<FAssetData> AssetList;
	AssetRegistry.Get().GetAssets(Filter, AssetList);
	
	for (const FAssetData& Asset : AssetList)
	{
		FString AssetPath = Asset.GetObjectPathString();
		IKRigTemplateOptions.Add(MakeShared<FString>(AssetPath));
	}
	
	// 기본 선택 - "AI_IK_Rig_Template" 우선 선택
	SelectedIKRigTemplate = nullptr;
	for (const TSharedPtr<FString>& Option : IKRigTemplateOptions)
	{
		if (Option.IsValid() && Option->Contains(TEXT("AI_IK_Rig_Template")))
		{
			SelectedIKRigTemplate = Option;
			break;
		}
	}
	
	// 기본 템플릿이 없으면 첫 번째 선택
	if (!SelectedIKRigTemplate.IsValid() && IKRigTemplateOptions.Num() > 0)
	{
		SelectedIKRigTemplate = IKRigTemplateOptions[0];
	}
	
	UE_LOG(LogTemp, Log, TEXT("[IKRig] Found %d IK Rig templates, selected: %s"), 
		IKRigTemplateOptions.Num(), 
		SelectedIKRigTemplate.IsValid() ? **SelectedIKRigTemplate : TEXT("None"));
}

TSharedRef<SWidget> SControlRigToolWidget::OnGenerateIKRigTemplateWidget(TSharedPtr<FString> InItem)
{
	FString DisplayName = FPaths::GetBaseFilename(*InItem);
	return SNew(STextBlock)
		.Text(FText::FromString(DisplayName))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
}

void SControlRigToolWidget::OnIKRigTemplateSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	SelectedIKRigTemplate = NewValue;
	UpdateIKTemplateThumbnail();
}

FText SControlRigToolWidget::GetSelectedIKRigTemplateName() const
{
	if (SelectedIKRigTemplate.IsValid())
	{
		return FText::FromString(FPaths::GetBaseFilename(*SelectedIKRigTemplate));
	}
	return LOCTEXT("NoIKRigTemplate", "Select IK Rig Template...");
}

FReply SControlRigToolWidget::OnCreateIKRigClicked()
{
	if (!SelectedIKRigTemplate.IsValid() || !SelectedIKMesh.IsValid() || IKBoneMapping.Num() == 0)
	{
		SetIKStatus(TEXT("Error: Select template, mesh and run AI Bone Mapping first"));
		return FReply::Handled();
	}
	
	CreateIKRigFromTemplate();
	return FReply::Handled();
}

void SControlRigToolWidget::CreateIKRigFromTemplate()
{
	SetIKStatus(TEXT("Creating IK Rig..."));
	
	// 1. 템플릿 IK Rig 로드
	UIKRigDefinition* Template = LoadObject<UIKRigDefinition>(nullptr, **SelectedIKRigTemplate);
	if (!Template)
	{
		SetIKStatus(TEXT("Error: Failed to load IK Rig template"));
		return;
	}
	
	// 2. 대상 스켈레탈 메쉬 로드
	FString MeshPath = GetSelectedIKMeshPath();
	USkeletalMesh* TargetMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!TargetMesh)
	{
		SetIKStatus(TEXT("Error: Failed to load skeletal mesh"));
		return;
	}
	
	// 3. 새 에셋 경로 생성
	FString OutputFolder = IKOutputFolderBox.IsValid() ? IKOutputFolderBox->GetText().ToString() : IKDefaultOutputFolder;
	FString OutputName = IKOutputNameBox.IsValid() ? IKOutputNameBox->GetText().ToString() : TEXT("NewIKRig");
	FString NewAssetPath = OutputFolder / OutputName;
	FString PackagePath = NewAssetPath;
	
	// 4. 패키지 생성
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		SetIKStatus(TEXT("Error: Failed to create package"));
		return;
	}
	
	// 5. 템플릿 복제
	FString AssetName = FPaths::GetBaseFilename(NewAssetPath);
	UIKRigDefinition* NewIKRig = DuplicateObject<UIKRigDefinition>(Template, Package, *AssetName);
	if (!NewIKRig)
	{
		SetIKStatus(TEXT("Error: Failed to duplicate IK Rig"));
		return;
	}
	
	NewIKRig->SetFlags(RF_Public | RF_Standalone);
	
	// 6. IK Rig Controller 얻기
	UIKRigController* Controller = UIKRigController::GetController(NewIKRig);
	if (!Controller)
	{
		SetIKStatus(TEXT("Error: Failed to get IK Rig controller"));
		return;
	}
	
	// 7. 스켈레탈 메쉬 설정
	bool bMeshSet = Controller->SetSkeletalMesh(TargetMesh);
	if (!bMeshSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IKRig] SetSkeletalMesh returned false, but continuing..."));
	}
	
	// 8. 각 체인의 본 이름 교체 (IKBoneMapping 사용)
	// IKBoneMapping 형식: Key = UE5 표준 본 (템플릿), Value = 실제 메쉬 본
	FString DebugLog = TEXT("=== IK Rig Bone Remapping ===\n");
	DebugLog += FString::Printf(TEXT("IKBoneMapping entries: %d\n"), IKBoneMapping.Num());
	
	// 디버그: 매핑 내용 출력
	for (const auto& M : IKBoneMapping)
	{
		DebugLog += FString::Printf(TEXT("  %s -> %s\n"), *M.Key.ToString(), *M.Value.ToString());
	}
	DebugLog += TEXT("\n");
	
	// 본 타입별로 매핑된 본들을 그룹화 (끝본 폴백용)
	// 예: "spine" -> [(1, spine_01, Bip_Spine), (2, spine_02, Bip_Spine1), ...]
	auto ExtractBoneTypeAndIndex = [](const FString& BoneName) -> TPair<FString, int32>
	{
		// spine_01, spine_02, neck_01, neck_02, thumb_01_l 등에서 타입과 인덱스 추출
		FString Lower = BoneName.ToLower();
		
		// 패턴: {type}_{index} 또는 {type}_{index}_{side}
		int32 LastUnderscore = Lower.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (LastUnderscore == INDEX_NONE) return TPair<FString, int32>(TEXT(""), -1);
		
		FString LastPart = Lower.Mid(LastUnderscore + 1);
		
		// 사이드 체크 (l, r)
		if (LastPart == TEXT("l") || LastPart == TEXT("r"))
		{
			// {type}_{index}_{side} 형태
			FString WithoutSide = Lower.Left(LastUnderscore);
			int32 SecondLastUnderscore = WithoutSide.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (SecondLastUnderscore != INDEX_NONE)
			{
				FString IndexPart = WithoutSide.Mid(SecondLastUnderscore + 1);
				FString TypePart = WithoutSide.Left(SecondLastUnderscore) + TEXT("_") + LastPart; // 예: thumb_l
				if (IndexPart.IsNumeric())
				{
					return TPair<FString, int32>(TypePart, FCString::Atoi(*IndexPart));
				}
			}
		}
		else if (LastPart.IsNumeric())
		{
			// {type}_{index} 형태 (예: spine_01)
			FString TypePart = Lower.Left(LastUnderscore);
			return TPair<FString, int32>(TypePart, FCString::Atoi(*LastPart));
		}
		
		return TPair<FString, int32>(TEXT(""), -1);
	};
	
	// 본 타입별 매핑 그룹 생성
	TMap<FString, TArray<TPair<int32, FName>>> BoneTypeGroups; // Type -> [(Index, MappedBone), ...]
	for (const auto& M : IKBoneMapping)
	{
		TPair<FString, int32> TypeAndIndex = ExtractBoneTypeAndIndex(M.Key.ToString());
		if (!TypeAndIndex.Key.IsEmpty() && TypeAndIndex.Value >= 0)
		{
			BoneTypeGroups.FindOrAdd(TypeAndIndex.Key).Add(TPair<int32, FName>(TypeAndIndex.Value, M.Value));
		}
	}
	
	// 각 그룹을 인덱스로 정렬
	for (auto& Group : BoneTypeGroups)
	{
		Group.Value.Sort([](const TPair<int32, FName>& A, const TPair<int32, FName>& B)
		{
			return A.Key < B.Key;
		});
	}
	
	const TArray<FBoneChain>& Chains = Controller->GetRetargetChains();
	
	for (const FBoneChain& Chain : Chains)
	{
		FName OldStartBone = Chain.StartBone.BoneName;
		FName OldEndBone = Chain.EndBone.BoneName;
		
		// 매핑된 본 찾기 (템플릿 본 -> 실제 메쉬 본)
		// IKBoneMapping: Key = UE5 표준 본 (템플릿), Value = 실제 메쉬 본
		FName NewStartBone = NAME_None;
		FName NewEndBone = NAME_None;
		
		// Key(UE5표준본)가 템플릿의 본 이름과 일치하면 Value(실제메쉬본)로 교체
		const FName* FoundStart = IKBoneMapping.Find(OldStartBone);
		if (FoundStart)
		{
			NewStartBone = *FoundStart;
		}
		
		const FName* FoundEnd = IKBoneMapping.Find(OldEndBone);
		if (FoundEnd)
		{
			NewEndBone = *FoundEnd;
		}
		
		// 끝본이 매핑되지 않은 경우: 같은 타입의 본들 중 가장 높은 인덱스의 매핑된 본 사용
		if (NewEndBone == NAME_None && NewStartBone != NAME_None)
		{
			TPair<FString, int32> EndTypeAndIndex = ExtractBoneTypeAndIndex(OldEndBone.ToString());
			if (!EndTypeAndIndex.Key.IsEmpty())
			{
				const TArray<TPair<int32, FName>>* Group = BoneTypeGroups.Find(EndTypeAndIndex.Key);
				if (Group && Group->Num() > 0)
				{
					// 가장 높은 인덱스의 매핑된 본 사용 (정렬되어 있으므로 마지막)
					NewEndBone = (*Group).Last().Value;
					DebugLog += FString::Printf(TEXT("Chain %s: End fallback - using highest mapped bone\n"), 
						*Chain.ChainName.ToString());
				}
			}
		}
		
		// 시작본이 매핑되지 않은 경우: 같은 타입의 본들 중 가장 낮은 인덱스의 매핑된 본 사용
		if (NewStartBone == NAME_None && NewEndBone != NAME_None)
		{
			TPair<FString, int32> StartTypeAndIndex = ExtractBoneTypeAndIndex(OldStartBone.ToString());
			if (!StartTypeAndIndex.Key.IsEmpty())
			{
				const TArray<TPair<int32, FName>>* Group = BoneTypeGroups.Find(StartTypeAndIndex.Key);
				if (Group && Group->Num() > 0)
				{
					// 가장 낮은 인덱스의 매핑된 본 사용 (정렬되어 있으므로 첫번째)
					NewStartBone = (*Group)[0].Value;
					DebugLog += FString::Printf(TEXT("Chain %s: Start fallback - using lowest mapped bone\n"), 
						*Chain.ChainName.ToString());
				}
			}
		}
		
		// 체인 업데이트
		if (NewStartBone != NAME_None)
		{
			Controller->SetRetargetChainStartBone(Chain.ChainName, NewStartBone);
			DebugLog += FString::Printf(TEXT("Chain %s: Start %s -> %s\n"), 
				*Chain.ChainName.ToString(), *OldStartBone.ToString(), *NewStartBone.ToString());
		}
		else
		{
			DebugLog += FString::Printf(TEXT("Chain %s: Start %s (NOT MAPPED)\n"), 
				*Chain.ChainName.ToString(), *OldStartBone.ToString());
		}
		
		if (NewEndBone != NAME_None)
		{
			Controller->SetRetargetChainEndBone(Chain.ChainName, NewEndBone);
			DebugLog += FString::Printf(TEXT("Chain %s: End %s -> %s\n"), 
				*Chain.ChainName.ToString(), *OldEndBone.ToString(), *NewEndBone.ToString());
		}
		else
		{
			DebugLog += FString::Printf(TEXT("Chain %s: End %s (NOT MAPPED)\n"), 
				*Chain.ChainName.ToString(), *OldEndBone.ToString());
		}
	}
	
	// 9. Retarget Root (Pelvis) 설정
	FName OldRoot = Controller->GetRetargetRoot();
	FName NewRoot = NAME_None;
	
	// Key(UE5표준본)가 템플릿의 pelvis 이름과 일치하면 Value(실제메쉬본)로 교체
	const FName* FoundRoot = IKBoneMapping.Find(OldRoot);
	if (FoundRoot)
	{
		NewRoot = *FoundRoot;
	}
	
	if (NewRoot != NAME_None && NewRoot != OldRoot)
	{
		Controller->SetRetargetRoot(NewRoot);
		DebugLog += FString::Printf(TEXT("Retarget Root: %s -> %s\n"), *OldRoot.ToString(), *NewRoot.ToString());
	}
	else if (NewRoot == NAME_None)
	{
		DebugLog += FString::Printf(TEXT("Retarget Root: %s (NOT MAPPED)\n"), *OldRoot.ToString());
	}
	
	// 10. 에셋 저장
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewIKRig);
	
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewIKRig, *PackageFileName, SaveArgs);
	
	DebugLog += FString::Printf(TEXT("\n=== IK Rig Created ===\nPath: %s\nChains: %d\n"), *NewAssetPath, Chains.Num());
	
	SetIKStatus(FString::Printf(TEXT("IK Rig created: %s"), *AssetName));
	ShowDebugPopup(TEXT("IK Rig Creation"), DebugLog);
	
	UE_LOG(LogTemp, Log, TEXT("[IKRig] IK Rig created: %s"), *NewAssetPath);
}

// ============================================================================
// IK Rig 탭 전용 함수들
// ============================================================================

TSharedRef<SWidget> SControlRigToolWidget::OnGenerateIKMeshWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(*InItem));
}

void SControlRigToolWidget::OnIKMeshSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	SelectedIKMesh = NewValue;
	UpdateIKMeshThumbnail();
	
	// 자동으로 출력 이름 설정: {메쉬이름}_IK_Rig
	if (SelectedIKMesh.IsValid() && IKOutputNameBox.IsValid())
	{
		FString MeshName = *SelectedIKMesh;
		FString AutoName = MeshName + TEXT("_IK_Rig");
		IKOutputNameBox->SetText(FText::FromString(AutoName));
	}
}

FText SControlRigToolWidget::GetSelectedIKMeshName() const
{
	return SelectedIKMesh.IsValid() ? FText::FromString(*SelectedIKMesh) : LOCTEXT("IKSelectMesh", "Select...");
}

FReply SControlRigToolWidget::OnUseSelectedIKTemplateClicked()
{
	// Content Browser에서 선택된 IK Rig 사용
	TArray<FAssetData> Selected;
	GEditor->GetContentBrowserSelections(Selected);
	
	for (const FAssetData& A : Selected)
	{
		if (A.AssetClassPath.GetAssetName() == TEXT("IKRigDefinition"))
		{
			FString Path = A.GetObjectPathString();
			for (int32 i = 0; i < IKRigTemplateOptions.Num(); ++i)
			{
				if (*IKRigTemplateOptions[i] == Path)
				{
					SelectedIKRigTemplate = IKRigTemplateOptions[i];
					if (IKRigTemplateComboBox.IsValid()) IKRigTemplateComboBox->SetSelectedItem(SelectedIKRigTemplate);
					UpdateIKTemplateThumbnail();
					SetIKStatus(FString::Printf(TEXT("Template: %s"), *A.AssetName.ToString()));
					return FReply::Handled();
				}
			}
			// 목록에 없으면 추가
			IKRigTemplateOptions.Add(MakeShared<FString>(Path));
			SelectedIKRigTemplate = IKRigTemplateOptions.Last();
			if (IKRigTemplateComboBox.IsValid()) 
			{
				IKRigTemplateComboBox->RefreshOptions();
				IKRigTemplateComboBox->SetSelectedItem(SelectedIKRigTemplate);
			}
			UpdateIKTemplateThumbnail();
			SetIKStatus(FString::Printf(TEXT("Template: %s"), *A.AssetName.ToString()));
			return FReply::Handled();
		}
	}
	SetIKStatus(TEXT("Select an IK Rig in Content Browser"));
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnUseSelectedIKMeshClicked()
{
	// Content Browser에서 선택된 Skeletal Mesh 사용
	TArray<FAssetData> Selected;
	GEditor->GetContentBrowserSelections(Selected);
	
	for (const FAssetData& A : Selected)
	{
		if (A.AssetClassPath.GetAssetName() == TEXT("SkeletalMesh"))
		{
			FString Name = A.AssetName.ToString();
			for (int32 i = 0; i < MeshOptions.Num(); ++i)
			{
				if (*MeshOptions[i] == Name)
				{
					SelectedIKMesh = MeshOptions[i];
					if (IKMeshComboBox.IsValid()) IKMeshComboBox->SetSelectedItem(SelectedIKMesh);
					UpdateIKMeshThumbnail();
					
					// 자동으로 출력 이름 설정: {메쉬이름}_IK_Rig
					if (IKOutputNameBox.IsValid())
					{
						FString AutoName = Name + TEXT("_IK_Rig");
						IKOutputNameBox->SetText(FText::FromString(AutoName));
					}
					
					SetIKStatus(FString::Printf(TEXT("Mesh: %s"), *Name));
					return FReply::Handled();
				}
			}
		}
	}
	SetIKStatus(TEXT("Select a Skeletal Mesh in Content Browser"));
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnIKAIBoneMappingClicked()
{
	if (!SelectedIKMesh.IsValid())
	{
		SetIKStatus(TEXT("Error: Select a skeletal mesh first"));
		return FReply::Handled();
	}
	
	RequestIKAIBoneMapping();
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnIKApproveMappingClicked()
{
	if (IKBoneMapping.Num() == 0)
	{
		SetIKStatus(TEXT("No mapping to approve"));
		return FReply::Handled();
	}
	
	// Control Rig과 동일한 Approve 로직 사용
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("http://127.0.0.1:8000/approve"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	
	// 매핑 데이터를 JSON으로 변환
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> MappingsObject = MakeShared<FJsonObject>();
	
	for (const auto& Pair : IKBoneMapping)
	{
		MappingsObject->SetStringField(Pair.Key.ToString(), Pair.Value.ToString());
	}
	JsonObject->SetObjectField(TEXT("mappings"), MappingsObject);
	
	FString Content;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	Request->SetContentAsString(Content);
	
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
	{
		if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
		{
			SetIKStatus(TEXT("Mapping approved for AI training!"));
		}
		else
		{
			SetIKStatus(TEXT("Approve request sent (server may be offline)"));
		}
	});
	
	Request->ProcessRequest();
	SetIKStatus(TEXT("Sending approve request..."));
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnIKBrowseFolderClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString OutFolder;
		if (DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Select Output Folder"),
			FPaths::ProjectContentDir(),
			OutFolder))
		{
			// Content 폴더 기준 상대 경로로 변환
			FString GamePath;
			if (FPackageName::TryConvertFilenameToLongPackageName(OutFolder, GamePath))
			{
				if (IKOutputFolderBox.IsValid())
				{
					IKOutputFolderBox->SetText(FText::FromString(GamePath));
				}
			}
		}
	}
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnMakeTPoseClicked()
{
	CreateTPoseAnimSequence();
	return FReply::Handled();
}

void SControlRigToolWidget::CreateTPoseAnimSequence()
{
	SetIKStatus(TEXT("Creating T-Pose Animation..."));
	
	// 1. AI Bone Mapping 확인
	if (IKBoneMapping.Num() == 0)
	{
		SetIKStatus(TEXT("Error: Run AI Bone Mapping first"));
		return;
	}
	
	// 2. 선택된 스켈레탈 메쉬 로드
	if (!SelectedIKMesh.IsValid())
	{
		SetIKStatus(TEXT("Error: No skeletal mesh selected"));
		return;
	}
	
	FString MeshPath = GetSelectedIKMeshPath();
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		SetIKStatus(TEXT("Error: Failed to load skeletal mesh"));
		return;
	}
	
	USkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton)
	{
		SetIKStatus(TEXT("Error: Skeletal mesh has no skeleton"));
		return;
	}
	
	// 3. 템플릿 T-Pose 애니메이션 로드
	UE_LOG(LogTemp, Warning, TEXT("[TPose] Step 3: Loading template animation..."));
	const FString TPoseTemplatePath = TEXT("/Game/00_CooT/00_CH/00_Template/RTG/Cit_Jishuka_A00_CooT_V2_Skeleton_Sequence_t_pose");
	UAnimSequence* TemplateAnim = LoadObject<UAnimSequence>(nullptr, *TPoseTemplatePath);
	if (!TemplateAnim)
	{
		SetIKStatus(TEXT("Error: Failed to load T-Pose template animation"));
		UE_LOG(LogTemp, Error, TEXT("[TPose] Failed to load template: %s"), *TPoseTemplatePath);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[TPose] Template loaded successfully"));
	
	// 4. Jishuka 본 매핑 (UE5 표준 본 → Jishuka 템플릿 본)
	// 템플릿 T-Pose가 Jishuka 스켈레톤을 사용하므로, 이 매핑으로 템플릿에서 회전값을 찾음
	TMap<FName, FName> JishukaBoneMapping;
	// 쇄골
	JishukaBoneMapping.Add(TEXT("clavicle_l"), TEXT("Bip001-L-Clavicle"));
	JishukaBoneMapping.Add(TEXT("clavicle_r"), TEXT("Bip001-R-Clavicle"));
	// 팔
	JishukaBoneMapping.Add(TEXT("upperarm_l"), TEXT("Bip001-L-UpperArm"));
	JishukaBoneMapping.Add(TEXT("upperarm_r"), TEXT("Bip001-R-UpperArm"));
	JishukaBoneMapping.Add(TEXT("lowerarm_l"), TEXT("Bip001-L-Forearm"));
	JishukaBoneMapping.Add(TEXT("lowerarm_r"), TEXT("Bip001-R-Forearm"));
	JishukaBoneMapping.Add(TEXT("hand_l"), TEXT("Bip001-L-Hand"));
	JishukaBoneMapping.Add(TEXT("hand_r"), TEXT("Bip001-R-Hand"));
	// 손가락 - 왼손
	JishukaBoneMapping.Add(TEXT("thumb_01_l"), TEXT("Bip001-L-Finger0"));
	JishukaBoneMapping.Add(TEXT("thumb_02_l"), TEXT("Bip001-L-Finger01"));
	JishukaBoneMapping.Add(TEXT("thumb_03_l"), TEXT("Bip001-L-Finger02"));
	JishukaBoneMapping.Add(TEXT("index_01_l"), TEXT("Bip001-L-Finger1"));
	JishukaBoneMapping.Add(TEXT("index_02_l"), TEXT("Bip001-L-Finger11"));
	JishukaBoneMapping.Add(TEXT("index_03_l"), TEXT("Bip001-L-Finger12"));
	JishukaBoneMapping.Add(TEXT("middle_01_l"), TEXT("Bip001-L-Finger2"));
	JishukaBoneMapping.Add(TEXT("middle_02_l"), TEXT("Bip001-L-Finger21"));
	JishukaBoneMapping.Add(TEXT("middle_03_l"), TEXT("Bip001-L-Finger22"));
	JishukaBoneMapping.Add(TEXT("ring_01_l"), TEXT("Bip001-L-Finger3"));
	JishukaBoneMapping.Add(TEXT("ring_02_l"), TEXT("Bip001-L-Finger31"));
	JishukaBoneMapping.Add(TEXT("ring_03_l"), TEXT("Bip001-L-Finger32"));
	JishukaBoneMapping.Add(TEXT("pinky_01_l"), TEXT("Bip001-L-Finger4"));
	JishukaBoneMapping.Add(TEXT("pinky_02_l"), TEXT("Bip001-L-Finger41"));
	JishukaBoneMapping.Add(TEXT("pinky_03_l"), TEXT("Bip001-L-Finger42"));
	// 손가락 - 오른손
	JishukaBoneMapping.Add(TEXT("thumb_01_r"), TEXT("Bip001-R-Finger0"));
	JishukaBoneMapping.Add(TEXT("thumb_02_r"), TEXT("Bip001-R-Finger01"));
	JishukaBoneMapping.Add(TEXT("thumb_03_r"), TEXT("Bip001-R-Finger02"));
	JishukaBoneMapping.Add(TEXT("index_01_r"), TEXT("Bip001-R-Finger1"));
	JishukaBoneMapping.Add(TEXT("index_02_r"), TEXT("Bip001-R-Finger11"));
	JishukaBoneMapping.Add(TEXT("index_03_r"), TEXT("Bip001-R-Finger12"));
	JishukaBoneMapping.Add(TEXT("middle_01_r"), TEXT("Bip001-R-Finger2"));
	JishukaBoneMapping.Add(TEXT("middle_02_r"), TEXT("Bip001-R-Finger21"));
	JishukaBoneMapping.Add(TEXT("middle_03_r"), TEXT("Bip001-R-Finger22"));
	JishukaBoneMapping.Add(TEXT("ring_01_r"), TEXT("Bip001-R-Finger3"));
	JishukaBoneMapping.Add(TEXT("ring_02_r"), TEXT("Bip001-R-Finger31"));
	JishukaBoneMapping.Add(TEXT("ring_03_r"), TEXT("Bip001-R-Finger32"));
	JishukaBoneMapping.Add(TEXT("pinky_01_r"), TEXT("Bip001-R-Finger4"));
	JishukaBoneMapping.Add(TEXT("pinky_02_r"), TEXT("Bip001-R-Finger41"));
	JishukaBoneMapping.Add(TEXT("pinky_03_r"), TEXT("Bip001-R-Finger42"));
	// 다리
	JishukaBoneMapping.Add(TEXT("thigh_l"), TEXT("Bip001-L-Thigh"));
	JishukaBoneMapping.Add(TEXT("thigh_r"), TEXT("Bip001-R-Thigh"));
	JishukaBoneMapping.Add(TEXT("calf_l"), TEXT("Bip001-L-Calf"));
	JishukaBoneMapping.Add(TEXT("calf_r"), TEXT("Bip001-R-Calf"));
	JishukaBoneMapping.Add(TEXT("foot_l"), TEXT("Bip001-L-Foot"));
	JishukaBoneMapping.Add(TEXT("foot_r"), TEXT("Bip001-R-Foot"));
	
	// 5. 템플릿 애니메이션에서 본 회전값 추출 (IAnimationDataModel 사용)
	UE_LOG(LogTemp, Warning, TEXT("[TPose] Step 5: Getting template skeleton..."));
	USkeleton* TemplateSkeleton = TemplateAnim->GetSkeleton();
	if (!TemplateSkeleton)
	{
		SetIKStatus(TEXT("Error: Template animation has no skeleton"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[TPose] Template skeleton found"));
	
	const FReferenceSkeleton& TemplateRefSkel = TemplateSkeleton->GetReferenceSkeleton();
	TMap<FName, FQuat> TemplateRotations; // Key = UE5 표준 본, Value = 회전값
	
	// DataModel을 통해 본 트랙 데이터 접근
	UE_LOG(LogTemp, Warning, TEXT("[TPose] Getting DataModel..."));
	const IAnimationDataModel* DataModel = TemplateAnim->GetDataModel();
	if (!DataModel)
	{
		SetIKStatus(TEXT("Error: Template animation has no data model"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[TPose] DataModel found"));
	
	// Jishuka 본 매핑을 사용해서 템플릿에서 회전값 추출
	// UE5 표준 본 → Jishuka 본으로 찾아서, UE5 표준 본 이름으로 저장
	UE_LOG(LogTemp, Warning, TEXT("[TPose] Starting extraction from template. JishukaBoneMapping has %d entries"), JishukaBoneMapping.Num());
	
	for (const auto& Mapping : JishukaBoneMapping)
	{
		FName StandardBone = Mapping.Key;   // UE5 표준 본 이름
		FName JishukaBone = Mapping.Value;  // Jishuka 템플릿 본 이름
		
		// 템플릿에서 Jishuka 본 이름으로 트랙 찾기 (UE5.2+ 새 API 사용)
		if (DataModel->IsValidBoneTrackName(JishukaBone))
		{
			// 첫 번째 프레임(FrameNumber 0)의 변환값 가져오기
			FTransform BoneTransform = DataModel->GetBoneTrackTransform(JishukaBone, FFrameNumber(0));
			TemplateRotations.Add(StandardBone, BoneTransform.GetRotation());
			UE_LOG(LogTemp, Log, TEXT("[TPose] Found track: %s -> %s"), *StandardBone.ToString(), *JishukaBone.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[TPose] Track NOT found: %s (for %s)"), *JishukaBone.ToString(), *StandardBone.ToString());
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[TPose] Extracted %d bone rotations from template"), TemplateRotations.Num());
	
	// 6. 출력 경로 설정 (파일명: {MeshName}_T_Pose)
	FString OutputFolder = IKOutputFolderBox.IsValid() ? IKOutputFolderBox->GetText().ToString() : TEXT("/Game/Animations");
	FString MeshName = FPaths::GetBaseFilename(*SelectedIKMesh);
	FString AnimName = MeshName + TEXT("_T_Pose");
	FString NewAssetPath = OutputFolder / AnimName;
	
	// 7. 패키지 생성
	UPackage* Package = CreatePackage(*NewAssetPath);
	if (!Package)
	{
		SetIKStatus(TEXT("Error: Failed to create package"));
		return;
	}
	
	// 8. UAnimSequence 생성
	UAnimSequence* AnimSequence = NewObject<UAnimSequence>(Package, *AnimName, RF_Public | RF_Standalone);
	if (!AnimSequence)
	{
		SetIKStatus(TEXT("Error: Failed to create AnimSequence"));
		return;
	}
	
	// 9. 스켈레톤 설정
	AnimSequence->SetSkeleton(Skeleton);
	
	// 10. IAnimationDataController 획득
	IAnimationDataController& Controller = AnimSequence->GetController();
	
	// 11. 모델 초기화
	Controller.InitializeModel();
	Controller.OpenBracket(LOCTEXT("CreateTPose", "Create T-Pose Animation"), false);
	
	// 12. 프레임 레이트 및 길이 설정 (2프레임 = 0~1프레임, 30fps)
	Controller.SetFrameRate(FFrameRate(30, 1), false);
	Controller.SetNumberOfFrames(FFrameNumber(2), false);
	
	// 13. 타겟 메쉬의 레퍼런스 포즈
	const FReferenceSkeleton& TargetRefSkel = Skeleton->GetReferenceSkeleton();
	const TArray<FTransform>& TargetRefPose = TargetRefSkel.GetRefBonePose();
	const int32 NumBones = TargetRefSkel.GetNum();
	
	// 14. T-Pose 트랜스폼 생성 (기본은 레퍼런스 포즈, 지정 본만 템플릿 회전 적용)
	TArray<FTransform> TPoseTransforms = TargetRefPose;
	
	// IKBoneMapping: Key = UE5 표준 본 (템플릿), Value = 실제 메쉬 본
	int32 AppliedCount = 0;
	for (const auto& Mapping : IKBoneMapping)
	{
		FName StandardBone = Mapping.Key;
		FName TargetBone = Mapping.Value;
		
		// 이 본이 T-Pose 적용 대상인지 확인 (JishukaBoneMapping에 있는 본만)
		if (!JishukaBoneMapping.Contains(StandardBone))
		{
			continue;
		}
		
		// 템플릿에서 회전값 가져오기
		const FQuat* TemplateRot = TemplateRotations.Find(StandardBone);
		if (!TemplateRot)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TPose] Standard bone %s not found in template"), *StandardBone.ToString());
			continue;
		}
		
		// 타겟 메쉬에서 본 인덱스 찾기
		int32 TargetBoneIdx = TargetRefSkel.FindBoneIndex(TargetBone);
		if (TargetBoneIdx == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TPose] Target bone %s not found in mesh"), *TargetBone.ToString());
			continue;
		}
		
		// 회전값 적용 (위치와 스케일은 원래 값 유지)
		TPoseTransforms[TargetBoneIdx].SetRotation(*TemplateRot);
		AppliedCount++;
		
		UE_LOG(LogTemp, Log, TEXT("[TPose] Applied: %s -> %s"), *StandardBone.ToString(), *TargetBone.ToString());
	}
	
	UE_LOG(LogTemp, Log, TEXT("[TPose] Applied T-Pose rotations to %d bones"), AppliedCount);
	
	// 15. 본 트랙 추가 및 키 설정 (2프레임: 0, 1)
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		FName BoneName = TargetRefSkel.GetBoneName(BoneIndex);
		const FTransform& BoneTransform = TPoseTransforms[BoneIndex];
		
		// 본 트랙 추가
		Controller.AddBoneCurve(BoneName, false);
		
		// 키 설정 (2프레임 - 0번과 1번 프레임에 동일한 값)
		TArray<FVector> Positions;
		TArray<FQuat> Rotations;
		TArray<FVector> Scales;
		
		// 프레임 0
		Positions.Add(BoneTransform.GetLocation());
		Rotations.Add(BoneTransform.GetRotation());
		Scales.Add(BoneTransform.GetScale3D());
		
		// 프레임 1 (동일한 값)
		Positions.Add(BoneTransform.GetLocation());
		Rotations.Add(BoneTransform.GetRotation());
		Scales.Add(BoneTransform.GetScale3D());
		
		Controller.SetBoneTrackKeys(BoneName, Positions, Rotations, Scales, false);
	}
	
	Controller.CloseBracket(false);
	
	// 16. 저장
	AnimSequence->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(AnimSequence);
	
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	FString PackageFileName = FPackageName::LongPackageNameToFilename(NewAssetPath, FPackageName::GetAssetPackageExtension());
	bool bSaved = UPackage::SavePackage(Package, AnimSequence, *PackageFileName, SaveArgs);
	
	if (bSaved)
	{
		SetIKStatus(FString::Printf(TEXT("T-Pose created: %s (%d bones adjusted)"), *NewAssetPath, AppliedCount));
		UE_LOG(LogTemp, Log, TEXT("[TPose] Created: %s with %d T-Pose bones"), *NewAssetPath, AppliedCount);
	}
	else
	{
		SetIKStatus(TEXT("Error: Failed to save T-Pose animation"));
	}
}

void SControlRigToolWidget::RequestIKAIBoneMapping()
{
	SetIKStatus(TEXT("Requesting AI Bone Mapping..."));
	
	// 선택된 메쉬 확인
	if (!SelectedIKMesh.IsValid())
	{
		SetIKStatus(TEXT("Error: No skeletal mesh selected"));
		return;
	}
	
	// 선택된 메쉬 로드
	FString MeshPath = GetSelectedIKMeshPath();
	UE_LOG(LogTemp, Log, TEXT("[IKRig] Selected mesh: %s, Path: %s"), **SelectedIKMesh, *MeshPath);
	
	if (MeshPath.IsEmpty())
	{
		SetIKStatus(TEXT("Error: Mesh path not found"));
		return;
	}
	
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		SetIKStatus(FString::Printf(TEXT("Error: Failed to load mesh: %s"), *MeshPath));
		return;
	}
	
	// Control Rig 탭과 동일한 JSON 형식으로 본 정보 생성
	const FReferenceSkeleton& Skel = Mesh->GetRefSkeleton();
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Bones;
	
	for (int32 i = 0; i < Skel.GetNum(); i++)
	{
		const FMeshBoneInfo& Info = Skel.GetRefBoneInfo()[i];
		TSharedPtr<FJsonObject> Bone = MakeShared<FJsonObject>();
		Bone->SetStringField("name", Info.Name.ToString());
		if (Info.ParentIndex >= 0)
			Bone->SetStringField("parent", Skel.GetBoneName(Info.ParentIndex).ToString());
		TArray<TSharedPtr<FJsonValue>> Children;
		for (int32 j = 0; j < Skel.GetNum(); j++)
			if (Skel.GetRefBoneInfo()[j].ParentIndex == i)
				Children.Add(MakeShared<FJsonValueString>(Skel.GetBoneName(j).ToString()));
		Bone->SetArrayField("children", Children);
		Bones.Add(MakeShared<FJsonValueObject>(Bone));
	}
	Root->SetArrayField("bones", Bones);
	Root->SetBoolField("use_ai", true);
	
	FString Body;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root.ToSharedRef(), W);
	
	// Control Rig 탭과 동일한 URL 사용
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(TEXT("http://localhost:8000/predict"));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(Body);
	Req->SetTimeout(120.0f);
	
	Req->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Res, bool Ok)
	{
		if (!Ok || !Res.IsValid())
		{
			SetIKStatus(TEXT("Error: AI server connection failed"));
			return;
		}
		
		// 응답 파싱
		TSharedPtr<FJsonObject> J;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Res->GetContentAsString());
		if (!FJsonSerializer::Deserialize(R, J))
		{
			SetIKStatus(TEXT("Error: Failed to parse response"));
			return;
		}
		
		// 매핑 결과 저장 (Control Rig 탭과 동일한 형식)
		IKBoneMapping.Empty();
		const TSharedPtr<FJsonObject>* Map;
		if (J->TryGetObjectField(TEXT("mapping"), Map))
		{
			for (const auto& Pair : (*Map)->Values)
			{
				FString Value;
				if (Pair.Value->TryGetString(Value))
				{
					IKBoneMapping.Add(FName(*Pair.Key), FName(*Value));
				}
			}
		}
		
		DisplayIKMappingResults();
		SetIKStatus(FString::Printf(TEXT("Mapped %d bones"), IKBoneMapping.Num()));
	});
	
	Req->ProcessRequest();
}

void SControlRigToolWidget::DisplayIKMappingResults()
{
	if (!IKMappingResultBox.IsValid()) return;
	
	IKMappingResultBox->ClearChildren();
	
	for (const auto& Pair : IKBoneMapping)
	{
		IKMappingResultBox->AddSlot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.45f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Pair.Key.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FLinearColor(0.7f, 0.8f, 0.9f, 1.0f))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("→")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f, 1.0f))
			]
			+ SHorizontalBox::Slot().FillWidth(0.45f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Pair.Value.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FLinearColor(0.3f, 0.8f, 0.4f, 1.0f))
			]
		];
	}
}

void SControlRigToolWidget::UpdateIKTemplateThumbnail()
{
	if (!IKTemplateThumbnailBox.IsValid() || !ThumbnailPool.IsValid()) return;
	
	FString Path = GetSelectedIKTemplatePath();
	if (Path.IsEmpty())
	{
		IKTemplateThumbnailBox->SetContent(
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.06f, 1.0f))
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("ClassIcon.IKRigDefinition"))
					.ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.35f, 1.0f))
					.DesiredSizeOverride(FVector2D(32, 32))
				]
			]
		);
		return;
	}
	
	IKTemplateThumbnail = MakeShared<FAssetThumbnail>(FSoftObjectPath(Path).TryLoad(), ThumbnailSize, ThumbnailSize, ThumbnailPool);
	IKTemplateThumbnailBox->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.1f, 1.0f))
		.Padding(2)
		[
			IKTemplateThumbnail->MakeThumbnailWidget()
		]
	);
}

void SControlRigToolWidget::UpdateIKMeshThumbnail()
{
	if (!IKMeshThumbnailBox.IsValid() || !ThumbnailPool.IsValid()) return;
	
	FString Path = GetSelectedIKMeshPath();
	if (Path.IsEmpty())
	{
		IKMeshThumbnailBox->SetContent(
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.06f, 1.0f))
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("ClassIcon.SkeletalMesh"))
					.ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.35f, 1.0f))
					.DesiredSizeOverride(FVector2D(32, 32))
				]
			]
		);
		return;
	}
	
	IKMeshThumbnail = MakeShared<FAssetThumbnail>(FSoftObjectPath(Path).TryLoad(), ThumbnailSize, ThumbnailSize, ThumbnailPool);
	IKMeshThumbnailBox->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.1f, 1.0f))
		.Padding(2)
		[
			IKMeshThumbnail->MakeThumbnailWidget()
		]
	);
}

FString SControlRigToolWidget::GetSelectedIKTemplatePath() const
{
	if (!SelectedIKRigTemplate.IsValid()) return FString();
	return *SelectedIKRigTemplate;
}

FString SControlRigToolWidget::GetSelectedIKMeshPath() const
{
	if (!SelectedIKMesh.IsValid()) return FString();
	
	for (const FAssetInfo& Info : SkeletalMeshes)
	{
		if (Info.Name == *SelectedIKMesh)
		{
			return Info.Path;
		}
	}
	return FString();
}

void SControlRigToolWidget::SetIKStatus(const FString& Message)
{
	if (IKStatusText.IsValid())
	{
		IKStatusText->SetText(FText::FromString(Message));
	}
	UE_LOG(LogTemp, Log, TEXT("[IKRig] %s"), *Message);
}

// ============================================================================
// IK Retargeter 관련 함수들
// ============================================================================

void SControlRigToolWidget::LoadRetargeterIKRigs()
{
	RetargeterSourceOptions.Empty();
	RetargeterTargetOptions.Empty();
	
	// Asset Registry에서 IK Rig 에셋 검색
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	
	FARFilter Filter;
	Filter.ClassPaths.Add(UIKRigDefinition::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	
	TArray<FAssetData> AssetList;
	AssetRegistry.Get().GetAssets(Filter, AssetList);
	
	for (const FAssetData& Asset : AssetList)
	{
		FString AssetPath = Asset.GetObjectPathString();
		RetargeterSourceOptions.Add(MakeShared<FString>(AssetPath));
		RetargeterTargetOptions.Add(MakeShared<FString>(AssetPath));
	}
	
	// ComboBox 갱신
	if (RetargeterSourceComboBox.IsValid())
	{
		RetargeterSourceComboBox->RefreshOptions();
	}
	if (RetargeterTargetComboBox.IsValid())
	{
		RetargeterTargetComboBox->RefreshOptions();
	}
	
	UE_LOG(LogTemp, Log, TEXT("[IKRetargeter] Loaded %d IK Rig options"), RetargeterSourceOptions.Num());
}

TSharedRef<SWidget> SControlRigToolWidget::OnGenerateRetargeterSourceWidget(TSharedPtr<FString> InItem)
{
	FString DisplayName = InItem.IsValid() ? FPaths::GetBaseFilename(*InItem) : TEXT("");
	return SNew(STextBlock)
		.Text(FText::FromString(DisplayName))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10));
}

TSharedRef<SWidget> SControlRigToolWidget::OnGenerateRetargeterTargetWidget(TSharedPtr<FString> InItem)
{
	FString DisplayName = InItem.IsValid() ? FPaths::GetBaseFilename(*InItem) : TEXT("");
	return SNew(STextBlock)
		.Text(FText::FromString(DisplayName))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10));
}

void SControlRigToolWidget::OnRetargeterSourceSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	SelectedRetargeterSource = NewValue;
	UpdateRetargeterSourceThumbnail();
	
	// 자동 이름 생성
	if (SelectedRetargeterSource.IsValid() && SelectedRetargeterTarget.IsValid() && RetargeterOutputNameBox.IsValid())
	{
		FString SourceName = FPaths::GetBaseFilename(*SelectedRetargeterSource);
		FString TargetName = FPaths::GetBaseFilename(*SelectedRetargeterTarget);
		RetargeterOutputNameBox->SetText(FText::FromString(FString::Printf(TEXT("RTG_%s_to_%s"), *SourceName, *TargetName)));
	}
}

void SControlRigToolWidget::OnRetargeterTargetSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	SelectedRetargeterTarget = NewValue;
	UpdateRetargeterTargetThumbnail();
	
	// 자동 이름 생성
	if (SelectedRetargeterSource.IsValid() && SelectedRetargeterTarget.IsValid() && RetargeterOutputNameBox.IsValid())
	{
		FString SourceName = FPaths::GetBaseFilename(*SelectedRetargeterSource);
		FString TargetName = FPaths::GetBaseFilename(*SelectedRetargeterTarget);
		RetargeterOutputNameBox->SetText(FText::FromString(FString::Printf(TEXT("RTG_%s_to_%s"), *SourceName, *TargetName)));
	}
}

FText SControlRigToolWidget::GetSelectedRetargeterSourceName() const
{
	if (!SelectedRetargeterSource.IsValid()) return FText::FromString(TEXT("Select Source IK Rig..."));
	return FText::FromString(FPaths::GetBaseFilename(*SelectedRetargeterSource));
}

FText SControlRigToolWidget::GetSelectedRetargeterTargetName() const
{
	if (!SelectedRetargeterTarget.IsValid()) return FText::FromString(TEXT("Select Target IK Rig..."));
	return FText::FromString(FPaths::GetBaseFilename(*SelectedRetargeterTarget));
}

FReply SControlRigToolWidget::OnUseSelectedRetargeterSourceClicked()
{
	// Content Browser에서 선택된 IK Rig 가져오기
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == UIKRigDefinition::StaticClass()->GetClassPathName())
		{
			FString AssetPath = Asset.GetObjectPathString();
			
			// 옵션에 없으면 추가
			bool bFound = false;
			for (const TSharedPtr<FString>& Option : RetargeterSourceOptions)
			{
				if (*Option == AssetPath)
				{
					SelectedRetargeterSource = Option;
					bFound = true;
					break;
				}
			}
			
			if (!bFound)
			{
				TSharedPtr<FString> NewOption = MakeShared<FString>(AssetPath);
				RetargeterSourceOptions.Add(NewOption);
				SelectedRetargeterSource = NewOption;
				if (RetargeterSourceComboBox.IsValid())
				{
					RetargeterSourceComboBox->RefreshOptions();
				}
			}
			
			if (RetargeterSourceComboBox.IsValid())
			{
				RetargeterSourceComboBox->SetSelectedItem(SelectedRetargeterSource);
			}
			UpdateRetargeterSourceThumbnail();
			
			SetIKStatus(FString::Printf(TEXT("Source IK Rig: %s"), *FPaths::GetBaseFilename(AssetPath)));
			break;
		}
	}
	
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnUseSelectedRetargeterTargetClicked()
{
	// Content Browser에서 선택된 IK Rig 가져오기
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == UIKRigDefinition::StaticClass()->GetClassPathName())
		{
			FString AssetPath = Asset.GetObjectPathString();
			
			// 옵션에 없으면 추가
			bool bFound = false;
			for (const TSharedPtr<FString>& Option : RetargeterTargetOptions)
			{
				if (*Option == AssetPath)
				{
					SelectedRetargeterTarget = Option;
					bFound = true;
					break;
				}
			}
			
			if (!bFound)
			{
				TSharedPtr<FString> NewOption = MakeShared<FString>(AssetPath);
				RetargeterTargetOptions.Add(NewOption);
				SelectedRetargeterTarget = NewOption;
				if (RetargeterTargetComboBox.IsValid())
				{
					RetargeterTargetComboBox->RefreshOptions();
				}
			}
			
			if (RetargeterTargetComboBox.IsValid())
			{
				RetargeterTargetComboBox->SetSelectedItem(SelectedRetargeterTarget);
			}
			UpdateRetargeterTargetThumbnail();
			
			SetIKStatus(FString::Printf(TEXT("Target IK Rig: %s"), *FPaths::GetBaseFilename(AssetPath)));
			break;
		}
	}
	
	return FReply::Handled();
}

void SControlRigToolWidget::UpdateRetargeterSourceThumbnail()
{
	if (!RetargeterSourceThumbnailBox.IsValid()) return;
	
	FString Path = SelectedRetargeterSource.IsValid() ? *SelectedRetargeterSource : FString();
	
	if (Path.IsEmpty())
	{
		RetargeterSourceThumbnailBox->SetContent(
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("ClassIcon.IKRigDefinition"))
				.ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.35f, 1.0f))
				.DesiredSizeOverride(FVector2D(32, 32))
			]
		);
		return;
	}
	
	RetargeterSourceThumbnail = MakeShared<FAssetThumbnail>(FSoftObjectPath(Path).TryLoad(), 48, 48, ThumbnailPool);
	RetargeterSourceThumbnailBox->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.1f, 1.0f))
		.Padding(2)
		[
			RetargeterSourceThumbnail->MakeThumbnailWidget()
		]
	);
}

void SControlRigToolWidget::UpdateRetargeterTargetThumbnail()
{
	if (!RetargeterTargetThumbnailBox.IsValid()) return;
	
	FString Path = SelectedRetargeterTarget.IsValid() ? *SelectedRetargeterTarget : FString();
	
	if (Path.IsEmpty())
	{
		RetargeterTargetThumbnailBox->SetContent(
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("ClassIcon.IKRigDefinition"))
				.ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.35f, 1.0f))
				.DesiredSizeOverride(FVector2D(32, 32))
			]
		);
		return;
	}
	
	RetargeterTargetThumbnail = MakeShared<FAssetThumbnail>(FSoftObjectPath(Path).TryLoad(), 48, 48, ThumbnailPool);
	RetargeterTargetThumbnailBox->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.1f, 1.0f))
		.Padding(2)
		[
			RetargeterTargetThumbnail->MakeThumbnailWidget()
		]
	);
}

FReply SControlRigToolWidget::OnCreateIKRetargeterClicked()
{
	if (!SelectedRetargeterSource.IsValid() || !SelectedRetargeterTarget.IsValid())
	{
		SetIKStatus(TEXT("Error: Select both Source and Target IK Rigs"));
		return FReply::Handled();
	}
	
	CreateIKRetargeter();
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnRetargeterBrowseFolderClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString OutFolder;
		FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
		
		bool bOpened = DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Select Retargeter Output Folder"),
			ContentDir,
			OutFolder
		);
		
		if (bOpened && !OutFolder.IsEmpty())
		{
			// 절대 경로를 /Game/ 형식으로 변환
			FString GamePath;
			if (FPackageName::TryConvertFilenameToLongPackageName(OutFolder, GamePath))
			{
				if (RetargeterOutputFolderBox.IsValid())
				{
					RetargeterOutputFolderBox->SetText(FText::FromString(GamePath));
				}
			}
			else if (OutFolder.StartsWith(ContentDir))
			{
				// Content 폴더 내의 경로인 경우 직접 변환
				FString RelPath = OutFolder.RightChop(ContentDir.Len());
				GamePath = TEXT("/Game") / RelPath;
				if (RetargeterOutputFolderBox.IsValid())
				{
					RetargeterOutputFolderBox->SetText(FText::FromString(GamePath));
				}
			}
		}
	}
	return FReply::Handled();
}

void SControlRigToolWidget::CreateIKRetargeter()
{
	SetIKStatus(TEXT("Creating IK Retargeter..."));
	
	// 1. 소스 및 타겟 IK Rig 로드
	UIKRigDefinition* SourceIKRig = LoadObject<UIKRigDefinition>(nullptr, **SelectedRetargeterSource);
	UIKRigDefinition* TargetIKRig = LoadObject<UIKRigDefinition>(nullptr, **SelectedRetargeterTarget);
	
	if (!SourceIKRig)
	{
		SetIKStatus(TEXT("Error: Failed to load Source IK Rig"));
		return;
	}
	
	if (!TargetIKRig)
	{
		SetIKStatus(TEXT("Error: Failed to load Target IK Rig"));
		return;
	}
	
	// 2. 출력 경로 생성
	FString OutputFolder = RetargeterOutputFolderBox.IsValid() ? RetargeterOutputFolderBox->GetText().ToString() : RetargeterDefaultOutputFolder;
	FString OutputName = RetargeterOutputNameBox.IsValid() ? RetargeterOutputNameBox->GetText().ToString() : TEXT("NewIKRetargeter");
	FString NewAssetPath = OutputFolder / OutputName;
	
	// 3. 패키지 생성
	UPackage* Package = CreatePackage(*NewAssetPath);
	if (!Package)
	{
		SetIKStatus(TEXT("Error: Failed to create package"));
		return;
	}
	
	// 4. IK Retargeter 에셋 생성
	FString AssetName = FPaths::GetBaseFilename(NewAssetPath);
	UIKRetargeter* NewRetargeter = NewObject<UIKRetargeter>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!NewRetargeter)
	{
		SetIKStatus(TEXT("Error: Failed to create IK Retargeter"));
		return;
	}
	
	// 5. IK Retargeter Controller로 설정
	UIKRetargeterController* Controller = UIKRetargeterController::GetController(NewRetargeter);
	if (!Controller)
	{
		SetIKStatus(TEXT("Error: Failed to get IK Retargeter controller"));
		return;
	}
	
	// 6. 소스/타겟 IK Rig 설정
	Controller->SetIKRig(ERetargetSourceOrTarget::Source, SourceIKRig);
	Controller->SetIKRig(ERetargetSourceOrTarget::Target, TargetIKRig);
	
	// 7. 기본 Op 추가 및 체인 자동 매핑
	Controller->AddDefaultOps();
	Controller->AutoMapChains(EAutoMapChainType::Fuzzy, true);
	
	// 8. 에셋 저장
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewRetargeter);
	
	FString PackageFileName = FPackageName::LongPackageNameToFilename(NewAssetPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewRetargeter, *PackageFileName, SaveArgs);
	
	SetIKStatus(FString::Printf(TEXT("IK Retargeter created: %s"), *AssetName));
	UE_LOG(LogTemp, Log, TEXT("[IKRetargeter] Created: %s"), *NewAssetPath);
}

// ============================================================================
// Kawaii Physics 탭 함수들
// ============================================================================

TSharedRef<SWidget> SControlRigToolWidget::OnGenerateKawaiiMeshWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(InItem.IsValid() ? FPaths::GetBaseFilename(*InItem) : TEXT("")))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
}

void SControlRigToolWidget::OnKawaiiMeshSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	SelectedKawaiiMesh = NewValue;
	UpdateKawaiiMeshThumbnail();
	
	if (NewValue.IsValid())
	{
		// 자동 출력 이름 설정
		FString MeshName = FPaths::GetBaseFilename(*NewValue);
		FString AutoName = FString::Printf(TEXT("ABP_%s_Kawaii"), *MeshName);
		if (KawaiiOutputNameBox.IsValid())
		{
			KawaiiOutputNameBox->SetText(FText::FromString(AutoName));
		}
		
		// 본 트리 빌드
		BuildKawaiiBoneDisplayList();
		
		SetKawaiiStatus(FString::Printf(TEXT("Loaded: %s (%d bones)"), *MeshName, KawaiiBoneDisplayList.Num()));
	}
}

FText SControlRigToolWidget::GetSelectedKawaiiMeshName() const
{
	if (!SelectedKawaiiMesh.IsValid()) return FText::FromString(TEXT("Select Skeletal Mesh..."));
	return FText::FromString(FPaths::GetBaseFilename(*SelectedKawaiiMesh));
}

FReply SControlRigToolWidget::OnUseSelectedKawaiiMeshClicked()
{
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
		{
			FString AssetPath = Asset.GetObjectPathString();
			
			bool bFound = false;
			for (const TSharedPtr<FString>& Option : MeshOptions)
			{
				if (*Option == AssetPath)
				{
					SelectedKawaiiMesh = Option;
					bFound = true;
					break;
				}
			}
			
			if (!bFound)
			{
				TSharedPtr<FString> NewOption = MakeShared<FString>(AssetPath);
				MeshOptions.Add(NewOption);
				SelectedKawaiiMesh = NewOption;
				if (KawaiiMeshComboBox.IsValid())
				{
					KawaiiMeshComboBox->RefreshOptions();
				}
			}
			
			if (KawaiiMeshComboBox.IsValid())
			{
				KawaiiMeshComboBox->SetSelectedItem(SelectedKawaiiMesh);
			}
			
			OnKawaiiMeshSelectionChanged(SelectedKawaiiMesh, ESelectInfo::Direct);
			break;
		}
	}
	
	return FReply::Handled();
}

void SControlRigToolWidget::UpdateKawaiiMeshThumbnail()
{
	if (!KawaiiMeshThumbnailBox.IsValid()) return;
	
	if (!SelectedKawaiiMesh.IsValid() || SelectedKawaiiMesh->IsEmpty())
	{
		KawaiiMeshThumbnailBox->SetContent(
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.06f, 1.0f))
		);
		return;
	}
	
	FString AssetPath = *SelectedKawaiiMesh;
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	
	if (AssetData.IsValid())
	{
		KawaiiMeshThumbnail = MakeShared<FAssetThumbnail>(AssetData, ThumbnailSize, ThumbnailSize, ThumbnailPool.ToSharedRef());
		KawaiiMeshThumbnailBox->SetContent(KawaiiMeshThumbnail->MakeThumbnailWidget());
	}
}

FString SControlRigToolWidget::GetSelectedKawaiiMeshPath() const
{
	return SelectedKawaiiMesh.IsValid() ? *SelectedKawaiiMesh : TEXT("");
}

void SControlRigToolWidget::SetKawaiiStatus(const FString& Message)
{
	if (KawaiiStatusText.IsValid())
	{
		KawaiiStatusText->SetText(FText::FromString(Message));
	}
	UE_LOG(LogTemp, Log, TEXT("[KawaiiPhysics] %s"), *Message);
}

FReply SControlRigToolWidget::OnKawaiiBrowseFolderClicked()
{
	FString ContentDir = FPaths::ProjectContentDir();
	FString OutFolder;
	
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		void* ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow().IsValid() 
			? FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle() 
			: nullptr;
		
		bool bOpened = DesktopPlatform->OpenDirectoryDialog(ParentWindow, TEXT("Select Output Folder"), ContentDir, OutFolder);
		if (bOpened && !OutFolder.IsEmpty())
		{
			FString GamePath;
			if (FPackageName::TryConvertFilenameToLongPackageName(OutFolder, GamePath))
			{
				if (KawaiiOutputFolderBox.IsValid())
				{
					KawaiiOutputFolderBox->SetText(FText::FromString(GamePath));
				}
			}
		}
	}
	return FReply::Handled();
}

void SControlRigToolWidget::BuildKawaiiBoneDisplayList()
{
	KawaiiBoneDisplayList.Empty();
	
	FString MeshPath = GetSelectedKawaiiMeshPath();
	if (MeshPath.IsEmpty()) return;
	
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh) return;
	
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		FKawaiiBoneDisplayInfo Info;
		Info.BoneName = RefSkel.GetBoneName(i);
		Info.BoneIndex = i;
		Info.ParentIndex = RefSkel.GetParentIndex(i);
		
		// 깊이 계산
		int32 Depth = 0;
		int32 ParentIdx = Info.ParentIndex;
		while (ParentIdx != INDEX_NONE)
		{
			Depth++;
			ParentIdx = RefSkel.GetParentIndex(ParentIdx);
		}
		Info.Depth = Depth;
		
		// 웨이트 체크
		Info.bHasSkinWeight = HasSkinWeight(Mesh, Info.BoneName);
		
		// Control Rig 탭에서 Secondary로 선택됐는지 확인
		Info.bIsSecondary = false;
		for (const FBoneDisplayInfo& BDI : BoneDisplayList)
		{
			if (BDI.BoneName == Info.BoneName && BDI.Classification == EBoneClassification::Secondary)
			{
				Info.bIsSecondary = true;
				break;
			}
		}
		
		Info.TagIndex = INDEX_NONE;
		Info.bExpanded = true;  // 기본 펼쳐진 상태
		Info.bHasChildren = false;  // 아래서 계산
		
		KawaiiBoneDisplayList.Add(Info);
	}
	
	// 자식 본 여부 계산
	for (int32 i = 0; i < KawaiiBoneDisplayList.Num(); ++i)
	{
		int32 BoneIdx = KawaiiBoneDisplayList[i].BoneIndex;
		// 다른 본이 이 본을 부모로 가지는지 확인
		for (int32 j = 0; j < KawaiiBoneDisplayList.Num(); ++j)
		{
			if (KawaiiBoneDisplayList[j].ParentIndex == BoneIdx)
			{
				KawaiiBoneDisplayList[i].bHasChildren = true;
				break;
			}
		}
	}
	
	UpdateKawaiiBoneTreeUI();
}

void SControlRigToolWidget::UpdateKawaiiBoneTreeUI()
{
	if (!KawaiiBoneTreeBox.IsValid()) return;
	
	KawaiiBoneTreeBox->ClearChildren();
	
	if (KawaiiBoneDisplayList.Num() == 0)
	{
		KawaiiBoneTreeBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoBones", "No bones to display"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.45f, 1.0f))
		];
		return;
	}
	
	for (int32 i = 0; i < KawaiiBoneDisplayList.Num(); ++i)
	{
		// 부모가 접혀있으면 표시하지 않음
		bool bVisible = true;
		int32 ParentIdx = KawaiiBoneDisplayList[i].ParentIndex;
		while (ParentIdx != INDEX_NONE && bVisible)
		{
			// ParentIndex(본 인덱스)를 배열에서 찾기
			for (int32 j = 0; j < KawaiiBoneDisplayList.Num(); ++j)
			{
				if (KawaiiBoneDisplayList[j].BoneIndex == ParentIdx)
				{
					if (!KawaiiBoneDisplayList[j].bExpanded)
					{
						bVisible = false;
					}
					ParentIdx = KawaiiBoneDisplayList[j].ParentIndex;
					break;
				}
			}
		}
		
		if (!bVisible) continue;
		
		KawaiiBoneTreeBox->AddSlot()
		.AutoHeight()
		[
			CreateKawaiiBoneRow(i)
		];
	}
}

TSharedRef<SWidget> SControlRigToolWidget::CreateKawaiiBoneRow(int32 Index)
{
	if (Index < 0 || Index >= KawaiiBoneDisplayList.Num())
	{
		return SNew(SBox);
	}
	
	FKawaiiBoneDisplayInfo& Info = KawaiiBoneDisplayList[Index];
	
	// 들여쓰기 (접기 버튼 공간 포함)
	float IndentPadding = Info.Depth * 12.0f;  // 좀 더 조밀하게
	bool bHasChildren = Info.bHasChildren;
	
	// SButton으로 전체 행을 감싸서 호버 효과 구현
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")  // 호버 시 배경 변경
		.ContentPadding(FMargin(2, 1))  // 줄 간격 축소
		.OnClicked_Lambda([this, Index]() -> FReply 
		{ 
			// 클릭 시 태그 적용 (선택된 태그가 있으면)
			if (SelectedKawaiiTagIndex != INDEX_NONE)
			{
				return OnApplySelectedTagClicked(Index);
			}
			return FReply::Handled(); 
		})
		[
			SNew(SHorizontalBox)
			// 들여쓰기
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(IndentPadding)
			]
			// 접기/펼치기 버튼 (자식 있을 때만)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(16.0f)
				.HeightOverride(16.0f)
				[
					bHasChildren ?
					static_cast<TSharedRef<SWidget>>(
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "NoBorder")
						.ContentPadding(0)
						.OnClicked_Lambda([this, Index]() -> FReply
						{
							if (Index >= 0 && Index < KawaiiBoneDisplayList.Num())
							{
								KawaiiBoneDisplayList[Index].bExpanded = !KawaiiBoneDisplayList[Index].bExpanded;
								UpdateKawaiiBoneTreeUI();
							}
							return FReply::Handled();
						})
						[
							SNew(SImage)
							.Image_Lambda([this, Index]() -> const FSlateBrush*
							{
								if (Index >= 0 && Index < KawaiiBoneDisplayList.Num() && KawaiiBoneDisplayList[Index].bExpanded)
									return FAppStyle::GetBrush("TreeArrow_Expanded");
								return FAppStyle::GetBrush("TreeArrow_Collapsed");
							})
							.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f))
						]
					)
					:
					static_cast<TSharedRef<SWidget>>(
						SNew(SSpacer)
					)
				]
			]
			// 본 이름
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(2, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Info.BoneName))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))  // 약간 작은 글씨
				.ColorAndOpacity_Lambda([this, Index]() -> FSlateColor
				{
					if (Index < 0 || Index >= KawaiiBoneDisplayList.Num()) 
						return FLinearColor(0.4f, 0.4f, 0.45f, 1.0f);
					
					const FKawaiiBoneDisplayInfo& BoneInfo = KawaiiBoneDisplayList[Index];
					
					if (!BoneInfo.bHasSkinWeight)
						return FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);  // 빨간색
					else if (BoneInfo.TagIndex != INDEX_NONE && BoneInfo.TagIndex < KawaiiTags.Num())
						return KawaiiTags[BoneInfo.TagIndex].Color;  // 태그 색상
					else if (BoneInfo.bIsSecondary)
						return FLinearColor::White;  // 흰색
					else
						return FLinearColor(0.5f, 0.5f, 0.55f, 1.0f);  // 회색
				})
			]
			// 태그 표시 (현재 적용된 태그)
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0).VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(60.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.BorderBackgroundColor_Lambda([this, Index]() -> FLinearColor
					{
						if (Index >= 0 && Index < KawaiiBoneDisplayList.Num())
						{
							int32 TagIdx = KawaiiBoneDisplayList[Index].TagIndex;
							if (TagIdx != INDEX_NONE && TagIdx < KawaiiTags.Num())
							{
								return FLinearColor(KawaiiTags[TagIdx].Color.R * 0.3f, 
									KawaiiTags[TagIdx].Color.G * 0.3f, 
									KawaiiTags[TagIdx].Color.B * 0.3f, 0.9f);
							}
						}
						return FLinearColor(0.08f, 0.08f, 0.1f, 1.0f);
					})
					.Padding(FMargin(4, 2))
					[
						SNew(STextBlock)
						.Text_Lambda([this, Index]() -> FText
						{
							if (Index < 0 || Index >= KawaiiBoneDisplayList.Num()) 
								return FText::FromString(TEXT("-"));
							int32 TagIdx = KawaiiBoneDisplayList[Index].TagIndex;
							if (TagIdx == INDEX_NONE || TagIdx >= KawaiiTags.Num()) 
								return FText::FromString(TEXT("-"));
							return FText::FromString(KawaiiTags[TagIdx].Name);
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
						.Justification(ETextJustify::Center)
					]
				]
			]
			// 태그 해제 버튼 (X) - 태그가 적용된 경우에만 표시
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(2))
				.Visibility_Lambda([this, Index]() 
				{ 
					if (Index >= 0 && Index < KawaiiBoneDisplayList.Num())
					{
						return KawaiiBoneDisplayList[Index].TagIndex != INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed;
					}
					return EVisibility::Collapsed;
				})
				.ToolTipText(FText::FromString(TEXT("태그 해제 (우클릭과 동일)")))
				.OnClicked_Lambda([this, Index]() -> FReply
				{
					if (Index >= 0 && Index < KawaiiBoneDisplayList.Num())
					{
						// 태그 해제
						KawaiiBoneDisplayList[Index].TagIndex = INDEX_NONE;
						UE_LOG(LogTemp, Log, TEXT("[KawaiiPhysics] Removed tag from bone: %s"), 
							*KawaiiBoneDisplayList[Index].BoneName.ToString());
						UpdateKawaiiBoneTreeUI();
					}
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.X"))
					.ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.4f, 0.8f))
					.DesiredSizeOverride(FVector2D(12, 12))
				]
			]
			// 선택된 태그 적용 버튼 (화살표)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return SelectedKawaiiTagIndex != INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.ChevronLeft"))
					.ColorAndOpacity(FLinearColor(1.0f, 0.5f, 0.8f, 1.0f))
					.DesiredSizeOverride(FVector2D(14, 14))
				]
			]
		];
}

void SControlRigToolWidget::OnKawaiiBoneTagChanged(int32 BoneIndex, int32 NewTagIndex)
{
	if (BoneIndex < 0 || BoneIndex >= KawaiiBoneDisplayList.Num()) return;
	
	KawaiiBoneDisplayList[BoneIndex].TagIndex = NewTagIndex;
	UpdateKawaiiBoneTreeUI();
}

FReply SControlRigToolWidget::OnApplySelectedTagClicked(int32 BoneIndex)
{
	if (SelectedKawaiiTagIndex != INDEX_NONE)
	{
		OnKawaiiBoneTagChanged(BoneIndex, SelectedKawaiiTagIndex);
	}
	return FReply::Handled();
}

void SControlRigToolWidget::UpdateKawaiiTagListUI()
{
	if (!KawaiiTagListBox.IsValid()) return;
	
	KawaiiTagListBox->ClearChildren();
	
	if (KawaiiTags.Num() == 0)
	{
		KawaiiTagListBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoTags", "No tags. Click + to add."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.45f, 1.0f))
		];
		return;
	}
	
	for (int32 i = 0; i < KawaiiTags.Num(); ++i)
	{
		KawaiiTagListBox->AddSlot()
		.AutoHeight()
		.Padding(0, 2)
		[
			CreateKawaiiTagRow(i)
		];
	}
}

TSharedRef<SWidget> SControlRigToolWidget::CreateKawaiiTagRow(int32 TagIndex)
{
	if (TagIndex < 0 || TagIndex >= KawaiiTags.Num())
	{
		return SNew(SBox);
	}
	
	FKawaiiTag& CurrentTag = KawaiiTags[TagIndex];
	bool bIsSelected = (TagIndex == SelectedKawaiiTagIndex);
	
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(bIsSelected ? FLinearColor(0.18f, 0.18f, 0.25f, 1.0f) : FLinearColor(0.06f, 0.06f, 0.08f, 1.0f))
		.Padding(FMargin(8, 6))  // 더 큰 패딩
		[
			SNew(SHorizontalBox)
			// 선택 버튼 (라디오 버튼 역할) - 더 크게
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
			[
				SNew(SBox)
				.WidthOverride(28)
				.HeightOverride(28)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "RadioButton")
					.IsChecked(bIsSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged_Lambda([this, TagIndex](ECheckBoxState NewState)
					{
						if (NewState == ECheckBoxState::Checked)
						{
							OnSelectKawaiiTag(TagIndex);
						}
					})
					.RenderTransform(FSlateRenderTransform(FScale2D(1.5f, 1.5f)))  // 1.5배 크기
					.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				]
			]
			// 컬러 버튼 (컬러 피커 팝업 오픈)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "FlatButton")
				.ContentPadding(0)
				.ToolTipText(LOCTEXT("ChangeColor", "Click to open color picker"))
				.OnClicked_Lambda([this, TagIndex]() -> FReply
				{
					if (TagIndex >= 0 && TagIndex < KawaiiTags.Num())
					{
						// 컬러 피커 창 열기
						FColorPickerArgs PickerArgs;
						PickerArgs.bOnlyRefreshOnOk = false;
						PickerArgs.bOnlyRefreshOnMouseUp = false;
						PickerArgs.bUseAlpha = false;
						PickerArgs.bExpandAdvancedSection = false;
						PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda([]() { return 2.2f; }));
						PickerArgs.InitialColor = KawaiiTags[TagIndex].Color;
						PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, TagIndex](FLinearColor NewColor)
						{
							if (TagIndex >= 0 && TagIndex < KawaiiTags.Num())
							{
								OnKawaiiTagColorChanged(TagIndex, NewColor);
							}
						});
						
						OpenColorPicker(PickerArgs);
					}
					return FReply::Handled();
				})
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([this, TagIndex]()
					{
						if (TagIndex >= 0 && TagIndex < KawaiiTags.Num())
							return KawaiiTags[TagIndex].Color;
						return FLinearColor::White;
					})
					.Padding(0)
					[
						SNew(SBox)
						.WidthOverride(24)
						.HeightOverride(24)
					]
				]
			]
			// 이름 입력 (더 큰 글씨)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredHeight(28.0f)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([this, TagIndex]() -> FText
					{
						if (TagIndex >= 0 && TagIndex < KawaiiTags.Num())
							return FText::FromString(KawaiiTags[TagIndex].Name);
						return FText::GetEmpty();
					})
					.OnTextCommitted_Lambda([this, TagIndex](const FText& NewText, ETextCommit::Type CommitType)
					{
						OnKawaiiTagNameChanged(TagIndex, NewText);
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))  // 더 큰 글씨
				]
			]
			// 삭제 버튼 (더 크게)
			+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "FlatButton")
				.ToolTipText(LOCTEXT("DeleteTag", "Delete tag"))
				.OnClicked_Lambda([this, TagIndex]() -> FReply
				{
					OnKawaiiTagDeleteClicked(TagIndex);
					return FReply::Handled();
				})
				.ContentPadding(FMargin(4))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.X"))
					.ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.4f, 1.0f))
					.DesiredSizeOverride(FVector2D(16, 16))  // 더 큰 아이콘
				]
			]
		];
}

FReply SControlRigToolWidget::OnAddKawaiiTagClicked()
{
	FKawaiiTag NewTag;
	NewTag.Name = FString::Printf(TEXT("Tag_%d"), KawaiiTags.Num() + 1);
	KawaiiTags.Add(NewTag);
	
	UpdateKawaiiTagListUI();
	SetKawaiiStatus(FString::Printf(TEXT("Added tag: %s"), *NewTag.Name));
	
	return FReply::Handled();
}

void SControlRigToolWidget::OnKawaiiTagNameChanged(int32 TagIndex, const FText& NewName)
{
	if (TagIndex >= 0 && TagIndex < KawaiiTags.Num())
	{
		KawaiiTags[TagIndex].Name = NewName.ToString();
		UpdateKawaiiTagListUI();
		UpdateKawaiiBoneTreeUI();  // 본 트리에서 태그 이름도 업데이트
	}
}

void SControlRigToolWidget::OnKawaiiTagColorChanged(int32 TagIndex, FLinearColor NewColor)
{
	if (TagIndex >= 0 && TagIndex < KawaiiTags.Num())
	{
		KawaiiTags[TagIndex].Color = NewColor;
		UpdateKawaiiTagListUI();
		UpdateKawaiiBoneTreeUI();  // 본 트리에서 컬러도 업데이트
	}
}

void SControlRigToolWidget::OnKawaiiTagDeleteClicked(int32 TagIndex)
{
	if (TagIndex >= 0 && TagIndex < KawaiiTags.Num())
	{
		// 해당 태그를 사용하는 본들의 태그 인덱스 리셋
		for (FKawaiiBoneDisplayInfo& Info : KawaiiBoneDisplayList)
		{
			if (Info.TagIndex == TagIndex)
			{
				Info.TagIndex = INDEX_NONE;
			}
			else if (Info.TagIndex > TagIndex)
			{
				Info.TagIndex--;  // 인덱스 조정
			}
		}
		
		KawaiiTags.RemoveAt(TagIndex);
		
		if (SelectedKawaiiTagIndex == TagIndex)
		{
			SelectedKawaiiTagIndex = INDEX_NONE;
		}
		else if (SelectedKawaiiTagIndex > TagIndex)
		{
			SelectedKawaiiTagIndex--;
		}
		
		UpdateKawaiiTagListUI();
		UpdateKawaiiBoneTreeUI();
	}
}

void SControlRigToolWidget::OnSelectKawaiiTag(int32 TagIndex)
{
	SelectedKawaiiTagIndex = TagIndex;
	UpdateKawaiiTagListUI();
	UpdateKawaiiBoneTreeUI();  // 화살표 버튼 표시 업데이트
	
	if (TagIndex >= 0 && TagIndex < KawaiiTags.Num())
	{
		SetKawaiiStatus(FString::Printf(TEXT("Selected tag: %s (click arrow to apply)"), *KawaiiTags[TagIndex].Name));
	}
}

void SControlRigToolWidget::TransferSecondaryDataToKawaii()
{
	// Control Rig 탭에서 선택한 메쉬가 있으면 Kawaii 탭으로 전달
	if (SelectedMesh.IsValid() && !SelectedMesh->IsEmpty())
	{
		// 메쉬 선택
		SelectedKawaiiMesh = SelectedMesh;
		if (KawaiiMeshComboBox.IsValid())
		{
			KawaiiMeshComboBox->SetSelectedItem(SelectedKawaiiMesh);
		}
		UpdateKawaiiMeshThumbnail();
		
		// 자동 출력 이름 설정
		FString MeshName = FPaths::GetBaseFilename(*SelectedMesh);
		FString AutoName = FString::Printf(TEXT("ABP_%s_Kawaii"), *MeshName);
		if (KawaiiOutputNameBox.IsValid())
		{
			KawaiiOutputNameBox->SetText(FText::FromString(AutoName));
		}
		
		// 본 트리 빌드 (Secondary 정보 포함)
		BuildKawaiiBoneDisplayList();
		
		// Secondary 본 개수 카운트
		int32 SecondaryCount = 0;
		for (const FKawaiiBoneDisplayInfo& Info : KawaiiBoneDisplayList)
		{
			if (Info.bIsSecondary) SecondaryCount++;
		}
		
		SetKawaiiStatus(FString::Printf(TEXT("Loaded from Control Rig: %s (%d secondary bones)"), *MeshName, SecondaryCount));
	}
}

FReply SControlRigToolWidget::OnCreateKawaiiAnimBPClicked()
{
	if (!SelectedKawaiiMesh.IsValid() || SelectedKawaiiMesh->IsEmpty())
	{
		SetKawaiiStatus(TEXT("Error: Please select a Skeletal Mesh first"));
		return FReply::Handled();
	}
	
	// 태그가 적용된 본이 있는지 확인
	int32 TaggedBoneCount = 0;
	for (const FKawaiiBoneDisplayInfo& Info : KawaiiBoneDisplayList)
	{
		if (Info.TagIndex != INDEX_NONE)
		{
			TaggedBoneCount++;
		}
	}
	
	if (TaggedBoneCount == 0)
	{
		SetKawaiiStatus(TEXT("Error: Please assign at least one tag to bones"));
		return FReply::Handled();
	}
	
	CreateKawaiiAnimBlueprint();
	
	return FReply::Handled();
}

bool SControlRigToolWidget::CreateKawaiiAnimBlueprint()
{
	SetKawaiiStatus(TEXT("Creating Kawaii AnimBlueprint..."));
	
	// ============================================================================
	// 1. 스켈레탈 메쉬 및 스켈레톤 가져오기
	// ============================================================================
	FString MeshPath = GetSelectedKawaiiMeshPath();
	if (MeshPath.IsEmpty())
	{
		SetKawaiiStatus(TEXT("Error: No skeletal mesh selected"));
		return false;
	}
	
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
	if (!SkeletalMesh)
	{
		SetKawaiiStatus(TEXT("Error: Failed to load skeletal mesh"));
		return false;
	}
	
	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	if (!Skeleton)
	{
		SetKawaiiStatus(TEXT("Error: Skeletal mesh has no skeleton"));
		return false;
	}
	
	// ============================================================================
	// 2. 출력 경로 설정
	// ============================================================================
	FString OutputFolder = KawaiiOutputFolderBox.IsValid() ? KawaiiOutputFolderBox->GetText().ToString() : KawaiiDefaultOutputFolder;
	FString OutputName = KawaiiOutputNameBox.IsValid() ? KawaiiOutputNameBox->GetText().ToString() : TEXT("NewKawaiiAnimBP");
	FString NewAssetPath = OutputFolder / OutputName;
	
	// 패키지 경로 준비
	FString PackagePath = NewAssetPath;
	FString AssetName = OutputName;
	
	// ============================================================================
	// 3. AnimBlueprint 생성
	// ============================================================================
	// 기존 에셋 확인 및 삭제
	FString FullAssetPath = PackagePath + TEXT(".") + AssetName;
	UObject* ExistingAsset = StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *FullAssetPath);
	if (ExistingAsset)
	{
		UE_LOG(LogTemp, Log, TEXT("Deleting existing AnimBP: %s"), *FullAssetPath);
		
		// 에셋 삭제
		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(ExistingAsset);
		ObjectTools::DeleteObjects(ObjectsToDelete, false);
		
		// 패키지도 정리
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		SetKawaiiStatus(TEXT("Error: Failed to create package"));
		return false;
	}
	
	// 혹시 패키지 내에 같은 이름의 블루프린트가 있는지 확인
	UBlueprint* ExistingBP = FindObject<UBlueprint>(Package, *AssetName);
	if (ExistingBP)
	{
		UE_LOG(LogTemp, Log, TEXT("Found existing Blueprint in package, removing..."));
		ExistingBP->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
	}
	
	// AnimBlueprint 생성 (FKismetEditorUtilities 사용)
	UAnimBlueprint* AnimBP = CastChecked<UAnimBlueprint>(
		FKismetEditorUtilities::CreateBlueprint(
			UAnimInstance::StaticClass(),     // ParentClass
			Package,                           // InParent
			FName(*AssetName),                 // Name
			BPTYPE_Normal,                     // BlueprintType
			UAnimBlueprint::StaticClass(),     // BlueprintClass
			UBlueprintGeneratedClass::StaticClass(),  // GeneratedClass
			NAME_None                          // CallingContext
		)
	);
	
	if (!AnimBP)
	{
		SetKawaiiStatus(TEXT("Error: Failed to create AnimBlueprint"));
		return false;
	}
	
	// 스켈레톤 설정
	AnimBP->TargetSkeleton = Skeleton;
	AnimBP->SetPreviewMesh(SkeletalMesh);
	
	// Generated Class에도 스켈레톤 설정
	if (UAnimBlueprintGeneratedClass* GeneratedClass = Cast<UAnimBlueprintGeneratedClass>(AnimBP->GeneratedClass))
	{
		GeneratedClass->TargetSkeleton = Skeleton;
	}
	
	// ============================================================================
	// 4. AnimGraph 가져오기
	// ============================================================================
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AnimBP->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
		{
			AnimGraph = Graph;
			break;
		}
	}
	
	if (!AnimGraph)
	{
		SetKawaiiStatus(TEXT("Error: AnimGraph not found in AnimBlueprint"));
		return false;
	}
	
	// ============================================================================
	// 5. A영역 기본 노드 생성 (Output Pose는 이미 존재)
	// ============================================================================
	
	// 기존 Output Pose (Root) 노드 찾기
	UAnimGraphNode_Root* RootNode = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		RootNode = Cast<UAnimGraphNode_Root>(Node);
		if (RootNode) break;
	}
	
	if (!RootNode)
	{
		SetKawaiiStatus(TEXT("Error: Output Pose node not found"));
		return false;
	}
	
	// 노드 위치 기준
	const float BaseX = RootNode->NodePosX - 200;
	const float BaseY = RootNode->NodePosY;
	const float NodeSpacing = 250.0f;
	
	// --- Input Pose 노드 생성 ---
	FGraphNodeCreator<UAnimGraphNode_LinkedInputPose> InputPoseCreator(*AnimGraph);
	UAnimGraphNode_LinkedInputPose* InputPoseNode = InputPoseCreator.CreateNode();
	InputPoseNode->NodePosX = BaseX - NodeSpacing * 6;
	InputPoseNode->NodePosY = BaseY;
	InputPoseCreator.Finalize();
	
	// --- Slot 노드 생성 ---
	FGraphNodeCreator<UAnimGraphNode_Slot> SlotCreator(*AnimGraph);
	UAnimGraphNode_Slot* SlotNode = SlotCreator.CreateNode();
	SlotNode->Node.SlotName = FName(TEXT("DefaultSlot"));
	SlotNode->Node.bAlwaysUpdateSourcePose = true;  // 항상 소스포즈 업데이트 체크
	SlotNode->NodePosX = BaseX - NodeSpacing * 5;
	SlotNode->NodePosY = BaseY;
	SlotCreator.Finalize();
	
	// --- Save Cached Pose 노드 생성 ---
	FGraphNodeCreator<UAnimGraphNode_SaveCachedPose> SaveCacheCreator(*AnimGraph);
	UAnimGraphNode_SaveCachedPose* SaveCacheNode = SaveCacheCreator.CreateNode();
	SaveCacheNode->CacheName = TEXT("Kawaii");
	SaveCacheNode->NodePosX = BaseX - NodeSpacing * 4;
	SaveCacheNode->NodePosY = BaseY;
	SaveCacheCreator.Finalize();
	
	// --- Use Cached Pose 노드 1 생성 (Local To Component 방향으로) ---
	FGraphNodeCreator<UAnimGraphNode_UseCachedPose> UseCacheCreator(*AnimGraph);
	UAnimGraphNode_UseCachedPose* UseCacheNode = UseCacheCreator.CreateNode();
	UseCacheNode->SaveCachedPoseNode = SaveCacheNode;
	UseCacheNode->NodePosX = BaseX - NodeSpacing * 3;
	UseCacheNode->NodePosY = BaseY;
	UseCacheCreator.Finalize();
	
	// --- Use Cached Pose 노드 2 생성 (Two Way Blend B 방향으로) ---
	FGraphNodeCreator<UAnimGraphNode_UseCachedPose> UseCacheCreator2(*AnimGraph);
	UAnimGraphNode_UseCachedPose* UseCacheNode2 = UseCacheCreator2.CreateNode();
	UseCacheNode2->SaveCachedPoseNode = SaveCacheNode;
	UseCacheNode2->NodePosX = BaseX - NodeSpacing * 3;
	UseCacheNode2->NodePosY = BaseY + 80;  // 첫번째 노드 아래에 배치
	UseCacheCreator2.Finalize();
	
	// --- Local To Component 노드 생성 ---
	FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> LocalToCompCreator(*AnimGraph);
	UAnimGraphNode_LocalToComponentSpace* LocalToCompNode = LocalToCompCreator.CreateNode();
	LocalToCompNode->NodePosX = BaseX - NodeSpacing * 2;
	LocalToCompNode->NodePosY = BaseY;
	LocalToCompCreator.Finalize();
	
	// --- Component To Local 노드 생성 ---
	FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> CompToLocalCreator(*AnimGraph);
	UAnimGraphNode_ComponentToLocalSpace* CompToLocalNode = CompToLocalCreator.CreateNode();
	CompToLocalNode->NodePosX = BaseX - NodeSpacing * 1;
	CompToLocalNode->NodePosY = BaseY;
	CompToLocalCreator.Finalize();
	
	// --- Two Way Blend 노드 생성 ---
	FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> BlendCreator(*AnimGraph);
	UAnimGraphNode_TwoWayBlend* BlendNode = BlendCreator.CreateNode();
	BlendNode->NodePosX = BaseX;
	BlendNode->NodePosY = BaseY;
	BlendCreator.Finalize();
	
	// ============================================================================
	// 6. A영역 노드 연결
	// ============================================================================
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	
	// 헬퍼: 핀 이름으로 찾기 (대소문자 무시, 부분 매칭)
	auto FindPinByName = [](UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Dir) -> UEdGraphPin* {
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == Dir && Pin->GetName().Contains(PinName))
			{
				return Pin;
			}
		}
		// 못 찾으면 첫 번째 핀 반환
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == Dir)
			{
				return Pin;
			}
		}
		return nullptr;
	};
	
	// 디버그: 핀 정보 출력
	auto LogPins = [](UEdGraphNode* Node, const FString& NodeName) {
		UE_LOG(LogTemp, Log, TEXT("=== %s Pins ==="), *NodeName);
		for (UEdGraphPin* Pin : Node->Pins)
		{
			UE_LOG(LogTemp, Log, TEXT("  Pin: %s, Direction: %s, Category: %s"), 
				*Pin->GetName(), 
				Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"),
				*Pin->PinType.PinCategory.ToString());
		}
	};
	
	LogPins(InputPoseNode, TEXT("InputPose"));
	LogPins(SlotNode, TEXT("Slot"));
	LogPins(SaveCacheNode, TEXT("SaveCachedPose"));
	LogPins(UseCacheNode, TEXT("UseCachedPose1"));
	LogPins(UseCacheNode2, TEXT("UseCachedPose2"));
	LogPins(LocalToCompNode, TEXT("LocalToComponent"));
	LogPins(CompToLocalNode, TEXT("ComponentToLocal"));
	LogPins(BlendNode, TEXT("TwoWayBlend"));
	LogPins(RootNode, TEXT("OutputPose"));
	
	// Input Pose 출력 -> Slot Source 입력 연결
	UEdGraphPin* InputPoseOutputPin = FindPinByName(InputPoseNode, TEXT("Pose"), EGPD_Output);
	if (!InputPoseOutputPin) InputPoseOutputPin = FindPinByName(InputPoseNode, TEXT(""), EGPD_Output);
	UEdGraphPin* SlotSourcePin = FindPinByName(SlotNode, TEXT("Source"), EGPD_Input);
	
	if (InputPoseOutputPin && SlotSourcePin)
	{
		bool bConnected = Schema->TryCreateConnection(InputPoseOutputPin, SlotSourcePin);
		UE_LOG(LogTemp, Log, TEXT("InputPose -> Slot.Source: %s (Pin: %s -> %s)"), 
			bConnected ? TEXT("SUCCESS") : TEXT("FAILED"),
			*InputPoseOutputPin->GetName(), *SlotSourcePin->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("InputPose -> Slot connection failed: InputPin=%s, SourcePin=%s"),
			InputPoseOutputPin ? TEXT("Found") : TEXT("NOT FOUND"),
			SlotSourcePin ? TEXT("Found") : TEXT("NOT FOUND"));
	}
	
	// Slot 출력 -> SaveCachedPose 입력 연결
	UEdGraphPin* SlotOutputPin = FindPinByName(SlotNode, TEXT("Pose"), EGPD_Output);
	UEdGraphPin* SaveCacheInputPin = FindPinByName(SaveCacheNode, TEXT("Pose"), EGPD_Input);
	
	if (SlotOutputPin && SaveCacheInputPin)
	{
		bool bConnected = Schema->TryCreateConnection(SlotOutputPin, SaveCacheInputPin);
		UE_LOG(LogTemp, Log, TEXT("Slot -> SaveCachedPose: %s (Pin: %s -> %s)"), 
			bConnected ? TEXT("SUCCESS") : TEXT("FAILED"),
			*SlotOutputPin->GetName(), *SaveCacheInputPin->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find pins for Slot -> SaveCachedPose"));
	}
	
	// UseCachedPose 출력 -> LocalToComponent 입력 연결
	UEdGraphPin* UseCacheOutputPin = FindPinByName(UseCacheNode, TEXT("Pose"), EGPD_Output);
	UEdGraphPin* LocalToCompInputPin = FindPinByName(LocalToCompNode, TEXT(""), EGPD_Input);
	
	if (UseCacheOutputPin && LocalToCompInputPin)
	{
		bool bConnected = Schema->TryCreateConnection(UseCacheOutputPin, LocalToCompInputPin);
		UE_LOG(LogTemp, Log, TEXT("UseCachedPose -> LocalToComponent: %s (Pin: %s -> %s)"), 
			bConnected ? TEXT("SUCCESS") : TEXT("FAILED"),
			*UseCacheOutputPin->GetName(), *LocalToCompInputPin->GetName());
	}
	
	// LocalToComponent 출력, ComponentToLocal 입력/출력 핀 찾기
	UEdGraphPin* LocalToCompOutputPin = FindPinByName(LocalToCompNode, TEXT(""), EGPD_Output);
	UEdGraphPin* CompToLocalInputPin = FindPinByName(CompToLocalNode, TEXT(""), EGPD_Input);
	UEdGraphPin* CompToLocalOutputPin = FindPinByName(CompToLocalNode, TEXT(""), EGPD_Output);
	
	// ComponentToLocal 출력 -> TwoWayBlend A 입력 연결
	UEdGraphPin* BlendInputA = FindPinByName(BlendNode, TEXT("A"), EGPD_Input);
	
	if (CompToLocalOutputPin && BlendInputA)
	{
		bool bConnected = Schema->TryCreateConnection(CompToLocalOutputPin, BlendInputA);
		UE_LOG(LogTemp, Log, TEXT("ComponentToLocal -> TwoWayBlend.A: %s (Pin: %s -> %s)"), 
			bConnected ? TEXT("SUCCESS") : TEXT("FAILED"),
			*CompToLocalOutputPin->GetName(), *BlendInputA->GetName());
	}
	
	// UseCachedPose2 출력 -> TwoWayBlend B 입력 연결
	UEdGraphPin* UseCacheOutputPin2 = FindPinByName(UseCacheNode2, TEXT("Pose"), EGPD_Output);
	UEdGraphPin* BlendInputB = FindPinByName(BlendNode, TEXT("B"), EGPD_Input);
	
	if (UseCacheOutputPin2 && BlendInputB)
	{
		bool bConnected = Schema->TryCreateConnection(UseCacheOutputPin2, BlendInputB);
		UE_LOG(LogTemp, Log, TEXT("UseCachedPose2 -> TwoWayBlend.B: %s (Pin: %s -> %s)"), 
			bConnected ? TEXT("SUCCESS") : TEXT("FAILED"),
			*UseCacheOutputPin2->GetName(), *BlendInputB->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UseCachedPose2 -> TwoWayBlend.B: Failed to find pins"));
	}
	
	// TwoWayBlend 출력 -> OutputPose 입력 연결
	UE_LOG(LogTemp, Log, TEXT("=== Connecting TwoWayBlend -> OutputPose ==="));
	UEdGraphPin* BlendOutputPin = FindPinByName(BlendNode, TEXT("Pose"), EGPD_Output);
	if (!BlendOutputPin) BlendOutputPin = FindPinByName(BlendNode, TEXT(""), EGPD_Output);
	UEdGraphPin* RootInputPin = FindPinByName(RootNode, TEXT("Result"), EGPD_Input);
	if (!RootInputPin) RootInputPin = FindPinByName(RootNode, TEXT(""), EGPD_Input);
	
	UE_LOG(LogTemp, Log, TEXT("BlendOutputPin: %s, RootInputPin: %s"),
		BlendOutputPin ? *BlendOutputPin->GetName() : TEXT("NULL"),
		RootInputPin ? *RootInputPin->GetName() : TEXT("NULL"));
	
	if (BlendOutputPin && RootInputPin)
	{
		bool bConnected = Schema->TryCreateConnection(BlendOutputPin, RootInputPin);
		UE_LOG(LogTemp, Log, TEXT("TwoWayBlend -> OutputPose: %s (Pin: %s -> %s)"), 
			bConnected ? TEXT("SUCCESS") : TEXT("FAILED"),
			*BlendOutputPin->GetName(), *RootInputPin->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TwoWayBlend -> OutputPose: Cannot connect, pins not found"));
	}
	
	// ============================================================================
	// A 영역 코멘트 박스 생성 - "Kawaii"
	// 노드들을 더 위로 띄우기 위해 먼저 모든 A영역 노드들의 Y 위치를 조정
	// ============================================================================
	const float AAreaYOffset = -150.0f;  // 노드들을 위로 올림
	InputPoseNode->NodePosY += AAreaYOffset;
	SlotNode->NodePosY += AAreaYOffset;
	SaveCacheNode->NodePosY += AAreaYOffset;
	UseCacheNode->NodePosY += AAreaYOffset;
	UseCacheNode2->NodePosY += AAreaYOffset;
	LocalToCompNode->NodePosY += AAreaYOffset;
	CompToLocalNode->NodePosY += AAreaYOffset;
	BlendNode->NodePosY += AAreaYOffset;
	RootNode->NodePosY += AAreaYOffset;
	
	UEdGraphNode_Comment* AAreaComment = NewObject<UEdGraphNode_Comment>(AnimGraph);
	AAreaComment->NodePosX = InputPoseNode->NodePosX - 30;
	AAreaComment->NodePosY = InputPoseNode->NodePosY - 50;
	// Input Pose부터 Output Pose까지의 너비 계산
	float AAreaWidth = (RootNode->NodePosX + 280) - (InputPoseNode->NodePosX - 30);
	AAreaComment->NodeWidth = AAreaWidth;
	AAreaComment->NodeHeight = 220;  // Use Cached Pose 2개 포함하도록 높이
	AAreaComment->CommentColor = FLinearColor(0.1f, 0.2f, 0.3f, 1.0f);  // 어두운 파란색
	AAreaComment->CreateNewGuid();
	AAreaComment->PostPlacedNewNode();
	// PostPlacedNewNode() 이후에 NodeComment 설정 (PostPlacedNewNode가 기본값으로 덮어쓰기 때문)
	AAreaComment->NodeComment = TEXT("Pose");
	AnimGraph->AddNode(AAreaComment, true, false);
	UE_LOG(LogTemp, Log, TEXT("Created A-Area comment box: Pose"));
	
	// LocalToComponent -> ComponentToLocal 직접 연결 (나중에 Kawaii Physics가 있으면 이 연결을 끊고 중간에 삽입)
	if (LocalToCompOutputPin && CompToLocalInputPin)
	{
		bool bConnected = Schema->TryCreateConnection(LocalToCompOutputPin, CompToLocalInputPin);
		UE_LOG(LogTemp, Log, TEXT("LocalToComponent -> ComponentToLocal (direct): %s (Pin: %s -> %s)"), 
			bConnected ? TEXT("SUCCESS") : TEXT("FAILED"),
			*LocalToCompOutputPin->GetName(), *CompToLocalInputPin->GetName());
	}
	
	// ============================================================================
	// 7. B영역 - 태그별 Kawaii Physics 노드 동적 생성
	// ============================================================================
	
	// 태그별 본 정보 수집
	TMap<int32, TArray<FName>> TaggedBones;
	for (const FKawaiiBoneDisplayInfo& Info : KawaiiBoneDisplayList)
	{
		if (Info.TagIndex != INDEX_NONE)
		{
			TaggedBones.FindOrAdd(Info.TagIndex).Add(Info.BoneName);
		}
	}
	
	// ============================================================================
	// Kawaii Physics 모듈 강제 로드 및 클래스 찾기
	// ============================================================================
	UE_LOG(LogTemp, Log, TEXT("=== Searching for Kawaii Physics ==="));
	
	// 1. KawaiiPhysicsEd 모듈 강제 로드 시도
	bool bKawaiiModuleLoaded = false;
	if (FModuleManager::Get().IsModuleLoaded("KawaiiPhysicsEd"))
	{
		UE_LOG(LogTemp, Log, TEXT("KawaiiPhysicsEd module already loaded"));
		bKawaiiModuleLoaded = true;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Attempting to load KawaiiPhysicsEd module..."));
		IModuleInterface* Module = FModuleManager::Get().LoadModule("KawaiiPhysicsEd");
		if (Module)
		{
			UE_LOG(LogTemp, Log, TEXT("KawaiiPhysicsEd module loaded successfully!"));
			bKawaiiModuleLoaded = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load KawaiiPhysicsEd module"));
		}
	}
	
	// 2. 런타임 모듈도 로드 시도
	if (!FModuleManager::Get().IsModuleLoaded("KawaiiPhysics"))
	{
		FModuleManager::Get().LoadModule("KawaiiPhysics");
	}
	
	// 3. 로드된 모듈 목록 출력 (디버그용)
	TArray<FModuleStatus> ModuleStatuses;
	FModuleManager::Get().QueryModules(ModuleStatuses);
	for (const FModuleStatus& Status : ModuleStatuses)
	{
		if (Status.Name.Contains(TEXT("Kawaii"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Log, TEXT("Found Kawaii module: %s (Loaded: %s)"), 
				*Status.Name, Status.bIsLoaded ? TEXT("Yes") : TEXT("No"));
		}
	}
	
	// 4. AnimGraphNode_KawaiiPhysics 클래스 찾기
	UClass* KawaiiNodeClass = nullptr;
	
	// 방법 1: UClass::TryFindTypeSlow로 찾기 (UE5 방식)
	KawaiiNodeClass = UClass::TryFindTypeSlow<UClass>(TEXT("AnimGraphNode_KawaiiPhysics"));
	if (KawaiiNodeClass)
	{
		UE_LOG(LogTemp, Log, TEXT("Found AnimGraphNode_KawaiiPhysics via TryFindTypeSlow: %s"), *KawaiiNodeClass->GetPathName());
	}
	
	// 방법 2: FindObject 실패 시 TObjectIterator로 검색
	if (!KawaiiNodeClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* TestClass = *It;
			FString ClassName = TestClass->GetName();
			if (ClassName.Equals(TEXT("AnimGraphNode_KawaiiPhysics"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogTemp, Log, TEXT("Found AnimGraphNode_KawaiiPhysics via TObjectIterator: %s"), *TestClass->GetPathName());
				if (TestClass->IsChildOf(UAnimGraphNode_Base::StaticClass()))
				{
					KawaiiNodeClass = TestClass;
					break;
				}
			}
		}
	}
	
	// 방법 3: 부분 매칭으로 찾기 (최후의 수단)
	if (!KawaiiNodeClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* TestClass = *It;
			FString ClassName = TestClass->GetName();
			if (ClassName.Contains(TEXT("Kawaii"), ESearchCase::IgnoreCase) && 
				ClassName.Contains(TEXT("AnimGraph"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogTemp, Log, TEXT("Found Kawaii AnimGraph class: %s (Path: %s)"), 
					*ClassName, *TestClass->GetPathName());
				
				if (TestClass->IsChildOf(UAnimGraphNode_Base::StaticClass()))
				{
					KawaiiNodeClass = TestClass;
					UE_LOG(LogTemp, Log, TEXT("Using class: %s"), *TestClass->GetPathName());
					break;
				}
			}
		}
	}
	
	bool bKawaiiPhysicsAvailable = (KawaiiNodeClass != nullptr);
	
	if (!KawaiiNodeClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Kawaii Physics node class not found. Nodes will not be auto-generated."));
		UE_LOG(LogTemp, Warning, TEXT("Module load status: KawaiiPhysicsEd=%s"), bKawaiiModuleLoaded ? TEXT("OK") : TEXT("FAILED"));
		UE_LOG(LogTemp, Warning, TEXT("Make sure KawaiiPhysics plugin is enabled in your project"));
	}
	
	// Kawaii Physics 노드들을 저장할 배열 (나중에 연결용)
	TArray<UEdGraphNode*> KawaiiPhysicsNodes;
	bool bKawaiiNodesCreated = false;
	
	// ============================================================================
	// 공용 변수 4개 생성 (태그와 무관하게 한 번만)
	// ============================================================================
	
	// 변수 이름 목록 (Instance Editable + Expose to Cinematics 설정용)
	TArray<FName> SharedVarNames;
	
	// 1. KawaiiAlpha 변수 (공용, Bool) - Two Way Blend bAlphaBoolEnabled에 연결됨
	FEdGraphPinType KawaiiBoolType;
	KawaiiBoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	FBlueprintEditorUtils::AddMemberVariable(AnimBP, FName(TEXT("KawaiiAlpha")), KawaiiBoolType);
	SharedVarNames.Add(FName(TEXT("KawaiiAlpha")));
	UE_LOG(LogTemp, Log, TEXT("Created shared variable: KawaiiAlpha (Bool)"));
	
	// 2. Enable Wind 변수 (공용)
	FEdGraphPinType BoolType;
	BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	FBlueprintEditorUtils::AddMemberVariable(AnimBP, FName(TEXT("EnableWind")), BoolType);
	SharedVarNames.Add(FName(TEXT("EnableWind")));
	UE_LOG(LogTemp, Log, TEXT("Created shared variable: EnableWind"));
	
	// 3. Wind Scale 변수 (공용)
	FEdGraphPinType FloatType;
	FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
	FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	FBlueprintEditorUtils::AddMemberVariable(AnimBP, FName(TEXT("WindScale")), FloatType);
	SharedVarNames.Add(FName(TEXT("WindScale")));
	UE_LOG(LogTemp, Log, TEXT("Created shared variable: WindScale"));
	
	// 4. Gravity 변수 (공용) - 이전 External Forces
	FEdGraphPinType VectorType;
	VectorType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	VectorType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	FBlueprintEditorUtils::AddMemberVariable(AnimBP, FName(TEXT("Gravity")), VectorType);
	SharedVarNames.Add(FName(TEXT("Gravity")));
	UE_LOG(LogTemp, Log, TEXT("Created shared variable: Gravity"));
	
	// 공용 변수들에 Instance Editable + Expose to Cinematics 설정
	for (const FName& VarName : SharedVarNames)
	{
		// Instance Editable 설정 (EditAnywhere와 동일 효과)
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(AnimBP, VarName, false);
		// Expose to Cinematics (Interp) 설정
		FBlueprintEditorUtils::SetInterpFlag(AnimBP, VarName, true);
		UE_LOG(LogTemp, Log, TEXT("Set Instance Editable + Expose to Cinematics: %s"), *VarName.ToString());
	}
	
	// 태그별 코멘트 박스 및 노드 생성
	float CommentY = BaseY + 200.0f;
	for (auto& Pair : TaggedBones)
	{
		int32 TagIdx = Pair.Key;
		if (TagIdx < 0 || TagIdx >= KawaiiTags.Num()) continue;
		
		const FKawaiiTag& CurrentTag = KawaiiTags[TagIdx];
		const TArray<FName>& TagBones = Pair.Value;
		int32 NumBones = TagBones.Num();
		
		// ============================================================================
		// Physics Settings 변수 생성 (태그별로 각각)
		// ============================================================================
		FString TagVarName = CurrentTag.Name;
		TagVarName = TagVarName.Replace(TEXT(" "), TEXT("_")); // 공백 제거
		
		FName PhysicsSettingsVarName = FName(*TagVarName);
		FEdGraphPinType PhysicsSettingsType;
		PhysicsSettingsType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		
		// KawaiiPhysicsSettings 구조체 찾기 - 여러 경로 시도
		UScriptStruct* KawaiiSettingsStruct = nullptr;
		
		// 방법 1: F 접두사 없이
		KawaiiSettingsStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/KawaiiPhysics.KawaiiPhysicsSettings"));
		
		// 방법 2: F 접두사 포함
		if (!KawaiiSettingsStruct)
		{
			KawaiiSettingsStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/KawaiiPhysics.FKawaiiPhysicsSettings"));
		}
		
		// 방법 3: TObjectIterator로 검색
		if (!KawaiiSettingsStruct)
		{
			for (TObjectIterator<UScriptStruct> It; It; ++It)
			{
				if (It->GetName().Contains(TEXT("KawaiiPhysicsSettings")))
				{
					UE_LOG(LogTemp, Log, TEXT("Found struct via iterator: %s"), *It->GetPathName());
					KawaiiSettingsStruct = *It;
					break;
				}
			}
		}
		
		if (KawaiiSettingsStruct)
		{
			PhysicsSettingsType.PinSubCategoryObject = KawaiiSettingsStruct;
			UE_LOG(LogTemp, Log, TEXT("Using KawaiiPhysicsSettings struct: %s"), *KawaiiSettingsStruct->GetPathName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("KawaiiPhysicsSettings struct not found, using default"));
			PhysicsSettingsType.PinCategory = UEdGraphSchema_K2::PC_Object;
			PhysicsSettingsType.PinSubCategoryObject = UObject::StaticClass();
		}
		FBlueprintEditorUtils::AddMemberVariable(AnimBP, PhysicsSettingsVarName, PhysicsSettingsType);
		// Physics Settings 변수에 Instance Editable + Expose to Cinematics 설정
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(AnimBP, PhysicsSettingsVarName, false);
		FBlueprintEditorUtils::SetInterpFlag(AnimBP, PhysicsSettingsVarName, true);
		UE_LOG(LogTemp, Log, TEXT("Created tag variable: %s (Physics Settings, Instance Editable, Exposed to Cinematics)"), *PhysicsSettingsVarName.ToString());
		
		// 코멘트 박스 생성 (변수용)
		UEdGraphNode_Comment* VarComment = NewObject<UEdGraphNode_Comment>(AnimGraph);
		VarComment->NodePosX = BaseX - NodeSpacing * 6;
		VarComment->NodePosY = CommentY;
		VarComment->NodeWidth = 350;
		VarComment->NodeHeight = 280;  // 변수 간격 조밀해서 높이 줄임
		VarComment->NodeComment = FString::Printf(TEXT("%s - Variables"), *CurrentTag.Name);
		VarComment->CommentColor = CurrentTag.Color;
		AnimGraph->AddNode(VarComment, true, false);
		
		// ============================================================================
		// 변수 Get 노드 생성 (코멘트 박스 내부)
		// ============================================================================
		float VarNodeX = VarComment->NodePosX + 30.0f;
		float VarNodeY = VarComment->NodePosY + 50.0f;
		const float VarNodeSpacingY = 55.0f;  // 변수 노드 간격 조밀하게
		
		TArray<UK2Node_VariableGet*> TagGetNodes;
		TArray<FName> TagVarNames = {
			PhysicsSettingsVarName,  // 태그별 Physics Settings
			FName(TEXT("Gravity")),
			FName(TEXT("EnableWind")),
			FName(TEXT("WindScale"))
		};
		
		for (int32 VarIdx = 0; VarIdx < TagVarNames.Num(); ++VarIdx)
		{
			UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(AnimGraph);
			if (GetNode)
			{
				GetNode->NodePosX = VarNodeX;
				GetNode->NodePosY = VarNodeY + VarIdx * VarNodeSpacingY;
				
				// 변수 참조 설정
				GetNode->VariableReference.SetSelfMember(TagVarNames[VarIdx]);
				
				// 노드 초기화
				GetNode->CreateNewGuid();
				GetNode->PostPlacedNewNode();
				GetNode->SetFlags(RF_Transactional);
				GetNode->AllocateDefaultPins();
				
				AnimGraph->AddNode(GetNode, true, false);
				TagGetNodes.Add(GetNode);
				
				UE_LOG(LogTemp, Log, TEXT("Created Variable Get node: %s at (%d, %d)"), 
					*TagVarNames[VarIdx].ToString(), GetNode->NodePosX, GetNode->NodePosY);
			}
		}
		
		// 코멘트 박스 생성 (Kawaii Physics 노드용)
		int32 Rows = (NumBones + 4) / 5; // 5개씩 배열
		const float KawaiiNodeWidth = 280.0f;   // 노드 간격 더 넓게
		const float KawaiiNodeHeight = 220.0f;  // 노드 높이 더 넓게
		float CommentWidth = FMath::Min(NumBones, 5) * KawaiiNodeWidth + 80.0f;  // 여유 공간 추가
		float CommentHeight = Rows * KawaiiNodeHeight + 80.0f;  // 여유 공간 추가
		
		UEdGraphNode_Comment* PhysicsComment = NewObject<UEdGraphNode_Comment>(AnimGraph);
		PhysicsComment->NodePosX = BaseX - NodeSpacing * 2;
		PhysicsComment->NodePosY = CommentY;
		PhysicsComment->NodeWidth = CommentWidth;
		PhysicsComment->NodeHeight = CommentHeight;
		PhysicsComment->NodeComment = FString::Printf(TEXT("%s - Kawaii Physics (%d bones)"), *CurrentTag.Name, NumBones);
		PhysicsComment->CommentColor = CurrentTag.Color;
		AnimGraph->AddNode(PhysicsComment, true, false);
		
		// Kawaii Physics 노드 동적 생성 (클래스가 있는 경우)
		if (KawaiiNodeClass)
		{
			float NodeStartX = PhysicsComment->NodePosX + 40.0f;
			float NodeStartY = PhysicsComment->NodePosY + 50.0f;
			const float NodeWidth = KawaiiNodeWidth;   // 코멘트 박스와 동일한 간격 사용
			const float NodeHeight = KawaiiNodeHeight; // 코멘트 박스와 동일한 간격 사용
			const int32 NodesPerRow = 5;
			
			for (int32 BoneIdx = 0; BoneIdx < TagBones.Num(); ++BoneIdx)
			{
				const FName& BoneName = TagBones[BoneIdx];
				
				// ============================================================================
				// 체인 분석: 웨이트 없는 본 찾기
				// ============================================================================
				FName ExcludeBoneName = NAME_None;
				bool bHasDeadBones = false;
				
				// 현재 본의 인덱스 찾기
				int32 RootBoneDispIdx = INDEX_NONE;
				for (int32 i = 0; i < KawaiiBoneDisplayList.Num(); ++i)
				{
					if (KawaiiBoneDisplayList[i].BoneName == BoneName)
					{
						RootBoneDispIdx = i;
						break;
					}
				}
				
				if (RootBoneDispIdx != INDEX_NONE)
				{
					// 자식 체인 추적 (깊이 우선 탐색)
					TArray<int32> ChainIndices;
					ChainIndices.Add(RootBoneDispIdx);
					
					// 자식들 순차 탐색
					for (int32 i = 0; i < KawaiiBoneDisplayList.Num(); ++i)
					{
						const FKawaiiBoneDisplayInfo& Info = KawaiiBoneDisplayList[i];
						// 이 본의 부모가 체인에 있으면 체인에 추가
						if (Info.ParentIndex != INDEX_NONE)
						{
							for (int32 ChainIdx : ChainIndices)
							{
								if (KawaiiBoneDisplayList[ChainIdx].BoneIndex == Info.ParentIndex)
								{
									ChainIndices.AddUnique(i);
									break;
								}
							}
						}
					}
					
					// 체인에서 웨이트 없는 첫 번째 본 찾기 (루트 본 제외)
					for (int32 i = 1; i < ChainIndices.Num(); ++i)
					{
						const FKawaiiBoneDisplayInfo& ChainInfo = KawaiiBoneDisplayList[ChainIndices[i]];
						if (!ChainInfo.bHasSkinWeight)
						{
							ExcludeBoneName = ChainInfo.BoneName;
							bHasDeadBones = true;
							UE_LOG(LogTemp, Log, TEXT("  Chain %s: Found dead bone (no weight) at %s"), 
								*BoneName.ToString(), *ExcludeBoneName.ToString());
							break;
						}
					}
				}
				
				// 노드 위치 계산 (5x? 그리드)
				int32 Col = BoneIdx % NodesPerRow;
				int32 Row = BoneIdx / NodesPerRow;
				float NodeX = NodeStartX + Col * NodeWidth;
				float NodeY = NodeStartY + Row * NodeHeight;
				
				// Kawaii Physics 노드 생성
				UEdGraphNode* KawaiiNode = NewObject<UEdGraphNode>(AnimGraph, KawaiiNodeClass);
				if (KawaiiNode)
				{
					KawaiiNode->NodePosX = NodeX;
					KawaiiNode->NodePosY = NodeY;
					
					// 리플렉션으로 속성 설정
					// Node 구조체 찾기
					FStructProperty* NodeProp = FindFProperty<FStructProperty>(KawaiiNodeClass, TEXT("Node"));
					if (NodeProp)
					{
						void* NodeStructPtr = NodeProp->ContainerPtrToValuePtr<void>(KawaiiNode);
						UScriptStruct* NodeStruct = NodeProp->Struct;
						
						if (NodeStruct && NodeStructPtr)
						{
							// RootBone 설정
							FProperty* RootBoneProp = NodeStruct->FindPropertyByName(TEXT("RootBone"));
							if (RootBoneProp)
							{
								// FBoneReference 타입일 것임
								if (FStructProperty* BoneRefProp = CastField<FStructProperty>(RootBoneProp))
								{
									void* BoneRefPtr = BoneRefProp->ContainerPtrToValuePtr<void>(NodeStructPtr);
									// BoneName 속성 찾기
									FProperty* BoneNameProp = BoneRefProp->Struct->FindPropertyByName(TEXT("BoneName"));
									if (BoneNameProp)
									{
										FNameProperty* NameProp = CastField<FNameProperty>(BoneNameProp);
										if (NameProp)
										{
											NameProp->SetPropertyValue_InContainer(BoneRefPtr, BoneName);
										}
									}
								}
							}
							
							// ExcludeBones 설정 (웨이트 없는 본이 있는 경우)
							if (bHasDeadBones && ExcludeBoneName != NAME_None)
							{
								FProperty* ExcludeBonesProp = NodeStruct->FindPropertyByName(TEXT("ExcludeBones"));
								if (ExcludeBonesProp)
								{
									FArrayProperty* ArrayProp = CastField<FArrayProperty>(ExcludeBonesProp);
									if (ArrayProp)
									{
										FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(NodeStructPtr));
										ArrayHelper.AddValue();
										
										// 배열 요소가 FBoneReference인 경우
										FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner);
										if (InnerStructProp)
										{
											void* ElementPtr = ArrayHelper.GetRawPtr(0);
											FProperty* BoneNameInnerProp = InnerStructProp->Struct->FindPropertyByName(TEXT("BoneName"));
											if (BoneNameInnerProp)
											{
												FNameProperty* NameProp = CastField<FNameProperty>(BoneNameInnerProp);
												if (NameProp)
												{
													NameProp->SetPropertyValue_InContainer(ElementPtr, ExcludeBoneName);
													UE_LOG(LogTemp, Log, TEXT("  Set ExcludeBone: %s"), *ExcludeBoneName.ToString());
												}
											}
										}
									}
								}
							}
							
							// DummyBoneLength 설정 (웨이트 없는 본 있으면 10, 없으면 0)
							FProperty* DummyLengthProp = NodeStruct->FindPropertyByName(TEXT("DummyBoneLength"));
							if (DummyLengthProp)
							{
								FFloatProperty* FloatProp = CastField<FFloatProperty>(DummyLengthProp);
								if (FloatProp)
								{
									float DummyLength = bHasDeadBones ? 10.0f : 0.0f;
									FloatProp->SetPropertyValue_InContainer(NodeStructPtr, DummyLength);
									UE_LOG(LogTemp, Log, TEXT("  Set DummyBoneLength: %.1f"), DummyLength);
								}
							}
							
							// Axis 설정 (웨이트 없는 본 있을 때만 X Positive)
							if (bHasDeadBones)
							{
								FProperty* AxisProp = NodeStruct->FindPropertyByName(TEXT("BoneForwardAxis"));
								if (!AxisProp) AxisProp = NodeStruct->FindPropertyByName(TEXT("DummyBoneRotation"));
								if (AxisProp)
								{
									FByteProperty* ByteProp = CastField<FByteProperty>(AxisProp);
									if (ByteProp)
									{
										ByteProp->SetPropertyValue_InContainer(NodeStructPtr, 0); // X_Positive
										UE_LOG(LogTemp, Log, TEXT("  Set BoneForwardAxis: X_Positive"));
									}
									else
									{
										// Enum property인 경우
										FEnumProperty* EnumProp = CastField<FEnumProperty>(AxisProp);
										if (EnumProp)
										{
											FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
											if (UnderlyingProp)
											{
												UnderlyingProp->SetIntPropertyValue(AxisProp->ContainerPtrToValuePtr<void>(NodeStructPtr), static_cast<int64>(0));
												UE_LOG(LogTemp, Log, TEXT("  Set BoneForwardAxis (Enum): X_Positive"));
											}
										}
									}
								}
							}
						}
					}
					
					// 노드 초기화 및 그래프에 추가
					KawaiiNode->CreateNewGuid();
					KawaiiNode->PostPlacedNewNode();
					KawaiiNode->AllocateDefaultPins();
					AnimGraph->AddNode(KawaiiNode, true, false);
					
					// 핀 정보 출력 (첫 번째 노드만)
					if (KawaiiPhysicsNodes.Num() == 0)
					{
						UE_LOG(LogTemp, Log, TEXT("=== Kawaii Physics Node Pins ==="));
						for (UEdGraphPin* Pin : KawaiiNode->Pins)
						{
							UE_LOG(LogTemp, Log, TEXT("  Pin: %s (Dir: %s, Category: %s)"),
								*Pin->GetName(),
								Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"),
								*Pin->PinType.PinCategory.ToString());
						}
					}
					
					// 핀 노출 설정 - ShowPinForProperties 배열 수정
					UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(KawaiiNode);
					if (AnimNode)
					{
						// ShowPinForProperties 배열에서 원하는 속성의 bShowPin을 true로 설정
						TArray<FName> PropertiesToExpose = {
							TEXT("PhysicsSettings"),
							TEXT("bEnableWind"),
							TEXT("WindScale"),
							TEXT("Gravity")  // ExternalForces = Gravity
						};
						
						for (FOptionalPinFromProperty& OptionalPin : AnimNode->ShowPinForProperties)
						{
							if (PropertiesToExpose.Contains(OptionalPin.PropertyName))
							{
								OptionalPin.bShowPin = true;
								UE_LOG(LogTemp, Log, TEXT("Exposed pin: %s"), *OptionalPin.PropertyName.ToString());
							}
						}
						
						// 핀 재생성 (노출 설정 적용)
						AnimNode->ReconstructNode();
						
						// ============================================================================
						// Get 노드 → Kawaii Physics 노드 핀 직접 연결
						// ============================================================================
						// Get 노드 인덱스: 0=PhysicsSettings, 1=ExternalForces, 2=EnableWind, 3=WindScale
						// Kawaii 핀 이름: PhysicsSettings, Gravity, bEnableWind, WindScale
						TArray<FName> KawaiiPinNames = {
							TEXT("PhysicsSettings"),
							TEXT("Gravity"),        // ExternalForces → Gravity 핀
							TEXT("bEnableWind"),
							TEXT("WindScale")
						};
						
						for (int32 GetIdx = 0; GetIdx < TagGetNodes.Num() && GetIdx < KawaiiPinNames.Num(); ++GetIdx)
						{
							UK2Node_VariableGet* GetNode = TagGetNodes[GetIdx];
							if (!GetNode) continue;
							
							// Get 노드의 출력 핀 찾기
							UEdGraphPin* GetOutputPin = nullptr;
							for (UEdGraphPin* Pin : GetNode->Pins)
							{
								if (Pin->Direction == EGPD_Output && 
									Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
								{
									GetOutputPin = Pin;
									break;
								}
							}
							
							// Kawaii 노드의 입력 핀 찾기
							FName TargetPinName = KawaiiPinNames[GetIdx];
							UEdGraphPin* KawaiiInputPin = AnimNode->FindPin(TargetPinName, EGPD_Input);
							
							// 연결
							if (GetOutputPin && KawaiiInputPin)
							{
								bool bConnected = Schema->TryCreateConnection(GetOutputPin, KawaiiInputPin);
								UE_LOG(LogTemp, Log, TEXT("  Connect %s -> %s: %s"), 
									*GetNode->GetVarName().ToString(), *TargetPinName.ToString(),
									bConnected ? TEXT("SUCCESS") : TEXT("FAILED"));
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("  Cannot connect: GetPin=%s, KawaiiPin=%s"),
									GetOutputPin ? TEXT("Found") : TEXT("NOT FOUND"),
									KawaiiInputPin ? TEXT("Found") : TEXT("NOT FOUND"));
							}
						}
					}
					
					KawaiiPhysicsNodes.Add(KawaiiNode);
					bKawaiiNodesCreated = true;
					
					UE_LOG(LogTemp, Log, TEXT("Created Kawaii Physics node for bone: %s"), *BoneName.ToString());
				}
			}
		}
		
		CommentY += CommentHeight + 80.0f;
		
		// 로그 출력
		UE_LOG(LogTemp, Log, TEXT("Kawaii Tag '%s' has %d bones:"), *CurrentTag.Name, TagBones.Num());
		for (const FName& BoneName : TagBones)
		{
			UE_LOG(LogTemp, Log, TEXT("  - %s"), *BoneName.ToString());
		}
	}
	
	// Kawaii Physics 노드들을 체인으로 연결 (LocalToComponent -> KawaiiNodes -> ComponentToLocal)
	if (bKawaiiNodesCreated && KawaiiPhysicsNodes.Num() > 0)
	{
		// 헬퍼: Pose 핀 찾기 (Component Space 또는 일반)
		auto FindPosePin = [](UEdGraphNode* Node, EEdGraphPinDirection Dir) -> UEdGraphPin* {
			// 우선순위: ComponentPose > Component Pose > Pose 포함 이름 > 첫 번째 핀
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction == Dir)
				{
					FString PinName = Pin->GetName();
					if (PinName.Contains(TEXT("ComponentPose")) || PinName.Contains(TEXT("Component Pose")))
					{
						return Pin;
					}
				}
			}
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction == Dir && Pin->GetName().Contains(TEXT("Pose")))
				{
					return Pin;
				}
			}
			// 첫 번째 핀 반환
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction == Dir)
				{
					return Pin;
				}
			}
			return nullptr;
		};
		
		// 이전 직접 연결 해제
		if (LocalToCompOutputPin && LocalToCompOutputPin->LinkedTo.Num() > 0)
		{
			LocalToCompOutputPin->BreakAllPinLinks();
		}
		
		// 첫 번째 Kawaii 노드의 모든 핀 출력 (디버그)
		UE_LOG(LogTemp, Log, TEXT("=== First Kawaii Physics Node Pins ==="));
		for (UEdGraphPin* Pin : KawaiiPhysicsNodes[0]->Pins)
		{
			UE_LOG(LogTemp, Log, TEXT("  Pin: %s (Dir: %s, Type: %s)"),
				*Pin->GetName(),
				Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"),
				*Pin->PinType.PinCategory.ToString());
		}
		
		// 첫 번째 Kawaii 노드를 LocalToComponent에 연결
		UEdGraphPin* FirstKawaiiInputPin = FindPosePin(KawaiiPhysicsNodes[0], EGPD_Input);
		if (LocalToCompOutputPin && FirstKawaiiInputPin)
		{
			bool bConnected = Schema->TryCreateConnection(LocalToCompOutputPin, FirstKawaiiInputPin);
			UE_LOG(LogTemp, Log, TEXT("LocalToComponent -> FirstKawaii: %s (%s -> %s)"),
				bConnected ? TEXT("SUCCESS") : TEXT("FAILED"),
				*LocalToCompOutputPin->GetName(), *FirstKawaiiInputPin->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to find pins for LocalToComponent -> FirstKawaii"));
		}
		
		// Kawaii 노드들 순차 연결
		for (int32 i = 0; i < KawaiiPhysicsNodes.Num() - 1; ++i)
		{
			UEdGraphPin* CurrentOutput = FindPosePin(KawaiiPhysicsNodes[i], EGPD_Output);
			UEdGraphPin* NextInput = FindPosePin(KawaiiPhysicsNodes[i + 1], EGPD_Input);
			if (CurrentOutput && NextInput)
			{
				bool bConnected = Schema->TryCreateConnection(CurrentOutput, NextInput);
				UE_LOG(LogTemp, Log, TEXT("Kawaii[%d] -> Kawaii[%d]: %s"), i, i+1, bConnected ? TEXT("SUCCESS") : TEXT("FAILED"));
			}
		}
		
		// 마지막 Kawaii 노드를 ComponentToLocal에 연결
		UEdGraphPin* LastKawaiiOutputPin = FindPosePin(KawaiiPhysicsNodes.Last(), EGPD_Output);
		if (LastKawaiiOutputPin && CompToLocalInputPin)
		{
			bool bConnected = Schema->TryCreateConnection(LastKawaiiOutputPin, CompToLocalInputPin);
			UE_LOG(LogTemp, Log, TEXT("LastKawaii -> ComponentToLocal: %s (%s -> %s)"),
				bConnected ? TEXT("SUCCESS") : TEXT("FAILED"),
				*LastKawaiiOutputPin->GetName(), *CompToLocalInputPin->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to find pins for LastKawaii -> ComponentToLocal"));
		}
	}
	else
	{
		// Kawaii Physics 노드가 없으면 직접 연결 (기존 로직)
		if (LocalToCompOutputPin && CompToLocalInputPin)
		{
			Schema->TryCreateConnection(LocalToCompOutputPin, CompToLocalInputPin);
		}
	}
	
	// ============================================================================
	// KawaiiAlpha 변수 Get 노드 생성 -> Two Way Blend Alpha 핀에 연결
	// (B 영역 이후에 실행되어야 함)
	// ============================================================================
	UE_LOG(LogTemp, Log, TEXT("=== Creating KawaiiAlpha Get Node (after B-area) ==="));
	
	// Two Way Blend의 Alpha 핀 찾기 (Bool -> Float 자동 변환됨)
	UEdGraphPin* AlphaPin = nullptr;
	for (UEdGraphPin* Pin : BlendNode->Pins)
	{
		FString PinName = Pin->GetName();
		UE_LOG(LogTemp, Log, TEXT("  BlendNode Pin: %s (Dir: %s)"), *PinName, Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"));
		if (Pin->Direction == EGPD_Input && PinName == TEXT("Alpha"))
		{
			AlphaPin = Pin;
			UE_LOG(LogTemp, Log, TEXT("  >>> Found Alpha pin: %s"), *PinName);
			break;  // 정확한 핀을 찾으면 바로 종료
		}
	}
	
	if (AlphaPin)
	{
		// KawaiiAlpha Get 노드 생성
		UK2Node_VariableGet* KawaiiAlphaGetNode = NewObject<UK2Node_VariableGet>(AnimGraph);
		if (KawaiiAlphaGetNode)
		{
			KawaiiAlphaGetNode->NodePosX = BlendNode->NodePosX - 120;
			KawaiiAlphaGetNode->NodePosY = BlendNode->NodePosY + 100;
			
			KawaiiAlphaGetNode->VariableReference.SetSelfMember(FName(TEXT("KawaiiAlpha")));
			
			KawaiiAlphaGetNode->CreateNewGuid();
			KawaiiAlphaGetNode->PostPlacedNewNode();
			KawaiiAlphaGetNode->AllocateDefaultPins();
			AnimGraph->AddNode(KawaiiAlphaGetNode, true, false);
			
			UE_LOG(LogTemp, Log, TEXT("Created KawaiiAlpha Get node at (%d, %d)"), 
				KawaiiAlphaGetNode->NodePosX, KawaiiAlphaGetNode->NodePosY);
			
			// Get 노드의 출력 핀 찾기
			UEdGraphPin* GetOutputPin = nullptr;
			for (UEdGraphPin* Pin : KawaiiAlphaGetNode->Pins)
			{
				if (Pin->Direction == EGPD_Output)
				{
					GetOutputPin = Pin;
					UE_LOG(LogTemp, Log, TEXT("  Found KawaiiAlpha Get output pin: %s"), *Pin->GetName());
					break;
				}
			}
			
			// 연결
			if (GetOutputPin)
			{
				bool bConnected = Schema->TryCreateConnection(GetOutputPin, AlphaPin);
				if (!bConnected)
				{
					bConnected = Schema->CreateAutomaticConversionNodeAndConnections(GetOutputPin, AlphaPin);
				}
				UE_LOG(LogTemp, Log, TEXT("KawaiiAlpha (Bool) -> TwoWayBlend.Alpha (Float, auto-converted): %s"), 
					bConnected ? TEXT("SUCCESS") : TEXT("FAILED"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("KawaiiAlpha Get output pin not found"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TwoWayBlend Alpha pin not found"));
	}
	
	// ============================================================================
	// 8. 블루프린트 컴파일 및 저장
	// ============================================================================
	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	
	FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
	
	// 패키지 저장
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
	UPackage::SavePackage(Package, AnimBP, *PackageFileName, SaveArgs);
	
	// 에셋 레지스트리 알림
	FAssetRegistryModule::AssetCreated(AnimBP);
	
	// ============================================================================
	// 9. 결과 보고
	// ============================================================================
	FString Summary = FString::Printf(TEXT("AnimBP created successfully!\n\nPath: %s\n\n"), *NewAssetPath);
	Summary += TEXT("A영역 기본 노드:\n");
	Summary += TEXT("  Slot → SaveCachedPose → UseCachedPose → LocalToComponent → ComponentToLocal → TwoWayBlend → OutputPose\n\n");
	Summary += TEXT("B영역 태그별 설정:\n");
	
	for (auto& Pair : TaggedBones)
	{
		if (Pair.Key >= 0 && Pair.Key < KawaiiTags.Num())
		{
			Summary += FString::Printf(TEXT("  - %s: %d bones\n"), *KawaiiTags[Pair.Key].Name, Pair.Value.Num());
		}
	}
	
	if (bKawaiiNodesCreated)
	{
		Summary += FString::Printf(TEXT("\n✓ Kawaii Physics 노드 %d개 자동 생성됨\n"), KawaiiPhysicsNodes.Num());
		Summary += TEXT("  - 노드들이 체인으로 연결되어 있습니다.\n");
		Summary += TEXT("  - Physics Settings, Enable Wind 등 핀을 변수에 연결하세요.\n");
	}
	else if (bKawaiiPhysicsAvailable && !KawaiiNodeClass)
	{
		Summary += TEXT("\n⚠ Kawaii Physics 플러그인은 로드되었으나 노드 클래스를 찾을 수 없습니다.\n");
		Summary += TEXT("  코멘트 박스 위치에 수동으로 노드를 추가해주세요.\n");
	}
	else
	{
		Summary += TEXT("\n※ Kawaii Physics 플러그인이 로드되지 않았습니다.\n");
		Summary += TEXT("  플러그인을 활성화한 후 에디터를 재시작하면\n");
		Summary += TEXT("  노드가 자동으로 생성됩니다.\n");
	}
	Summary += TEXT("\n코멘트 박스에 태그별 위치가 표시되어 있습니다.");
	
	SetKawaiiStatus(TEXT("AnimBP created: ") + NewAssetPath);
	
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Summary));
	
	// Content Browser에서 열기
	TArray<UObject*> ObjectsToSync;
	ObjectsToSync.Add(AnimBP);
	GEditor->SyncBrowserToObjects(ObjectsToSync);
	
	return true;
}

// ============================================================================
// Physics Asset 탭 콘텐츠
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::CreatePhysicsAssetTab()
{
	// 주황/노랑 계열 색상
	const FLinearColor AccentColor(0.9f, 0.6f, 0.2f, 1.0f);         // 주황색 강조
	const FLinearColor AccentColorDark(0.7f, 0.45f, 0.15f, 1.0f);   // 어두운 주황
	const FLinearColor SectionBg(0.04f, 0.035f, 0.03f, 1.0f);       // 따뜻한 배경
	const FLinearColor TextMuted(0.55f, 0.55f, 0.5f, 1.0f);
	const FLinearColor TextBright(0.95f, 0.9f, 0.85f, 1.0f);
	
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			
			// ========== 섹션 1: 스켈레탈 메쉬 선택 ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(SectionBg)
				.Padding(16)
				[
					SNew(SVerticalBox)
					// 섹션 헤더
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("PhysicsAssetEditor.Tabs.Body"))
							.ColorAndOpacity(AccentColor)
							.DesiredSizeOverride(FVector2D(18, 18))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PhysAsset_MeshSection", "Skeletal Mesh"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(TextBright)
						]
					]
					// 드롭다운 + 화살표
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						// 썸네일
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
						[
							SAssignNew(PhysAssetMeshThumbnailBox, SBox)
							.WidthOverride(64)
							.HeightOverride(64)
						]
						// 드롭다운
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
						SAssignNew(PhysAssetMeshComboBox, SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&MeshOptions)
							.OnSelectionChanged(this, &SControlRigToolWidget::OnPhysAssetMeshSelectionChanged)
							.OnGenerateWidget(this, &SControlRigToolWidget::OnGeneratePhysAssetMeshWidget)
							.Content()
							[
								SNew(STextBlock)
								.Text(this, &SControlRigToolWidget::GetSelectedPhysAssetMeshName)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
							]
						]
						// 화살표 버튼
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "FlatButton")
							.ContentPadding(FMargin(8, 8))
							.ToolTipText(LOCTEXT("PhysAsset_UseSelected", "Use selected asset from Content Browser"))
							.OnClicked(this, &SControlRigToolWidget::OnUseSelectedPhysAssetMeshClicked)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.ArrowLeft"))
								.ColorAndOpacity(AccentColor)
								.DesiredSizeOverride(FVector2D(20, 20))
							]
						]
					]
				]
			]
			
			// ========== 섹션 2: 본 매핑 ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(SectionBg)
				.Padding(16)
				[
					SNew(SVerticalBox)
					// 섹션 헤더
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("SkeletonTree.Bone"))
							.ColorAndOpacity(AccentColor)
							.DesiredSizeOverride(FVector2D(18, 18))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PhysAsset_BoneMappingSection", "Bone Mapping (Main Bones Only)"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(TextBright)
						]
					]
					// AI 본 매핑 버튼
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton")
						.ContentPadding(FMargin(16, 10))
						.OnClicked(this, &SControlRigToolWidget::OnPhysAssetBoneMappingClicked)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
							.BorderBackgroundColor(AccentColorDark)
							.Padding(FMargin(16, 8))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("Icons.Visible"))
									.ColorAndOpacity(FLinearColor::White)
									.DesiredSizeOverride(FVector2D(16, 16))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("PhysAsset_AIBoneMapping", "AI Bone Mapping"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
									.ColorAndOpacity(FLinearColor::White)
								]
							]
						]
					]
					// 메인 본 목록
					+ SVerticalBox::Slot().AutoHeight().MaxHeight(250)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(PhysAssetBoneListBox, SVerticalBox)
						]
					]
				]
			]
			
			// ========== 섹션 3: 출력 설정 ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(SectionBg)
				.Padding(16)
				[
					SNew(SVerticalBox)
					// 섹션 헤더
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("Icons.Save"))
							.ColorAndOpacity(AccentColor)
							.DesiredSizeOverride(FVector2D(18, 18))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PhysAsset_OutputSection", "Output Settings"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(TextBright)
						]
					]
					// 출력 이름
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PhysAsset_OutputName", "Name:"))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							.ColorAndOpacity(TextMuted)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(PhysAssetOutputNameBox, SEditableTextBox)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							.HintText(LOCTEXT("PhysAsset_OutputNameHint", "PA_MeshName"))
						]
					]
					// 출력 폴더
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PhysAsset_OutputFolder", "Folder:"))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							.ColorAndOpacity(TextMuted)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(PhysAssetOutputFolderBox, SEditableTextBox)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							.Text(FText::FromString(PhysAssetDefaultOutputFolder))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "FlatButton")
							.ContentPadding(4)
							.OnClicked(this, &SControlRigToolWidget::OnPhysAssetBrowseFolderClicked)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.FolderOpen"))
								.ColorAndOpacity(TextMuted)
								.DesiredSizeOverride(FVector2D(14, 14))
							]
						]
					]
				]
			]
			
			// ========== 섹션 4: 생성 버튼 ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(SectionBg)
				.Padding(16)
				[
					SNew(SVerticalBox)
					// 생성 버튼
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton")
						.ContentPadding(FMargin(24, 14))
						.OnClicked(this, &SControlRigToolWidget::OnCreatePhysicsAssetClicked)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
							.BorderBackgroundColor(AccentColor)
							.Padding(FMargin(24, 12))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("PhysicsAssetEditor.Tabs.Body"))
									.ColorAndOpacity(FLinearColor::White)
									.DesiredSizeOverride(FVector2D(18, 18))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("PhysAsset_CreateButton", "Generate Physics Asset"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
									.ColorAndOpacity(FLinearColor::White)
								]
							]
						]
					]
				]
			]
			
			// ========== 상태 표시 ==========
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(PhysAssetStatusText, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(TextMuted)
			]
		];
}

// ============================================================================
// Physics Asset UI 함수들
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::OnGeneratePhysAssetMeshWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InItem))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11));
}

void SControlRigToolWidget::OnPhysAssetMeshSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	SelectedPhysAssetMesh = NewValue;
	UpdatePhysAssetMeshThumbnail();
	
	// 자동 이름 설정
	if (NewValue.IsValid() && PhysAssetOutputNameBox.IsValid())
	{
		FString MeshName = *NewValue;
		// SM_, SK_ 등 접두사 제거
		if (MeshName.StartsWith(TEXT("SM_")) || MeshName.StartsWith(TEXT("SK_")))
		{
			MeshName = MeshName.RightChop(3);
		}
		PhysAssetOutputNameBox->SetText(FText::FromString(FString::Printf(TEXT("PA_%s"), *MeshName)));
	}
	
	// 매핑 초기화
	PhysAssetBoneMapping.Empty();
	PhysAssetMainBones.Empty();
	UpdatePhysAssetBoneListUI();
	
	if (NewValue.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[PhysicsAsset] Selected mesh: %s"), **NewValue);
	}
}

FText SControlRigToolWidget::GetSelectedPhysAssetMeshName() const
{
	if (SelectedPhysAssetMesh.IsValid())
	{
		return FText::FromString(*SelectedPhysAssetMesh);
	}
	return LOCTEXT("PhysAsset_SelectMesh", "Select Skeletal Mesh...");
}

FReply SControlRigToolWidget::OnUseSelectedPhysAssetMeshClicked()
{
	// Content Browser에서 선택된 에셋 사용
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
		{
			FString AssetName = Asset.AssetName.ToString();
			for (TSharedPtr<FString>& Option : MeshOptions)
			{
				if (*Option == AssetName)
				{
					SelectedPhysAssetMesh = Option;
					if (PhysAssetMeshComboBox.IsValid())
					{
						PhysAssetMeshComboBox->SetSelectedItem(Option);
					}
					OnPhysAssetMeshSelectionChanged(Option, ESelectInfo::Direct);
					return FReply::Handled();
				}
			}
		}
	}
	
	SetPhysAssetStatus(TEXT("No Skeletal Mesh selected in Content Browser"));
	return FReply::Handled();
}

void SControlRigToolWidget::UpdatePhysAssetMeshThumbnail()
{
	if (!PhysAssetMeshThumbnailBox.IsValid()) return;
	
	TSharedPtr<SWidget> ThumbnailWidget = SNullWidget::NullWidget;
	
	if (SelectedPhysAssetMesh.IsValid())
	{
		FString AssetPath;
		for (const FAssetInfo& MeshInfo : SkeletalMeshes)
		{
			if (MeshInfo.Name == *SelectedPhysAssetMesh)
			{
				AssetPath = MeshInfo.Path;
				break;
			}
		}
		
		if (!AssetPath.IsEmpty())
		{
			PhysAssetMeshThumbnail = MakeShared<FAssetThumbnail>(
				LoadObject<UObject>(nullptr, *AssetPath),
				64, 64, ThumbnailPool
			);
			
			if (PhysAssetMeshThumbnail.IsValid())
			{
				ThumbnailWidget = PhysAssetMeshThumbnail->MakeThumbnailWidget();
			}
		}
	}
	
	PhysAssetMeshThumbnailBox->SetContent(ThumbnailWidget.ToSharedRef());
}

void SControlRigToolWidget::SetPhysAssetStatus(const FString& Status)
{
	if (PhysAssetStatusText.IsValid())
	{
		PhysAssetStatusText->SetText(FText::FromString(Status));
	}
	UE_LOG(LogTemp, Log, TEXT("[PhysicsAsset] %s"), *Status);
}

FReply SControlRigToolWidget::OnPhysAssetBrowseFolderClicked()
{
	// 폴더 선택 다이얼로그 (간단히 텍스트로 처리)
	return FReply::Handled();
}

void SControlRigToolWidget::UpdatePhysAssetBoneListUI()
{
	if (!PhysAssetBoneListBox.IsValid()) return;
	
	PhysAssetBoneListBox->ClearChildren();
	
	if (PhysAssetMainBones.Num() == 0)
	{
		PhysAssetBoneListBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PhysAsset_NoBones", "Click 'AI Bone Mapping' to detect main bones"))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
			.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
		];
		return;
	}
	
	const FLinearColor AccentColor(0.9f, 0.6f, 0.2f, 1.0f);
	
	for (const FName& BoneName : PhysAssetMainBones)
	{
		FName MappedBone = NAME_None;
		for (auto& Pair : PhysAssetBoneMapping)
		{
			if (Pair.Value == BoneName || Pair.Key == BoneName)
			{
				MappedBone = Pair.Value;
				break;
			}
		}
		
		PhysAssetBoneListBox->AddSlot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("SkeletonTree.Bone"))
				.ColorAndOpacity(AccentColor)
				.DesiredSizeOverride(FVector2D(14, 14))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(BoneName))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(FLinearColor::White)
			]
		];
	}
}

FReply SControlRigToolWidget::OnPhysAssetBoneMappingClicked()
{
	if (!SelectedPhysAssetMesh.IsValid())
	{
		SetPhysAssetStatus(TEXT("Please select a Skeletal Mesh first"));
		return FReply::Handled();
	}
	
	SetPhysAssetStatus(TEXT("Running AI Bone Mapping..."));
	
	// 기존 Control Rig 탭의 AI 본 매핑 로직 재사용
	// LastBoneMapping에서 결과를 가져옴
	if (LastBoneMapping.Num() > 0)
	{
		PhysAssetBoneMapping = LastBoneMapping;
		PhysAssetMainBones.Empty();
		
		for (auto& Pair : LastBoneMapping)
		{
			PhysAssetMainBones.Add(Pair.Value);
		}
		
		UpdatePhysAssetBoneListUI();
		SetPhysAssetStatus(FString::Printf(TEXT("Found %d main bones from existing mapping"), PhysAssetMainBones.Num()));
		return FReply::Handled();
	}
	
	// 새로 매핑 실행 (간단히 API 호출)
	// 여기서는 기존 OnAIBoneMappingClicked 로직을 공유하거나 호출할 수 있음
	// 우선 간단하게 처리
	
	// 메쉬 로드
	USkeletalMesh* TargetMesh = nullptr;
	for (const FAssetInfo& MeshInfo : SkeletalMeshes)
	{
		if (MeshInfo.Name == *SelectedPhysAssetMesh)
		{
			TargetMesh = Cast<USkeletalMesh>(LoadObject<USkeletalMesh>(nullptr, *MeshInfo.Path));
			break;
		}
	}
	
	if (!TargetMesh)
	{
		SetPhysAssetStatus(TEXT("Failed to load skeletal mesh"));
		return FReply::Handled();
	}
	
	// 본 정보 수집 및 API 호출
	const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
	TArray<TSharedPtr<FJsonValue>> BonesArray;
	
	for (int32 i = 0; i < RefSkeleton.GetNum(); ++i)
	{
		TSharedPtr<FJsonObject> BoneObj = MakeShared<FJsonObject>();
		FName BoneName = RefSkeleton.GetBoneName(i);
		int32 ParentIdx = RefSkeleton.GetParentIndex(i);
		
		BoneObj->SetStringField(TEXT("name"), BoneName.ToString());
		BoneObj->SetStringField(TEXT("parent"), ParentIdx >= 0 ? RefSkeleton.GetBoneName(ParentIdx).ToString() : TEXT(""));
		
		// 자식 본 찾기
		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		for (int32 j = 0; j < RefSkeleton.GetNum(); ++j)
		{
			if (RefSkeleton.GetParentIndex(j) == i)
			{
				ChildrenArray.Add(MakeShared<FJsonValueString>(RefSkeleton.GetBoneName(j).ToString()));
			}
		}
		BoneObj->SetArrayField(TEXT("children"), ChildrenArray);
		
		BonesArray.Add(MakeShared<FJsonValueObject>(BoneObj));
	}
	
	// API 요청
	TSharedPtr<FJsonObject> RequestObj = MakeShared<FJsonObject>();
	RequestObj->SetArrayField(TEXT("bones"), BonesArray);
	
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);
	
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("http://localhost:8000/predict"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);
	
	HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
		{
			TSharedPtr<FJsonObject> JsonResponse;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			
			if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
			{
				const TSharedPtr<FJsonObject>* MappingObj;
				if (JsonResponse->TryGetObjectField(TEXT("mapping"), MappingObj))
				{
					PhysAssetBoneMapping.Empty();
					PhysAssetMainBones.Empty();
					
					for (auto& Pair : (*MappingObj)->Values)
					{
						FName StandardBone = FName(*Pair.Key);
						FName MeshBone = FName(*Pair.Value->AsString());
						PhysAssetBoneMapping.Add(StandardBone, MeshBone);
						PhysAssetMainBones.Add(MeshBone);
					}
					
					AsyncTask(ENamedThreads::GameThread, [this]()
					{
						UpdatePhysAssetBoneListUI();
						SetPhysAssetStatus(FString::Printf(TEXT("Found %d main bones"), PhysAssetMainBones.Num()));
					});
				}
			}
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				SetPhysAssetStatus(TEXT("API Error - Make sure API server is running"));
			});
		}
	});
	
	HttpRequest->ProcessRequest();
	
	return FReply::Handled();
}

FReply SControlRigToolWidget::OnCreatePhysicsAssetClicked()
{
	if (PhysAssetMainBones.Num() == 0)
	{
		SetPhysAssetStatus(TEXT("Please run AI Bone Mapping first"));
		return FReply::Handled();
	}
	
	if (CreatePhysicsAsset())
	{
		SetPhysAssetStatus(TEXT("Physics Asset created successfully!"));
	}
	
	return FReply::Handled();
}

bool SControlRigToolWidget::CreatePhysicsAsset()
{
	SetPhysAssetStatus(TEXT("Creating Physics Asset..."));
	
	if (!SelectedPhysAssetMesh.IsValid())
	{
		SetPhysAssetStatus(TEXT("No mesh selected"));
		return false;
	}
	
	// 1. 스켈레탈 메쉬 로드
	USkeletalMesh* TargetMesh = nullptr;
	for (const FAssetInfo& MeshInfo : SkeletalMeshes)
	{
		if (MeshInfo.Name == *SelectedPhysAssetMesh)
		{
			TargetMesh = Cast<USkeletalMesh>(LoadObject<USkeletalMesh>(nullptr, *MeshInfo.Path));
			break;
		}
	}
	
	if (!TargetMesh)
	{
		SetPhysAssetStatus(TEXT("Failed to load skeletal mesh"));
		return false;
	}
	
	// 2. 출력 경로 설정
	FString OutputName = PhysAssetOutputNameBox.IsValid() ? 
		PhysAssetOutputNameBox->GetText().ToString() : 
		FString::Printf(TEXT("PA_%s"), *TargetMesh->GetName());
	
	if (OutputName.IsEmpty())
	{
		OutputName = FString::Printf(TEXT("PA_%s"), *TargetMesh->GetName());
	}
	
	FString OutputFolder = PhysAssetOutputFolderBox.IsValid() ? 
		PhysAssetOutputFolderBox->GetText().ToString() : 
		PhysAssetDefaultOutputFolder;
	
	FString PackagePath = OutputFolder / OutputName;
	FString PackageName = FPackageName::ObjectPathToPackageName(PackagePath);
	
	// 3. 기존 에셋 삭제 (있는 경우)
	UPackage* ExistingPackage = FindPackage(nullptr, *PackageName);
	if (ExistingPackage)
	{
		UPhysicsAsset* ExistingAsset = FindObject<UPhysicsAsset>(ExistingPackage, *OutputName);
		if (ExistingAsset)
		{
			UE_LOG(LogTemp, Log, TEXT("[PhysicsAsset] Deleting existing asset: %s"), *PackageName);
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(ExistingAsset);
			
			TArray<UObject*> ObjectsToDelete;
			ObjectsToDelete.Add(ExistingAsset);
			ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
		}
	}
	
	// 4. 새 패키지 생성
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		SetPhysAssetStatus(TEXT("Failed to create package"));
		return false;
	}
	
	// 5. Physics Asset 생성
	UPhysicsAsset* PhysAsset = NewObject<UPhysicsAsset>(Package, *OutputName, RF_Public | RF_Standalone);
	if (!PhysAsset)
	{
		SetPhysAssetStatus(TEXT("Failed to create Physics Asset"));
		return false;
	}
	
	// 6. 스켈레톤 정보 가져오기
	const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
	const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();
	
	// 6.5 버텍스 기반 본 크기 계산 (메쉬 두께 반영)
	TArray<FBoneVertInfo> BoneVertInfos;
	FMeshUtilitiesEngine::CalcBoneVertInfos(TargetMesh, BoneVertInfos, true);
	UE_LOG(LogTemp, Log, TEXT("[PhysicsAsset] Calculated vertex info for %d bones"), BoneVertInfos.Num());
	
	int32 BodiesCreated = 0;
	
	// 7. 각 메인 본에 대해 BodySetup 생성
	for (const FName& BoneName : PhysAssetMainBones)
	{
		// Root 본은 캡슐 생성 제외
		FString RootCheckName = BoneName.ToString().ToLower();
		if (RootCheckName.Contains(TEXT("root")))
		{
			UE_LOG(LogTemp, Log, TEXT("[PhysicsAsset] Skipping root bone: %s"), *BoneName.ToString());
			continue;
		}
		
		int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PhysicsAsset] Bone not found: %s"), *BoneName.ToString());
			continue;
		}
		
		// 부모가 없는 본(진짜 루트)도 제외
		int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		if (ParentIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Log, TEXT("[PhysicsAsset] Skipping root bone (no parent): %s"), *BoneName.ToString());
			continue;
		}
		
		// 본 길이 계산 (자식 본까지의 평균 거리)
		float BoneLength = 10.0f; // 기본값
		float BoneRadius = 5.0f;  // 기본 반지름
		
		// ★ 버텍스 기반 크기 계산 (메쉬 두께 반영)
		float VertexBasedRadius = 5.0f;
		float VertexBasedLength = 10.0f;
		FVector VertexCenter = FVector::ZeroVector;
		FVector VertexBoxSize = FVector::ZeroVector;
		bool bHasVertexInfo = false;
		
		if (BoneIndex < BoneVertInfos.Num() && BoneVertInfos[BoneIndex].Positions.Num() > 0)
		{
			const FBoneVertInfo& VertInfo = BoneVertInfos[BoneIndex];
			bHasVertexInfo = true;
			
			// 버텍스 바운딩 박스 계산
			FBox VertexBox(ForceInit);
			for (const FVector3f& Pos : VertInfo.Positions)
			{
				VertexBox += FVector(Pos);
			}
			
			VertexBoxSize = VertexBox.GetSize();
			VertexCenter = VertexBox.GetCenter();
			
			// 가장 긴 축을 길이로, 나머지 두 축 평균을 반지름으로
			float MaxAxis = FMath::Max3(VertexBoxSize.X, VertexBoxSize.Y, VertexBoxSize.Z);
			float SumOtherAxes = VertexBoxSize.X + VertexBoxSize.Y + VertexBoxSize.Z - MaxAxis;
			
			VertexBasedLength = MaxAxis;
			VertexBasedRadius = SumOtherAxes * 0.25f; // 나머지 두 축 평균의 절반
			VertexBasedRadius = FMath::Clamp(VertexBasedRadius, 2.0f, MaxAxis * 0.4f); // 반지름이 길이보다 크지 않도록
			
			UE_LOG(LogTemp, Log, TEXT("[PhysicsAsset] %s vertex box: (%.1f, %.1f, %.1f), length=%.1f, radius=%.1f"), 
				*BoneName.ToString(), VertexBoxSize.X, VertexBoxSize.Y, VertexBoxSize.Z, VertexBasedLength, VertexBasedRadius);
		}
		
		// 자식 본들의 위치를 확인해서 길이 계산
		TArray<int32> ChildIndices;
		for (int32 i = 0; i < RefSkeleton.GetNum(); ++i)
		{
			if (RefSkeleton.GetParentIndex(i) == BoneIndex)
			{
				ChildIndices.Add(i);
			}
		}
		
		// 자식 본 방향 계산 (캡슐 방향 결정용)
		FVector BoneDirection = FVector::XAxisVector; // 기본값: X축
		
		if (ChildIndices.Num() > 0)
		{
			// 자식 본들까지의 평균 거리 및 방향 계산
			FVector AvgChildDirection = FVector::ZeroVector;
			float TotalLength = 0.0f;
			
			for (int32 ChildIndex : ChildIndices)
			{
				FVector ChildLocalPos = RefBonePose[ChildIndex].GetLocation();
				float ChildDist = ChildLocalPos.Size();
				TotalLength += ChildDist;
				
				if (ChildDist > KINDA_SMALL_NUMBER)
				{
					AvgChildDirection += ChildLocalPos.GetSafeNormal();
				}
			}
			
			BoneLength = FMath::Max(TotalLength / ChildIndices.Num(), 5.0f);
			
			// 평균 방향 계산
			if (!AvgChildDirection.IsNearlyZero())
			{
				BoneDirection = AvgChildDirection.GetSafeNormal();
			}
		}
		else
		{
			// 자식 본이 없으면 자신의 로컬 위치 방향 사용 (부모로부터의 방향)
			FVector BoneLocalPos = RefBonePose[BoneIndex].GetLocation();
			BoneLength = FMath::Max(BoneLocalPos.Size() * 0.5f, 5.0f);
			
			if (!BoneLocalPos.IsNearlyZero())
			{
				BoneDirection = BoneLocalPos.GetSafeNormal();
			}
		}
		
		// 본 이름에 따라 회전 고정 및 크기 제한 결정
		FString BoneNameStr = BoneName.ToString().ToLower();
		bool bForceZeroRotation = false; // spine 등은 회전 0으로 고정
		float MaxRadiusLimit = 30.0f; // 기본 최대 반지름
		
		// pelvis, spine 계열만 회전 0으로 고정 (세로 방향 몸통)
		if (BoneNameStr.Contains(TEXT("spine")) || BoneNameStr.Contains(TEXT("pelvis")) || 
		    BoneNameStr.Contains(TEXT("hips")))
		{
			bForceZeroRotation = true;
		}
		
		// 본 이름별 반지름 범위 설정
		float MinRadiusLimit = 4.0f; // 기본 최소 반지름
		
		if (BoneNameStr.Contains(TEXT("finger")) || BoneNameStr.Contains(TEXT("thumb")) ||
		    BoneNameStr.Contains(TEXT("index")) || BoneNameStr.Contains(TEXT("middle")) ||
		    BoneNameStr.Contains(TEXT("ring")) || BoneNameStr.Contains(TEXT("pinky")))
		{
			MinRadiusLimit = 1.5f;
			MaxRadiusLimit = 3.0f; // 손가락
		}
		else if (BoneNameStr.Contains(TEXT("hand")) || BoneNameStr.Contains(TEXT("wrist")))
		{
			MinRadiusLimit = 3.0f;
			MaxRadiusLimit = 6.0f; // 손목/손
		}
		else if (BoneNameStr.Contains(TEXT("clavicle")) || BoneNameStr.Contains(TEXT("shoulder")))
		{
			MinRadiusLimit = 3.0f;
			MaxRadiusLimit = 5.0f; // 쇄골 (얇게)
		}
		else if (BoneNameStr.Contains(TEXT("spine")))
		{
			MinRadiusLimit = 5.0f;
			MaxRadiusLimit = 10.0f; // spine 반지름 제한 (너무 크지 않게)
		}
		else if (BoneNameStr.Contains(TEXT("upperarm")) || BoneNameStr.Contains(TEXT("upper_arm")))
		{
			MinRadiusLimit = 6.0f; // upperarm 더 크게
			MaxRadiusLimit = 15.0f;
		}
		else if (BoneNameStr.Contains(TEXT("forearm")) || BoneNameStr.Contains(TEXT("lowerarm")) || BoneNameStr.Contains(TEXT("lower_arm")))
		{
			MinRadiusLimit = 5.0f;
			MaxRadiusLimit = 12.0f;
		}
		
		// ★ 길이/반지름 계산: 버텍스 기반 우선, 없으면 본 길이 기반 폴백
		if (bHasVertexInfo)
		{
			// 버텍스 기반 사용 (메쉬 실제 크기 반영)
			BoneLength = VertexBasedLength;
			BoneRadius = VertexBasedRadius;
		}
		else
		{
			// 버텍스 정보 없으면 본 길이의 25%
			BoneRadius = FMath::Clamp(BoneLength * 0.25f, 3.0f, 20.0f);
		}
		
		// 캡슐 방향, 길이, 반지름, 중심 계산
		FRotator CapsuleRotator;
		FVector CapsuleCenter;
		float CapsuleLength;
		
		// ★ 캡슐 방향 결정
		if (bForceZeroRotation)
		{
			// spine/pelvis는 항상 Z축 정렬 (회전 없음)
			CapsuleRotator = FRotator::ZeroRotator;
		}
		else
		{
			// 다른 본은 자식 본 방향 기반
			FQuat CapsuleRotation = FQuat::FindBetweenNormals(FVector::ZAxisVector, BoneDirection);
			CapsuleRotator = CapsuleRotation.Rotator();
		}
		
		if (bHasVertexInfo)
		{
			// ★ 버텍스 박스에서 크기 가져오기
			float LengthAxis, Axis1, Axis2;
			
			if (bForceZeroRotation)
			{
				// spine/pelvis는 항상 Z축이 길이
				LengthAxis = VertexBoxSize.Z;
				Axis1 = VertexBoxSize.X;
				Axis2 = VertexBoxSize.Y;
			}
			else
			{
				// 본 방향 축의 크기 = 길이
				FVector AbsDir = BoneDirection.GetAbs();
				if (AbsDir.X >= AbsDir.Y && AbsDir.X >= AbsDir.Z)
				{
					LengthAxis = VertexBoxSize.X;
					Axis1 = VertexBoxSize.Y;
					Axis2 = VertexBoxSize.Z;
				}
				else if (AbsDir.Y >= AbsDir.X && AbsDir.Y >= AbsDir.Z)
				{
					LengthAxis = VertexBoxSize.Y;
					Axis1 = VertexBoxSize.X;
					Axis2 = VertexBoxSize.Z;
				}
				else
				{
					LengthAxis = VertexBoxSize.Z;
					Axis1 = VertexBoxSize.X;
					Axis2 = VertexBoxSize.Y;
				}
			}
			
			// 길이와 반지름 계산
			float OriginalBoneLength = BoneLength; // 자식 본까지의 거리 (원래 값 보존)
			BoneLength = FMath::Max(LengthAxis, 5.0f);
			BoneRadius = FMath::Max(Axis1, Axis2) * 0.5f;
			
			// ★ upperarm만 특별 처리 (버텍스 박스가 너무 작음)
			if (BoneNameStr.Contains(TEXT("upperarm")) || BoneNameStr.Contains(TEXT("upper_arm")))
			{
				// 버텍스 박스와 본 체인 길이 중 큰 값 사용
				BoneLength = FMath::Max(LengthAxis, OriginalBoneLength);
				
				// 반지름 = 버텍스 박스의 두께 또는 본 길이의 30% 중 큰 값
				float VertexRadius = FMath::Max(Axis1, Axis2) * 0.5f;
				float LengthBasedRadius = BoneLength * 0.30f;
				BoneRadius = FMath::Max(VertexRadius, LengthBasedRadius);
				BoneRadius = FMath::Clamp(BoneRadius, 5.0f, 12.0f); // 5~12 범위
				
				UE_LOG(LogTemp, Warning, TEXT("[PhysicsAsset] UPPERARM %s: VertexBox=(%.1f,%.1f,%.1f), BoneChainLen=%.1f -> Final Length=%.1f, Radius=%.1f"),
					*BoneName.ToString(), VertexBoxSize.X, VertexBoxSize.Y, VertexBoxSize.Z, OriginalBoneLength, BoneLength, BoneRadius);
			}
			
			BoneRadius = FMath::Clamp(BoneRadius, MinRadiusLimit, MaxRadiusLimit); // 본 타입별 최소/최대 적용
			
			// 캡슐 길이 = 전체 길이 - 양쪽 반구 (최소 5 보장으로 구체 방지)
			CapsuleLength = BoneLength - BoneRadius * 2.0f;
			
			// 캡슐 길이가 너무 짧으면 반지름을 줄여서 캡슐 형태 유지
			if (CapsuleLength < 5.0f)
			{
				// 최소 길이 5를 확보하면서 반지름 재계산
				BoneRadius = FMath::Max((BoneLength - 5.0f) * 0.5f, MinRadiusLimit * 0.5f);
				CapsuleLength = FMath::Max(BoneLength - BoneRadius * 2.0f, 5.0f);
			}
			
			// 버텍스 중심을 캡슐 중심으로 사용
			CapsuleCenter = VertexCenter;
			
			UE_LOG(LogTemp, Log, TEXT("[PhysicsAsset] %s: BoxSize=(%.1f,%.1f,%.1f) -> CapsuleLen=%.1f, Radius=%.1f"),
				*BoneName.ToString(), VertexBoxSize.X, VertexBoxSize.Y, VertexBoxSize.Z, CapsuleLength, BoneRadius);
		}
		else
		{
			// 버텍스 정보 없으면 자식 본 방향 기반
			BoneRadius = FMath::Clamp(BoneRadius, MinRadiusLimit, MaxRadiusLimit);
			CapsuleLength = FMath::Max(BoneLength - BoneRadius * 2.0f, 5.0f);
			CapsuleCenter = BoneDirection * (BoneLength * 0.5f);
			
			UE_LOG(LogTemp, Warning, TEXT("[PhysicsAsset] %s: NO VERTEX INFO! Using bone chain. Length=%.1f, Radius=%.1f"),
				*BoneName.ToString(), CapsuleLength, BoneRadius);
		}
		
		// ★★★ 팔/다리 본 강제 처리 (버텍스 유무와 관계없이, 본 방향 기준) ★★★
		bool bForceBoneChain = BoneNameStr.Contains(TEXT("upperarm")) || BoneNameStr.Contains(TEXT("upper_arm")) ||
		                       BoneNameStr.Contains(TEXT("forearm")) || BoneNameStr.Contains(TEXT("lower_arm")) ||
		                       BoneNameStr.Contains(TEXT("thigh")) || BoneNameStr.Contains(TEXT("calf")) ||
		                       BoneNameStr.Contains(TEXT("shin")) || BoneNameStr.Contains(TEXT("lowerleg"));
		
		if (bForceBoneChain)
		{
			// 자식 본까지 거리 다시 계산
			float ChildDist = 0.0f;
			for (int32 i = 0; i < RefSkeleton.GetNum(); ++i)
			{
				if (RefSkeleton.GetParentIndex(i) == BoneIndex)
				{
					FVector ChildPos = RefBonePose[i].GetLocation();
					ChildDist = FMath::Max(ChildDist, ChildPos.Size());
				}
			}
			
			if (ChildDist > 5.0f)
			{
				// 반지름 결정 (본 타입별)
				float RadiusRatio = 0.25f; // 기본
				float MinR = 4.0f, MaxR = 10.0f;
				
				if (BoneNameStr.Contains(TEXT("upperarm")) || BoneNameStr.Contains(TEXT("upper_arm")))
				{
					RadiusRatio = 0.28f; MinR = 5.0f; MaxR = 10.0f;
				}
				else if (BoneNameStr.Contains(TEXT("forearm")) || BoneNameStr.Contains(TEXT("lower_arm")))
				{
					RadiusRatio = 0.22f; MinR = 3.0f; MaxR = 8.0f;
				}
				else if (BoneNameStr.Contains(TEXT("thigh")))
				{
					RadiusRatio = 0.22f; MinR = 5.0f; MaxR = 10.0f; // 조금 얇게
				}
				else if (BoneNameStr.Contains(TEXT("calf")) || BoneNameStr.Contains(TEXT("shin")) || BoneNameStr.Contains(TEXT("lowerleg")))
				{
					RadiusRatio = 0.20f; MinR = 4.0f; MaxR = 8.0f;
				}
				
				// 캡슐 길이 = 자식 본까지 거리의 85%
				CapsuleLength = ChildDist * 0.85f;
				// 반지름 = 길이의 비율
				BoneRadius = CapsuleLength * RadiusRatio;
				BoneRadius = FMath::Clamp(BoneRadius, MinR, MaxR);
				// 중심 = 본 방향으로 절반
				CapsuleCenter = BoneDirection * (ChildDist * 0.5f);
				// 캡슐 방향 = 본 방향 (버텍스 박스 무시)
				FQuat CapsuleRot = FQuat::FindBetweenNormals(FVector::ZAxisVector, BoneDirection);
				CapsuleRotator = CapsuleRot.Rotator();
			}
		}
		
		// 8. SkeletalBodySetup 생성
		USkeletalBodySetup* BodySetup = NewObject<USkeletalBodySetup>(PhysAsset, BoneName, RF_Transactional);
		BodySetup->BoneName = BoneName;
		
		// 콜리전 타입 설정
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		BodySetup->PhysicsType = PhysType_Kinematic;
		
		// 9. 캡슐 콜리전 생성
		FKSphylElem CapsuleElem;
		CapsuleElem.Center = CapsuleCenter;
		CapsuleElem.Rotation = CapsuleRotator;
		CapsuleElem.Radius = BoneRadius;
		CapsuleElem.Length = CapsuleLength;
		CapsuleElem.SetName(BoneName);
		
		// AggGeom에 캡슐 추가
		BodySetup->AggGeom.SphylElems.Add(CapsuleElem);
		
		// BodySetup 완료
		BodySetup->CreatePhysicsMeshes();
		
		// Physics Asset에 추가
		PhysAsset->SkeletalBodySetups.Add(BodySetup);
		
		BodiesCreated++;
		
		UE_LOG(LogTemp, Log, TEXT("[PhysicsAsset] Created body for %s: Length=%.1f, Radius=%.1f, Dir=(%.2f,%.2f,%.2f)"), 
			*BoneName.ToString(), BoneLength, BoneRadius, BoneDirection.X, BoneDirection.Y, BoneDirection.Z);
	}
	
	// 10. Physics Asset 후처리
	PhysAsset->UpdateBoundsBodiesArray();
	PhysAsset->UpdateBodySetupIndexMap();
	
#if WITH_EDITOR
	// Preview mesh 설정
	PhysAsset->PreviewSkeletalMesh = TSoftObjectPtr<USkeletalMesh>(TargetMesh);
#endif
	
	// 11. 패키지 저장
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(PhysAsset);
	
	FString PackageFilePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;
	UPackage::SavePackage(Package, PhysAsset, *PackageFilePath, SaveArgs);
	
	// 12. 에셋 에디터에서 열기
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(PhysAsset);
	
	// Content Browser에서 선택
	TArray<UObject*> ObjectsToSync;
	ObjectsToSync.Add(PhysAsset);
	GEditor->SyncBrowserToObjects(ObjectsToSync);
	
	FString Summary = FString::Printf(TEXT("Physics Asset Created!\n\nPath: %s\nBodies: %d"), *PackagePath, BodiesCreated);
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Summary));
	
	SetPhysAssetStatus(FString::Printf(TEXT("Created: %s (%d bodies)"), *OutputName, BodiesCreated));
	
	return true;
}

// ============================================================================
// Control Rig Sec 탭 (세컨더리 전용 Control Rig)
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::CreateControlRigSecTab()
{
	// 기본값 설정
	SecDefaultOutputFolder = TEXT("/Game/ControlRigs");
	bSecMultipleMode = false;
	
	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(SVerticalBox)
			
			// ========== 모드 선택 버튼 (Single / Multiple) ==========
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.06f, 1.0f))
				.Padding(FMargin(12, 8))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 5, 0)
					[
						SAssignNew(SecSingleModeButton, SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(20, 8))
						.OnClicked(this, &SControlRigToolWidget::OnSecSingleModeClicked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SecSingleMode", "Single"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5, 0, 0, 0)
					[
						SAssignNew(SecMultipleModeButton, SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(20, 8))
						.OnClicked(this, &SControlRigToolWidget::OnSecMultipleModeClicked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SecMultipleMode", "Multiple"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
						]
					]
				]
			]
			
			// ========== 모드별 콘텐츠 (SWidgetSwitcher) ==========
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SAssignNew(SecModeSwitcher, SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return bSecMultipleMode ? 1 : 0; })
				
				// ===== Index 0: Single 모드 =====
				+ SWidgetSwitcher::Slot()
				[
					SNew(SVerticalBox)
					
					// ========== 섹션 1: Skeletal Mesh 선택 ==========
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.05f, 1.0f))
						.Padding(FMargin(12, 8))
						[
							SNew(SVerticalBox)
							// 헤더
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("ClassIcon.SkeletalMesh"))
									.ColorAndOpacity(FLinearColor(0.4f, 0.7f, 1.0f))
									.DesiredSizeOverride(FVector2D(16, 16))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SecMeshSection", "Skeletal Mesh"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
									.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f))
								]
							]
							// 콤보박스 + 버튼
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 5, 0)
								[
									SAssignNew(SecMeshComboBox, SComboBox<TSharedPtr<FString>>)
									.OptionsSource(&MeshOptions)
									.OnSelectionChanged(this, &SControlRigToolWidget::OnSecMeshSelectionChanged)
									.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateSecMeshWidget)
									.Content()
									[
										SNew(STextBlock)
										.Text(this, &SControlRigToolWidget::GetSelectedSecMeshName)
										.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
									]
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SButton)
									.Text(LOCTEXT("SecUseSelected", "←"))
									.ToolTipText(LOCTEXT("SecUseSelectedTooltip", "Use selected asset from Content Browser"))
									.OnClicked(this, &SControlRigToolWidget::OnUseSelectedSecMeshClicked)
								]
							]
							// 썸네일
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0).HAlign(HAlign_Center)
							[
								SAssignNew(SecMeshThumbnailBox, SBox)
								.WidthOverride(ThumbnailSize)
								.HeightOverride(ThumbnailSize)
							]
						]
					]
					
					// ========== Single 모드 - 섹션 2: AI Bone Mapping ==========
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.05f, 0.02f, 0.02f, 1.0f))
						.Padding(FMargin(12, 8))
						[
							SNew(SVerticalBox)
							// 헤더
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("Icons.Search"))
									.ColorAndOpacity(FLinearColor(1.0f, 0.6f, 0.3f))
									.DesiredSizeOverride(FVector2D(16, 16))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SecBoneMappingSection", "Bone Selection"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
									.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f))
								]
							]
							// AI 본 매핑 버튼
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FMargin(20, 8))
								.OnClicked(this, &SControlRigToolWidget::OnSecBoneMappingClicked)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
									[
										SNew(SImage)
										.Image(FAppStyle::GetBrush("Persona.ImportAnimation"))
										.DesiredSizeOverride(FVector2D(16, 16))
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("SecAIMapping", "AI Bone Mapping"))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
									]
								]
							]
							// 본 목록 스크롤 박스
							+ SVerticalBox::Slot().AutoHeight().MaxHeight(400.0f)
							[
								SAssignNew(SecBoneScrollBox, SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(SecBoneListBox, SVerticalBox)
								]
							]
						]
					]
					
					// ========== Single 모드 - 섹션 3: Output Settings ==========
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.02f, 0.04f, 0.02f, 1.0f))
						.Padding(FMargin(12, 8))
						[
							SNew(SVerticalBox)
							// 헤더
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("Icons.Save"))
									.ColorAndOpacity(FLinearColor(0.3f, 0.8f, 0.4f))
									.DesiredSizeOverride(FVector2D(16, 16))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SecOutputSection", "Output Settings"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
									.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f))
								]
							]
							// 이름
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SecOutputName", "Name:"))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
									.ColorAndOpacity(FLinearColor(0.6f, 0.65f, 0.7f))
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f)
								[
									SAssignNew(SecOutputNameBox, SEditableTextBox)
									.Text(FText::FromString(TEXT("CTR_NewRig_Rig")))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								]
							]
							// 폴더
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SecOutputFolder", "Folder:"))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
									.ColorAndOpacity(FLinearColor(0.6f, 0.65f, 0.7f))
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 5, 0)
								[
									SAssignNew(SecOutputFolderBox, SEditableTextBox)
									.Text(FText::FromString(SecDefaultOutputFolder))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SButton)
									.Text(LOCTEXT("SecBrowse", "..."))
									.OnClicked(this, &SControlRigToolWidget::OnSecBrowseFolderClicked)
								]
							]
						]
					]
					
					// ========== Single 모드 - 섹션 4: Create Button ==========
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.03f, 0.05f, 0.08f, 1.0f))
						.Padding(FMargin(12, 8))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SAssignNew(SecCreateButton, SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FMargin(30, 12))
								.IsEnabled(false)
								.OnClicked(this, &SControlRigToolWidget::OnCreateSecControlRigClicked)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
									[
										SNew(SImage)
										.Image(FAppStyle::GetBrush("Icons.Plus"))
										.DesiredSizeOverride(FVector2D(18, 18))
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("SecCreateBtn", "Create Secondary Control Rig"))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
									]
								]
							]
						]
					]
					
					// ========== Single 모드 - 상태 표시 ==========
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(SecStatusText, STextBlock)
						.Text(LOCTEXT("SecStatusInit", "Select a Skeletal Mesh and load bone structure"))
						.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
						.ColorAndOpacity(FLinearColor(0.5f, 0.55f, 0.6f))
					]
				]  // Single 모드 SVerticalBox 끝
				
				// ===== Index 1: Multiple 모드 =====
				+ SWidgetSwitcher::Slot()
				[
					SNew(SVerticalBox)
					
					// ========== Multiple 모드 - 섹션 1: 메쉬 목록 ==========
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.05f, 1.0f))
						.Padding(FMargin(12, 8))
						[
							SNew(SVerticalBox)
							// 헤더
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("ClassIcon.SkeletalMesh"))
									.ColorAndOpacity(FLinearColor(0.4f, 0.7f, 1.0f))
									.DesiredSizeOverride(FVector2D(16, 16))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SecMultipleMeshList", "Skeletal Meshes"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
									.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f))
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f)
								[
									SNullWidget::NullWidget
								]
								// 추가 버튼
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SButton)
									.Text(LOCTEXT("SecMultipleAdd", "+ Add Selected"))
									.ToolTipText(LOCTEXT("SecMultipleAddTooltip", "Add selected meshes from Content Browser"))
									.OnClicked(this, &SControlRigToolWidget::OnSecMultipleAddMeshClicked)
								]
							]
							// 메쉬 목록 (스크롤)
							+ SVerticalBox::Slot().AutoHeight().MaxHeight(300.0f)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(SecMultipleMeshListBox, SVerticalBox)
								]
							]
						]
					]
					
					// ========== Multiple 모드 - 섹션 2: Output Folder ==========
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.02f, 0.04f, 0.02f, 1.0f))
						.Padding(FMargin(12, 8))
						[
							SNew(SVerticalBox)
							// 헤더
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("Icons.Save"))
									.ColorAndOpacity(FLinearColor(0.3f, 0.8f, 0.4f))
									.DesiredSizeOverride(FVector2D(16, 16))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SecMultipleOutputSection", "Output Folder"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
									.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f))
								]
							]
							// 폴더
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SecMultipleFolder", "Folder:"))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
									.ColorAndOpacity(FLinearColor(0.6f, 0.65f, 0.7f))
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 5, 0)
								[
									SNew(SEditableTextBox)
									.Text(FText::FromString(SecDefaultOutputFolder))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
									.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
									{
										SecDefaultOutputFolder = NewText.ToString();
									})
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SButton)
									.Text(LOCTEXT("SecMultipleBrowse", "..."))
									.OnClicked(this, &SControlRigToolWidget::OnSecBrowseFolderClicked)
								]
							]
							// 설명
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SecMultipleNamingInfo", "Output: CTR_[MeshName]_Rig (auto-generated)"))
								.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
								.ColorAndOpacity(FLinearColor(0.5f, 0.55f, 0.6f))
							]
						]
					]
					
					// ========== Multiple 모드 - 섹션 3: Create All Button ==========
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.03f, 0.05f, 0.08f, 1.0f))
						.Padding(FMargin(12, 8))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SAssignNew(SecMultipleCreateButton, SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FMargin(30, 12))
								.IsEnabled_Lambda([this]() { return SecMultipleMeshPaths.Num() > 0; })
								.OnClicked(this, &SControlRigToolWidget::OnSecMultipleCreateClicked)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
									[
										SNew(SImage)
										.Image(FAppStyle::GetBrush("Icons.Plus"))
										.DesiredSizeOverride(FVector2D(18, 18))
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text_Lambda([this]() 
										{
											return FText::Format(LOCTEXT("SecMultipleCreateBtn", "Create All ({0} meshes)"), 
												FText::AsNumber(SecMultipleMeshPaths.Num()));
										})
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
									]
								]
							]
						]
					]
					
					// ========== Multiple 모드 - 상태 표시 ==========
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SecMultipleStatusInit", "Add skeletal meshes and click Create All"))
						.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
						.ColorAndOpacity(FLinearColor(0.5f, 0.55f, 0.6f))
					]
				]  // Multiple 모드 SVerticalBox 끝
			]  // SWidgetSwitcher 끝
		];
}

// ============================================================================
// Control Rig Sec - Mesh 콤보박스 위젯 생성
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::OnGenerateSecMeshWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(InItem.IsValid() ? *InItem : TEXT("")))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
}

void SControlRigToolWidget::OnSecMeshSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	SelectedSecMesh = NewValue;
	UpdateSecMeshThumbnail();
	
	// 메쉬가 변경되면 본 목록 초기화
	SecBoneDisplayList.Empty();
	if (SecBoneListBox.IsValid())
	{
		SecBoneListBox->ClearChildren();
	}
	if (SecCreateButton.IsValid())
	{
		SecCreateButton->SetEnabled(false);
	}
	
	// 메쉬 이름으로 출력 이름 자동 설정
	if (NewValue.IsValid() && SecOutputNameBox.IsValid())
	{
		FString MeshName = FPaths::GetBaseFilename(*NewValue);
		SecOutputNameBox->SetText(FText::FromString(FString::Printf(TEXT("CTR_%s_Rig"), *MeshName)));
	}
	
	SetSecStatus(TEXT("Mesh selected. Click 'Load Bone Structure' to continue."));
}

FText SControlRigToolWidget::GetSelectedSecMeshName() const
{
	if (SelectedSecMesh.IsValid())
		return FText::FromString(FPaths::GetBaseFilename(*SelectedSecMesh));
	return LOCTEXT("SecSelectMesh", "-- Select Mesh --");
}

FReply SControlRigToolWidget::OnUseSelectedSecMeshClicked()
{
	TArray<FAssetData> SelectedAssets;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
	
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.GetClass()->IsChildOf(USkeletalMesh::StaticClass()))
		{
			FString Path = Asset.GetObjectPathString();
			
			// 옵션에 추가 (없으면)
			bool bFound = false;
			for (const TSharedPtr<FString>& Option : MeshOptions)
			{
				if (Option.IsValid() && *Option == Path)
				{
					bFound = true;
					SelectedSecMesh = Option;
					break;
				}
			}
			if (!bFound)
			{
				TSharedPtr<FString> NewOption = MakeShared<FString>(Path);
				MeshOptions.Add(NewOption);
				SelectedSecMesh = NewOption;
			}
			
			if (SecMeshComboBox.IsValid())
			{
				SecMeshComboBox->SetSelectedItem(SelectedSecMesh);
				SecMeshComboBox->RefreshOptions();
			}
			
			OnSecMeshSelectionChanged(SelectedSecMesh, ESelectInfo::Direct);
			break;
		}
	}
	
	return FReply::Handled();
}

void SControlRigToolWidget::UpdateSecMeshThumbnail()
{
	if (!SecMeshThumbnailBox.IsValid() || !ThumbnailPool.IsValid())
		return;
	
	FString Path;
	if (SelectedSecMesh.IsValid())
		Path = *SelectedSecMesh;
	
	if (!Path.IsEmpty())
	{
		SecMeshThumbnail = MakeShared<FAssetThumbnail>(
			LoadObject<UObject>(nullptr, *Path),
			ThumbnailSize, ThumbnailSize, ThumbnailPool);
		
		SecMeshThumbnailBox->SetContent(SecMeshThumbnail->MakeThumbnailWidget());
	}
	else
	{
		SecMeshThumbnailBox->SetContent(SNullWidget::NullWidget);
	}
}

void SControlRigToolWidget::SetSecStatus(const FString& Status)
{
	if (SecStatusText.IsValid())
	{
		SecStatusText->SetText(FText::FromString(Status));
	}
}

// ============================================================================
// Control Rig Sec - AI 본 매핑 요청
// ============================================================================
FReply SControlRigToolWidget::OnSecBoneMappingClicked()
{
	if (!SelectedSecMesh.IsValid())
	{
		SetSecStatus(TEXT("ERROR: Select a Skeletal Mesh first"));
		return FReply::Handled();
	}
	
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, **SelectedSecMesh);
	if (!Mesh)
	{
		SetSecStatus(TEXT("ERROR: Failed to load mesh"));
		return FReply::Handled();
	}
	
	SecCachedMesh = Mesh;
	SetSecStatus(TEXT("Requesting AI Bone Mapping..."));
	
	// AI 서버에 본 매핑 요청 (기존 RequestAIBoneMapping과 동일한 로직)
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// JSON 요청 생성
	TSharedPtr<FJsonObject> RequestObj = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> BonesArray;
	
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		TSharedPtr<FJsonObject> BoneObj = MakeShareable(new FJsonObject);
		BoneObj->SetStringField(TEXT("name"), RefSkel.GetBoneName(i).ToString());
		
		int32 ParentIdx = RefSkel.GetParentIndex(i);
		if (ParentIdx != INDEX_NONE)
			BoneObj->SetStringField(TEXT("parent"), RefSkel.GetBoneName(ParentIdx).ToString());
		else
			BoneObj->SetStringField(TEXT("parent"), TEXT(""));
		
		// 자식 본 수집
		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		for (int32 j = 0; j < RefSkel.GetNum(); ++j)
		{
			if (RefSkel.GetParentIndex(j) == i)
			{
				ChildrenArray.Add(MakeShareable(new FJsonValueString(RefSkel.GetBoneName(j).ToString())));
			}
		}
		BoneObj->SetArrayField(TEXT("children"), ChildrenArray);
		
		BonesArray.Add(MakeShareable(new FJsonValueObject(BoneObj)));
	}
	
	RequestObj->SetArrayField(TEXT("bones"), BonesArray);
	
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);
	
	// HTTP 요청
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("http://localhost:8000/predict"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);
	
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, Mesh](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			if (!bSuccess || !Response.IsValid())
			{
				SetSecStatus(TEXT("ERROR: AI Server not responding. Check if server is running."));
				return;
			}
			
			if (Response->GetResponseCode() != 200)
			{
				SetSecStatus(FString::Printf(TEXT("ERROR: Server returned %d"), Response->GetResponseCode()));
				return;
			}
			
			// 응답 파싱
			TSharedPtr<FJsonObject> JsonResponse;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, JsonResponse))
			{
				SetSecStatus(TEXT("ERROR: Failed to parse response"));
				return;
			}
			
			// 매핑 결과 저장
			LastBoneMapping.Empty();
			const TSharedPtr<FJsonObject>* MappingObj;
			if (JsonResponse->TryGetObjectField(TEXT("mapping"), MappingObj))
			{
				for (const auto& Pair : (*MappingObj)->Values)
				{
					FString TargetBone = Pair.Key;  // UE5 표준 이름
					FString SourceBone = Pair.Value->AsString();  // 실제 본 이름
					LastBoneMapping.Add(FName(*TargetBone), FName(*SourceBone));
				}
			}
			
			// 본 목록 구축 및 UI 업데이트
			BuildSecBoneDisplayList();
			UpdateSecBoneListUI();
			
			// 세컨더리 본 개수 확인
			int32 SecondaryCount = 0;
			for (const FBoneDisplayInfo& Info : SecBoneDisplayList)
			{
				if (Info.Classification == EBoneClassification::Secondary)
					SecondaryCount++;
			}
			
			if (SecCreateButton.IsValid())
			{
				SecCreateButton->SetEnabled(SecondaryCount > 0);
			}
			
			SetSecStatus(FString::Printf(TEXT("AI Mapping complete! %d bones, %d secondary selected"), 
				SecBoneDisplayList.Num(), SecondaryCount));
		}
	);
	
	HttpRequest->ProcessRequest();
	return FReply::Handled();
}

// ============================================================================
// Control Rig Sec - 본 목록 구축 (AI 매핑 결과 기반)
// ============================================================================
void SControlRigToolWidget::BuildSecBoneDisplayList()
{
	SecBoneDisplayList.Empty();
	
	if (!SecCachedMesh.IsValid())
		return;
	
	USkeletalMesh* Mesh = SecCachedMesh.Get();
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// AI 매핑 결과에서 메인본 목록 수집 (Value가 실제 본 이름)
	TSet<FName> MappedMainBones;
	for (const auto& Pair : LastBoneMapping)
	{
		MappedMainBones.Add(Pair.Value);  // 실제 본 이름
	}
	
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		FBoneDisplayInfo Info;
		Info.BoneName = RefSkel.GetBoneName(i);
		Info.BoneIndex = i;
		Info.ParentIndex = RefSkel.GetParentIndex(i);
		
		// 깊이 계산
		int32 Depth = 0;
		int32 ParentIdx = Info.ParentIndex;
		while (ParentIdx != INDEX_NONE)
		{
			Depth++;
			ParentIdx = RefSkel.GetParentIndex(ParentIdx);
		}
		Info.Depth = Depth;
		
		// AI 매핑 결과로 메인본 여부 판별
		bool bIsMappedMainBone = MappedMainBones.Contains(Info.BoneName);
		
		// bip01 계열도 메인본으로 처리
		FString BoneNameLower = Info.BoneName.ToString().ToLower();
		bool bIsBip01 = BoneNameLower.Contains(TEXT("bip01")) || BoneNameLower.Contains(TEXT("bip001"));
		
		Info.bIsZeroBone = bIsMappedMainBone || IsZeroBone(Info.BoneName.ToString()) || bIsBip01;
		
		// 스킨 웨이트 여부
		Info.bHasSkinWeight = HasSkinWeight(Mesh, Info.BoneName);
		
		// 메인본, bip01, 웨이트 없는 본은 X로 고정
		// 나머지만 Secondary로 선택 가능
		if (Info.bIsZeroBone || !Info.bHasSkinWeight)
		{
			Info.Classification = EBoneClassification::Helper;  // X (고정)
		}
		else
		{
			Info.Classification = EBoneClassification::Secondary;
		}
		
		SecBoneDisplayList.Add(Info);
	}
}

// ============================================================================
// Control Rig Sec - 본 목록 UI 업데이트
// ============================================================================
void SControlRigToolWidget::UpdateSecBoneListUI()
{
	if (!SecBoneListBox.IsValid())
		return;
	
	SecBoneListBox->ClearChildren();
	
	for (int32 i = 0; i < SecBoneDisplayList.Num(); ++i)
	{
		SecBoneListBox->AddSlot()
		.AutoHeight()
		.Padding(0, 1)
		[
			CreateSecBoneRow(i)
		];
	}
}

// ============================================================================
// Control Rig Sec - 본 행 생성 (색상 바 포함)
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::CreateSecBoneRow(int32 Index)
{
	if (Index < 0 || Index >= SecBoneDisplayList.Num())
		return SNullWidget::NullWidget;
	
	const FBoneDisplayInfo& Info = SecBoneDisplayList[Index];
	
	// 색상 결정 (분류에 따른 색상)
	FLinearColor BarColor;
	FLinearColor TextColor;
	
	if (Info.bIsZeroBone)
	{
		BarColor = FLinearColor(1.0f, 0.3f, 0.3f);   // 빨강: 메인본(제로본)
		TextColor = FLinearColor(1.0f, 0.3f, 0.3f);
	}
	else if (!Info.bHasSkinWeight)
	{
		BarColor = FLinearColor(0.5f, 0.5f, 0.5f);   // 회색: 웨이트 없음
		TextColor = FLinearColor(0.5f, 0.5f, 0.5f);
	}
	else if (Info.Classification == EBoneClassification::Secondary)
	{
		BarColor = FLinearColor(0.3f, 1.0f, 0.5f);   // 녹색: Secondary 선택됨
		TextColor = FLinearColor(0.3f, 1.0f, 0.5f);
	}
	else
	{
		BarColor = FLinearColor(0.7f, 0.7f, 0.7f);   // 밝은 회색: X (제외)
		TextColor = FLinearColor(0.7f, 0.7f, 0.7f);
	}
	
	// 들여쓰기
	float Indent = Info.Depth * 15.0f;
	
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(Info.Classification == EBoneClassification::Secondary ? 
			FLinearColor(0.05f, 0.1f, 0.05f, 1.0f) : FLinearColor(0.03f, 0.03f, 0.03f, 1.0f))
		.Padding(FMargin(2))
		[
			SNew(SHorizontalBox)
			// 들여쓰기 공간
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(Indent)
			]
			// 색상 바 (분류 표시)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill).Padding(0, 2, 5, 2)
			[
				SNew(SBox)
				.WidthOverride(4)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(BarColor)
				]
			]
			// 본 이름
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(5, 2)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Info.BoneName))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(TextColor)
			]
			// 분류 라디오 버튼들
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5, 0)
			[
				SNew(SHorizontalBox)
				// X 라디오 (제외)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
				[
					SNew(SCheckBox)
					.Style(FCoreStyle::Get(), "RadioButton")
					.IsChecked_Lambda([this, Index]()
					{
						if (Index < SecBoneDisplayList.Num())
							return SecBoneDisplayList[Index].Classification == EBoneClassification::Helper ? 
								ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						return ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this, Index](ECheckBoxState NewState)
					{
						if (NewState == ECheckBoxState::Checked)
							OnSecBoneClassificationChanged(EBoneClassification::Helper, Index);
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SecExclude", "X"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity(FLinearColor(1.0f, 0.3f, 0.3f))
					]
				]
				// Secondary 라디오 (메인본 또는 웨이트 없는 본은 비활성화)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SCheckBox)
					.Style(FCoreStyle::Get(), "RadioButton")
					.IsEnabled_Lambda([this, Index]()
					{
						// 메인본 또는 웨이트 없는 본은 Secondary 선택 불가
						if (Index < SecBoneDisplayList.Num())
						{
							const FBoneDisplayInfo& BoneInfo = SecBoneDisplayList[Index];
							return !BoneInfo.bIsZeroBone && BoneInfo.bHasSkinWeight;
						}
						return false;
					})
					.IsChecked_Lambda([this, Index]()
					{
						if (Index < SecBoneDisplayList.Num())
							return SecBoneDisplayList[Index].Classification == EBoneClassification::Secondary ? 
								ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						return ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this, Index](ECheckBoxState NewState)
					{
						if (NewState == ECheckBoxState::Checked)
							OnSecBoneClassificationChanged(EBoneClassification::Secondary, Index);
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SecSecondary", "Secondary"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FLinearColor(0.3f, 0.9f, 0.5f))
					]
				]
			]
		];
}

// ============================================================================
// Control Rig Sec - 분류 변경 처리
// ============================================================================
void SControlRigToolWidget::OnSecBoneClassificationChanged(EBoneClassification NewClassification, int32 BoneIndex)
{
	if (BoneIndex < 0 || BoneIndex >= SecBoneDisplayList.Num())
		return;
	
	SecBoneDisplayList[BoneIndex].Classification = NewClassification;
	UpdateSecBoneListUI();
	
	// 세컨더리 본 개수 카운트
	int32 SecondaryCount = 0;
	for (const FBoneDisplayInfo& Info : SecBoneDisplayList)
	{
		if (Info.Classification == EBoneClassification::Secondary)
			SecondaryCount++;
	}
	
	// Create 버튼 활성화/비활성화
	if (SecCreateButton.IsValid())
	{
		SecCreateButton->SetEnabled(SecondaryCount > 0);
	}
	
	SetSecStatus(FString::Printf(TEXT("%d Secondary bones selected"), SecondaryCount));
}

// ============================================================================
// Control Rig Sec - 폴더 찾아보기
// ============================================================================
FReply SControlRigToolWidget::OnSecBrowseFolderClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString SelectedFolder;
		const FString DefaultPath = FPaths::ProjectContentDir();
		
		if (DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("Select Output Folder"),
			DefaultPath,
			SelectedFolder))
		{
			// 프로젝트 Content 폴더 기준 상대 경로로 변환
			FString ContentDir = FPaths::ProjectContentDir();
			FPaths::MakePathRelativeTo(SelectedFolder, *ContentDir);
			
			FString GamePath = TEXT("/Game/") + SelectedFolder;
			GamePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			
			if (SecOutputFolderBox.IsValid())
			{
				SecOutputFolderBox->SetText(FText::FromString(GamePath));
			}
		}
	}
	
	return FReply::Handled();
}

// ============================================================================
// Control Rig Sec - Create 버튼 클릭
// ============================================================================
FReply SControlRigToolWidget::OnCreateSecControlRigClicked()
{
	if (CreateSecondaryOnlyControlRigFromSecTab())
	{
		SetSecStatus(TEXT("Secondary Control Rig created successfully!"));
	}
	return FReply::Handled();
}

// ============================================================================
// Control Rig Sec - 세컨더리 전용 Control Rig 생성 (템플릿 복제 후 메인본 노드 삭제)
// ============================================================================
bool SControlRigToolWidget::CreateSecondaryOnlyControlRigFromSecTab()
{
	// 1. 메쉬 확인
	if (!SecCachedMesh.IsValid())
	{
		SetSecStatus(TEXT("ERROR: Select a mesh first and load bone structure"));
		return false;
	}
	
	USkeletalMesh* Mesh = SecCachedMesh.Get();
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// 2. 세컨더리로 선택된 본 수집
	TArray<FName> SelectedSecondaryBones;
	for (const FBoneDisplayInfo& Info : SecBoneDisplayList)
	{
		if (Info.Classification == EBoneClassification::Secondary)
		{
			SelectedSecondaryBones.Add(Info.BoneName);
		}
	}
	
	if (SelectedSecondaryBones.Num() == 0)
	{
		SetSecStatus(TEXT("ERROR: No secondary bones selected"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Creating Control Rig with %d secondary bones"), SelectedSecondaryBones.Num());
	
	// 3. 템플릿 로드
	const FString TemplatePath = TEXT("/Game/00_CooT/Template/CTR_Template.CTR_Template");
	UControlRigBlueprint* TemplateRig = LoadObject<UControlRigBlueprint>(nullptr, *TemplatePath);
	if (!TemplateRig)
	{
		SetSecStatus(TEXT("ERROR: Template not found at /Game/00_CooT/Template/CTR_Template"));
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec] Template not found: %s"), *TemplatePath);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Loaded template: %s"), *TemplatePath);
	
	// 4. 출력 경로 설정
	FString OutputName = SecOutputNameBox.IsValid() ? SecOutputNameBox->GetText().ToString() : TEXT("CTR_NewRig_Rig");
	FString OutputFolder = SecOutputFolderBox.IsValid() ? SecOutputFolderBox->GetText().ToString() : SecDefaultOutputFolder;
	FString OutputPath = OutputFolder / OutputName;
	
	// 기존 에셋 삭제
	if (UEditorAssetLibrary::DoesAssetExist(OutputPath))
	{
		UEditorAssetLibrary::DeleteAsset(OutputPath);
	}
	
	// 5. 템플릿 복제
	UControlRigBlueprint* NewRig = Cast<UControlRigBlueprint>(
		UEditorAssetLibrary::DuplicateAsset(TemplateRig->GetPathName(), OutputPath)
	);
	
	if (!NewRig)
	{
		SetSecStatus(TEXT("ERROR: Failed to duplicate template"));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Duplicated template to: %s"), *OutputPath);
	
	// 6. Preview Mesh 설정
	NewRig->SetPreviewMesh(Mesh, true);
	
	URigHierarchyController* HC = NewRig->GetHierarchyController();
	URigHierarchy* Hierarchy = NewRig->Hierarchy;
	
	if (!HC || !Hierarchy)
	{
		SetSecStatus(TEXT("ERROR: Failed to get hierarchy controller"));
		return false;
	}
	
	// 7. 템플릿의 기존 본/컨트롤러/스페이스 모두 삭제
	TArray<FRigElementKey> AllElements = Hierarchy->GetAllKeys();
	for (const FRigElementKey& Key : AllElements)
	{
		if (Key.Type == ERigElementType::Bone || 
			Key.Type == ERigElementType::Null || 
			Key.Type == ERigElementType::Control)
		{
			HC->RemoveElement(Key, false);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Removed template elements"));
	
	// 8. 새 메쉬의 본 임포트
	USkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton)
	{
		SetSecStatus(TEXT("ERROR: No skeleton"));
		return false;
	}
	
	TArray<FRigElementKey> ImportedBones = HC->ImportBones(
		Skeleton, NAME_None, true, false, false, true, false
	);
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Imported %d bones"), ImportedBones.Num());
	
	// 9. 버텍스 기반 Shape Info 계산
	CalculateBoneShapeInfos(Mesh);
	
	// 10. Space별로 세컨더리 본 그룹화 (AI 매핑 결과 사용)
	TMap<FName, TArray<FName>> ChainsBySpace;
	
	// 세컨더리 본의 부모 중 매핑된 제로본 찾기
	auto FindMappedZeroBoneParent = [&](const FName& BoneName) -> FName
	{
		int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE) return FName(TEXT("root"));
		
		// 부모를 따라 올라가며 매핑된 제로본 찾기
		int32 ParentIndex = RefSkel.GetParentIndex(BoneIndex);
		while (ParentIndex != INDEX_NONE)
		{
			FName ParentBoneName = RefSkel.GetBoneName(ParentIndex);
			
			// LastBoneMapping에서 이 본이 매핑되어 있는지 확인 (source -> target)
			for (const auto& Pair : LastBoneMapping)
			{
				if (Pair.Value == ParentBoneName)
				{
					// 매핑된 UE5 표준 이름 반환
					return Pair.Key;
				}
			}
			
			ParentIndex = RefSkel.GetParentIndex(ParentIndex);
		}
		return FName(TEXT("root"));
	};
	
	for (const FName& BoneName : SelectedSecondaryBones)
	{
		FName SpaceParent = FindMappedZeroBoneParent(BoneName);
		ChainsBySpace.FindOrAdd(SpaceParent).Add(BoneName);
	}
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Grouped into %d spaces (using AI mapping)"), ChainsBySpace.Num());
	
	// 11. Space 및 Control 생성
	LastSecondaryControlCount = 0;
	for (const auto& Pair : ChainsBySpace)
	{
		FName SpaceParentName = Pair.Key;  // UE5 표준 이름 (head, pelvis 등)
		const TArray<FName>& ChainBones = Pair.Value;
		
		// Space 이름: head_space, pelvis_space 등
		FString SpaceNameStr = SpaceParentName.ToString() + TEXT("_space");
		FName SpaceFName(*SpaceNameStr);
		
		// Space Transform: 첫 번째 세컨더리 본의 부모 위치 사용
		FTransform SpaceTransform = FTransform::Identity;
		if (ChainBones.Num() > 0)
		{
			int32 FirstBoneIdx = RefSkel.FindBoneIndex(ChainBones[0]);
			if (FirstBoneIdx != INDEX_NONE)
			{
				int32 ParentIdx = RefSkel.GetParentIndex(FirstBoneIdx);
				if (ParentIdx != INDEX_NONE)
				{
					SpaceTransform = RefSkel.GetRefBonePose()[ParentIdx];
				}
			}
		}
		
		CreateSpaceNull(HC, SpaceFName, SpaceTransform);
		CreateChainControls(HC, Hierarchy, SpaceFName, ChainBones, RefSkel);
		
		UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Created space '%s' with %d controls"), *SpaceNameStr, ChainBones.Num());
	}
	
	// 12. AI 함수 노드 연결 (정답 구조: 각 Event → 각 AI 함수 직접 연결)
	ConnectSecondaryFunctionsSimple(NewRig, ChainsBySpace);
	
	// 13. 컴파일 및 저장
	FBlueprintEditorUtils::MarkBlueprintAsModified(NewRig);
	NewRig->MarkPackageDirty();
	
	UPackage* Package = NewRig->GetPackage();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewRig, *FPackageName::LongPackageNameToFilename(OutputPath, FPackageName::GetAssetPackageExtension()), SaveArgs);
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Saved: %s"), *OutputPath);
	
	// 에셋 에디터에서 열기
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewRig);
	
	TArray<UObject*> ObjectsToSync;
	ObjectsToSync.Add(NewRig);
	GEditor->SyncBrowserToObjects(ObjectsToSync);
	
	FString Msg = FString::Printf(TEXT("Secondary Control Rig Created!\n\nPath: %s\nSpaces: %d\nControls: %d"), 
		*OutputPath, ChainsBySpace.Num(), LastSecondaryControlCount);
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Msg));
	
	return true;
}

// ============================================================================
// Control Rig Sec - 템플릿 없이 AI 함수 노드 직접 연결
// Construction Event -> AI_Setup
// Forward Solve Event -> AI_Forward  
// Backward Solve Event -> AI_Backward
// ============================================================================
void SControlRigToolWidget::ConnectSecondaryFunctionNodesFromScratch(UControlRigBlueprint* Rig, 
	const TMap<FName, TArray<FName>>& ChainsBySpace)
{
	if (!Rig)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec] Control Rig Blueprint is NULL!"));
		return;
	}
	
	if (ChainsBySpace.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControlRigSec] No secondary bones selected!"));
		return;
	}
	
	TArray<URigVMGraph*> AllGraphs = Rig->GetAllModels();
	URigVMGraph* MainGraph = nullptr;
	
	for (URigVMGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName().Equals(TEXT("RigVMModel")))
		{
			MainGraph = Graph;
			break;
		}
	}
	
	if (!MainGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec] Main graph not found!"));
		return;
	}
	
	URigVMController* Controller = Rig->GetController(MainGraph);
	if (!Controller)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec] Controller not found!"));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Connecting AI function nodes for %d spaces"), ChainsBySpace.Num());
	
	// Event 노드들 찾기
	URigVMNode* ConstructionEvent = nullptr;
	URigVMNode* ForwardEvent = nullptr;
	URigVMNode* BackwardEvent = nullptr;
	
	for (URigVMNode* Node : MainGraph->GetNodes())
	{
		FString NodeName = Node->GetName();
		if (NodeName.Contains(TEXT("RigUnit_BeginExecution")) || NodeName.Contains(TEXT("Construction")))
		{
			ConstructionEvent = Node;
		}
		else if (NodeName.Contains(TEXT("RigUnit_PrepareForExecute")) || NodeName.Contains(TEXT("Forwards")))
		{
			ForwardEvent = Node;
		}
		else if (NodeName.Contains(TEXT("RigUnit_InverseExecution")) || NodeName.Contains(TEXT("Backwards")))
		{
			BackwardEvent = Node;
		}
	}
	
	// 노드 위치 설정
	FVector2D SetupStartPos(600.0f, 200.0f);
	FVector2D ForwardStartPos(600.0f, 400.0f);
	FVector2D BackwardStartPos(600.0f, 600.0f);
	
	const float XSpacing = 350.0f;
	
	URigVMNode* LastSetupNode = ConstructionEvent;
	URigVMNode* LastForwardNode = ForwardEvent;
	URigVMNode* LastBackwardNode = BackwardEvent;
	
	FString DebugInfo;
	int32 SpaceIndex = 0;
	
	// 각 Space에 대해 함수 노드 추가
	for (const auto& Pair : ChainsBySpace)
	{
		FName SpaceParentName = Pair.Key;
		const TArray<FName>& ChainBones = Pair.Value;
		
		FString SpaceNameStr = SpaceParentName.ToString() + TEXT("_space");
		FName SpaceName(*SpaceNameStr);
		
		TArray<FName> ControlNames;
		for (const FName& BoneName : ChainBones)
		{
			ControlNames.Add(FName(*(BoneName.ToString() + TEXT("_ctrl"))));
		}
		
		// 실제 본 이름 찾기 (매핑된 이름이 있으면 사용)
		FName ActualBoneName = SpaceParentName;
		
		UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Processing space: %s with %d bones"), *SpaceNameStr, ChainBones.Num());
		
		// AI_Setup 노드 추가
		{
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Setup"), 
				FVector2D(SetupStartPos.X + SpaceIndex * XSpacing, SetupStartPos.Y), DebugInfo);
			if (FuncNode)
			{
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				
				if (LastSetupNode)
				{
					bool bLinked = Controller->AddLink(LastSetupNode->GetName() + TEXT(".ExecuteContext"), FuncNode->GetName() + TEXT(".ExecuteContext"), false);
					if (!bLinked)
						bLinked = Controller->AddLink(LastSetupNode->GetName() + TEXT(".Execute"), FuncNode->GetName() + TEXT(".Execute"), false);
					UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Setup: %s -> %s (%s)"), *LastSetupNode->GetName(), *FuncNode->GetName(), bLinked ? TEXT("OK") : TEXT("FAIL"));
				}
				LastSetupNode = FuncNode;
			}
		}
		
		// AI_Forward 노드 추가
		{
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Forward"), 
				FVector2D(ForwardStartPos.X + SpaceIndex * XSpacing, ForwardStartPos.Y), DebugInfo);
			if (FuncNode)
			{
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				
				if (LastForwardNode)
				{
					bool bLinked = Controller->AddLink(LastForwardNode->GetName() + TEXT(".ExecuteContext"), FuncNode->GetName() + TEXT(".ExecuteContext"), false);
					if (!bLinked)
						bLinked = Controller->AddLink(LastForwardNode->GetName() + TEXT(".Execute"), FuncNode->GetName() + TEXT(".Execute"), false);
					UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Forward: %s -> %s (%s)"), *LastForwardNode->GetName(), *FuncNode->GetName(), bLinked ? TEXT("OK") : TEXT("FAIL"));
				}
				LastForwardNode = FuncNode;
			}
		}
		
		// AI_Backward 노드 추가
		{
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Backward"), 
				FVector2D(BackwardStartPos.X + SpaceIndex * XSpacing, BackwardStartPos.Y), DebugInfo);
			if (FuncNode)
			{
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				
				if (LastBackwardNode)
				{
					bool bLinked = Controller->AddLink(LastBackwardNode->GetName() + TEXT(".ExecuteContext"), FuncNode->GetName() + TEXT(".ExecuteContext"), false);
					if (!bLinked)
						bLinked = Controller->AddLink(LastBackwardNode->GetName() + TEXT(".Execute"), FuncNode->GetName() + TEXT(".Execute"), false);
					UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Backward: %s -> %s (%s)"), *LastBackwardNode->GetName(), *FuncNode->GetName(), bLinked ? TEXT("OK") : TEXT("FAIL"));
				}
				LastBackwardNode = FuncNode;
			}
		}
		
		SpaceIndex++;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Function nodes connected: %d spaces processed"), ChainsBySpace.Num());
}

// ============================================================================
// Control Rig Sec - 모든 기존 노드 삭제 후 AI 함수만 깔끔하게 연결
// Construction Script → AI_Setup
// Forward Solve → AI_Forward
// Backward Solve → AI_Backward
// ============================================================================
void SControlRigToolWidget::CleanupMainBoneNodesAndConnectSecondary(UControlRigBlueprint* Rig,
	const TMap<FName, TArray<FName>>& ChainsBySpace)
{
	if (!Rig || ChainsBySpace.Num() == 0)
		return;
	
	TArray<URigVMGraph*> AllGraphs = Rig->GetAllModels();
	URigVMGraph* MainGraph = nullptr;
	
	for (URigVMGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName().Equals(TEXT("RigVMModel")))
		{
			MainGraph = Graph;
			break;
		}
	}
	
	if (!MainGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec] Main graph not found"));
		return;
	}
	
	URigVMController* Controller = Rig->GetController(MainGraph);
	if (!Controller)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec] Controller not found"));
		return;
	}
	
	// 1. 모든 노드 로깅 (디버그용)
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] === All nodes in graph ==="));
	for (URigVMNode* Node : MainGraph->GetNodes())
	{
		UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Node: %s"), *Node->GetName());
	}
	
	// 2. Event 노드 찾기 - "Forwards Solve"가 실제 이름
	URigVMNode* ConstructionEvent = nullptr;
	URigVMNode* ForwardEvent = nullptr;
	URigVMNode* BackwardEvent = nullptr;
	
	for (URigVMNode* Node : MainGraph->GetNodes())
	{
		FString NodeName = Node->GetName();
		
		// Construction Script Event
		if (NodeName.Contains(TEXT("RigUnit_BeginExecution")) || 
			NodeName.Contains(TEXT("Construction")))
		{
			ConstructionEvent = Node;
		}
		// Forward Solve Event - "Forwards Solve" 또는 "RigUnit_PrepareForExecute"
		else if (NodeName.Contains(TEXT("Forwards")) || 
				 NodeName.Contains(TEXT("Forward")) ||
				 NodeName.Contains(TEXT("RigUnit_PrepareForExecute")))
		{
			ForwardEvent = Node;
		}
		// Backward Solve Event - "Backwards Solve" 또는 "RigUnit_InverseExecution"
		else if (NodeName.Contains(TEXT("Backwards")) || 
				 NodeName.Contains(TEXT("Backward")) ||
				 NodeName.Contains(TEXT("RigUnit_InverseExecution")))
		{
			BackwardEvent = Node;
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Found - Construction: %s, Forward: %s, Backward: %s"),
		ConstructionEvent ? *ConstructionEvent->GetName() : TEXT("NULL"),
		ForwardEvent ? *ForwardEvent->GetName() : TEXT("NULL"),
		BackwardEvent ? *BackwardEvent->GetName() : TEXT("NULL"));
	
	// 2. AI 함수 노드만 삭제 (Event 노드와 기타 필요한 노드는 유지)
	TArray<URigVMNode*> NodesToRemove;
	for (URigVMNode* Node : MainGraph->GetNodes())
	{
		FString NodeName = Node->GetName();
		// AI 관련 함수 노드만 삭제 (기존 템플릿의 메인본용 노드들)
		if (NodeName.Contains(TEXT("AI_Setup")) || 
			NodeName.Contains(TEXT("AI_Forward")) || 
			NodeName.Contains(TEXT("AI_Backward")) ||
			NodeName.Contains(TEXT("Make Array")))
		{
			NodesToRemove.Add(Node);
			UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Will remove: %s"), *NodeName);
		}
	}
	
	for (URigVMNode* Node : NodesToRemove)
	{
		Controller->RemoveNode(Node, false, false);
	}
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Removed %d AI function nodes"), NodesToRemove.Num());
	
	// 3. Forward Solve 이벤트가 있으면, 그것을 기준으로 3가지 함수 연결
	// Construction Script/Backward Solve가 없으면 Forward Solve만 사용
	if (!ConstructionEvent && !BackwardEvent && ForwardEvent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControlRigSec] Only Forward Solve exists. Connecting all functions to Forward Solve."));
		// Forward Solve에 AI_Setup → AI_Forward → AI_Backward 순서로 연결
	}
	
	// 4. AI 함수 노드 생성 및 연결
	FString DebugInfo;
	int32 SpaceIndex = 0;
	const float XSpacing = 400.0f;
	
	// Forward Solve만 있는 경우: Forward → AI_Setup → AI_Forward → AI_Backward 순서로 연결
	bool bOnlyForwardSolve = ForwardEvent && !ConstructionEvent && !BackwardEvent;
	
	URigVMNode* LastChainNode = ForwardEvent;  // 모든 함수가 이어지는 체인
	
	// Event 노드 위치 기준으로 함수 노드 배치
	FVector2D StartPos = ForwardEvent ? ForwardEvent->GetPosition() + FVector2D(300, 0) : FVector2D(400, 200);
	
	for (const auto& Pair : ChainsBySpace)
	{
		FName SpaceParentName = Pair.Key;  // UE5 표준 이름 (head, pelvis 등)
		const TArray<FName>& ChainBones = Pair.Value;
		
		// Space 이름: head_space, pelvis_space 등
		FString SpaceNameStr = SpaceParentName.ToString() + TEXT("_space");
		FName SpaceName(*SpaceNameStr);
		
		// 실제 본 이름 찾기 (LastBoneMapping에서 UE5 표준 이름 -> 실제 본 이름)
		FName ActualBoneName = SpaceParentName;
		if (const FName* FoundBone = LastBoneMapping.Find(SpaceParentName))
		{
			ActualBoneName = *FoundBone;
		}
		UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Space: %s -> Actual Bone: %s"), *SpaceParentName.ToString(), *ActualBoneName.ToString());
		
		// Control 이름: bone_ctrl
		TArray<FName> ControlNames;
		for (const FName& BoneName : ChainBones)
			ControlNames.Add(FName(*(BoneName.ToString() + TEXT("_ctrl"))));
		
		UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Processing space: %s with %d bones"), *SpaceNameStr, ChainBones.Num());
		
		// Forward Solve만 있는 경우: 하나의 체인으로 연결
		// Forward Solve → AI_Setup → AI_Forward → AI_Backward
		
		// AI_Setup 노드
		{
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Setup"),
				FVector2D(StartPos.X + SpaceIndex * XSpacing, StartPos.Y), DebugInfo);
			if (FuncNode)
			{
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				if (LastChainNode)
				{
					// Execute 핀 이름 시도 (ExecuteContext 또는 Execute)
					bool bLinked = Controller->AddLink(LastChainNode->GetName() + TEXT(".ExecuteContext"), 
						FuncNode->GetName() + TEXT(".ExecuteContext"), false);
					if (!bLinked)
					{
						bLinked = Controller->AddLink(LastChainNode->GetName() + TEXT(".Execute"), 
							FuncNode->GetName() + TEXT(".Execute"), false);
					}
					UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] %s -> %s (%s)"), 
						*LastChainNode->GetName(), *FuncNode->GetName(), bLinked ? TEXT("OK") : TEXT("FAIL"));
				}
				LastChainNode = FuncNode;
			}
		}
		
		// AI_Forward 노드
		{
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Forward"),
				FVector2D(StartPos.X + SpaceIndex * XSpacing, StartPos.Y + 200), DebugInfo);
			if (FuncNode)
			{
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				if (LastChainNode)
				{
					bool bLinked = Controller->AddLink(LastChainNode->GetName() + TEXT(".ExecuteContext"),
						FuncNode->GetName() + TEXT(".ExecuteContext"), false);
					if (!bLinked)
					{
						bLinked = Controller->AddLink(LastChainNode->GetName() + TEXT(".Execute"),
							FuncNode->GetName() + TEXT(".Execute"), false);
					}
					UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] %s -> %s (%s)"), 
						*LastChainNode->GetName(), *FuncNode->GetName(), bLinked ? TEXT("OK") : TEXT("FAIL"));
				}
				LastChainNode = FuncNode;
			}
		}
		
		// AI_Backward 노드
		{
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Backward"),
				FVector2D(StartPos.X + SpaceIndex * XSpacing, StartPos.Y + 400), DebugInfo);
			if (FuncNode)
			{
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				if (LastChainNode)
				{
					bool bLinked = Controller->AddLink(LastChainNode->GetName() + TEXT(".ExecuteContext"),
						FuncNode->GetName() + TEXT(".ExecuteContext"), false);
					if (!bLinked)
					{
						bLinked = Controller->AddLink(LastChainNode->GetName() + TEXT(".Execute"),
							FuncNode->GetName() + TEXT(".Execute"), false);
					}
					UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] %s -> %s (%s)"), 
						*LastChainNode->GetName(), *FuncNode->GetName(), bLinked ? TEXT("OK") : TEXT("FAIL"));
				}
				LastChainNode = FuncNode;
			}
		}
		
		SpaceIndex++;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Created and connected AI functions for %d spaces"), ChainsBySpace.Num());
}

// ============================================================================
// Control Rig Sec - 정답 구조로 AI 함수 연결
// Backwards Solve → AI_Backward
// Construction Event → AI_Setup
// Forwards Solve → AI_Forward
// ============================================================================
void SControlRigToolWidget::ConnectSecondaryFunctionsSimple(UControlRigBlueprint* Rig,
	const TMap<FName, TArray<FName>>& ChainsBySpace)
{
	if (!Rig || ChainsBySpace.Num() == 0)
		return;
	
	TArray<URigVMGraph*> AllGraphs = Rig->GetAllModels();
	URigVMGraph* MainGraph = nullptr;
	
	for (URigVMGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName().Equals(TEXT("RigVMModel")))
		{
			MainGraph = Graph;
			break;
		}
	}
	
	if (!MainGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec] Main graph not found"));
		return;
	}
	
	URigVMController* Controller = Rig->GetController(MainGraph);
	if (!Controller)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec] Controller not found"));
		return;
	}
	
	// 1. 모든 노드 로깅 및 Event 노드 찾기
	URigVMNode* ConstructionEvent = nullptr;  // RigUnit_BeginExecution -> AI_Setup
	URigVMNode* ForwardEvent = nullptr;       // PrepareForExecution -> AI_Forward
	URigVMNode* BackwardEvent = nullptr;      // InverseExecution -> AI_Backward
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] === All nodes in graph ==="));
	for (URigVMNode* Node : MainGraph->GetNodes())
	{
		FString NodeName = Node->GetName();
		FVector2D Pos = Node->GetPosition();
		UE_LOG(LogTemp, Log, TEXT("  Node: %s at (%.0f, %.0f)"), *NodeName, Pos.X, Pos.Y);
		
		// Construction Event (RigUnit_BeginExecution)
		if (NodeName.Contains(TEXT("BeginExecution")))
		{
			ConstructionEvent = Node;
			UE_LOG(LogTemp, Log, TEXT("    -> This is CONSTRUCTION EVENT"));
		}
		// Forward Solve (PrepareForExecution)
		else if (NodeName.Contains(TEXT("PrepareForExecution")) || NodeName.Contains(TEXT("PrepareFor")))
		{
			ForwardEvent = Node;
			UE_LOG(LogTemp, Log, TEXT("    -> This is FORWARD SOLVE"));
		}
		// Backward Solve (InverseExecution)
		else if (NodeName.Contains(TEXT("InverseExecution")))
		{
			BackwardEvent = Node;
			UE_LOG(LogTemp, Log, TEXT("    -> This is BACKWARD SOLVE"));
		}
	}
	
	// 2. Event 3개 제외하고 모든 노드 삭제 (기존 AI 함수 포함!)
	TArray<URigVMNode*> NodesToRemove;
	for (URigVMNode* Node : MainGraph->GetNodes())
	{
		if (Node == ConstructionEvent || Node == ForwardEvent || Node == BackwardEvent)
			continue;
		NodesToRemove.Add(Node);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Removing %d nodes (including existing AI functions)"), NodesToRemove.Num());
	for (URigVMNode* Node : NodesToRemove)
	{
		UE_LOG(LogTemp, Log, TEXT("  Removing: %s"), *Node->GetName());
		Controller->RemoveNode(Node, true, true);
	}
	
	// 확인: 남은 노드 출력
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] Remaining nodes after cleanup:"));
	for (URigVMNode* Node : MainGraph->GetNodes())
	{
		UE_LOG(LogTemp, Log, TEXT("  %s at (%.0f, %.0f)"), *Node->GetName(), Node->GetPosition().X, Node->GetPosition().Y);
	}
	
	// 3. 캡쳐 기준 연결 (ForwardEvent와 ConstructionEvent 스왑!)
	// 캡쳐에서:
	// - Backwards Solve → AI_Backward ✓
	// - Construction Event → AI_Setup (현재 코드에서 ForwardEvent가 이걸 가리킴)
	// - Forwards Solve → AI_Forward (현재 코드에서 ConstructionEvent가 이걸 가리킴)
	
	FString DebugInfo;
	int32 SpaceIndex = 0;
	const float FuncXOffset = 400.0f;
	const float FuncXSpacing = 350.0f;
	
	URigVMNode* LastBackwardFunc = nullptr;
	URigVMNode* LastForwardFunc = nullptr;
	URigVMNode* LastSetupFunc = nullptr;
	
	for (const auto& Pair : ChainsBySpace)
	{
		FName SpaceParentName = Pair.Key;
		const TArray<FName>& ChainBones = Pair.Value;
		
		FString SpaceNameStr = SpaceParentName.ToString() + TEXT("_space");
		FName SpaceName(*SpaceNameStr);
		
		FName ActualBoneName = SpaceParentName;
		if (const FName* FoundBone = LastBoneMapping.Find(SpaceParentName))
		{
			ActualBoneName = *FoundBone;
		}
		
		TArray<FName> ControlNames;
		for (const FName& BoneName : ChainBones)
			ControlNames.Add(FName(*(BoneName.ToString() + TEXT("_ctrl"))));
		
		// Event의 실제 위치 기준으로 AI 함수 배치
		FVector2D BackwardPos = BackwardEvent ? BackwardEvent->GetPosition() : FVector2D(0, 0);
		FVector2D ForwardPos = ForwardEvent ? ForwardEvent->GetPosition() : FVector2D(0, 300);
		FVector2D ConstructionPos = ConstructionEvent ? ConstructionEvent->GetPosition() : FVector2D(0, 600);
		
		float XOffset = FuncXOffset + SpaceIndex * FuncXSpacing;
		
		// AI_Backward - Backward Solve(InverseExecution) 옆에 배치, 연결
		if (BackwardEvent)
		{
			FVector2D FuncPos(BackwardPos.X + XOffset, BackwardPos.Y);
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Backward"), FuncPos, DebugInfo);
			if (FuncNode)
			{
				Controller->SetNodePosition(FuncNode, FuncPos, false, false);
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				
				FString SrcPin = (SpaceIndex == 0) ? 
					BackwardEvent->GetName() + TEXT(".ExecuteContext") : 
					LastBackwardFunc->GetName() + TEXT(".ExecuteContext");
				
				bool bLinked = Controller->AddLink(SrcPin, FuncNode->GetName() + TEXT(".ExecuteContext"), false);
				UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] %s -> AI_Backward [%s]"), *SrcPin, bLinked ? TEXT("OK") : TEXT("FAIL"));
				LastBackwardFunc = FuncNode;
			}
		}
		
		// ★ 스왑: ForwardEvent → AI_Setup (캡쳐에서 Construction Event가 중간)
		if (ForwardEvent)
		{
			FVector2D FuncPos(ForwardPos.X + XOffset, ForwardPos.Y);
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Setup"), FuncPos, DebugInfo);
			if (FuncNode)
			{
				Controller->SetNodePosition(FuncNode, FuncPos, false, false);
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				
				FString SrcPin = (SpaceIndex == 0) ? 
					ForwardEvent->GetName() + TEXT(".ExecuteContext") : 
					LastSetupFunc->GetName() + TEXT(".ExecuteContext");
				
				bool bLinked = Controller->AddLink(SrcPin, FuncNode->GetName() + TEXT(".ExecuteContext"), false);
				UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] %s -> AI_Setup [%s]"), *SrcPin, bLinked ? TEXT("OK") : TEXT("FAIL"));
				LastSetupFunc = FuncNode;
			}
		}
		
		// ★ 스왑: ConstructionEvent → AI_Forward (캡쳐에서 Forwards Solve가 아래)
		if (ConstructionEvent)
		{
			FVector2D FuncPos(ConstructionPos.X + XOffset, ConstructionPos.Y);
			URigVMNode* FuncNode = AddFunctionReferenceNode(Controller, TEXT("AI_Forward"), FuncPos, DebugInfo);
			if (FuncNode)
			{
				Controller->SetNodePosition(FuncNode, FuncPos, false, false);
				SetFunctionNodePins(Controller, FuncNode, ActualBoneName, SpaceName, ChainBones, ControlNames);
				
				FString SrcPin = (SpaceIndex == 0) ? 
					ConstructionEvent->GetName() + TEXT(".ExecuteContext") : 
					LastForwardFunc->GetName() + TEXT(".ExecuteContext");
				
				bool bLinked = Controller->AddLink(SrcPin, FuncNode->GetName() + TEXT(".ExecuteContext"), false);
				UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] %s -> AI_Forward [%s]"), *SrcPin, bLinked ? TEXT("OK") : TEXT("FAIL"));
				LastForwardFunc = FuncNode;
			}
		}
		
		SpaceIndex++;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec] === Done (swapped Forward<->Setup) ==="));
}

// ============================================================================
// Control Rig Sec - Single 모드 버튼 클릭
// ============================================================================
FReply SControlRigToolWidget::OnSecSingleModeClicked()
{
	bSecMultipleMode = false;
	SetSecStatus(TEXT("Single mode: Select a mesh and configure bone selection"));
	return FReply::Handled();
}

// ============================================================================
// Control Rig Sec - Multiple 모드 버튼 클릭
// ============================================================================
FReply SControlRigToolWidget::OnSecMultipleModeClicked()
{
	bSecMultipleMode = true;
	SetSecStatus(TEXT("Multiple mode: Add meshes and create all at once"));
	return FReply::Handled();
}

// ============================================================================
// Control Rig Sec - Multiple 모드에서 메쉬 추가
// ============================================================================
FReply SControlRigToolWidget::OnSecMultipleAddMeshClicked()
{
	// 콘텐츠 브라우저에서 선택된 에셋들 가져오기
	TArray<FAssetData> SelectedAssets;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
	
	int32 AddedCount = 0;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (AssetData.GetClass() == USkeletalMesh::StaticClass())
		{
			FString MeshPath = AssetData.GetSoftObjectPath().ToString();
			if (!SecMultipleMeshPaths.Contains(MeshPath))
			{
				SecMultipleMeshPaths.Add(MeshPath);
				AddedCount++;
			}
		}
	}
	
	if (AddedCount > 0)
	{
		UpdateSecMultipleMeshListUI();
		SetSecStatus(FString::Printf(TEXT("Added %d mesh(es). Total: %d"), AddedCount, SecMultipleMeshPaths.Num()));
	}
	else
	{
		SetSecStatus(TEXT("No new skeletal meshes selected in Content Browser"));
	}
	
	return FReply::Handled();
}

// ============================================================================
// Control Rig Sec - Multiple 모드에서 메쉬 제거
// ============================================================================
FReply SControlRigToolWidget::OnSecMultipleRemoveMeshClicked(int32 Index)
{
	if (SecMultipleMeshPaths.IsValidIndex(Index))
	{
		SecMultipleMeshPaths.RemoveAt(Index);
		UpdateSecMultipleMeshListUI();
		SetSecStatus(FString::Printf(TEXT("Removed mesh. Remaining: %d"), SecMultipleMeshPaths.Num()));
	}
	return FReply::Handled();
}

// ============================================================================
// Control Rig Sec - Multiple 모드에서 AI 매핑 (현재는 사용 안함, 자동 판별 사용)
// ============================================================================
FReply SControlRigToolWidget::OnSecMultipleAIMappingClicked()
{
	// Multiple 모드에서는 자동 판별을 사용하므로 별도 AI 매핑 불필요
	SetSecStatus(TEXT("Multiple mode uses automatic bone classification"));
	return FReply::Handled();
}

// ============================================================================
// Control Rig Sec - Multiple 모드에서 일괄 생성
// ============================================================================
FReply SControlRigToolWidget::OnSecMultipleCreateClicked()
{
	if (SecMultipleMeshPaths.Num() == 0)
	{
		SetSecStatus(TEXT("ERROR: No meshes added"));
		return FReply::Handled();
	}
	
	CreateMultipleSecondaryControlRigs();
	return FReply::Handled();
}

// ============================================================================
// Control Rig Sec - Multiple 모드 메쉬 목록 UI 업데이트
// ============================================================================
void SControlRigToolWidget::UpdateSecMultipleMeshListUI()
{
	if (!SecMultipleMeshListBox.IsValid())
		return;
	
	SecMultipleMeshListBox->ClearChildren();
	
	for (int32 i = 0; i < SecMultipleMeshPaths.Num(); i++)
	{
		const FString& MeshPath = SecMultipleMeshPaths[i];
		FString MeshName = FPackageName::GetShortName(MeshPath);
		
		SecMultipleMeshListBox->AddSlot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.03f, 0.03f, 0.05f, 1.0f))
			.Padding(FMargin(8, 4))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(FText::AsNumber(i + 1))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.6f))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(MeshName))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("SecMultipleRemove", "X"))
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Danger")
					.ContentPadding(FMargin(4, 2))
					.OnClicked_Lambda([this, i]() { return OnSecMultipleRemoveMeshClicked(i); })
				]
			]
		];
	}
}

// ============================================================================
// Control Rig Sec - Multiple 모드 일괄 생성 로직
// ============================================================================
bool SControlRigToolWidget::CreateMultipleSecondaryControlRigs()
{
	if (SecMultipleMeshPaths.Num() == 0)
	{
		SetSecStatus(TEXT("ERROR: No meshes to process"));
		return false;
	}
	
	int32 SuccessCount = 0;
	int32 FailCount = 0;
	
	for (const FString& MeshPath : SecMultipleMeshPaths)
	{
		UE_LOG(LogTemp, Log, TEXT("[ControlRigSec Multiple] Processing: %s"), *MeshPath);
		
		if (CreateSingleSecondaryControlRigAuto(MeshPath))
		{
			SuccessCount++;
		}
		else
		{
			FailCount++;
			UE_LOG(LogTemp, Warning, TEXT("[ControlRigSec Multiple] Failed: %s"), *MeshPath);
		}
	}
	
	FString ResultMsg = FString::Printf(TEXT("Created %d Control Rigs. Failed: %d"), SuccessCount, FailCount);
	SetSecStatus(ResultMsg);
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec Multiple] %s"), *ResultMsg);
	
	// 완료 후 목록 초기화
	SecMultipleMeshPaths.Empty();
	UpdateSecMultipleMeshListUI();
	
	return SuccessCount > 0;
}

// ============================================================================
// Control Rig Sec - 단일 메쉬에 대해 자동 세컨더리 판별로 Control Rig 생성
// ============================================================================
bool SControlRigToolWidget::CreateSingleSecondaryControlRigAuto(const FString& MeshPath)
{
	// 1. 메쉬 로드
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec Auto] Failed to load mesh: %s"), *MeshPath);
		return false;
	}
	
	FString MeshName = Mesh->GetName();
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec Auto] Processing mesh: %s"), *MeshName);
	
	// 2. AI 본 매핑 요청 (동기식으로 처리 - LastBoneMapping 업데이트)
	// 여기서는 기존 규칙 기반 + AI 매핑 결과를 사용
	// 임시로 CachedMesh와 LastBoneMapping을 설정
	SecCachedMesh = Mesh;
	
	// 본 매핑 API 호출 (동기식)
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	TArray<TSharedPtr<FJsonValue>> BoneArray;
	
	for (int32 i = 0; i < RefSkel.GetNum(); i++)
	{
		TSharedPtr<FJsonObject> BoneObj = MakeShareable(new FJsonObject);
		BoneObj->SetStringField(TEXT("name"), RefSkel.GetBoneName(i).ToString());
		
		int32 ParentIdx = RefSkel.GetParentIndex(i);
		if (ParentIdx != INDEX_NONE)
			BoneObj->SetStringField(TEXT("parent"), RefSkel.GetBoneName(ParentIdx).ToString());
		else
			BoneObj->SetStringField(TEXT("parent"), TEXT(""));
		
		TArray<TSharedPtr<FJsonValue>> ChildArray;
		for (int32 j = 0; j < RefSkel.GetNum(); j++)
		{
			if (RefSkel.GetParentIndex(j) == i)
				ChildArray.Add(MakeShareable(new FJsonValueString(RefSkel.GetBoneName(j).ToString())));
		}
		BoneObj->SetArrayField(TEXT("children"), ChildArray);
		
		BoneArray.Add(MakeShareable(new FJsonValueObject(BoneObj)));
	}
	
	TSharedPtr<FJsonObject> RequestObj = MakeShareable(new FJsonObject);
	RequestObj->SetArrayField(TEXT("bones"), BoneArray);
	RequestObj->SetStringField(TEXT("mesh_name"), MeshName);
	
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);
	
	// 동기식 HTTP 요청은 불가능하므로, 여기서는 규칙 기반으로만 처리
	// (실제로는 비동기 처리 필요하지만, 단순화를 위해 규칙 기반 사용)
	LastBoneMapping.Empty();
	
	// 규칙 기반으로 메인 본 찾기 (ZeroBones 기준)
	for (int32 i = 0; i < RefSkel.GetNum(); i++)
	{
		FString BoneNameStr = RefSkel.GetBoneName(i).ToString().ToLower();
		
		// ZeroBones에 있는 키워드와 매칭
		for (const FString& ZeroBone : ZeroBones)
		{
			if (BoneNameStr.Contains(ZeroBone.ToLower()))
			{
				LastBoneMapping.Add(FName(*ZeroBone), RefSkel.GetBoneName(i));
				break;
			}
		}
	}
	
	// 3. 출력 경로 설정
	FString OutputPath = SecDefaultOutputFolder / FString::Printf(TEXT("CTR_%s_Rig"), *MeshName);
	
	// 4. 세컨더리 본 자동 판별하여 체인 구성
	TMap<FName, TArray<FName>> ChainsBySpace;
	
	for (int32 i = 0; i < RefSkel.GetNum(); i++)
	{
		FName BoneName = RefSkel.GetBoneName(i);
		FString BoneNameStr = BoneName.ToString();
		
		// 제로본(메인본)은 제외
		if (IsZeroBone(BoneNameStr))
			continue;
		
		// bip01 계열도 제외
		if (BoneNameStr.ToLower().Contains(TEXT("bip01")) || BoneNameStr.ToLower().Contains(TEXT("bip001")))
			continue;
		
		// 스킨 웨이트 없는 본 제외
		if (!HasSkinWeight(Mesh, BoneName))
			continue;
		
		// 헬퍼 본 제외
		if (IsHelperBone(BoneNameStr))
			continue;
		
		// 세컨더리 본으로 판정 - 부모 메인본 찾기
		FName ParentZeroBone = FindZeroBoneParent(BoneName, RefSkel);
		if (ParentZeroBone != NAME_None)
		{
			ChainsBySpace.FindOrAdd(ParentZeroBone).Add(BoneName);
		}
	}
	
	if (ChainsBySpace.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControlRigSec Auto] No secondary bones found for: %s"), *MeshName);
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec Auto] Found %d secondary chains"), ChainsBySpace.Num());
	
	// 5. 템플릿 로드
	FString TemplatePath = TEXT("/Game/00_CooT/Template/CTR_Template");
	UControlRigBlueprint* TemplateRig = LoadObject<UControlRigBlueprint>(nullptr, *TemplatePath);
	if (!TemplateRig)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec Auto] Failed to load template: %s"), *TemplatePath);
		return false;
	}
	
	// 6. 템플릿 복제
	UPackage* Package = CreatePackage(*OutputPath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec Auto] Failed to create package: %s"), *OutputPath);
		return false;
	}
	
	FString AssetName = FPackageName::GetLongPackageAssetName(OutputPath);
	UControlRigBlueprint* NewRig = Cast<UControlRigBlueprint>(
		StaticDuplicateObject(TemplateRig, Package, FName(*AssetName))
	);
	
	if (!NewRig)
	{
		UE_LOG(LogTemp, Error, TEXT("[ControlRigSec Auto] Failed to duplicate template"));
		return false;
	}
	
	// 7. 메쉬 설정
	NewRig->SetPreviewMesh(Mesh);
	
	// 8. 본 가져오기 및 Shape 정보 계산
	URigHierarchy* Hierarchy = NewRig->GetHierarchy();
	URigHierarchyController* HierarchyController = Hierarchy->GetController();
	
	HierarchyController->ImportBones(RefSkel, NAME_None, false, false, false);
	CalculateBoneShapeInfos(Mesh);
	
	// 9. 기존 템플릿의 요소들 정리
	TArray<FRigElementKey> AllElements = Hierarchy->GetAllKeys();
	for (const FRigElementKey& Key : AllElements)
	{
		if (Key.Type == ERigElementType::Control || Key.Type == ERigElementType::Null)
		{
			HierarchyController->RemoveElement(Key, true);
		}
	}
	
	// 10. Space와 Control 생성
	for (const auto& Pair : ChainsBySpace)
	{
		FName SpaceParentName = Pair.Key;
		const TArray<FName>& ChainBones = Pair.Value;
		
		// 실제 본 이름 찾기
		FName ActualParentBone = SpaceParentName;
		if (const FName* FoundBone = LastBoneMapping.Find(SpaceParentName))
		{
			ActualParentBone = *FoundBone;
		}
		
		// Space 이름 및 Transform
		FString SpaceNameStr = SpaceParentName.ToString() + TEXT("_space");
		FName SpaceName(*SpaceNameStr);
		
		int32 ParentBoneIdx = RefSkel.FindBoneIndex(ActualParentBone);
		FTransform SpaceTransform = (ParentBoneIdx != INDEX_NONE) ? 
			RefSkel.GetRefBonePose()[ParentBoneIdx] : FTransform::Identity;
		
		// Space 생성
		CreateSpaceNull(HierarchyController, SpaceName, SpaceTransform);
		
		// Control 생성
		CreateChainControls(HierarchyController, Hierarchy, SpaceName, ChainBones, RefSkel);
	}
	
	// 11. RigVM 함수 노드 연결
	ConnectSecondaryFunctionsSimple(NewRig, ChainsBySpace);
	
	// 12. 컴파일 및 저장
	NewRig->RecompileVM();
	NewRig->MarkPackageDirty();
	
	FAssetRegistryModule::AssetCreated(NewRig);
	
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewRig, *FPackageName::LongPackageNameToFilename(OutputPath, FPackageName::GetAssetPackageExtension()), SaveArgs);
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigSec Auto] Created: %s"), *OutputPath);
	return true;
}

// ============================================================================
// Anim Picker 탭 콘텐츠
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::CreateAnimPickerTab()
{
	const FLinearColor CardBgColor(0.04f, 0.04f, 0.05f, 1.0f);
	const FLinearColor AccentColor(0.2f, 0.6f, 0.9f, 1.0f);
	
	// Control Rig 에셋 목록 로드
	LoadAnimPickerControlRigs();
	
	TSharedRef<SWidget> Result = SNew(SVerticalBox)
		
		// ========== 헤더 ==========
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.05f, 0.08f, 0.12f, 1.0f))
			.Padding(FMargin(15, 12))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Sequencer.Tracks.ControlRig"))
					.ColorAndOpacity(AccentColor)
					.DesiredSizeOverride(FVector2D(24, 24))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimPickerHeader", "Animation Picker"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimPickerSubtitle", "Interactive Controller Selection"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
				]
			]
		]
		
		// ========== Control Rig 선택 (썸네일 + 드롭다운 + 화살표) ==========
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(CardBgColor)
			.Padding(FMargin(15, 12))
			[
				SNew(SVerticalBox)
				
				// 섹션 헤더
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("ControlRig.RigUnit"))
						.ColorAndOpacity(FLinearColor(0.4f, 0.7f, 1.0f))
						.DesiredSizeOverride(FVector2D(16, 16))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AnimPickerControlRig", "Control Rig Selection"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
						.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f))
					]
				]
				
				// 썸네일 + 드롭다운 + 화살표 버튼
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
				[
					SNew(SHorizontalBox)
					// 썸네일
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f, 1.0f))
						.Padding(2)
						[
							SAssignNew(AnimPickerControlRigThumbnailBox, SBox)
							.WidthOverride(ThumbnailSize)
							.HeightOverride(ThumbnailSize)
						]
					]
					// 드롭다운 + 버튼들
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
							[
								SAssignNew(AnimPickerControlRigComboBox, SComboBox<TSharedPtr<FString>>)
								.OptionsSource(&AnimPickerControlRigOptions)
								.OnGenerateWidget(this, &SControlRigToolWidget::OnGenerateAnimPickerControlRigWidget)
								.OnSelectionChanged(this, &SControlRigToolWidget::OnAnimPickerControlRigSelectionChanged)
								[
									SNew(STextBlock)
									.Text(this, &SControlRigToolWidget::GetSelectedAnimPickerControlRigName)
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
								]
							]
							// 화살표 버튼 (선택된 에셋 가져오기)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "FlatButton")
								.ContentPadding(FMargin(6, 4))
								.ToolTipText(LOCTEXT("AnimPickerUseSelectedTooltip", "Use selected Control Rig from Content Browser"))
								.OnClicked(this, &SControlRigToolWidget::OnAnimPickerUseSelectedControlRigClicked)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("Icons.ArrowLeft"))
									.ColorAndOpacity(FLinearColor(0.5f, 0.7f, 1.0f))
									.DesiredSizeOverride(FVector2D(16, 16))
								]
							]
						]
						// Load + Refresh 버튼
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							// Load 버튼
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FMargin(12, 6))
								.OnClicked(this, &SControlRigToolWidget::OnAnimPickerLoadControlRigClicked)
								.ButtonColorAndOpacity(FLinearColor(0.1f, 0.35f, 0.6f, 1.0f))
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
									[
										SNew(SImage)
										.Image(FAppStyle::GetBrush("Icons.Import"))
										.DesiredSizeOverride(FVector2D(14, 14))
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("AnimPickerLoadBtn", "Load"))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
									]
								]
							]
							// Refresh 버튼
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FMargin(10, 6))
								.OnClicked(this, &SControlRigToolWidget::OnAnimPickerRefreshClicked)
								.ButtonColorAndOpacity(FLinearColor(0.15f, 0.4f, 0.2f, 1.0f))
								.ToolTipText(LOCTEXT("AnimPickerRefreshTooltip", "Refresh Control Rig list"))
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("Icons.Refresh"))
									.DesiredSizeOverride(FVector2D(14, 14))
								]
							]
						]
					]
				]
				
				// 모드 전환 버튼 (3개: List / 2D / 3D)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					// List View 버튼
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 2, 0)
					[
						SAssignNew(AnimPickerListViewButton, SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(0, 6))
						.OnClicked(this, &SControlRigToolWidget::OnAnimPickerModeChanged, EAnimPickerMode::ListView)
						.ButtonColorAndOpacity_Lambda([this]() 
						{
							return CurrentAnimPickerMode == EAnimPickerMode::ListView 
								? FLinearColor(0.1f, 0.4f, 0.7f, 1.0f) 
								: FLinearColor(0.05f, 0.05f, 0.06f, 1.0f);
						})
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.Toolbar.Details"))
								.DesiredSizeOverride(FVector2D(12, 12))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AnimPickerListView", "List"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							]
						]
					]
					// 2D View 버튼
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2, 0, 2, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(0, 6))
						.OnClicked(this, &SControlRigToolWidget::OnAnimPickerModeChanged, EAnimPickerMode::View2D)
						.ButtonColorAndOpacity_Lambda([this]() 
						{
							return CurrentAnimPickerMode == EAnimPickerMode::View2D 
								? FLinearColor(0.1f, 0.4f, 0.7f, 1.0f) 
								: FLinearColor(0.05f, 0.05f, 0.06f, 1.0f);
						})
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.Visible"))
								.DesiredSizeOverride(FVector2D(12, 12))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AnimPicker2DView", "2D"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							]
						]
					]
					// 3D View 버튼
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2, 0, 0, 0)
					[
						SAssignNew(AnimPicker3DViewButton, SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(0, 6))
						.OnClicked(this, &SControlRigToolWidget::OnAnimPickerModeChanged, EAnimPickerMode::View3D)
						.ButtonColorAndOpacity_Lambda([this]() 
						{
							return CurrentAnimPickerMode == EAnimPickerMode::View3D 
								? FLinearColor(0.1f, 0.4f, 0.7f, 1.0f) 
								: FLinearColor(0.05f, 0.05f, 0.06f, 1.0f);
						})
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("EditorViewport.ToggleLODs"))
								.DesiredSizeOverride(FVector2D(12, 12))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AnimPicker3DView", "3D"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							]
						]
					]
				]
			]
		]
		
		// ========== 컨텐츠 영역 (모드에 따라 전환) ==========
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 0, 0, 10)
		[
			SAssignNew(AnimPickerModeSwitcher, SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return (int32)CurrentAnimPickerMode; })
			
			// ===== List View (index 0) =====
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(CardBgColor)
				.Padding(FMargin(10, 10))
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(AnimPickerListViewBox, SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0, 50)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AnimPickerNoControlRig", "Select a Control Rig and click 'Load' to view controllers"))
							.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
							.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.45f))
						]
					]
				]
			]
			
			// ===== 2D View (index 1) - 자동 배치 피커 (스크롤 + 줌) =====
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(CardBgColor)
				.Padding(FMargin(5, 5))
				[
					SNew(SVerticalBox)
					
					// ★ 줌 컨트롤 바 ★
					+ SVerticalBox::Slot().AutoHeight().Padding(5, 2)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
						[
							SNew(SButton)
							.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
							.ContentPadding(FMargin(4))
							.OnClicked_Lambda([this]() -> FReply
							{
								AnimPicker2DZoomScale = FMath::Clamp(AnimPicker2DZoomScale - 0.15f, 0.5f, 2.5f);
								UpdateAnimPicker2DView();
								return FReply::Handled();
							})
							.ToolTipText(LOCTEXT("ZoomOut", "Zoom Out (-)"))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("−")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
								.ColorAndOpacity(FLinearColor::White)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0)
						[
							SNew(STextBlock)
							.Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("%.0f%%"), AnimPicker2DZoomScale * 100)); })
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.8f))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
						[
							SNew(SButton)
							.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
							.ContentPadding(FMargin(4))
							.OnClicked_Lambda([this]() -> FReply
							{
								AnimPicker2DZoomScale = FMath::Clamp(AnimPicker2DZoomScale + 0.15f, 0.5f, 2.5f);
								UpdateAnimPicker2DView();
								return FReply::Handled();
							})
							.ToolTipText(LOCTEXT("ZoomIn", "Zoom In (+)"))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("+")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
								.ColorAndOpacity(FLinearColor::White)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
						[
							SNew(SButton)
							.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
							.ContentPadding(FMargin(4))
							.OnClicked_Lambda([this]() -> FReply
							{
								AnimPicker2DZoomScale = 1.0f;
								UpdateAnimPicker2DView();
								return FReply::Handled();
							})
							.ToolTipText(LOCTEXT("ZoomReset", "Reset Zoom (100%)"))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Reset")))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.7f))
							]
						]
					]
					
					// ★ 2D 피커 (마우스 휠 줌 지원) ★
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SScrollBox)
						.Orientation(Orient_Vertical)
						+ SScrollBox::Slot().HAlign(HAlign_Center)
						[
							SNew(SScrollBox)
							.Orientation(Orient_Horizontal)
							+ SScrollBox::Slot().VAlign(VAlign_Center)
							[
								// 2D 피커 패널 (마우스 휠 줌 지원)
								SNew(SBox)
								.WidthOverride_Lambda([this]() { return 550.0f * AnimPicker2DZoomScale; })
								.HeightOverride_Lambda([this]() { return 900.0f * AnimPicker2DZoomScale; })
								[
									SAssignNew(AnimPicker2DPanel, SAnimPicker2DPanel)
									.OwnerWidget(this)
								]
							]
						]
					]
				]
			]
			
			// ===== 3D View (index 2) =====
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(CardBgColor)
				.Padding(FMargin(2, 2))
				[
					SNew(SBox)
					.MinDesiredHeight(400)
					[
						SAssignNew(AnimPicker3DViewport, SAnimPickerViewport)
					]
				]
			]
		]
		
		// ========== 상태 표시 ==========
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f, 1.0f))
			.Padding(FMargin(15, 8))
			[
				SAssignNew(AnimPickerStatusText, STextBlock)
				.Text(LOCTEXT("AnimPickerReady", "Ready - Select a Control Rig to load controllers"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(FLinearColor(0.4f, 0.6f, 0.8f))
			]
		];
	
	// 3D 뷰포트 클릭 콜백 연결
	if (AnimPicker3DViewport.IsValid())
	{
		AnimPicker3DViewport->OnControllerClicked.BindSP(this, &SControlRigToolWidget::OnAnimPicker3DControllerClicked);
	}
	
	return Result;
}

// ============================================================================
// Anim Picker - Control Rig 에셋 로드
// ============================================================================
void SControlRigToolWidget::LoadAnimPickerControlRigs()
{
	AnimPickerControlRigOptions.Empty();
	AnimPickerControlRigPaths.Empty();
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/ControlRigDeveloper"), TEXT("ControlRigBlueprint")), AssetDataList);
	
	for (const FAssetData& AssetData : AssetDataList)
	{
		FString AssetName = AssetData.AssetName.ToString();
		FString AssetPath = AssetData.GetObjectPathString();
		
		AnimPickerControlRigOptions.Add(MakeShared<FString>(AssetName));
		AnimPickerControlRigPaths.Add(AssetName, AssetPath);
	}
	
	// 알파벳 순 정렬
	AnimPickerControlRigOptions.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B)
	{
		return *A < *B;
	});
}

// ============================================================================
// Anim Picker - 콤보박스 위젯 생성
// ============================================================================
TSharedRef<SWidget> SControlRigToolWidget::OnGenerateAnimPickerControlRigWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InItem))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10));
}

// ============================================================================
// Anim Picker - 선택 변경
// ============================================================================
void SControlRigToolWidget::OnAnimPickerControlRigSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	SelectedAnimPickerControlRig = NewValue;
	UpdateAnimPickerControlRigThumbnail();
}

// ============================================================================
// Anim Picker - 선택된 이름 가져오기
// ============================================================================
FText SControlRigToolWidget::GetSelectedAnimPickerControlRigName() const
{
	if (SelectedAnimPickerControlRig.IsValid())
	{
		return FText::FromString(*SelectedAnimPickerControlRig);
	}
	return LOCTEXT("AnimPickerSelectControlRig", "Select Control Rig...");
}

// ============================================================================
// Anim Picker - 썸네일 업데이트
// ============================================================================
void SControlRigToolWidget::UpdateAnimPickerControlRigThumbnail()
{
	if (!AnimPickerControlRigThumbnailBox.IsValid() || !ThumbnailPool.IsValid())
	{
		return;
	}
	
	FString Path = GetSelectedAnimPickerControlRigPath();
	if (Path.IsEmpty())
	{
		AnimPickerControlRigThumbnailBox->SetContent(SNullWidget::NullWidget);
		return;
	}
	
	FAssetData AssetData = FAssetData(LoadObject<UObject>(nullptr, *Path));
	if (AssetData.IsValid())
	{
		AnimPickerControlRigThumbnail = MakeShared<FAssetThumbnail>(AssetData, ThumbnailSize, ThumbnailSize, ThumbnailPool);
		AnimPickerControlRigThumbnailBox->SetContent(AnimPickerControlRigThumbnail->MakeThumbnailWidget());
	}
	else
	{
		AnimPickerControlRigThumbnailBox->SetContent(SNullWidget::NullWidget);
	}
}

// ============================================================================
// Anim Picker - 선택된 Control Rig 경로 가져오기
// ============================================================================
FString SControlRigToolWidget::GetSelectedAnimPickerControlRigPath() const
{
	if (SelectedAnimPickerControlRig.IsValid())
	{
		const FString* PathPtr = AnimPickerControlRigPaths.Find(*SelectedAnimPickerControlRig);
		if (PathPtr)
		{
			return *PathPtr;
		}
	}
	return FString();
}

// ============================================================================
// Anim Picker - 화살표 버튼 (Content Browser에서 선택된 에셋 가져오기)
// ============================================================================
FReply SControlRigToolWidget::OnAnimPickerUseSelectedControlRigClicked()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
	
	for (const FAssetData& AssetData : SelectedAssets)
	{
		// Control Rig Blueprint인지 확인
		if (AssetData.AssetClassPath == FTopLevelAssetPath(TEXT("/Script/ControlRigDeveloper"), TEXT("ControlRigBlueprint")))
		{
			FString AssetName = AssetData.AssetName.ToString();
			FString AssetPath = AssetData.GetObjectPathString();
			
			// 옵션에 없으면 추가
			bool bFound = false;
			for (const TSharedPtr<FString>& Option : AnimPickerControlRigOptions)
			{
				if (*Option == AssetName)
				{
					bFound = true;
					SelectedAnimPickerControlRig = Option;
					break;
				}
			}
			
			if (!bFound)
			{
				TSharedPtr<FString> NewOption = MakeShared<FString>(AssetName);
				AnimPickerControlRigOptions.Add(NewOption);
				AnimPickerControlRigPaths.Add(AssetName, AssetPath);
				SelectedAnimPickerControlRig = NewOption;
			}
			
			// 콤보박스 업데이트
			if (AnimPickerControlRigComboBox.IsValid())
			{
				AnimPickerControlRigComboBox->SetSelectedItem(SelectedAnimPickerControlRig);
			}
			
			UpdateAnimPickerControlRigThumbnail();
			SetAnimPickerStatus(FString::Printf(TEXT("Selected: %s"), *AssetName));
			return FReply::Handled();
		}
	}
	
	SetAnimPickerStatus(TEXT("Please select a Control Rig Blueprint in Content Browser"));
	return FReply::Handled();
}

// ============================================================================
// Anim Picker - 모드 전환
// ============================================================================
FReply SControlRigToolWidget::OnAnimPickerModeChanged(EAnimPickerMode NewMode)
{
	CurrentAnimPickerMode = NewMode;
	
	// 모드 변경 시 해당 뷰 업데이트
	if (NewMode == EAnimPickerMode::View2D)
	{
		UpdateAnimPicker2DView();
	}
	else if (NewMode == EAnimPickerMode::View3D)
	{
		UpdateAnimPicker3DView();
	}
	
	return FReply::Handled();
}

// ============================================================================
// Anim Picker - Control Rig 로드
// ============================================================================
FReply SControlRigToolWidget::OnAnimPickerLoadControlRigClicked()
{
	if (!SelectedAnimPickerControlRig.IsValid())
	{
		SetAnimPickerStatus(TEXT("Please select a Control Rig first"));
		return FReply::Handled();
	}
	
	FString* PathPtr = AnimPickerControlRigPaths.Find(*SelectedAnimPickerControlRig);
	if (!PathPtr)
	{
		SetAnimPickerStatus(TEXT("Control Rig path not found"));
		return FReply::Handled();
	}
	
	LoadControllersFromControlRig(*PathPtr);
	
	return FReply::Handled();
}

// ============================================================================
// Anim Picker - 새로고침 (Control Rig 목록 다시 로드)
// ============================================================================
FReply SControlRigToolWidget::OnAnimPickerRefreshClicked()
{
	// Control Rig 목록 다시 스캔
	LoadAnimPickerControlRigs();
	
	// ComboBox 새로고침
	if (AnimPickerControlRigComboBox.IsValid())
	{
		AnimPickerControlRigComboBox->RefreshOptions();
	}
	
	// 현재 선택된 Control Rig가 있으면 다시 로드
	if (SelectedAnimPickerControlRig.IsValid())
	{
		FString* PathPtr = AnimPickerControlRigPaths.Find(*SelectedAnimPickerControlRig);
		if (PathPtr)
		{
			LoadControllersFromControlRig(*PathPtr);
		}
	}
	
	SetAnimPickerStatus(FString::Printf(TEXT("Refreshed: Found %d Control Rigs"), AnimPickerControlRigOptions.Num()));
	
	return FReply::Handled();
}

// ============================================================================
// Anim Picker - Control Rig에서 컨트롤러 로드
// ============================================================================
void SControlRigToolWidget::LoadControllersFromControlRig(const FString& ControlRigPath)
{
	AnimPickerControllersBySpace.Empty();
	AnimPickerSpaceExpanded.Empty();
	
	// ★★★ 로드하는 Control Rig 경로 출력 ★★★
	UE_LOG(LogTemp, Error, TEXT("[AnimPicker] Loading Control Rig: %s"), *ControlRigPath);
	
	// Control Rig Blueprint 로드
	UControlRigBlueprint* ControlRigBP = LoadObject<UControlRigBlueprint>(nullptr, *ControlRigPath);
	if (!ControlRigBP)
	{
		SetAnimPickerStatus(TEXT("Failed to load Control Rig Blueprint"));
		return;
	}
	
	UE_LOG(LogTemp, Error, TEXT("[AnimPicker] Loaded: %s"), *ControlRigBP->GetName());
	
	URigHierarchy* Hierarchy = ControlRigBP->Hierarchy;
	if (!Hierarchy)
	{
		SetAnimPickerStatus(TEXT("Control Rig has no hierarchy"));
		return;
	}
	
	// 모든 컨트롤 요소 가져오기
	TArray<FRigControlElement*> Controls = Hierarchy->GetControls();
	
	// Space(Null) 요소 가져오기
	TArray<FRigNullElement*> Nulls = Hierarchy->GetNulls();
	TMap<FName, FName> ControlToSpace;  // Control -> Space 매핑
	
	// 각 Control의 부모 Space 찾기
	for (FRigControlElement* Control : Controls)
	{
		FName ControlName = Control->GetFName();
		FString NameStr = ControlName.ToString();
		FString NameLower = NameStr.ToLower();
		
		// ★ 필터링: _ctrl 또는 _switch로 끝나는 것만 ★
		bool bPassFilter = NameLower.EndsWith(TEXT("_ctrl")) || NameLower.EndsWith(TEXT("_switch"));
		if (!bPassFilter)
		{
			continue;
		}
		
		// 필터 통과 로그
		if (NameLower.Contains(TEXT("switch")))
		{
			UE_LOG(LogTemp, Error, TEXT("[AnimPicker LOAD] Switch controller PASSED filter: '%s'"), *NameStr);
		}
		
		FName ParentSpace = NAME_None;
		
		// 부모 체인을 따라가며 Space(Null) 찾기
		FRigBaseElement* Parent = Hierarchy->GetFirstParent(Control);
		while (Parent)
		{
			if (FRigNullElement* NullParent = Cast<FRigNullElement>(Parent))
			{
				ParentSpace = NullParent->GetFName();
				break;
			}
			Parent = Hierarchy->GetFirstParent(Parent);
		}
		
		// Space가 없으면 "root" 그룹에
		if (ParentSpace == NAME_None)
		{
			ParentSpace = FName(TEXT("root"));
		}
		
		// 컨트롤러 정보 생성
		FAnimPickerControllerInfo Info;
		Info.ControlName = ControlName;
		Info.ParentSpace = ParentSpace;
		Info.Transform = Hierarchy->GetGlobalTransform(Control->GetKey());
		
		// ★★★ Control Rig에서 Shape 정보 직접 추출 (올바른 API 사용!) ★★★
		const FRigControlSettings& Settings = Control->Settings;
		
		// ★★★ Shape Transform: GetLocalControlShapeTransform() 사용! ★★★
		// Settings.ShapeTransform은 기본값만 갖고 있고, 실제 값은 별도 저장됨!
		FTransform ActualShapeTransform = Hierarchy->GetLocalControlShapeTransform(Control->GetKey(), false); // bInitial = false
		Info.ShapeTransform = ActualShapeTransform;
		Info.ShapeName = Settings.ShapeName;
		Info.ShapeScale = ActualShapeTransform.GetScale3D();
		
		// ★ 색상: Control Rig의 원본 ShapeColor 사용 ★
		Info.Color = Settings.ShapeColor;
		
		// ★★★ 모든 컨트롤러 ShapeTransform 디버그 (처음 20개) ★★★
		static int32 DebugCounter = 0;
		if (DebugCounter < 20)
		{
			FVector ShapeLoc = ActualShapeTransform.GetLocation();
			FVector ShapeScale = ActualShapeTransform.GetScale3D();
			FRotator ShapeRot = ActualShapeTransform.GetRotation().Rotator();
			UE_LOG(LogTemp, Error, TEXT("[AnimPicker SHAPE] '%s': Shape='%s', ShapeTransform=(Loc=%.1f,%.1f,%.1f / Rot=%.1f,%.1f,%.1f / Scale=%.2f,%.2f,%.2f)"),
				*NameStr,
				*Settings.ShapeName.ToString(),
				ShapeLoc.X, ShapeLoc.Y, ShapeLoc.Z,
				ShapeRot.Pitch, ShapeRot.Yaw, ShapeRot.Roll,
				ShapeScale.X, ShapeScale.Y, ShapeScale.Z);
			DebugCounter++;
		}
		
		// Space별로 그룹화
		if (!AnimPickerControllersBySpace.Contains(ParentSpace))
		{
			AnimPickerControllersBySpace.Add(ParentSpace, TArray<FAnimPickerControllerInfo>());
			AnimPickerSpaceExpanded.Add(ParentSpace, true);  // 기본 펼침
		}
		AnimPickerControllersBySpace[ParentSpace].Add(Info);
	}
	
	// 컨트롤러 순서 저장 (Shift 범위 선택용)
	AnimPickerControllerOrder.Empty();
	for (const auto& Pair : AnimPickerControllersBySpace)
	{
		for (const FAnimPickerControllerInfo& Info : Pair.Value)
		{
			AnimPickerControllerOrder.Add(Info.ControlName);
		}
	}
	
	// 선택 초기화
	AnimPickerSelectedControllers.Empty();
	AnimPickerLastSelectedIndex = INDEX_NONE;
	
	// UI 업데이트
	UpdateAnimPickerListViewUI();
	UpdateAnimPicker2DView();
	UpdateAnimPicker3DView();
	SetAnimPickerStatus(FString::Printf(TEXT("Loaded %d controllers from %d spaces"), Controls.Num(), AnimPickerControllersBySpace.Num()));
}

// ============================================================================
// Anim Picker - Space별 색상 가져오기
// ============================================================================
FLinearColor SControlRigToolWidget::GetColorForSpace(const FName& SpaceName) const
{
	// Space 이름에 따라 색상 결정 (세컨더리용)
	FString SpaceStr = SpaceName.ToString().ToLower();
	
	if (SpaceStr.Contains(TEXT("head")) || SpaceStr.Contains(TEXT("hair")))
	{
		return FLinearColor(0.3f, 0.6f, 1.0f);  // 파랑
	}
	else if (SpaceStr.Contains(TEXT("pelvis")) || SpaceStr.Contains(TEXT("skirt")))
	{
		return FLinearColor(0.3f, 0.8f, 0.4f);  // 초록
	}
	else if (SpaceStr.Contains(TEXT("spine")) || SpaceStr.Contains(TEXT("cloak")) || SpaceStr.Contains(TEXT("cape")))
	{
		return FLinearColor(0.9f, 0.7f, 0.2f);  // 노랑
	}
	else if (SpaceStr.Contains(TEXT("arm")) || SpaceStr.Contains(TEXT("hand")))
	{
		return FLinearColor(0.9f, 0.4f, 0.3f);  // 빨강
	}
	else if (SpaceStr.Contains(TEXT("leg")) || SpaceStr.Contains(TEXT("foot")))
	{
		return FLinearColor(0.7f, 0.4f, 0.9f);  // 보라
	}
	
	// 기본 색상 - 해시 기반
	uint32 Hash = GetTypeHash(SpaceName);
	float Hue = (Hash % 360) / 360.0f;
	return FLinearColor::MakeFromHSV8(Hue * 255, 180, 200);
}

// ============================================================================
// Anim Picker - 컨트롤러별 색상 가져오기 (Control Rig 기본 색상과 일치)
// ============================================================================
FLinearColor SControlRigToolWidget::GetColorForController(const FName& ControllerName) const
{
	FString CtrlStr = ControllerName.ToString().ToLower();
	
	// 왼쪽 본 - 파란색/청록색 (Control Rig 기본)
	if (CtrlStr.EndsWith(TEXT("_l")) || CtrlStr.EndsWith(TEXT("_l_ctrl")) || 
		CtrlStr.Contains(TEXT("_l_")) || CtrlStr.StartsWith(TEXT("l_")))
	{
		return FLinearColor(0.2f, 0.6f, 1.0f);  // 파란색
	}
	
	// 오른쪽 본 - 빨간색/주황색 (Control Rig 기본)
	if (CtrlStr.EndsWith(TEXT("_r")) || CtrlStr.EndsWith(TEXT("_r_ctrl")) || 
		CtrlStr.Contains(TEXT("_r_")) || CtrlStr.StartsWith(TEXT("r_")))
	{
		return FLinearColor(1.0f, 0.3f, 0.2f);  // 빨간색
	}
	
	// Spine 계열 - 노란색
	if (CtrlStr.Contains(TEXT("spine")) || CtrlStr.Contains(TEXT("pelvis")) || 
		CtrlStr.Contains(TEXT("hips")) || CtrlStr.Contains(TEXT("root")))
	{
		return FLinearColor(1.0f, 0.85f, 0.2f);  // 노란색
	}
	
	// Head/Neck 계열 - 노란색 (중앙)
	if (CtrlStr.Contains(TEXT("head")) || CtrlStr.Contains(TEXT("neck")))
	{
		return FLinearColor(1.0f, 0.85f, 0.2f);  // 노란색
	}
	
	// IK 컨트롤러
	if (CtrlStr.Contains(TEXT("ik_")) || CtrlStr.Contains(TEXT("_ik")))
	{
		if (CtrlStr.Contains(TEXT("_l")) || CtrlStr.Contains(TEXT("left")))
			return FLinearColor(0.2f, 0.8f, 0.8f);  // 청록색
		if (CtrlStr.Contains(TEXT("_r")) || CtrlStr.Contains(TEXT("right")))
			return FLinearColor(1.0f, 0.5f, 0.3f);  // 주황색
		return FLinearColor(0.6f, 0.9f, 0.3f);  // 연두색
	}
	
	// 기본 - 회색
	return FLinearColor(0.6f, 0.6f, 0.65f);
}

// ============================================================================
// Anim Picker - Space 접기/펼치기
// ============================================================================
FReply SControlRigToolWidget::OnAnimPickerSpaceToggleClicked(FName SpaceName)
{
	if (bool* bExpanded = AnimPickerSpaceExpanded.Find(SpaceName))
	{
		*bExpanded = !(*bExpanded);
		UpdateAnimPickerListViewUI();
	}
	return FReply::Handled();
}

// ============================================================================
// Anim Picker - 컨트롤러 클릭 (다중 선택 지원)
// ============================================================================
FReply SControlRigToolWidget::OnAnimPickerControllerClicked(FName ControlName)
{
	// 현재 인덱스 찾기
	int32 CurrentIndex = AnimPickerControllerOrder.IndexOfByKey(ControlName);
	
	// 키보드 상태 확인
	const bool bCtrlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();
	const bool bShiftDown = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	
	if (bCtrlDown)
	{
		// Ctrl+클릭: 토글 (추가/제거)
		if (AnimPickerSelectedControllers.Contains(ControlName))
		{
			AnimPickerSelectedControllers.Remove(ControlName);
		}
		else
		{
			AnimPickerSelectedControllers.Add(ControlName);
		}
		AnimPickerLastSelectedIndex = CurrentIndex;
	}
	else if (bShiftDown && AnimPickerLastSelectedIndex != INDEX_NONE)
	{
		// Shift+클릭: 범위 선택
		int32 StartIdx = FMath::Min(AnimPickerLastSelectedIndex, CurrentIndex);
		int32 EndIdx = FMath::Max(AnimPickerLastSelectedIndex, CurrentIndex);
		
		for (int32 i = StartIdx; i <= EndIdx; i++)
		{
			if (AnimPickerControllerOrder.IsValidIndex(i))
			{
				AnimPickerSelectedControllers.Add(AnimPickerControllerOrder[i]);
			}
		}
	}
	else
	{
		// 일반 클릭: 단일 선택 (기존 선택 해제)
		AnimPickerSelectedControllers.Empty();
		AnimPickerSelectedControllers.Add(ControlName);
		AnimPickerLastSelectedIndex = CurrentIndex;
	}
	
	// 시퀀서/에디터에서 실제 선택
	bool bClearFirst = !bCtrlDown && !bShiftDown;
	SelectControlsInEditor(AnimPickerSelectedControllers, bClearFirst);
	
	// UI 업데이트
	UpdateAnimPickerListViewUI();
	
	// 상태 표시
	if (AnimPickerSelectedControllers.Num() == 1)
	{
		SetAnimPickerStatus(FString::Printf(TEXT("Selected: %s"), *ControlName.ToString()));
	}
	else
	{
		SetAnimPickerStatus(FString::Printf(TEXT("Selected: %d controllers"), AnimPickerSelectedControllers.Num()));
	}
	
	UE_LOG(LogTemp, Log, TEXT("[AnimPicker] Selection: %d controllers"), AnimPickerSelectedControllers.Num());
	
	return FReply::Handled();
}

// ============================================================================
// Anim Picker - 2D 피커 업데이트 (사람 실루엣 + 정확한 피커 배치)
// ============================================================================
void SControlRigToolWidget::UpdateAnimPicker2DView()
{
	// 패널에서 오버레이 가져오기
	if (AnimPicker2DPanel.IsValid())
	{
		AnimPicker2DOverlay = AnimPicker2DPanel->GetOverlay();
		AnimPicker2DPanel->SetZoomScale(AnimPicker2DZoomScale);
	}
	
	if (!AnimPicker2DOverlay.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimPicker 2D] AnimPicker2DOverlay is not valid!"));
		return;
	}
	
	// 기존 슬롯 모두 제거
	AnimPicker2DOverlay->ClearChildren();
	
	// ★ 배경 슬롯 추가 (어두운 색) ★
	AnimPicker2DOverlay->AddSlot()
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.07f, 1.0f))
	];
	
	if (AnimPickerControllersBySpace.Num() == 0)
	{
		AnimPicker2DOverlay->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AnimPicker2DNoData", "2D Picker - Select Control Rig to load"))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
			.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.6f))
		];
		return;
	}
	
	// ★★★ 캔버스 크기 및 줌 스케일 ★★★
	const float ZoomScale = AnimPicker2DZoomScale;
	const float CanvasWidth = 500.0f * ZoomScale;
	const float CanvasHeight = 700.0f * ZoomScale;
	
	// 실루엣 색상 (반투명 회색)
	const FLinearColor SilhouetteColor(0.25f, 0.25f, 0.28f, 0.6f);
	
	// ★★★ 사람 실루엣 그리기 ★★★
	// 머리 (원형 - 둥근 테두리)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.42f, CanvasHeight * 0.02f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.16f).HeightOverride(CanvasWidth * 0.16f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(SilhouetteColor)
		]
	];
	
	// 목
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.46f, CanvasHeight * 0.115f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.08f).HeightOverride(CanvasHeight * 0.04f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	
	// 어깨/쇄골 (가로로 넓은 사각형)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.22f, CanvasHeight * 0.15f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.56f).HeightOverride(CanvasHeight * 0.035f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	
	// 몸통 (세로로 긴 사각형)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.35f, CanvasHeight * 0.185f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.30f).HeightOverride(CanvasHeight * 0.22f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	
	// 골반 (가로로 넓은 사각형)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.32f, CanvasHeight * 0.40f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.36f).HeightOverride(CanvasHeight * 0.05f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	
	// 왼쪽 팔 (upperarm)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.18f, CanvasHeight * 0.17f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.055f).HeightOverride(CanvasHeight * 0.12f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	// 왼쪽 팔 (lowerarm)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.14f, CanvasHeight * 0.29f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.05f).HeightOverride(CanvasHeight * 0.12f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	// 왼쪽 손
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.10f, CanvasHeight * 0.41f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.07f).HeightOverride(CanvasHeight * 0.06f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	
	// 오른쪽 팔 (upperarm)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.765f, CanvasHeight * 0.17f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.055f).HeightOverride(CanvasHeight * 0.12f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	// 오른쪽 팔 (lowerarm)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.81f, CanvasHeight * 0.29f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.05f).HeightOverride(CanvasHeight * 0.12f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	// 오른쪽 손
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.83f, CanvasHeight * 0.41f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.07f).HeightOverride(CanvasHeight * 0.06f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	
	// 왼쪽 다리 (thigh)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.35f, CanvasHeight * 0.45f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.065f).HeightOverride(CanvasHeight * 0.16f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	// 왼쪽 다리 (calf)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.34f, CanvasHeight * 0.61f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.055f).HeightOverride(CanvasHeight * 0.16f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	// 왼쪽 발
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.30f, CanvasHeight * 0.77f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.085f).HeightOverride(CanvasHeight * 0.035f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	
	// 오른쪽 다리 (thigh)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.585f, CanvasHeight * 0.45f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.065f).HeightOverride(CanvasHeight * 0.16f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	// 오른쪽 다리 (calf)
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.605f, CanvasHeight * 0.61f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.055f).HeightOverride(CanvasHeight * 0.16f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	// 오른쪽 발
	AnimPicker2DOverlay->AddSlot()
	.Padding(FMargin(CanvasWidth * 0.615f, CanvasHeight * 0.77f, 0, 0))
	.HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(SBox).WidthOverride(CanvasWidth * 0.085f).HeightOverride(CanvasHeight * 0.035f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(SilhouetteColor)
		]
	];
	
	// ★★★ 피커 위치 정의 (실루엣에 맞춰서) ★★★
	TMap<FString, FVector2D> BonePositions;
	
	// === 머리/목 ===
	BonePositions.Add(TEXT("head"), FVector2D(0.50f, 0.055f));
	BonePositions.Add(TEXT("neck"), FVector2D(0.50f, 0.125f));
	BonePositions.Add(TEXT("neck_01"), FVector2D(0.50f, 0.125f));
	
	// === 척추 (중앙 세로) ===
	BonePositions.Add(TEXT("spine_03"), FVector2D(0.50f, 0.195f));
	BonePositions.Add(TEXT("spine_02"), FVector2D(0.50f, 0.26f));
	BonePositions.Add(TEXT("spine_01"), FVector2D(0.50f, 0.325f));
	BonePositions.Add(TEXT("spine"), FVector2D(0.50f, 0.325f));
	
	// === 골반/힙 ===
	BonePositions.Add(TEXT("pelvis"), FVector2D(0.50f, 0.39f));
	BonePositions.Add(TEXT("hips"), FVector2D(0.50f, 0.42f));
	BonePositions.Add(TEXT("body"), FVector2D(0.50f, 0.365f));
	BonePositions.Add(TEXT("body_offset"), FVector2D(0.50f, 0.355f));
	BonePositions.Add(TEXT("global"), FVector2D(0.50f, 0.44f));
	// === 왼쪽 쇄골/팔 (실루엣 위치에 맞춤) ===
	BonePositions.Add(TEXT("clavicle_l"), FVector2D(0.36f, 0.16f));
	BonePositions.Add(TEXT("upperarm_l"), FVector2D(0.205f, 0.22f));
	BonePositions.Add(TEXT("lowerarm_l"), FVector2D(0.165f, 0.34f));
	BonePositions.Add(TEXT("hand_l"), FVector2D(0.135f, 0.435f));
	
	// 왼쪽 손가락 (손 아래쪽에 밀집)
	BonePositions.Add(TEXT("thumb_01_l"), FVector2D(0.095f, 0.425f));
	BonePositions.Add(TEXT("thumb_02_l"), FVector2D(0.08f, 0.435f));
	BonePositions.Add(TEXT("thumb_03_l"), FVector2D(0.065f, 0.445f));
	BonePositions.Add(TEXT("index_01_l"), FVector2D(0.10f, 0.455f));
	BonePositions.Add(TEXT("index_02_l"), FVector2D(0.085f, 0.465f));
	BonePositions.Add(TEXT("index_03_l"), FVector2D(0.07f, 0.475f));
	BonePositions.Add(TEXT("middle_01_l"), FVector2D(0.115f, 0.46f));
	BonePositions.Add(TEXT("middle_02_l"), FVector2D(0.10f, 0.47f));
	BonePositions.Add(TEXT("middle_03_l"), FVector2D(0.085f, 0.48f));
	BonePositions.Add(TEXT("ring_01_l"), FVector2D(0.13f, 0.46f));
	BonePositions.Add(TEXT("ring_02_l"), FVector2D(0.115f, 0.47f));
	BonePositions.Add(TEXT("ring_03_l"), FVector2D(0.10f, 0.48f));
	BonePositions.Add(TEXT("pinky_01_l"), FVector2D(0.145f, 0.455f));
	BonePositions.Add(TEXT("pinky_02_l"), FVector2D(0.13f, 0.465f));
	BonePositions.Add(TEXT("pinky_03_l"), FVector2D(0.115f, 0.475f));
	
	// === 오른쪽 쇄골/팔 ===
	BonePositions.Add(TEXT("clavicle_r"), FVector2D(0.64f, 0.16f));
	BonePositions.Add(TEXT("upperarm_r"), FVector2D(0.795f, 0.22f));
	BonePositions.Add(TEXT("lowerarm_r"), FVector2D(0.835f, 0.34f));
	BonePositions.Add(TEXT("hand_r"), FVector2D(0.865f, 0.435f));
	
	// 오른쪽 손가락
	BonePositions.Add(TEXT("thumb_01_r"), FVector2D(0.905f, 0.425f));
	BonePositions.Add(TEXT("thumb_02_r"), FVector2D(0.92f, 0.435f));
	BonePositions.Add(TEXT("thumb_03_r"), FVector2D(0.935f, 0.445f));
	BonePositions.Add(TEXT("index_01_r"), FVector2D(0.90f, 0.455f));
	BonePositions.Add(TEXT("index_02_r"), FVector2D(0.915f, 0.465f));
	BonePositions.Add(TEXT("index_03_r"), FVector2D(0.93f, 0.475f));
	BonePositions.Add(TEXT("middle_01_r"), FVector2D(0.885f, 0.46f));
	BonePositions.Add(TEXT("middle_02_r"), FVector2D(0.90f, 0.47f));
	BonePositions.Add(TEXT("middle_03_r"), FVector2D(0.915f, 0.48f));
	BonePositions.Add(TEXT("ring_01_r"), FVector2D(0.87f, 0.46f));
	BonePositions.Add(TEXT("ring_02_r"), FVector2D(0.885f, 0.47f));
	BonePositions.Add(TEXT("ring_03_r"), FVector2D(0.90f, 0.48f));
	BonePositions.Add(TEXT("pinky_01_r"), FVector2D(0.855f, 0.455f));
	BonePositions.Add(TEXT("pinky_02_r"), FVector2D(0.87f, 0.465f));
	BonePositions.Add(TEXT("pinky_03_r"), FVector2D(0.885f, 0.475f));
	
	// === 왼쪽 다리 (실루엣 위치에 맞춤) ===
	BonePositions.Add(TEXT("thigh_l"), FVector2D(0.38f, 0.50f));
	BonePositions.Add(TEXT("calf_l"), FVector2D(0.365f, 0.66f));
	BonePositions.Add(TEXT("foot_l"), FVector2D(0.35f, 0.77f));
	BonePositions.Add(TEXT("ball_l"), FVector2D(0.32f, 0.79f));
	BonePositions.Add(TEXT("toe_l"), FVector2D(0.30f, 0.785f));
	
	// === 오른쪽 다리 ===
	BonePositions.Add(TEXT("thigh_r"), FVector2D(0.62f, 0.50f));
	BonePositions.Add(TEXT("calf_r"), FVector2D(0.635f, 0.66f));
	BonePositions.Add(TEXT("foot_r"), FVector2D(0.65f, 0.77f));
	BonePositions.Add(TEXT("ball_r"), FVector2D(0.68f, 0.79f));
	BonePositions.Add(TEXT("toe_r"), FVector2D(0.70f, 0.785f));
	
	// === IK 컨트롤러 (실루엣 바깥쪽에 배치) ===
	// 왼쪽 IK
	BonePositions.Add(TEXT("hand_l_ik"), FVector2D(0.04f, 0.44f));
	BonePositions.Add(TEXT("arm_l_pv"), FVector2D(0.08f, 0.28f));
	BonePositions.Add(TEXT("foot_l_ik"), FVector2D(0.25f, 0.78f));
	BonePositions.Add(TEXT("leg_l_pv"), FVector2D(0.28f, 0.60f));
	BonePositions.Add(TEXT("ball_l_ik"), FVector2D(0.25f, 0.80f));
	
	// 오른쪽 IK
	BonePositions.Add(TEXT("hand_r_ik"), FVector2D(0.96f, 0.44f));
	BonePositions.Add(TEXT("arm_r_pv"), FVector2D(0.92f, 0.28f));
	BonePositions.Add(TEXT("foot_r_ik"), FVector2D(0.75f, 0.78f));
	BonePositions.Add(TEXT("leg_r_pv"), FVector2D(0.72f, 0.60f));
	BonePositions.Add(TEXT("ball_r_ik"), FVector2D(0.75f, 0.80f));
	
	// === Curl 컨트롤러 (손 바깥쪽) ===
	BonePositions.Add(TEXT("thumb_curl_l"), FVector2D(0.05f, 0.42f));
	BonePositions.Add(TEXT("index_curl_l"), FVector2D(0.05f, 0.455f));
	BonePositions.Add(TEXT("middle_curl_l"), FVector2D(0.05f, 0.47f));
	BonePositions.Add(TEXT("ring_curl_l"), FVector2D(0.05f, 0.485f));
	BonePositions.Add(TEXT("pinky_curl_l"), FVector2D(0.05f, 0.50f));
	
	BonePositions.Add(TEXT("thumb_curl_r"), FVector2D(0.95f, 0.42f));
	BonePositions.Add(TEXT("index_curl_r"), FVector2D(0.95f, 0.455f));
	BonePositions.Add(TEXT("middle_curl_r"), FVector2D(0.95f, 0.47f));
	BonePositions.Add(TEXT("ring_curl_r"), FVector2D(0.95f, 0.485f));
	BonePositions.Add(TEXT("pinky_curl_r"), FVector2D(0.95f, 0.50f));
	
	// === Foot 보조 컨트롤러 ===
	BonePositions.Add(TEXT("heel_l"), FVector2D(0.30f, 0.81f));
	BonePositions.Add(TEXT("foot_bk1_l"), FVector2D(0.28f, 0.82f));
	BonePositions.Add(TEXT("foot_roll_l"), FVector2D(0.26f, 0.81f));
	BonePositions.Add(TEXT("roll_blend_l"), FVector2D(0.24f, 0.82f));
	BonePositions.Add(TEXT("tip_l"), FVector2D(0.27f, 0.80f));
	
	BonePositions.Add(TEXT("heel_r"), FVector2D(0.70f, 0.81f));
	BonePositions.Add(TEXT("foot_bk1_r"), FVector2D(0.72f, 0.82f));
	BonePositions.Add(TEXT("foot_roll_r"), FVector2D(0.74f, 0.81f));
	BonePositions.Add(TEXT("roll_blend_r"), FVector2D(0.76f, 0.82f));
	BonePositions.Add(TEXT("tip_r"), FVector2D(0.73f, 0.80f));
	
	// ★★★ 모든 컨트롤러 수집 ★★★
	struct F2DPickerInfo
	{
		FName ControlName;
		FName SpaceName;
		bool bIsMainBone;
		FVector2D ScreenPos;
		bool bHasFixedPos;
	};
	TArray<F2DPickerInfo> MainControllers;
	TMap<FString, TArray<F2DPickerInfo>> SecondaryByParent;  // 부모 본 이름 → 세컨더리 목록
	
	// Space 이름에서 부모 본 이름 추출 함수
	auto GetParentBoneFromSpace = [](const FName& SpaceName) -> FString
	{
		FString SpaceStr = SpaceName.ToString().ToLower();
		// 예: "head_space" → "head", "pelvis_ctrl_space" → "pelvis"
		SpaceStr = SpaceStr.Replace(TEXT("_space"), TEXT(""));
		SpaceStr = SpaceStr.Replace(TEXT("_ctrl"), TEXT(""));
		return SpaceStr;
	};
	
	// ★★★ 디버그: 모든 컨트롤러 출력 ★★★
	UE_LOG(LogTemp, Warning, TEXT("========== [AnimPicker 2D] All Controllers =========="));
	int32 TotalCount = 0;
	for (const auto& Pair : AnimPickerControllersBySpace)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Space: %s (%d controllers)"), *Pair.Key.ToString(), Pair.Value.Num());
		for (const FAnimPickerControllerInfo& Info : Pair.Value)
		{
			UE_LOG(LogTemp, Warning, TEXT("    - %s"), *Info.ControlName.ToString());
			TotalCount++;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Total: %d controllers in %d spaces"), TotalCount, AnimPickerControllersBySpace.Num());
	UE_LOG(LogTemp, Warning, TEXT("====================================================="));
	
	// ★★★ 세컨더리 패턴 정의 (의류/장식 컨트롤러만 세컨더리) ★★★
	auto IsSecondaryPattern = [](const FString& NameLower) -> bool
	{
		// 의류/장식 컨트롤러만 세컨더리
		if (NameLower.Contains(TEXT("cape")) || NameLower.Contains(TEXT("cloak")) || 
			NameLower.Contains(TEXT("skirt")) || NameLower.Contains(TEXT("armor")) ||
			NameLower.Contains(TEXT("hair")) || NameLower.Contains(TEXT("ribbon")) ||
			NameLower.Contains(TEXT("belt")) || NameLower.Contains(TEXT("pouch")) ||
			NameLower.Contains(TEXT("cloth")) || NameLower.Contains(TEXT("accessory")))
		{
			return true;
		}
		return false;
	};
	
	for (const auto& Pair : AnimPickerControllersBySpace)
	{
		for (const FAnimPickerControllerInfo& Info : Pair.Value)
		{
			FString NameStr = Info.ControlName.ToString();
			FString NameLower = NameStr.ToLower();
			
			// ★ 필터링: _ctrl 또는 _switch로 끝나는 것만 ★
			if (!NameLower.EndsWith(TEXT("_ctrl")) && !NameLower.EndsWith(TEXT("_switch")))
			{
				continue;
			}
			
			// ★★★ 메인/세컨더리 분류: 이름 패턴 기반 ★★★
			bool bIsMainBone = !IsSecondaryPattern(NameLower);
			
			F2DPickerInfo PickerInfo;
			PickerInfo.ControlName = Info.ControlName;
			PickerInfo.SpaceName = Pair.Key;
			PickerInfo.bIsMainBone = bIsMainBone;
			PickerInfo.bHasFixedPos = false;
			
			if (bIsMainBone)
			{
				// 메인 본: 고정 위치 찾기
				FString MatchedKey = TEXT("NONE");
				for (auto& PosEntry : BonePositions)
				{
					if (NameLower.Contains(PosEntry.Key))
					{
						PickerInfo.ScreenPos = FVector2D(PosEntry.Value.X * CanvasWidth, PosEntry.Value.Y * CanvasHeight);
						PickerInfo.bHasFixedPos = true;
						MatchedKey = PosEntry.Key;
						break;
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("[2D MAIN] %s -> Key=%s, HasPos=%d, Pos=(%.0f, %.0f)"), 
					*NameStr, *MatchedKey, PickerInfo.bHasFixedPos ? 1 : 0, PickerInfo.ScreenPos.X, PickerInfo.ScreenPos.Y);
				MainControllers.Add(PickerInfo);
			}
			else
			{
				// 세컨더리: 부모 본별로 그룹화
				FString ParentBone = GetParentBoneFromSpace(Pair.Key);
				UE_LOG(LogTemp, Warning, TEXT("[2D SEC] %s -> ParentBone=%s (from Space=%s)"), 
					*NameStr, *ParentBone, *Pair.Key.ToString());
				if (!SecondaryByParent.Contains(ParentBone))
				{
					SecondaryByParent.Add(ParentBone, TArray<F2DPickerInfo>());
				}
				SecondaryByParent[ParentBone].Add(PickerInfo);
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[2D SUMMARY] MainControllers=%d, SecondaryParents=%d"), MainControllers.Num(), SecondaryByParent.Num());
	
	// ★★★ 메인 본 위치 맵 (세컨더리 배치용) ★★★
	TMap<FString, FVector2D> MainBonePositions;
	
	// ★★★ 메인 본 버튼 생성 ★★★
	int32 MainCount = 0;
	int32 SecCount = 0;
	
	for (const F2DPickerInfo& Info : MainControllers)
	{
		if (!Info.bHasFixedPos) continue;
		
		FString LowerName = Info.ControlName.ToString().ToLower();
		
		// ★★★ 컨트롤러 종류별 크기 설정 (직사각형 형태) ★★★
		float BtnW = 32.0f * ZoomScale;
		float BtnH = 32.0f * ZoomScale;
		
		// ★ SPINE (가로로 긴 직사각형) ★
		if (LowerName.Contains(TEXT("spine")))
		{
			BtnW = 55.0f * ZoomScale;
			BtnH = 22.0f * ZoomScale;
		}
		// ★ HIPS / PELVIS (가로로 넓음) ★
		else if (LowerName.Contains(TEXT("hips")) || LowerName.Contains(TEXT("pelvis")))
		{
			BtnW = 65.0f * ZoomScale;
			BtnH = 26.0f * ZoomScale;
		}
		// ★ HEAD (정사각형, 큼) ★
		else if (LowerName.Contains(TEXT("head")))
		{
			BtnW = 42.0f * ZoomScale;
			BtnH = 42.0f * ZoomScale;
		}
		// ★ NECK (가로로 약간 긴) ★
		else if (LowerName.Contains(TEXT("neck")))
		{
			BtnW = 38.0f * ZoomScale;
			BtnH = 24.0f * ZoomScale;
		}
		// ★ BODY (정사각형, 큼) ★
		else if (LowerName.Contains(TEXT("body")))
		{
			BtnW = 45.0f * ZoomScale;
			BtnH = 30.0f * ZoomScale;
		}
		// ★ CLAVICLE (가로로 긴) ★
		else if (LowerName.Contains(TEXT("clavicle")))
		{
			BtnW = 40.0f * ZoomScale;
			BtnH = 20.0f * ZoomScale;
		}
		// ★ UPPERARM / THIGH (세로로 긴 직사각형) ★
		else if (LowerName.Contains(TEXT("upperarm")) || LowerName.Contains(TEXT("thigh")))
		{
			BtnW = 24.0f * ZoomScale;
			BtnH = 45.0f * ZoomScale;
		}
		// ★ LOWERARM / CALF (세로로 긴 직사각형) ★
		else if (LowerName.Contains(TEXT("lowerarm")) || LowerName.Contains(TEXT("calf")))
		{
			BtnW = 22.0f * ZoomScale;
			BtnH = 40.0f * ZoomScale;
		}
		// ★ HAND / FOOT (정사각형, 중간) ★
		else if (LowerName.Contains(TEXT("hand")) || LowerName.Contains(TEXT("foot")))
		{
			BtnW = 30.0f * ZoomScale;
			BtnH = 30.0f * ZoomScale;
		}
		// ★ BALL (정사각형, 작음) ★
		else if (LowerName.Contains(TEXT("ball")))
		{
			BtnW = 24.0f * ZoomScale;
			BtnH = 24.0f * ZoomScale;
		}
		// ★ IK 컨트롤러 (원형 느낌) ★
		else if (LowerName.Contains(TEXT("_ik")))
		{
			BtnW = 26.0f * ZoomScale;
			BtnH = 26.0f * ZoomScale;
		}
		// ★ PV 컨트롤러 (다이아몬드 느낌, 작음) ★
		else if (LowerName.Contains(TEXT("_pv")))
		{
			BtnW = 20.0f * ZoomScale;
			BtnH = 20.0f * ZoomScale;
		}
		// ★ 발 보조 컨트롤러 (heel, foot_roll, roll_blend, tip, foot_bk1) ★
		else if (LowerName.Contains(TEXT("heel")) || LowerName.Contains(TEXT("foot_roll")) ||
			LowerName.Contains(TEXT("roll_blend")) || LowerName.Contains(TEXT("tip_")) ||
			LowerName.Contains(TEXT("foot_bk")))
		{
			BtnW = 22.0f * ZoomScale;
			BtnH = 18.0f * ZoomScale;
		}
		// ★ Curl 컨트롤러 ★
		else if (LowerName.Contains(TEXT("curl")))
		{
			BtnW = 18.0f * ZoomScale;
			BtnH = 18.0f * ZoomScale;
		}
		// ★ 손가락/발가락 (작은 직사각형) ★
		else if (LowerName.Contains(TEXT("thumb")) || LowerName.Contains(TEXT("index")) || 
			LowerName.Contains(TEXT("middle")) || LowerName.Contains(TEXT("ring")) || 
			LowerName.Contains(TEXT("pinky")) || LowerName.Contains(TEXT("toe")))
		{
			BtnW = 14.0f * ZoomScale;
			BtnH = 18.0f * ZoomScale;
		}
		// ★ GLOBAL (큰 정사각형) ★
		else if (LowerName.Contains(TEXT("global")))
		{
			BtnW = 50.0f * ZoomScale;
			BtnH = 28.0f * ZoomScale;
		}
		
		// 색상: 왼쪽=파랑, 오른쪽=빨강, 중앙=노랑 (진한 색상)
		FLinearColor BtnColor(1.0f, 0.75f, 0.0f, 1.0f);  // 기본 진한 노랑/골드
		
		if (LowerName.Contains(TEXT("_l_")) || LowerName.EndsWith(TEXT("_l_ctrl")) || LowerName.Contains(TEXT("_l_fk")) || LowerName.Contains(TEXT("_l_ik")))
			BtnColor = FLinearColor(0.1f, 0.5f, 1.0f, 1.0f);  // 왼쪽 진한 파랑
		else if (LowerName.Contains(TEXT("_r_")) || LowerName.EndsWith(TEXT("_r_ctrl")) || LowerName.Contains(TEXT("_r_fk")) || LowerName.Contains(TEXT("_r_ik")))
			BtnColor = FLinearColor(1.0f, 0.2f, 0.15f, 1.0f);  // 오른쪽 진한 빨강
		
		// 선택 여부
		if (AnimPickerSelectedControllers.Contains(Info.ControlName))
		{
			BtnColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
			BtnW += 6.0f;
			BtnH += 6.0f;
		}
		
		float ScreenX = Info.ScreenPos.X;
		float ScreenY = Info.ScreenPos.Y;
		
		// 메인 본 위치 저장 (세컨더리 배치용)
		// 컨트롤러 이름에서 본 이름 추출 (예: head_ctrl → head)
		FString BoneName = LowerName.Replace(TEXT("_ctrl"), TEXT(""));
		MainBonePositions.Add(BoneName, FVector2D(ScreenX, ScreenY));
		
		FName ControlName = Info.ControlName;
		
		AnimPicker2DOverlay->AddSlot()
		.Padding(FMargin(ScreenX - BtnW/2, ScreenY - BtnH/2, 0, 0))
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(BtnW)
			.HeightOverride(BtnH)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(BtnColor)
				.ContentPadding(0)
				.OnClicked_Lambda([this, ControlName]() -> FReply
				{
					OnAnimPickerControllerClicked(ControlName);
					return FReply::Handled();
				})
				.ToolTipText(FText::FromName(ControlName))
			]
		];
		MainCount++;
	}
	
	// ★★★ 세컨더리 컨트롤러 (해당 부위 근처에 직관적 배치) ★★★
	// 모든 세컨더리를 하나의 배열로 수집
	TArray<F2DPickerInfo> AllSecondary;
	for (auto& SecPair : SecondaryByParent)
	{
		for (const F2DPickerInfo& Info : SecPair.Value)
		{
			AllSecondary.Add(Info);
		}
	}
	
	// 세컨더리 크기
	const float SecBtnSize = 14.0f * ZoomScale;
	const float SecSpacing = 16.0f * ZoomScale;
	
	// ★★★ 세컨더리 위치 계산 함수 - 이름 기반으로 해당 부위 근처 배치 ★★★
	// 카운터: 같은 영역에 여러 개 있을 때 오프셋용
	TMap<FString, int32> AreaCounters;
	
	auto GetSecondaryPosition = [&](const FString& NameLower) -> FVector2D
	{
		// 기본 위치 (중앙)
		float X = 0.5f;
		float Y = 0.5f;
		FString AreaKey = TEXT("default");
		
		// === 어깨 갑옷 (ShoulderArmor) - 어깨 바깥쪽 ===
		if (NameLower.Contains(TEXT("shoulderarmor")))
		{
			if (NameLower.Contains(TEXT("l_")))
			{
				AreaKey = TEXT("l_shoulder_armor");
				X = 0.12f;
				Y = 0.16f;
			}
			else
			{
				AreaKey = TEXT("r_shoulder_armor");
				X = 0.88f;
				Y = 0.16f;
			}
		}
		// === 망토 (Cape/Cloak) - 등/어깨 뒤쪽 ===
		else if (NameLower.Contains(TEXT("cape")) || NameLower.Contains(TEXT("cloak")))
		{
			if (NameLower.StartsWith(TEXT("l_cape")) || NameLower.StartsWith(TEXT("lb_cape")))
			{
				AreaKey = TEXT("l_cape");
				X = 0.18f;
				Y = 0.25f;
			}
			else if (NameLower.StartsWith(TEXT("r_cape")) || NameLower.StartsWith(TEXT("rb_cape")))
			{
				AreaKey = TEXT("r_cape");
				X = 0.82f;
				Y = 0.25f;
			}
			else if (NameLower.StartsWith(TEXT("lf_cape")))
			{
				AreaKey = TEXT("lf_cape");
				X = 0.28f;
				Y = 0.20f;
			}
			else if (NameLower.StartsWith(TEXT("rf_cape")))
			{
				AreaKey = TEXT("rf_cape");
				X = 0.72f;
				Y = 0.20f;
			}
			else if (NameLower.StartsWith(TEXT("b_cape")) || NameLower.Contains(TEXT("center")))
			{
				AreaKey = TEXT("b_cape");
				X = 0.50f;
				Y = 0.28f;
			}
			else
			{
				AreaKey = TEXT("cape_other");
				X = 0.50f;
				Y = 0.22f;
			}
		}
		// === 치마 (Skirt) - 골반/다리 주변 ===
		else if (NameLower.Contains(TEXT("skirt")))
		{
			if (NameLower.StartsWith(TEXT("l_skirt")) || NameLower.StartsWith(TEXT("lb_skirt")))
			{
				AreaKey = TEXT("l_skirt");
				X = 0.26f;
				Y = 0.52f;
			}
			else if (NameLower.StartsWith(TEXT("r_skirt")) || NameLower.StartsWith(TEXT("rb_skirt")))
			{
				AreaKey = TEXT("r_skirt");
				X = 0.74f;
				Y = 0.52f;
			}
			else if (NameLower.StartsWith(TEXT("lf_skirt")))
			{
				AreaKey = TEXT("lf_skirt");
				X = 0.32f;
				Y = 0.46f;
			}
			else if (NameLower.StartsWith(TEXT("rf_skirt")))
			{
				AreaKey = TEXT("rf_skirt");
				X = 0.68f;
				Y = 0.46f;
			}
			else if (NameLower.StartsWith(TEXT("f_skirt")))
			{
				AreaKey = TEXT("f_skirt");
				X = 0.50f;
				Y = 0.46f;
			}
			else if (NameLower.StartsWith(TEXT("b_skirt")))
			{
				AreaKey = TEXT("b_skirt");
				X = 0.50f;
				Y = 0.52f;
			}
			else
			{
				AreaKey = TEXT("skirt_other");
				X = 0.50f;
				Y = 0.50f;
			}
		}
		// === 머리카락 (Hair) - 머리 주변 ===
		else if (NameLower.Contains(TEXT("hair")))
		{
			if (NameLower.Contains(TEXT("l_")))
			{
				AreaKey = TEXT("l_hair");
				X = 0.38f;
				Y = 0.06f;
			}
			else if (NameLower.Contains(TEXT("r_")))
			{
				AreaKey = TEXT("r_hair");
				X = 0.62f;
				Y = 0.06f;
			}
			else
			{
				AreaKey = TEXT("hair");
				X = 0.50f;
				Y = 0.02f;
			}
		}
		// === 기타 ===
		else
		{
			AreaKey = TEXT("other");
			X = 0.50f;
			Y = 0.85f;
		}
		
		// 같은 영역에 여러 개 있으면 아래로 오프셋
		if (!AreaCounters.Contains(AreaKey))
		{
			AreaCounters.Add(AreaKey, 0);
		}
		int32 Idx = AreaCounters[AreaKey];
		AreaCounters[AreaKey] = Idx + 1;
		
		// 세로로 체인 배치
		Y += (Idx * SecSpacing / CanvasHeight);
		
		return FVector2D(X, Y);
	};
	
	for (int32 i = 0; i < AllSecondary.Num(); i++)
	{
		const F2DPickerInfo& Info = AllSecondary[i];
		FString NameLower = Info.ControlName.ToString().ToLower();
		
		// 위치 계산
		FVector2D NormPos = GetSecondaryPosition(NameLower);
		float ScreenX = NormPos.X * CanvasWidth;
		float ScreenY = NormPos.Y * CanvasHeight;
		
		// 세컨더리 색상 (타입 기반)
		FLinearColor BtnColor(0.6f, 0.4f, 0.8f, 1.0f);  // 기본 보라
		
		if (NameLower.Contains(TEXT("cape")) || NameLower.Contains(TEXT("cloak")))
			BtnColor = FLinearColor(0.5f, 0.35f, 0.9f, 1.0f);  // 보라
		else if (NameLower.Contains(TEXT("skirt")))
			BtnColor = FLinearColor(0.9f, 0.35f, 0.5f, 1.0f);  // 핑크
		else if (NameLower.Contains(TEXT("armor")))
			BtnColor = FLinearColor(0.3f, 0.6f, 0.9f, 1.0f);  // 시안
		else if (NameLower.Contains(TEXT("hair")))
			BtnColor = FLinearColor(0.9f, 0.6f, 0.3f, 1.0f);  // 오렌지
		
		float BtnW = SecBtnSize;
		float BtnH = SecBtnSize;
		
		// 선택 여부
		if (AnimPickerSelectedControllers.Contains(Info.ControlName))
		{
			BtnColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
			BtnW += 4.0f * ZoomScale;
			BtnH += 4.0f * ZoomScale;
		}
		
		FName ControlName = Info.ControlName;
		
		// 세컨더리 버튼
		AnimPicker2DOverlay->AddSlot()
		.Padding(FMargin(ScreenX - BtnW/2, ScreenY - BtnH/2, 0, 0))
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(BtnW)
			.HeightOverride(BtnH)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(BtnColor)
				.ContentPadding(0)
				.OnClicked_Lambda([this, ControlName]() -> FReply
				{
					OnAnimPickerControllerClicked(ControlName);
					return FReply::Handled();
				})
				.ToolTipText(FText::FromName(ControlName))
			]
		];
		SecCount++;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[AnimPicker 2D] Created %d main bones, %d secondary"), MainCount, SecCount);
}

// ============================================================================
// Anim Picker - 3D 뷰에서 컨트롤러 클릭
// ============================================================================
void SControlRigToolWidget::OnAnimPicker3DControllerClicked(FName ControlName, bool bCtrlDown)
{
	// 현재 인덱스 찾기
	int32 CurrentIndex = AnimPickerControllerOrder.IndexOfByKey(ControlName);
	
	if (bCtrlDown)
	{
		// Ctrl+클릭: 토글 (추가/제거)
		if (AnimPickerSelectedControllers.Contains(ControlName))
		{
			AnimPickerSelectedControllers.Remove(ControlName);
		}
		else
		{
			AnimPickerSelectedControllers.Add(ControlName);
		}
		AnimPickerLastSelectedIndex = CurrentIndex;
	}
	else
	{
		// 일반 클릭: 단일 선택 (기존 선택 해제)
		AnimPickerSelectedControllers.Empty();
		AnimPickerSelectedControllers.Add(ControlName);
		AnimPickerLastSelectedIndex = CurrentIndex;
	}
	
	// 시퀀서/에디터에서 실제 선택
	SelectControlsInEditor(AnimPickerSelectedControllers, !bCtrlDown);
	
	// UI 업데이트 (List View도 동기화)
	UpdateAnimPickerListViewUI();
	UpdateAnimPicker2DView();  // 2D 피커 선택 상태 업데이트
	
	// ★ 3D 뷰는 선택 상태만 업데이트 (마커 재생성 안 함 - 시점 리셋 방지) ★
	if (AnimPicker3DViewport.IsValid())
	{
		AnimPicker3DViewport->SetSelectedControllers(AnimPickerSelectedControllers);
	}
	
	// 상태 표시
	if (AnimPickerSelectedControllers.Num() == 1)
	{
		SetAnimPickerStatus(FString::Printf(TEXT("3D View - Selected: %s"), *ControlName.ToString()));
	}
	else
	{
		SetAnimPickerStatus(FString::Printf(TEXT("3D View - Selected: %d controllers"), AnimPickerSelectedControllers.Num()));
	}
	
	UE_LOG(LogTemp, Log, TEXT("[AnimPicker 3D] Selection: %d controllers"), AnimPickerSelectedControllers.Num());
}

// ============================================================================
// Anim Picker - 3D 뷰포트 업데이트 (본 위치는 RefSkeleton, Shape 정보는 Control Rig에서)
// ============================================================================
void SControlRigToolWidget::UpdateAnimPicker3DView()
{
	if (!AnimPicker3DViewport.IsValid())
	{
		return;
	}
	
	// Control Rig에서 Preview Mesh 가져와서 설정
	FString ControlRigPath = GetSelectedAnimPickerControlRigPath();
	USkeletalMesh* PreviewMesh = nullptr;
	
	if (!ControlRigPath.IsEmpty())
	{
		UControlRigBlueprint* ControlRigBP = LoadObject<UControlRigBlueprint>(nullptr, *ControlRigPath);
		if (ControlRigBP)
		{
			PreviewMesh = ControlRigBP->GetPreviewMesh();
			if (PreviewMesh)
			{
				AnimPicker3DViewport->SetSkeletalMesh(PreviewMesh);
				UE_LOG(LogTemp, Log, TEXT("[AnimPicker 3D] Set preview mesh: %s"), *PreviewMesh->GetName());
			}
		}
	}
	
	// ★★★ 본 위치: RefSkeleton에서, Shape 정보: Control Rig에서 ★★★
	TArray<TPair<FName, FTransform>> ControllerTransforms;
	TMap<FName, FLinearColor> ControllerColors;
	TMap<FName, FVector> ControllerShapeScales;
	TMap<FName, FName> ControllerShapeNames;  // Shape 모양 (Box, Sphere, Circle 등)
	
	// 본 이름 → 글로벌 위치 캐시 (스켈레탈 메쉬 기준)
	TMap<FName, FTransform> BoneTransformCache;
	if (PreviewMesh)
	{
		const FReferenceSkeleton& RefSkeleton = PreviewMesh->GetRefSkeleton();
		TArray<FTransform> ComponentSpaceTransforms;
		
		// 로컬 → 컴포넌트 스페이스 변환
		const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
		ComponentSpaceTransforms.SetNum(RefPose.Num());
		
		for (int32 BoneIdx = 0; BoneIdx < RefPose.Num(); BoneIdx++)
		{
			int32 ParentIdx = RefSkeleton.GetParentIndex(BoneIdx);
			if (ParentIdx == INDEX_NONE)
			{
				ComponentSpaceTransforms[BoneIdx] = RefPose[BoneIdx];
			}
			else
			{
				ComponentSpaceTransforms[BoneIdx] = RefPose[BoneIdx] * ComponentSpaceTransforms[ParentIdx];
			}
			
			FName BoneName = RefSkeleton.GetBoneName(BoneIdx);
			BoneTransformCache.Add(BoneName, ComponentSpaceTransforms[BoneIdx]);
		}
		
		UE_LOG(LogTemp, Log, TEXT("[AnimPicker 3D] Cached %d bone transforms from RefSkeleton"), BoneTransformCache.Num());
	}
	
	// 컨트롤러 정보 수집 - 모든 _ctrl 컨트롤러 포함!
	for (const auto& Pair : AnimPickerControllersBySpace)
	{
		for (const FAnimPickerControllerInfo& Info : Pair.Value)
		{
			FString CtrlName = Info.ControlName.ToString();
			FString NameLower = CtrlName.ToLower();
			
			// ★ 필터링: _ctrl 또는 _switch로 끝나는 것만 ★
			if (!NameLower.EndsWith(TEXT("_ctrl")) && !NameLower.EndsWith(TEXT("_switch")))
			{
				continue;
			}
			
			// ★★★ FK/IK Switch 컨트롤러 특별 처리 ★★★
			bool bIsSwitch = NameLower.Contains(TEXT("switch"));
			FString BoneNameStr;
			
			if (bIsSwitch)
			{
				// switch 컨트롤러: 관련 본 이름 추출
				// arm_l_fk_ik_switch → upperarm_l
				// leg_l_fk_ik_switch → thigh_l
				if (NameLower.Contains(TEXT("arm_l")))
				{
					BoneNameStr = TEXT("upperarm_l");
				}
				else if (NameLower.Contains(TEXT("arm_r")))
				{
					BoneNameStr = TEXT("upperarm_r");
				}
				else if (NameLower.Contains(TEXT("leg_l")))
				{
					BoneNameStr = TEXT("thigh_l");
				}
				else if (NameLower.Contains(TEXT("leg_r")))
				{
					BoneNameStr = TEXT("thigh_r");
				}
				else
				{
					BoneNameStr = TEXT("pelvis");  // 기본
				}
				
				UE_LOG(LogTemp, Warning, TEXT("[AnimPicker 3D] Switch '%s' -> Bone '%s'"), *CtrlName, *BoneNameStr);
			}
			else
			{
				// 일반 컨트롤러: 이름에서 본 이름 추출
				BoneNameStr = CtrlName;
				BoneNameStr.RemoveFromEnd(TEXT("_ctrl"));
				
				// FK/IK 접미사 제거
				if (BoneNameStr.EndsWith(TEXT("_fk")))
				{
					BoneNameStr.RemoveFromEnd(TEXT("_fk"));
				}
				else if (BoneNameStr.EndsWith(TEXT("_ik")))
				{
					BoneNameStr.RemoveFromEnd(TEXT("_ik"));
				}
			}
			
			FName BoneName = FName(*BoneNameStr);
			FTransform BoneTransform = FTransform::Identity;
			bool bFoundBone = false;
			
			// RefSkeleton에서 본 위치 찾기
			if (BoneTransformCache.Contains(BoneName))
			{
				BoneTransform = BoneTransformCache[BoneName];
				bFoundBone = true;
			}
			else
			{
				// 대소문자 무시 검색
				for (const auto& CachePair : BoneTransformCache)
				{
					FString CacheBoneLower = CachePair.Key.ToString().ToLower();
					if (CacheBoneLower == BoneNameStr.ToLower())
					{
						BoneTransform = CachePair.Value;
						bFoundBone = true;
						break;
					}
					// 부분 매칭도 시도 (예: spine_01 in Spine_01_drv)
					if (CacheBoneLower.Contains(BoneNameStr.ToLower()) || BoneNameStr.ToLower().Contains(CacheBoneLower))
					{
						BoneTransform = CachePair.Value;
						bFoundBone = true;
						break;
					}
				}
			}
			
			// ★ 본을 못 찾아도 Control Rig의 Transform + ShapeOffset 사용 ★
			FVector FinalLocation;
			if (bFoundBone)
			{
				// 본 위치 + ShapeTransform 오프셋
				FinalLocation = BoneTransform.GetLocation() + Info.ShapeTransform.GetLocation();
				
				// ★★★ Switch 컨트롤러는 본 옆에 오프셋 적용 ★★★
				if (bIsSwitch)
				{
					// 왼쪽/오른쪽에 따라 X 방향 오프셋
					float SideOffset = 30.0f;  // 본에서 30cm 옆
					if (NameLower.Contains(TEXT("_l_")))
					{
						FinalLocation.Y += SideOffset;  // 왼쪽은 +Y
					}
					else if (NameLower.Contains(TEXT("_r_")))
					{
						FinalLocation.Y -= SideOffset;  // 오른쪽은 -Y
					}
					FinalLocation.Z += 15.0f;  // 약간 위로
				}
			}
			else
			{
				// 본 못 찾음 → Control Rig Transform + ShapeOffset 사용
				FinalLocation = Info.Transform.GetLocation() + Info.ShapeTransform.GetLocation();
				UE_LOG(LogTemp, Warning, TEXT("[AnimPicker 3D] Bone not found for '%s' (tried: %s), using ControlRig transform"), *CtrlName, *BoneNameStr);
			}
			
			FTransform FinalTransform = bFoundBone ? BoneTransform : Info.Transform;
			FinalTransform.SetLocation(FinalLocation);
			
			ControllerTransforms.Add(TPair<FName, FTransform>(Info.ControlName, FinalTransform));
			
			// ★ Control Rig의 원본 색상 사용 ★
			ControllerColors.Add(Info.ControlName, Info.Color);
			
			// ★ Control Rig의 Shape 스케일 사용 ★
			ControllerShapeScales.Add(Info.ControlName, Info.ShapeScale);
			
			// ★ Control Rig의 Shape 이름 사용 ★
			ControllerShapeNames.Add(Info.ControlName, Info.ShapeName);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[AnimPicker 3D] Collected %d controllers (Bone pos from RefSkeleton, Shape from ControlRig)"), ControllerTransforms.Num());
	
	// 3D 뷰포트에 마커 설정 (Shape 스케일 + Shape 이름 정보 포함)
	AnimPicker3DViewport->SetControllerMarkersWithShapeInfo(ControllerTransforms, ControllerColors, ControllerShapeScales, ControllerShapeNames);
	
	UE_LOG(LogTemp, Log, TEXT("[AnimPicker 3D] Updated with %d controllers"), ControllerTransforms.Num());
}

// ============================================================================
// Anim Picker - 시퀀서/에디터에서 실제 컨트롤러 선택
// ============================================================================
void SControlRigToolWidget::SelectControlsInEditor(const TSet<FName>& ControlNames, bool bClearSelection)
{
	// Control Rig Edit Mode 가져오기
	FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(
		GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
	
	if (!EditMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimPicker] Control Rig Edit Mode not active. Open Sequencer with a Control Rig track first."));
		return;
	}
	
	// 활성 Control Rig 가져오기
	TArray<UControlRig*> ActiveControlRigs = EditMode->GetControlRigsArray(true);
	if (ActiveControlRigs.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimPicker] No active Control Rigs found."));
		return;
	}
	
	for (UControlRig* ControlRig : ActiveControlRigs)
	{
		if (!ControlRig) continue;
		
		// 선택 초기화 (필요시)
		if (bClearSelection)
		{
			ControlRig->ClearControlSelection();
		}
		
		// 컨트롤러 선택
		for (const FName& ControlName : ControlNames)
		{
			ControlRig->SelectControl(ControlName, true);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[AnimPicker] Selected %d controls in editor"), ControlNames.Num());
}

// ============================================================================
// Anim Picker - List View UI 업데이트
// ============================================================================
void SControlRigToolWidget::UpdateAnimPickerListViewUI()
{
	if (!AnimPickerListViewBox.IsValid())
	{
		return;
	}
	
	AnimPickerListViewBox->ClearChildren();
	
	if (AnimPickerControllersBySpace.Num() == 0)
	{
		AnimPickerListViewBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(0, 50)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AnimPickerNoControllers", "Select a Control Rig and click 'Load' to view controllers"))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
			.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.45f))
		];
		return;
	}
	
	// ★★★ 복사 버튼 추가 ★★★
	AnimPickerListViewBox->AddSlot()
	.AutoHeight()
	.Padding(4, 4, 4, 8)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
		.ContentPadding(FMargin(12, 6))
		.OnClicked_Lambda([this]() -> FReply
		{
			// 모든 컨트롤러 이름을 텍스트로 수집
			FString AllControllers;
			int32 TotalCount = 0;
			
			for (const auto& Pair : AnimPickerControllersBySpace)
			{
				AllControllers += FString::Printf(TEXT("=== %s (%d) ===\n"), *Pair.Key.ToString(), Pair.Value.Num());
				for (const FAnimPickerControllerInfo& Info : Pair.Value)
				{
					AllControllers += Info.ControlName.ToString() + TEXT("\n");
					TotalCount++;
				}
				AllControllers += TEXT("\n");
			}
			
			AllControllers = FString::Printf(TEXT("Total: %d controllers\n\n"), TotalCount) + AllControllers;
			
			// 클립보드에 복사
			FPlatformApplicationMisc::ClipboardCopy(*AllControllers);
			
			SetAnimPickerStatus(FString::Printf(TEXT("Copied %d controller names to clipboard"), TotalCount));
			
			return FReply::Handled();
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("GenericCommands.Copy"))
				.ColorAndOpacity(FLinearColor::White)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CopyAllControllers", "Copy All Controller Names"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
		]
	];
	
	// Space별로 표시
	for (auto& Pair : AnimPickerControllersBySpace)
	{
		FName SpaceName = Pair.Key;
		TArray<FAnimPickerControllerInfo>& Controllers = Pair.Value;
		bool bExpanded = AnimPickerSpaceExpanded.FindOrAdd(SpaceName, true);
		FLinearColor SpaceColor = GetColorForSpace(SpaceName);
		
		// Space 헤더
		AnimPickerListViewBox->AddSlot()
		.AutoHeight()
		.Padding(0, 4, 0, 2)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "FlatButton")
			.ContentPadding(FMargin(8, 6))
			.OnClicked(this, &SControlRigToolWidget::OnAnimPickerSpaceToggleClicked, SpaceName)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(bExpanded ? "TreeArrow_Expanded" : "TreeArrow_Collapsed"))
					.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.65f))
					.DesiredSizeOverride(FVector2D(10, 10))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(SpaceColor)
					.Padding(0)
					[
						SNew(SBox)
						.WidthOverride(12)
						.HeightOverride(12)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromName(SpaceName))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.95f))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("(%d)"), Controllers.Num())))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
				]
			]
		];
		
		// 컨트롤러 목록 (펼쳐진 경우만)
		if (bExpanded)
		{
			for (const FAnimPickerControllerInfo& Info : Controllers)
			{
				FLinearColor CtrlColor = Info.Color;
				bool bIsSelected = AnimPickerSelectedControllers.Contains(Info.ControlName);
				
				// 선택된 경우 배경색 변경
				FLinearColor BgColor = bIsSelected 
					? FLinearColor(0.15f, 0.35f, 0.55f, 1.0f)  // 선택됨 - 파란 배경
					: FLinearColor(0.03f, 0.03f, 0.04f, 1.0f); // 기본 - 어두운 배경
				
				AnimPickerListViewBox->AddSlot()
				.AutoHeight()
				.Padding(20, 1, 0, 1)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(BgColor)
					.Padding(0)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton")
						.ContentPadding(FMargin(8, 4))
						.OnClicked(this, &SControlRigToolWidget::OnAnimPickerControllerClicked, Info.ControlName)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
								.BorderBackgroundColor(CtrlColor)
								.Padding(0)
								[
									SNew(SBox)
									.WidthOverride(10)
									.HeightOverride(10)
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10, 0, 0, 0)
							[
								SNew(STextBlock)
								.Text(FText::FromName(Info.ControlName))
								.Font(FCoreStyle::GetDefaultFontStyle(bIsSelected ? "Bold" : "Regular", 10))
								.ColorAndOpacity(bIsSelected ? FLinearColor::White : (CtrlColor * 0.9f + FLinearColor(0.1f, 0.1f, 0.1f)))
							]
						]
					]
				];
			}
		}
	}
}

// ============================================================================
// Anim Picker - 상태 메시지 설정
// ============================================================================
void SControlRigToolWidget::SetAnimPickerStatus(const FString& Message)
{
	if (AnimPickerStatusText.IsValid())
	{
		AnimPickerStatusText->SetText(FText::FromString(Message));
	}
}

#undef LOCTEXT_NAMESPACE
