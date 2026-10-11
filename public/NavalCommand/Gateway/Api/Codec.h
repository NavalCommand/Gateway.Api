#pragma once
#include <NavalCommand/Gateway/Api/IProvider.h>
#include <navalcommand/gateway/v1/provider.pb.h>

#include <vector>

namespace NavalCommand::Gateway::Api {
namespace Wire = navalcommand::gateway::v1;
Wire::OrderEligibilityState Encode(const std::vector<Orders::Api::Eligibility> &);
std::vector<Orders::Api::Eligibility> Decode(const Wire::OrderEligibilityState &);
Wire::OrderDiscovery Encode(const Orders::Api::OrderAvailability &);
Orders::Api::OrderAvailability Decode(const Wire::OrderDiscovery &);
Wire::ObserverSnapshot Encode(const World::Api::ObserverSnapshot &);
World::Api::ObserverSnapshot Decode(const Wire::ObserverSnapshot &);
Wire::ProviderObservation Encode(const ProviderObservation &);
ProviderObservation Decode(const Wire::ProviderObservation &);
Wire::OrderRequest Encode(const Orders::Api::Request &);
Orders::Api::Request Decode(const Wire::OrderRequest &);
Wire::OrderResult Encode(const Orders::Api::Result &);
Orders::Api::Result Decode(const Wire::OrderResult &);
}
