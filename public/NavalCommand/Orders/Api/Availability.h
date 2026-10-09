#pragma once
namespace NavalCommand::Orders::Api {
enum class Availability {
	Unknown,
	Executable,
	Blocked,
	Unsupported
};
} // namespace NavalCommand::Orders::Api
