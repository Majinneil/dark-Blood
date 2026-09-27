#include "Debug/DBCheatManager.h"

#include "Abilities/DBAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Character/DBLesserDemon.h"
#include "Character/DBNpcCharacter.h"
#include "Character/DBPlayerCharacter.h"
#include "Character/DBTrainingDummy.h"
#include "Combat/DBCombatStatics.h"
#include "Combat/DBLockOnComponent.h"
#include "Dialogue/DBDialogueComponent.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBItemDefinition.h"
#include "EngineUtils.h"
#include "GameplayTagsManager.h"
#include "TimerManager.h"
#include "Art/DBArtMaterials.h"
#include "Art/DBVisualSliceDirector.h"
#include "DarkBlood.h"
#include "Engine/SkeletalMesh.h"
#include "RHIStats.h"
#include "Visual/DBAnimationSetDefinition.h"
#include "Visual/DBCharacterVisualComponent.h"
#include "Visual/DBCharacterVisualDefinition.h"
#include "GameplayEffect.h"
#include "UObject/Package.h"
#include "Engine/World.h"
#include "Framework/DBDevelopmentSlice.h"
#include "Framework/DBGameMode.h"
#include "Framework/DBGameState.h"
#include "Inventory/DBInventoryComponent.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"
#include "Quest/DBQuestComponent.h"
#include "Quest/DBQuestSubsystem.h"
#include "World/DBEconomyActors.h"
#include "World/DBWorldStateComponent.h"

namespace
{
	template <typename TEnum>
	bool ParseEnum(const FString& Text, TEnum& OutValue)
	{
		const int64 Value = StaticEnum<TEnum>()->GetValueByNameString(Text);
		if (Value == INDEX_NONE)
		{
			return false;
		}
		OutValue = static_cast<TEnum>(Value);
		return true;
	}
}

bool UDBCheatManager::ForwardToServer(const FString& Command) const
{
	ADBPlayerController* Controller = Cast<ADBPlayerController>(GetOuterAPlayerController());
	if (Controller && !Controller->HasAuthority())
	{
		Controller->ServerRunDevCommand(Command);
		return true;
	}
	return false;
}

ADBPlayerState* UDBCheatManager::GetDBPlayerState() const
{
	const APlayerController* Controller = GetOuterAPlayerController();
	return Controller ? Controller->GetPlayerState<ADBPlayerState>() : nullptr;
}

void UDBCheatManager::DBGiveXp(int32 Amount)
{
	if (ForwardToServer(FString::Printf(TEXT("DBGiveXp %d"), Amount))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		PlayerState->GetProgression()->AwardXp(Amount);
	}
}

void UDBCheatManager::DBSetLevel(int32 Level)
{
	if (ForwardToServer(FString::Printf(TEXT("DBSetLevel %d"), Level))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		PlayerState->GetProgression()->SetLevelForDevelopment(Level);
	}
}

void UDBCheatManager::DBGiveSkillPoints(int32 Amount)
{
	if (ForwardToServer(FString::Printf(TEXT("DBGiveSkillPoints %d"), Amount))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		PlayerState->GetProgression()->AwardSkillPoints(Amount);
	}
}

void UDBCheatManager::DBGiveItem(FName ItemId, int32 Count)
{
	if (ForwardToServer(FString::Printf(TEXT("DBGiveItem %s %d"), *ItemId.ToString(), Count))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		const int32 Added = PlayerState->GetInventory()->AddItem(ItemId, Count, true);
		UE_LOG(LogDarkBlood, Display, TEXT("DBGiveItem %s: %d/%d added"), *ItemId.ToString(), Added, Count);
	}
}

void UDBCheatManager::DBGiveCurrency(int32 Amount)
{
	if (ForwardToServer(FString::Printf(TEXT("DBGiveCurrency %d"), Amount))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		PlayerState->GetInventory()->AddCurrency(Amount);
	}
}

void UDBCheatManager::DBEquipBag(int32 Section, int32 Index)
{
	// Uses the regular client request path so the RPC validation is exercised as well.
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		FDBSlotRef Slot;
		Slot.Section = Section;
		Slot.Index = Index;
		PlayerState->GetInventory()->RequestEquipBag(Slot);
	}
}

void UDBCheatManager::DBEquipItem(int32 Section, int32 Index, const FString& Slot)
{
	EDBEquipSlot EquipSlot;
	if (!ParseEnum(Slot, EquipSlot))
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBEquipItem: unknown slot '%s' (MainHand, OffHand, Head, Chest, ...)"), *Slot);
		return;
	}
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		FDBSlotRef From;
		From.Section = Section;
		From.Index = Index;
		PlayerState->GetInventory()->RequestEquipItem(From, EquipSlot);
	}
}

void UDBCheatManager::DBStartQuest(FName QuestId)
{
	if (ForwardToServer(FString::Printf(TEXT("DBStartQuest %s"), *QuestId.ToString()))) return;
	if (UDBQuestSubsystem* Quests = UDBQuestSubsystem::Get(this))
	{
		Quests->StartQuest(QuestId, GetDBPlayerState());
	}
}

void UDBCheatManager::DBQuestEvent(const FString& Kind, FName Target, int32 Amount)
{
	if (ForwardToServer(FString::Printf(TEXT("DBQuestEvent %s %s %d"), *Kind, *Target.ToString(), Amount))) return;
	EDBObjectiveKind ObjectiveKind;
	if (!ParseEnum(Kind, ObjectiveKind))
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBQuestEvent: unknown kind '%s' (Kill, Collect, Reach, Talk, Interact, Custom)"), *Kind);
		return;
	}
	if (UDBQuestSubsystem* Quests = UDBQuestSubsystem::Get(this))
	{
		Quests->ReportEvent(ObjectiveKind, Target, Amount, GetDBPlayerState());
	}
}

void UDBCheatManager::DBSetStoryFlag(FName Flag)
{
	if (ForwardToServer(FString::Printf(TEXT("DBSetStoryFlag %s"), *Flag.ToString()))) return;
	if (const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>())
	{
		GameState->GetWorldState()->SetStoryFlag(Flag);
	}
}

void UDBCheatManager::DBDefeatBoss(FName BossId, const FString& Rank, FName RegionId)
{
	if (ForwardToServer(FString::Printf(TEXT("DBDefeatBoss %s %s %s"), *BossId.ToString(), *Rank, *RegionId.ToString()))) return;
	EDBBossRank BossRank;
	if (!ParseEnum(Rank, BossRank))
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBDefeatBoss: unknown rank '%s' (WorldBoss, MidBoss, Vassal, DemonKing)"), *Rank);
		return;
	}
	if (const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>())
	{
		GameState->GetWorldState()->NotifyBossDefeated(BossId, BossRank, RegionId);
	}
}

void UDBCheatManager::DBSetTime(float Hour)
{
	if (ForwardToServer(FString::Printf(TEXT("DBSetTime %f"), Hour))) return;
	if (const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>())
	{
		GameState->GetWorldState()->SetTimeOfDay(Hour);
	}
}

void UDBCheatManager::DBDamageSelf(float Amount)
{
	if (ForwardToServer(FString::Printf(TEXT("DBDamageSelf %f"), Amount))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		// ApplyModToAttribute only sets the base value and skips PostGameplayEffectExecute, so apply a
		// transient instant effect instead: the damage meta attribute and death handling run as in combat.
		UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("DBDevDamageSelf"));
		DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UDBAttributeSet::GetIncomingDamageAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(FMath::Max(0.f, Amount)));
		DamageEffect->Modifiers.Add(Modifier);

		UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent();
		AbilitySystem->ApplyGameplayEffectToSelf(DamageEffect, 1.f, AbilitySystem->MakeEffectContext());
	}
}

void UDBCheatManager::DBHeal()
{
	if (ForwardToServer(TEXT("DBHeal"))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		PlayerState->GetProgression()->RecalculateAttributes(true);
	}
}

void UDBCheatManager::DBSaveAll()
{
	if (ForwardToServer(TEXT("DBSaveAll"))) return;
	if (ADBGameMode* GameMode = GetWorld()->GetAuthGameMode<ADBGameMode>())
	{
		GameMode->SaveAll();
	}
}

void UDBCheatManager::DBDumpCharacter()
{
	if (ForwardToServer(TEXT("DBDumpCharacter"))) return;
	const ADBPlayerState* PlayerState = GetDBPlayerState();
	if (!PlayerState)
	{
		return;
	}
	const UDBProgressionComponent* Progression = PlayerState->GetProgression();
	const UDBInventoryComponent* Inventory = PlayerState->GetInventory();
	UE_LOG(LogDarkBlood, Display, TEXT("=== %s (%s) ==="), *PlayerState->GetPlayerName(), *PlayerState->GetProfile().ClassId.ToString());
	UE_LOG(LogDarkBlood, Display, TEXT("Level %d, XP %lld/%lld, skill points %d, power %d"), Progression->GetLevel(),
		Progression->GetXpIntoLevel(), Progression->GetXpToNextLevel(), Progression->GetUnspentSkillPoints(), Progression->GetPowerRating());
	UE_LOG(LogDarkBlood, Display, TEXT("Currency %lld Mon, pending deliveries %d, region %s"), Inventory->GetCurrency(),
		Inventory->GetPendingDeliveryCount(), *PlayerState->GetCurrentRegionId().ToString());
	if (const UAbilitySystemComponent* ASC = PlayerState->GetAbilitySystemComponent())
	{
		UE_LOG(LogDarkBlood, Display, TEXT("Stats: HP %.0f  AP %.1f  SP %.1f  Armor %.1f  Crit %.2f  FireRes %.2f  SpiritRes %.2f"),
			ASC->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute()), ASC->GetNumericAttribute(UDBAttributeSet::GetAttackPowerAttribute()),
			ASC->GetNumericAttribute(UDBAttributeSet::GetSpellPowerAttribute()), ASC->GetNumericAttribute(UDBAttributeSet::GetArmorAttribute()),
			ASC->GetNumericAttribute(UDBAttributeSet::GetCritChanceAttribute()), ASC->GetNumericAttribute(UDBAttributeSet::GetResistFireAttribute()),
			ASC->GetNumericAttribute(UDBAttributeSet::GetResistSpiritAttribute()));
	}
	for (const FDBInventoryEntry& Entry : Inventory->GetEntries())
	{
		UE_LOG(LogDarkBlood, Display, TEXT("  [%d:%d] %s x%d (dur %d)"), Entry.Section, Entry.SlotIndex, *Entry.Stack.ItemId.ToString(),
			Entry.Stack.Count, Entry.Stack.Durability);
	}
	for (uint8 SlotIndex = 0; SlotIndex <= static_cast<uint8>(EDBEquipSlot::Accessory2); ++SlotIndex)
	{
		const EDBEquipSlot Slot = static_cast<EDBEquipSlot>(SlotIndex);
		const FDBItemStackView Equipped = Inventory->GetEquipped(Slot);
		if (!Equipped.ItemId.IsNone())
		{
			UE_LOG(LogDarkBlood, Display, TEXT("  equipped %s: %s (dur %d)"), *StaticEnum<EDBEquipSlot>()->GetNameStringByValue(SlotIndex),
				*Equipped.ItemId.ToString(), Equipped.Durability);
		}
	}
}

void UDBCheatManager::DBDumpWorld()
{
	if (ForwardToServer(TEXT("DBDumpWorld"))) return;
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	if (!GameState)
	{
		return;
	}
	const UDBWorldStateComponent* World = GameState->GetWorldState();
	UE_LOG(LogDBWorld, Display, TEXT("=== World: day %d, %.2f h, night=%d, vassals %d/14, final region open=%d ==="), World->GetDay(),
		World->GetTimeOfDay(), World->IsNight() ? 1 : 0, World->GetDefeatedVassalCount(), World->IsFinalRegionOpen() ? 1 : 0);
	for (const DarkBlood::Rules::FRegionState& Region : World->GetRulesState().GetRegions())
	{
		UE_LOG(LogDBWorld, Display, TEXT("  %hs control=%d influence=%.2f vassal=%d"), Region.RegionId.c_str(), static_cast<int32>(Region.Control),
			Region.DemonInfluence, Region.bVassalDefeated ? 1 : 0);
	}
	for (const FDBQuestProgressView& Quest : GameState->GetSharedQuests()->GetQuests())
	{
		UE_LOG(LogDBQuest, Display, TEXT("  shared quest %s status=%d"), *Quest.QuestId.ToString(), static_cast<int32>(Quest.Status));
	}
}

// ---- Combat -----------------------------------------------------------------------------------

void UDBCheatManager::DBSpawnDummy(float Distance)
{
	if (ForwardToServer(FString::Printf(TEXT("DBSpawnDummy %f"), Distance))) return;
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (!Pawn)
	{
		return;
	}
	const float UsedDistance = Distance > 0.f ? Distance : 200.f;
	const FVector Location = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * UsedDistance;
	const FRotator Facing(0.f, (Pawn->GetActorLocation() - Location).Rotation().Yaw, 0.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const ADBTrainingDummy* Dummy = GetWorld()->SpawnActor<ADBTrainingDummy>(ADBTrainingDummy::StaticClass(), Location, Facing, Params);
	UE_LOG(LogDBCombat, Display, TEXT("DBSpawnDummy: %s at %.0f cm"), Dummy ? *Dummy->GetName() : TEXT("failed"), UsedDistance);
}

void UDBCheatManager::DBSpawnEnemy(float Distance)
{
	if (ForwardToServer(FString::Printf(TEXT("DBSpawnEnemy %f"), Distance))) return;
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (!Pawn)
	{
		return;
	}
	const float UsedDistance = Distance > 0.f ? Distance : 800.f;
	const FVector Location = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * UsedDistance;
	const FRotator Facing(0.f, (Pawn->GetActorLocation() - Location).Rotation().Yaw, 0.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const ADBLesserDemon* Demon = GetWorld()->SpawnActor<ADBLesserDemon>(ADBLesserDemon::StaticClass(), Location, Facing, Params);
	UE_LOG(LogDBCombat, Display, TEXT("DBSpawnEnemy: %s at %.0f cm"), Demon ? *Demon->GetName() : TEXT("failed"), UsedDistance);
}

void UDBCheatManager::DBDummyAttack()
{
	if (ForwardToServer(TEXT("DBDummyAttack"))) return;
	for (TActorIterator<ADBTrainingDummy> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogDBCombat, Display, TEXT("DBDummyAttack: %s %s"), *It->GetName(), It->SwingAtNearestPlayer() ? TEXT("swings") : TEXT("has no target"));
	}
}

void UDBCheatManager::DBDummyAutoAttack(float Interval)
{
	if (ForwardToServer(FString::Printf(TEXT("DBDummyAutoAttack %f"), Interval))) return;
	for (TActorIterator<ADBTrainingDummy> It(GetWorld()); It; ++It)
	{
		It->SetAutoAttack(Interval);
	}
}

void UDBCheatManager::DBInput(const FString& Input, float HoldSeconds)
{
	ADBPlayerCharacter* Character = Cast<ADBPlayerCharacter>(GetOuterAPlayerController()->GetPawn());
	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*(TEXT("Input.") + Input)), false);
	if (!Character || !Tag.IsValid())
	{
		UE_LOG(LogDBCombat, Warning, TEXT("DBInput: unknown input '%s' or no character"), *Input);
		return;
	}
	Character->PressAbilityInput(Tag);
	const TWeakObjectPtr<ADBPlayerCharacter> WeakCharacter = Character;
	FTimerHandle Release;
	GetWorld()->GetTimerManager().SetTimer(Release, FTimerDelegate::CreateWeakLambda(this, [WeakCharacter, Tag]()
	{
		if (WeakCharacter.IsValid())
		{
			WeakCharacter->ReleaseAbilityInput(Tag);
		}
	}), FMath::Max(0.01f, HoldSeconds), false);
}

void UDBCheatManager::DBJump()
{
	ACharacter* Character = Cast<ACharacter>(GetOuterAPlayerController()->GetPawn());
	if (!Character)
	{
		return;
	}
	Character->Jump();
	const TWeakObjectPtr<ACharacter> WeakCharacter = Character;
	FTimerHandle Release;
	GetWorld()->GetTimerManager().SetTimer(Release, FTimerDelegate::CreateWeakLambda(this, [WeakCharacter]()
	{
		if (WeakCharacter.IsValid())
		{
			WeakCharacter->StopJumping();
			UE_LOG(LogDBCombat, Display, TEXT("DBJump: jump %d/%d, height %.0f"), WeakCharacter->JumpCurrentCount, WeakCharacter->JumpMaxCount,
				WeakCharacter->GetActorLocation().Z);
		}
	}), 0.1f, false);
}

void UDBCheatManager::DBLockOn()
{
	if (const ADBPlayerCharacter* Character = Cast<ADBPlayerCharacter>(GetOuterAPlayerController()->GetPawn()))
	{
		Character->GetLockOn()->ToggleLockOn();
	}
}

void UDBCheatManager::DBAfter(float Seconds, const FString& Command)
{
	APlayerController* Controller = GetOuterAPlayerController();
	const TWeakObjectPtr<APlayerController> WeakController = Controller;
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakController, Command]()
	{
		if (WeakController.IsValid())
		{
			WeakController->ConsoleCommand(Command, true);
		}
	}), FMath::Max(0.01f, Seconds), false);
}

void UDBCheatManager::DBDumpCombat()
{
	if (ForwardToServer(TEXT("DBDumpCombat"))) return;
	for (TActorIterator<ADBCharacterBase> It(GetWorld()); It; ++It)
	{
		const UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent();
		if (!ASC)
		{
			continue;
		}
		FGameplayTagContainer Tags;
		ASC->GetOwnedGameplayTags(Tags);
		UE_LOG(LogDBCombat, Display, TEXT("%s%s: HP %.1f/%.0f  ST %.1f/%.0f  Poise %.1f/%.0f  Tags [%s]"), *DBCombat::GetCombatName(*It),
			It->IsDead() ? TEXT(" (dead)") : TEXT(""), ASC->GetNumericAttribute(UDBAttributeSet::GetHealthAttribute()),
			ASC->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute()), ASC->GetNumericAttribute(UDBAttributeSet::GetStaminaAttribute()),
			ASC->GetNumericAttribute(UDBAttributeSet::GetMaxStaminaAttribute()), ASC->GetNumericAttribute(UDBAttributeSet::GetPoiseAttribute()),
			ASC->GetNumericAttribute(UDBAttributeSet::GetMaxPoiseAttribute()), *Tags.ToStringSimple());
	}
}

// ---- Story ------------------------------------------------------------------------------------

void UDBCheatManager::DBSetupSlice()
{
	if (ForwardToServer(TEXT("DBSetupSlice"))) return;
	if (const APawn* Pawn = GetOuterAPlayerController()->GetPawn())
	{
		DBDevelopmentSlice::Spawn(GetWorld(), FTransform(FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), Pawn->GetActorLocation()));
	}
}

void UDBCheatManager::DBGoto(const FString& Target)
{
	if (ForwardToServer(FString::Printf(TEXT("DBGoto %s"), *Target))) return;
	APlayerController* Controller = GetOuterAPlayerController();
	APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		return;
	}
	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		FString Id;
		if (const ADBNpcCharacter* Npc = Cast<ADBNpcCharacter>(*It))
		{
			Id = Npc->GetNpcId().ToString();
		}
		else if (const ADBEnemyCharacter* Enemy = Cast<ADBEnemyCharacter>(*It); Enemy && !Enemy->IsDead())
		{
			Id = Enemy->GetEnemyId().ToString();
		}
		else if (const ADBCraftingStation* Station = Cast<ADBCraftingStation>(*It))
		{
			Id = Station->GetStationId().ToString();
		}
		else if (Cast<ADBLootChest>(*It))
		{
			Id = TEXT("Chest");
		}
		const float Distance = FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation());
		if (!Id.IsEmpty() && Id.Contains(Target) && Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	if (!Best)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBGoto: nothing matches '%s'"), *Target);
		return;
	}
	const FVector Forward = Best->GetActorForwardVector();
	const FVector Destination = Best->GetActorLocation() + Forward * 170.f;
	const FRotator Facing(0.f, (-Forward).Rotation().Yaw, 0.f);
	Pawn->TeleportTo(Destination, Facing);
	Controller->ClientSetRotation(Facing);
	UE_LOG(LogDarkBlood, Display, TEXT("DBGoto: %s"), *DBCombat::GetCombatName(Best));
}

void UDBCheatManager::DBDialogueChoose(int32 Index)
{
	if (const ADBPlayerController* Controller = Cast<ADBPlayerController>(GetOuterAPlayerController()))
	{
		Controller->GetDialogue()->Choose(Index);
	}
}

void UDBCheatManager::DBCreateCharacter(FName ClassId, const FString& Name)
{
	if (ADBPlayerController* Controller = Cast<ADBPlayerController>(GetOuterAPlayerController()))
	{
		FString Error;
		const bool bOk = Controller->SubmitCharacterCreation(Name, ClassId, FDBAppearance(), Error);
		UE_LOG(LogDarkBlood, Display, TEXT("DBCreateCharacter: %s"), bOk ? TEXT("ok") : *Error);
	}
}

void UDBCheatManager::DBUnlockSkill(FName NodeId)
{
	if (ForwardToServer(FString::Printf(TEXT("DBUnlockSkill %s"), *NodeId.ToString()))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		PlayerState->GetProgression()->RequestUnlockSkill(NodeId);
	}
}

// ---- Economy ----------------------------------------------------------------------------------

namespace
{
	AActor* FindNearestStation(const APawn* Pawn)
	{
		AActor* Best = nullptr;
		float BestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<ADBCraftingStation> It(Pawn->GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = *It;
			}
		}
		return Best;
	}
}

void UDBCheatManager::DBUseItem(int32 Section, int32 Index)
{
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		FDBSlotRef Slot;
		Slot.Section = Section;
		Slot.Index = Index;
		PlayerState->GetInventory()->RequestUseItem(Slot);
	}
}

void UDBCheatManager::DBCraft(FName RecipeId)
{
	ADBPlayerState* PlayerState = GetDBPlayerState();
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (PlayerState && Pawn)
	{
		PlayerState->GetInventory()->RequestCraft(RecipeId, FindNearestStation(Pawn));
	}
}

void UDBCheatManager::DBRepair()
{
	ADBPlayerState* PlayerState = GetDBPlayerState();
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (PlayerState && Pawn)
	{
		PlayerState->GetInventory()->RequestRepairAll(FindNearestStation(Pawn));
	}
}

void UDBCheatManager::DBGrantLoot(FName LootTableId)
{
	if (ForwardToServer(FString::Printf(TEXT("DBGrantLoot %s"), *LootTableId.ToString()))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState())
	{
		PlayerState->GetInventory()->GrantLootTable(LootTableId, TEXT("DEV"));
	}
}

namespace
{
	bool FindCarried(const UDBInventoryComponent* Inventory, FName ItemId, FDBSlotRef& OutSlot)
	{
		for (const FDBInventoryEntry& Entry : Inventory->GetEntries())
		{
			if (Entry.Section >= 0 && Entry.Stack.ItemId == ItemId)
			{
				OutSlot.Section = Entry.Section;
				OutSlot.Index = Entry.SlotIndex;
				return true;
			}
		}
		return false;
	}
}

void UDBCheatManager::DBEquipById(FName ItemId)
{
	ADBPlayerState* PlayerState = GetDBPlayerState();
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	const UDBItemDefinition* Definition = Data ? Data->FindItem(ItemId) : nullptr;
	FDBSlotRef Slot;
	if (PlayerState && Definition && FindCarried(PlayerState->GetInventory(), ItemId, Slot))
	{
		PlayerState->GetInventory()->RequestEquipItem(Slot, Definition->EquipSlot);
	}
	else
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBEquipById: %s not carried"), *ItemId.ToString());
	}
}

void UDBCheatManager::DBUse(FName ItemId)
{
	ADBPlayerState* PlayerState = GetDBPlayerState();
	FDBSlotRef Slot;
	if (PlayerState && FindCarried(PlayerState->GetInventory(), ItemId, Slot))
	{
		PlayerState->GetInventory()->RequestUseItem(Slot);
	}
}

void UDBCheatManager::DBVisualSlice(int32 bEnabled)
{
	if (ForwardToServer(FString::Printf(TEXT("DBVisualSlice %d"), bEnabled))) return;
	ADBVisualSliceDirector* Director = ADBVisualSliceDirector::Get(GetWorld());
	if (!Director && bEnabled)
	{
		const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
		Director = ADBVisualSliceDirector::SpawnFor(GetWorld(), Pawn ? FTransform(FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), Pawn->GetActorLocation())
																	: FTransform::Identity);
	}
	if (Director)
	{
		Director->SetSliceEnabled(bEnabled != 0);
	}
}

void UDBCheatManager::DBTimeOfDay(const FString& Preset)
{
	if (ForwardToServer(FString::Printf(TEXT("DBTimeOfDay %s"), *Preset))) return;
	const int64 Value = StaticEnum<EDBTimeOfDay>()->GetValueByNameString(Preset);
	ADBVisualSliceDirector* Director = ADBVisualSliceDirector::Get(GetWorld());
	if (Value == INDEX_NONE || !Director)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBTimeOfDay: use Day | Dusk | Night | DemonNight (visual slice must exist)"));
		return;
	}
	Director->SetTimeOfDay(static_cast<EDBTimeOfDay>(Value));
}

void UDBCheatManager::DBVisuals(int32 bEnabled)
{
	UDBCharacterVisualComponent::SetVisualsEnabled(GetWorld(), bEnabled != 0);
	UE_LOG(LogDarkBlood, Display, TEXT("DBVIS character visuals %s"), bEnabled ? TEXT("on") : TEXT("off (greybox)"));
}

void UDBCheatManager::DBPerfSnapshot()
{
	extern ENGINE_API float GAverageFPS;
	extern ENGINE_API float GAverageMS;
	const double GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	int32 DrawCalls = 0;
	int32 Primitives = 0;
	for (int32 Gpu = 0; Gpu < MAX_NUM_GPUS; ++Gpu)
	{
		DrawCalls += GNumDrawCallsRHI[Gpu];
		Primitives += GNumPrimitivesDrawnRHI[Gpu];
	}
	int32 Characters = 0;
	int32 WithBody = 0;
	for (TActorIterator<ADBCharacterBase> It(GetWorld()); It; ++It)
	{
		++Characters;
		WithBody += It->GetVisuals() && It->GetVisuals()->HasVisualBody() ? 1 : 0;
	}
	const ADBVisualSliceDirector* Director = ADBVisualSliceDirector::Get(GetWorld());
	UE_LOG(LogDarkBlood, Display, TEXT("DBVIS perf: %.1f fps, frame %.2f ms, GPU %.2f ms, %d draw calls, %d primitives; characters %d (%d with body); %s"), GAverageFPS,
		GAverageMS, GpuMs, DrawCalls, Primitives, Characters, WithBody, Director ? *Director->DescribeLocalSlice() : TEXT("no visual slice"));
}

void UDBCheatManager::DBVisualAudit()
{
	int32 Missing = 0;
	for (int32 Index = 0; Index < static_cast<int32>(EDBArtMaterial::Count); ++Index)
	{
		const EDBArtMaterial Slot = static_cast<EDBArtMaterial>(Index);
		const FString Path = UDBArtMaterialSubsystem::GetAssetPath(Slot);
		if (!Path.IsEmpty() && !UDBArtMaterialSubsystem::IsAuthored(Slot))
		{
			++Missing;
			UE_LOG(LogDarkBlood, Warning, TEXT("DBVIS audit: material slot %s missing (%s) - flat fallback"),
				*StaticEnum<EDBArtMaterial>()->GetNameStringByValue(Index), *Path);
		}
	}
	if (const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(GetWorld()))
	{
		for (const UDBCharacterVisualDefinition* Profile : Data->GetAllCharacterVisuals())
		{
			const bool bBody = Profile->BodyMesh.LoadSynchronous() != nullptr;
			const bool bAnim = Profile->AnimationSet && !Profile->AnimationSet->AnimClass.IsNull() && Profile->AnimationSet->AnimClass.LoadSynchronous();
			Missing += bBody ? 0 : 1;
			UE_LOG(LogDarkBlood, Display, TEXT("DBVIS audit: profile %s tier %d body %s anim %s%s"), *Profile->ProfileId.ToString(),
				static_cast<int32>(Profile->QualityTier), bBody ? TEXT("ok") : TEXT("MISSING"), bAnim ? TEXT("ok") : TEXT("missing"),
				Profile->bDevelopmentPlaceholder ? TEXT(" [DEV placeholder]") : TEXT(""));
			if (Profile->AnimationSet)
			{
				for (const FDBAnimationEntry& Entry : Profile->AnimationSet->Entries)
				{
					for (const TSoftObjectPtr<UAnimMontage>& Montage : Entry.Montages)
					{
						if (!Montage.LoadSynchronous())
						{
							++Missing;
							UE_LOG(LogDarkBlood, Warning, TEXT("DBVIS audit: %s montage %s missing"), *Entry.Key.ToString(), *Montage.ToString());
						}
					}
				}
			}
		}
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBVIS audit done: %d missing references"), Missing);
}

void UDBCheatManager::DBView(float X, float Y, float Yaw, float Pitch)
{
	if (ForwardToServer(FString::Printf(TEXT("DBView %f %f %f %f"), X, Y, Yaw, Pitch))) return;
	APlayerController* Controller = GetOuterAPlayerController();
	APawn* Pawn = Controller->GetPawn();
	const ADBVisualSliceDirector* Director = ADBVisualSliceDirector::Get(GetWorld());
	if (!Pawn)
	{
		return;
	}
	const FTransform Origin = Director ? Director->GetActorTransform() : FTransform::Identity;
	const FVector Target = Origin.TransformPosition(FVector(X, Y, 120.f));
	Pawn->TeleportTo(Target, FRotator(0.f, Origin.Rotator().Yaw + Yaw, 0.f));
	Controller->ClientSetRotation(FRotator(Pitch, Origin.Rotator().Yaw + Yaw, 0.f));
}
