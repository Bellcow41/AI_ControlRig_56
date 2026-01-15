// AnimPickerLayoutAsset.cpp

#include "AnimPickerLayoutAsset.h"

UAnimPickerLayoutAsset::UAnimPickerLayoutAsset()
{
	ZoomScale2D = 1.0f;
	PanOffset2D = FVector2D::ZeroVector;
}

bool UAnimPickerLayoutAsset::IsCompatibleWith(const FSoftObjectPath& ControlRigPath) const
{
	// 타겟이 지정되지 않았으면 모든 Control Rig과 호환
	if (TargetControlRig.IsNull())
	{
		return true;
	}

	// 경로가 일치하면 호환
	return TargetControlRig == ControlRigPath;
}

bool UAnimPickerLayoutAsset::IsEmpty() const
{
	return CustomPickers.Num() == 0 && ControllerPositions2D.Num() == 0;
}

#if WITH_EDITOR
void UAnimPickerLayoutAsset::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	// Control Rig 이름을 태그로 추가 (검색용)
	if (!TargetControlRig.IsNull())
	{
		FString ControlRigName = FPaths::GetBaseFilename(TargetControlRig.GetAssetPathString());
		OutTags.Add(FAssetRegistryTag("TargetControlRig", ControlRigName, FAssetRegistryTag::TT_Alphabetical));
	}

	// 커스텀 피커 개수
	OutTags.Add(FAssetRegistryTag("CustomPickerCount", FString::FromInt(CustomPickers.Num()), FAssetRegistryTag::TT_Numerical));
}
#endif

