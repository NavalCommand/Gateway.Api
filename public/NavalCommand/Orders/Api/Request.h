#pragma once
#include <NavalCommand/Orders/Api/Waypoint.h>
#include <cstdint>
#include <optional>
#include <string>

namespace NavalCommand::Orders::Api {
struct Request {
	std::string sessionId;
	std::string requestId;
	std::string entityId;
	std::uint64_t basedOnVersion{};
	std::optional<Waypoint> waypoint;
};
} // namespace NavalCommand::Orders::Api
