#include <NavalCommand/Gateway/Api/Codec.h>
#include <gtest/gtest.h>
using namespace NavalCommand;
TEST(Codec, DomainDoesNotRequireTransport) {
	World::Api::View domainView;
	domainView.sessionId = "opaque";
	domainView.ready = true;
	domainView.units.push_back({"u", "own", {61.96, -10.37, {}}, {}, 42, {{"set_waypoint", Orders::Api::Availability::Executable, "", "append"}}});
	auto wireValue = Gateway::Api::Encode(domainView);
	auto roundtripValue = Gateway::Api::Decode(wireValue);
	EXPECT_EQ(roundtripValue.units[0].id, "u");
	EXPECT_FALSE(roundtripValue.units[0].position.altitudeMeters);
	EXPECT_FALSE(wireValue.own_units(0).has_speed_mps());
	EXPECT_EQ(roundtripValue.units[0].orders[0].kind, "set_waypoint");
}
TEST(Codec, OptionalZeroAndUnknownEnum) {
	navalcommand::gateway::v1::OrderResult wireValue;
	wireValue.set_displacement_m(0);
	wireValue.set_stage(static_cast<navalcommand::gateway::v1::Stage>(99));
	auto roundtripValue = Gateway::Api::Encode(Gateway::Api::Decode(wireValue));
	EXPECT_TRUE(roundtripValue.has_displacement_m());
	EXPECT_EQ(roundtripValue.stage(), 99);
}
