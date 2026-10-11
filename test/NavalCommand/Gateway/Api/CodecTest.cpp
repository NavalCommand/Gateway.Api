#include <NavalCommand/Gateway/Api/Codec.h>
#include <navalcommand/gateway/v1/recording.pb.h>
#include <gtest/gtest.h>
#include <type_traits>
#include <stdexcept>
#include <string>
#include <variant>

namespace Physics = NavalCommand::Physics::Api;
namespace Catalog = NavalCommand::Catalog::Api;
namespace Observations = NavalCommand::Observations::Api;
namespace Orders = NavalCommand::Orders::Api;
namespace World = NavalCommand::World::Api;
namespace Gateway = NavalCommand::Gateway::Api;
template <class T> using Exact = NavalCommand::Uncertainty::Api::Exact<T>;

TEST(Codec, ObserverSnapshotsPreserveTwoStateAndPartialObservations) {
	static_assert(std::is_same_v<decltype(Observations::UnitObservation::Identity), Observations::ObservationId>);
	static_assert(std::is_same_v<decltype(World::ObserverSnapshot::Observer), Observations::ObserverId>);
	World::ObserverSnapshot snapshot;
	snapshot.SessionId = "session";
	snapshot.Observer = {"red"};
	snapshot.Ready = true;
	snapshot.Version = 9;
	snapshot.CapturedAt = {150};
	Observations::UnitObservation contact;
	contact.Identity = {"red-contact"};
	contact.Type = Exact<Catalog::UnitTypeId>::Known({"missile"});
	contact.Kinematics.Position = Observations::PartiallyLocalizedPosition{{Physics::Degrees{61}}, {},
		Observations::ObservedErrorEllipse{Physics::Meters{125}, Physics::Meters{50}, Physics::Radians{0}}};
	contact.Kinematics.Speed = Exact<Physics::MetersPerSecond>::Known({0});
	contact.Name = Exact<std::string>::Known("");
	contact.Contributors.push_back({{"source-observer"}, {{"radar"}}});
	contact.MeasuredAt = Exact<Physics::Seconds>::Known({125});
	snapshot.Units.push_back(contact);
	snapshot.Memberships.push_back({contact.Identity, Exact<World::FactionId>::Known({"blue"}), Exact<World::TaskForceId>::Unknown()});
	const auto decoded = Gateway::Decode(Gateway::Encode(snapshot));
	ASSERT_EQ(decoded.Units.size(), 1);
	EXPECT_TRUE(decoded.Units[0].Name.HasValue());
	EXPECT_EQ(decoded.Units[0].Name.Value(), "");
	EXPECT_EQ(decoded.Units[0].MeasuredAt.Value().Value, 125);
	EXPECT_EQ(decoded.Units[0].Kinematics.Speed.Value().Value, 0);
	const auto* partial = std::get_if<Observations::PartiallyLocalizedPosition>(&decoded.Units[0].Kinematics.Position);
	ASSERT_NE(partial, nullptr);
	EXPECT_TRUE(partial->Latitude.has_value());
	EXPECT_FALSE(partial->Longitude.has_value());
	EXPECT_EQ(partial->ErrorEllipse->MajorAxis.Value, 125);
	EXPECT_EQ(partial->ErrorEllipse->MinorAxis.Value, 50);
	ASSERT_EQ(decoded.Units[0].Contributors.size(), 1);
	EXPECT_EQ(decoded.Units[0].Contributors[0].Observer.Value, "source-observer");
	ASSERT_EQ(decoded.Units[0].Contributors[0].Sensors.size(), 1);
	EXPECT_EQ(decoded.Units[0].Contributors[0].Sensors[0].Value, "radar");
	ASSERT_EQ(decoded.Memberships.size(), 1);
	EXPECT_TRUE(decoded.Memberships[0].Faction.HasValue());
	EXPECT_FALSE(decoded.Memberships[0].TaskForce.HasValue());
	EXPECT_FALSE(decoded.Units[0].Aircraft.has_value());
	EXPECT_FALSE(decoded.Units[0].AirOperations.has_value());
	const auto bytes = Gateway::Encode(Gateway::ProviderObservation{snapshot, {}}).SerializeAsString();
	EXPECT_EQ(bytes.find("truth-private"), std::string::npos);
}

TEST(Codec, KnownZeroFalseAndEmptyRouteRemainDistinctFromUnknown) {
	World::ObserverSnapshot source;
	source.SessionId = "session";
	source.Observer = {"observer"};
	Observations::UnitObservation unit;
	unit.Identity = {"contact"};
	unit.Kinematics.Speed = Exact<Physics::MetersPerSecond>::Known({0});
	unit.Aircraft = Observations::AircraftObservation{Exact<bool>::Known(false)};
	unit.Kinematics.Position = Exact<Physics::Position>::Known({{61}, {-10}, Physics::Meters{0}});
	unit.Navigation = Observations::NavigationObservation{Exact<std::vector<Observations::ObservedRoutePoint>>::Known({})};
	source.Units.push_back(unit);
	const auto decoded = Gateway::Decode(Gateway::Encode(source));
	const auto& result = decoded.Units.front();
	EXPECT_TRUE(result.Kinematics.Speed.HasValue());
	EXPECT_EQ(result.Kinematics.Speed.Value().Value, 0);
	ASSERT_TRUE(result.Aircraft.has_value());
	EXPECT_TRUE(result.Aircraft->Airborne.HasValue());
	EXPECT_FALSE(result.Aircraft->Airborne.Value());
	const auto* exactPosition = std::get_if<Exact<Physics::Position>>(&result.Kinematics.Position);
	ASSERT_NE(exactPosition, nullptr);
	ASSERT_TRUE(exactPosition->HasValue());
	EXPECT_EQ(exactPosition->Value().Latitude.Value, 61);
	EXPECT_EQ(exactPosition->Value().Longitude.Value, -10);
	ASSERT_TRUE(result.Navigation.has_value());
	EXPECT_TRUE(result.Navigation->PlannedRoute.HasValue());
	EXPECT_TRUE(result.Navigation->PlannedRoute.Value().empty());
	EXPECT_TRUE(result.Equipment.empty());
}

TEST(Codec, OrdersCarryScopedHandlesAndExactlyOneTypedPayload) {
	Orders::Request request;
	request.SessionId = "session";
	request.RequestId = "request";
	request.Observer = {"red"};
	request.Observation = {"contact"};
	request.BasedOnVersion = 3;
	request.Payload = Orders::RouteCommand{{61}, {-10}, Orders::RouteMode::AppendWaypoint};
	const auto decoded = Gateway::Decode(Gateway::Encode(request));
	EXPECT_EQ(decoded.Observer.Value, "red");
	EXPECT_EQ(decoded.Observation.Value, "contact");
	EXPECT_EQ(std::get<Orders::RouteCommand>(decoded.Payload).Mode, Orders::RouteMode::AppendWaypoint);
	request.Payload = Orders::LaunchAircraft{{{"aircraft"}, "squadron", {"loadout"}}, 2};
	EXPECT_EQ(std::get<Orders::LaunchAircraft>(Gateway::Decode(Gateway::Encode(request)).Payload).Count, 2);
	Gateway::Wire::OrderRequest invalid;
	EXPECT_THROW(Gateway::Decode(invalid), std::invalid_argument);
}

TEST(Codec, OrderOverlayContainsOnlyReportedRowsAndValidatesScope) {
	World::ObserverSnapshot snapshot;
	snapshot.SessionId = "session";
	snapshot.Observer = {"red"};
	snapshot.Version = 2;
	Observations::UnitObservation contact;
	contact.Identity = {"contact"};
	snapshot.Units.push_back(contact);
	Orders::Eligibility row{Orders::OrderKind::ReplaceRoute, Orders::Availability::Executable, ""};
	Orders::OrderAvailability availability{{"red"}, {"contact"}, 2, {row}};
	Gateway::ProviderObservation provider{snapshot, {availability}};
	EXPECT_EQ(Gateway::Decode(Gateway::Encode(provider)).OrderAvailability.size(), 1);
	auto invalid = Gateway::Encode(provider);
	invalid.mutable_order_availability(0)->set_based_on_version(3);
	EXPECT_THROW(Gateway::Decode(invalid), std::invalid_argument);
}

TEST(Codec, PrivilegedRecordedSessionRemainsSeparateFromPublicEnvelope) {
	Gateway::Wire::RecordedSession recording;
	recording.mutable_simulation()->set_session_id("private-session");
	recording.mutable_simulation()->add_units()->mutable_identity()->set_value("truth-private");
	auto* observer = recording.add_perspectives();
	observer->set_schema_version(4);
	observer->mutable_observer()->set_value("observer");
	auto* association = recording.add_associations();
	association->mutable_observer()->set_value("observer");
	association->mutable_observation()->set_value("contact");
	association->mutable_truth()->set_value("truth-private");
	EXPECT_EQ(recording.simulation().units(0).identity().value(), "truth-private");
	Gateway::ProviderObservation publicObservation;
	publicObservation.Snapshot.SessionId = "public-session";
	publicObservation.Snapshot.Observer = {"public-observer"};
	const auto publicBytes = Gateway::Encode(publicObservation).SerializeAsString();
	EXPECT_EQ(publicBytes.find("truth-private"), std::string::npos);
}

TEST(Codec, ObserverSnapshotRequiresCanonicalSchemaVersion) {
	Gateway::Wire::ObserverSnapshot invalid;
	invalid.mutable_observer()->set_value("observer");
	EXPECT_THROW(Gateway::Decode(invalid), std::invalid_argument);
}
