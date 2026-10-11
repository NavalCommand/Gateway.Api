#include <NavalCommand/Gateway/Api/Codec.h>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
	if (argc != 3) {
		return 2;
	}
	std::ifstream inputStream(argv[1], std::ios::binary);
	navalcommand::gateway::v1::OrderRequest wireRequest;
	if (!wireRequest.ParseFromIstream(&inputStream) || !wireRequest.has_waypoint() || wireRequest.request_id() != "conformance" ||
		wireRequest.waypoint().latitude_deg() != 61.96 || wireRequest.unit_id().value().empty()) {
		return 3;
	}
	wireRequest.mutable_waypoint()->set_longitude_deg(-10.37);
	std::ofstream outputStream(argv[2], std::ios::binary);
	if (!wireRequest.SerializeToOstream(&outputStream)) {
		return 4;
	}
	std::ifstream worldInput(std::string{argv[1]} + ".world", std::ios::binary);
	navalcommand::gateway::v1::ProviderObservation observation;
	if (!observation.ParseFromIstream(&worldInput)) {
		return 5;
	}
	const auto domain = NavalCommand::Gateway::Api::Decode(observation);
	if (domain.Snapshot.Instances.size() != 3 || !domain.Snapshot.Instances[0].Aircraft || domain.Snapshot.Instances[0].Aircraft->Airborne.Value() ||
		!domain.Snapshot.Instances[1].Kinematics.Position.IsEstimated() || domain.Snapshot.Instances[1].Observation.MeasuredAt.HasValue() ||
		domain.OrderAvailability.size() != 1 || !domain.Snapshot.Instances[0].AirOperations) {
		return 6;
	}
	const auto &resources = domain.Snapshot.Instances[0].Equipment.Items()[0].Resources.Items();
	if (std::get<uint64_t>(resources[0].Amount.Value()) != 0 || std::get<double>(resources[1].Amount.Value()) != 12.5 || resources[2].Amount.HasValue()) {
		return 8;
	}
	std::ofstream worldOutput(std::string{argv[2]} + ".world", std::ios::binary);
	if (!NavalCommand::Gateway::Api::Encode(domain).SerializeToOstream(&worldOutput)) {
		return 7;
	}
	std::cout << "C++ preserved direct payload, IDs, SI values, uncertainty, inventory and separate availability\n";
}
