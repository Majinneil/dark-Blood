// Talking NPC (quest givers, merchants later). Neutral: never a combat target.
#pragma once

#include "Character/DBCharacterBase.h"
#include "Interaction/DBInteractable.h"

#include "DBNpcCharacter.generated.h"

class UTextRenderComponent;

UCLASS()
class DARKBLOOD_API ADBNpcCharacter : public ADBCharacterBase, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBNpcCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetCombatDisplayName() const override;

	// IDBInteractable
	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override;
	virtual void Interact(APlayerController* User) override;

	FName GetNpcId() const { return NpcId; }

	/** Development/tests: configure a spawned NPC before it is used. */
	void Setup(FName InNpcId, const FText& InDisplayName, FName InDialogueId);

protected:
	virtual void BeginPlay() override;

	/** Id used by Talk quest objectives, e.g. "NPC_King". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|NPC")
	FName NpcId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|NPC")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|NPC")
	FName DialogueId;

	/** DEVELOPMENT nameplate. */
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Visuals")
	TObjectPtr<UTextRenderComponent> Nameplate;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Identity)
	FText ReplicatedName;

	/** CV_<NpcId> when such a profile exists, else CV_NPC_Default. */
	UPROPERTY(ReplicatedUsing = OnRep_Identity)
	FName VisualProfileId;

	void UpdateLookAt();
	float LookAtRefreshSeconds = 0.f;

	UFUNCTION()
	void OnRep_Identity();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
