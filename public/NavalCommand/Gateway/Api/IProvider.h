#pragma once
#include <NavalCommand/Orders/Api/Order.h>
#include <NavalCommand/World/Api/View.h>
#include <expected>
namespace NavalCommand::Gateway::Api {
enum class Error {
	NotReady,
	Busy,
	TooLarge,
	Malformed,
	Stopped
};
// Thread-safe I/O-side provider. Returns owned values. Never calls a simulator.
// Submission only acknowledges mailbox receipt; native acceptance is
// asynchronous.
struct IProvider {
	virtual ~IProvider() = default;
	virtual std::expected<World::Api::View, Error> View() = 0;
	virtual Orders::Api::Result Submit(const Orders::Api::Request &) = 0;
	virtual Orders::Api::Result Status(const std::string &session, const std::string &request) = 0;
};
} // namespace NavalCommand::Gateway::Api
