#pragma once
namespace NavalCommand::Gateway::Api {
enum class Error {
	NotReady,
	Busy,
	TooLarge,
	Malformed,
	Stopped
};
} // namespace NavalCommand::Gateway::Api
