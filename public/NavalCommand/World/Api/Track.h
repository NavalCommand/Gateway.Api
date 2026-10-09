#pragma once
#include <NavalCommand/World/Api/Position.h>
#include <string>

namespace NavalCommand::World::Api {
struct Track {
	std::string id;
	Position detectedPosition;
	double sampledAtSeconds{};
	std::string provenance;
};
} // namespace NavalCommand::World::Api
