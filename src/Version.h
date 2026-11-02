#pragma once

#include <cstdint>

namespace HouseRules::Version
{
	// Defined by xmake from set_version.
	inline constexpr std::uint16_t kMajor = HOUSERULES_VERSION_MAJOR;
	inline constexpr std::uint16_t kMinor = HOUSERULES_VERSION_MINOR;
	inline constexpr std::uint16_t kPatch = HOUSERULES_VERSION_PATCH;
}
