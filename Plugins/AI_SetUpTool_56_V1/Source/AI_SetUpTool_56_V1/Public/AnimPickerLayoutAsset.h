// AnimPickerLayoutAsset.h
// 커스텀 피커 레이아웃을 저장하는 Data Asset

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AnimPickerLayoutAsset.generated.h"

// ============================================================================
// 커스텀 피커 그룹 데이터
// ============================================================================
USTRUCT(BlueprintType)
struct FCustomPickerData
{
	GENERATED_BODY()

	// 피커 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Picker")
	FString PickerName;

	// 포함된 컨트롤러 이름들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Picker")
	TArray<FName> ControllerNames;

	// 피커 색상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Picker")
	FLinearColor Color = FLinearColor(0.3f, 0.6f, 1.0f, 1.0f);

	// 2D 뷰에서의 위치 (정규화 좌표 0~1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Picker")
	FVector2D Position2D = FVector2D(0.5f, 0.85f);

	// 2D 뷰에서의 크기 (픽셀)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Picker")
	FVector2D Size2D = FVector2D(80.0f, 30.0f);

	FCustomPickerData()
		: PickerName(TEXT("NewPicker"))
		, Color(FLinearColor(0.3f, 0.6f, 1.0f, 1.0f))
		, Position2D(FVector2D(0.5f, 0.85f))
		, Size2D(FVector2D(80.0f, 30.0f))
	{}
};

// ============================================================================
// 피커 레이아웃 에셋 - 여러 사람이 각자 커스텀 가능
// ============================================================================
UCLASS(BlueprintType)
class AI_SETUPTOOL_56_V1_API UAnimPickerLayoutAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 이 레이아웃이 어떤 Control Rig용인지 (소프트 레퍼런스)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
	FSoftObjectPath TargetControlRig;

	// 레이아웃 설명 (예: "김대리 손가락 전용", "얼굴 애니메이션용" 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Info")
	FString Description;

	// 커스텀 피커들 (Selection Sets / 그룹 버튼)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomPickers")
	TArray<FCustomPickerData> CustomPickers;

	// 2D 뷰에서 기본 컨트롤러들의 위치 (드래그로 조정한 경우)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	TMap<FName, FVector2D> ControllerPositions2D;

	// 2D 뷰 줌 스케일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float ZoomScale2D = 1.0f;

	// 2D 뷰 패닝 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector2D PanOffset2D = FVector2D::ZeroVector;

public:
	UAnimPickerLayoutAsset();

	// 특정 Control Rig과 호환되는지 확인
	bool IsCompatibleWith(const FSoftObjectPath& ControlRigPath) const;

	// 현재 레이아웃이 비어있는지 확인
	bool IsEmpty() const;

#if WITH_EDITOR
	// 에디터에서 에셋 이름 표시용
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif
};

