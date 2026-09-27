// Runs NPC conversations for one player (lives on the player controller). The server owns the conversation
// state and applies effects (quests, story flags); the owning client receives a view to display and sends
// back the index of the option it picked.
#pragma once

#include "Components/ActorComponent.h"

#include "DarkBloodRules/Dialogue.h"

#include "DBDialogueComponent.generated.h"

class UDBDialogueDefinition;

USTRUCT(BlueprintType)
struct FDBDialogueView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName DialogueId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText Speaker;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText Text;

	/** Offered options, in display order. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TArray<FText> Choices;

	/** No options: the next confirmation closes the conversation. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bEnds = false;
};

DECLARE_MULTICAST_DELEGATE(FDBOnDialogueChanged);

UCLASS(ClassGroup = (DarkBlood))
class DARKBLOOD_API UDBDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBDialogueComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Server: opens DialogueId with Npc for the owning player. */
	void StartDialogue(AActor* Npc, FName NpcId, FName DialogueId);

	/** Local: pick option DisplayIndex of the current view (or close it when it ends). */
	void Choose(int32 DisplayIndex);

	/** Local: leave the conversation. */
	void Close();

	bool IsDialogueOpen() const { return bOpen; }
	const FDBDialogueView& GetView() const { return View; }

	/** Local: fires when a view arrives or the conversation closes (UI). */
	FDBOnDialogueChanged OnDialogueChanged;

protected:
	UFUNCTION(Server, Reliable)
	void ServerChoose(int32 DisplayIndex);

	UFUNCTION(Server, Reliable)
	void ServerClose();

	UFUNCTION(Client, Reliable)
	void ClientShow(const FDBDialogueView& NewView);

	UFUNCTION(Client, Reliable)
	void ClientClose();

private:
	DarkBlood::Rules::FDialogueContext BuildContext() const;
	void ApplyEffects(const std::vector<DarkBlood::Rules::FDialogueEffect>& Effects);
	void SendCurrentNode();
	void EndServerDialogue();
	const UDBDialogueDefinition* GetDefinition() const;

	// Server state
	TWeakObjectPtr<AActor> ActiveNpc;
	FName ActiveDialogueId;
	FName ActiveNodeId;

	// Local (owning client) state
	bool bOpen = false;
	FDBDialogueView View;
};
