#pragma once
#include <NavalCommand/Orders/Api/Availability.h>
#include <string>
namespace NavalCommand::Orders::Api {

struct Eligibility {
	std::string kind;
	Availability availability{};
	std::string reason;
	std::string arguments;
};
} // namespace NavalCommand::Orders::Api
