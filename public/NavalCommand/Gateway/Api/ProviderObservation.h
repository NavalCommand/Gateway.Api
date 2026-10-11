#pragma once
#include <NavalCommand/Orders/Api/OrderAvailability.h>
#include <NavalCommand/World/Api/ObserverSnapshot.h>
#include <vector>

namespace NavalCommand::Gateway::Api {
// Public I/O view is one observer snapshot plus a separately authorized order overlay.
struct ProviderObservation {
	World::Api::ObserverSnapshot Snapshot;
	std::vector<Orders::Api::OrderAvailability> OrderAvailability;
};
}
