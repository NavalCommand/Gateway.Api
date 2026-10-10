#pragma once
#include <NavalCommand/Gateway/Api/Error.h>
#include <NavalCommand/Orders/Api/Result.h>
#include <NavalCommand/Orders/Api/Request.h>
#include <NavalCommand/World/Api/View.h>
#include <expected>

namespace NavalCommand::Gateway::Api
{
	// Thread-safe I/O-side provider. Returns owned values. Never calls a simulator.
	// Submission only acknowledges mailbox receipt; native acceptance is
	// asynchronous.
	struct IProvider
	{
		virtual ~IProvider() = default;
		virtual std::expected<World::Api::View, Error> View() = 0;
		virtual Orders::Api::Result Submit(const Orders::Api::Request&) = 0;
		virtual Orders::Api::Result Status(const std::string& session, const std::string& request) = 0;
	};
}
