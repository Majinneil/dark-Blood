// DARK BLOOD - Rules Core: minimal UTF-8 helpers.
#pragma once

#include "DarkBloodRules/RulesCore.h"

#include <string>
#include <string_view>
#include <vector>

namespace DarkBlood::Rules::Utf8
{
	/** Decodes UTF-8 into code points. Returns false on malformed, overlong or surrogate sequences. */
	DARKBLOODRULES_API bool Decode(std::string_view Text, std::vector<char32_t>& OutCodePoints);

	DARKBLOODRULES_API void Append(std::string& Out, char32_t CodePoint);
}
