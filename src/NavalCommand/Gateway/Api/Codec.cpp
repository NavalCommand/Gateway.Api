#include <NavalCommand/Gateway/Api/Codec.h>
#include <NavalCommand/Observations/Api/AirOperationsObservation.h>
#include <NavalCommand/Observations/Api/EquipmentObservation.h>
#include <NavalCommand/Observations/Api/NavigationObservation.h>
#include <NavalCommand/Orders/Api/LaunchAircraft.h>
#include <NavalCommand/Orders/Api/RouteCommand.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>

namespace NavalCommand::Gateway::Api {
namespace Wire = navalcommand::gateway::v1;
namespace Physics = NavalCommand::Physics::Api;
namespace Catalog = NavalCommand::Catalog::Api;
namespace Obs = NavalCommand::Observations::Api;
namespace Orders = NavalCommand::Orders::Api;
namespace World = NavalCommand::World::Api;
namespace Uncertainty = NavalCommand::Uncertainty::Api;
template<class T> using Knowledge = Uncertainty::Knowledge<T>;
template<class T> using Collection = Uncertainty::ObservedCollection<T>;

static void requireId(std::string_view value) {
	if (value.empty()) {
		throw std::invalid_argument("An opaque session-scoped identity must not be empty");
	}
}

static void put(Wire::ObserverId& wire, const Obs::ObserverId& value) { wire.set_value(value.Value); }
static void put(Wire::ObservationId& wire, const Obs::ObservationId& value) { wire.set_value(value.Value); }
static void put(Wire::UnitTypeId& wire, const Catalog::UnitTypeId& value) { wire.set_value(value.Value); }
static void put(Wire::EquipmentTypeId& wire, const Catalog::EquipmentTypeId& value) { wire.set_value(value.Value); }
static void put(Wire::ResourceId& wire, const Catalog::ResourceId& value) { wire.set_value(value.Value); }
static void put(Wire::FactionId& wire, const World::FactionId& value) { wire.set_value(value.Value); }
static void put(Wire::TaskForceId& wire, const World::TaskForceId& value) { wire.set_value(value.Value); }
static void put(Wire::LoadoutId& wire, const Catalog::LoadoutId& value) { wire.set_value(value.Value); }
static void put(std::string& wire, const std::string& value) { wire = value; }

static void put(Wire::PositionObservation& wire, const Physics::Position& value) {
	auto* coordinates = wire.mutable_coordinates();
	coordinates->set_latitude_deg(value.Latitude.Value);
	coordinates->set_longitude_deg(value.Longitude.Value);
	if (value.Altitude) {
		coordinates->set_altitude_m(value.Altitude->Value);
	}
}

static void put(Wire::RoutePoint& wire, const Obs::ObservedRoutePoint& value) {
	wire.set_latitude_deg(value.Position.Latitude.Value);
	wire.set_longitude_deg(value.Position.Longitude.Value);
	wire.set_label(value.Label);
	if (value.Position.Altitude) {
		wire.set_altitude_m(value.Position.Altitude->Value);
	}
}

static void put(Wire::Route& wire, const std::vector<Obs::ObservedRoutePoint>& value) {
	for (const auto& point : value) {
		put(*wire.add_points(), point);
	}
}

static void put(Wire::ResourceValue& wire, const Catalog::ResourceValue& value) {
	if (const auto* discrete = std::get_if<uint64_t>(&value)) {
		wire.set_discrete(*discrete);
	} else {
		wire.set_continuous(std::get<double>(value));
	}
}

template<class T>
static decltype(auto) scalar(const T& value) {
	if constexpr (requires { value.Value; }) {
		return (value.Value);
	} else {
		return (value);
	}
}

template<class WireKnowledge, class T>
static void encodeKnowledge(WireKnowledge& wire, const Knowledge<T>& value) {
	if (!value.HasValue()) {
		return;
	}
	if constexpr (requires { wire.mutable_known(); }) {
		if (value.IsKnown()) {
			put(*wire.mutable_known(), value.Value());
		} else {
			auto* estimate = wire.mutable_estimated();
			put(*estimate->mutable_value(), value.Value());
			estimate->set_source(value.Source());
		}
	} else if (value.IsKnown()) {
		wire.set_known(scalar(value.Value()));
	} else {
		auto* estimate = wire.mutable_estimated();
		estimate->set_value(scalar(value.Value()));
		estimate->set_source(value.Source());
	}
}

template<class T, class WireValue>
static T get(const WireValue& wire) {
	if constexpr (std::is_same_v<T, Physics::Position>) {
		Physics::Position value;
		if constexpr (requires { wire.coordinates(); }) {
			if (!wire.coordinates().has_latitude_deg() || !wire.coordinates().has_longitude_deg()) {
				throw std::invalid_argument("Position coordinates are missing");
			}
			value.Latitude = Physics::Degrees{wire.coordinates().latitude_deg()};
			value.Longitude = Physics::Degrees{wire.coordinates().longitude_deg()};
			if (wire.coordinates().has_altitude_m()) {
				value.Altitude = Physics::Meters{wire.coordinates().altitude_m()};
			}
		} else {
			value.Latitude = Physics::Degrees{wire.latitude_deg()};
			value.Longitude = Physics::Degrees{wire.longitude_deg()};
			if (wire.has_altitude_m()) {
				value.Altitude = Physics::Meters{wire.altitude_m()};
			}
		}
		return value;
	} else if constexpr (std::is_same_v<T, Obs::ObservedRoutePoint>) {
		return {get<Physics::Position>(wire), wire.label()};
	} else if constexpr (std::is_same_v<T, std::vector<Obs::ObservedRoutePoint>>) {
		T value;
		value.reserve(wire.points_size());
		for (const auto& point : wire.points()) {
			value.push_back(get<Obs::ObservedRoutePoint>(point));
		}
		return value;
	} else if constexpr (std::is_same_v<T, Catalog::UnitTypeId> ||
		std::is_same_v<T, Catalog::EquipmentTypeId> || std::is_same_v<T, Catalog::ResourceId> ||
		std::is_same_v<T, Catalog::LoadoutId> || std::is_same_v<T, Obs::ObserverId> ||
		std::is_same_v<T, Obs::ObservationId> || std::is_same_v<T, World::FactionId> ||
		std::is_same_v<T, World::TaskForceId>) {
		return T{wire.value()};
	} else if constexpr (std::is_same_v<T, Catalog::ResourceValue>) {
		if (wire.has_discrete()) {
			return Catalog::ResourceValue{wire.discrete()};
		}
		if (wire.has_continuous()) {
			return Catalog::ResourceValue{wire.continuous()};
		}
		throw std::invalid_argument("Resource quantity is missing");
	} else if constexpr (std::is_same_v<T, Physics::Seconds>) {
		return Physics::Seconds{wire};
	} else if constexpr (std::is_same_v<T, Physics::MetersPerSecond>) {
		return Physics::MetersPerSecond{wire};
	} else if constexpr (std::is_same_v<T, Physics::Radians>) {
		return Physics::Radians{wire};
	} else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, uint32_t> || std::is_same_v<T, std::string>) {
		return wire;
	} else {
		static_assert(sizeof(T) == 0, "No wire decoder is defined for this domain type");
	}
}

template<class T, class WireKnowledge>
static Knowledge<T> decodeKnowledge(const WireKnowledge& wire) {
	if (wire.has_estimated()) {
		if constexpr (requires { wire.estimated().has_value(); }) {
			if (!wire.estimated().has_value()) {
				throw std::invalid_argument("Estimated knowledge has no value");
			}
		}
		return Knowledge<T>::Estimated(get<T>(wire.estimated().value()), wire.estimated().source());
	}
	if (wire.has_known()) {
		return Knowledge<T>::Known(get<T>(wire.known()));
	}
	return {};
}

template<class T>
static typename Collection<T>::Coverage coverage(Wire::CollectionCoverage value) {
	if (!Wire::CollectionCoverage_IsValid(value)) {
		throw std::invalid_argument("Invalid observed collection coverage");
	}
	return static_cast<typename Collection<T>::Coverage>(value);
}

static void encodeSelection(Wire::AircraftSelection& wire, const Obs::AircraftSelection& value) {
	put(*wire.mutable_type(), value.Type);
	wire.set_squadron(value.Squadron);
	put(*wire.mutable_loadout(), value.Loadout);
}

static Obs::AircraftSelection decodeSelection(const Wire::AircraftSelection& wire) {
	return {{wire.type().value()}, wire.squadron(), {wire.loadout().value()}};
}

static void encodeLaunch(Wire::LaunchAircraft& wire, const Orders::LaunchAircraft& value) {
	encodeSelection(*wire.mutable_selection(), value.Aircraft);
	wire.set_count(value.Count);
}

static Orders::LaunchAircraft decodeLaunch(const Wire::LaunchAircraft& wire) {
	return {decodeSelection(wire.selection()), wire.count()};
}

Wire::OrderEligibilityState Encode(const Collection<Orders::Eligibility>& value) {
	Wire::OrderEligibilityState wire;
	wire.set_coverage(static_cast<Wire::CollectionCoverage>(value.GetCoverage()));
	for (const auto& item : value.Items()) {
		auto* encoded = wire.add_items();
		encoded->set_kind(static_cast<Wire::OrderKind>(item.Kind));
		encoded->set_availability(static_cast<Wire::Availability>(item.Availability));
		encoded->set_reason(item.Reason);
		if (item.LaunchOption) {
			encodeLaunch(*encoded->mutable_launch_option(), *item.LaunchOption);
		}
	}
	return wire;
}

Collection<Orders::Eligibility> Decode(const Wire::OrderEligibilityState& wire) {
	std::vector<Orders::Eligibility> items;
	items.reserve(wire.items_size());
	for (const auto& item : wire.items()) {
		if (!Wire::OrderKind_IsValid(item.kind()) || !Wire::Availability_IsValid(item.availability())) {
			throw std::invalid_argument("Invalid order kind or availability");
		}
		Orders::Eligibility value{static_cast<Orders::OrderKind>(item.kind()),
			static_cast<Orders::Availability>(item.availability()), item.reason()};
		if (item.has_launch_option()) {
			value.LaunchOption = decodeLaunch(item.launch_option());
		}
		items.push_back(std::move(value));
	}
	return {coverage<Orders::Eligibility>(wire.coverage()), std::move(items)};
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
	encodeKnowledge(*wire.mutable_name(), value.Name);
	encodeKnowledge(*wire.mutable_type(), value.Type);
	auto* kinematics = wire.mutable_kinematics();
	encodeKnowledge(*kinematics->mutable_position(), value.Kinematics.Position);
	encodeKnowledge(*kinematics->mutable_speed_mps(), value.Kinematics.Speed);
	encodeKnowledge(*kinematics->mutable_heading_rad(), value.Kinematics.Heading);
	encodeKnowledge(*kinematics->mutable_course_over_ground_rad(), value.Kinematics.CourseOverGround);
	auto* equipment = wire.mutable_equipment();
	equipment->set_coverage(static_cast<Wire::CollectionCoverage>(value.Equipment.GetCoverage()));
	for (const auto& item : value.Equipment.Items()) {
		auto* encoded = equipment->add_items();
		encodeKnowledge(*encoded->mutable_type_id(), item.Type);
		encodeKnowledge(*encoded->mutable_active(), item.Active);
		auto* resources = encoded->mutable_resources();
		resources->set_coverage(static_cast<Wire::CollectionCoverage>(item.Resources.GetCoverage()));
		for (const auto& resource : item.Resources.Items()) {
			auto* resourceWire = resources->add_items();
			put(*resourceWire->mutable_resource_id(), resource.Id);
			encodeKnowledge(*resourceWire->mutable_amount(), resource.Amount);
		}
	}
	if (value.Navigation) {
		encodeKnowledge(*wire.mutable_navigation()->mutable_planned_route(), value.Navigation->PlannedRoute);
	}
	if (value.Aircraft) {
		encodeKnowledge(*wire.mutable_aircraft()->mutable_airborne(), value.Aircraft->Airborne);
	}
	if (value.AirOperations) {
		const auto& operations = *value.AirOperations;
		auto* airOperations = wire.mutable_air_operations();
		auto* inventory = airOperations->mutable_inventory();
		inventory->set_coverage(static_cast<Wire::CollectionCoverage>(operations.Inventory.GetCoverage()));
		for (const auto& entry : operations.Inventory.Items()) {
			auto* entryWire = inventory->add_items();
			encodeKnowledge(*entryWire->mutable_type(), entry.Type);
			encodeKnowledge(*entryWire->mutable_squadron(), entry.Squadron);
			encodeKnowledge(*entryWire->mutable_unreserved_count(), entry.UnreservedCount);
			auto* loadouts = entryWire->mutable_loadouts();
			loadouts->set_coverage(static_cast<Wire::CollectionCoverage>(entry.Loadouts.GetCoverage()));
			for (const auto& loadout : entry.Loadouts.Items()) {
				put(*loadouts->add_items(), loadout);
			}
		}
		encodeKnowledge(*airOperations->mutable_pending_operations(), operations.PendingOperations);
		encodeKnowledge(*airOperations->mutable_onboard_count(), operations.OnboardCount);
		encodeKnowledge(*airOperations->mutable_reserved_count(), operations.ReservedCount);
		encodeKnowledge(*airOperations->mutable_cooldown_count(), operations.CooldownCount);
		encodeKnowledge(*airOperations->mutable_measured_at_seconds(), operations.MeasuredAt);
	}
	if (value.Evidence.MeasuredAt) {
		wire.mutable_evidence()->mutable_measured_at_seconds()->set_known(value.Evidence.MeasuredAt->Value);
	}
	for (const auto& evidence : value.Evidence.Evidence) {
		wire.mutable_evidence()->add_evidence(evidence);
	}
}

static Obs::UnitObservation decodeUnitObservation(const Wire::UnitObservation& wire) {
	requireId(wire.identity().value());
	Obs::UnitObservation value;
	value.Identity = {wire.identity().value()};
	value.Name = decodeKnowledge<std::string>(wire.name());
	value.Type = decodeKnowledge<Catalog::UnitTypeId>(wire.type());
	const auto& kinematics = wire.kinematics();
	value.Kinematics.Position = decodeKnowledge<Physics::Position>(kinematics.position());
	value.Kinematics.Speed = decodeKnowledge<Physics::MetersPerSecond>(kinematics.speed_mps());
	value.Kinematics.Heading = decodeKnowledge<Physics::Radians>(kinematics.heading_rad());
	value.Kinematics.CourseOverGround = decodeKnowledge<Physics::Radians>(kinematics.course_over_ground_rad());
	std::vector<Obs::EquipmentObservation> equipment;
	equipment.reserve(wire.equipment().items_size());
	for (const auto& item : wire.equipment().items()) {
		Obs::EquipmentObservation decoded;
		decoded.Type = decodeKnowledge<Catalog::EquipmentTypeId>(item.type_id());
		decoded.Active = decodeKnowledge<bool>(item.active());
		std::vector<Obs::EquipmentObservation::Resource> resources;
		resources.reserve(item.resources().items_size());
		for (const auto& resource : item.resources().items()) {
			resources.push_back({{resource.resource_id().value()}, decodeKnowledge<Catalog::ResourceValue>(resource.amount())});
		}
		decoded.Resources = {coverage<Obs::EquipmentObservation::Resource>(item.resources().coverage()), std::move(resources)};
		equipment.push_back(std::move(decoded));
	}
	value.Equipment = {coverage<Obs::EquipmentObservation>(wire.equipment().coverage()), std::move(equipment)};
	if (wire.has_navigation()) {
		value.Navigation = Obs::NavigationObservation{decodeKnowledge<std::vector<Obs::ObservedRoutePoint>>(wire.navigation().planned_route())};
	}
	if (wire.has_aircraft()) {
		value.Aircraft = Obs::AircraftObservation{decodeKnowledge<bool>(wire.aircraft().airborne())};
	}
	if (wire.has_air_operations()) {
		const auto& encoded = wire.air_operations();
		Obs::AirOperationsObservation operations;
		std::vector<Obs::AirOperationsObservation::AircraftInventoryEntry> inventory;
		inventory.reserve(encoded.inventory().items_size());
		for (const auto& entry : encoded.inventory().items()) {
			Obs::AirOperationsObservation::AircraftInventoryEntry decoded;
			decoded.Type = decodeKnowledge<Catalog::UnitTypeId>(entry.type());
			decoded.Squadron = decodeKnowledge<std::string>(entry.squadron());
			decoded.UnreservedCount = decodeKnowledge<uint32_t>(entry.unreserved_count());
			std::vector<Catalog::LoadoutId> loadouts;
			loadouts.reserve(entry.loadouts().items_size());
			for (const auto& loadout : entry.loadouts().items()) {
				loadouts.push_back({loadout.value()});
			}
			decoded.Loadouts = {coverage<Catalog::LoadoutId>(entry.loadouts().coverage()), std::move(loadouts)};
			inventory.push_back(std::move(decoded));
		}
		operations.Inventory = {coverage<Obs::AirOperationsObservation::AircraftInventoryEntry>(encoded.inventory().coverage()), std::move(inventory)};
		operations.PendingOperations = decodeKnowledge<uint32_t>(encoded.pending_operations());
		operations.OnboardCount = decodeKnowledge<uint32_t>(encoded.onboard_count());
		operations.ReservedCount = decodeKnowledge<uint32_t>(encoded.reserved_count());
		operations.CooldownCount = decodeKnowledge<uint32_t>(encoded.cooldown_count());
		operations.MeasuredAt = decodeKnowledge<Physics::Seconds>(encoded.measured_at_seconds());
		value.AirOperations = std::move(operations);
	}
	if (wire.has_evidence() && wire.evidence().measured_at_seconds().has_known()) {
		value.Evidence.MeasuredAt = Physics::Seconds{wire.evidence().measured_at_seconds().known()};
	}
	for (const auto& evidence : wire.evidence().evidence()) {
		value.Evidence.Evidence.push_back(evidence);
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
	for (const auto& unit : value.Units) {
		encodeUnitObservation(*wire.add_units(), unit);
	}
	auto* memberships = wire.mutable_memberships();
	memberships->set_coverage(static_cast<Wire::CollectionCoverage>(value.Memberships.GetCoverage()));
	for (const auto& item : value.Memberships.Items()) {
		auto* relationship = memberships->add_items();
		put(*relationship->mutable_observation(), item.Observation);
		encodeKnowledge(*relationship->mutable_faction(), item.Faction);
		encodeKnowledge(*relationship->mutable_task_force(), item.TaskForce);
	}
	return wire;
}

World::ObserverSnapshot Decode(const Wire::ObserverSnapshot& wire) {
	if (wire.schema_version() != 4) {
		throw std::invalid_argument("Unsupported ObserverSnapshot schema version; expected 4");
	}
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
		if (!identities.insert(decoded.Identity.Value).second) {
			throw std::invalid_argument("Duplicate ObservationId in observer snapshot");
		}
		value.Units.push_back(std::move(decoded));
	}
	std::vector<World::ObserverUnitRelationship> memberships;
	memberships.reserve(wire.memberships().items_size());
	for (const auto& relationship : wire.memberships().items()) {
		requireId(relationship.observation().value());
		memberships.push_back({{relationship.observation().value()}, decodeKnowledge<World::FactionId>(relationship.faction()),
			decodeKnowledge<World::TaskForceId>(relationship.task_force())});
	}
	value.Memberships = {coverage<World::ObserverUnitRelationship>(wire.memberships().coverage()), std::move(memberships)};
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
	for (const auto& unit : value.Snapshot.Units) {
		observations.insert(unit.Identity.Value);
	}
	std::unordered_set<std::string_view> overlays;
	value.OrderAvailability.reserve(wire.order_availability_size());
	for (const auto& discovery : wire.order_availability()) {
		if (discovery.session_id() != value.Snapshot.SessionId || discovery.observer().value() != value.Snapshot.Observer.Value ||
			discovery.based_on_version() != value.Snapshot.Version) {
			throw std::invalid_argument("Order overlay scope/version differs from ObserverSnapshot");
		}
		if (!observations.contains(discovery.observation().value())) {
			throw std::invalid_argument("Order overlay references an absent observation");
		}
		if (!overlays.insert(discovery.observation().value()).second) {
			throw std::invalid_argument("Duplicate order overlay for ObservationId");
		}
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
	} else {
		encodeLaunch(*wire.mutable_launch_aircraft(), std::get<Orders::LaunchAircraft>(value.Payload));
	}
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
	if (wire.has_waypoint()) {
		value.Payload = Orders::RouteCommand{Physics::Degrees{wire.waypoint().latitude_deg()},
			Physics::Degrees{wire.waypoint().longitude_deg()}, static_cast<Orders::RouteMode>(wire.waypoint().route_mode())};
	} else if (wire.has_launch_aircraft()) {
		value.Payload = decodeLaunch(wire.launch_aircraft());
	} else {
		throw std::invalid_argument("Order request requires exactly one typed payload");
	}
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
	if (value.Displacement) {
		wire.set_displacement_m(value.Displacement->Value);
	}
	for (const auto& point : value.RouteBefore) {
		put(*wire.add_route_before(), point);
	}
	for (const auto& point : value.RouteAfter) {
		put(*wire.add_route_after(), point);
	}
	return wire;
}

Orders::Result Decode(const Wire::OrderResult& wire) {
	if (!Wire::Stage_IsValid(wire.stage()) || !Wire::RouteMode_IsValid(wire.route_mode())) {
		throw std::invalid_argument("Invalid order result enum");
	}
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
	if (wire.has_displacement_m()) {
		value.Displacement = Physics::Meters{wire.displacement_m()};
	}
	for (const auto& point : wire.route_before()) {
		value.RouteBefore.push_back(get<Obs::ObservedRoutePoint>(point));
	}
	for (const auto& point : wire.route_after()) {
		value.RouteAfter.push_back(get<Obs::ObservedRoutePoint>(point));
	}
	return value;
}
}
