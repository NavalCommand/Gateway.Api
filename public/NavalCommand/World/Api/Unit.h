#pragma once
#include <NavalCommand/Orders/Api/Eligibility.h>
#include <NavalCommand/World/Api/Position.h>
#include <optional>
#include <string>
#include <vector>

namespace NavalCommand::World::Api {
using Eligibility = NavalCommand::Orders::Api::Eligibility;
struct Unit {
	std::string id;
	std::string name;
	Position position;
	std::optional<double> speedMetersPerSecond;
	double sampledAtSeconds{};
	std::vector<Eligibility> orders;
};
} // namespace NavalCommand::World::Api
