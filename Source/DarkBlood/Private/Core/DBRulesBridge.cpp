#include "Core/DBRulesBridge.h"

#include "Misc/Guid.h"

#include "DarkBloodRules/Stats.h"

namespace R = DarkBlood::Rules;

// Keep Blueprint enums and rules enums in lockstep.
static_assert(static_cast<uint8>(EDBItemCategory::Misc) + 1 == static_cast<uint8>(R::EItemCategory::Count));
static_assert(static_cast<uint8>(EDBEquipSlot::Accessory2) + 1 == static_cast<uint8>(R::EEquipSlot::Count));
static_assert(static_cast<uint8>(EDBBagKind::Loot) + 1 == static_cast<uint8>(R::EBagKind::Count));
static_assert(static_cast<uint8>(EDBItemRarity::Demonic) == static_cast<uint8>(R::EItemRarity::Demonic));
static_assert(static_cast<uint8>(EDBDangerTier::Extreme) == static_cast<uint8>(R::EDangerTier::Extreme));
static_assert(static_cast<uint8>(EDBObjectiveKind::Custom) == static_cast<uint8>(R::EObjectiveKind::Custom));
static_assert(static_cast<uint8>(EDBQuestCategory::Settlement) == static_cast<uint8>(R::EQuestCategory::Settlement));
static_assert(static_cast<uint8>(EDBQuestStatus::Failed) == static_cast<uint8>(R::EQuestStatus::Failed));
static_assert(static_cast<uint8>(EDBRegionControl::Liberated) == static_cast<uint8>(R::ERegionControl::Liberated));
static_assert(static_cast<uint8>(EDBRegionKind::Epilogue) == static_cast<uint8>(R::ERegionKind::Epilogue));
static_assert(static_cast<uint8>(EDBBodyType::TypeB) == static_cast<uint8>(R::EBodyType::TypeB));

namespace DBBridge
{
	namespace
	{
		uint32 PackColor(const FColor& Color)
		{
			return (uint32(Color.R) << 24) | (uint32(Color.G) << 16) | (uint32(Color.B) << 8) | uint32(Color.A);
		}

		FColor UnpackColor(uint32 Packed)
		{
			return FColor(uint8(Packed >> 24), uint8(Packed >> 16), uint8(Packed >> 8), uint8(Packed));
		}
	}

	std::string ToStd(const FString& In)
	{
		const FTCHARToUTF8 Converter(*In);
		return std::string(Converter.Get(), Converter.Length());
	}

	std::string ToStd(FName In)
	{
		return In.IsNone() ? std::string() : ToStd(In.ToString());
	}

	FString ToFString(const std::string& In)
	{
		const FUTF8ToTCHAR Converter(In.data(), static_cast<int32>(In.size()));
		return FString(Converter.Length(), Converter.Get());
	}

	FName ToFName(const std::string& In)
	{
		return In.empty() ? NAME_None : FName(*ToFString(In));
	}

	R::FCharacterAppearance ToRules(const FDBAppearance& In)
	{
		R::FCharacterAppearance Out;
		Out.BodyType = CastEnum<R::EBodyType>(In.BodyType);
		Out.FacePreset = In.FacePreset;
		Out.SkinTone = In.SkinTone;
		Out.SkinDetail = In.SkinDetail;
		Out.HairStyle = In.HairStyle;
		Out.HairColor = PackColor(In.HairColor);
		Out.EyeStyle = In.EyeStyle;
		Out.EyeColor = PackColor(In.EyeColor);
		Out.Scars.assign(In.Scars.begin(), In.Scars.end());
		for (const FDBMorphValue& Morph : In.Morphs)
		{
			Out.Morphs[ToStd(Morph.Morph)] = FMath::Clamp(Morph.Value, -1.f, 1.f);
		}
		Out.VoicePreset = In.VoicePreset;
		return Out;
	}

	FDBAppearance FromRules(const R::FCharacterAppearance& In)
	{
		FDBAppearance Out;
		Out.BodyType = CastEnum<EDBBodyType>(In.BodyType);
		Out.FacePreset = In.FacePreset;
		Out.SkinTone = In.SkinTone;
		Out.SkinDetail = In.SkinDetail;
		Out.HairStyle = In.HairStyle;
		Out.HairColor = UnpackColor(In.HairColor);
		Out.EyeStyle = In.EyeStyle;
		Out.EyeColor = UnpackColor(In.EyeColor);
		for (const int32 Scar : In.Scars)
		{
			Out.Scars.Add(Scar);
		}
		for (const auto& [Key, Value] : In.Morphs)
		{
			FDBMorphValue& Morph = Out.Morphs.AddDefaulted_GetRef();
			Morph.Morph = ToFName(Key);
			Morph.Value = Value;
		}
		Out.VoicePreset = In.VoicePreset;
		return Out;
	}

	R::FItemStack MakeStack(FName ItemId, int32 Count, uint64 InstanceId, int32 Durability)
	{
		R::FItemStack Stack;
		Stack.ItemId = ToStd(ItemId);
		Stack.Count = Count;
		Stack.InstanceId = InstanceId;
		Stack.Durability = Durability;
		return Stack;
	}

	R::FSlotRef ToRules(const FDBSlotRef& In)
	{
		return R::FSlotRef{In.Section, In.Index};
	}

	TArray<uint8> ToArray(const std::vector<uint8_t>& In)
	{
		TArray<uint8> Out;
		Out.Append(In.data(), static_cast<int32>(In.size()));
		return Out;
	}

	uint64 NewInstanceId()
	{
		for (;;)
		{
			const FGuid Guid = FGuid::NewGuid();
			const uint64 Id = ((uint64(Guid.A) << 32) | Guid.B) ^ ((uint64(Guid.C) << 32) | Guid.D);
			if (Id != 0)
			{
				return Id;
			}
		}
	}
}
