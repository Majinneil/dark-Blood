// Conversions between Unreal types and the engine-independent rules core.
#pragma once

#include "CoreMinimal.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/Records.h"

#include <string>

namespace DBBridge
{
	DARKBLOOD_API std::string ToStd(const FString& In);
	DARKBLOOD_API std::string ToStd(FName In);
	DARKBLOOD_API FString ToFString(const std::string& In);
	DARKBLOOD_API FName ToFName(const std::string& In);

	DARKBLOOD_API DarkBlood::Rules::FCharacterAppearance ToRules(const FDBAppearance& In);
	DARKBLOOD_API FDBAppearance FromRules(const DarkBlood::Rules::FCharacterAppearance& In);

	DARKBLOOD_API DarkBlood::Rules::FItemStack MakeStack(FName ItemId, int32 Count, uint64 InstanceId = 0, int32 Durability = -1);
	DARKBLOOD_API DarkBlood::Rules::FSlotRef ToRules(const FDBSlotRef& In);

	DARKBLOOD_API TArray<uint8> ToArray(const std::vector<uint8_t>& In);

	/** Stable non-zero 64-bit id for unique item instances (forged weapons etc.). */
	DARKBLOOD_API uint64 NewInstanceId();

	template <typename TTo, typename TFrom>
	TTo CastEnum(TFrom Value)
	{
		return static_cast<TTo>(static_cast<uint8>(Value));
	}
}
