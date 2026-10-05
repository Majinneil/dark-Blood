#include "Debug/DBCheatManager.h"

#include "Abilities/DBAttributeSet.h"
#include "Art/DBArtBuilder.h"
#include "Art/DBModelLibrary.h"
#include "Boss/DBBoss.h"
#include "Boss/DBBossDefinition.h"
#include "World/DBRegionLife.h"
#include "DarkBloodRules/Region.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemComponent.h"
#include "Character/DBHorse.h"
#include "Character/DBLesserDemon.h"
#include "Character/DBNpcCharacter.h"
#include "Character/DBPlayerCharacter.h"
#include "Character/DBTrainingDummy.h"
#include "Character/DBVillagerCharacter.h"
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
#include "RenderTimer.h"
#include "PrimitiveSceneProxy.h"
#include "UObject/UObjectIterator.h"
#include "Settings/DBGameUserSettings.h"
#include "World/DBCarriageStation.h"
#include "World/DBDungeon.h"
#include "World/DBRealmDirector.h"
#include "World/DBRealmLayout.h"
#include "World/DBSettlementLifeComponent.h"
#include "World/DBShip.h"
#include "Visual/DBAnimationSetDefinition.h"
#include "Visual/DBCharacterVisualComponent.h"
#include "Visual/DBCharacterVisualDefinition.h"
#include "GameplayEffect.h"
#include "UObject/Package.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Framework/DBDevelopmentSlice.h"
#include "Framework/DBGameMode.h"
#include "Framework/DBGameState.h"
#include "Inventory/DBInventoryComponent.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Player/DBSurvivalComponent.h"
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
	if (const UDBSurvivalComponent* Survival = PlayerState->GetSurvival())
	{
		const UAbilitySystemComponent* ASC = PlayerState->GetAbilitySystemComponent();
		UE_LOG(LogDarkBlood, Display, TEXT("Survival: satiety %.0f warmth %.0f status 0x%02x cold %.2f, stamina regen %.2f/s, health regen %.2f/s"),
			Survival->GetSatiety(), Survival->GetWarmth(), Survival->GetStatus(), Survival->GetColdExposure(),
			ASC ? ASC->GetNumericAttribute(UDBAttributeSet::GetStaminaRegenAttribute()) : 0.f,
			ASC ? ASC->GetNumericAttribute(UDBAttributeSet::GetHealthRegenAttribute()) : 0.f);
	}
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
	UE_LOG(LogDBWorld, Display, TEXT("=== World: day %d, %.2f h, night=%d, vassals %d/%d, final region open=%d, demon king reachable=%d ==="), World->GetDay(),
		World->GetTimeOfDay(), World->IsNight() ? 1 : 0, World->GetDefeatedVassalCount(), DarkBlood::Rules::NumVassals, World->IsFinalRegionOpen() ? 1 : 0,
		World->GetRulesState().IsDemonKingReachable() ? 1 : 0);
	for (const DarkBlood::Rules::FRegionState& Region : World->GetRulesState().GetRegions())
	{
		UE_LOG(LogDBWorld, Display, TEXT("  %hs control=%d influence=%.2f vassal=%d"), Region.RegionId.c_str(), static_cast<int32>(Region.Control),
			Region.DemonInfluence, Region.bVassalDefeated ? 1 : 0);
	}
	for (const FDBQuestProgressView& Quest : GameState->GetSharedQuests()->GetQuests())
	{
		FString Counts;
		for (const int32 Count : Quest.ObjectiveCounts)
		{
			Counts += FString::Printf(TEXT(" %d"), Count);
		}
		UE_LOG(LogDBQuest, Display, TEXT("  shared quest %s status=%d objectives%s"), *Quest.QuestId.ToString(), static_cast<int32>(Quest.Status), *Counts);
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
	int32 NaniteProxies = 0;
	int32 ClassicProxies = 0;
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->SceneProxy && It->IsVisible())
		{
			It->SceneProxy->IsNaniteMesh() ? ++NaniteProxies : ++ClassicProxies;
			// Which classic primitives use a mesh that has Nanite data (they should have been Nanite)?
			const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(*It);
			static int32 Reported = 0;
			if (!It->SceneProxy->IsNaniteMesh() && MeshComponent && MeshComponent->GetStaticMesh() && Reported < 12)
			{
				++Reported;
				const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
				UE_LOG(LogDarkBlood, Display, TEXT("DBVIS classic: %s (%s) mesh %s nanite-data %d, material %s"), *It->GetClass()->GetName(),
					*GetNameSafe(It->GetOwner()), *Mesh->GetName(), Mesh->HasValidNaniteData() ? 1 : 0, *GetNameSafe(MeshComponent->GetMaterial(0)));
			}
		}
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBVIS threads: game %.2f ms, render %.2f ms, RHI %.2f ms; primitives: %d Nanite, %d classic"),
		FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(GRHIThreadTime),
		NaniteProxies, ClassicProxies);
	const IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	const IConsoleVariable* LumenHardware = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"));
	UE_LOG(LogDarkBlood, Display, TEXT("DBVIS graphics: r.ScreenPercentage %.1f, Lumen HWRT %d (available %d); %s"), ScreenPercentage ? ScreenPercentage->GetFloat() : 0.f,
		LumenHardware ? LumenHardware->GetInt() : -1, UDBGameUserSettings::IsHardwareRayTracingAvailable() ? 1 : 0,
		UDBGameUserSettings::Get() ? *UDBGameUserSettings::Get()->Describe() : TEXT("-"));
}

void UDBCheatManager::DBGraphics(int32 Quality, int32 Upscaling, int32 bRayTracing)
{
	if (UDBGameUserSettings* Settings = UDBGameUserSettings::Get())
	{
		Settings->SetOverallScalabilityLevel(FMath::Clamp(Quality, 0, 4));
		Settings->Upscaling = static_cast<EDBUpscaling>(FMath::Clamp(Upscaling, 0, 4));
		Settings->bHardwareRayTracing = bRayTracing != 0;
		Settings->ApplyNonResolutionSettings();
		UE_LOG(LogDarkBlood, Display, TEXT("DBGraphics: %s"), *Settings->Describe());
	}
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
	for (int32 Index = 0; Index < static_cast<int32>(EDBArtMeshSet::Count); ++Index)
	{
		const EDBArtMeshSet Set = static_cast<EDBArtMeshSet>(Index);
		const int32 Loaded = UDBArtMaterialSubsystem::GetMeshes(Set).Num();
		const int32 Expected = UDBArtMaterialSubsystem::GetMeshPaths(Set).Num();
		Missing += Expected - Loaded;
		UE_LOG(LogDarkBlood, Display, TEXT("DBVIS audit: mesh set %s %d/%d%s"), *StaticEnum<EDBArtMeshSet>()->GetNameStringByValue(Index), Loaded, Expected,
			Loaded == 0 ? TEXT(" (primitive fallback)") : TEXT(""));
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

void UDBCheatManager::DBOrbit(float Yaw, float Pitch, float Distance)
{
	APlayerController* Controller = GetOuterAPlayerController();
	ADBPlayerCharacter* Character = Controller ? Cast<ADBPlayerCharacter>(Controller->GetPawn()) : nullptr;
	if (!Character)
	{
		return;
	}
	Character->GetCameraBoom()->TargetArmLength = Distance;
	Character->GetCameraBoom()->bDoCollisionTest = false;
	Controller->SetControlRotation(FRotator(Pitch, Character->GetActorRotation().Yaw + Yaw, 0.f));
}

void UDBCheatManager::DBTravel(const FString& Region, float OffsetX, float OffsetY)
{
	if (ForwardToServer(FString::Printf(TEXT("DBTravel %s %f %f"), *Region, OffsetX, OffsetY))) return;
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	// Settlements first (exact name prefix), then regions.
	for (const FDBRealmSettlement& Site : DBRealm::GetSettlements())
	{
		if (Pawn && FString(Site.Name).StartsWith(Region, ESearchCase::IgnoreCase))
		{
			const double X = Site.Center.X + OffsetX;
			const double Y = Site.Center.Y + OffsetY;
			Pawn->TeleportTo(FVector(X * 100.0, Y * 100.0, FMath::Max(DBRealm::SampleHeight(X, Y), 0.0) * 100.0 + 250.0), Pawn->GetActorRotation());
			UE_LOG(LogDarkBlood, Display, TEXT("DBTravel: settlement %s at %.0f / %.0f m"), Site.Name, X, Y);
			return;
		}
	}
	const TArray<FDBRealmRegion>& Regions = DBRealm::GetRegions();
	const FDBRealmRegion* Target = nullptr;
	for (int32 Index = 0; Index < Regions.Num(); ++Index)
	{
		if (Region == FString::FromInt(Index) || Region.Equals(Regions[Index].RegionId.ToString(), ESearchCase::IgnoreCase)
			|| FString(Regions[Index].DisplayName).StartsWith(Region, ESearchCase::IgnoreCase))
		{
			Target = &Regions[Index];
			break;
		}
	}
	if (!Pawn || !Target)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBTravel: unknown region '%s'"), *Region);
		return;
	}
	const double X = Target->Center.X + OffsetX;
	const double Y = Target->Center.Y + OffsetY;
	const double Ground = FMath::Max(DBRealm::SampleHeight(X, Y), 0.0);
	Pawn->TeleportTo(FVector(X * 100.0, Y * 100.0, Ground * 100.0 + 250.0), Pawn->GetActorRotation());
	UE_LOG(LogDarkBlood, Display, TEXT("DBTravel: %s (%s) at %.0f / %.0f m, ground %.0f m"), Target->DisplayName, *Target->RegionId.ToString(), X, Y, Ground);
}

void UDBCheatManager::DBWalk(float Yaw, float Seconds)
{
	ACharacter* Character = Cast<ACharacter>(GetOuterAPlayerController()->GetPawn());
	if (!Character)
	{
		return;
	}
	const FVector Direction = FRotator(0.f, Yaw, 0.f).Vector();
	const double EndTime = GetWorld()->GetTimeSeconds() + Seconds;
	const TWeakObjectPtr<ACharacter> WeakCharacter = Character;
	const TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	UE_LOG(LogDarkBlood, Display, TEXT("DBWalk: yaw %.0f for %.1f s"), Yaw, Seconds);
	GetWorld()->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakCharacter, Direction, Yaw, EndTime, Handle]()
	{
		ADBPlayerCharacter* Player = Cast<ADBPlayerCharacter>(WeakCharacter.Get());
		const ADBHorse* Horse = ADBHorse::FindRiddenBy(Player);
		if (WeakCharacter.IsValid() && GetWorld()->GetTimeSeconds() < EndTime)
		{
			// In the saddle the same input steers the horse (forward, relative to the given yaw).
			if (Horse && Player)
			{
				Player->ServerSteerHorse(FVector2D(0.f, 1.f), Yaw);
			}
			else
			{
				WeakCharacter->AddMovementInput(Direction);
			}
			return;
		}
		GetWorld()->GetTimerManager().ClearTimer(*Handle);
		if (Horse && Player)
		{
			Player->ServerSteerHorse(FVector2D::ZeroVector, Yaw);
			UE_LOG(LogDarkBlood, Display, TEXT("DBWalk: riding, horse speed %.0f cm/s, stamina %.0f, %s at %s"), Horse->GetVelocity().Size2D(), Horse->GetHorseStamina(),
				*StaticEnum<EMovementMode>()->GetNameStringByValue(Horse->GetCharacterMovement()->MovementMode), *(Horse->GetActorLocation() / 100.0).ToString());
		}
		if (WeakCharacter.IsValid())
		{
			const FVector Meters = WeakCharacter->GetActorLocation() / 100.0;
			UE_LOG(LogDarkBlood, Display, TEXT("DBWalk: done at %.0f / %.0f m, height %.1f m (ground %.1f m), %s"), Meters.X, Meters.Y, Meters.Z,
				DBRealm::SampleHeight(Meters.X, Meters.Y), *StaticEnum<EMovementMode>()->GetNameStringByValue(WeakCharacter->GetCharacterMovement()->MovementMode));
		}
	}), 0.01f, true);
}

void UDBCheatManager::DBDumpSettlements()
{
	if (ForwardToServer(TEXT("DBDumpSettlements"))) return;
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	if (!GameState)
	{
		return;
	}
	const UDBWorldStateComponent* World = GameState->GetWorldState();
	const ADBRealmDirector* Director = ADBRealmDirector::Get(GetWorld());
	UE_LOG(LogDBWorld, Display, TEXT("=== Settlements: day %d, %.2f h ==="), World->GetDay(), World->GetTimeOfDay());
	for (const DarkBlood::Rules::FSettlementState& S : World->GetRulesState().Settlements)
	{
		float Worst = 1.f;
		int32 Levels = 0;
		for (const DarkBlood::Rules::FSettlementBuilding& Building : S.Buildings)
		{
			Worst = FMath::Min(Worst, Building.Condition);
			Levels += Building.Level;
		}
		const FString Name = UTF8_TO_TCHAR(S.SettlementId.c_str());
		const int32 Villagers = Director && Director->GetSettlementLife() ? Director->GetSettlementLife()->CountVillagers(Name) : -1;
		UE_LOG(LogDBWorld, Display, TEXT("  %-16s %hs pop %d (%d/%d/%d) guards %d food %.0f wood %.0f stone %.0f mon %.0f pros %.2f sec %.2f threat %.2f worst %.2f levels %d projects %d villagers %d%s"),
			*Name, S.RegionId.c_str(), S.GetPopulation(), S.Children, S.Adults, S.Elders, S.Guards, S.Stocks.Food, S.Stocks.Wood, S.Stocks.Stone,
			S.Stocks.Money, S.Prosperity, S.Security, S.Threat, Worst, Levels, static_cast<int32>(S.Projects.size()), Villagers,
			S.bStoryProtected ? TEXT(" [story]") : TEXT(""));
	}
}

void UDBCheatManager::DBCarriage(const FString& Destination)
{
	if (ForwardToServer(FString::Printf(TEXT("DBCarriage %s"), *Destination))) return;
	APlayerController* Controller = GetOuterAPlayerController();
	const APawn* Pawn = Controller->GetPawn();
	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	int32 Target = INDEX_NONE;
	for (int32 Index = 0; Index < Settlements.Num() && Target == INDEX_NONE; ++Index)
	{
		Target = FString(Settlements[Index].Name).StartsWith(Destination, ESearchCase::IgnoreCase) ? Index : INDEX_NONE;
	}
	// The nearest station; testing walks the player there first.
	ADBCarriageStation* Nearest = nullptr;
	for (TActorIterator<ADBCarriageStation> It(GetWorld()); It && Pawn; ++It)
	{
		if (!Nearest || FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < FVector::Dist(Nearest->GetActorLocation(), Pawn->GetActorLocation()))
		{
			Nearest = *It;
		}
	}
	FText Reason;
	const bool bTravelled = Nearest && Nearest->Travel(Controller, Target, Reason);
	UE_LOG(LogDarkBlood, Display, TEXT("DBCarriage %s: %s"), *Destination, bTravelled ? TEXT("ok") : *Reason.ToString());
}

void UDBCheatManager::DBDungeonEnter(const FString& Dungeon)
{
	if (ForwardToServer(FString::Printf(TEXT("DBDungeonEnter %s"), *Dungeon))) return;
	const int32 Site = DBDungeon::FindSite(Dungeon);
	const bool bEntered = Site != INDEX_NONE && ADBDungeonInstance::Enter(GetOuterAPlayerController(), Site);
	UE_LOG(LogDarkBlood, Display, TEXT("DBDungeonEnter %s: %s"), *Dungeon, bEntered ? TEXT("ok") : TEXT("unknown dungeon"));
}

void UDBCheatManager::DBDungeonRoom(const FString& Room)
{
	if (ForwardToServer(FString::Printf(TEXT("DBDungeonRoom %s"), *Room))) return;
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	ADBDungeonInstance* Instance = Pawn ? ADBDungeonInstance::FindAt(GetWorld(), Pawn->GetActorLocation()) : nullptr;
	if (!Instance)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBDungeonRoom: not inside a dungeon"));
		return;
	}
	// A room index or a kind (Boss, Treasure, Trap, Rest, Entrance, Combat).
	const DarkBlood::Rules::FDungeonLayout& Layout = Instance->GetLayout();
	int32 Index = Room.IsNumeric() ? FCString::Atoi(*Room) : INDEX_NONE;
	for (int32 Candidate = 0; Index == INDEX_NONE && Candidate < static_cast<int32>(Layout.Rooms.size()); ++Candidate)
	{
		if (Room.Equals(UTF8_TO_TCHAR(DarkBlood::Rules::ToString(Layout.Rooms[static_cast<size_t>(Candidate)].Kind)), ESearchCase::IgnoreCase))
		{
			Index = Candidate;
		}
	}
	if (Index < 0 || Index >= static_cast<int32>(Layout.Rooms.size()))
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBDungeonRoom: no room '%s'"), *Room);
		return;
	}
	Pawn->TeleportTo(Instance->GetRoomCenter(Index) + FVector(0.f, 0.f, 120.f), Pawn->GetActorRotation());
	UE_LOG(LogDarkBlood, Display, TEXT("DBDungeonRoom: room %d (%hs)"), Index, DarkBlood::Rules::ToString(Layout.Rooms[static_cast<size_t>(Index)].Kind));
}

void UDBCheatManager::DBDungeonDump()
{
	if (ForwardToServer(TEXT("DBDungeonDump"))) return;
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	const TArray<FDBDungeonSite>& Sites = DBDungeon::GetSites();
	for (int32 Site = 0; Site < Sites.Num(); ++Site)
	{
		const ADBDungeonInstance* Instance = ADBDungeonInstance::Find(GetWorld(), Site);
		const bool bCleared = GameState && GameState->GetWorldState() && GameState->GetWorldState()->IsDungeonCleared(Sites[Site].Id);
		UE_LOG(LogDarkBlood, Display, TEXT("  %-20s %-10s stage %d gate %.0f / %.0f m%s%s"), *Sites[Site].Name, *Sites[Site].RegionId.ToString(), Sites[Site].Difficulty,
			Sites[Site].Entrance.X, Sites[Site].Entrance.Y, bCleared ? TEXT(" [cleared]") : TEXT(""), Instance ? TEXT(" [open]") : TEXT(""));
		if (!Instance)
		{
			continue;
		}
		const DarkBlood::Rules::FDungeonLayout& Layout = Instance->GetLayout();
		for (int32 Room = 0; Room < static_cast<int32>(Layout.Rooms.size()); ++Room)
		{
			const DarkBlood::Rules::FDungeonRoom& Data = Layout.Rooms[static_cast<size_t>(Room)];
			UE_LOG(LogDarkBlood, Display, TEXT("      room %2d %-8hs depth %d enemies %d state %d alive %d"), Room, DarkBlood::Rules::ToString(Data.Kind), Data.Depth, Data.Enemies,
				Instance->GetRoomState(Room), Instance->CountLivingEnemies(Room));
		}
	}
}

void UDBCheatManager::DBKillNearby(float RadiusMeters)
{
	if (ForwardToServer(FString::Printf(TEXT("DBKillNearby %f"), RadiusMeters))) return;
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	int32 Killed = 0;
	for (TActorIterator<ADBEnemyCharacter> It(GetWorld()); It && Pawn; ++It)
	{
		UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent();
		if (It->IsDead() || !ASC || FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) > RadiusMeters * 100.f)
		{
			continue;
		}
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None);
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UDBAttributeSet::GetIncomingDamageAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(99999.f));
		Effect->Modifiers.Add(Modifier);
		ASC->ApplyGameplayEffectToSelf(Effect, 1.f, ASC->MakeEffectContext());
		++Killed;
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBKillNearby: %d enemies"), Killed);
}

namespace
{
	const TCHAR* ArenaStateName(const ADBBossArena* Arena)
	{
		if (!Arena)
		{
			return TEXT("no arena");
		}
		switch (Arena->GetArenaState())
		{
		case EDBArenaState::Idle: return TEXT("waiting");
		case EDBArenaState::Fighting: return TEXT("fighting");
		case EDBArenaState::Defeated: return TEXT("defeated");
		}
		return TEXT("?");
	}

	ADBBossCharacter* FindNearestBoss(const UWorld* World, const APawn* Pawn)
	{
		ADBBossCharacter* Nearest = nullptr;
		for (TActorIterator<ADBBossCharacter> It(const_cast<UWorld*>(World)); It && Pawn; ++It)
		{
			if (!It->IsDead() && (!Nearest || FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < FVector::Dist(Nearest->GetActorLocation(), Pawn->GetActorLocation())))
			{
				Nearest = *It;
			}
		}
		return Nearest;
	}
}

void UDBCheatManager::DBBossList()
{
	if (ForwardToServer(TEXT("DBBossList"))) return;
	for (const UDBBossDefinition* Boss : DBBosses::GetAll())
	{
		const FVector Arena = DBBosses::GetArenaLocation(*Boss);
		UE_LOG(LogDarkBlood, Display, TEXT("DBBOSS %-18s %2d %-12s %-10s %-34s hp %5.0f phases %d arena (%.0f, %.0f, %.0f) m: %s"), *Boss->BossId.ToString(), Boss->Order,
			*Boss->DisplayName.ToString(), *Boss->RegionId.ToString(), *Boss->Title.ToString(), Boss->MaxHealth, Boss->Phases.Num(), Arena.X / 100.0, Arena.Y / 100.0,
			Arena.Z / 100.0, ArenaStateName(ADBBossArena::Find(GetWorld(), Boss->BossId)));
	}
	if (const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>())
	{
		const DarkBlood::Rules::FWorldState& State = GameState->GetWorldState()->GetRulesState();
		UE_LOG(LogDarkBlood, Display, TEXT("DBBOSS vassals %d/%d, Das Ende %s, demon king %s"), State.CountDefeatedVassals(), DarkBlood::Rules::NumVassals,
			State.IsFinalRegionOpen() ? TEXT("open") : TEXT("sealed"), State.IsDemonKingReachable() ? TEXT("reachable") : TEXT("sealed"));
	}
}

void UDBCheatManager::DBBossArena(const FString& Boss)
{
	if (ForwardToServer(FString::Printf(TEXT("DBBossArena %s"), *Boss))) return;
	const UDBBossDefinition* Definition = DBBosses::FindByName(Boss);
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	const FVector Arena = Definition ? DBBosses::GetArenaLocation(*Definition) : FVector::ZeroVector;
	if (!Pawn || Arena.IsZero())
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBBossArena: no arena for '%s'"), *Boss);
		return;
	}
	// Inside the ring, west of its center, facing east (toward the boss).
	const double X = Arena.X - Definition->ArenaRadius * 0.5;
	const double Ground = DBRealm::SampleHeight(X / 100.0, Arena.Y / 100.0) * 100.0;
	const FVector Destination(X, Arena.Y, FMath::Max(Ground, 0.0) + 250.0);
	Pawn->TeleportTo(Destination, FRotator::ZeroRotator);
	if (AController* Controller = Pawn->GetController())
	{
		Controller->SetControlRotation(FRotator(-10.f, 0.f, 0.f));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBBossArena %s at (%.0f, %.0f) m"), *Definition->BossId.ToString(), Destination.X / 100.0, Destination.Y / 100.0);
}

void UDBCheatManager::DBBossSpawn(const FString& Boss, float Distance)
{
	if (ForwardToServer(FString::Printf(TEXT("DBBossSpawn %s %f"), *Boss, Distance))) return;
	const UDBBossDefinition* Definition = DBBosses::FindByName(Boss);
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (!Definition || !Pawn)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBBossSpawn: unknown boss '%s'"), *Boss);
		return;
	}
	const FVector Location = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * Distance + FVector(0.f, 0.f, 150.f);
	const int32 Players = GetWorld()->GetGameState() ? GetWorld()->GetGameState()->PlayerArray.Num() : 1;
	const ADBBossCharacter* Spawned = ADBBossCharacter::SpawnBoss(GetWorld(), Definition, Location, (-Pawn->GetActorForwardVector()).Rotation(), Players);
	UE_LOG(LogDarkBlood, Display, TEXT("DBBossSpawn %s: %s"), *Definition->BossId.ToString(), Spawned ? TEXT("ok") : TEXT("failed"));
}

void UDBCheatManager::DBBossDump()
{
	if (ForwardToServer(TEXT("DBBossDump"))) return;
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	int32 Count = 0;
	for (TActorIterator<ADBBossCharacter> It(GetWorld()); It; ++It)
	{
		const UDBBossDefinition* Definition = It->GetDefinition();
		const UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent();
		UE_LOG(LogDarkBlood, Display, TEXT("DBBOSSDUMP %s %s phase %d/%d health %.0f/%.0f (%.0f%%) attack %.1f fight %.0f s specials %d adds %d scale %.2f dist %.0f m"),
			*It->GetBossId().ToString(), It->IsDead() ? TEXT("dead") : TEXT("alive"), It->GetPhase() + 1, Definition ? Definition->Phases.Num() : 0,
			ASC->GetNumericAttribute(UDBAttributeSet::GetHealthAttribute()), ASC->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute()), It->GetHealthFraction() * 100.f,
			ASC->GetNumericAttribute(UDBAttributeSet::GetAttackPowerAttribute()), It->GetFightSeconds(), It->GetSpecialAttacks(), It->CountLivingAdds(), It->GetActorScale3D().X,
			Pawn ? FVector::Dist(Pawn->GetActorLocation(), It->GetActorLocation()) / 100.0 : 0.0);
		++Count;
	}
	for (TActorIterator<ADBBossArena> It(GetWorld()); It; ++It)
	{
		if (It->GetArenaState() != EDBArenaState::Idle || (Pawn && FVector::Dist(Pawn->GetActorLocation(), It->GetActorLocation()) < 10000.f))
		{
			UE_LOG(LogDarkBlood, Display, TEXT("DBBOSSDUMP arena %s: %s"), *It->GetBossId().ToString(), ArenaStateName(*It));
		}
	}
	for (const APlayerState* Entry : GetWorld()->GetGameState()->PlayerArray)
	{
		const APawn* Player = Entry ? Entry->GetPawn() : nullptr;
		const FVector At = Player ? Player->GetActorLocation() : FVector::ZeroVector;
		UE_LOG(LogDarkBlood, Display, TEXT("DBBOSSDUMP player %s at (%.0f, %.0f, %.0f) m %s"), *Entry->GetPlayerName(), At.X / 100.0, At.Y / 100.0, At.Z / 100.0,
			Player ? *Player->GetName() : TEXT("no pawn"));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBBOSSDUMP %d bosses"), Count);
}

void UDBCheatManager::DBBossHurt(float HealthFraction)
{
	if (ForwardToServer(FString::Printf(TEXT("DBBossHurt %f"), HealthFraction))) return;
	ADBBossCharacter* Boss = FindNearestBoss(GetWorld(), GetOuterAPlayerController()->GetPawn());
	UAbilitySystemComponent* ASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBBossHurt: no boss"));
		return;
	}
	// Through the damage pipeline (death, phases): a raw IncomingDamage effect.
	const float Damage = ASC->GetNumericAttribute(UDBAttributeSet::GetHealthAttribute()) - HealthFraction * ASC->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute());
	if (Damage > 0.f)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None);
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UDBAttributeSet::GetIncomingDamageAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Damage));
		Effect->Modifiers.Add(Modifier);
		ASC->ApplyGameplayEffectToSelf(Effect, 1.f, ASC->MakeEffectContext());
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBBossHurt %s -> %.0f%%"), *Boss->GetBossId().ToString(), Boss->GetHealthFraction() * 100.f);
}

void UDBCheatManager::DBBossDefeat(const FString& Boss)
{
	if (ForwardToServer(FString::Printf(TEXT("DBBossDefeat %s"), *Boss))) return;
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	if (!GameState)
	{
		return;
	}
	const bool bOuter = Boss.Equals(TEXT("Outer"), ESearchCase::IgnoreCase);
	const UDBBossDefinition* Named = bOuter ? nullptr : DBBosses::FindByName(Boss);
	int32 Count = 0;
	for (const UDBBossDefinition* Definition : DBBosses::GetAll())
	{
		if ((bOuter && Definition->Rank == EDBBossRank::Vassal && Definition->Order <= DarkBlood::Rules::NumVassalRegions) || Definition == Named)
		{
			GameState->GetWorldState()->NotifyBossDefeated(Definition->BossId, Definition->Rank, Definition->RegionId);
			++Count;
		}
	}
	const DarkBlood::Rules::FWorldState& State = GameState->GetWorldState()->GetRulesState();
	UE_LOG(LogDarkBlood, Display, TEXT("DBBossDefeat %s: %d bosses; vassals %d/%d, Das Ende %s, demon king %s"), *Boss, Count, State.CountDefeatedVassals(),
		DarkBlood::Rules::NumVassals, State.IsFinalRegionOpen() ? TEXT("open") : TEXT("sealed"), State.IsDemonKingReachable() ? TEXT("reachable") : TEXT("sealed"));
}

namespace
{
	FName ResolveRegion(const FString& Region)
	{
		if (Region.IsNumeric())
		{
			return FName(*FString::Printf(TEXT("Region%02d"), FCString::Atoi(*Region)));
		}
		for (const FDBRealmRegion& Candidate : DBRealm::GetRegions())
		{
			if (Candidate.RegionId.ToString().Equals(Region, ESearchCase::IgnoreCase) || FString(Candidate.DisplayName).StartsWith(Region, ESearchCase::IgnoreCase))
			{
				return Candidate.RegionId;
			}
		}
		return FName(*Region);
	}
}

void UDBCheatManager::DBBossSignature()
{
	if (ForwardToServer(TEXT("DBBossSignature"))) return;
	ADBBossCharacter* Boss = FindNearestBoss(GetWorld(), GetOuterAPlayerController()->GetPawn());
	if (!Boss)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBBossSignature: no boss"));
		return;
	}
	Boss->ForceSignature();
	UE_LOG(LogDarkBlood, Display, TEXT("DBBossSignature %s: %d signatures, %d adds"), *Boss->GetBossId().ToString(), Boss->GetSignatures(), Boss->CountLivingAdds());
}

void UDBCheatManager::DBRegionDump()
{
	if (ForwardToServer(TEXT("DBRegionDump"))) return;
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	if (!GameState)
	{
		return;
	}
	const DarkBlood::Rules::FWorldState& State = GameState->GetWorldState()->GetRulesState();
	const bool bNight = GameState->GetWorldState()->IsNight();
	for (const DarkBlood::Rules::FRegionState& Region : State.GetRegions())
	{
		const FName RegionId(UTF8_TO_TCHAR(Region.RegionId.c_str()));
		const ADBDemonCamp* Camp = ADBDemonCamp::Find(GetWorld(), RegionId);
		UE_LOG(LogDarkBlood, Display, TEXT("DBREGION %-9s %-9hs influence %.2f packs/player %d (%s) camp %s"), *RegionId.ToString(), DarkBlood::Rules::ToString(Region.Control),
			Region.DemonInfluence, DarkBlood::Rules::GetRegionalPackBudget(Region, bNight), bNight ? TEXT("night") : TEXT("day"),
			Camp ? (Camp->IsBroken() ? TEXT("broken") : Camp->GetCommander() ? TEXT("fighting") : TEXT("standing")) : TEXT("-"));
	}
	if (const ADBRealmDirector* Director = ADBRealmDirector::Get(GetWorld()); Director && Director->GetRegionLife())
	{
		Director->GetRegionLife()->LogState();
	}
	if (const ADBPlayerState* Player = GetDBPlayerState())
	{
		UE_LOG(LogDarkBlood, Display, TEXT("DBREGION player in %s"), *Player->GetCurrentRegionId().ToString());
	}
}

void UDBCheatManager::DBCamp(const FString& Region)
{
	if (ForwardToServer(FString::Printf(TEXT("DBCamp %s"), *Region))) return;
	const FName RegionId = ResolveRegion(Region);
	const FVector Camp = DBRegions::GetCampLocation(RegionId);
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (!Pawn || Camp.IsZero())
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("DBCamp: no camp in '%s'"), *Region);
		return;
	}
	// 48 m short of the camp (inside its wake radius), facing it.
	const FVector2D Here = FVector2D(Camp) / 100.0 + FVector2D(-48.0, 0.0);
	const FVector Destination(Here.X * 100.0, Here.Y * 100.0, FMath::Max(DBRealm::SampleHeight(Here.X, Here.Y), 0.0) * 100.0 + 250.0);
	Pawn->TeleportTo(Destination, FRotator::ZeroRotator);
	if (AController* Controller = Pawn->GetController())
	{
		Controller->SetControlRotation(FRotator(-8.f, 0.f, 0.f));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBCamp %s at (%.0f, %.0f) m"), *RegionId.ToString(), Camp.X / 100.0, Camp.Y / 100.0);
}

void UDBCheatManager::DBRegionPack()
{
	if (ForwardToServer(TEXT("DBRegionPack"))) return;
	const ADBRealmDirector* Director = ADBRealmDirector::Get(GetWorld());
	const bool bSpawned = Director && Director->GetRegionLife() && Director->GetRegionLife()->SpawnPackNear(GetDBPlayerState(), true);
	UE_LOG(LogDarkBlood, Display, TEXT("DBRegionPack: %s"), bSpawned ? TEXT("spawned") : TEXT("no pack (region without demons or no free ground)"));
}

void UDBCheatManager::DBRide()
{
	if (ForwardToServer(TEXT("DBRide"))) return;
	APlayerController* Controller = GetOuterAPlayerController();
	APawn* Pawn = Controller->GetPawn();
	ADBHorse* Horse = ADBHorse::FindRiddenBy(Pawn);
	if (!Horse)
	{
		Horse = ADBHorse::CallHorse(Pawn);
	}
	if (Horse)
	{
		Horse->Interact(Controller);
		UE_LOG(LogDarkBlood, Display, TEXT("DBRide: %s"), ADBHorse::FindRiddenBy(Pawn) ? TEXT("mounted") : TEXT("dismounted"));
	}
}

void UDBCheatManager::DBSurvival(float Satiety, float Warmth)
{
	if (ForwardToServer(FString::Printf(TEXT("DBSurvival %f %f"), Satiety, Warmth))) return;
	if (ADBPlayerState* PlayerState = GetDBPlayerState(); PlayerState && PlayerState->GetSurvival())
	{
		PlayerState->GetSurvival()->SetValues(Satiety, Warmth);
	}
}

void UDBCheatManager::DBDumpVillagers()
{
	if (ForwardToServer(TEXT("DBDumpVillagers"))) return;
	int32 Count = 0;
	for (TActorIterator<ADBVillagerCharacter> It(GetWorld()); It; ++It)
	{
		const FVector Meters = It->GetActorLocation() / 100.0;
		UE_LOG(LogDBWorld, Display, TEXT("  villager %-8s of %-12s at %.1f / %.1f m, speed %.0f cm/s"), *It->GetCombatDisplayName(), *It->GetSettlementId().ToString(),
			Meters.X, Meters.Y, It->GetVelocity().Size2D());
		++Count;
	}
	UE_LOG(LogDBWorld, Display, TEXT("DBDumpVillagers: %d"), Count);
}

void UDBCheatManager::DBSkipHours(float Hours)
{
	if (ForwardToServer(FString::Printf(TEXT("DBSkipHours %f"), Hours))) return;
	if (const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>())
	{
		GameState->GetWorldState()->SkipHours(Hours);
		UE_LOG(LogDBWorld, Display, TEXT("DBSkipHours: %.1f h"), Hours);
	}
}

void UDBCheatManager::DBSettlementAttack(const FString& Settlement)
{
	if (ForwardToServer(FString::Printf(TEXT("DBSettlementAttack %s"), *Settlement))) return;
	const ADBRealmDirector* Director = ADBRealmDirector::Get(GetWorld());
	const bool bStarted = Director && Director->GetSettlementLife() && Director->GetSettlementLife()->StartAttackEncounter(Settlement);
	UE_LOG(LogDBWorld, Display, TEXT("DBSettlementAttack %s: %s"), *Settlement, bStarted ? TEXT("started") : TEXT("no player there / unknown"));
}

void UDBCheatManager::DBSpawnShip(float Distance, int32 Style)
{
	if (ForwardToServer(FString::Printf(TEXT("DBSpawnShip %f %d"), Distance, Style))) return;
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (Pawn)
	{
		const EDBShipStyle ShipStyle = static_cast<EDBShipStyle>(FMath::Clamp(Style, 0, static_cast<int32>(EDBShipStyle::Boat)));
		const FVector At = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * Distance;
		ADBShip::SpawnAt(GetWorld(), FVector2D(At), Pawn->GetActorRotation().Yaw + 90.f, DBShipArt::GetSpec(ShipStyle).DisplayName, ShipStyle);
	}
}

void UDBCheatManager::DBModelShowroom(float Spacing)
{
	for (AActor* Actor : ShowroomActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	const bool bWasSet = ShowroomActors.Num() > 0;
	ShowroomActors.Reset();
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (bWasSet || !Pawn)
	{
		return;
	}
	const FVector Forward = FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f).Vector();
	const FVector Right(-Forward.Y, Forward.X, 0.f);
	int32 Index = 0;
	for (const DBModels::FModelInfo& Info : DBModels::GetAll())
	{
		// A row across the view, 30 m ahead; each model stands on the ground below its spot, facing the player (+X towards).
		FVector At = Pawn->GetActorLocation() + Forward * 3000.f + Right * (Index - DBModels::GetAll().Num() * 0.5f) * Spacing;
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 20000.f), At - FVector(0.f, 0.f, 20000.f), ECC_WorldStatic))
		{
			At = Hit.ImpactPoint;
		}
		DBArtBuild::FArtBuilder Builder{*GetWorld(), FTransform(FRotator(0.f, Pawn->GetActorRotation().Yaw + 180.f, 0.f), At), ShowroomActors};
		const ADBPropActor* Model = Builder.Model(FVector::ZeroVector, 0.f, Info.Key);
		FVector Origin, Extent;
		if (Model)
		{
			Model->GetActorBounds(false, Origin, Extent);
		}
		UE_LOG(LogDarkBlood, Display, TEXT("DBModelShowroom %d %s: %d parts, %s, size %s"), Index, Info.Key, DBModels::GetParts(Info.Key).Num(),
			Model ? TEXT("placed") : TEXT("MISSING"), *(Extent * 2.f).ToCompactString());
		++Index;
	}
}

void UDBCheatManager::DBModelShow(const FString& Key, float Distance, float Yaw)
{
	for (AActor* Actor : ShowroomActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	ShowroomActors.Reset();
	const APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (!Pawn)
	{
		return;
	}
	FVector At = Pawn->GetActorLocation() + FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f).Vector() * Distance;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 20000.f), At - FVector(0.f, 0.f, 20000.f), ECC_WorldStatic))
	{
		At = Hit.ImpactPoint;
	}
	DBArtBuild::FArtBuilder Builder{*GetWorld(), FTransform(FRotator(0.f, Pawn->GetActorRotation().Yaw + 180.f + Yaw, 0.f), At), ShowroomActors};
	const ADBPropActor* Model = Builder.Model(FVector::ZeroVector, 0.f, *Key);
	UE_LOG(LogDarkBlood, Display, TEXT("DBModelShow %s: %s"), *Key, Model ? TEXT("placed") : TEXT("MISSING"));
}

void UDBCheatManager::DBBoardShip(int32 Style)
{
	if (ForwardToServer(FString::Printf(TEXT("DBBoardShip %d"), Style))) return;
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (!Pawn)
	{
		return;
	}
	ADBShip* Best = nullptr;
	for (TActorIterator<ADBShip> It(GetWorld()); It; ++It)
	{
		if (static_cast<int32>(It->Style) == Style
			&& (!Best || FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) < FVector::DistSquared(Best->GetActorLocation(), Pawn->GetActorLocation())))
		{
			Best = *It;
		}
	}
	if (Best)
	{
		const bool bTeleported = Pawn->TeleportTo(Best->GetActorLocation() + FVector(0.f, 0.f, 200.f), Best->GetActorRotation());
		UE_LOG(LogDarkBlood, Display, TEXT("DBBoardShip: on %s (teleport %d, ship at %s)"), *Best->ShipName.ToString(), bTeleported ? 1 : 0,
			*Best->GetActorLocation().ToCompactString());
		const TWeakObjectPtr<APawn> WeakPawn = Pawn;
		FTimerHandle Check;
		GetWorld()->GetTimerManager().SetTimer(Check, FTimerDelegate::CreateWeakLambda(this, [WeakPawn]()
		{
			if (const ACharacter* Character = Cast<ACharacter>(WeakPawn.Get()))
			{
				UE_LOG(LogDarkBlood, Display, TEXT("DBBoardShip: standing at %s on %s"), *Character->GetActorLocation().ToCompactString(),
					*GetNameSafe(Cast<UPrimitiveComponent>(Character->GetMovementBaseObject()) ? Cast<UPrimitiveComponent>(Character->GetMovementBaseObject())->GetOwner() : nullptr));
			}
		}), 1.5f, false);
	}
}

void UDBCheatManager::DBSail(float Rudder, float Sails, float Seconds)
{
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		// The client's own view: replicated ships and whether the pawn rides one.
		const APawn* Viewer = GetOuterAPlayerController()->GetPawn();
		int32 Ships = 0;
		for (TActorIterator<ADBShip> It(GetWorld()); It; ++It)
		{
			++Ships;
		}
		UE_LOG(LogDarkBlood, Display, TEXT("DBSail (client): %d ships, pawn at %s riding %s"), Ships, Viewer ? *Viewer->GetActorLocation().ToCompactString() : TEXT("-"),
			Viewer ? *GetNameSafe(Viewer->GetAttachParentActor()) : TEXT("-"));
	}
	if (ForwardToServer(FString::Printf(TEXT("DBSail %f %f %f"), Rudder, Sails, Seconds))) return;
	APlayerController* Controller = GetOuterAPlayerController();
	APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		return;
	}
	ADBShip* Ship = ADBShip::FindSteeredBy(Pawn);
	if (!Ship)
	{
		double Best = TNumericLimits<double>::Max();
		for (TActorIterator<ADBShip> It(GetWorld()); It; ++It)
		{
			const double Distance = FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation());
			if (Distance < Best && It->CanInteract(Pawn))
			{
				Best = Distance;
				Ship = *It;
			}
		}
		if (!Ship)
		{
			UE_LOG(LogDarkBlood, Warning, TEXT("DBSail: no free ship"));
			return;
		}
		// Board it: stand on the deck, then take the helm like the interaction does.
		Pawn->TeleportTo(Ship->GetActorLocation() + FVector(0.f, 0.f, 200.f), Pawn->GetActorRotation());
		Ship->Interact(Controller);
	}
	Ship->SetSteering(FVector2D(Rudder, Sails));
	const FVector Start = Ship->GetActorLocation();
	// What lies under the hull: the sea floor far below, or (wrongly) ground at the water line.
	FHitResult Ground;
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(DBSail), false, Ship);
	GroundParams.AddIgnoredActor(Pawn);
	if (GetWorld()->LineTraceSingleByChannel(Ground, Start + FVector(0.f, 0.f, 5000.f), Start - FVector(0.f, 0.f, 10000.f), ECC_Visibility, GroundParams))
	{
		UE_LOG(LogDarkBlood, Display, TEXT("DBSail: under the hull %s (%s) at Z %.0f, layout height %.1f m"), *GetNameSafe(Ground.GetActor()),
			*GetNameSafe(Ground.GetComponent()), Ground.ImpactPoint.Z, DBRealm::SampleHeight(Start.X / 100.0, Start.Y / 100.0));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBSail: %s at %s, rudder %.1f sails %.1f for %.0f s"), *Ship->ShipName.ToString(), *Start.ToCompactString(), Rudder, Sails, Seconds);
	const TWeakObjectPtr<ADBShip> WeakShip = Ship;
	FTimerHandle Stop;
	GetWorld()->GetTimerManager().SetTimer(Stop, FTimerDelegate::CreateWeakLambda(this, [WeakShip, Start]()
	{
		if (WeakShip.IsValid())
		{
			WeakShip->SetSteering(FVector2D::ZeroVector);
			const APawn* Helmsman = WeakShip->GetHelmsman();
			UE_LOG(LogDarkBlood, Display, TEXT("DBSail: sailed %.0f m to %s, yaw %.0f, helmsman %s at %s"), FVector::Dist2D(Start, WeakShip->GetActorLocation()) / 100.0,
				*WeakShip->GetActorLocation().ToCompactString(), WeakShip->GetActorRotation().Yaw, *GetNameSafe(Helmsman),
				Helmsman ? *Helmsman->GetActorLocation().ToCompactString() : TEXT("-"));
		}
	}), FMath::Max(0.1f, Seconds), false);
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
