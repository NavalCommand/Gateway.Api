#pragma once
#include <optional>

namespace NavalCommand::World::Api {
struct Position {
	double latitudeDeg{};
	double longitudeDeg{};
	std::optional<double> altitudeMeters;
};
} // namespace NavalCommand::World::Api
