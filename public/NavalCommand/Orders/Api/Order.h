#pragma once
#include <cstdint>
#include <optional>
#include <string>
namespace NavalCommand::Orders::Api {
enum class Stage {
	Unknown,
	Received,
	Rejected,
	NativeTaskCreated,
	MovementObserved
};
struct Waypoint {
	double latitudeDeg{}, longitudeDeg{};
};
struct Request {
	std::string sessionId, requestId, entityId;
	std::uint64_t basedOnVersion{};
	std::optional<Waypoint> waypoint;
};
struct Result {
	std::string sessionId, requestId;
	Stage stage{};
	std::string reason;
	double sampledAtSeconds{};
	std::optional<double> displacementMeters;
};
} // namespace NavalCommand::Orders::Api
