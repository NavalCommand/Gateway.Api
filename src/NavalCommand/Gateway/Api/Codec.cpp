#include <NavalCommand/Gateway/Api/Codec.h>
#include <NavalCommand/Orders/Api/FacilityLaunch.h>
#include <NavalCommand/Orders/Api/RouteCommand.h>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_set>

namespace NavalCommand::Gateway::Api {
namespace Physics = NavalCommand::Physics::Api;
namespace Catalog = NavalCommand::Catalog::Api;
namespace Units = NavalCommand::Units::Api;
namespace Orders = NavalCommand::Orders::Api;
namespace World = NavalCommand::World::Api;
namespace Uncertainty = NavalCommand::Uncertainty::Api;

template <class T> using Knowledge = Uncertainty::Knowledge<T>;
template <class T> using Collection = Uncertainty::ObservedCollection<T>;

static void RequireUnit(const Wire::UnitId &unit) {
	if (unit.value().empty()) {
		throw std::invalid_argument("A nonempty session-scoped UnitId is required");
	}
}

static void EncodeValue(Wire::RoutePoint &wire, const Units::RoutePoint &value) {
	wire.set_latitude_deg(value.Latitude.Value);
	wire.set_longitude_deg(value.Longitude.Value);
	wire.set_label(value.Label);
}

static void EncodeValue(Wire::PositionObservation &wire, const Units::PositionObservation &value) {
	auto coordinates = wire.mutable_coordinates();
	coordinates->set_latitude_deg(value.Coordinates.Latitude.Value);
	coordinates->set_longitude_deg(value.Coordinates.Longitude.Value);
	if (value.Coordinates.Altitude) {
		coordinates->set_altitude_m(value.Coordinates.Altitude->Value);
	}
	if (value.ErrorEllipse) {
		auto ellipse = wire.mutable_error_ellipse();
		ellipse->set_major_axis_m(value.ErrorEllipse->MajorAxis.Value);
		ellipse->set_minor_axis_m(value.ErrorEllipse->MinorAxis.Value);
		ellipse->set_orientation_rad(value.ErrorEllipse->Orientation.Value);
	}
}

static void EncodeValue(Wire::Route &wire, const std::vector<Units::RoutePoint> &value) {
	for (const auto &point : value) {
		EncodeValue(*wire.add_points(), point);
	}
}

static void EncodeValue(Wire::UnitTypeId &wire, const Catalog::UnitTypeId &value) {
	wire.set_value(value.Value);
}

static void EncodeValue(Wire::EquipmentTypeId &wire, const Catalog::EquipmentTypeId &value) {
	wire.set_value(value.Value);
}

static void EncodeValue(Wire::ResourceValue &wire, const Catalog::ResourceValue &value) {
	if (const auto discrete = std::get_if<uint64_t>(&value)) {
		wire.set_discrete(*discrete);
	} else {
		wire.set_continuous(std::get<double>(value));
	}
}

template <class T, class WireValue> static T DecodeValue(const WireValue &wire) {
	if constexpr (std::is_same_v<T, Units::PositionObservation>) {
		const auto &coordinates = wire.coordinates();
		if (!coordinates.has_latitude_deg() || !coordinates.has_longitude_deg()) {
			throw std::invalid_argument("Position coordinates are missing");
		}
		T value;
		value.Coordinates.Latitude = Physics::Degrees{coordinates.latitude_deg()};
		value.Coordinates.Longitude = Physics::Degrees{coordinates.longitude_deg()};
		if (coordinates.has_altitude_m()) {
			value.Coordinates.Altitude = Physics::Meters{coordinates.altitude_m()};
		}
		if (wire.has_error_ellipse()) {
			const auto &ellipse = wire.error_ellipse();
			value.ErrorEllipse = Units::PositionObservation::Uncertainty{
				Physics::Meters{ellipse.major_axis_m()}, Physics::Meters{ellipse.minor_axis_m()}, Physics::Radians{ellipse.orientation_rad()}};
		}
		return value;
	} else if constexpr (std::is_same_v<T, std::vector<Units::RoutePoint>>) {
		T value;
		value.reserve(wire.points_size());
		for (const auto &point : wire.points()) {
			value.push_back({Physics::Degrees{point.latitude_deg()}, Physics::Degrees{point.longitude_deg()}, point.label()});
		}
		return value;
	} else if constexpr (std::is_same_v<T, Catalog::UnitTypeId> || std::is_same_v<T, Catalog::EquipmentTypeId>) {
		return T{wire.value()};
	} else if constexpr (std::is_same_v<T, Catalog::ResourceValue>) {
		if (wire.has_discrete()) {
			return Catalog::ResourceValue{wire.discrete()};
		}
		if (wire.has_continuous()) {
			return Catalog::ResourceValue{wire.continuous()};
		}
		throw std::invalid_argument("Resource value requires discrete or continuous quantity");
	} else if constexpr (std::is_enum_v<T>) {
		return static_cast<T>(wire);
	} else {
		return T{wire};
	}
}

template <class T> static decltype(auto) ScalarValue(const T &value) {
	if constexpr (requires { value.Value; }) {
		return (value.Value);
	} else {
		return value;
	}
}

template <class WireScalar, class T> static decltype(auto) WireScalarValue(const T &value) {
	if constexpr (std::is_enum_v<WireScalar>) {
		return static_cast<WireScalar>(value);
	} else {
		return ScalarValue(value);
	}
}

template <class WireKnowledge, class T> static void EncodeKnowledge(WireKnowledge &wire, const Knowledge<T> &value) {
	if (!value.HasValue()) {
		return;
	}
	if constexpr (requires { wire.mutable_known()->Clear(); }) {
		if (value.IsKnown()) {
			EncodeValue(*wire.mutable_known(), value.Value());
		} else {
			auto estimate = wire.mutable_estimated();
			EncodeValue(*estimate->mutable_value(), value.Value());
			estimate->set_source(value.Source());
		}
	} else {
		using WireScalar = std::remove_cvref_t<decltype(wire.known())>;
		const auto &scalar = WireScalarValue<WireScalar>(value.Value());
		if (value.IsKnown()) {
			wire.set_known(scalar);
		} else {
			auto estimate = wire.mutable_estimated();
			estimate->set_value(scalar);
			estimate->set_source(value.Source());
		}
	}
}

template <class T, class WireKnowledge> static Knowledge<T> DecodeKnowledge(const WireKnowledge &wire) {
	if (wire.has_estimated() && !wire.estimated().has_value()) {
		throw std::invalid_argument("Estimated knowledge has no value");
	}
	if constexpr (std::is_same_v<T, Units::EquipmentInstance::Readiness>) {
		if ((wire.has_known() && !Wire::EquipmentReadiness_IsValid(wire.known())) ||
			(wire.has_estimated() && !Wire::EquipmentReadiness_IsValid(wire.estimated().value()))) {
			throw std::invalid_argument("Invalid equipment readiness");
		}
	}
	if constexpr (std::is_same_v<T, Units::EquipmentInstance::Damage>) {
		if ((wire.has_known() && !Wire::EquipmentDamage_IsValid(wire.known())) ||
			(wire.has_estimated() && !Wire::EquipmentDamage_IsValid(wire.estimated().value()))) {
			throw std::invalid_argument("Invalid equipment damage");
		}
	}
	if (wire.has_known()) {
		return Knowledge<T>::Known(DecodeValue<T>(wire.known()));
	}
	if (wire.has_estimated()) {
		return Knowledge<T>::Estimated(DecodeValue<T>(wire.estimated().value()), wire.estimated().source());
	}
	return {};
}

template <class T> static typename Collection<T>::Coverage DecodeCoverage(Wire::CollectionCoverage coverage) {
	if (!Wire::CollectionCoverage_IsValid(coverage)) {
		throw std::invalid_argument("Invalid collection coverage");
	}
	return static_cast<typename Collection<T>::Coverage>(coverage);
}

static void EncodeLaunch(Wire::AircraftLaunch &wire, const Orders::FacilityLaunch &value) {
	wire.mutable_unit_type_id()->set_value(value.Aircraft.TypeId.Value);
	wire.set_squadron(value.Aircraft.Squadron);
	wire.mutable_loadout_id()->set_value(value.Aircraft.Loadout.Value);
	wire.set_count(value.Count);
}

static Orders::FacilityLaunch DecodeLaunch(const Wire::AircraftLaunch &wire) {
	return {{{wire.unit_type_id().value()}, wire.squadron(), {wire.loadout_id().value()}}, wire.count()};
}

Wire::OrderEligibilityState Encode(const Collection<Orders::Eligibility> &value) {
	Wire::OrderEligibilityState wire;
	wire.set_coverage(static_cast<Wire::CollectionCoverage>(value.GetCoverage()));
	for (const auto &item : value.Items()) {
		auto target = wire.add_items();
		target->set_kind(static_cast<Wire::OrderKind>(item.Kind));
		target->set_availability(static_cast<Wire::Availability>(item.Availability));
		target->set_reason(item.Reason);
		if (item.LaunchOption) {
			EncodeLaunch(*target->mutable_launch_option(), *item.LaunchOption);
		}
	}
	return wire;
}

Collection<Orders::Eligibility> Decode(const Wire::OrderEligibilityState &wire) {
	std::vector<Orders::Eligibility> items;
	items.reserve(wire.items_size());
	for (const auto &item : wire.items()) {
		if (!Wire::OrderKind_IsValid(item.kind()) || !Wire::Availability_IsValid(item.availability())) {
			throw std::invalid_argument("Invalid typed order availability");
		}
		Orders::Eligibility value{static_cast<Orders::OrderKind>(item.kind()), static_cast<Orders::Availability>(item.availability()), item.reason()};
		if (item.has_launch_option()) {
			value.LaunchOption = DecodeLaunch(item.launch_option());
		}
		items.push_back(std::move(value));
	}
	return {DecodeCoverage<Orders::Eligibility>(wire.coverage()), std::move(items)};
}

Wire::OrderDiscovery Encode(const Orders::OrderAvailability &value) {
	Wire::OrderDiscovery wire;
	wire.mutable_unit_id()->set_value(value.Unit.Value);
	RequireUnit(wire.unit_id());
	wire.set_based_on_version(value.BasedOnVersion);
	*wire.mutable_eligibility() = Encode(value.Orders);
	return wire;
}

Orders::OrderAvailability Decode(const Wire::OrderDiscovery &wire) {
	RequireUnit(wire.unit_id());
	return {{wire.unit_id().value()}, wire.based_on_version(), Decode(wire.eligibility())};
}

static void EncodeAirOperations(Wire::AirOperations &wire, const Units::AirOperations &value) {
	auto inventory = wire.mutable_inventory();
	inventory->set_coverage(static_cast<Wire::CollectionCoverage>(value.Inventory.GetCoverage()));
	for (const auto &item : value.Inventory.Items()) {
		auto entry = inventory->add_items();
		entry->mutable_unit_type_id()->set_value(item.TypeId.Value);
		entry->set_squadron(item.Squadron);
		EncodeKnowledge(*entry->mutable_unreserved_count(), item.UnreservedCount);
		for (const auto &loadout : item.Loadouts) {
			entry->add_loadouts()->set_value(loadout.Value);
		}
	}
	EncodeKnowledge(*wire.mutable_pending_operations(), value.PendingOperations);
	EncodeKnowledge(*wire.mutable_onboard_count(), value.OnboardCount);
	EncodeKnowledge(*wire.mutable_reserved_count(), value.ReservedCount);
	EncodeKnowledge(*wire.mutable_cooldown_count(), value.CooldownCount);
	EncodeKnowledge(*wire.mutable_measured_at_seconds(), value.MeasuredAt);
}

static Units::AirOperations DecodeAirOperations(const Wire::AirOperations &wire) {
	Units::AirOperations value;
	std::vector<Units::AircraftInventoryEntry> entries;
	entries.reserve(wire.inventory().items_size());
	for (const auto &item : wire.inventory().items()) {
		Units::AircraftInventoryEntry entry;
		entry.TypeId = {item.unit_type_id().value()};
		entry.Squadron = item.squadron();
		entry.UnreservedCount = DecodeKnowledge<uint32_t>(item.unreserved_count());
		entry.Loadouts.reserve(item.loadouts_size());
		for (const auto &loadout : item.loadouts()) {
			entry.Loadouts.push_back({loadout.value()});
		}
		entries.push_back(std::move(entry));
	}
	value.Inventory = {DecodeCoverage<Units::AircraftInventoryEntry>(wire.inventory().coverage()), std::move(entries)};
	value.PendingOperations = DecodeKnowledge<uint32_t>(wire.pending_operations());
	value.OnboardCount = DecodeKnowledge<uint32_t>(wire.onboard_count());
	value.ReservedCount = DecodeKnowledge<uint32_t>(wire.reserved_count());
	value.CooldownCount = DecodeKnowledge<uint32_t>(wire.cooldown_count());
	value.MeasuredAt = DecodeKnowledge<Physics::Seconds>(wire.measured_at_seconds());
	return value;
}

static void EncodeInstance(Wire::UnitInstance &wire, const Units::UnitInstance &value) {
	wire.mutable_unit_id()->set_value(value.Identity.Value);
	RequireUnit(wire.unit_id());
	EncodeKnowledge(*wire.mutable_display()->mutable_name(), value.Display.Name);
	EncodeKnowledge(*wire.mutable_display()->mutable_type_id(), value.Display.TypeId);
	EncodeKnowledge(*wire.mutable_kinematics()->mutable_position(), value.Kinematics.Position);
	EncodeKnowledge(*wire.mutable_kinematics()->mutable_speed_mps(), value.Kinematics.Speed);
	EncodeKnowledge(*wire.mutable_kinematics()->mutable_heading_rad(), value.Kinematics.Heading);
	EncodeKnowledge(*wire.mutable_kinematics()->mutable_course_over_ground_rad(), value.Kinematics.CourseOverGround);
	EncodeKnowledge(*wire.mutable_observation()->mutable_measured_at_seconds(), value.Observation.MeasuredAt);
	auto equipment = wire.mutable_equipment();
	equipment->set_coverage(static_cast<Wire::CollectionCoverage>(value.Equipment.GetCoverage()));
	for (const auto &item : value.Equipment.Items()) {
		auto target = equipment->add_items();
		EncodeKnowledge(*target->mutable_type_id(), item.TypeId);
		EncodeKnowledge(*target->mutable_active(), item.Active);
		EncodeKnowledge(*target->mutable_operational_readiness(), item.OperationalReadiness);
		EncodeKnowledge(*target->mutable_damage_state(), item.DamageState);
		auto resources = target->mutable_resources();
		resources->set_coverage(static_cast<Wire::CollectionCoverage>(item.Resources.GetCoverage()));
		for (const auto &resource : item.Resources.Items()) {
			auto targetResource = resources->add_items();
			targetResource->mutable_resource_id()->set_value(resource.Id.Value);
			EncodeKnowledge(*targetResource->mutable_amount(), resource.Amount);
		}
	}
	if (value.Navigation) {
		EncodeKnowledge(*wire.mutable_navigation()->mutable_planned_route(), value.Navigation->PlannedRoute);
	}
	if (value.Aircraft) {
		EncodeKnowledge(*wire.mutable_aircraft()->mutable_airborne(), value.Aircraft->Airborne);
	}
	if (value.AirOperations) {
		EncodeAirOperations(*wire.mutable_air_operations(), *value.AirOperations);
	}
}

static Units::UnitInstance DecodeInstance(const Wire::UnitInstance &wire) {
	RequireUnit(wire.unit_id());
	Units::UnitInstance value;
	value.Identity = {wire.unit_id().value()};
	value.Display.Name = DecodeKnowledge<std::string>(wire.display().name());
	value.Display.TypeId = DecodeKnowledge<Catalog::UnitTypeId>(wire.display().type_id());
	value.Kinematics.Position = DecodeKnowledge<Units::PositionObservation>(wire.kinematics().position());
	value.Kinematics.Speed = DecodeKnowledge<Physics::MetersPerSecond>(wire.kinematics().speed_mps());
	value.Kinematics.Heading = DecodeKnowledge<Physics::Radians>(wire.kinematics().heading_rad());
	value.Kinematics.CourseOverGround = DecodeKnowledge<Physics::Radians>(wire.kinematics().course_over_ground_rad());
	value.Observation.MeasuredAt = DecodeKnowledge<Physics::Seconds>(wire.observation().measured_at_seconds());
	std::vector<Units::EquipmentInstance> equipment;
	equipment.reserve(wire.equipment().items_size());
	for (const auto &item : wire.equipment().items()) {
		Units::EquipmentInstance result;
		result.TypeId = DecodeKnowledge<Catalog::EquipmentTypeId>(item.type_id());
		result.Active = DecodeKnowledge<bool>(item.active());
		result.OperationalReadiness = DecodeKnowledge<Units::EquipmentInstance::Readiness>(item.operational_readiness());
		result.DamageState = DecodeKnowledge<Units::EquipmentInstance::Damage>(item.damage_state());
		std::vector<Units::EquipmentInstance::Resource> resources;
		resources.reserve(item.resources().items_size());
		for (const auto &resource : item.resources().items()) {
			resources.push_back({{resource.resource_id().value()}, DecodeKnowledge<Catalog::ResourceValue>(resource.amount())});
		}
		result.Resources = {DecodeCoverage<Units::EquipmentInstance::Resource>(item.resources().coverage()), std::move(resources)};
		equipment.push_back(std::move(result));
	}
	value.Equipment = {DecodeCoverage<Units::EquipmentInstance>(wire.equipment().coverage()), std::move(equipment)};
	if (wire.has_navigation()) {
		value.Navigation = Units::NavigationState{DecodeKnowledge<std::vector<Units::RoutePoint>>(wire.navigation().planned_route())};
	}
	if (wire.has_aircraft()) {
		value.Aircraft = Units::AircraftState{DecodeKnowledge<bool>(wire.aircraft().airborne())};
	}
	if (wire.has_air_operations()) {
		value.AirOperations = DecodeAirOperations(wire.air_operations());
	}
	return value;
}

Wire::WorldSnapshot Encode(const World::View &value) {
	Wire::WorldSnapshot wire;
	auto session = wire.mutable_session();
	session->set_schema_version(3);
	session->set_session_id(value.SessionId);
	session->set_observer_id(value.ObserverId);
	session->set_ready(value.Ready);
	wire.set_observation_version(value.Version);
	wire.set_operational_seconds(value.CapturedAt.Value);
	wire.set_truncated(value.Truncated);
	for (const auto &instance : value.Instances) {
		EncodeInstance(*wire.add_instances(), instance);
	}
	auto memberships = wire.mutable_memberships();
	memberships->set_coverage(static_cast<Wire::CollectionCoverage>(value.Memberships.GetCoverage()));
	for (const auto &item : value.Memberships.Items()) {
		auto membership = memberships->add_items();
		membership->mutable_unit_id()->set_value(item.Unit.Value);
		RequireUnit(membership->unit_id());
		EncodeKnowledge(*membership->mutable_faction(), item.Faction);
		EncodeKnowledge(*membership->mutable_task_force(), item.TaskForce);
	}
	return wire;
}

World::View Decode(const Wire::WorldSnapshot &wire) {
	if (wire.session().schema_version() != 3) {
		throw std::invalid_argument("Unsupported world schema version; expected 3");
	}
	World::View value;
	value.SessionId = wire.session().session_id();
	value.ObserverId = wire.session().observer_id();
	value.Ready = wire.session().ready();
	value.Version = wire.observation_version();
	value.CapturedAt = Physics::Seconds{wire.operational_seconds()};
	value.Truncated = wire.truncated();
	value.Instances.reserve(wire.instances_size());
	for (const auto &instance : wire.instances()) {
		value.Instances.push_back(DecodeInstance(instance));
	}
	std::vector<World::UnitMembership> memberships;
	memberships.reserve(wire.memberships().items_size());
	for (const auto &item : wire.memberships().items()) {
		RequireUnit(item.unit_id());
		memberships.push_back({{item.unit_id().value()}, DecodeKnowledge<std::string>(item.faction()), DecodeKnowledge<std::string>(item.task_force())});
	}
	value.Memberships = {DecodeCoverage<World::UnitMembership>(wire.memberships().coverage()), std::move(memberships)};
	return value;
}

Wire::ProviderObservation Encode(const ProviderObservation &value) {
	Wire::ProviderObservation wire;
	*wire.mutable_snapshot() = Encode(value.Snapshot);
	for (const auto &item : value.OrderAvailability) {
		auto discovery = wire.add_order_availability();
		*discovery = Encode(item);
		discovery->set_session_id(value.Snapshot.SessionId);
	}
	return wire;
}

ProviderObservation Decode(const Wire::ProviderObservation &wire) {
	ProviderObservation value;
	value.Snapshot = Decode(wire.snapshot());
	std::unordered_set<std::string_view> units;
	units.reserve(value.Snapshot.Instances.size());
	for (const auto &unit : value.Snapshot.Instances) {
		if (!units.insert(unit.Identity.Value).second) {
			throw std::invalid_argument("Duplicate UnitId in provider snapshot");
		}
	}
	std::unordered_set<std::string_view> discoveredUnits;
	discoveredUnits.reserve(wire.order_availability_size());
	value.OrderAvailability.reserve(wire.order_availability_size());
	for (const auto &item : wire.order_availability()) {
		if (item.session_id() != value.Snapshot.SessionId) {
			throw std::invalid_argument("Order overlay belongs to a different session");
		}
		if (!units.contains(item.unit_id().value())) {
			throw std::invalid_argument("Order overlay references a unit outside the snapshot");
		}
		if (!discoveredUnits.insert(item.unit_id().value()).second) {
			throw std::invalid_argument("Duplicate per-unit order overlay");
		}
		if (item.based_on_version() != value.Snapshot.Version) {
			throw std::invalid_argument("Order overlay version differs from the snapshot");
		}
		value.OrderAvailability.push_back(Decode(item));
	}
	return value;
}

Wire::OrderRequest Encode(const Orders::Request &value) {
	Wire::OrderRequest wire;
	wire.set_session_id(value.SessionId);
	wire.set_request_id(value.RequestId);
	wire.mutable_unit_id()->set_value(value.Unit.Value);
	RequireUnit(wire.unit_id());
	wire.set_based_on_version(value.BasedOnVersion);
	if (const auto route = std::get_if<Orders::RouteCommand>(&value.Payload)) {
		auto waypoint = wire.mutable_waypoint();
		waypoint->set_latitude_deg(route->Latitude.Value);
		waypoint->set_longitude_deg(route->Longitude.Value);
		waypoint->set_route_mode(static_cast<Wire::RouteMode>(route->Mode));
	} else {
		EncodeLaunch(*wire.mutable_aircraft_launch(), std::get<Orders::FacilityLaunch>(value.Payload));
	}
	return wire;
}

Orders::Request Decode(const Wire::OrderRequest &wire) {
	RequireUnit(wire.unit_id());
	Orders::Request value{wire.session_id(), wire.request_id(), {wire.unit_id().value()}, wire.based_on_version()};
	if (wire.has_waypoint()) {
		value.Payload = Orders::RouteCommand{Physics::Degrees{wire.waypoint().latitude_deg()}, Physics::Degrees{wire.waypoint().longitude_deg()},
			static_cast<Orders::RouteMode>(wire.waypoint().route_mode())};
	} else if (wire.has_aircraft_launch()) {
		value.Payload = DecodeLaunch(wire.aircraft_launch());
	} else {
		throw std::invalid_argument("Order request requires exactly one typed payload");
	}
	return value;
}

Wire::OrderResult Encode(const Orders::Result &value) {
	Wire::OrderResult wire;
	wire.set_session_id(value.SessionId);
	wire.set_request_id(value.RequestId);
	wire.set_stage(static_cast<Wire::Stage>(value.Stage));
	wire.set_reason(value.Reason);
	wire.set_sampled_at_seconds(value.SampledAt.Value);
	wire.set_route_mode(static_cast<Wire::RouteMode>(value.Mode));
	if (value.SpawnedUnit) {
		wire.mutable_spawned_unit_id()->set_value(value.SpawnedUnit->Value);
		RequireUnit(wire.spawned_unit_id());
	}
	if (value.Displacement) {
		wire.set_displacement_m(value.Displacement->Value);
	}
	for (const auto &point : value.RouteBefore) {
		EncodeValue(*wire.add_route_before(), point);
	}
	for (const auto &point : value.RouteAfter) {
		EncodeValue(*wire.add_route_after(), point);
	}
	return wire;
}

Orders::Result Decode(const Wire::OrderResult &wire) {
	Orders::Result value{wire.session_id(), wire.request_id(), static_cast<Orders::Stage>(wire.stage()), wire.reason(), Physics::Seconds{wire.sampled_at_seconds()}};
	value.Mode = static_cast<Orders::RouteMode>(wire.route_mode());
	if (wire.has_spawned_unit_id()) {
		RequireUnit(wire.spawned_unit_id());
		value.SpawnedUnit = Units::UnitId{wire.spawned_unit_id().value()};
	}
	if (wire.has_displacement_m()) {
		value.Displacement = Physics::Meters{wire.displacement_m()};
	}
	value.RouteBefore.reserve(wire.route_before_size());
	for (const auto &point : wire.route_before()) {
		value.RouteBefore.push_back({Physics::Degrees{point.latitude_deg()}, Physics::Degrees{point.longitude_deg()}, point.label()});
	}
	value.RouteAfter.reserve(wire.route_after_size());
	for (const auto &point : wire.route_after()) {
		value.RouteAfter.push_back({Physics::Degrees{point.latitude_deg()}, Physics::Degrees{point.longitude_deg()}, point.label()});
	}
	return value;
}
}
