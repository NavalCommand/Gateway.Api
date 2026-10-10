#include <NavalCommand/Gateway/Api/Codec.h>
namespace NavalCommand::Gateway::Api {
static void EncodeRoute(Wire::RoutePoint &wirePoint, const Orders::Api::RoutePoint &point) {
	wirePoint.set_latitude_deg(point.latitudeDeg);
	wirePoint.set_longitude_deg(point.longitudeDeg);
	wirePoint.set_label(point.label);
}
static Orders::Api::RoutePoint DecodeRoute(const Wire::RoutePoint &point) {
	return {point.latitude_deg(), point.longitude_deg(), point.label()};
}
static void EncodePosition(Wire::GeoPoint &wirePosition, const World::Api::Position &domainValue) {
	wirePosition.set_latitude_deg(domainValue.latitudeDeg);
	wirePosition.set_longitude_deg(domainValue.longitudeDeg);
	if (domainValue.altitudeMeters) {
		wirePosition.set_altitude_m(*domainValue.altitudeMeters);
	}
}
static World::Api::Position DecodePosition(const Wire::GeoPoint &wirePosition) {
	return {wirePosition.latitude_deg(), wirePosition.longitude_deg(), wirePosition.has_altitude_m() ? std::optional<double>{wirePosition.altitude_m()} : std::nullopt};
}
Wire::WorldSnapshot Encode(const World::Api::View &domainValue) {
	Wire::WorldSnapshot wireValue;
	auto wireSession = wireValue.mutable_session();
	wireSession->set_schema_version(1);
	wireSession->set_session_id(domainValue.sessionId);
	wireSession->set_observer_id(domainValue.observerId);
	wireSession->set_ready(domainValue.ready);
	wireValue.set_observation_version(domainValue.version);
	wireValue.set_operational_seconds(domainValue.operationalSeconds);
	wireValue.set_truncated(domainValue.truncated);
	for (auto &domainUnit : domainValue.units) {
		auto wireEntity = wireValue.add_own_units();
		wireEntity->set_id(domainUnit.id);
		wireEntity->set_name(domainUnit.name);
		EncodePosition(*wireEntity->mutable_position(), domainUnit.position);
		if (domainUnit.speedMetersPerSecond) {
			wireEntity->set_speed_mps(*domainUnit.speedMetersPerSecond);
		}
		wireEntity->set_sampled_at_seconds(domainUnit.sampledAtSeconds);
		wireEntity->set_route_inspected(domainUnit.routeInspected);
		if (domainUnit.airborne) {
			wireEntity->set_airborne(*domainUnit.airborne);
		}
		for (const auto &point : domainUnit.plannedRoute) {
			EncodeRoute(*wireEntity->add_planned_route(), point);
		}
		for (auto &eligibility : domainUnit.orders) {
			auto wireEligibility = wireEntity->add_orders();
			wireEligibility->set_kind(eligibility.kind);
			wireEligibility->set_availability(static_cast<Wire::Availability>(eligibility.availability));
			wireEligibility->set_reason(eligibility.reason);
			wireEligibility->set_argument_semantics(eligibility.arguments);
		}
	}
	for (auto &domainTrack : domainValue.tracks) {
		auto wireEntity = wireValue.add_tracks();
		wireEntity->set_id(domainTrack.id);
		EncodePosition(*wireEntity->mutable_detected_position(), domainTrack.detectedPosition);
		wireEntity->set_sampled_at_seconds(domainTrack.sampledAtSeconds);
		wireEntity->set_provenance(domainTrack.provenance);
	}
	for (auto &domainFacility : domainValue.facilities) {
		auto wireEntity = wireValue.add_facilities();
		wireEntity->set_entity_id(domainFacility.entityId);
		wireEntity->set_pending_operations(domainFacility.pendingOperations);
		wireEntity->set_sampled_at_seconds(domainFacility.sampledAtSeconds);
		if (domainFacility.onboardCount) {
			wireEntity->set_onboard_count(*domainFacility.onboardCount);
		}
		if (domainFacility.reservedCount) {
			wireEntity->set_reserved_count(*domainFacility.reservedCount);
		}
		if (domainFacility.cooldownCount) {
			wireEntity->set_cooldown_count(*domainFacility.cooldownCount);
		}
		for (auto &stock : domainFacility.stocks) {
			auto wireStock = wireEntity->add_stocks();
			wireStock->set_class_key(stock.classKey);
			wireStock->set_squadron(stock.squadron);
			wireStock->set_unreserved_count(stock.unreservedCount);
			for (const auto &option : stock.launchOptions) {
				auto wireOption = wireStock->add_launch_options();
				wireOption->set_loadout(option.loadout);
				wireOption->set_availability(static_cast<Wire::Availability>(option.availability));
				wireOption->set_reason(option.reason);
			}
		}
	}
	return wireValue;
}
World::Api::View Decode(const Wire::WorldSnapshot &wireValue) {
	World::Api::View domainValue;
	domainValue.sessionId = wireValue.session().session_id();
	domainValue.observerId = wireValue.session().observer_id();
	domainValue.ready = wireValue.session().ready();
	domainValue.version = wireValue.observation_version();
	domainValue.operationalSeconds = wireValue.operational_seconds();
	domainValue.truncated = wireValue.truncated();
	for (auto &wireEntity : wireValue.own_units()) {
		World::Api::Unit domainUnit;
		domainUnit.id = wireEntity.id();
		domainUnit.name = wireEntity.name();
		domainUnit.position = DecodePosition(wireEntity.position());
		if (wireEntity.has_speed_mps()) {
			domainUnit.speedMetersPerSecond = wireEntity.speed_mps();
		}
		domainUnit.sampledAtSeconds = wireEntity.sampled_at_seconds();
		domainUnit.routeInspected = wireEntity.route_inspected();
		if (wireEntity.has_airborne()) {
			domainUnit.airborne = wireEntity.airborne();
		}
		for (const auto &point : wireEntity.planned_route()) {
			domainUnit.plannedRoute.push_back(DecodeRoute(point));
		}
		for (auto &eligibility : wireEntity.orders()) {
			domainUnit.orders.push_back(
				{eligibility.kind(), static_cast<Orders::Api::Availability>(eligibility.availability()), eligibility.reason(), eligibility.argument_semantics()});
		}
		domainValue.units.push_back(std::move(domainUnit));
	}
	for (auto &wireEntity : wireValue.tracks()) {
		domainValue.tracks.push_back({wireEntity.id(), DecodePosition(wireEntity.detected_position()), wireEntity.sampled_at_seconds(), wireEntity.provenance()});
	}
	for (auto &wireEntity : wireValue.facilities()) {
		World::Api::Facility domainFacility;
		domainFacility.entityId = wireEntity.entity_id();
		domainFacility.pendingOperations = wireEntity.pending_operations();
		domainFacility.sampledAtSeconds = wireEntity.sampled_at_seconds();
		if (wireEntity.has_onboard_count()) {
			domainFacility.onboardCount = wireEntity.onboard_count();
		}
		if (wireEntity.has_reserved_count()) {
			domainFacility.reservedCount = wireEntity.reserved_count();
		}
		if (wireEntity.has_cooldown_count()) {
			domainFacility.cooldownCount = wireEntity.cooldown_count();
		}
		for (auto &stock : wireEntity.stocks()) {
			World::Api::Stock domainStock{stock.class_key(), stock.squadron(), stock.unreserved_count()};
			for (const auto &option : stock.launch_options()) {
				domainStock.launchOptions.push_back({option.loadout(), static_cast<Orders::Api::Availability>(option.availability()), option.reason()});
			}
			domainFacility.stocks.push_back(std::move(domainStock));
		}
		domainValue.facilities.push_back(std::move(domainFacility));
	}
	return domainValue;
}
Wire::OrderRequest Encode(const Orders::Api::Request &domainValue) {
	Wire::OrderRequest wireValue;
	wireValue.set_session_id(domainValue.sessionId);
	wireValue.set_request_id(domainValue.requestId);
	wireValue.set_entity_id(domainValue.entityId);
	wireValue.set_based_on_version(domainValue.basedOnVersion);
	if (domainValue.facilityLaunch) {
		auto launch = wireValue.mutable_facility_launch();
		launch->set_class_key(domainValue.facilityLaunch->classKey);
		launch->set_squadron(domainValue.facilityLaunch->squadron);
		launch->set_loadout(domainValue.facilityLaunch->loadout);
		launch->set_count(domainValue.facilityLaunch->count);
	}
	if (domainValue.waypoint) {
		wireValue.mutable_waypoint()->set_latitude_deg(domainValue.waypoint->latitudeDeg);
		wireValue.mutable_waypoint()->set_longitude_deg(domainValue.waypoint->longitudeDeg);
		wireValue.mutable_waypoint()->set_route_mode(static_cast<Wire::RouteMode>(domainValue.waypoint->routeMode));
	}
	return wireValue;
}
Orders::Api::Request Decode(const Wire::OrderRequest &wireValue) {
	Orders::Api::Request domainValue{wireValue.session_id(), wireValue.request_id(), wireValue.entity_id(), wireValue.based_on_version()};
	if (wireValue.has_facility_launch()) {
		const auto &launch = wireValue.facility_launch();
		domainValue.facilityLaunch = Orders::Api::FacilityLaunch{launch.class_key(), launch.squadron(), launch.loadout(), launch.count()};
	}
	if (wireValue.has_waypoint()) {
		domainValue.waypoint = Orders::Api::Waypoint{
			wireValue.waypoint().latitude_deg(), wireValue.waypoint().longitude_deg(), static_cast<Orders::Api::RouteMode>(wireValue.waypoint().route_mode())};
	}
	return domainValue;
}
Wire::OrderResult Encode(const Orders::Api::Result &domainValue) {
	Wire::OrderResult wireValue;
	wireValue.set_session_id(domainValue.sessionId);
	wireValue.set_request_id(domainValue.requestId);
	wireValue.set_stage(static_cast<Wire::Stage>(domainValue.stage));
	wireValue.set_reason(domainValue.reason);
	wireValue.set_spawned_entity_id(domainValue.spawnedEntityId);
	wireValue.set_route_mode(static_cast<Wire::RouteMode>(domainValue.routeMode));
	for (const auto &point : domainValue.routeBefore) {
		EncodeRoute(*wireValue.add_route_before(), point);
	}
	for (const auto &point : domainValue.routeAfter) {
		EncodeRoute(*wireValue.add_route_after(), point);
	}
	wireValue.set_sampled_at_seconds(domainValue.sampledAtSeconds);
	if (domainValue.displacementMeters) {
		wireValue.set_displacement_m(*domainValue.displacementMeters);
	}
	return wireValue;
}
Orders::Api::Result Decode(const Wire::OrderResult &wireValue) {
	Orders::Api::Result domainValue{
		wireValue.session_id(), wireValue.request_id(), static_cast<Orders::Api::Stage>(wireValue.stage()), wireValue.reason(), wireValue.sampled_at_seconds()};
	domainValue.spawnedEntityId = wireValue.spawned_entity_id();
	domainValue.routeMode = static_cast<Orders::Api::RouteMode>(wireValue.route_mode());
	for (const auto &point : wireValue.route_before()) {
		domainValue.routeBefore.push_back(DecodeRoute(point));
	}
	for (const auto &point : wireValue.route_after()) {
		domainValue.routeAfter.push_back(DecodeRoute(point));
	}
	if (wireValue.has_displacement_m()) {
		domainValue.displacementMeters = wireValue.displacement_m();
	}
	return domainValue;
}
} // namespace NavalCommand::Gateway::Api
