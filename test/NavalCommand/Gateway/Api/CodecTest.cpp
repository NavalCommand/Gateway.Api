#include <NavalCommand/Gateway/Api/Codec.h>
#include <gtest/gtest.h>
using namespace NavalCommand;
TEST(Codec, DomainDoesNotRequireTransport)
{
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

TEST(Codec, OptionalZeroAndUnknownEnum)
{
	navalcommand::gateway::v1::OrderResult wireValue;
	wireValue.set_displacement_m(0);
	wireValue.set_stage(static_cast<navalcommand::gateway::v1::Stage>(99));
	auto roundtripValue = Gateway::Api::Encode(Gateway::Api::Decode(wireValue));
	EXPECT_TRUE(roundtripValue.has_displacement_m());
	EXPECT_EQ(roundtripValue.stage(), 99);
}

TEST(Codec, RouteModesAndReceipts)
{
	using namespace Orders::Api;
	Request legacy{"session", "legacy", "unit", 7, Waypoint{62, -10}};
	EXPECT_EQ(Gateway::Api::Encode(legacy).waypoint().route_mode(), 0);
	legacy.waypoint->routeMode = RouteMode::AppendWaypoint;
	EXPECT_EQ(Gateway::Api::Decode(Gateway::Api::Encode(legacy)).waypoint->routeMode, RouteMode::AppendWaypoint);
	Result result{"session", "id", Stage::RouteMutated, "sequence_verified"};
	result.routeBefore.push_back({62, -10, "A"});
	result.routeAfter = {{62, -10, "A"}, {63, -11, "B"}};
	result.routeMode = RouteMode::AppendWaypoint;
	auto decoded = Gateway::Api::Decode(Gateway::Api::Encode(result));
	ASSERT_EQ(decoded.routeAfter.size(), 2);
	EXPECT_EQ(decoded.routeAfter[1].label, "B");
	EXPECT_EQ(decoded.routeBefore[0].latitudeDeg, 62);
	EXPECT_EQ(decoded.stage, Stage::RouteMutated);
}

TEST(Codec, FacilityLaunchAndGroundedPresence)
{
	Orders::Api::Request request{"session", "launch", "facility", 1};
	request.facilityLaunch = Orders::Api::FacilityLaunch{"class", "squadron", "loadout", 1};
	auto decoded = Gateway::Api::Decode(Gateway::Api::Encode(request));
	ASSERT_TRUE(decoded.facilityLaunch);
	EXPECT_FALSE(decoded.waypoint);
	EXPECT_EQ(decoded.facilityLaunch->loadout, "loadout");
	EXPECT_EQ(decoded.facilityLaunch->count, 1);
	World::Api::View view;
	World::Api::Unit grounded;
	grounded.id = "spawn";
	grounded.airborne = false;
	view.units.push_back(grounded);
	view.facilities.push_back({"facility", {{"class", "squadron", 1, {{"loadout", Orders::Api::Availability::Blocked, "resources_unavailable"}}}}});
	auto wire = Gateway::Api::Encode(view);
	EXPECT_TRUE(wire.own_units(0).has_airborne());
	EXPECT_FALSE(wire.own_units(0).airborne());
	EXPECT_EQ(Gateway::Api::Decode(wire).facilities[0].stocks[0].launchOptions[0].reason, "resources_unavailable");
	Orders::Api::Result spawned{"session", "launch", Orders::Api::Stage::AircraftSpawned, "spawned"};
	spawned.spawnedEntityId = "spawn";
	EXPECT_EQ(Gateway::Api::Decode(Gateway::Api::Encode(spawned)).spawnedEntityId, "spawn");
}
