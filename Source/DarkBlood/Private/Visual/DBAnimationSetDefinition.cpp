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
	return FindConventionMontage(Key, Index, bOutFitToDuration);
}

UAnimMontage* UDBAnimationSetDefinition::FindConventionMontage(const FGameplayTag& Key, int32 Index, bool* bOutFitToDuration) const
{
	if (MontageFolder.IsEmpty())
	{
		return nullptr;
	}
	// Montages named after our keys (Tools/UE58/db_setup_paragon.py): AM_Attack_01.., AM_Attack_Heavy, AM_HitReact, ...
	auto Load = [this](const TCHAR* Name) -> UAnimMontage*
	{
		const FString Path = FString::Printf(TEXT("%s/%s.%s"), *MontageFolder, Name, Name);
		return LoadObject<UAnimMontage>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	const FString Tag = Key.ToString();
	UAnimMontage* Found = nullptr;
	bool bFit = true;
	if (Tag.StartsWith(TEXT("Anim.Attack.Heavy")) || Tag.StartsWith(TEXT("Anim.Attack.Charged")))
	{
		Found = Load(TEXT("AM_Attack_Heavy"));
	}
	else if (Tag.StartsWith(TEXT("Anim.Attack")))
	{
		static const TCHAR* Combo[] = {TEXT("AM_Attack_01"), TEXT("AM_Attack_02"), TEXT("AM_Attack_03")};
		const int32 Start = FMath::Max(0, Index) % UE_ARRAY_COUNT(Combo);
		for (int32 Step = 0; Step < static_cast<int32>(UE_ARRAY_COUNT(Combo)) && !Found; ++Step)
		{
			Found = Load(Combo[(Start + Step) % UE_ARRAY_COUNT(Combo)]);
		}
	}
	else if (Tag.StartsWith(TEXT("Anim.Cast")))
	{
		Found = Load(TEXT("AM_Signature"));
	}
	else if (Tag.StartsWith(TEXT("Anim.Knockdown")))
	{
		Found = Load(TEXT("AM_Knockdown"));
	}
	else if (Tag.StartsWith(TEXT("Anim.HitReact")))
	{
		Found = Load(TEXT("AM_HitReact"));
	}
	else if (Tag.StartsWith(TEXT("Anim.Death")))
	{
		Found = Load(TEXT("AM_Death"));
		bFit = false;
	}
	if (Found && bOutFitToDuration)
	{
		*bOutFitToDuration = bFit;
	}
	return Found;
}
