#pragma once
#include <NavalCommand/Gateway/Api/IProvider.h>
#include <navalcommand/gateway/v1/world.pb.h>

namespace NavalCommand::Gateway::Api
{
	namespace Wire = navalcommand::gateway::v1;
	Wire::WorldSnapshot Encode(const World::Api::View&);
	World::Api::View Decode(const Wire::WorldSnapshot&);
	Wire::OrderRequest Encode(const Orders::Api::Request&);
	Orders::Api::Request Decode(const Wire::OrderRequest&);
	Wire::OrderResult Encode(const Orders::Api::Result&);
	Orders::Api::Result Decode(const Wire::OrderResult&);
}
