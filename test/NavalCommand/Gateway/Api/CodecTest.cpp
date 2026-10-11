#include <NavalCommand/Gateway/Api/Codec.h>
#include <gtest/gtest.h>
#include <type_traits>

namespace Physics = NavalCommand::Physics::Api;
namespace Catalog = NavalCommand::Catalog::Api;
namespace Units = NavalCommand::Units::Api;
namespace Orders = NavalCommand::Orders::Api;
namespace World = NavalCommand::World::Api;
namespace Gateway = NavalCommand::Gateway::Api;
template <class T> using Knowledge = NavalCommand::Uncertainty::Api::Knowledge<T>;
template <class T> using Collection = NavalCommand::Uncertainty::Api::ObservedCollection<T>;

TEST(Codec, UnifiedIdentityStrongValuesAndIndependentOverlay) {
	static_assert(std::is_same_v<decltype(Units::UnitInstance::Identity), decltype(Orders::Request::Unit)>);
	Units::UnitInstance own;
	own.Identity = {"own"};
	own.Display.Name = Knowledge<std::string>::Known("Aircraft");
	own.Display.TypeId = Knowledge<Catalog::UnitTypeId>::Estimated({"helicopter"}, "classification");
	own.Kinematics.Position = Knowledge<Units::PositionObservation>::Known(
		{{{0}, {0}, Physics::Meters{0}}, Units::PositionObservation::Uncertainty{{100}, {50}, {0}}});
	own.Kinematics.Speed = Knowledge<Physics::MetersPerSecond>::Known({0});
	own.Kinematics.Heading = Knowledge<Physics::Radians>::Known({0});
	own.Kinematics.CourseOverGround = Knowledge<Physics::Radians>::Estimated({1}, "observed-motion");
	own.Navigation = Units::NavigationState{Knowledge<std::vector<Units::RoutePoint>>::Known({{{1}, {2}, "destination"}})};
	own.Aircraft = Units::AircraftState{Knowledge<bool>::Known(false)};
	own.Observation.MeasuredAt = Knowledge<Physics::Seconds>::Known({42});
	Units::EquipmentInstance equipment;
	equipment.TypeId = Knowledge<Catalog::EquipmentTypeId>::Known({"missile"});
	equipment.Active = Knowledge<bool>::Estimated(false, "emitter");
	equipment.OperationalReadiness = Knowledge<Units::EquipmentInstance::Readiness>::Known(Units::EquipmentInstance::Readiness::Ready);
	equipment.DamageState = Knowledge<Units::EquipmentInstance::Damage>::Estimated(Units::EquipmentInstance::Damage::Damaged, "imagery");
	equipment.Resources = {Collection<Units::EquipmentInstance::Resource>::Coverage::Partial,
		{{{"missiles"}, Knowledge<Catalog::ResourceValue>::Known(uint64_t{0})}, {{"channels"}, {}},
		 {{"fuel"}, Knowledge<Catalog::ResourceValue>::Estimated(12.5, "tank-gauge")}}};
	own.Equipment = {Collection<Units::EquipmentInstance>::Coverage::Partial, {equipment}};
	Units::UnitInstance contact;
	contact.Identity = {"contact"};
	contact.Kinematics.Position = Knowledge<Units::PositionObservation>::Estimated({{{1}, {2}, {}}}, "observer");
	Gateway::ProviderObservation observation;
	observation.Snapshot.SessionId = "session";
	observation.Snapshot.CapturedAt = {100};
	observation.Snapshot.Version = 7;
	observation.Snapshot.Instances = {own, contact};
	observation.Snapshot.Memberships = {Collection<World::UnitMembership>::Coverage::Partial,
		{{own.Identity, Knowledge<std::string>::Known("friendly"), Knowledge<std::string>::Known("group")}}};
	observation.OrderAvailability.push_back({own.Identity, 7, {Collection<Orders::Eligibility>::Coverage::Complete,
		{{Orders::OrderKind::ReplaceRoute, Orders::Availability::Executable, ""}}}});
	const auto wire = Gateway::Encode(observation);
	const auto decoded = Gateway::Decode(wire);
	ASSERT_EQ(decoded.Snapshot.Instances.size(), 2);
	const auto &restored = decoded.Snapshot.Instances[0];
	EXPECT_EQ(restored.Identity.Value, "own");
	EXPECT_EQ(decoded.Snapshot.Instances[1].Identity.Value, "contact");
	EXPECT_EQ(restored.Display.TypeId.Value().Value, "helicopter");
	EXPECT_EQ(restored.Display.TypeId.Source(), "classification");
	EXPECT_EQ(restored.Kinematics.Position.Value().ErrorEllipse->MajorAxis.Value, 100);
	EXPECT_EQ(restored.Kinematics.Position.Value().Coordinates.Altitude->Value, 0);
	EXPECT_EQ(restored.Kinematics.Speed.Value().Value, 0);
	EXPECT_EQ(restored.Kinematics.Heading.Value().Value, 0);
	EXPECT_EQ(restored.Kinematics.CourseOverGround.Value().Value, 1);
	EXPECT_EQ(restored.Navigation->PlannedRoute.Value()[0].Label, "destination");
	EXPECT_FALSE(restored.Aircraft->Airborne.Value());
	EXPECT_EQ(restored.Observation.MeasuredAt.Value().Value, 42);
	const auto &restoredEquipment = restored.Equipment.Items()[0];
	EXPECT_FALSE(restoredEquipment.Active.Value());
	EXPECT_TRUE(restoredEquipment.Active.IsEstimated());
	EXPECT_EQ(restoredEquipment.DamageState.Source(), "imagery");
	EXPECT_EQ(std::get<uint64_t>(restoredEquipment.Resources.Items()[0].Amount.Value()), 0);
	EXPECT_FALSE(restoredEquipment.Resources.Items()[1].Amount.HasValue());
	EXPECT_EQ(std::get<double>(restoredEquipment.Resources.Items()[2].Amount.Value()), 12.5);
	EXPECT_EQ(restoredEquipment.Resources.GetCoverage(), Collection<Units::EquipmentInstance::Resource>::Coverage::Partial);
	EXPECT_FALSE(decoded.Snapshot.Instances[1].Observation.MeasuredAt.HasValue());
	EXPECT_EQ(decoded.Snapshot.CapturedAt.Value, 100);
	EXPECT_EQ(decoded.Snapshot.Memberships.Items()[0].Unit.Value, "own");
	EXPECT_EQ(decoded.Snapshot.Memberships.Items()[0].TaskForce.Value(), "group");
	EXPECT_EQ(decoded.OrderAvailability[0].Unit, restored.Identity);
	EXPECT_EQ(decoded.OrderAvailability[0].Orders.Items()[0].Kind, Orders::OrderKind::ReplaceRoute);
	EXPECT_EQ(Gateway::Encode(decoded).SerializeAsString(), wire.SerializeAsString());
}

TEST(Codec, UnknownPartialAndCompleteEmptyCollectionsRemainDistinct) {
	for (auto coverage : {Collection<Units::EquipmentInstance>::Coverage::Unknown,
		 Collection<Units::EquipmentInstance>::Coverage::Partial, Collection<Units::EquipmentInstance>::Coverage::Complete}) {
		World::View view;
		Units::UnitInstance unit;
		unit.Identity = {"unit"};
		unit.Equipment = {coverage, {}};
		unit.Navigation = Units::NavigationState{Knowledge<std::vector<Units::RoutePoint>>::Known({})};
		view.Instances.push_back(unit);
		const auto decoded = Gateway::Decode(Gateway::Encode(view));
		EXPECT_EQ(decoded.Instances[0].Equipment.GetCoverage(), coverage);
		EXPECT_TRUE(decoded.Instances[0].Navigation->PlannedRoute.IsKnown());
		EXPECT_TRUE(decoded.Instances[0].Navigation->PlannedRoute.Value().empty());
	}
	for (auto coverage : {Collection<Units::EquipmentInstance::Resource>::Coverage::Unknown,
		 Collection<Units::EquipmentInstance::Resource>::Coverage::Partial, Collection<Units::EquipmentInstance::Resource>::Coverage::Complete}) {
		World::View view;
		Units::UnitInstance unit;
		unit.Identity = {"unit"};
		Units::EquipmentInstance equipment;
		equipment.Resources = {coverage, {}};
		unit.Equipment = {Collection<Units::EquipmentInstance>::Coverage::Complete, {equipment}};
		unit.Navigation = Units::NavigationState{};
		view.Instances.push_back(unit);
		const auto decoded = Gateway::Decode(Gateway::Encode(view));
		EXPECT_EQ(decoded.Instances[0].Equipment.Items()[0].Resources.GetCoverage(), coverage);
		EXPECT_FALSE(decoded.Instances[0].Navigation->PlannedRoute.HasValue());
	}
}

TEST(Codec, AircraftInventoryAndLaunchEligibilityAreSeparate) {
	Gateway::ProviderObservation observation;
	Units::UnitInstance carrier;
	carrier.Identity = {"carrier"};
	carrier.AirOperations.emplace();
	carrier.AirOperations->Inventory = {Collection<Units::AircraftInventoryEntry>::Coverage::Complete, {
		{{"aircraft"}, "squadron", Knowledge<uint32_t>::Known(0), {{"loadout"}}}, {{"unknown-aircraft"}, "other", {}, {}}}};
	carrier.AirOperations->OnboardCount = Knowledge<uint32_t>::Known(0);
	carrier.AirOperations->MeasuredAt = Knowledge<Physics::Seconds>::Known({5});
	observation.Snapshot.Instances.push_back(carrier);
	observation.Snapshot.Version = 1;
	Orders::FacilityLaunch launch{{{"aircraft"}, "squadron", {"loadout"}}, 1};
	observation.OrderAvailability.push_back({carrier.Identity, 1, {Collection<Orders::Eligibility>::Coverage::Complete, {
		{Orders::OrderKind::LaunchAircraft, Orders::Availability::Blocked, "resources_unavailable", launch}}}});
	const auto decoded = Gateway::Decode(Gateway::Encode(observation));
	const auto &air = *decoded.Snapshot.Instances[0].AirOperations;
	EXPECT_EQ(air.Inventory.Items()[0].UnreservedCount.Value(), 0);
	EXPECT_FALSE(air.Inventory.Items()[1].UnreservedCount.HasValue());
	EXPECT_EQ(air.OnboardCount.Value(), 0);
	EXPECT_FALSE(air.ReservedCount.HasValue());
	EXPECT_EQ(decoded.OrderAvailability[0].Orders.Items()[0].Reason, "resources_unavailable");
	Orders::Request request{"session", "launch", carrier.Identity, 1, launch};
	const auto command = Gateway::Decode(Gateway::Encode(request));
	EXPECT_EQ(std::get<Orders::FacilityLaunch>(command.Payload).Aircraft.Loadout.Value, "loadout");
	Orders::Result spawned{"session", "launch", Orders::Stage::AircraftSpawned, "spawned"};
	spawned.SpawnedUnit = Units::UnitId{"spawn"};
	EXPECT_EQ(Gateway::Decode(Gateway::Encode(spawned)).SpawnedUnit->Value, "spawn");
}

TEST(Codec, DirectPayloadRouteModesAndReceipts) {
	Orders::Request request{"session", "id", {"unit"}, 7, Orders::RouteCommand{{62}, {-10}, Orders::RouteMode::AppendWaypoint}};
	const auto command = Gateway::Decode(Gateway::Encode(request));
	EXPECT_EQ(std::get<Orders::RouteCommand>(command.Payload).Mode, Orders::RouteMode::AppendWaypoint);
	Orders::Result result{"session", "id", Orders::Stage::RouteMutated, "sequence_verified"};
	result.RouteBefore = {{{62}, {-10}, "A"}};
	result.RouteAfter = {{{62}, {-10}, "A"}, {{63}, {-11}, "B"}};
	result.Mode = Orders::RouteMode::AppendWaypoint;
	const auto decoded = Gateway::Decode(Gateway::Encode(result));
	ASSERT_EQ(decoded.RouteAfter.size(), 2);
	EXPECT_EQ(decoded.RouteAfter[1].Label, "B");
	EXPECT_EQ(decoded.RouteBefore[0].Latitude.Value, 62);
	EXPECT_EQ(decoded.Stage, Orders::Stage::RouteMutated);
}

TEST(Codec, OptionalZeroAndUnknownLifecycleEnum) {
	Gateway::Wire::OrderResult wire;
	wire.set_displacement_m(0);
	wire.set_stage(static_cast<Gateway::Wire::Stage>(99));
	const auto restored = Gateway::Encode(Gateway::Decode(wire));
	EXPECT_TRUE(restored.has_displacement_m());
	EXPECT_EQ(restored.displacement_m(), 0);
	EXPECT_EQ(restored.stage(), 99);
}

TEST(Codec, RejectsMalformedKnowledgeCoverageIdentityAndPayload) {
	World::View view;
	Units::UnitInstance unit;
	unit.Identity = {"unit"};
	view.Instances.push_back(unit);
	auto wire = Gateway::Encode(view);
	wire.mutable_instances(0)->mutable_kinematics()->mutable_speed_mps()->mutable_estimated()->set_source("source");
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	wire = Gateway::Encode(view);
	wire.mutable_instances(0)->mutable_kinematics()->mutable_position()->mutable_known();
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	wire = Gateway::Encode(view);
	wire.mutable_instances(0)->mutable_equipment()->add_items();
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	wire = Gateway::Encode(view);
	wire.mutable_instances(0)->mutable_equipment()->set_coverage(static_cast<Gateway::Wire::CollectionCoverage>(99));
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	wire = Gateway::Encode(view);
	wire.mutable_instances(0)->mutable_unit_id()->clear_value();
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	Gateway::Wire::OrderRequest request;
	request.mutable_unit_id()->set_value("contact");
	EXPECT_THROW(Gateway::Decode(request), std::invalid_argument);
	request.mutable_waypoint();
	// Identity decoding deliberately grants no authority; the executing provider validates it.
	EXPECT_EQ(Gateway::Decode(request).Unit.Value, "contact");
	request.mutable_waypoint()->set_route_mode(static_cast<Gateway::Wire::RouteMode>(99));
	// Preserve unsupported wire modes for the provider's operational rejection receipt.
	EXPECT_EQ(static_cast<int>(std::get<Orders::RouteCommand>(Gateway::Decode(request).Payload).Mode), 99);
	Gateway::Wire::OrderEligibilityState availability;
	availability.set_coverage(Gateway::Wire::COVERAGE_COMPLETE);
	availability.add_items()->set_kind(static_cast<Gateway::Wire::OrderKind>(99));
	EXPECT_THROW(Gateway::Decode(availability), std::invalid_argument);
}

TEST(Codec, RejectsInconsistentProviderOverlays) {
	Gateway::ProviderObservation observation;
	observation.Snapshot.SessionId = "session";
	observation.Snapshot.Version = 7;
	Units::UnitInstance unit;
	unit.Identity = {"unit"};
	observation.Snapshot.Instances.push_back(unit);
	observation.OrderAvailability.push_back({unit.Identity, 7, {}});
	const auto valid = Gateway::Encode(observation);
	auto wire = valid;
	wire.mutable_order_availability(0)->set_session_id("other");
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	wire = valid;
	wire.mutable_order_availability(0)->mutable_unit_id()->set_value("absent");
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	wire = valid;
	wire.mutable_order_availability(0)->set_based_on_version(8);
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	wire = valid;
	*wire.add_order_availability() = wire.order_availability(0);
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	wire = valid;
	*wire.mutable_snapshot()->add_instances() = wire.snapshot().instances(0);
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
	wire = valid;
	wire.mutable_snapshot()->mutable_session()->set_schema_version(2);
	EXPECT_THROW(Gateway::Decode(wire), std::invalid_argument);
}
