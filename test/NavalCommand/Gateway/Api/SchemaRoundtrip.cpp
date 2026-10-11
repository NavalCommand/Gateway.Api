#include <NavalCommand/Gateway/Api/Codec.h>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
	if (argc != 3) {
		return 2;
	}
	std::ifstream inputStream(argv[1], std::ios::binary);
	navalcommand::gateway::v1::OrderRequest wireRequest;
	if (!wireRequest.ParseFromIstream(&inputStream) || !wireRequest.has_waypoint() || wireRequest.request_id() != "conformance" || wireRequest.waypoint().latitude_deg() != 61.96) {
		return 3;
	}
	wireRequest.mutable_waypoint()->set_longitude_deg(-10.37);
	std::ofstream outputStream(argv[2], std::ios::binary);
	if (!wireRequest.SerializeToOstream(&outputStream)) {
		return 4;
	}
	std::ifstream worldInput(std::string{argv[1]} + ".world", std::ios::binary);
	navalcommand::gateway::v1::WorldSnapshot world;
	if (!world.ParseFromIstream(&worldInput)) {
		return 5;
	}
	auto domain = NavalCommand::Gateway::Api::Decode(world);
	if (domain.Instances.size() != 3 || !domain.Instances[0].Aircraft || domain.Instances[0].Aircraft->Airborne.Value() || !domain.Instances[1].Kinematics.Position.IsEstimated()) {
		return 6;
	}
	std::ofstream worldOutput(std::string{argv[2]} + ".world", std::ios::binary);
	if (!NavalCommand::Gateway::Api::Encode(domain).SerializeToOstream(&worldOutput)) {
		return 7;
	}
	std::cout << "C++ parsed oneof, stable IDs, degrees and preserved unknown fields\n";
}
