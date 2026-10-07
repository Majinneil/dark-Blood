// DARK BLOOD - Rules Core: persistent records (character + world) and their versioned serialization.
//
// Container format:  magic[4] | u32 version | u32 payload size | u32 crc32(payload) | payload
// The checksum detects corruption, it is NOT an anti-cheat measure. Records uploaded by
// clients are always re-validated by the server (ValidateCharacterRecord).
#pragma once

#include "DarkBloodRules/CharacterName.h"
#include "DarkBloodRules/Equipment.h"
#include "DarkBloodRules/SkillTree.h"
#include "DarkBloodRules/WorldState.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace DarkBlood::Rules
{
	constexpr uint32 CharacterRecordVersion = 1;
	/** 2: settlement simulation (version 1 worlds load without settlements; the game creates them).
	 *  3: dungeon progress (older worlds start with no dungeon cleared).
	 *  4: vassals per region (16 vassals; older worlds: one per region, defeated if the region was liberated). */
	constexpr uint32 WorldRecordVersion = 5;

	enum class EBodyType : uint8
	{
		TypeA, // masculine base body
		TypeB, // feminine base body
	};

	struct FCharacterAppearance
	{
		EBodyType BodyType = EBodyType::TypeA;
		int32 FacePreset = 0;
		int32 SkinTone = 0;
		int32 SkinDetail = 0;
		int32 HairStyle = 0;
		uint32 HairColor = 0x1A1A1AFFu; // RGBA
		int32 EyeStyle = 0;
		uint32 EyeColor = 0x3B2A1EFFu; // RGBA
		std::vector<int32> Scars;
		/** Named morph targets / face and body sliders in [-1, 1]. */
		std::map<std::string, float, std::less<>> Morphs;
		int32 VoicePreset = 0;
	};

	struct FCharacterRecord
	{
		/** Stable unique id (GUID string), independent of the account and of the name. */
		std::string CharacterId;
		std::string Name;
		std::string ClassId;
		FCharacterAppearance Appearance;
		FProgressionState Progression;
		FSkillTreeState Skills;
		int64 Currency = 0; // "Mon"
		FInventory Inventory;
		FEquipment Equipment;
		FQuestLog PersonalQuests;
		std::set<std::string, std::less<>> DiscoveredRegions;
		std::set<std::string, std::less<>> Titles;
		std::string RespawnPointId;
		int64 PlayTimeSeconds = 0;
		/** Rewards that did not fit into the inventory; delivered as soon as space is free (never discarded). */
		std::vector<FItemStack> PendingDeliveries;
	};

	struct FWorldRecord
	{
		std::string WorldId;
		FWorldState World;
	};

	enum class ELoadResult : uint8
	{
		Ok,
		BadMagic,
		UnsupportedVersion,
		ChecksumMismatch,
		Corrupt,
	};

	DARKBLOODRULES_API const char* ToString(ELoadResult Result);

	DARKBLOODRULES_API std::vector<uint8> SerializeCharacter(const FCharacterRecord& Record);
	DARKBLOODRULES_API ELoadResult DeserializeCharacter(const uint8* Data, size_t Size, FCharacterRecord& OutRecord);

	DARKBLOODRULES_API std::vector<uint8> SerializeWorld(const FWorldRecord& Record);
	DARKBLOODRULES_API ELoadResult DeserializeWorld(const uint8* Data, size_t Size, FWorldRecord& OutRecord);

	struct FRecordValidationRules
	{
		FNameRules Names;
		FProgressionRules Progression;
		int64 MaxCurrency = 999'999'999;
		/** Optional: when set, spent skill points are checked against earned points. */
		const FSkillTreeDefinition* SkillTree = nullptr;
	};

	/** Returns human-readable issues; empty means the record is acceptable. */
	DARKBLOODRULES_API std::vector<std::string> ValidateCharacterRecord(const FCharacterRecord& Record, const FItemCatalog& Catalog,
		const FRecordValidationRules& Rules = FRecordValidationRules());

	/** Creates a fresh level-1 character with an empty inventory. */
	DARKBLOODRULES_API FCharacterRecord MakeNewCharacter(std::string CharacterId, std::string Name, std::string ClassId,
		const FCharacterAppearance& Appearance);
}
