#include "TestFramework.h"

#include "DarkBloodRules/Dialogue.h"

using namespace DarkBlood::Rules;

namespace
{
	FDialogueCondition Flag(const char* Id, bool bHas = true)
	{
		FDialogueCondition Condition;
		Condition.Kind = bHas ? EDialogueConditionKind::HasStoryFlag : EDialogueConditionKind::LacksStoryFlag;
		Condition.Id = Id;
		return Condition;
	}

	FDialogueCondition Quest(const char* Id, EQuestStatus Status, bool bIs = true)
	{
		FDialogueCondition Condition;
		Condition.Kind = bIs ? EDialogueConditionKind::QuestStatusIs : EDialogueConditionKind::QuestStatusIsNot;
		Condition.Id = Id;
		Condition.Status = Status;
		return Condition;
	}

	FDialogueEffect Effect(EDialogueEffectKind Kind, const char* Id)
	{
		FDialogueEffect Result;
		Result.Kind = Kind;
		Result.Id = Id;
		return Result;
	}

	// King: greets, gives the quest, accepts the turn-in, then small talk.
	FDialogueDefinition MakeKingDialogue()
	{
		FDialogueDefinition D;
		D.Id = "King";
		D.Entries = {
			{"TurnIn", {Quest("MQ01", EQuestStatus::ReadyToTurnIn)}},
			{"Waiting", {Quest("MQ01", EQuestStatus::Active)}},
			{"Done", {Quest("MQ01", EQuestStatus::Completed)}},
			{"Greeting", {}},
		};

		FDialogueNode Greeting{"Greeting", "King", "Wanderer, the demons gather.", {}, {Effect(EDialogueEffectKind::ReportTalk, "NPC_King")}};
		Greeting.Choices.push_back({"I will fight.", "Accept", {}, {Effect(EDialogueEffectKind::StartQuest, "MQ01")}});
		Greeting.Choices.push_back({"Who are you?", "Lore", {}, {}});
		Greeting.Choices.push_back({"[Secret] I know about the Dark Blood.", "", {Flag("Story.SecretKnown")}, {}});
		Greeting.Choices.push_back({"Farewell.", "", {}, {}});

		D.Nodes.push_back(Greeting);
		D.Nodes.push_back({"Accept", "King", "Train with the dummies, then return.", {}, {}});
		D.Nodes.push_back({"Lore", "King", "The last king of a dying realm.", {{"Back", "Greeting", {}, {}}}, {}});
		D.Nodes.push_back({"Waiting", "King", "The dummies await.", {}, {}});
		FDialogueNode TurnIn{"TurnIn", "King", "Well fought.", {}, {}};
		TurnIn.Choices.push_back({"Report", "", {}, {Effect(EDialogueEffectKind::TurnInQuest, "MQ01"), Effect(EDialogueEffectKind::SetStoryFlag, "Story.Sworn")}});
		D.Nodes.push_back(TurnIn);
		D.Nodes.push_back({"Done", "King", "Go with honour.", {}, {}});
		return D;
	}
}

DB_TEST(Dialogue_ValidationCatchesBrokenGraphs)
{
	FDialogueDefinition King = MakeKingDialogue();
	DB_CHECK_EQ(ValidateDialogue(King), EDialogueValidation::Ok);

	FDialogueDefinition Broken = King;
	Broken.Nodes[2].Choices[0].NextNodeId = "Nowhere";
	DB_CHECK_EQ(ValidateDialogue(Broken), EDialogueValidation::UnknownNextNode);

	Broken = King;
	Broken.Entries.push_back({"Missing", {}});
	DB_CHECK_EQ(ValidateDialogue(Broken), EDialogueValidation::UnknownEntryNode);

	Broken = King;
	Broken.Nodes.push_back(Broken.Nodes[0]);
	DB_CHECK_EQ(ValidateDialogue(Broken), EDialogueValidation::DuplicateNodeId);

	Broken = King;
	Broken.Nodes[0].Choices[0].Effects[0].Id.clear();
	DB_CHECK_EQ(ValidateDialogue(Broken), EDialogueValidation::EmptyEffectId);

	Broken = King;
	Broken.Entries.clear();
	DB_CHECK_EQ(ValidateDialogue(Broken), EDialogueValidation::NoEntries);
	DB_CHECK_EQ(std::string(ToString(EDialogueValidation::UnknownNextNode)), std::string("UnknownNextNode"));
}

DB_TEST(Dialogue_EntryDependsOnQuestProgress)
{
	const FDialogueDefinition King = MakeKingDialogue();
	FDialogueContext Context;

	FDialogueStep Step = BeginDialogue(King, Context);
	DB_CHECK(Step.bValid);
	DB_CHECK_EQ(Step.NodeId, std::string("Greeting"));
	DB_CHECK(!Step.bEnded);
	DB_CHECK_EQ(Step.Effects.size(), size_t(1)); // talking to the king counts as a Talk event
	DB_CHECK_EQ(Step.Effects[0].Kind, EDialogueEffectKind::ReportTalk);

	Context.QuestStatuses["MQ01"] = EQuestStatus::Active;
	Step = BeginDialogue(King, Context);
	DB_CHECK_EQ(Step.NodeId, std::string("Waiting"));
	DB_CHECK(Step.bEnded); // no choices -> single line

	Context.QuestStatuses["MQ01"] = EQuestStatus::ReadyToTurnIn;
	DB_CHECK_EQ(BeginDialogue(King, Context).NodeId, std::string("TurnIn"));

	Context.QuestStatuses["MQ01"] = EQuestStatus::Completed;
	DB_CHECK_EQ(BeginDialogue(King, Context).NodeId, std::string("Done"));

	FDialogueDefinition NoFallback = King;
	NoFallback.Entries.pop_back();
	Context.QuestStatuses.clear();
	DB_CHECK(!BeginDialogue(NoFallback, Context).bValid);
}

DB_TEST(Dialogue_ChoicesAreFilteredAndValidatedServerSide)
{
	const FDialogueDefinition King = MakeKingDialogue();
	FDialogueContext Context;
	const FDialogueNode* Greeting = FindDialogueNode(King, "Greeting");
	DB_CHECK(Greeting != nullptr);

	// The secret option is hidden without the story flag ...
	DB_CHECK_EQ(GetAvailableChoices(*Greeting, Context), (std::vector<int32>{0, 1, 3}));
	DB_CHECK(!ChooseDialogueOption(King, "Greeting", 2, Context).bValid); // ... and cannot be forced by a client
	DB_CHECK(!ChooseDialogueOption(King, "Greeting", 7, Context).bValid);
	DB_CHECK(!ChooseDialogueOption(King, "Greeting", -1, Context).bValid);
	DB_CHECK(!ChooseDialogueOption(King, "NoSuchNode", 0, Context).bValid);

	Context.StoryFlags.insert("Story.SecretKnown");
	DB_CHECK_EQ(GetAvailableChoices(*Greeting, Context).size(), size_t(4));
	const FDialogueStep Secret = ChooseDialogueOption(King, "Greeting", 2, Context);
	DB_CHECK(Secret.bValid && Secret.bEnded);

	// Accept: starts the quest and moves to the confirmation line, which ends the talk.
	const FDialogueStep Accept = ChooseDialogueOption(King, "Greeting", 0, Context);
	DB_CHECK(Accept.bValid);
	DB_CHECK_EQ(Accept.NodeId, std::string("Accept"));
	DB_CHECK(Accept.bEnded);
	DB_CHECK_EQ(Accept.Effects.size(), size_t(1));
	DB_CHECK_EQ(Accept.Effects[0].Kind, EDialogueEffectKind::StartQuest);
	DB_CHECK_EQ(Accept.Effects[0].Id, std::string("MQ01"));

	// Lore loops back to the greeting, whose OnEnter effects apply again.
	const FDialogueStep Lore = ChooseDialogueOption(King, "Greeting", 1, Context);
	DB_CHECK_EQ(Lore.NodeId, std::string("Lore"));
	const FDialogueStep Back = ChooseDialogueOption(King, "Lore", 0, Context);
	DB_CHECK_EQ(Back.NodeId, std::string("Greeting"));
	DB_CHECK_EQ(Back.Effects.size(), size_t(1));

	// Turn-in: effects in order.
	Context.QuestStatuses["MQ01"] = EQuestStatus::ReadyToTurnIn;
	const FDialogueStep TurnIn = ChooseDialogueOption(King, "TurnIn", 0, Context);
	DB_CHECK(TurnIn.bValid && TurnIn.bEnded);
	DB_CHECK_EQ(TurnIn.Effects.size(), size_t(2));
	DB_CHECK_EQ(TurnIn.Effects[0].Kind, EDialogueEffectKind::TurnInQuest);
	DB_CHECK_EQ(TurnIn.Effects[1].Kind, EDialogueEffectKind::SetStoryFlag);

	// Status-is-not conditions.
	DB_CHECK(AreDialogueConditionsMet({Quest("MQ01", EQuestStatus::Completed, false)}, Context));
	DB_CHECK(!AreDialogueConditionsMet({Quest("MQ01", EQuestStatus::ReadyToTurnIn, false)}, Context));
	DB_CHECK(AreDialogueConditionsMet({Flag("Story.Unknown", false)}, Context));
}
