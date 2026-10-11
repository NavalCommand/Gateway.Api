#include <NavalCommand/Gateway/Api/Codec.h>
#include <NavalCommand/Orders/Api/RouteCommand.h>
#include <stdexcept>

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
	if (!wirePosition.has_latitude_deg() || !wirePosition.has_longitude_deg()) {
		throw std::invalid_argument("Position coordinates are missing");
	}
	return {wirePosition.latitude_deg(), wirePosition.longitude_deg(), wirePosition.has_altitude_m() ? std::optional<double>{wirePosition.altitude_m()} : std::nullopt};
}

namespace Domain = World::Api;

static void EncodePositionObservation(Wire::PositionObservation &wire, const Domain::PositionObservation &value) {
	EncodePosition(*wire.mutable_coordinates(), value.Coordinates);
	if (value.ErrorEllipse) {
		auto ellipse = wire.mutable_error_ellipse();
		ellipse->set_major_axis_m(value.ErrorEllipse->MajorAxisMeters);
		ellipse->set_minor_axis_m(value.ErrorEllipse->MinorAxisMeters);
		ellipse->set_orientation_rad(value.ErrorEllipse->OrientationRadians);
	}
}

static Domain::PositionObservation DecodePositionObservation(const Wire::PositionObservation &wire) {
	Domain::PositionObservation result{DecodePosition(wire.coordinates())};
	if (wire.has_error_ellipse()) {
		const auto &ellipse = wire.error_ellipse();
		result.ErrorEllipse = Domain::PositionObservation::Uncertainty{ellipse.major_axis_m(), ellipse.minor_axis_m(), ellipse.orientation_rad()};
	}
	return result;
}

static void EncodeRouteValue(Wire::Route &wire, const std::vector<Orders::Api::RoutePoint> &value) {
	for (const auto &point : value) {
		EncodeRoute(*wire.add_points(), point);
	}
}

static std::vector<Orders::Api::RoutePoint> DecodeRouteValue(const Wire::Route &wire) {
	std::vector<Orders::Api::RoutePoint> result;
	for (const auto &point : wire.points()) {
		result.push_back(DecodeRoute(point));
	}
	return result;
}

static void EncodeKnowledge(Wire::StringKnowledge &wire, const Domain::Knowledge<std::string> &value) {
	if (value.IsKnown()) {
		wire.set_known(value.Value());
	} else if (value.IsEstimated()) {
		auto estimate = wire.mutable_estimated();
		estimate->set_value(value.Value());
		estimate->set_source(value.Source());
	}
}

static Domain::Knowledge<std::string> DecodeKnowledge(const Wire::StringKnowledge &wire) {
	if (wire.has_estimated() && !wire.estimated().has_value()) {
		throw std::invalid_argument("Estimated knowledge has no value");
	}

	if (wire.has_known()) {
		return Domain::Knowledge<std::string>::Known(wire.known());
	}
	if (wire.has_estimated()) {
		return Domain::Knowledge<std::string>::Estimated(wire.estimated().value(), wire.estimated().source());
	}
	return {};
}

static void EncodeKnowledge(Wire::DoubleKnowledge &wire, const Domain::Knowledge<double> &value) {
	if (value.IsKnown()) {
		wire.set_known(value.Value());
	} else if (value.IsEstimated()) {
		auto estimate = wire.mutable_estimated();
		estimate->set_value(value.Value());
		estimate->set_source(value.Source());
	}
}

static Domain::Knowledge<double> DecodeKnowledge(const Wire::DoubleKnowledge &wire) {
	if (wire.has_estimated() && !wire.estimated().has_value()) {
		throw std::invalid_argument("Estimated knowledge has no value");
	}

	if (wire.has_known()) {
		return Domain::Knowledge<double>::Known(wire.known());
	}
	if (wire.has_estimated()) {
		return Domain::Knowledge<double>::Estimated(wire.estimated().value(), wire.estimated().source());
	}
	return {};
}

static void EncodeKnowledge(Wire::BoolKnowledge &wire, const Domain::Knowledge<bool> &value) {
	if (value.IsKnown()) {
		wire.set_known(value.Value());
	} else if (value.IsEstimated()) {
		auto estimate = wire.mutable_estimated();
		estimate->set_value(value.Value());
		estimate->set_source(value.Source());
	}
}

static Domain::Knowledge<bool> DecodeKnowledge(const Wire::BoolKnowledge &wire) {
	if (wire.has_estimated() && !wire.estimated().has_value()) {
		throw std::invalid_argument("Estimated knowledge has no value");
	}

	if (wire.has_known()) {
		return Domain::Knowledge<bool>::Known(wire.known());
	}
	if (wire.has_estimated()) {
		return Domain::Knowledge<bool>::Estimated(wire.estimated().value(), wire.estimated().source());
	}
	return {};
}

static void EncodeKnowledge(Wire::PositionKnowledge &wire, const Domain::Knowledge<Domain::PositionObservation> &value) {
	if (value.IsKnown()) {
		EncodePositionObservation(*wire.mutable_known(), value.Value());
	} else if (value.IsEstimated()) {
		auto estimate = wire.mutable_estimated();
		EncodePositionObservation(*estimate->mutable_value(), value.Value());
		estimate->set_source(value.Source());
	}
}

static Domain::Knowledge<Domain::PositionObservation> DecodeKnowledge(const Wire::PositionKnowledge &wire) {
	if (wire.has_estimated() && !wire.estimated().has_value()) {
		throw std::invalid_argument("Estimated knowledge has no value");
	}
	if (wire.has_known()) {
		return Domain::Knowledge<Domain::PositionObservation>::Known(DecodePositionObservation(wire.known()));
	}
	if (wire.has_estimated()) {
		return Domain::Knowledge<Domain::PositionObservation>::Estimated(DecodePositionObservation(wire.estimated().value()), wire.estimated().source());
	}
	return {};
}

static void EncodeKnowledge(Wire::RouteKnowledge &wire, const Domain::Knowledge<std::vector<Orders::Api::RoutePoint>> &value) {
	if (value.IsKnown()) {
		EncodeRouteValue(*wire.mutable_known(), value.Value());
	} else if (value.IsEstimated()) {
		auto estimate = wire.mutable_estimated();
		EncodeRouteValue(*estimate->mutable_value(), value.Value());
		estimate->set_source(value.Source());
	}
}

static Domain::Knowledge<std::vector<Orders::Api::RoutePoint>> DecodeKnowledge(const Wire::RouteKnowledge &wire) {
	if (wire.has_estimated() && !wire.estimated().has_value()) {
		throw std::invalid_argument("Estimated knowledge has no value");
	}
	if (wire.has_known()) {
		return Domain::Knowledge<std::vector<Orders::Api::RoutePoint>>::Known(DecodeRouteValue(wire.known()));
	}
	if (wire.has_estimated()) {
		return Domain::Knowledge<std::vector<Orders::Api::RoutePoint>>::Estimated(DecodeRouteValue(wire.estimated().value()), wire.estimated().source());
	}
	return {};
}

static void EncodeKnowledge(Wire::ReadinessKnowledge &wire, const Domain::Knowledge<Domain::EquipmentInstance::Readiness> &value) {
	if (value.IsKnown()) {
		wire.set_known(static_cast<Wire::EquipmentReadiness>(value.Value()));
	} else if (value.IsEstimated()) {
		auto estimate = wire.mutable_estimated();
		estimate->set_value(static_cast<Wire::EquipmentReadiness>(value.Value()));
		estimate->set_source(value.Source());
	}
}

static Domain::Knowledge<Domain::EquipmentInstance::Readiness> DecodeKnowledge(const Wire::ReadinessKnowledge &wire) {
	if (wire.has_estimated() && !wire.estimated().has_value()) {
		throw std::invalid_argument("Estimated knowledge has no value");
	}
	if ((wire.has_known() && !Wire::EquipmentReadiness_IsValid(wire.known())) || (wire.has_estimated() && !Wire::EquipmentReadiness_IsValid(wire.estimated().value()))) {
		throw std::invalid_argument("Invalid equipment state");
	}
	if (wire.has_known()) {
		return Domain::Knowledge<Domain::EquipmentInstance::Readiness>::Known(static_cast<Domain::EquipmentInstance::Readiness>(wire.known()));
	}
	if (wire.has_estimated()) {
		return Domain::Knowledge<Domain::EquipmentInstance::Readiness>::Estimated(static_cast<Domain::EquipmentInstance::Readiness>(wire.estimated().value()),
																				  wire.estimated().source());
	}
	return {};
}

static void EncodeKnowledge(Wire::DamageKnowledge &wire, const Domain::Knowledge<Domain::EquipmentInstance::Damage> &value) {
	if (value.IsKnown()) {
		wire.set_known(static_cast<Wire::EquipmentDamage>(value.Value()));
	} else if (value.IsEstimated()) {
		auto estimate = wire.mutable_estimated();
		estimate->set_value(static_cast<Wire::EquipmentDamage>(value.Value()));
		estimate->set_source(value.Source());
	}
}

static Domain::Knowledge<Domain::EquipmentInstance::Damage> DecodeKnowledge(const Wire::DamageKnowledge &wire) {
	if (wire.has_estimated() && !wire.estimated().has_value()) {
		throw std::invalid_argument("Estimated knowledge has no value");
	}
	if ((wire.has_known() && !Wire::EquipmentDamage_IsValid(wire.known())) || (wire.has_estimated() && !Wire::EquipmentDamage_IsValid(wire.estimated().value()))) {
		throw std::invalid_argument("Invalid equipment state");
	}
	if (wire.has_known()) {
		return Domain::Knowledge<Domain::EquipmentInstance::Damage>::Known(static_cast<Domain::EquipmentInstance::Damage>(wire.known()));
	}
	if (wire.has_estimated()) {
		return Domain::Knowledge<Domain::EquipmentInstance::Damage>::Estimated(static_cast<Domain::EquipmentInstance::Damage>(wire.estimated().value()), wire.estimated().source());
	}
	return {};
}

static void EncodePhysical(Wire::PhysicalTraits &wire, const Domain::PhysicalTraits &value) {
	if (value.HorizontalArc) {
		wire.mutable_horizontal_arc()->set_start_rad(value.HorizontalArc->StartRadians);
		wire.mutable_horizontal_arc()->set_end_rad(value.HorizontalArc->EndRadians);
	}
	if (value.OrientationRadians) {
		wire.set_orientation_rad(*value.OrientationRadians);
	}
}

static Domain::PhysicalTraits DecodePhysical(const Wire::PhysicalTraits &wire) {
	Domain::PhysicalTraits result;
	if (wire.has_horizontal_arc()) {
		result.HorizontalArc = Domain::PhysicalTraits::Arc{wire.horizontal_arc().start_rad(), wire.horizontal_arc().end_rad()};
	}
	if (wire.has_orientation_rad()) {
		result.OrientationRadians = wire.orientation_rad();
	}
	return result;
}

template <class T> static typename Domain::ObservedCollection<T>::Coverage DecodeCoverage(Wire::CollectionCoverage coverage) {
	if (!Wire::CollectionCoverage_IsValid(coverage)) {
		throw std::invalid_argument("Invalid collection coverage");
	}
	return static_cast<typename Domain::ObservedCollection<T>::Coverage>(coverage);
}

Wire::OrderEligibilityState Encode(const Domain::OrderEligibilityState &value) {
	Wire::OrderEligibilityState wire;
	wire.set_coverage(static_cast<Wire::CollectionCoverage>(value.Orders.GetCoverage()));
	for (const auto &item : value.Orders.Items()) {
		auto target = wire.add_items();
		target->set_kind(item.kind);
		target->set_availability(static_cast<Wire::Availability>(item.availability));
		target->set_reason(item.reason);
		target->set_argument_semantics(item.arguments);
	}
	return wire;
}

Domain::OrderEligibilityState Decode(const Wire::OrderEligibilityState &wire) {
	std::vector<Orders::Api::Eligibility> items;
	for (const auto &item : wire.items()) {
		items.push_back({item.kind(), static_cast<Orders::Api::Availability>(item.availability()), item.reason(), item.argument_semantics()});
	}
	return {{DecodeCoverage<Orders::Api::Eligibility>(wire.coverage()), std::move(items)}};
}

static void EncodeInstance(Wire::UnitInstance &wire, const Domain::UnitInstance &value) {
	if (auto own = std::get_if<Orders::Api::OwnUnitId>(&value.Identity); own && !own->Value.empty()) {
		wire.mutable_subject()->set_own_unit_id(own->Value);
	} else if (auto track = std::get_if<Domain::TrackId>(&value.Identity); track && !track->Value.empty()) {
		wire.mutable_subject()->set_track_id(track->Value);
	} else {
		throw std::invalid_argument("Instance requires a nonempty tagged subject");
	}
	EncodeKnowledge(*wire.mutable_display()->mutable_name(), value.Display.Name);
	EncodeKnowledge(*wire.mutable_display()->mutable_type_id(), value.Display.TypeId);
	EncodeKnowledge(*wire.mutable_kinematics()->mutable_position(), value.Kinematics.Position);
	EncodeKnowledge(*wire.mutable_kinematics()->mutable_speed_mps(), value.Kinematics.SpeedMetersPerSecond);
	EncodeKnowledge(*wire.mutable_observation()->mutable_sampled_at_seconds(), value.Observation.SampledAtSeconds);
	EncodeKnowledge(*wire.mutable_observation()->mutable_provenance(), value.Observation.Provenance);
	auto equipment = wire.mutable_equipment();
	equipment->set_coverage(static_cast<Wire::CollectionCoverage>(value.Equipment.GetCoverage()));
	for (const auto &item : value.Equipment.Items()) {
		auto target = equipment->add_items();
		EncodeKnowledge(*target->mutable_type_id(), item.TypeId);
		EncodeKnowledge(*target->mutable_active(), item.Active);
		EncodeKnowledge(*target->mutable_operational_readiness(), item.OperationalReadiness);
		EncodeKnowledge(*target->mutable_damage_state(), item.DamageState);
		for (const auto &resource : item.Resources) {
			auto targetResource = target->add_resources();
			targetResource->set_kind(static_cast<Wire::ResourceKind>(resource.Kind));
			EncodeKnowledge(*targetResource->mutable_amount(), resource.Amount);
		}
		if (item.Physical) {
			EncodePhysical(*target->mutable_physical(), *item.Physical);
		}
	}
	if (value.Navigation) {
		EncodeKnowledge(*wire.mutable_navigation()->mutable_inspected(), value.Navigation->Inspected);
		EncodeKnowledge(*wire.mutable_navigation()->mutable_planned_route(), value.Navigation->PlannedRoute);
	}
	if (value.OrderEligibility) {
		*wire.mutable_order_eligibility() = Encode(*value.OrderEligibility);
	}
	if (value.Aircraft) {
		EncodeKnowledge(*wire.mutable_aircraft()->mutable_airborne(), value.Aircraft->Airborne);
	}
}

static Domain::UnitInstance DecodeInstance(const Wire::UnitInstance &wire) {
	Domain::UnitInstance value;
	if (wire.subject().has_own_unit_id() && !wire.subject().own_unit_id().empty()) {
		value.Identity = Orders::Api::OwnUnitId{wire.subject().own_unit_id()};
	} else if (wire.subject().has_track_id() && !wire.subject().track_id().empty()) {
		value.Identity = Domain::TrackId{wire.subject().track_id()};
	} else {
		throw std::invalid_argument("Instance requires a nonempty tagged subject");
	}
	value.Display.Name = DecodeKnowledge(wire.display().name());
	value.Display.TypeId = DecodeKnowledge(wire.display().type_id());
	value.Kinematics.Position = DecodeKnowledge(wire.kinematics().position());
	value.Kinematics.SpeedMetersPerSecond = DecodeKnowledge(wire.kinematics().speed_mps());
	value.Observation.SampledAtSeconds = DecodeKnowledge(wire.observation().sampled_at_seconds());
	value.Observation.Provenance = DecodeKnowledge(wire.observation().provenance());
	std::vector<Domain::EquipmentInstance> equipment;
	for (const auto &item : wire.equipment().items()) {
		Domain::EquipmentInstance result;
		result.TypeId = DecodeKnowledge(item.type_id());
		result.Active = DecodeKnowledge(item.active());
		result.OperationalReadiness = DecodeKnowledge(item.operational_readiness());
		result.DamageState = DecodeKnowledge(item.damage_state());
		for (const auto &resource : item.resources()) {
			if (!Wire::ResourceKind_IsValid(resource.kind())) {
				throw std::invalid_argument("Invalid resource kind");
			}
			result.Resources.push_back({static_cast<Domain::ResourceQuantity::Kind>(resource.kind()), DecodeKnowledge(resource.amount())});
		}
		if (item.has_physical()) {
			result.Physical = DecodePhysical(item.physical());
		}
		equipment.push_back(std::move(result));
	}
	value.Equipment = {DecodeCoverage<Domain::EquipmentInstance>(wire.equipment().coverage()), std::move(equipment)};
	if (wire.has_navigation()) {
		value.Navigation = Domain::NavigationState{DecodeKnowledge(wire.navigation().inspected()), DecodeKnowledge(wire.navigation().planned_route())};
	}
	if (wire.has_order_eligibility()) {
		value.OrderEligibility = Decode(wire.order_eligibility());
	}
	if (wire.has_aircraft()) {
		value.Aircraft = Domain::AircraftState{DecodeKnowledge(wire.aircraft().airborne())};
	}
	return value;
}

Wire::WorldSnapshot Encode(const World::Api::View &domainValue) {
	Wire::WorldSnapshot wireValue;
	auto wireSession = wireValue.mutable_session();
	wireSession->set_schema_version(2);
	wireSession->set_session_id(domainValue.sessionId);
	wireSession->set_observer_id(domainValue.observerId);
	wireSession->set_ready(domainValue.ready);
	wireValue.set_observation_version(domainValue.version);
	wireValue.set_operational_seconds(domainValue.operationalSeconds);
	wireValue.set_truncated(domainValue.truncated);
	for (const auto &instance : domainValue.Instances) {
		EncodeInstance(*wireValue.add_instances(), instance);
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
	for (const auto &instance : wireValue.instances()) {
		domainValue.Instances.push_back(DecodeInstance(instance));
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
	wireValue.mutable_subject()->set_own_unit_id(domainValue.Subject.Value);
	wireValue.set_based_on_version(domainValue.basedOnVersion);
	if (domainValue.facilityLaunch) {
		auto launch = wireValue.mutable_facility_launch();
		launch->set_class_key(domainValue.facilityLaunch->classKey);
		launch->set_squadron(domainValue.facilityLaunch->squadron);
		launch->set_loadout(domainValue.facilityLaunch->loadout);
		launch->set_count(domainValue.facilityLaunch->count);
	}
	if (domainValue.routeCommand) {
		wireValue.mutable_waypoint()->set_latitude_deg(domainValue.routeCommand->latitudeDeg);
		wireValue.mutable_waypoint()->set_longitude_deg(domainValue.routeCommand->longitudeDeg);
		wireValue.mutable_waypoint()->set_route_mode(static_cast<Wire::RouteMode>(domainValue.routeCommand->routeMode));
	}
	return wireValue;
}

Orders::Api::Request Decode(const Wire::OrderRequest &wireValue) {
	if (!wireValue.subject().has_own_unit_id() || wireValue.subject().own_unit_id().empty()) {
		throw std::invalid_argument("Only an own-unit subject can authorize orders");
	}
	Orders::Api::Request domainValue{wireValue.session_id(), wireValue.request_id(), {wireValue.subject().own_unit_id()}, wireValue.based_on_version()};
	if (wireValue.has_facility_launch()) {
		const auto &launch = wireValue.facility_launch();
		domainValue.facilityLaunch = Orders::Api::FacilityLaunch{launch.class_key(), launch.squadron(), launch.loadout(), launch.count()};
	}
	if (wireValue.has_waypoint()) {
		domainValue.routeCommand = Orders::Api::RouteCommand{
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
}
