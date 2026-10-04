// DARK BLOOD - Rules Core: character names.
// Character names are chosen freely by the player and are independent from
// the platform/account name. The server always re-validates names.
#pragma once

#include "DarkBloodRules/RulesCore.h"

#include <string>
#include <string_view>

namespace DarkBlood::Rules
{
	enum class ENameValidation : uint8
	{
		Ok,
		Empty,
		InvalidUtf8,
		TooShort,
		TooLong,
		InvalidCharacter,
		SeparatorAtEdge,
		ConsecutiveSeparators,
		TooManyWords,
	};

	struct FNameRules
	{
		int32 MinLength = 2;  // in code points
		int32 MaxLength = 24; // in code points
		int32 MaxWords = 3;
	};

	/** Trims leading/trailing whitespace and collapses internal whitespace runs to one space. */
	DARKBLOODRULES_API std::string NormalizeCharacterName(std::string_view Raw);

	/**
	 * Validates an already normalized name. Allowed: Latin letters (incl. umlauts, ss (eszett) and
	 * Latin Extended-A), Hiragana, Katakana, CJK ideographs; separators: space, '-', apostrophe.
	 */
	DARKBLOODRULES_API ENameValidation ValidateCharacterName(std::string_view Name, const FNameRules& Rules = FNameRules());

	/** The short form NPCs use when addressing the character ("Jin Akagi" -> "Jin"). */
	DARKBLOODRULES_API std::string GetCallName(std::string_view FullName);

	DARKBLOODRULES_API const char* ToString(ENameValidation Result);
}
