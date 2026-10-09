#pragma once

namespace NavalCommand::Orders::Api {
enum class Stage {
	Unknown,
	Received,
	Rejected,
	NativeTaskCreated,
	MovementObserved
};
} // namespace NavalCommand::Orders::Api
