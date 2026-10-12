#pragma once

#include <NavalCommand/Gateway/Api/Codec.h>
#include <chrono>
#include <memory>

namespace NavalCommand::Gateway::Api
{
	// Worker-facing boundary. Publications are immutable serialized-domain
	// values. No implementation may synchronously access Unity on an I/O worker.
	class IProvider
	{
	public:
		virtual ~IProvider() = default;
		virtual std::shared_ptr<const Wire::Publication> Publication() const = 0;
		// Success is an applied result, not queue acceptance. Expired requests
		// must be cancelled before simulator mutation.
		virtual Error::Api::Result<Wire::Result> Apply(const Wire::Control& request, std::chrono::steady_clock::time_point deadline) = 0;
		// Must reliably schedule owner release; never wait for the game thread.
		virtual void Disconnected(const std::string& client) = 0;
	};
}
