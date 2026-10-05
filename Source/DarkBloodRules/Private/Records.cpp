#include "DarkBloodRules/Records.h"

#include "DarkBloodRules/Archive.h"

#include <cstring>
#include <unordered_set>

namespace DarkBlood::Rules
{
	namespace
	{
		constexpr char CharacterMagic[4] = {'D', 'B', 'C', 'H'};
		constexpr char WorldMagic[4] = {'D', 'B', 'W', 'D'};
		constexpr size_t HeaderSize = 16;

		template <typename TEnum>
		bool ReadEnum(FBinaryReader& Reader, TEnum& Out, uint8 Count)
		{
			uint8 Raw = 0;
			if (!Reader.ReadU8(Raw) || Raw >= Count)
			{
				return false;
			}
			Out = static_cast<TEnum>(Raw);
			return true;
		}

		template <typename TEnum>
		void WriteEnum(FBinaryWriter& Writer, TEnum Value)
		{
			Writer.WriteU8(static_cast<uint8>(Value));
		}

		// ---- Container ------------------------------------------------------------------------------

		std::vector<uint8> WrapPayload(const char (&Magic)[4], uint32 Version, const std::vector<uint8>& Payload)
		{
			FBinaryWriter Header;
			for (const char C : Magic)
			{
				Header.WriteU8(static_cast<uint8>(C));
			}
			Header.WriteU32(Version);
			Header.WriteU32(static_cast<uint32>(Payload.size()));
			Header.WriteU32(Crc32(Payload.data(), Payload.size()));

			std::vector<uint8> Result = Header.TakeData();
			Result.insert(Result.end(), Payload.begin(), Payload.end());
			return Result;
		}

		ELoadResult UnwrapPayload(const char (&Magic)[4], uint32 CurrentVersion, const uint8* Data, size_t Size,
			uint32& OutVersion, const uint8*& OutPayload, size_t& OutPayloadSize)
		{
			if (!Data || Size < HeaderSize || std::memcmp(Data, Magic, 4) != 0)
			{
				return ELoadResult::BadMagic;
			}
			FBinaryReader Reader(Data + 4, Size - 4);
			uint32 PayloadSize = 0;
			uint32 StoredCrc = 0;
			Reader.ReadU32(OutVersion);
			Reader.ReadU32(PayloadSize);
			Reader.ReadU32(StoredCrc);
			if (Reader.HasError())
			{
				return ELoadResult::Corrupt;
			}
			if (OutVersion == 0 || OutVersion > CurrentVersion)
			{
				return ELoadResult::UnsupportedVersion;
			}
			if (PayloadSize != Size - HeaderSize)
			{
				return ELoadResult::Corrupt;
			}
			OutPayload = Data + HeaderSize;
			OutPayloadSize = PayloadSize;
			if (Crc32(OutPayload, OutPayloadSize) != StoredCrc)
			{
				return ELoadResult::ChecksumMismatch;
			}
			return ELoadResult::Ok;
		}

		// ---- Shared pieces --------------------------------------------------------------------------

		void WriteStack(FBinaryWriter& W, const FItemStack& Stack)
		{
			W.WriteString(Stack.ItemId);
			W.WriteI32(Stack.Count);
			W.WriteU64(Stack.InstanceId);
			W.WriteI32(Stack.Durability);
		}

		bool ReadStack(FBinaryReader& R, FItemStack& Stack)
		{
			return R.ReadString(Stack.ItemId) && R.ReadI32(Stack.Count) && R.ReadU64(Stack.InstanceId) && R.ReadI32(Stack.Durability);
		}

		void WriteStringSet(FBinaryWriter& W, const std::set<std::string, std::less<>>& Set)
		{
			W.WriteU32(static_cast<uint32>(Set.size()));
			for (const std::string& Value : Set)
			{
				W.WriteString(Value);
			}
		}

		bool ReadStringSet(FBinaryReader& R, std::set<std::string, std::less<>>& Set)
		{
			uint32 Count = 0;
			if (!R.ReadCount(Count))
			{
				return false;
			}
			Set.clear();
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				std::string Value;
				if (!R.ReadString(Value))
				{
					return false;
				}
				Set.insert(std::move(Value));
			}
			return true;
		}

		void WriteQuestLog(FBinaryWriter& W, const FQuestLog& Log)
		{
			W.WriteU32(static_cast<uint32>(Log.GetAll().size()));
			for (const FQuestProgress& Entry : Log.GetAll())
			{
				W.WriteString(Entry.QuestId);
				WriteEnum(W, Entry.Status);
				W.WriteU32(static_cast<uint32>(Entry.ObjectiveCounts.size()));
				for (const int32 Count : Entry.ObjectiveCounts)
				{
					W.WriteI32(Count);
				}
			}
		}

		bool ReadQuestLog(FBinaryReader& R, FQuestLog& Log)
		{
			uint32 Count = 0;
			if (!R.ReadCount(Count))
			{
				return false;
			}
			std::vector<FQuestProgress> Entries(Count);
			for (FQuestProgress& Entry : Entries)
			{
				uint32 Objectives = 0;
				if (!R.ReadString(Entry.QuestId) || !ReadEnum(R, Entry.Status, static_cast<uint8>(EQuestStatus::Failed) + 1) ||
					!R.ReadCount(Objectives))
				{
					return false;
				}
				Entry.ObjectiveCounts.resize(Objectives);
				for (int32& Value : Entry.ObjectiveCounts)
				{
					if (!R.ReadI32(Value))
					{
						return false;
					}
				}
			}
			Log.RestoreRaw(std::move(Entries));
			return true;
		}

		// ---- Character ------------------------------------------------------------------------------

		void WriteAppearance(FBinaryWriter& W, const FCharacterAppearance& A)
		{
			WriteEnum(W, A.BodyType);
			W.WriteI32(A.FacePreset);
			W.WriteI32(A.SkinTone);
			W.WriteI32(A.SkinDetail);
			W.WriteI32(A.HairStyle);
			W.WriteU32(A.HairColor);
			W.WriteI32(A.EyeStyle);
			W.WriteU32(A.EyeColor);
			W.WriteU32(static_cast<uint32>(A.Scars.size()));
			for (const int32 Scar : A.Scars)
			{
				W.WriteI32(Scar);
			}
			W.WriteU32(static_cast<uint32>(A.Morphs.size()));
			for (const auto& [Key, Value] : A.Morphs)
			{
				W.WriteString(Key);
				W.WriteF32(Value);
			}
			W.WriteI32(A.VoicePreset);
		}

		bool ReadAppearance(FBinaryReader& R, FCharacterAppearance& A)
		{
			uint32 Count = 0;
			if (!ReadEnum(R, A.BodyType, 2) || !R.ReadI32(A.FacePreset) || !R.ReadI32(A.SkinTone) || !R.ReadI32(A.SkinDetail) ||
				!R.ReadI32(A.HairStyle) || !R.ReadU32(A.HairColor) || !R.ReadI32(A.EyeStyle) || !R.ReadU32(A.EyeColor) ||
				!R.ReadCount(Count))
			{
				return false;
			}
			A.Scars.resize(Count);
			for (int32& Scar : A.Scars)
			{
				if (!R.ReadI32(Scar))
				{
					return false;
				}
			}
			if (!R.ReadCount(Count))
			{
				return false;
			}
			A.Morphs.clear();
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				std::string Key;
				float Value = 0.f;
				if (!R.ReadString(Key) || !R.ReadF32(Value))
				{
					return false;
				}
				A.Morphs[Key] = Value;
			}
			return R.ReadI32(A.VoicePreset);
		}

		void WriteInventory(FBinaryWriter& W, const FInventory& Inventory)
		{
			W.WriteU32(static_cast<uint32>(Inventory.NumSections()));
			for (int32 SectionIndex = 0; SectionIndex < Inventory.NumSections(); ++SectionIndex)
			{
				const FInventorySection& Section = Inventory.GetSection(SectionIndex);
				WriteStack(W, Section.BagItem);
				W.WriteU32(Section.AcceptedCategories);
				W.WriteU32(static_cast<uint32>(Section.Slots.size()));
				for (const FItemStack& Slot : Section.Slots)
				{
					WriteStack(W, Slot);
				}
			}
			W.WriteU32(static_cast<uint32>(Inventory.GetQuestItems().size()));
			for (const FItemStack& Quest : Inventory.GetQuestItems())
			{
				WriteStack(W, Quest);
			}
		}

		bool ReadInventory(FBinaryReader& R, FInventory& Inventory)
		{
			uint32 SectionCount = 0;
			if (!R.ReadCount(SectionCount))
			{
				return false;
			}
			std::vector<FInventorySection> Sections(SectionCount);
			for (FInventorySection& Section : Sections)
			{
				uint32 Slots = 0;
				if (!ReadStack(R, Section.BagItem) || !R.ReadU32(Section.AcceptedCategories) || !R.ReadCount(Slots))
				{
					return false;
				}
				Section.Slots.resize(Slots);
				for (FItemStack& Slot : Section.Slots)
				{
					if (!ReadStack(R, Slot))
					{
						return false;
					}
				}
			}
			uint32 QuestCount = 0;
			if (!R.ReadCount(QuestCount))
			{
				return false;
			}
			std::vector<FItemStack> QuestItems(QuestCount);
			for (FItemStack& Quest : QuestItems)
			{
				if (!ReadStack(R, Quest))
				{
					return false;
				}
			}
			return Inventory.RestoreRaw(std::move(Sections), std::move(QuestItems));
		}

		void WriteCharacterPayload(FBinaryWriter& W, const FCharacterRecord& C)
		{
			W.WriteString(C.CharacterId);
			W.WriteString(C.Name);
			W.WriteString(C.ClassId);
			WriteAppearance(W, C.Appearance);

			W.WriteI32(C.Progression.Level);
			W.WriteI64(C.Progression.XpIntoLevel);
			W.WriteI64(C.Progression.TotalXp);
			W.WriteI32(C.Progression.UnspentSkillPoints);
			W.WriteI32(C.Progression.TotalSkillPointsEarned);

			W.WriteU32(static_cast<uint32>(C.Skills.Ranks.size()));
			for (const auto& [NodeId, Rank] : C.Skills.Ranks)
			{
				W.WriteString(NodeId);
				W.WriteI32(Rank);
			}

			W.WriteI64(C.Currency);
			WriteInventory(W, C.Inventory);

			W.WriteU32(static_cast<uint32>(FEquipment::NumSlots));
			for (int32 Slot = 0; Slot < FEquipment::NumSlots; ++Slot)
			{
				WriteStack(W, C.Equipment.Get(static_cast<EEquipSlot>(Slot)));
			}

			WriteQuestLog(W, C.PersonalQuests);
			WriteStringSet(W, C.DiscoveredRegions);
			WriteStringSet(W, C.Titles);
			W.WriteString(C.RespawnPointId);
			W.WriteI64(C.PlayTimeSeconds);
			W.WriteU32(static_cast<uint32>(C.PendingDeliveries.size()));
			for (const FItemStack& Stack : C.PendingDeliveries)
			{
				WriteStack(W, Stack);
			}
		}

		bool ReadCharacterPayload(FBinaryReader& R, uint32 /*Version*/, FCharacterRecord& C)
		{
			// Version-specific migration branches go here once CharacterRecordVersion > 1.
			if (!R.ReadString(C.CharacterId) || !R.ReadString(C.Name) || !R.ReadString(C.ClassId) || !ReadAppearance(R, C.Appearance))
			{
				return false;
			}
			if (!R.ReadI32(C.Progression.Level) || !R.ReadI64(C.Progression.XpIntoLevel) || !R.ReadI64(C.Progression.TotalXp) ||
				!R.ReadI32(C.Progression.UnspentSkillPoints) || !R.ReadI32(C.Progression.TotalSkillPointsEarned))
			{
				return false;
			}

			uint32 Count = 0;
			if (!R.ReadCount(Count))
			{
				return false;
			}
			C.Skills.Ranks.clear();
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				std::string NodeId;
				int32 Rank = 0;
				if (!R.ReadString(NodeId) || !R.ReadI32(Rank))
				{
					return false;
				}
				C.Skills.Ranks[NodeId] = Rank;
			}

			if (!R.ReadI64(C.Currency) || !ReadInventory(R, C.Inventory) || !R.ReadCount(Count))
			{
				return false;
			}
			for (uint32 Slot = 0; Slot < Count; ++Slot)
			{
				FItemStack Stack;
				if (!ReadStack(R, Stack))
				{
					return false;
				}
				if (Slot < static_cast<uint32>(FEquipment::NumSlots))
				{
					C.Equipment.GetMutable(static_cast<EEquipSlot>(Slot)) = std::move(Stack);
				}
			}

			if (!ReadQuestLog(R, C.PersonalQuests) || !ReadStringSet(R, C.DiscoveredRegions) || !ReadStringSet(R, C.Titles) ||
				!R.ReadString(C.RespawnPointId) || !R.ReadI64(C.PlayTimeSeconds) || !R.ReadCount(Count))
			{
				return false;
			}
			C.PendingDeliveries.resize(Count);
			for (FItemStack& Stack : C.PendingDeliveries)
			{
				if (!ReadStack(R, Stack))
				{
					return false;
				}
			}
			return true;
		}

		// ---- Settlements ----------------------------------------------------------------------------

		void WriteSettlements(FBinaryWriter& W, const std::vector<FSettlementState>& Settlements)
		{
			W.WriteU32(static_cast<uint32>(Settlements.size()));
			for (const FSettlementState& S : Settlements)
			{
				W.WriteString(S.SettlementId);
				W.WriteString(S.RegionId);
				W.WriteI32(S.Children);
				W.WriteI32(S.Adults);
				W.WriteI32(S.Elders);
				W.WriteI32(S.Guards);
				W.WriteF64(S.Stocks.Food);
				W.WriteF64(S.Stocks.Wood);
				W.WriteF64(S.Stocks.Stone);
				W.WriteF64(S.Stocks.Ore);
				W.WriteF64(S.Stocks.Money);
				W.WriteF32(S.Prosperity);
				W.WriteF32(S.Security);
				W.WriteF32(S.Threat);
				W.WriteU32(static_cast<uint32>(S.Buildings.size()));
				for (const FSettlementBuilding& Building : S.Buildings)
				{
					WriteEnum(W, Building.Type);
					W.WriteF32(Building.Condition);
					W.WriteI32(Building.Level);
				}
				W.WriteU32(static_cast<uint32>(S.Projects.size()));
				for (const FSettlementProjectState& Project : S.Projects)
				{
					WriteEnum(W, Project.Kind);
					WriteEnum(W, Project.Target);
					W.WriteF64(Project.Progress);
					W.WriteF64(Project.WorkNeeded);
				}
				W.WriteBool(S.bStoryProtected);
				W.WriteF64(S.SimulatedHours);
				W.WriteU32(S.Seed);
				W.WriteI32(S.HungryHours);
			}
		}

		bool ReadSettlements(FBinaryReader& R, std::vector<FSettlementState>& Settlements)
		{
			uint32 Count = 0;
			if (!R.ReadCount(Count))
			{
				return false;
			}
			Settlements.assign(Count, FSettlementState());
			for (FSettlementState& S : Settlements)
			{
				uint32 Buildings = 0;
				if (!R.ReadString(S.SettlementId) || !R.ReadString(S.RegionId) || !R.ReadI32(S.Children) || !R.ReadI32(S.Adults) ||
					!R.ReadI32(S.Elders) || !R.ReadI32(S.Guards) || !R.ReadF64(S.Stocks.Food) || !R.ReadF64(S.Stocks.Wood) ||
					!R.ReadF64(S.Stocks.Stone) || !R.ReadF64(S.Stocks.Ore) || !R.ReadF64(S.Stocks.Money) || !R.ReadF32(S.Prosperity) ||
					!R.ReadF32(S.Security) || !R.ReadF32(S.Threat) || !R.ReadCount(Buildings))
				{
					return false;
				}
				if (S.Children < 0 || S.Adults < 0 || S.Elders < 0 || S.Guards < 0 || S.Guards > S.Adults)
				{
					return false;
				}
				S.Buildings.assign(Buildings, FSettlementBuilding());
				for (FSettlementBuilding& Building : S.Buildings)
				{
					if (!ReadEnum(R, Building.Type, static_cast<uint8>(ESettlementBuilding::Count)) || !R.ReadF32(Building.Condition) ||
						!R.ReadI32(Building.Level))
					{
						return false;
					}
				}
				uint32 Projects = 0;
				if (!R.ReadCount(Projects))
				{
					return false;
				}
				S.Projects.assign(Projects, FSettlementProjectState());
				for (FSettlementProjectState& Project : S.Projects)
				{
					if (!ReadEnum(R, Project.Kind, static_cast<uint8>(ESettlementProject::Upgrade) + 1) ||
						!ReadEnum(R, Project.Target, static_cast<uint8>(ESettlementBuilding::Count)) || !R.ReadF64(Project.Progress) ||
						!R.ReadF64(Project.WorkNeeded))
					{
						return false;
					}
				}
				if (!R.ReadBool(S.bStoryProtected) || !R.ReadF64(S.SimulatedHours) || !R.ReadU32(S.Seed) || !R.ReadI32(S.HungryHours))
				{
					return false;
				}
			}
			return true;
		}

		// ---- World ----------------------------------------------------------------------------------

		void WriteWorldPayload(FBinaryWriter& W, const FWorldRecord& Record)
		{
			const FWorldState& World = Record.World;
			W.WriteString(Record.WorldId);
			W.WriteF64(World.Clock.TotalHours);
			WriteStringSet(W, World.StoryFlags);
			WriteStringSet(W, World.DefeatedBosses);
			W.WriteU32(static_cast<uint32>(World.GetRegions().size()));
			for (const FRegionState& Region : World.GetRegions())
			{
				W.WriteString(Region.RegionId);
				WriteEnum(W, Region.Kind);
				WriteEnum(W, Region.Control);
				W.WriteF32(Region.DemonInfluence);
				W.WriteBool(Region.bMidBossDefeated);
				W.WriteBool(Region.bVassalDefeated);
				W.WriteF64(Region.LiberatedAtHours);
				W.WriteI32(Region.VassalCount);
				W.WriteI32(Region.VassalsDefeated);
			}
			WriteQuestLog(W, World.SharedQuests);
			WriteSettlements(W, World.Settlements);
			W.WriteU32(static_cast<uint32>(World.Dungeons.size()));
			for (const FDungeonProgress& Dungeon : World.Dungeons)
			{
				W.WriteString(Dungeon.DungeonId);
				W.WriteI32(Dungeon.TimesCleared);
				W.WriteF64(Dungeon.ClearedAtHours);
			}
		}

		bool ReadWorldPayload(FBinaryReader& R, uint32 Version, FWorldRecord& Record)
		{
			FWorldState& World = Record.World;
			uint32 Count = 0;
			if (!R.ReadString(Record.WorldId) || !R.ReadF64(World.Clock.TotalHours) || !ReadStringSet(R, World.StoryFlags) ||
				!ReadStringSet(R, World.DefeatedBosses) || !R.ReadCount(Count))
			{
				return false;
			}
			std::vector<FRegionState>& Regions = World.GetRegionsMutable();
			Regions.assign(Count, FRegionState());
			for (FRegionState& Region : Regions)
			{
				if (!R.ReadString(Region.RegionId) || !ReadEnum(R, Region.Kind, static_cast<uint8>(ERegionKind::Epilogue) + 1) ||
					!ReadEnum(R, Region.Control, static_cast<uint8>(ERegionControl::Liberated) + 1) || !R.ReadF32(Region.DemonInfluence) ||
					!R.ReadBool(Region.bMidBossDefeated) || !R.ReadBool(Region.bVassalDefeated) || !R.ReadF64(Region.LiberatedAtHours))
				{
					return false;
				}
				if (Version >= 4)
				{
					if (!R.ReadI32(Region.VassalCount) || !R.ReadI32(Region.VassalsDefeated) || Region.VassalCount < 0 ||
						Region.VassalsDefeated < 0 || Region.VassalsDefeated > Region.VassalCount)
					{
						return false;
					}
				}
				else
				{
					Region.VassalCount = 1;
					Region.VassalsDefeated = Region.bVassalDefeated ? 1 : 0;
				}
			}
			if (!ReadQuestLog(R, World.SharedQuests))
			{
				return false;
			}
			if (Version >= 2 && !ReadSettlements(R, World.Settlements))
			{
				return false;
			}
			if (Version >= 3)
			{
				uint32 DungeonCount = 0;
				if (!R.ReadCount(DungeonCount))
				{
					return false;
				}
				World.Dungeons.assign(DungeonCount, FDungeonProgress());
				for (FDungeonProgress& Dungeon : World.Dungeons)
				{
					if (!R.ReadString(Dungeon.DungeonId) || !R.ReadI32(Dungeon.TimesCleared) || !R.ReadF64(Dungeon.ClearedAtHours) || Dungeon.TimesCleared < 0)
					{
						return false;
					}
				}
			}
			return true;
		}
	}

	const char* ToString(ELoadResult Result)
	{
		switch (Result)
		{
		case ELoadResult::Ok: return "Ok";
		case ELoadResult::BadMagic: return "BadMagic";
		case ELoadResult::UnsupportedVersion: return "UnsupportedVersion";
		case ELoadResult::ChecksumMismatch: return "ChecksumMismatch";
		case ELoadResult::Corrupt: return "Corrupt";
		}
		return "Unknown";
	}

	std::vector<uint8> SerializeCharacter(const FCharacterRecord& Record)
	{
		FBinaryWriter Writer;
		WriteCharacterPayload(Writer, Record);
		return WrapPayload(CharacterMagic, CharacterRecordVersion, Writer.GetData());
	}

	ELoadResult DeserializeCharacter(const uint8* Data, size_t Size, FCharacterRecord& OutRecord)
	{
		uint32 Version = 0;
		const uint8* Payload = nullptr;
		size_t PayloadSize = 0;
		const ELoadResult Header = UnwrapPayload(CharacterMagic, CharacterRecordVersion, Data, Size, Version, Payload, PayloadSize);
		if (Header != ELoadResult::Ok)
		{
			return Header;
		}
		FBinaryReader Reader(Payload, PayloadSize);
		FCharacterRecord Record;
		if (!ReadCharacterPayload(Reader, Version, Record) || !Reader.IsAtEnd())
		{
			return ELoadResult::Corrupt;
		}
		OutRecord = std::move(Record);
		return ELoadResult::Ok;
	}

	std::vector<uint8> SerializeWorld(const FWorldRecord& Record)
	{
		FBinaryWriter Writer;
		WriteWorldPayload(Writer, Record);
		return WrapPayload(WorldMagic, WorldRecordVersion, Writer.GetData());
	}

	ELoadResult DeserializeWorld(const uint8* Data, size_t Size, FWorldRecord& OutRecord)
	{
		uint32 Version = 0;
		const uint8* Payload = nullptr;
		size_t PayloadSize = 0;
		const ELoadResult Header = UnwrapPayload(WorldMagic, WorldRecordVersion, Data, Size, Version, Payload, PayloadSize);
		if (Header != ELoadResult::Ok)
		{
			return Header;
		}
		FBinaryReader Reader(Payload, PayloadSize);
		FWorldRecord Record;
		if (!ReadWorldPayload(Reader, Version, Record) || !Reader.IsAtEnd())
		{
			return ELoadResult::Corrupt;
		}
		OutRecord = std::move(Record);
		return ELoadResult::Ok;
	}

	std::vector<std::string> ValidateCharacterRecord(const FCharacterRecord& Record, const FItemCatalog& Catalog,
		const FRecordValidationRules& Rules)
	{
		std::vector<std::string> Issues;
		auto Issue = [&Issues](std::string Text) { Issues.push_back(std::move(Text)); };

		if (Record.CharacterId.empty())
		{
			Issue("missing character id");
		}
		if (NormalizeCharacterName(Record.Name) != Record.Name)
		{
			Issue("name is not normalized");
		}
		const ENameValidation NameResult = ValidateCharacterName(Record.Name, Rules.Names);
		if (NameResult != ENameValidation::Ok)
		{
			Issue(std::string("invalid name: ") + ToString(NameResult));
		}
		if (Record.ClassId.empty())
		{
			Issue("missing class");
		}
		if (!IsProgressionConsistent(Record.Progression, Rules.Progression))
		{
			Issue("inconsistent progression");
		}
		if (Record.Currency < 0 || Record.Currency > Rules.MaxCurrency)
		{
			Issue("currency out of range");
		}
		if (Record.PlayTimeSeconds < 0)
		{
			Issue("negative play time");
		}

		std::unordered_set<uint64> InstanceIds;
		auto CheckStack = [&](const FItemStack& Stack, const char* Where)
		{
			const FItemDefinition* Definition = Catalog.Find(Stack.ItemId);
			if (!Definition)
			{
				Issue(std::string("unknown item '") + Stack.ItemId + "' in " + Where);
				return;
			}
			if (Stack.Count < 1 || (Definition->Category != EItemCategory::Quest && Stack.Count > Definition->MaxStack))
			{
				Issue(std::string("invalid stack size for '") + Stack.ItemId + "' in " + Where);
			}
			if (Stack.InstanceId != 0)
			{
				if (Stack.Count != 1)
				{
					Issue(std::string("unique item '") + Stack.ItemId + "' with count != 1");
				}
				if (!InstanceIds.insert(Stack.InstanceId).second)
				{
					Issue(std::string("duplicated item instance of '") + Stack.ItemId + "'");
				}
			}
			if (Stack.Durability > Definition->MaxDurability || (Definition->MaxDurability > 0 && Stack.Durability < -1))
			{
				Issue(std::string("invalid durability for '") + Stack.ItemId + "'");
			}
		};

		const FInventory& Inventory = Record.Inventory;
		for (int32 SectionIndex = 0; SectionIndex < Inventory.NumSections(); ++SectionIndex)
		{
			const FInventorySection& Section = Inventory.GetSection(SectionIndex);
			if (!Section.bIsBasePouch && !Section.BagItem.IsEmpty())
			{
				CheckStack(Section.BagItem, "bag slot");
				const FItemDefinition* Bag = Catalog.Find(Section.BagItem.ItemId);
				if (Bag && (!Bag->bIsBag || Bag->Bag.Kind != Section.Kind || Bag->Bag.Capacity != Section.Capacity()))
				{
					Issue("bag slot does not match its bag definition");
				}
			}
			for (const FItemStack& Slot : Section.Slots)
			{
				if (Slot.IsEmpty())
				{
					continue;
				}
				CheckStack(Slot, "inventory");
				const FItemDefinition* Definition = Catalog.Find(Slot.ItemId);
				if (Definition && !Section.Accepts(Definition->Category))
				{
					Issue(std::string("item '") + Slot.ItemId + "' stored in a bag that does not accept it");
				}
			}
		}
		for (const FItemStack& Quest : Inventory.GetQuestItems())
		{
			CheckStack(Quest, "quest items");
		}
		for (const FItemStack& Pending : Record.PendingDeliveries)
		{
			CheckStack(Pending, "pending deliveries");
		}

		const FEquipContext Context{Record.Progression.Level, Record.ClassId};
		for (int32 SlotIndex = 1; SlotIndex < FEquipment::NumSlots; ++SlotIndex)
		{
			const EEquipSlot Slot = static_cast<EEquipSlot>(SlotIndex);
			const FItemStack& Stack = Record.Equipment.Get(Slot);
			if (Stack.IsEmpty())
			{
				continue;
			}
			CheckStack(Stack, "equipment");
			if (const FItemDefinition* Definition = Catalog.Find(Stack.ItemId))
			{
				const EInventoryResult Result = CanEquip(*Definition, Slot, Context);
				if (Result != EInventoryResult::Ok)
				{
					Issue(std::string("equipped item '") + Stack.ItemId + "' not allowed: " + ToString(Result));
				}
			}
		}
		if (!Record.Equipment.Get(EEquipSlot::None).IsEmpty())
		{
			Issue("item stored in the 'None' equipment slot");
		}

		if (Rules.SkillTree)
		{
			for (const auto& [NodeId, Rank] : Record.Skills.Ranks)
			{
				const FSkillNodeDefinition* Node = Rules.SkillTree->Find(NodeId);
				if (!Node || Rank < 0 || Rank > Node->MaxRank)
				{
					Issue("invalid skill node or rank: " + NodeId);
				}
			}
			const int32 Spent = CountSpentPoints(*Rules.SkillTree, Record.Skills);
			if (Spent + Record.Progression.UnspentSkillPoints > Record.Progression.TotalSkillPointsEarned)
			{
				Issue("more skill points spent than earned");
			}
		}
		return Issues;
	}

	FCharacterRecord MakeNewCharacter(std::string CharacterId, std::string Name, std::string ClassId, const FCharacterAppearance& Appearance)
	{
		FCharacterRecord Record;
		Record.CharacterId = std::move(CharacterId);
		Record.Name = NormalizeCharacterName(Name);
		Record.ClassId = std::move(ClassId);
		Record.Appearance = Appearance;
		return Record;
	}
}
