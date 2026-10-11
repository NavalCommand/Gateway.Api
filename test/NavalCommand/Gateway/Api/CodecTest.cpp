#include <NavalCommand/Gateway/Api/Codec.h>
#include <NavalCommand/Orders/Api/RouteCommand.h>
#include <gtest/gtest.h>
using namespace NavalCommand;
using namespace World::Api;
using Orders::Api::OwnUnitId;

TEST(Codec, UnifiedComponentsRoundtrip) {
	UnitInstance own;
	own.Identity = OwnUnitId{"own"};
	own.Display.Name = Knowledge<std::string>::Known("Aircraft");
	own.Display.TypeId = Knowledge<std::string>::Estimated("helicopter", "classification");
	own.Kinematics.Position = Knowledge<PositionObservation>::Known({{0, 0, 0}, PositionObservation::Uncertainty{100, 50, 0}});
	own.Kinematics.SpeedMetersPerSecond = Knowledge<double>::Known(0);
	own.Navigation = NavigationState{Knowledge<bool>::Known(true), Knowledge<std::vector<Orders::Api::RoutePoint>>::Known({{1, 2, "destination"}})};
	own.OrderEligibility =
		OrderEligibilityState{{ObservedCollection<Orders::Api::Eligibility>::Coverage::Complete, {{"replace_route", Orders::Api::Availability::Executable, "", "replace"}}}};
	own.Aircraft = AircraftState{Knowledge<bool>::Known(false)};
	own.Observation.SampledAtSeconds = Knowledge<double>::Known(42);
	EquipmentInstance equipment;
	equipment.TypeId = Knowledge<std::string>::Known("missile");
	equipment.Active = Knowledge<bool>::Estimated(false, "emitter");
	equipment.OperationalReadiness = Knowledge<EquipmentInstance::Readiness>::Known(EquipmentInstance::Readiness::Ready);
	equipment.DamageState = Knowledge<EquipmentInstance::Damage>::Estimated(EquipmentInstance::Damage::Damaged, "imagery");
	equipment.Resources = {{ResourceQuantity::Kind::Ammunition, Knowledge<double>::Known(0)}, {ResourceQuantity::Kind::JamChannels, {}}};
	equipment.Physical = PhysicalTraits{PhysicalTraits::Arc{0, 1}, 0};
	own.Equipment = {ObservedCollection<EquipmentInstance>::Coverage::Partial, {equipment}};
	UnitInstance track;
	track.Identity = TrackId{"own"};
	track.Display.TypeId = Knowledge<std::string>::Known("helicopter");
	track.Kinematics.Position = Knowledge<PositionObservation>::Estimated({{1, 2, {}}}, "observer");
	track.Kinematics.SpeedMetersPerSecond = Knowledge<double>::Estimated(0, "observer-motion");
	track.Observation.SampledAtSeconds = Knowledge<double>::Estimated(40, "contact-age");
	track.Observation.Provenance = Knowledge<std::string>::Known("observer");
	View view;
	view.Instances = {own, track};
	auto wire = Gateway::Api::Encode(view);
	auto decoded = Gateway::Api::Decode(wire);
	ASSERT_EQ(decoded.Instances.size(), 2);
	auto &restored = decoded.Instances[0];
	EXPECT_EQ(std::get<OwnUnitId>(restored.Identity).Value, "own");
	EXPECT_EQ(std::get<TrackId>(decoded.Instances[1].Identity).Value, "own");
	EXPECT_EQ(restored.Display.Name.Value(), "Aircraft");
	EXPECT_TRUE(restored.Display.TypeId.IsEstimated());
	EXPECT_EQ(restored.Display.TypeId.Source(), "classification");
	EXPECT_EQ(restored.Kinematics.Position.Value().ErrorEllipse->MajorAxisMeters, 100);
	EXPECT_EQ(restored.Kinematics.Position.Value().Coordinates.altitudeMeters, 0);
	EXPECT_EQ(restored.Kinematics.SpeedMetersPerSecond.Value(), 0);
	EXPECT_TRUE(restored.Navigation->Inspected.Value());
	EXPECT_EQ(restored.Navigation->PlannedRoute.Value()[0].label, "destination");
	EXPECT_EQ(restored.OrderEligibility->Orders.Items()[0].kind, "replace_route");
	EXPECT_FALSE(restored.Aircraft->Airborne.Value());
	EXPECT_EQ(restored.Observation.SampledAtSeconds.Value(), 42);
	EXPECT_EQ(restored.Equipment.GetCoverage(), ObservedCollection<EquipmentInstance>::Coverage::Partial);
	auto &restoredEquipment = restored.Equipment.Items()[0];
	EXPECT_TRUE(restoredEquipment.Active.IsEstimated());
	EXPECT_FALSE(restoredEquipment.Active.Value());
	EXPECT_EQ(restoredEquipment.OperationalReadiness.Value(), EquipmentInstance::Readiness::Ready);
	EXPECT_EQ(restoredEquipment.DamageState.Source(), "imagery");
	EXPECT_EQ(restoredEquipment.Resources[0].Amount.Value(), 0);
	EXPECT_FALSE(restoredEquipment.Resources[1].Amount.HasValue());
	EXPECT_EQ(restoredEquipment.Physical->OrientationRadians, 0);
	EXPECT_TRUE(decoded.Instances[1].Kinematics.Position.IsEstimated());
	EXPECT_EQ(decoded.Instances[1].Kinematics.Position.Source(), "observer");
	EXPECT_TRUE(decoded.Instances[1].Kinematics.SpeedMetersPerSecond.IsEstimated());
	EXPECT_EQ(decoded.Instances[1].Observation.Provenance.Value(), "observer");
	EXPECT_FALSE(decoded.Instances[1].OrderEligibility);
	EXPECT_EQ(decoded.Instances[1].Equipment.GetCoverage(), ObservedCollection<EquipmentInstance>::Coverage::Unknown);
	EXPECT_EQ(Gateway::Api::Encode(decoded).SerializeAsString(), wire.SerializeAsString());
}

TEST(Codec, KnowledgeAndCoveragePresence) {
	for (auto coverage : {ObservedCollection<EquipmentInstance>::Coverage::Unknown,
						  ObservedCollection<EquipmentInstance>::Coverage::Complete,
						  ObservedCollection<EquipmentInstance>::Coverage::Partial}) {
		View view;
		UnitInstance unit;
		unit.Identity = OwnUnitId{"own"};
		unit.Equipment = {coverage, {}};
		unit.Navigation = NavigationState{Knowledge<bool>::Known(false), Knowledge<std::vector<Orders::Api::RoutePoint>>::Estimated({}, "route-source")};
		unit.Aircraft = AircraftState{};
		view.Instances.push_back(unit);
		auto wire = Gateway::Api::Encode(view);
		auto decoded = Gateway::Api::Decode(wire);
		EXPECT_EQ(decoded.Instances[0].Equipment.GetCoverage(), coverage);
		EXPECT_FALSE(decoded.Instances[0].Kinematics.Position.HasValue());
		EXPECT_FALSE(decoded.Instances[0].Navigation->Inspected.Value());
		EXPECT_TRUE(decoded.Instances[0].Navigation->PlannedRoute.IsEstimated());
		EXPECT_TRUE(decoded.Instances[0].Navigation->PlannedRoute.Value().empty());
		EXPECT_FALSE(decoded.Instances[0].Aircraft->Airborne.HasValue());
		wire.mutable_instances(0)->mutable_equipment()->add_items();
		if (coverage == ObservedCollection<EquipmentInstance>::Coverage::Unknown) {
			EXPECT_THROW(Gateway::Api::Decode(wire), std::invalid_argument);
		}
	}
	Gateway::Api::Wire::WorldSnapshot malformed;
	auto entity = malformed.add_instances();
	EXPECT_THROW(Gateway::Api::Decode(malformed), std::invalid_argument);
	entity->mutable_subject()->set_track_id("contact");
	entity->mutable_kinematics()->mutable_position()->mutable_known();
	EXPECT_THROW(Gateway::Api::Decode(malformed), std::invalid_argument);
	entity->mutable_kinematics()->clear_position();
	entity->mutable_equipment()->set_coverage(static_cast<Gateway::Api::Wire::CollectionCoverage>(99));
	EXPECT_THROW(Gateway::Api::Decode(malformed), std::invalid_argument);
}

TEST(Codec, OptionalZeroAndUnknownEnum) {
	navalcommand::gateway::v1::OrderResult wireValue;
	wireValue.set_displacement_m(0);
	wireValue.set_stage(static_cast<navalcommand::gateway::v1::Stage>(99));
	auto roundtripValue = Gateway::Api::Encode(Gateway::Api::Decode(wireValue));
	EXPECT_TRUE(roundtripValue.has_displacement_m());
	EXPECT_EQ(roundtripValue.stage(), 99);
}

TEST(Codec, RouteModesAndReceipts) {
	using namespace Orders::Api;
	Request idle{"session", "idle", {"unit"}, 7, RouteCommand{62, -10}};
	EXPECT_EQ(Gateway::Api::Encode(idle).waypoint().route_mode(), 0);
	idle.routeCommand->routeMode = RouteMode::AppendWaypoint;
	auto decodedCommand = Gateway::Api::Decode(Gateway::Api::Encode(idle));
	ASSERT_TRUE(decodedCommand.routeCommand);
	EXPECT_EQ(decodedCommand.routeCommand->routeMode, RouteMode::AppendWaypoint);
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

TEST(Codec, FacilityLaunchAndGroundedPresence) {
	Orders::Api::Request request{"session", "launch", {"facility"}, 1};
	request.facilityLaunch = Orders::Api::FacilityLaunch{"class", "squadron", "loadout", 1};
	auto decoded = Gateway::Api::Decode(Gateway::Api::Encode(request));
	ASSERT_TRUE(decoded.facilityLaunch);
	EXPECT_FALSE(decoded.routeCommand);
	EXPECT_EQ(decoded.facilityLaunch->loadout, "loadout");
	EXPECT_EQ(decoded.facilityLaunch->count, 1);
	World::Api::View view;
	UnitInstance grounded;
	grounded.Identity = OwnUnitId{"spawn"};
	grounded.Aircraft = AircraftState{Knowledge<bool>::Known(false)};
	view.Instances.push_back(grounded);
	view.facilities.push_back({"facility", {{"class", "squadron", 1, {{"loadout", Orders::Api::Availability::Blocked, "resources_unavailable"}}}}});
	auto wire = Gateway::Api::Encode(view);
	EXPECT_TRUE(wire.instances(0).aircraft().airborne().has_known());
	EXPECT_FALSE(wire.instances(0).aircraft().airborne().known());
	EXPECT_EQ(Gateway::Api::Decode(wire).facilities[0].stocks[0].launchOptions[0].reason, "resources_unavailable");
	Orders::Api::Result spawned{"session", "launch", Orders::Api::Stage::AircraftSpawned, "spawned"};
	spawned.spawnedEntityId = "spawn";
	EXPECT_EQ(Gateway::Api::Decode(Gateway::Api::Encode(spawned)).spawnedEntityId, "spawn");
}

TEST(Codec, EstimatedScalarRequiresAValue) {
	Gateway::Api::Wire::WorldSnapshot malformed;
	auto entity = malformed.add_instances();
	entity->mutable_subject()->set_own_unit_id("own");
	entity->mutable_kinematics()->mutable_speed_mps()->mutable_estimated()->set_source("source");
	EXPECT_THROW(Gateway::Api::Decode(malformed), std::invalid_argument);
}

TEST(Codec, TrackSubjectsCannotBecomeCommands) {
	Gateway::Api::Wire::OrderRequest request;
	request.mutable_subject()->set_track_id("own");
	EXPECT_THROW(Gateway::Api::Decode(request), std::invalid_argument);
}
