#pragma once
#include <NavalCommand/World/Api/Facility.h>
#include <NavalCommand/World/Api/Track.h>
#include <NavalCommand/World/Api/Unit.h>
#include <cstdint>
#include <string>
#include <vector>

namespace NavalCommand::World::Api {
struct View {
	std::string sessionId;
	std::string observerId;
	bool ready{};
	bool truncated{};
	std::uint64_t version{};
	double operationalSeconds{};
	std::vector<Unit> units;
	std::vector<Track> tracks;
	std::vector<Facility> facilities;
};
} // namespace NavalCommand::World::Api
