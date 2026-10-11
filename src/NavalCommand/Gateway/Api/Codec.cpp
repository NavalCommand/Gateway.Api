#include <NavalCommand/Gateway/Api/Codec.h>
#include <NavalCommand/Observations/Api/AirOperationsObservation.h>
#include <NavalCommand/Observations/Api/EquipmentObservation.h>
#include <NavalCommand/Observations/Api/NavigationObservation.h>
#include <NavalCommand/Orders/Api/LaunchAircraft.h>
#include <NavalCommand/Orders/Api/RouteCommand.h>
#include <variant>

#include <stdexcept>
#include <string>
#include <utility>
#include <string_view>
#include <unordered_set>

namespace NavalCommand::Gateway::Api {
namespace Wire = navalcommand::gateway::v1;
namespace Physics = NavalCommand::Physics::Api;
namespace Catalog = NavalCommand::Catalog::Api;
namespace Obs = NavalCommand::Observations::Api;
namespace Orders = NavalCommand::Orders::Api;
namespace World = NavalCommand::World::Api;
namespace Exact = NavalCommand::Uncertainty::Api;

template<class T> using ExactValue = Exact::Exact<T>;

static void requireId(std::string_view value) {
	if (value.empty()) throw std::invalid_argument("An opaque session-scoped identity must not be empty");
}

static void put(Wire::ObserverId& wire, const Obs::ObserverId& value) { wire.set_value(value.Value); }
static void put(Wire::ObservationId& wire, const Obs::ObservationId& value) { wire.set_value(value.Value); }
static void put(Wire::UnitTypeId& wire, const Catalog::UnitTypeId& value) { wire.set_value(value.Value); }
static void put(Wire::EquipmentTypeId& wire, const Catalog::EquipmentTypeId& value) { wire.set_value(value.Value); }
static void put(Wire::ResourceId& wire, const Catalog::ResourceId& value) { wire.set_value(value.Value); }
static void put(Wire::FactionId& wire, const World::FactionId& value) { wire.set_value(value.Value); }
static void put(Wire::TaskForceId& wire, const World::TaskForceId& value) { wire.set_value(value.Value); }
static void put(Wire::LoadoutId& wire, const Catalog::LoadoutId& value) { wire.set_value(value.Value); }

static void put(Wire::ResourceValue& wire, const Catalog::ResourceValue& value) {
	if (const auto* discrete = std::get_if<uint64_t>(&value)) wire.set_discrete(*discrete);
	else wire.set_continuous(std::get<double>(value));
}


template<class T, class Setter>
static void encodeExact(const ExactValue<T>& value, Setter&& setter) {
	if (value.HasValue()) setter(value.Value());
}

template<class T, class Getter>
static ExactValue<T> decodeExact(bool present, Getter&& getter) {
	return present ? ExactValue<T>::Known(getter()) : ExactValue<T>::Unknown();
}

static void put(Wire::GeoPoint& wire, const Physics::Position& value) {
	wire.set_latitude_deg(value.Latitude.Value);
	wire.set_longitude_deg(value.Longitude.Value);
	if (value.Altitude) wire.set_altitude_m(value.Altitude->Value);
}

static Physics::Position getExactPosition(const Wire::GeoPoint& wire) {
	if (!wire.has_latitude_deg() || !wire.has_longitude_deg()) throw std::invalid_argument("Exact position coordinates are missing");
	Physics::Position value;
	value.Latitude = Physics::Degrees{wire.latitude_deg()};
	value.Longitude = Physics::Degrees{wire.longitude_deg()};
	if (wire.has_altitude_m()) value.Altitude = Physics::Meters{wire.altitude_m()};
	return value;
}

static void put(Wire::ObservedPosition& wire, const Obs::PartiallyLocalizedPosition& value) {
	if (value.Latitude) wire.set_latitude_deg(value.Latitude->Value);
	if (value.Longitude) wire.set_longitude_deg(value.Longitude->Value);
	if (value.ErrorEllipse) {
		auto* ellipse = wire.mutable_error_ellipse();
		ellipse->set_major_axis_m(value.ErrorEllipse->MajorAxis.Value);
		ellipse->set_minor_axis_m(value.ErrorEllipse->MinorAxis.Value);
		ellipse->set_orientation_rad(value.ErrorEllipse->Orientation.Value);
	}
}

static Obs::PartiallyLocalizedPosition getPartialPosition(const Wire::ObservedPosition& wire) {
	Obs::PartiallyLocalizedPosition value;
	if (wire.has_latitude_deg()) value.Latitude = Physics::Degrees{wire.latitude_deg()};
	if (wire.has_longitude_deg()) value.Longitude = Physics::Degrees{wire.longitude_deg()};
	if (wire.has_error_ellipse()) {
		value.ErrorEllipse = Obs::ObservedErrorEllipse{Physics::Meters{wire.error_ellipse().major_axis_m()},
			Physics::Meters{wire.error_ellipse().minor_axis_m()}, Physics::Radians{wire.error_ellipse().orientation_rad()}};
	}
	return value;
}

static void put(Wire::RoutePoint& wire, const Obs::ObservedRoutePoint& value) {
	wire.set_latitude_deg(value.Position.Latitude.Value);
	wire.set_longitude_deg(value.Position.Longitude.Value);
	wire.set_label(value.Label);
	if (value.Position.Altitude) wire.set_altitude_m(value.Position.Altitude->Value);
}

static Obs::ObservedRoutePoint getRoutePoint(const Wire::RoutePoint& wire) {
	Physics::Position position;
	position.Latitude = Physics::Degrees{wire.latitude_deg()};
	position.Longitude = Physics::Degrees{wire.longitude_deg()};
	if (wire.has_altitude_m()) position.Altitude = Physics::Meters{wire.altitude_m()};
	return {position, wire.label()};
}

static void put(Wire::Route& wire, const std::vector<Obs::ObservedRoutePoint>& value) {
	for (const auto& point : value) put(*wire.add_points(), point);
}

static std::vector<Obs::ObservedRoutePoint> getRoute(const Wire::Route& wire) {
	std::vector<Obs::ObservedRoutePoint> value;
	value.reserve(wire.points_size());
	for (const auto& point : wire.points()) value.push_back(getRoutePoint(point));
	return value;
}

static void put(Wire::AircraftSelection& wire, const Obs::AircraftSelection& value) {
	put(*wire.mutable_type(), value.Type);
	wire.set_squadron(value.Squadron);
	put(*wire.mutable_loadout(), value.Loadout);
}

static Obs::AircraftSelection getSelection(const Wire::AircraftSelection& wire) {
	return {{wire.type().value()}, wire.squadron(), {wire.loadout().value()}};
}

static void put(Wire::LaunchAircraft& wire, const Orders::LaunchAircraft& value) {
	put(*wire.mutable_selection(), value.Aircraft);
	wire.set_count(value.Count);
}

static Orders::LaunchAircraft getLaunch(const Wire::LaunchAircraft& wire) {
	return {getSelection(wire.selection()), wire.count()};
}

Wire::OrderEligibilityState Encode(const std::vector<Orders::Eligibility>& value) {
	Wire::OrderEligibilityState wire;
	for (const auto& item : value) {
		auto* encoded = wire.add_items();
		encoded->set_kind(static_cast<Wire::OrderKind>(item.Kind));
		encoded->set_availability(static_cast<Wire::Availability>(item.Availability));
		encoded->set_reason(item.Reason);
		if (item.LaunchOption) put(*encoded->mutable_launch_option(), *item.LaunchOption);
	}
	return wire;
}

std::vector<Orders::Eligibility> Decode(const Wire::OrderEligibilityState& wire) {
	std::vector<Orders::Eligibility> items;
	items.reserve(wire.items_size());
	for (const auto& item : wire.items()) {
		if (!Wire::OrderKind_IsValid(item.kind()) || !Wire::Availability_IsValid(item.availability()))
			throw std::invalid_argument("Invalid order kind or availability");
		Orders::Eligibility value{static_cast<Orders::OrderKind>(item.kind()), static_cast<Orders::Availability>(item.availability()), item.reason()};
		if (item.has_launch_option()) value.LaunchOption = getLaunch(item.launch_option());
		items.push_back(std::move(value));
	}
	return items;
}

Wire::OrderDiscovery Encode(const Orders::OrderAvailability& value) {
	Wire::OrderDiscovery wire;
	put(*wire.mutable_observer(), value.Observer);
	requireId(value.Observer.Value);
	put(*wire.mutable_observation(), value.Observation);
	requireId(value.Observation.Value);
	wire.set_based_on_version(value.BasedOnVersion);
	*wire.mutable_eligibility() = Encode(value.Orders);
	return wire;
}

Orders::OrderAvailability Decode(const Wire::OrderDiscovery& wire) {
	requireId(wire.observer().value());
	requireId(wire.observation().value());
	return {{wire.observer().value()}, {wire.observation().value()}, wire.based_on_version(), Decode(wire.eligibility())};
}

static void encodeUnitObservation(Wire::UnitObservation& wire, const Obs::UnitObservation& value) {
	put(*wire.mutable_identity(), value.Identity);
	requireId(value.Identity.Value);
	encodeExact(value.Name, [&](const std::string& name) { wire.set_name(name); });
	encodeExact(value.Type, [&](const Catalog::UnitTypeId& type) { put(*wire.mutable_type(), type); });
	auto* kinematics = wire.mutable_kinematics();
	if (const auto* exactPosition = std::get_if<ExactValue<Physics::Position>>(&value.Kinematics.Position)) {
		if (exactPosition->HasValue()) put(*kinematics->mutable_exact_position(), exactPosition->Value());
	} else {
		put(*kinematics->mutable_partial_position(), std::get<Obs::PartiallyLocalizedPosition>(value.Kinematics.Position));
	}
	encodeExact(value.Kinematics.Speed, [&](Physics::MetersPerSecond speed) { kinematics->set_speed_mps(speed.Value); });
	encodeExact(value.Kinematics.Heading, [&](Physics::Radians heading) { kinematics->set_heading_rad(heading.Value); });
	encodeExact(value.Kinematics.CourseOverGround, [&](Physics::Radians course) { kinematics->set_course_over_ground_rad(course.Value); });
	for (const auto& item : value.Equipment) {
		auto* encoded = wire.add_equipment();
		encodeExact(item.Type, [&](const Catalog::EquipmentTypeId& type) { put(*encoded->mutable_type_id(), type); });
		encodeExact(item.Active, [&](bool active) { encoded->set_active(active); });
		for (const auto& resource : item.Resources) {
			auto* resourceWire = encoded->add_resources();
			put(*resourceWire->mutable_resource_id(), resource.Id);
			encodeExact(resource.Amount, [&](const Catalog::ResourceValue& amount) { put(*resourceWire->mutable_amount(), amount); });
		}
	}
	if (value.Navigation) encodeExact(value.Navigation->PlannedRoute, [&](const auto& route) { put(*wire.mutable_navigation()->mutable_planned_route(), route); });
	if (value.Aircraft) encodeExact(value.Aircraft->Airborne, [&](bool airborne) { wire.mutable_aircraft()->set_airborne(airborne); });
	if (value.AirOperations) {
		const auto& operations = *value.AirOperations;
		auto* air = wire.mutable_air_operations();
		for (const auto& entry : operations.Inventory) {
			auto* item = air->add_inventory();
			encodeExact(entry.Type, [&](const Catalog::UnitTypeId& type) { put(*item->mutable_type(), type); });
			encodeExact(entry.Squadron, [&](const std::string& squadron) { item->set_squadron(squadron); });
			encodeExact(entry.UnreservedCount, [&](std::uint32_t count) { item->set_unreserved_count(count); });
			for (const auto& loadout : entry.Loadouts) put(*item->add_loadouts(), loadout);
		}
		encodeExact(operations.PendingOperations, [&](std::uint32_t count) { air->set_pending_operations(count); });
		encodeExact(operations.OnboardCount, [&](std::uint32_t count) { air->set_onboard_count(count); });
		encodeExact(operations.ReservedCount, [&](std::uint32_t count) { air->set_reserved_count(count); });
		encodeExact(operations.CooldownCount, [&](std::uint32_t count) { air->set_cooldown_count(count); });
		encodeExact(operations.MeasuredAt, [&](Physics::Seconds time) { air->set_measured_at_seconds(time.Value); });
	}
	encodeExact(value.MeasuredAt, [&](Physics::Seconds time) { wire.set_measured_at_seconds(time.Value); });
	for (const auto& contributor : value.Contributors) {
		auto* encoded = wire.add_contributors();
		put(*encoded->mutable_observer(), contributor.Observer);
		for (const auto& sensor : contributor.Sensors) put(*encoded->add_sensors(), sensor);
	}
}

static Obs::UnitObservation decodeUnitObservation(const Wire::UnitObservation& wire) {
	requireId(wire.identity().value());
	Obs::UnitObservation value;
	value.Identity = {wire.identity().value()};
	value.Name = decodeExact<std::string>(wire.has_name(), [&] { return wire.name(); });
	value.Type = decodeExact<Catalog::UnitTypeId>(wire.has_type(), [&] { return Catalog::UnitTypeId{wire.type().value()}; });
	const auto& k = wire.kinematics();
	if (k.has_exact_position()) value.Kinematics.Position = ExactValue<Physics::Position>::Known(getExactPosition(k.exact_position()));
	else if (k.has_partial_position()) value.Kinematics.Position = getPartialPosition(k.partial_position());
	value.Kinematics.Speed = decodeExact<Physics::MetersPerSecond>(k.has_speed_mps(), [&] { return Physics::MetersPerSecond{k.speed_mps()}; });
	value.Kinematics.Heading = decodeExact<Physics::Radians>(k.has_heading_rad(), [&] { return Physics::Radians{k.heading_rad()}; });
	value.Kinematics.CourseOverGround = decodeExact<Physics::Radians>(k.has_course_over_ground_rad(), [&] { return Physics::Radians{k.course_over_ground_rad()}; });
	value.Equipment.reserve(wire.equipment_size());
	for (const auto& item : wire.equipment()) {
		Obs::EquipmentObservation decoded;
		decoded.Type = decodeExact<Catalog::EquipmentTypeId>(item.has_type_id(), [&] { return Catalog::EquipmentTypeId{item.type_id().value()}; });
		decoded.Active = decodeExact<bool>(item.has_active(), [&] { return item.active(); });
		decoded.Resources.reserve(item.resources_size());
		for (const auto& resource : item.resources()) {
			auto amount = decodeExact<Catalog::ResourceValue>(resource.has_amount(), [&] {
				if (resource.amount().has_discrete()) return Catalog::ResourceValue{resource.amount().discrete()};
				if (resource.amount().has_continuous()) return Catalog::ResourceValue{resource.amount().continuous()};
				throw std::invalid_argument("Resource quantity is missing");
			});
			decoded.Resources.push_back({{resource.resource_id().value()}, std::move(amount)});
		}
		value.Equipment.push_back(std::move(decoded));
	}
	if (wire.has_navigation()) {
		Obs::NavigationObservation navigation;
		navigation.PlannedRoute = decodeExact<std::vector<Obs::ObservedRoutePoint>>(wire.navigation().has_planned_route(), [&] { return getRoute(wire.navigation().planned_route()); });
		value.Navigation = std::move(navigation);
	}
	if (wire.has_aircraft()) value.Aircraft = Obs::AircraftObservation{decodeExact<bool>(wire.aircraft().has_airborne(), [&] { return wire.aircraft().airborne(); })};
	if (wire.has_air_operations()) {
		const auto& encoded = wire.air_operations();
		Obs::AirOperationsObservation operations;
		operations.Inventory.reserve(encoded.inventory_size());
		for (const auto& entry : encoded.inventory()) {
			Obs::AirOperationsObservation::AircraftInventoryEntry decoded;
			decoded.Type = decodeExact<Catalog::UnitTypeId>(entry.has_type(), [&] { return Catalog::UnitTypeId{entry.type().value()}; });
			decoded.Squadron = decodeExact<std::string>(entry.has_squadron(), [&] { return entry.squadron(); });
			decoded.UnreservedCount = decodeExact<std::uint32_t>(entry.has_unreserved_count(), [&] { return entry.unreserved_count(); });
			decoded.Loadouts.reserve(entry.loadouts_size());
			for (const auto& loadout : entry.loadouts()) decoded.Loadouts.push_back({loadout.value()});
			operations.Inventory.push_back(std::move(decoded));
		}
		operations.PendingOperations = decodeExact<std::uint32_t>(encoded.has_pending_operations(), [&] { return encoded.pending_operations(); });
		operations.OnboardCount = decodeExact<std::uint32_t>(encoded.has_onboard_count(), [&] { return encoded.onboard_count(); });
		operations.ReservedCount = decodeExact<std::uint32_t>(encoded.has_reserved_count(), [&] { return encoded.reserved_count(); });
		operations.CooldownCount = decodeExact<std::uint32_t>(encoded.has_cooldown_count(), [&] { return encoded.cooldown_count(); });
		operations.MeasuredAt = decodeExact<Physics::Seconds>(encoded.has_measured_at_seconds(), [&] { return Physics::Seconds{encoded.measured_at_seconds()}; });
		value.AirOperations = std::move(operations);
	}
	value.Contributors.reserve(wire.contributors_size());
	value.MeasuredAt = decodeExact<Physics::Seconds>(wire.has_measured_at_seconds(), [&] { return Physics::Seconds{wire.measured_at_seconds()}; });
	for (const auto& contributor : wire.contributors()) {
		requireId(contributor.observer().value());
		Obs::ObservationContributor decoded{{contributor.observer().value()}, {}};
		for (const auto& sensor : contributor.sensors()) decoded.Sensors.push_back({sensor.value()});
		value.Contributors.push_back(std::move(decoded));
	}
	return value;
}

Wire::ObserverSnapshot Encode(const World::ObserverSnapshot& value) {
	Wire::ObserverSnapshot wire;
	wire.set_schema_version(4);
	wire.set_session_id(value.SessionId);
	put(*wire.mutable_observer(), value.Observer);
	requireId(value.Observer.Value);
	wire.set_ready(value.Ready);
	wire.set_truncated(value.Truncated);
	wire.set_version(value.Version);
	wire.set_captured_at_seconds(value.CapturedAt.Value);
	for (const auto& unit : value.Units) encodeUnitObservation(*wire.add_units(), unit);
	for (const auto& item : value.Memberships) {
		auto* relation = wire.add_memberships();
		put(*relation->mutable_observation(), item.Observation);
		encodeExact(item.Faction, [&](const World::FactionId& id) { put(*relation->mutable_faction(), id); });
		encodeExact(item.TaskForce, [&](const World::TaskForceId& id) { put(*relation->mutable_task_force(), id); });
	}
	return wire;
}

World::ObserverSnapshot Decode(const Wire::ObserverSnapshot& wire) {
	if (wire.schema_version() != 4) throw std::invalid_argument("Unsupported ObserverSnapshot schema version; expected 4");
	requireId(wire.observer().value());
	World::ObserverSnapshot value;
	value.SessionId = wire.session_id();
	value.Observer = {wire.observer().value()};
	value.Ready = wire.ready();
	value.Truncated = wire.truncated();
	value.Version = wire.version();
	value.CapturedAt = Physics::Seconds{wire.captured_at_seconds()};
	value.Units.reserve(wire.units_size());
	std::unordered_set<std::string_view> identities;
	for (const auto& unit : wire.units()) {
		auto decoded = decodeUnitObservation(unit);
		if (!identities.insert(decoded.Identity.Value).second) throw std::invalid_argument("Duplicate ObservationId in observer snapshot");
		value.Units.push_back(std::move(decoded));
	}
	value.Memberships.reserve(wire.memberships_size());
	for (const auto& item : wire.memberships()) {
		requireId(item.observation().value());
		World::ObserverUnitRelationship decoded;
		decoded.Observation = {item.observation().value()};
		decoded.Faction = decodeExact<World::FactionId>(item.has_faction(), [&] { return World::FactionId{item.faction().value()}; });
		decoded.TaskForce = decodeExact<World::TaskForceId>(item.has_task_force(), [&] { return World::TaskForceId{item.task_force().value()}; });
		value.Memberships.push_back(std::move(decoded));
	}
	return value;
}

Wire::ProviderObservation Encode(const ProviderObservation& value) {
	Wire::ProviderObservation wire;
	*wire.mutable_snapshot() = Encode(value.Snapshot);
	for (const auto& availability : value.OrderAvailability) {
		auto* discovery = wire.add_order_availability();
		*discovery = Encode(availability);
		discovery->set_session_id(value.Snapshot.SessionId);
	}
	return wire;
}

ProviderObservation Decode(const Wire::ProviderObservation& wire) {
	ProviderObservation value;
	value.Snapshot = Decode(wire.snapshot());
	std::unordered_set<std::string_view> observations;
	for (const auto& unit : value.Snapshot.Units) observations.insert(unit.Identity.Value);
	std::unordered_set<std::string_view> overlays;
	value.OrderAvailability.reserve(wire.order_availability_size());
	for (const auto& discovery : wire.order_availability()) {
		if (discovery.session_id() != value.Snapshot.SessionId || discovery.observer().value() != value.Snapshot.Observer.Value || discovery.based_on_version() != value.Snapshot.Version)
			throw std::invalid_argument("Order overlay scope/version differs from ObserverSnapshot");
		if (!observations.contains(discovery.observation().value())) throw std::invalid_argument("Order overlay references an absent observation");
		if (!overlays.insert(discovery.observation().value()).second) throw std::invalid_argument("Duplicate order overlay for ObservationId");
		value.OrderAvailability.push_back(Decode(discovery));
	}
	return value;
}

Wire::OrderRequest Encode(const Orders::Request& value) {
	Wire::OrderRequest wire;
	wire.set_session_id(value.SessionId);
	wire.set_request_id(value.RequestId);
	put(*wire.mutable_observer(), value.Observer);
	requireId(value.Observer.Value);
	put(*wire.mutable_observation(), value.Observation);
	requireId(value.Observation.Value);
	wire.set_based_on_version(value.BasedOnVersion);
	if (const auto* route = std::get_if<Orders::RouteCommand>(&value.Payload)) {
		auto* waypoint = wire.mutable_waypoint();
		waypoint->set_latitude_deg(route->Latitude.Value);
		waypoint->set_longitude_deg(route->Longitude.Value);
		waypoint->set_route_mode(static_cast<Wire::RouteMode>(route->Mode));
	} else put(*wire.mutable_launch_aircraft(), std::get<Orders::LaunchAircraft>(value.Payload));
	return wire;
}

Orders::Request Decode(const Wire::OrderRequest& wire) {
	requireId(wire.observer().value());
	requireId(wire.observation().value());
	Orders::Request value;
	value.SessionId = wire.session_id();
	value.RequestId = wire.request_id();
	value.Observer = {wire.observer().value()};
	value.Observation = {wire.observation().value()};
	value.BasedOnVersion = wire.based_on_version();
	if (wire.has_waypoint()) value.Payload = Orders::RouteCommand{Physics::Degrees{wire.waypoint().latitude_deg()}, Physics::Degrees{wire.waypoint().longitude_deg()}, static_cast<Orders::RouteMode>(wire.waypoint().route_mode())};
	else if (wire.has_launch_aircraft()) value.Payload = getLaunch(wire.launch_aircraft());
	else throw std::invalid_argument("Order request requires exactly one typed payload");
	return value;
}

Wire::OrderResult Encode(const Orders::Result& value) {
	Wire::OrderResult wire;
	wire.set_session_id(value.SessionId);
	wire.set_request_id(value.RequestId);
	wire.set_stage(static_cast<Wire::Stage>(value.Stage));
	wire.set_reason(value.Reason);
	wire.set_sampled_at_seconds(value.SampledAt.Value);
	wire.set_route_mode(static_cast<Wire::RouteMode>(value.Mode));
	put(*wire.mutable_observer(), value.Observer);
	requireId(value.Observer.Value);
	if (value.SpawnedObservation) {
		put(*wire.mutable_spawned_observation_id(), *value.SpawnedObservation);
		requireId(value.SpawnedObservation->Value);
	}
	if (value.Displacement) wire.set_displacement_m(value.Displacement->Value);
	for (const auto& point : value.RouteBefore) put(*wire.add_route_before(), point);
	for (const auto& point : value.RouteAfter) put(*wire.add_route_after(), point);
	return wire;
}

Orders::Result Decode(const Wire::OrderResult& wire) {
	if (!Wire::Stage_IsValid(wire.stage()) || !Wire::RouteMode_IsValid(wire.route_mode())) throw std::invalid_argument("Invalid order result enum");
	requireId(wire.observer().value());
	Orders::Result value;
	value.SessionId = wire.session_id();
	value.RequestId = wire.request_id();
	value.Observer = {wire.observer().value()};
	value.Stage = static_cast<Orders::Stage>(wire.stage());
	value.Reason = wire.reason();
	value.SampledAt = Physics::Seconds{wire.sampled_at_seconds()};
	value.Mode = static_cast<Orders::RouteMode>(wire.route_mode());
	if (wire.has_spawned_observation_id()) {
		requireId(wire.spawned_observation_id().value());
		value.SpawnedObservation = Obs::ObservationId{wire.spawned_observation_id().value()};
	}
	if (wire.has_displacement_m()) value.Displacement = Physics::Meters{wire.displacement_m()};
	for (const auto& point : wire.route_before()) value.RouteBefore.push_back(getRoutePoint(point));
	for (const auto& point : wire.route_after()) value.RouteAfter.push_back(getRoutePoint(point));
	return value;
}
}
