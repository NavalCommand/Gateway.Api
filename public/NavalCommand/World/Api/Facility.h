#pragma once
#include <NavalCommand/World/Api/Stock.h>
#include <cstdint>
#include <string>
#include <vector>

namespace NavalCommand::World::Api {
struct Facility {
	std::string entityId;
	std::vector<Stock> stocks;
	std::uint32_t pendingOperations{};
	double sampledAtSeconds{};
};
} // namespace NavalCommand::World::Api
