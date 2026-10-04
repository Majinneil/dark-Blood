#include "Dialogue/DBDialogueComponent.h"

#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBDialogueDefinition.h"
#include "Data/DBGameDataSubsystem.h"
#include "Engine/World.h"
#include "Framework/DBGameState.h"
#include "GameFramework/PlayerController.h"
#include "Player/DBPlayerState.h"
#include "Quest/DBQuestComponent.h"
#include "Quest/DBQuestSubsystem.h"
#include "World/DBWorldStateComponent.h"

namespace R = DarkBlood::Rules;

namespace
{
	/** Conversations end when the player walks this far away from the NPC. */
	constexpr float MaxDialogueDistance = 700.f;
}

UDBDialogueComponent::UDBDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	SetIsReplicatedByDefault(true);
}

const UDBDialogueDefinition* UDBDialogueComponent::GetDefinition() const
{
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	return Data ? Data->FindDialogue(ActiveDialogueId) : nullptr;
}

R::FDialogueContext UDBDialogueComponent::BuildContext() const
{
	R::FDialogueContext Context;
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	if (const UDBWorldStateComponent* WorldState = GameState ? GameState->GetWorldState() : nullptr)
	{
		Context.StoryFlags = WorldState->GetRulesState().StoryFlags;
	}
	auto AddLog = [&Context](const UDBQuestComponent* Log)
	{
		if (Log)
		{
			for (const R::FQuestProgress& Progress : Log->GetLog().GetAll())
			{
				Context.QuestStatuses[Progress.QuestId] = Progress.Status;
			}
		}
	};
	AddLog(GameState ? GameState->GetSharedQuests() : nullptr);
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const ADBPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ADBPlayerState>() : nullptr;
	AddLog(PlayerState ? PlayerState->GetPersonalQuests() : nullptr);
	return Context;
}

void UDBDialogueComponent::StartDialogue(AActor* Npc, FName NpcId, FName DialogueId)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	ActiveNpc = Npc;
	ActiveDialogueId = DialogueId;
	const UDBDialogueDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		UE_LOG(LogDBQuest, Warning, TEXT("NPC %s has no dialogue '%s'"), *NpcId.ToString(), *DialogueId.ToString());
		EndServerDialogue();
		return;
	}
	const R::FDialogueStep Step = R::BeginDialogue(Definition->ToRules(), BuildContext());
	if (!Step.bValid)
	{
		EndServerDialogue();
		return;
	}
	ActiveNodeId = DBBridge::ToFName(Step.NodeId);
	ApplyEffects(Step.Effects);
	SendCurrentNode();
}

void UDBDialogueComponent::ServerChoose_Implementation(int32 DisplayIndex)
{
	const UDBDialogueDefinition* Definition = GetDefinition();
	if (!Definition || ActiveNodeId.IsNone())
	{
		EndServerDialogue();
		return;
	}
	const R::FDialogueDefinition Rules = Definition->ToRules();
	const R::FDialogueContext Context = BuildContext();
	const R::FDialogueNode* Node = R::FindDialogueNode(Rules, DBBridge::ToStd(ActiveNodeId));
	const std::vector<int32> Available = Node ? R::GetAvailableChoices(*Node, Context) : std::vector<int32>();
	if (Available.empty())
	{
		EndServerDialogue(); // last line confirmed
		return;
	}
	if (DisplayIndex < 0 || DisplayIndex >= static_cast<int32>(Available.size()))
	{
		UE_LOG(LogDBQuest, Warning, TEXT("Dialogue %s: refused option %d"), *ActiveDialogueId.ToString(), DisplayIndex);
		return;
	}
	const R::FDialogueStep Step = R::ChooseDialogueOption(Rules, DBBridge::ToStd(ActiveNodeId), Available[static_cast<size_t>(DisplayIndex)], Context);
	if (!Step.bValid)
	{
		return;
	}
	ApplyEffects(Step.Effects);
	if (Step.NodeId.empty())
	{
		EndServerDialogue();
		return;
	}
	ActiveNodeId = DBBridge::ToFName(Step.NodeId);
	SendCurrentNode();
}

void UDBDialogueComponent::ServerClose_Implementation()
{
	EndServerDialogue();
}

void UDBDialogueComponent::ApplyEffects(const std::vector<R::FDialogueEffect>& Effects)
{
	UDBQuestSubsystem* Quests = UDBQuestSubsystem::Get(this);
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	APlayerState* PlayerState = Controller ? Controller->PlayerState : nullptr;
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	UDBWorldStateComponent* WorldState = GameState ? GameState->GetWorldState() : nullptr;

	for (const R::FDialogueEffect& Effect : Effects)
	{
		const FName Id = DBBridge::ToFName(Effect.Id);
		switch (Effect.Kind)
		{
		case R::EDialogueEffectKind::SetStoryFlag:
			if (WorldState)
			{
				WorldState->SetStoryFlag(Id);
			}
			break;
		case R::EDialogueEffectKind::StartQuest:
			if (Quests)
			{
				Quests->StartQuest(Id, PlayerState);
			}
			break;
		case R::EDialogueEffectKind::TurnInQuest:
			if (Quests)
			{
				Quests->TurnInQuest(Id, PlayerState);
			}
			break;
		case R::EDialogueEffectKind::ReportTalk:
			if (Quests)
			{
				Quests->ReportEvent(EDBObjectiveKind::Talk, Id, 1, PlayerState);
			}
			break;
		}
	}
}

void UDBDialogueComponent::SendCurrentNode()
{
	const UDBDialogueDefinition* Definition = GetDefinition();
	const FDBDialogueNode* Node = Definition ? Definition->FindNode(ActiveNodeId) : nullptr;
	if (!Node)
	{
		EndServerDialogue();
		return;
	}
	// Offered options are evaluated after the node's effects ran (a quest may just have started).
	const R::FDialogueDefinition Rules = Definition->ToRules();
	const R::FDialogueNode* RulesNode = R::FindDialogueNode(Rules, DBBridge::ToStd(ActiveNodeId));
	const std::vector<int32> Available = RulesNode ? R::GetAvailableChoices(*RulesNode, BuildContext()) : std::vector<int32>();

	FDBDialogueView NewView;
	NewView.DialogueId = ActiveDialogueId;
	NewView.NodeId = ActiveNodeId;
	NewView.Speaker = Node->Speaker;
	NewView.Text = Node->Text;
	for (const int32 Index : Available)
	{
		NewView.Choices.Add(Node->Choices[Index].Text);
	}
	NewView.bEnds = NewView.Choices.IsEmpty();
	UE_LOG(LogDBQuest, Log, TEXT("Dialogue %s -> %s (%d options)"), *ActiveDialogueId.ToString(), *ActiveNodeId.ToString(), NewView.Choices.Num());
	ClientShow(NewView);
}

void UDBDialogueComponent::EndServerDialogue()
{
	const bool bWasActive = !ActiveDialogueId.IsNone();
	ActiveNpc.Reset();
	ActiveDialogueId = NAME_None;
	ActiveNodeId = NAME_None;
	if (bWasActive)
	{
		ClientClose();
	}
}

void UDBDialogueComponent::ClientShow_Implementation(const FDBDialogueView& NewView)
{
	View = NewView;
	bOpen = true;
	OnDialogueChanged.Broadcast();
}

void UDBDialogueComponent::ClientClose_Implementation()
{
	bOpen = false;
	View = FDBDialogueView();
	OnDialogueChanged.Broadcast();
}

void UDBDialogueComponent::Choose(int32 DisplayIndex)
{
	if (bOpen)
	{
		ServerChoose(View.bEnds ? 0 : DisplayIndex);
	}
}

void UDBDialogueComponent::Close()
{
	if (bOpen)
	{
		ServerClose();
	}
}

void UDBDialogueComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority() || ActiveDialogueId.IsNone())
	{
		return;
	}
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const AActor* Npc = ActiveNpc.Get();
	if (!Pawn || !Npc || FVector::Dist2D(Pawn->GetActorLocation(), Npc->GetActorLocation()) > MaxDialogueDistance)
	{
		EndServerDialogue();
	}
}
