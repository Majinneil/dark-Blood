#include "Visual/DBCombatFeedback.h"

#include "Character/DBCharacterBase.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

namespace
{
	struct FFxEntry
	{
		const TCHAR* Name;
		const TCHAR* Path;
		float Scale;
	};

	/** One entry per EDBCombatFx (same order). */
	const FFxEntry Entries[] = {
		{TEXT("HitByPlayer"), TEXT("/Game/ParagonGreystone/FX/Particles/Greystone/Abilities/Primary/FX/P_Greystone_Primary_Impact.P_Greystone_Primary_Impact"), 0.8f},
		{TEXT("HitByPlayerHeavy"), TEXT("/Game/ParagonGrux/FX/Particles/Abilities/Primary/FX/P_Grux_Melee_SucessfulImpact.P_Grux_Melee_SucessfulImpact"), 0.9f},
		{TEXT("HitByDemon"), TEXT("/Game/ParagonKhaimera/FX/ParticleSystems/Abilities/Primary/FX/P_Khaimera_LMB_Impact.P_Khaimera_LMB_Impact"), 0.8f},
		{TEXT("Blocked"), TEXT("/Game/ParagonGreystone/FX/Particles/Greystone/Abilities/Primary/FX/P_Greystone_Primary_Sparks.P_Greystone_Primary_Sparks"), 0.9f},
		// A shower of golden sparks: steel on steel (Kwang's sword impact calls lightning from the sky - too much).
		{TEXT("Parried"), TEXT("/Game/ParagonGreystone/FX/Particles/Greystone/Abilities/Primary/FX/P_Greystone_Primary_Sparks.P_Greystone_Primary_Sparks"), 1.9f},
		{TEXT("Stagger"), TEXT("/Game/ParagonKhaimera/FX/ParticleSystems/Abilities/ThreeStrikeBuff/FX/P_Q_Impact.P_Q_Impact"), 0.7f},
		{TEXT("DemonDeath"), TEXT("/Game/ParagonMinions/FX/Particles/Buffs/Buff_Black/FX/P_BlackBuff_Spawn.P_BlackBuff_Spawn"), 0.9f},
		{TEXT("BossDeath"), TEXT("/Game/ParagonGrux/FX/Particles/Abilities/Primary/FX/P_Grux_Melee_ShockwaveImpact.P_Grux_Melee_ShockwaveImpact"), 2.2f},
		{TEXT("GroundBlast"), TEXT("/Game/ParagonGrux/FX/Particles/Abilities/Primary/FX/P_Grux_Melee_ShockwaveImpact.P_Grux_Melee_ShockwaveImpact"), 1.f},
		{TEXT("WaterSplash"), TEXT("/Game/ParagonKallari/FX/Particles/Kallari/Abilities/DaggerThrow/FX/P_Kallari_DaggerThrow_HitWorld_WaterImpact.P_Kallari_DaggerThrow_HitWorld_WaterImpact"), 3.f},
	};
	static_assert(UE_ARRAY_COUNT(Entries) == static_cast<int32>(EDBCombatFx::Count), "one effect per EDBCombatFx");

	bool CanRender(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_DedicatedServer && FApp::CanEverRender();
	}
}

const TCHAR* DBCombatFeedback::GetName(EDBCombatFx Fx)
{
	return Entries[static_cast<int32>(Fx)].Name;
}

UParticleSystem* DBCombatFeedback::GetSystem(EDBCombatFx Fx)
{
	static TArray<TWeakObjectPtr<UParticleSystem>> Loaded;
	static TArray<bool> Tried;
	const int32 Index = static_cast<int32>(Fx);
	if (Loaded.Num() == 0)
	{
		Loaded.SetNum(static_cast<int32>(EDBCombatFx::Count));
		Tried.Init(false, static_cast<int32>(EDBCombatFx::Count));
	}
	if (!Loaded[Index].IsValid() && !Tried[Index])
	{
		Tried[Index] = true;
		UParticleSystem* System = LoadObject<UParticleSystem>(nullptr, Entries[Index].Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (System)
		{
			System->AddToRoot(); // a handful of small systems, kept for the session
		}
		Loaded[Index] = System;
		UE_LOG(LogDarkBlood, Log, TEXT("Combat FX %s: %s"), Entries[Index].Name, System ? TEXT("loaded") : TEXT("missing (pack not installed)"));
	}
	return Loaded[Index].Get();
}

void DBCombatFeedback::Play(const UObject* WorldContext, EDBCombatFx Fx, const FVector& Location, const FVector& Normal, float Scale)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UParticleSystem* System = CanRender(World) ? GetSystem(Fx) : nullptr;
	if (!System)
	{
		return;
	}
	const FRotator Facing = Normal.IsNearlyZero() ? FRotator::ZeroRotator : Normal.Rotation();
	UGameplayStatics::SpawnEmitterAtLocation(World, System, FTransform(Facing, Location, FVector(Entries[static_cast<int32>(Fx)].Scale * Scale)), true,
		EPSCPoolMethod::AutoRelease);
}

void DBCombatFeedback::HandleCue(AActor* Target, FGameplayTag Cue, const FGameplayCueParameters& Parameters)
{
	UE_LOG(LogDBCombat, Verbose, TEXT("Combat cue %s on %s (%.1f)"), *Cue.ToString(), *GetNameSafe(Target), Parameters.RawMagnitude);
	if (!Target || !CanRender(Target->GetWorld()))
	{
		return;
	}
	// The server sends where the blow landed; older callers without a place hit the chest.
	const FVector Location = Parameters.Location.IsNearlyZero() ? Target->GetActorLocation() + FVector(0.f, 0.f, 40.f) : FVector(Parameters.Location);
	const FVector Normal = Parameters.Normal;
	const ADBCharacterBase* Character = Cast<ADBCharacterBase>(Target);
	const bool bDemonStruck = Character && Character->GetTeam() == EDBTeam::Demons;
	if (Cue == DBTags::GameplayCue_Combat_Hit)
	{
		const EDBCombatFx Fx = !bDemonStruck ? EDBCombatFx::HitByDemon
							 : Parameters.RawMagnitude >= HeavyHitDamage ? EDBCombatFx::HitByPlayerHeavy
																		  : EDBCombatFx::HitByPlayer;
		Play(Target, Fx, Location, Normal);
	}
	else if (Cue == DBTags::GameplayCue_Combat_Blocked)
	{
		Play(Target, EDBCombatFx::Blocked, Location, Normal);
	}
	else if (Cue == DBTags::GameplayCue_Combat_Parried)
	{
		Play(Target, EDBCombatFx::Parried, Location, Normal);
	}
	else if (Cue == DBTags::GameplayCue_Combat_Stagger)
	{
		Play(Target, EDBCombatFx::Stagger, Target->GetActorLocation() + FVector(0.f, 0.f, 60.f), FVector::UpVector);
	}
}
