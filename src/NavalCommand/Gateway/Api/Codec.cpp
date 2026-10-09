#include <NavalCommand/Gateway/Api/Codec.h>
namespace NavalCommand::Gateway::Api {
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
		for (auto &stock : domainFacility.stocks) {
			auto wireStock = wireEntity->add_stocks();
			wireStock->set_class_key(stock.classKey);
			wireStock->set_squadron(stock.squadron);
			wireStock->set_unreserved_count(stock.unreservedCount);
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
		for (auto &stock : wireEntity.stocks()) {
			domainFacility.stocks.push_back({stock.class_key(), stock.squadron(), stock.unreserved_count()});
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
	if (domainValue.waypoint) {
		wireValue.mutable_waypoint()->set_latitude_deg(domainValue.waypoint->latitudeDeg);
		wireValue.mutable_waypoint()->set_longitude_deg(domainValue.waypoint->longitudeDeg);
	}
	return wireValue;
}
Orders::Api::Request Decode(const Wire::OrderRequest &wireValue) {
	Orders::Api::Request domainValue{wireValue.session_id(), wireValue.request_id(), wireValue.entity_id(), wireValue.based_on_version()};
	if (wireValue.has_waypoint()) {
		domainValue.waypoint = Orders::Api::Waypoint{wireValue.waypoint().latitude_deg(), wireValue.waypoint().longitude_deg()};
	}
	return domainValue;
}
Wire::OrderResult Encode(const Orders::Api::Result &domainValue) {
	Wire::OrderResult wireValue;
	wireValue.set_session_id(domainValue.sessionId);
	wireValue.set_request_id(domainValue.requestId);
	wireValue.set_stage(static_cast<Wire::Stage>(domainValue.stage));
	wireValue.set_reason(domainValue.reason);
	wireValue.set_sampled_at_seconds(domainValue.sampledAtSeconds);
	if (domainValue.displacementMeters) {
		wireValue.set_displacement_m(*domainValue.displacementMeters);
	}
	return wireValue;
}
Orders::Api::Result Decode(const Wire::OrderResult &wireValue) {
	Orders::Api::Result domainValue{
		wireValue.session_id(), wireValue.request_id(), static_cast<Orders::Api::Stage>(wireValue.stage()), wireValue.reason(), wireValue.sampled_at_seconds()};
	if (wireValue.has_displacement_m()) {
		domainValue.displacementMeters = wireValue.displacement_m();
	}
	return domainValue;
}
} // namespace NavalCommand::Gateway::Api
