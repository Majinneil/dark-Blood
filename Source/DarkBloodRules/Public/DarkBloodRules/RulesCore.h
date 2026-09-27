// DARK BLOOD - Rules Core
// Engine-independent gameplay rules. This code must compile both inside
// Unreal Build Tool (as the DarkBloodRules module) and standalone via CMake
// for unit tests. Constraints (mirroring UE defaults):
//   - no exceptions, no RTTI
//   - no Unreal headers in any file except DarkBloodRulesModule.cpp
//   - all text is UTF-8 encoded std::string
#pragma once

#include <cstdint>

#ifndef DARKBLOODRULES_API
#define DARKBLOODRULES_API
#endif

namespace DarkBlood::Rules
{
	using int8 = std::int8_t;
	using int16 = std::int16_t;
	using int32 = std::int32_t;
	using int64 = std::int64_t;
	using uint8 = std::uint8_t;
	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;
	using uint64 = std::uint64_t;
}
