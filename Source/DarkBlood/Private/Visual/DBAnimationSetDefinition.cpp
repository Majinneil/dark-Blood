#include "Visual/DBAnimationSetDefinition.h"

#include "Animation/AnimMontage.h"

const FPrimaryAssetType UDBAnimationSetDefinition::AssetType(TEXT("DBAnimationSet"));

FPrimaryAssetId UDBAnimationSetDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, AnimationSetId.IsNone() ? GetFName() : AnimationSetId);
}

UAnimMontage* UDBAnimationSetDefinition::FindMontage(const FGameplayTag& Key, int32 Index, bool* bOutFitToDuration) const
{
	for (FGameplayTag Current = Key; Current.IsValid(); Current = Current.RequestDirectParent())
	{
		for (const FDBAnimationEntry& Entry : Entries)
		{
			if (Entry.Key != Current || Entry.Montages.Num() == 0)
			{
				continue;
			}
			const int32 Variant = Index > 0 ? Index % Entry.Montages.Num() : 0;
			if (UAnimMontage* Montage = Entry.Montages[Variant].LoadSynchronous())
			{
				if (bOutFitToDuration)
				{
					*bOutFitToDuration = Entry.bFitToActionDuration;
				}
				return Montage;
			}
		}
	}
	return nullptr;
}
