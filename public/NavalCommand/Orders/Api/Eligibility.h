#pragma once
#include <string>
namespace NavalCommand::Orders::Api {
enum class Availability {
	Unknown,
	Executable,
	Blocked,
	Unsupported
};
struct Eligibility {
	std::string kind;
	Availability availability{};
	std::string reason, arguments;
};
} // namespace NavalCommand::Orders::Api
