#include <NavalCommand/Gateway/Api/Codec.h>
#include <fstream>
#include <iostream>
#include <variant>

int main(int argc, char** argv) {
	if (argc != 3) return 2;
	std::ifstream requestInput(argv[1], std::ios::binary);
	navalcommand::gateway::v1::OrderRequest request;
	if (!request.ParseFromIstream(&requestInput) || !request.has_waypoint() || request.request_id() != "conformance" ||
		request.waypoint().latitude_deg() != 61.96 || request.observer().value() != "red" ||
		request.observation().value() != "own-contact") return 3;
	request.mutable_waypoint()->set_longitude_deg(-10.37);
	std::ofstream requestOutput(argv[2], std::ios::binary);
	if (!request.SerializeToOstream(&requestOutput)) return 4;

	std::ifstream providerInput(std::string{argv[1]} + ".world", std::ios::binary);
	navalcommand::gateway::v1::ProviderObservation provider;
	if (!provider.ParseFromIstream(&providerInput)) return 5;
	const auto value = NavalCommand::Gateway::Api::Decode(provider);
	if (value.Snapshot.Units.size() != 2 || value.Snapshot.Units[0].Identity.Value != "own-contact" ||
		value.Snapshot.Units[1].Identity.Value != "contact" || !value.Snapshot.Units[0].Name.HasValue() ||
		value.Snapshot.Units[0].Name.Value() != "" || value.Snapshot.Units[1].Name.HasValue() ||
		!value.Snapshot.Units[0].Aircraft || value.Snapshot.Units[0].Aircraft->Airborne.Value() ||
		value.Snapshot.Units[0].Kinematics.Speed.Value().Value != 0 ||
		value.Snapshot.Units[0].MeasuredAt.Value().Value != 42 || value.Snapshot.Units[1].MeasuredAt.HasValue() ||
		value.OrderAvailability.size() != 1 || value.Snapshot.Memberships.size() != 1 ||
		value.Snapshot.Memberships[0].Faction.Value().Value != "blue" ||
		value.Snapshot.Memberships[0].TaskForce.HasValue()) return 6;
	const auto* exactPosition = std::get_if<NavalCommand::Uncertainty::Api::Exact<NavalCommand::Physics::Api::Position>>(
		&value.Snapshot.Units[0].Kinematics.Position);
	const auto* partialPosition = std::get_if<NavalCommand::Observations::Api::PartiallyLocalizedPosition>(
		&value.Snapshot.Units[1].Kinematics.Position);
	if (!exactPosition || !exactPosition->HasValue() || exactPosition->Value().Latitude.Value != 0 ||
		exactPosition->Value().Longitude.Value != 0 || !partialPosition || !partialPosition->Longitude ||
		partialPosition->Longitude->Value != 0 || !partialPosition->ErrorEllipse ||
		partialPosition->ErrorEllipse->MajorAxis.Value != 100 || partialPosition->ErrorEllipse->MinorAxis.Value != 25) return 9;
	const auto& equipment = value.Snapshot.Units[0].Equipment[0];
	if (std::get<uint64_t>(equipment.Resources[0].Amount.Value()) != 0) return 8;
	std::ofstream providerOutput(std::string{argv[2]} + ".world", std::ios::binary);
	if (!NavalCommand::Gateway::Api::Encode(value).SerializeToOstream(&providerOutput)) return 7;
	std::cout << "PASS observer-only C++ protobuf roundtrip with exact presence, partial location, contributors and order rows\n";
}
