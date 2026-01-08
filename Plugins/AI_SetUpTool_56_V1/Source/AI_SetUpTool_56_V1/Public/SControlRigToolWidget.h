#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "AssetThumbnail.h"

class UControlRigBlueprint;
class USkeletalMesh;
class SEditableTextBox;
class STextBlock;
class SBox;
class SVerticalBox;
class SScrollBox;
class SWidgetSwitcher;

// ============================================================================
// 워크플로우 단계
// ============================================================================
enum class EControlRigWorkflowStep : uint8
{
	Step1_Setup,        // 메시/템플릿 선택, AI 매핑
	Step2_BodyRig,      // Body Control Rig 생성 완료
	Step3_SelectBones,  // 본 선택 (세컨더리/헬퍼 구분)
	Step4_Complete      // 최종 Control Rig 생성 완료
};

// ============================================================================
// 본 분류 타입 (라디오 버튼용)
// ============================================================================
enum class EBoneClassification : uint8
{
	Helper,     // 컨트롤러 생성 X
	Secondary,  // 일반 세컨더리 (AI_Setup, AI_Forward, AI_Backward)
	Weapon      // 무기 본 (AI_Setup_Weapon, AI_Forward_Weapon, AI_Backward_Weapon)
};

// ============================================================================
// Kawaii Physics 태그 정보
// ============================================================================
struct FKawaiiTag
{
	FString Name;           // 태그 이름 (cloak, skirt 등)
	FLinearColor Color;     // 태그 컬러
	
	FKawaiiTag()
		: Name(TEXT("NewTag"))
		, Color(FLinearColor(0.6f, 0.7f, 0.2f, 1.0f))  // 올리브색 기본
	{}
	
	FKawaiiTag(const FString& InName, const FLinearColor& InColor)
		: Name(InName)
		, Color(InColor)
	{}
};

// ============================================================================
// Kawaii Physics 본 표시 정보
// ============================================================================
struct FKawaiiBoneDisplayInfo
{
	FName BoneName;
	int32 BoneIndex;
	int32 ParentIndex;
	int32 Depth;
	bool bHasSkinWeight;           // 웨이트 있음
	bool bIsSecondary;             // Control Rig에서 Secondary로 선택됨
	int32 TagIndex;                // 할당된 태그 인덱스 (-1이면 없음)
	bool bExpanded;                // 접기/펼치기 상태
	bool bHasChildren;             // 자식 본이 있는지 여부
	
	FKawaiiBoneDisplayInfo()
		: BoneIndex(INDEX_NONE)
		, ParentIndex(INDEX_NONE)
		, Depth(0)
		, bHasSkinWeight(true)
		, bIsSecondary(false)
		, TagIndex(INDEX_NONE)
		, bExpanded(true)
		, bHasChildren(false)
	{}
};

// ============================================================================
// 본 정보 구조체
// ============================================================================
struct FBoneDisplayInfo
{
	FName BoneName;
	int32 BoneIndex;
	int32 ParentIndex;
	int32 Depth;                     // 계층 깊이
	bool bIsZeroBone;                // 제로본 (빨강)
	bool bHasSkinWeight;             // 스킨 웨이트 있음
	EBoneClassification Classification;  // Helper/Secondary/Weapon
	bool bIsSelected;                // 현재 선택됨 (Shift 다중선택용)
	
	FBoneDisplayInfo()
		: BoneIndex(INDEX_NONE)
		, ParentIndex(INDEX_NONE)
		, Depth(0)
		, bIsZeroBone(false)
		, bHasSkinWeight(true)
		, Classification(EBoneClassification::Helper)
		, bIsSelected(false)
	{}
};

class SControlRigToolWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SControlRigToolWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SControlRigToolWidget();

private:
	struct FAssetInfo
	{
		FString Name;
		FString Path;
	};

	// UI 생성
	TSharedRef<SWidget> CreateTabBar();                // 탭 바
	TSharedRef<SWidget> CreateControlRigTab();         // Control Rig 탭 콘텐츠
	TSharedRef<SWidget> CreateIKRigTab();              // IK Rig 탭 콘텐츠
	TSharedRef<SWidget> CreateKawaiiPhysicsTab();      // Kawaii Physics 탭 콘텐츠
	TSharedRef<SWidget> CreatePhysicsAssetTab();       // Physics Asset 탭 콘텐츠
	TSharedRef<SWidget> CreateTemplateSection();
	TSharedRef<SWidget> CreateMeshSection();
	TSharedRef<SWidget> CreateOutputSection();
	TSharedRef<SWidget> CreateButtonSection();
	TSharedRef<SWidget> CreateBoneSelectionSection();  // 본 선택 UI
	TSharedRef<SWidget> CreateIKRigSection();  // IK Rig 생성 섹션
	
	// 탭 전환
	void OnTabChanged(int32 NewTabIndex);
	FSlateColor GetTabButtonColor(int32 TabIndex) const;
	EVisibility GetTabContentVisibility(int32 TabIndex) const;
	
	// 전체 새로고침
	FReply OnResetAllClicked();

	// 데이터
	void LoadAssetData();
	void RefreshData();

	// 썸네일
	void UpdateTemplateThumbnail();
	void UpdateMeshThumbnail();

	// 템플릿 콤보박스
	TSharedRef<SWidget> OnGenerateTemplateWidget(TSharedPtr<FString> InItem);
	void OnTemplateSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetSelectedTemplateName() const;

	// 메시 콤보박스
	TSharedRef<SWidget> OnGenerateMeshWidget(TSharedPtr<FString> InItem);
	void OnMeshSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetSelectedMeshName() const;

	// 버튼 액션
	FReply OnRefreshClicked();
	FReply OnUseSelectedTemplateClicked();
	FReply OnUseSelectedMeshClicked();
	FReply OnAIBoneMappingClicked();
	FReply OnCreateBodyControlRigClicked();   // 이름 변경
	FReply OnCreateFinalControlRigClicked();  // 새로 추가 (최종)
	FReply OnBrowseFolderClicked();
	FReply OnApproveMappingClicked();

	// 핵심 기능
	void RequestAIBoneMapping();
	bool CreateBodyControlRig();       // Body Control Rig만 생성 (저장 X)
	bool CreateFinalControlRig();      // 세컨더리 추가 + 최종 저장
	void RemapBoneReferences(class UControlRigBlueprint* Rig);
	void SendApproveRequest();
	
	// 세컨더리 전용 Control Rig 생성 (템플릿 없이)
	FReply OnCreateSecondaryOnlyControlRigClicked();
	bool CreateSecondaryOnlyControlRig();
	
	// 세컨더리 컨트롤러 생성
	void CreateSecondaryControlsFromSelection(class UControlRigBlueprint* Rig, class USkeletalMesh* Mesh);
	bool IsZeroBone(const FString& BoneName) const;
	bool IsAccessoryBone(const FString& BoneName) const;
	bool IsHelperBone(const FString& BoneName) const;
	
	// 체인 분석 및 Space/Control 생성
	FName FindZeroBoneParent(const FName& BoneName, const FReferenceSkeleton& RefSkel) const;
	void BuildSecondaryChains(class USkeletalMesh* Mesh, TMap<FName, TArray<FName>>& OutChainsBySpace);
	void CreateSpaceNull(class URigHierarchyController* HC, const FName& SpaceName, const FTransform& Transform);
	void CreateChainControls(class URigHierarchyController* HC, class URigHierarchy* Hierarchy, 
		const FName& SpaceName, const TArray<FName>& ChainBones, const FReferenceSkeleton& RefSkel);
	bool HasSkinWeight(class USkeletalMesh* Mesh, const FName& BoneName) const;
	
	// 본 선택 UI 관련
	void BuildBoneDisplayList();
	void UpdateBoneSelectionUI();
	void OnBoneClassificationChanged(EBoneClassification NewClassification, int32 BoneIndex);
	void OnBoneRowClicked(int32 BoneIndex, const FModifierKeysState& ModifierKeys);
	TSharedRef<SWidget> CreateBoneRow(int32 Index);
	
	// Weapon 본 처리
	void CreateWeaponControlsFromSelection(class UControlRigBlueprint* Rig, class USkeletalMesh* Mesh);
	void CreateWeaponSpaceAndControls(class URigHierarchyController* HC, class URigHierarchy* Hierarchy,
		bool bIsLeft, const TArray<FName>& WeaponBones, const FReferenceSkeleton& RefSkel);
	void ConnectWeaponFunctionNodes(class UControlRigBlueprint* Rig, 
		bool bIsLeft, const FName& WeaponSpaceName, const TArray<FName>& WeaponBones, const TArray<FName>& WeaponCtrls);
	
	// 헬퍼
	FString GetSelectedTemplatePath() const;
	FString GetSelectedMeshPath() const;
	void SetStatus(const FString& Message);
	void DisplayMappingResults();
	void UpdateWorkflowUI();  // 워크플로우 단계에 따라 UI 업데이트
	
	// 분류 피드백 (AI 학습용)
	void SendClassificationFeedback(const FString& BoneName, const FString& Classification);
	
	// RigVM 함수 노드 연결 (AI_Setup, AI_Forward, AI_Backward)
	void ConnectSecondaryFunctionNodes(class UControlRigBlueprint* Rig, 
		const TMap<FName, TArray<FName>>& ChainsBySpace);
	class URigVMNode* FindLastAIFunctionNode(class URigVMGraph* Graph, const FString& FunctionPrefix);
	class URigVMNode* AddFunctionReferenceNode(class URigVMController* Controller, 
		const FString& FunctionName, const FVector2D& Position, FString& OutDebugInfo);
	void SetFunctionNodePins(class URigVMController* Controller, class URigVMNode* FuncNode,
		const FName& BoneName, const FName& SpaceName, 
		const TArray<FName>& Bones, const TArray<FName>& Controls);

private:
	// 워크플로우 상태
	EControlRigWorkflowStep CurrentStep = EControlRigWorkflowStep::Step1_Setup;
	TWeakObjectPtr<UControlRigBlueprint> PendingControlRig;  // 아직 저장 안 된 임시 Control Rig
	FString PendingOutputPath;  // 저장할 경로
	
	// 에셋 데이터
	TArray<FAssetInfo> ControlRigs;
	TArray<FAssetInfo> SkeletalMeshes;

	// 콤보박스 옵션
	TArray<TSharedPtr<FString>> TemplateOptions;
	TArray<TSharedPtr<FString>> MeshOptions;
	TSharedPtr<FString> SelectedTemplate;
	TSharedPtr<FString> SelectedMesh;

	// 위젯
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TemplateComboBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> MeshComboBox;

	// 썸네일
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<FAssetThumbnail> TemplateThumbnail;
	TSharedPtr<FAssetThumbnail> MeshThumbnail;
	TSharedPtr<SBox> TemplateThumbnailBox;
	TSharedPtr<SBox> MeshThumbnailBox;

	// 입력 필드
	TSharedPtr<SEditableTextBox> OutputNameBox;
	TSharedPtr<SEditableTextBox> OutputFolderBox;

	// 상태/결과
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SVerticalBox> MappingResultBox;
	
	// 본 선택 UI
	TSharedPtr<SVerticalBox> BoneSelectionBox;
	TSharedPtr<SScrollBox> BoneScrollBox;
	TArray<FBoneDisplayInfo> BoneDisplayList;
	int32 LastSelectedBoneIndex = INDEX_NONE;  // Shift 다중선택용

	// 버튼 위젯 (활성화/비활성화용)
	TSharedPtr<SButton> BodyRigButton;
	TSharedPtr<SButton> FinalCreateButton;
	TSharedPtr<SButton> SecondaryOnlyButton;  // 세컨더리 전용 Control Rig 버튼

	// 매핑 데이터
	TMap<FName, FName> LastBoneMapping;  // target -> source
	TWeakObjectPtr<USkeletalMesh> CachedMesh;
	
	// 세컨더리 컨트롤러 생성 결과
	int32 LastSecondaryControlCount = 0;
	
	// 본별 버텍스 기반 Shape Transform 정보
	struct FBoneShapeInfo
	{
		FVector Scale = FVector(0.3f, 0.3f, 0.3f);  // XYZ 각각 다른 스케일
		FVector Offset = FVector::ZeroVector;  // 메쉬 바깥으로 이동할 오프셋
		FVector AverageNormal = FVector(0.0f, 0.0f, 1.0f);  // 버텍스 노멀 평균 (바깥 방향)
	};
	TMap<FName, FBoneShapeInfo> BoneShapeInfoMap;
	void CalculateBoneShapeInfos(class USkeletalMesh* Mesh);
	FBoneShapeInfo GetBoneShapeInfo(const FName& BoneName) const;
	
	// 바디 컨트롤러 오토스케일 적용 (템플릿에서 복사된 컨트롤러들)
	void ApplyAutoScaleToBodyControls(class UControlRigBlueprint* Rig, class USkeletalMesh* Mesh);
	
	// IK Rig 생성 관련
	FReply OnCreateIKRigClicked();
	void CreateIKRigFromTemplate();
	TSharedRef<SWidget> OnGenerateIKRigTemplateWidget(TSharedPtr<FString> InItem);
	void OnIKRigTemplateSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetSelectedIKRigTemplateName() const;
	void LoadIKRigTemplates();
	
	// IK Rig 탭 전용 함수
	TSharedRef<SWidget> OnGenerateIKMeshWidget(TSharedPtr<FString> InItem);
	void OnIKMeshSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetSelectedIKMeshName() const;
	FReply OnUseSelectedIKTemplateClicked();
	FReply OnUseSelectedIKMeshClicked();
	FReply OnIKAIBoneMappingClicked();
	FReply OnIKApproveMappingClicked();
	FReply OnIKBrowseFolderClicked();
	FReply OnMakeTPoseClicked();
	void CreateTPoseAnimSequence();
	void RequestIKAIBoneMapping();
	
	// ============================================================================
	// Kawaii Physics 탭 함수
	// ============================================================================
	TSharedRef<SWidget> OnGenerateKawaiiMeshWidget(TSharedPtr<FString> InItem);
	void OnKawaiiMeshSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetSelectedKawaiiMeshName() const;
	FReply OnUseSelectedKawaiiMeshClicked();
	FReply OnKawaiiBrowseFolderClicked();
	FReply OnCreateKawaiiAnimBPClicked();
	FReply OnAddKawaiiTagClicked();
	void UpdateKawaiiMeshThumbnail();
	void BuildKawaiiBoneDisplayList();
	void UpdateKawaiiBoneTreeUI();
	void UpdateKawaiiTagListUI();
	TSharedRef<SWidget> CreateKawaiiBoneRow(int32 Index);
	TSharedRef<SWidget> CreateKawaiiTagRow(int32 TagIndex);
	void OnKawaiiBoneTagChanged(int32 BoneIndex, int32 NewTagIndex);
	void OnKawaiiTagNameChanged(int32 TagIndex, const FText& NewName);
	void OnKawaiiTagColorChanged(int32 TagIndex, FLinearColor NewColor);
	void OnKawaiiTagDeleteClicked(int32 TagIndex);
	void OnSelectKawaiiTag(int32 TagIndex);
	FReply OnApplySelectedTagClicked(int32 BoneIndex);
	void SetKawaiiStatus(const FString& Message);
	FString GetSelectedKawaiiMeshPath() const;
	void TransferSecondaryDataToKawaii();  // Control Rig 탭의 Secondary 정보 가져오기
	bool CreateKawaiiAnimBlueprint();      // AnimBP 생성
	void DisplayIKMappingResults();
	void UpdateIKTemplateThumbnail();
	void UpdateIKMeshThumbnail();
	FString GetSelectedIKTemplatePath() const;
	FString GetSelectedIKMeshPath() const;
	void SetIKStatus(const FString& Message);

	// 기본값
	FString DefaultOutputFolder = TEXT("/Game/ControlRigs");
	static constexpr int32 ThumbnailSize = 64;
	
	// 제로 뼈구조 (UE5 표준 본) - 이 외의 본들이 세컨더리
	static const TSet<FString> ZeroBones;
	
	// 액세서리 키워드 (컨트롤러 생성 O)
	static const TArray<FString> AccessoryKeywords;
	
	// 헬퍼 키워드 (컨트롤러 생성 X)
	static const TArray<FString> HelperKeywords;
	
	// IK Rig 관련
	TArray<TSharedPtr<FString>> IKRigTemplateOptions;
	TSharedPtr<FString> SelectedIKRigTemplate;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> IKRigTemplateComboBox;
	
	// IK Rig 탭 전용 멤버
	TSharedPtr<FString> SelectedIKMesh;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> IKMeshComboBox;
	TSharedPtr<FAssetThumbnail> IKTemplateThumbnail;
	TSharedPtr<FAssetThumbnail> IKMeshThumbnail;
	TSharedPtr<SBox> IKTemplateThumbnailBox;
	TSharedPtr<SBox> IKMeshThumbnailBox;
	TSharedPtr<SEditableTextBox> IKOutputNameBox;
	TSharedPtr<SEditableTextBox> IKOutputFolderBox;
	TSharedPtr<STextBlock> IKStatusText;
	TSharedPtr<SVerticalBox> IKMappingResultBox;
	TMap<FName, FName> IKBoneMapping;  // target -> source (IK Rig용)
	FString IKDefaultOutputFolder = TEXT("/Game/IKRigs");
	
	// 탭 관련
	int32 CurrentTabIndex = 0;  // 0: Control Rig, 1: IK Rig, 2: Kawaii Physics, 3: Physics Asset
	TSharedPtr<SWidgetSwitcher> TabContentSwitcher;
	
	// ============================================================================
	// Kawaii Physics 탭 관련
	// ============================================================================
	TArray<FKawaiiTag> KawaiiTags;                        // 태그 목록
	TArray<FKawaiiBoneDisplayInfo> KawaiiBoneDisplayList; // 본 목록
	TSharedPtr<FString> SelectedKawaiiMesh;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> KawaiiMeshComboBox;
	TSharedPtr<FAssetThumbnail> KawaiiMeshThumbnail;
	TSharedPtr<SBox> KawaiiMeshThumbnailBox;
	TSharedPtr<SVerticalBox> KawaiiBoneTreeBox;           // 스켈레톤 트리 박스
	TSharedPtr<SVerticalBox> KawaiiTagListBox;            // 태그 목록 박스
	TSharedPtr<SEditableTextBox> KawaiiOutputNameBox;
	TSharedPtr<SEditableTextBox> KawaiiOutputFolderBox;
	TSharedPtr<STextBlock> KawaiiStatusText;
	FString KawaiiDefaultOutputFolder = TEXT("/Game/AnimBP");
	int32 SelectedKawaiiTagIndex = INDEX_NONE;            // 현재 선택된 태그 (화살표용)
	
	// ============================================================================
	// IK Retargeter 생성 관련
	// ============================================================================
	TArray<TSharedPtr<FString>> RetargeterSourceOptions;  // 소스 IK Rig 옵션
	TArray<TSharedPtr<FString>> RetargeterTargetOptions;  // 타겟 IK Rig 옵션
	TSharedPtr<FString> SelectedRetargeterSource;
	TSharedPtr<FString> SelectedRetargeterTarget;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> RetargeterSourceComboBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> RetargeterTargetComboBox;
	TSharedPtr<FAssetThumbnail> RetargeterSourceThumbnail;
	TSharedPtr<FAssetThumbnail> RetargeterTargetThumbnail;
	TSharedPtr<SBox> RetargeterSourceThumbnailBox;
	TSharedPtr<SBox> RetargeterTargetThumbnailBox;
	TSharedPtr<SEditableTextBox> RetargeterOutputNameBox;
	TSharedPtr<SEditableTextBox> RetargeterOutputFolderBox;
	FString RetargeterDefaultOutputFolder = TEXT("/Game/Retargeters");
	
	// IK Retargeter UI 함수
	TSharedRef<SWidget> OnGenerateRetargeterSourceWidget(TSharedPtr<FString> InItem);
	TSharedRef<SWidget> OnGenerateRetargeterTargetWidget(TSharedPtr<FString> InItem);
	void OnRetargeterSourceSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void OnRetargeterTargetSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetSelectedRetargeterSourceName() const;
	FText GetSelectedRetargeterTargetName() const;
	FReply OnUseSelectedRetargeterSourceClicked();
	FReply OnUseSelectedRetargeterTargetClicked();
	FReply OnCreateIKRetargeterClicked();
	FReply OnRetargeterBrowseFolderClicked();
	void UpdateRetargeterSourceThumbnail();
	void UpdateRetargeterTargetThumbnail();
	void LoadRetargeterIKRigs();
	void CreateIKRetargeter();
	
	// ============================================================================
	// Physics Asset 탭 관련
	// ============================================================================
	TSharedPtr<FString> SelectedPhysAssetMesh;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PhysAssetMeshComboBox;
	TSharedPtr<FAssetThumbnail> PhysAssetMeshThumbnail;
	TSharedPtr<SBox> PhysAssetMeshThumbnailBox;
	TSharedPtr<SVerticalBox> PhysAssetBoneListBox;        // 메인 본 목록 박스
	TSharedPtr<SEditableTextBox> PhysAssetOutputNameBox;
	TSharedPtr<SEditableTextBox> PhysAssetOutputFolderBox;
	TSharedPtr<STextBlock> PhysAssetStatusText;
	FString PhysAssetDefaultOutputFolder = TEXT("/Game/PhysicsAssets");
	TMap<FName, FName> PhysAssetBoneMapping;              // 본 매핑 결과 (StandardBone -> MeshBone)
	TArray<FName> PhysAssetMainBones;                     // 메인 본 목록 (캡슐 생성 대상)
	
	// Physics Asset UI 함수
	TSharedRef<SWidget> OnGeneratePhysAssetMeshWidget(TSharedPtr<FString> InItem);
	void OnPhysAssetMeshSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetSelectedPhysAssetMeshName() const;
	FReply OnUseSelectedPhysAssetMeshClicked();
	FReply OnPhysAssetBoneMappingClicked();
	FReply OnCreatePhysicsAssetClicked();
	FReply OnPhysAssetBrowseFolderClicked();
	void UpdatePhysAssetMeshThumbnail();
	void SetPhysAssetStatus(const FString& Status);
	void UpdatePhysAssetBoneListUI();
	bool CreatePhysicsAsset();
};
