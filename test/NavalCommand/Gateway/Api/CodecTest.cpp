#include <NavalCommand/Gateway/Api/Codec.h>
#include <navalcommand/gateway/v1/recording.pb.h>
#include <gtest/gtest.h>
#include <type_traits>
#include <stdexcept>
#include <string>

namespace Physics = NavalCommand::Physics::Api;
namespace Catalog = NavalCommand::Catalog::Api;
namespace Observations = NavalCommand::Observations::Api;
namespace Orders = NavalCommand::Orders::Api;
namespace World = NavalCommand::World::Api;
namespace Gateway = NavalCommand::Gateway::Api;
template <class T> using Knowledge = NavalCommand::Uncertainty::Api::Knowledge<T>;
template <class T> using Collection = NavalCommand::Uncertainty::Api::ObservedCollection<T>;

TEST(Codec, ObserverSnapshotsHaveIndependentContactIdentities) {
	static_assert(std::is_same_v<decltype(Observations::UnitObservation::Identity), Observations::ObservationId>);
	static_assert(std::is_same_v<decltype(World::ObserverSnapshot::Observer), Observations::ObserverId>);

	World::ObserverSnapshot first;
	first.SessionId = "session";
	first.Observer = {"red"};
	first.Ready = true;
	first.Version = 9;
	first.CapturedAt = {150};

	Observations::UnitObservation contact;
	contact.Identity = {"red-contact-1"};
	contact.Type = Knowledge<Catalog::UnitTypeId>::Estimated({"missile"}, "ambiguous-classification");
	contact.Kinematics.Position = Knowledge<Physics::Position>::Estimated({{61.0}, {-10.0}, {}}, "sensor-fusion");
	contact.Kinematics.Speed = Knowledge<Physics::MetersPerSecond>::Known({0});
	contact.Equipment = {Collection<Observations::EquipmentObservation>::Coverage::Unknown, {}};
	first.Units = {contact, contact};
	first.Units[1].Identity = {"red-fused-contact"};
	first.Units[0].Name = Knowledge<std::string>::Known("Missile contact");
	first.Memberships = {Collection<World::ObserverUnitRelationship>::Coverage::Complete,
		{{contact.Identity, Knowledge<World::FactionId>::Known({"blue"}),
			Knowledge<World::TaskForceId>::Estimated({"group-1"}, "fusion")}}};

	World::ObserverSnapshot second = first;
	second.Observer = {"blue"};
	second.Units[0].Identity = {"blue-contact-8"};
	second.Units[0].Type = Knowledge<Catalog::UnitTypeId>::Known({"missile"});
	second.Units[0].Kinematics.Position = {};

	const auto red = Gateway::Decode(Gateway::Encode(first));
	const auto blue = Gateway::Decode(Gateway::Encode(second));
	ASSERT_EQ(red.Units.size(), 2);
	ASSERT_EQ(blue.Units.size(), 2);
	EXPECT_EQ(red.Units[0].Identity.Value, "red-contact-1");
	EXPECT_EQ(blue.Units[0].Identity.Value, "blue-contact-8");
	EXPECT_TRUE(red.Units[0].Type.IsEstimated());
	EXPECT_TRUE(blue.Units[0].Type.IsKnown());
	EXPECT_EQ(red.Units[0].Name.Value(), "Missile contact");
	EXPECT_FALSE(red.Units[1].Name.HasValue());
	ASSERT_EQ(red.Memberships.Items().size(), 1);
	EXPECT_EQ(red.Memberships.Items()[0].Faction.Value().Value, "blue");
	EXPECT_EQ(red.Memberships.Items()[0].TaskForce.Source(), "fusion");
	EXPECT_FALSE(blue.Units[0].Kinematics.Position.HasValue());
	EXPECT_FALSE(red.Units[0].Aircraft.has_value());
	EXPECT_FALSE(red.Units[0].AirOperations.has_value());
	EXPECT_EQ(red.Memberships.GetCoverage(), Collection<World::ObserverUnitRelationship>::Coverage::Complete);

	const auto publicBytes = Gateway::Encode(Gateway::ProviderObservation{first, {}}).SerializeAsString();
	EXPECT_EQ(publicBytes.find("truth-private"), std::string::npos);
}

TEST(Codec, KnownZeroFalseAndEmptyRouteDifferFromUnknown) {
	World::ObserverSnapshot source;
	source.SessionId = "session";
	source.Observer = {"observer"};
	source.Version = 4;
	Observations::UnitObservation unit;
	unit.Identity = {"contact"};
	unit.Kinematics.Speed = Knowledge<Physics::MetersPerSecond>::Known({0});
	unit.Aircraft = Observations::AircraftObservation{Knowledge<bool>::Known(false)};
	unit.Navigation = Observations::NavigationObservation{Knowledge<std::vector<Observations::ObservedRoutePoint>>::Known({})};
	unit.Equipment = {Collection<Observations::EquipmentObservation>::Coverage::Complete, {}};
	source.Units.push_back(unit);

	const auto decoded = Gateway::Decode(Gateway::Encode(source));
	const auto& result = decoded.Units[0];
	EXPECT_TRUE(result.Kinematics.Speed.IsKnown());
	EXPECT_EQ(result.Kinematics.Speed.Value().Value, 0);
	ASSERT_TRUE(result.Aircraft);
	EXPECT_TRUE(result.Aircraft->Airborne.IsKnown());

	EXPECT_FALSE(result.Aircraft->Airborne.Value());
	ASSERT_TRUE(result.Navigation);
	EXPECT_TRUE(result.Navigation->PlannedRoute.IsKnown());
	EXPECT_TRUE(result.Navigation->PlannedRoute.Value().empty());
	EXPECT_EQ(result.Equipment.GetCoverage(), Collection<Observations::EquipmentObservation>::Coverage::Complete);
}

TEST(Codec, OrdersCarryScopedHandlesAndOneTypedPayload) {
	Orders::Request request;
	request.SessionId = "session";
	request.RequestId = "route-request";
	request.Observer = {"red"};
	request.Observation = {"contact-1"};
	request.BasedOnVersion = 11;
	request.Payload = Orders::RouteCommand{{61.96}, {-10.37}, Orders::RouteMode::AppendWaypoint};

	const auto routeWire = Gateway::Encode(request);
	ASSERT_TRUE(routeWire.has_waypoint());
	EXPECT_EQ(routeWire.observer().value(), "red");
	EXPECT_EQ(routeWire.observation().value(), "contact-1");
	EXPECT_EQ(Gateway::Decode(routeWire).Observation.Value, "contact-1");

	Orders::Request launch;
	launch.SessionId = "session";
	launch.RequestId = "launch-request";
	launch.Observer = {"red"};
	launch.Observation = {"carrier-contact"};
	launch.BasedOnVersion = 11;
	launch.Payload = Orders::LaunchAircraft{{{"fighter"}, "VFA-1", {"loadout"}}, 2};
	const auto launchWire = Gateway::Encode(launch);
	ASSERT_TRUE(launchWire.has_launch_aircraft());
	EXPECT_EQ(launchWire.launch_aircraft().selection().type().value(), "fighter");
	EXPECT_EQ(std::get<Orders::LaunchAircraft>(Gateway::Decode(launchWire).Payload).Count, 2);

	Orders::Result result;
	result.SessionId = "session";
	result.RequestId = "launch-request";
	result.Observer = {"red"};
	result.Stage = Orders::Stage::AircraftSpawned;
	result.SpawnedObservation = Observations::ObservationId{"new-aircraft-contact"};
	result.RouteBefore.push_back({{{1}, {2}, Physics::Meters{30}}, "old-waypoint"});
	const auto resultWire = Gateway::Encode(result);
	ASSERT_TRUE(resultWire.has_spawned_observation_id());
	EXPECT_EQ(resultWire.spawned_observation_id().value(), "new-aircraft-contact");
	EXPECT_EQ(resultWire.route_before(0).label(), "old-waypoint");
	const auto decodedResult = Gateway::Decode(resultWire);
	EXPECT_EQ(decodedResult.SpawnedObservation->Value, "new-aircraft-contact");
	EXPECT_EQ(decodedResult.RouteBefore[0].Label, "old-waypoint");
	EXPECT_EQ(decodedResult.RouteBefore[0].Position.Altitude->Value, 30);
}

TEST(Codec, ProviderOverlayValidatesObserverObservationAndVersion) {
	Gateway::ProviderObservation value;
	value.Snapshot.SessionId = "session";
	value.Snapshot.Observer = {"red"};
	value.Snapshot.Version = 2;
	Observations::UnitObservation unit;
	unit.Identity = {"contact"};
	value.Snapshot.Units.push_back(unit);
	value.OrderAvailability.push_back({{"red"}, {"contact"}, 2,
		{Collection<Orders::Eligibility>::Coverage::Complete,
			{{Orders::OrderKind::ReplaceRoute, Orders::Availability::Executable, ""}}}});
	EXPECT_EQ(Gateway::Decode(Gateway::Encode(value)).OrderAvailability.size(), 1);

	auto forged = Gateway::Encode(value);
	forged.mutable_order_availability(0)->mutable_observer()->set_value("blue");
	EXPECT_THROW(Gateway::Decode(forged), std::invalid_argument);
}

TEST(Codec, PrivilegedRecordingHoldsTruthPerspectivesAndOptionalManyToManyLabels) {
	using namespace navalcommand::gateway::v1;
	RecordedSession record;
	record.mutable_simulation()->set_session_id("session");
	record.mutable_simulation()->set_captured_at_seconds(3);
	auto* truth = record.mutable_simulation()->add_units();
	truth->mutable_identity()->set_value("truth-private");
	truth->mutable_type()->set_value("missile");
	truth->mutable_kinematics()->mutable_position()->set_latitude_deg(0);
	truth->mutable_kinematics()->mutable_position()->set_longitude_deg(0);

	auto* red = record.add_perspectives();
	red->set_session_id("session");
	red->set_schema_version(4);
	red->mutable_observer()->set_value("red");
	red->add_units()->mutable_identity()->set_value("red-contact");
	red->mutable_units(0)->mutable_type()->mutable_estimated()->mutable_value()->set_value("missile");
	auto* blue = record.add_perspectives();
	blue->set_session_id("session");
	blue->set_schema_version(4);
	blue->mutable_observer()->set_value("blue");
	blue->add_units()->mutable_identity()->set_value("blue-contact");
	blue->mutable_units(0)->mutable_type()->mutable_known()->set_value("missile");

	auto* redLink = record.add_associations();
	redLink->mutable_observer()->set_value("red");
	redLink->mutable_observation()->set_value("red-contact");
	redLink->mutable_truth()->set_value("truth-private");
	auto* blueLink = record.add_associations();
	blueLink->mutable_observer()->set_value("blue");
	blueLink->mutable_observation()->set_value("blue-contact");
	blueLink->mutable_truth()->set_value("truth-private");

	auto* request = record.add_decisions();
	request->set_session_id("session");
	request->mutable_observer()->set_value("red");
	request->mutable_observation()->set_value("red-contact");
	request->mutable_waypoint()->set_latitude_deg(1);
	auto* outcome = record.add_outcomes();
	outcome->set_session_id("session");
	outcome->mutable_observer()->set_value("red");
	outcome->set_stage(REQUEST_RECEIVED);

	RecordedSession decoded;
	ASSERT_TRUE(decoded.ParseFromString(record.SerializeAsString()));
	EXPECT_EQ(decoded.simulation().units(0).identity().value(), "truth-private");
	EXPECT_EQ(decoded.perspectives_size(), 2);
	EXPECT_EQ(decoded.associations_size(), 2);
	EXPECT_EQ(decoded.decisions_size(), 1);
	EXPECT_EQ(decoded.outcomes_size(), 1);
	ASSERT_TRUE(decoded.simulation().units(0).has_kinematics());
	EXPECT_FALSE(decoded.simulation().units(0).has_air_operations());
	EXPECT_FALSE(decoded.perspectives(0).units(0).has_aircraft());

	ProviderObservation publicView;
	publicView.mutable_snapshot()->set_session_id("session");
	EXPECT_EQ(publicView.GetDescriptor()->FindFieldByName("snapshot")->message_type()->name(), "ObserverSnapshot");
}
TEST(Codec, ObserverSnapshotRequiresCanonicalSchemaVersion) {
	World::ObserverSnapshot value;
	value.Observer = {"observer"};
	const auto wire = Gateway::Encode(value);
	EXPECT_EQ(wire.schema_version(), 4);

	auto obsolete = wire;
	obsolete.set_schema_version(3);
	EXPECT_THROW(Gateway::Decode(obsolete), std::invalid_argument);
}
