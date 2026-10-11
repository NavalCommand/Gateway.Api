#pragma once
#include <NavalCommand/Gateway/Api/IProvider.h>
#include <navalcommand/gateway/v1/provider.pb.h>

namespace NavalCommand::Gateway::Api {
namespace Wire = navalcommand::gateway::v1;
Wire::OrderEligibilityState Encode(const Uncertainty::Api::ObservedCollection<Orders::Api::Eligibility> &);
Uncertainty::Api::ObservedCollection<Orders::Api::Eligibility> Decode(const Wire::OrderEligibilityState &);
Wire::OrderDiscovery Encode(const Orders::Api::OrderAvailability &);
Orders::Api::OrderAvailability Decode(const Wire::OrderDiscovery &);
Wire::WorldSnapshot Encode(const World::Api::View &);
World::Api::View Decode(const Wire::WorldSnapshot &);
Wire::ProviderObservation Encode(const ProviderObservation &);
ProviderObservation Decode(const Wire::ProviderObservation &);
Wire::OrderRequest Encode(const Orders::Api::Request &);
Orders::Api::Request Decode(const Wire::OrderRequest &);
Wire::OrderResult Encode(const Orders::Api::Result &);
Orders::Api::Result Decode(const Wire::OrderResult &);
}
