#pragma once
#include <NavalCommand/Orders/Api/Stage.h>
#include <optional>
#include <string>

namespace NavalCommand::Orders::Api {
struct Result {
	std::string sessionId;
	std::string requestId;
	Stage stage{};
	std::string reason;
	double sampledAtSeconds{};
	std::optional<double> displacementMeters;
};
} // namespace NavalCommand::Orders::Api
