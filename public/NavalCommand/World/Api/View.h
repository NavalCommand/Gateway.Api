#pragma once
#include <NavalCommand/Orders/Api/Eligibility.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace NavalCommand::World::Api {
struct Position {
	double latitudeDeg{}, longitudeDeg{};
	std::optional<double> altitudeMeters;
};
using Eligibility = NavalCommand::Orders::Api::Eligibility;
struct Unit {
	std::string id, name;
	Position position;
	std::optional<double> speedMetersPerSecond;
	double sampledAtSeconds{};
	std::vector<Eligibility> orders;
};
struct Track {
	std::string id;
	Position detectedPosition;
	double sampledAtSeconds{};
	std::string provenance;
};
struct Stock {
	std::string classKey, squadron;
	std::uint32_t unreservedCount{};
};
struct Facility {
	std::string entityId;
	std::vector<Stock> stocks;
	std::uint32_t pendingOperations{};
	double sampledAtSeconds{};
};
struct View {
	std::string sessionId, observerId;
	bool ready{}, truncated{};
	std::uint64_t version{};
	double operationalSeconds{};
	std::vector<Unit> units;
	std::vector<Track> tracks;
	std::vector<Facility> facilities;
};
} // namespace NavalCommand::World::Api
