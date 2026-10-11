#pragma once
#include <NavalCommand/Orders/Api/OrderAvailability.h>
#include <NavalCommand/World/Api/View.h>
#include <vector>

namespace NavalCommand::Gateway::Api {
// Owned I/O snapshot and independent order-policy overlay. Domain World stays order-free.
struct ProviderObservation {
	World::Api::View Snapshot;
	std::vector<Orders::Api::OrderAvailability> OrderAvailability;
};
}
