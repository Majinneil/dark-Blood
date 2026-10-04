// Class signature abilities (granted by skill tree nodes) and passive nodes.
//   Krieger:        Eiserne Haltung, Durchbruch
//   Schattenlaeufer: Schattenmal, Rauchschleier
//   Magier:         Magiegeschoss/Frostlanze (moveset), Schutzkreis, Flug
//   Moench:         Konterhaltung, Himmelstritt
// Ranks of the granting node are read by the abilities themselves (mechanic upgrades, no filler stats).
#pragma once

#include "Abilities/DBGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"

#include "DBClassAbilities.generated.h"

struct FGameplayEventData;

// ---- Skill node ids (shared by abilities, development content and docs) ----------------------------
namespace DBSkillNodes
{
	inline const FName IronStance(TEXT("W_IronStance"));
	inline const FName Breakthrough(TEXT("W_Breakthrough"));
	inline const FName CounterSlash(TEXT("W_CounterSlash"));
	inline const FName Bloodlust(TEXT("W_Bloodlust"));
	inline const FName ShadowMark(TEXT("S_ShadowMark"));
	inline const FName SmokeVeil(TEXT("S_SmokeVeil"));
	inline const FName Ambush(TEXT("S_Ambush"));
	inline const FName LightFooted(TEXT("S_LightFooted"));
	inline const FName WardingCircle(TEXT("M_WardingCircle"));
	inline const FName Flight(TEXT("M_Flight"));
	inline const FName ChainBolt(TEXT("M_ChainBolt"));
	inline const FName ManaFlow(TEXT("M_ManaFlow"));
	inline const FName CounterStance(TEXT("K_CounterStance"));
	inline const FName SkyKick(TEXT("K_SkyKick"));
	inline const FName InnerCalm(TEXT("K_InnerCalm"));
	inline const FName IronBody(TEXT("K_IronBody"));
	inline const FName DoubleJump(TEXT("X_DoubleJump"));
}

// ---- Krieger ----------------------------------------------------------------------------------------

/** Toggle: +armor, +poise, blocks cost half stamina, 20 % slower. Rank 2: poise damage halved. */
UCLASS()
class DARKBLOOD_API UDBAbility_IronStance : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_IronStance();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

private:
	FActiveGameplayEffectHandle StanceEffect;
	float BaseWalkSpeed = 0.f;
	bool bAddedUnshakable = false;
};

/** Charging dash through enemies: damage and knockdown along the path. Rank 2: shorter cooldown. */
UCLASS()
class DARKBLOOD_API UDBAbility_Breakthrough : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_Breakthrough();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual float GetCooldownSeconds() const override;

	UFUNCTION()
	void OnDashFinished();

private:
	FVector DashStart = FVector::ZeroVector;
};

// ---- Schattenlaeufer --------------------------------------------------------------------------------

/**
 * First use marks an enemy (lock-on target or the nearest one ahead); second use teleports behind it with
 * brief invulnerability and an empowered next hit. Rank 2: the arrival deals shadow damage. Server only.
 */
UCLASS()
class DARKBLOOD_API UDBAbility_ShadowMark : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_ShadowMark();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual float GetCooldownSeconds() const override { return 0.f; } // applied manually after the teleport

private:
	AActor* FindMarkTarget() const;

	TWeakObjectPtr<AActor> MarkedTarget;
	double MarkExpiresAt = 0.0;
};

/** Enemies lose track of you for a few seconds; attacking breaks the veil. */
UCLASS()
class DARKBLOOD_API UDBAbility_SmokeVeil : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_SmokeVeil();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnVeilEnded();
};

// ---- Magier -----------------------------------------------------------------------------------------

/** Ranged attack: after a short windup a projectile flies at the focus target / camera direction. */
UCLASS(Abstract)
class DARKBLOOD_API UDBAbility_ProjectileAttack : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_ProjectileAttack();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnWindupFinished();

	UFUNCTION()
	void OnRecoveryFinished();

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Projectile")
	float Damage = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Projectile")
	float PoiseDamage = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Projectile", meta = (Categories = "Damage.Type"))
	FGameplayTag DamageType;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Projectile")
	float Speed = 2400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Projectile")
	float WindupSeconds = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Projectile")
	float RecoverySeconds = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Projectile")
	FLinearColor Color = FLinearColor(1.f, 0.4f, 0.1f);

	/** Skill node whose rank adds chain jumps (None = no chaining). */
	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Projectile")
	FName ChainNodeId;
};

/** Mage light attack: fire bolt. */
UCLASS()
class DARKBLOOD_API UDBAbility_MagicBolt : public UDBAbility_ProjectileAttack
{
	GENERATED_BODY()

public:
	UDBAbility_MagicBolt();
};

/** Mage heavy attack: slow, strong frost lance. */
UCLASS()
class DARKBLOOD_API UDBAbility_FrostLance : public UDBAbility_ProjectileAttack
{
	GENERATED_BODY()

public:
	UDBAbility_FrostLance();
};

/** Places a protective circle (see ADBWardingCircle). Rank 2: allies inside are healed. */
UCLASS()
class DARKBLOOD_API UDBAbility_WardingCircle : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_WardingCircle();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Toggle: fly in the camera direction while mana lasts. */
UCLASS()
class DARKBLOOD_API UDBAbility_Flight : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_Flight();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	UFUNCTION()
	void OnDrainTick();

	void ScheduleDrain();

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Flight")
	float ManaPerSecond = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Flight")
	float FlySpeed = 750.f;
};

// ---- Moench -----------------------------------------------------------------------------------------

/** Short stance: the next hit is caught (like a perfect parry) and answered with a counter strike. */
UCLASS()
class DARKBLOOD_API UDBAbility_CounterStance : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_CounterStance();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnCounterTriggered(FGameplayEventData Payload);

	UFUNCTION()
	void OnStanceEnded();
};

/** Launches the monk (and enemies in front) into the air - follow up with air attacks. */
UCLASS()
class DARKBLOOD_API UDBAbility_SkyKick : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_SkyKick();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnKickFinished();
};

// ---- Passive nodes ----------------------------------------------------------------------------------

/** Base for passive skill nodes: applies an infinite effect scaled by the node rank (server only). */
UCLASS(Abstract)
class DARKBLOOD_API UDBAbility_PassiveBonus : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_PassiveBonus();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Passive")
	TSubclassOf<class UGameplayEffect> Effect;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Passive")
	float MagnitudePerRank = 1.f;

private:
	FActiveGameplayEffectHandle Applied;
};

/** Magier "Manafluss": +mana regeneration per rank. */
UCLASS()
class DARKBLOOD_API UDBAbility_ManaFlow : public UDBAbility_PassiveBonus
{
	GENERATED_BODY()

public:
	UDBAbility_ManaFlow();
};

/** Moench "Eisenkoerper": +max poise per rank. */
UCLASS()
class DARKBLOOD_API UDBAbility_IronBody : public UDBAbility_PassiveBonus
{
	GENERATED_BODY()

public:
	UDBAbility_IronBody();
};
