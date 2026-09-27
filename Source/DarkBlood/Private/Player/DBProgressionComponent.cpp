#include "Player/DBProgressionComponent.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBGameplayAbility.h"
#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBGameDataSubsystem.h"
#include "Inventory/DBInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerState.h"

namespace R = DarkBlood::Rules;

UDBProgressionComponent::UDBProgressionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UDBProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDBProgressionComponent, Level); // visible to the whole party
	DOREPLIFETIME(UDBProgressionComponent, GearScore);
	DOREPLIFETIME_CONDITION(UDBProgressionComponent, XpIntoLevel, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDBProgressionComponent, XpToNextLevel, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDBProgressionComponent, UnspentSkillPoints, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDBProgressionComponent, SkillRanks, COND_OwnerOnly);
}

const R::FProgressionRules& UDBProgressionComponent::GetRules()
{
	static const R::FProgressionRules Rules;
	return Rules;
}

ADBPlayerState* UDBProgressionComponent::GetOwningPlayerState() const
{
	return Cast<ADBPlayerState>(GetOwner());
}

const UDBClassDefinition* UDBProgressionComponent::GetClassDefinition() const
{
	const ADBPlayerState* PlayerState = GetOwningPlayerState();
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	return PlayerState && Data ? Data->FindClass(PlayerState->GetProfile().ClassId) : nullptr;
}

void UDBProgressionComponent::AwardXp(int64 Amount)
{
	if (!GetOwner()->HasAuthority() || Amount <= 0)
	{
		return;
	}
	const R::FXpGrantResult Result = R::GrantXp(State, Amount, GetRules());
	if (Result.LevelsGained > 0)
	{
		UE_LOG(LogDarkBlood, Log, TEXT("%s reached level %d (+%d skill points)"), *GetOwner()->GetName(), State.Level,
			Result.SkillPointsGained);
		RecalculateAttributes(true);
	}
	PushReplicatedState();
}

void UDBProgressionComponent::AwardSkillPoints(int32 Amount)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	R::GrantSkillPoints(State, Amount, GetRules());
	PushReplicatedState();
}

void UDBProgressionComponent::SetLevelForDevelopment(int32 NewLevel)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	R::SetLevel(State, NewLevel, GetRules());
	RecalculateAttributes(true);
	PushReplicatedState();
}

void UDBProgressionComponent::RestoreFromRecord(const R::FProgressionState& InState, const R::FSkillTreeState& InSkills)
{
	State = InState;
	Skills = InSkills;

	// Re-grant abilities of unlocked skill nodes.
	const UDBClassDefinition* ClassDefinition = GetClassDefinition();
	ADBPlayerState* PlayerState = GetOwningPlayerState();
	if (ClassDefinition && PlayerState)
	{
		for (const auto& [NodeId, Rank] : Skills.Ranks)
		{
			const FDBSkillNode* Node = ClassDefinition->FindSkillNode(DBBridge::ToFName(NodeId));
			if (Node && Node->GrantedAbility && Rank > 0)
			{
				PlayerState->GrantSkillAbility(Node->GrantedAbility, Rank, Node->InputTag);
			}
		}
	}

	RecalculateAttributes(true);
	PushReplicatedState();
}

void UDBProgressionComponent::RecalculateAttributes(bool bRestoreVitals)
{
	ADBPlayerState* PlayerState = GetOwningPlayerState();
	UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !GetOwner()->HasAuthority())
	{
		return;
	}

	const UDBClassDefinition* ClassDefinition = GetClassDefinition();
	if (!ClassDefinition)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("No class definition for %s - attributes not recalculated"), *PlayerState->GetPlayerName());
		return;
	}

	const R::FPrimaryStats Primary = R::ComputePrimaryStats(ClassDefinition->GetGrowth(), State.Level);
	// Equipment stat bonuses are added here in Phase 5.
	const R::FDerivedStats Derived = R::ComputeDerivedStats(Primary, R::FDerivedStatFormula());

	ASC->SetNumericAttributeBase(UDBAttributeSet::GetMaxHealthAttribute(), Derived.MaxHealth);
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetMaxStaminaAttribute(), Derived.MaxStamina);
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetMaxManaAttribute(), Derived.MaxMana);
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetHealthRegenAttribute(), Derived.HealthRegen);
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetStaminaRegenAttribute(), Derived.StaminaRegen);
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetManaRegenAttribute(), Derived.ManaRegen);
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetAttackPowerAttribute(), Derived.AttackPower);
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetSpellPowerAttribute(), Derived.SpellPower);
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetCritChanceAttribute(), Derived.CritChance);

	if (bRestoreVitals)
	{
		ASC->SetNumericAttributeBase(UDBAttributeSet::GetHealthAttribute(), Derived.MaxHealth);
		ASC->SetNumericAttributeBase(UDBAttributeSet::GetStaminaAttribute(), Derived.MaxStamina);
		ASC->SetNumericAttributeBase(UDBAttributeSet::GetManaAttribute(), Derived.MaxMana);
	}

	if (const UDBInventoryComponent* Inventory = PlayerState->GetInventory())
	{
		GearScore = Inventory->ComputeGearScore();
	}
}

void UDBProgressionComponent::RequestUnlockSkill(FName NodeId)
{
	ServerUnlockSkill(NodeId);
}

void UDBProgressionComponent::ServerUnlockSkill_Implementation(FName NodeId)
{
	const UDBClassDefinition* ClassDefinition = GetClassDefinition();
	ADBPlayerState* PlayerState = GetOwningPlayerState();
	if (!ClassDefinition || !PlayerState)
	{
		return;
	}

	const R::FSkillTreeDefinition Tree = ClassDefinition->BuildSkillTree();
	const std::string Id = DBBridge::ToStd(NodeId);
	const R::ESkillUnlockResult Result = R::UnlockSkill(Tree, Skills, State, Id);
	if (Result != R::ESkillUnlockResult::Ok)
	{
		UE_LOG(LogDarkBlood, Log, TEXT("Skill unlock %s refused: %hs"), *NodeId.ToString(), R::ToString(Result));
		return;
	}
	UE_LOG(LogDarkBlood, Log, TEXT("Skill unlocked: %s (rank %d)"), *NodeId.ToString(), Skills.GetRank(Id));

	const FDBSkillNode* Node = ClassDefinition->FindSkillNode(NodeId);
	if (Node && Node->GrantedAbility)
	{
		PlayerState->GrantSkillAbility(Node->GrantedAbility, Skills.GetRank(Id), Node->InputTag);
	}
	PushReplicatedState();
}

int32 UDBProgressionComponent::GetSkillRank(FName NodeId) const
{
	const FDBSkillRank* Found = SkillRanks.FindByPredicate([NodeId](const FDBSkillRank& Rank) { return Rank.NodeId == NodeId; });
	return Found ? Found->Rank : 0;
}

int32 UDBProgressionComponent::GetPowerRating() const
{
	return R::ComputePowerRating(Level, GearScore);
}

void UDBProgressionComponent::PushReplicatedState()
{
	Level = State.Level;
	XpIntoLevel = State.XpIntoLevel;
	XpToNextLevel = GetRules().Curve.XpToNextLevel(State.Level);
	UnspentSkillPoints = State.UnspentSkillPoints;

	SkillRanks.Reset();
	for (const auto& [NodeId, Rank] : Skills.Ranks)
	{
		FDBSkillRank& Entry = SkillRanks.AddDefaulted_GetRef();
		Entry.NodeId = DBBridge::ToFName(NodeId);
		Entry.Rank = Rank;
	}

	if (const ADBPlayerState* PlayerState = GetOwningPlayerState())
	{
		if (const UDBInventoryComponent* Inventory = PlayerState->GetInventory())
		{
			GearScore = Inventory->ComputeGearScore();
		}
	}
	OnProgressionChanged.Broadcast(this);
}

void UDBProgressionComponent::OnRep_Progression()
{
	OnProgressionChanged.Broadcast(this);
}
