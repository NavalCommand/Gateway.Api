#include <NavalCommand/Gateway/Api/Codec.h>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
	if (argc != 3) {
		return 2;
	}
	std::ifstream requestInput(argv[1], std::ios::binary);
	navalcommand::gateway::v1::OrderRequest request;
	if (!request.ParseFromIstream(&requestInput) || !request.has_waypoint() || request.request_id() != "conformance" ||
		request.waypoint().latitude_deg() != 61.96 || request.observer().value() != "red" ||
		request.observation().value() != "own-contact") {
		return 3;
	}
	request.mutable_waypoint()->set_longitude_deg(-10.37);
	std::ofstream requestOutput(argv[2], std::ios::binary);
	if (!request.SerializeToOstream(&requestOutput)) {
		return 4;
	}

	std::ifstream providerInput(std::string{argv[1]} + ".world", std::ios::binary);
	navalcommand::gateway::v1::ProviderObservation provider;
	if (!provider.ParseFromIstream(&providerInput)) {
		return 5;
	}
	const auto value = NavalCommand::Gateway::Api::Decode(provider);
	if (value.Snapshot.Units.size() != 2 || value.Snapshot.Units[0].Identity.Value != "own-contact" ||
		value.Snapshot.Units[1].Identity.Value != "fused-contact" || value.Snapshot.Units[0].Name.Value() != "Helo-1" ||
		value.Snapshot.Units[1].Name.HasValue() || !value.Snapshot.Units[0].Aircraft ||
		value.Snapshot.Units[0].Aircraft->Airborne.Value() || value.Snapshot.Units[0].Kinematics.Speed.Value().Value != 0 ||
		value.OrderAvailability.size() != 1 || value.Snapshot.Memberships.Items().size() != 1 ||
		value.Snapshot.Memberships.Items()[0].Faction.Value().Value != "friendly" ||
		value.Snapshot.Memberships.Items()[0].TaskForce.Value().Value != "group-1") {
		return 6;
	}
	const auto& equipment = value.Snapshot.Units[0].Equipment.Items()[0];
	if (std::get<uint64_t>(equipment.Resources.Items()[0].Amount.Value()) != 0 ||
		std::get<double>(equipment.Resources.Items()[1].Amount.Value()) != 12.5 ||
		equipment.Resources.Items()[2].Amount.HasValue()) {
		return 8;
	}
	std::ofstream providerOutput(std::string{argv[2]} + ".world", std::ios::binary);
	if (!NavalCommand::Gateway::Api::Encode(value).SerializeToOstream(&providerOutput)) {
		return 7;
	}
	std::cout << "PASS observer-only C++ protobuf roundtrip with names, relationships, orders and uncertainty\n";
}
