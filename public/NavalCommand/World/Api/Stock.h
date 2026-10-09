#pragma once
#include <cstdint>
#include <string>

namespace NavalCommand::World::Api {
struct Stock {
	std::string classKey;
	std::string squadron;
	std::uint32_t unreservedCount{};
};
} // namespace NavalCommand::World::Api
