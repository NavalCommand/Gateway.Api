#include <NavalCommand/Gateway/Api/Codec.h>
#include <stdexcept>

namespace NavalCommand::Gateway::Api
{
	namespace
	{
		template <class Enumeration> Enumeration DecodeEnum(int value, int maximum)
		{
			if (value < 0 || value > maximum)
			{
				throw std::invalid_argument("invalid_enum");
			}
			return static_cast<Enumeration>(value);
		}

		template <class Enumeration> int EncodeEnum(Enumeration value, int maximum)
		{
			const int encoded = static_cast<int>(value);
			if (encoded < 0 || encoded > maximum)
			{
				throw std::invalid_argument("invalid_enum");
			}
			return encoded;
		}

		Wire::ResourceValue EncodeResource(const Resources::Api::Value& value)
		{
			Wire::ResourceValue result;
			if (auto discrete = std::get_if<std::uint64_t>(&value))
			{
				result.set_discrete(*discrete);
			}
			else
			{
				result.set_continuous(std::get<double>(value));
			}
			return result;
		}

		Resources::Api::Value DecodeResource(const Wire::ResourceValue& value)
		{
			if (value.has_discrete())
			{
				return value.discrete();
			}
			if (value.has_continuous())
			{
				return value.continuous();
			}
			throw std::invalid_argument("missing_resource_value");
		}
	}

	Wire::Position Encode(const Physics::Api::Position& value)
	{
		Wire::Position result;
		result.set_latitude_degrees(value.Latitude.Value);
		result.set_longitude_degrees(value.Longitude.Value);
		result.set_elevation_meters(value.Elevation.Value);
		return result;
	}

	Physics::Api::Position Decode(const Wire::Position& value)
	{
		return {{value.latitude_degrees()}, {value.longitude_degrees()}, {value.elevation_meters()}};
	}

	Wire::Unit Encode(const Units::Api::Instance& value)
	{
		Wire::Unit result;
		result.set_id(value.Identity.Value);
		result.set_name(value.Name);
		result.set_type(value.Type.Value);
		result.set_current_loadout(value.CurrentLoadout.Value);
		result.set_is_destroyed(value.IsDestroyed);
		result.set_faction(value.Faction.Value);
		auto& kinematics = *result.mutable_kinematics();
		if (value.Kinematics.Position)
		{
			*kinematics.mutable_position() = Encode(*value.Kinematics.Position);
		}
		if (value.Kinematics.Speed)
		{
			kinematics.set_speed_meters_per_second(value.Kinematics.Speed->Value);
		}
		if (value.Kinematics.Heading)
		{
			kinematics.set_heading_radians(value.Kinematics.Heading->Value);
		}
		if (value.Kinematics.CourseOverGround)
		{
			kinematics.set_course_radians(value.Kinematics.CourseOverGround->Value);
		}

		if (value.Aircraft)
		{
			auto& aircraft = *result.mutable_aircraft();
			if (value.Aircraft->Airborne)
			{
				aircraft.set_airborne(*value.Aircraft->Airborne);
			}
		}
		for (const auto& equipment : value.Equipment)
		{
			auto& entry = *result.add_equipment();
			entry.set_type(equipment.Type.Value);
			entry.set_operational_state(static_cast<int>(equipment.State));
			entry.set_damage(static_cast<int>(equipment.DamageState));
			if (equipment.Inventory)
			{
				entry.mutable_inventory()->set_resource_id(equipment.Inventory->ResourceId.Value);
				*entry.mutable_inventory()->mutable_quantity() = EncodeResource(equipment.Inventory->Quantity);
			}
		}
		return result;
	}

	Units::Api::Instance Decode(const Wire::Unit& value)
	{
		Units::Api::Instance result{};
		result.Identity.Value = value.id();
		result.Name = value.name();
		result.Type.Value = value.type();
		result.CurrentLoadout.Value = value.current_loadout();
		result.IsDestroyed = value.is_destroyed();
		result.Faction.Value = value.faction();
		const auto& kinematics = value.kinematics();
		if (kinematics.has_position())
		{
			result.Kinematics.Position = Decode(kinematics.position());
		}
		if (kinematics.has_speed_meters_per_second())
		{
			result.Kinematics.Speed = Physics::Api::MetersPerSecond{kinematics.speed_meters_per_second()};
		}
		if (kinematics.has_heading_radians())
		{
			result.Kinematics.Heading = Physics::Api::Radians{kinematics.heading_radians()};
		}
		if (kinematics.has_course_radians())
		{
			result.Kinematics.CourseOverGround = Physics::Api::Radians{kinematics.course_radians()};
		}

		if (value.has_aircraft())
		{
			result.Aircraft.emplace();
			if (value.aircraft().has_airborne())
			{
				result.Aircraft->Airborne = value.aircraft().airborne();
			}
		}
		for (const auto& equipment : value.equipment())
		{
			Equipment::Api::Instance decoded{
				{equipment.type()}, DecodeEnum<Equipment::Api::OperationalState>(equipment.operational_state(), 2), DecodeEnum<Equipment::Api::Damage>(equipment.damage(), 2), {}};
			if (equipment.has_inventory())
			{
				decoded.Inventory = Equipment::Api::Instance::ResourceInventory{{equipment.inventory().resource_id()}, DecodeResource(equipment.inventory().quantity())};
			}
			result.Equipment.push_back(std::move(decoded));
		}
		return result;
	}

	Wire::Observation Encode(const Observations::Api::Instance& value)
	{
		Wire::Observation result;
		result.set_id(value.Identity.Value);
		result.set_name(value.ObservationName);
		result.set_observer_faction(value.ObserverFaction.Value);
		if (value.ObservedFaction.IsKnown())
		{
			result.set_observed_faction(value.ObservedFaction.Value().Value);
		}
		if (auto type = value.Type.GetIf<Units::Api::TypeId>())
		{
			result.set_type(type->Value);
		}
		if (auto subCategory = value.Type.GetIf<Classification::Api::SubCategory>())
		{
			result.set_sub_category(EncodeEnum(*subCategory, static_cast<int>(Classification::Api::SubCategory::Other)));
		}
		else if (auto category = value.Type.GetIf<Classification::Api::Category>())
		{
			result.set_category(EncodeEnum(*category, static_cast<int>(Classification::Api::Category::Other)));
		}
		else if (auto dimension = value.Type.GetIf<Classification::Api::Dimension>())
		{
			result.set_dimension(EncodeEnum(*dimension, static_cast<int>(Classification::Api::Dimension::Other)));
		}
		if (value.CurrentLoadout.IsKnown())
		{
			result.set_current_loadout(value.CurrentLoadout.Value().Value);
		}
		if (value.IsDestroyed.IsKnown())
		{
			result.set_is_destroyed(value.IsDestroyed.Value());
		}
		auto& kinematics = *result.mutable_kinematics();
		auto& position = *kinematics.mutable_position();
		if (auto exact = std::get_if<Physics::Api::Position>(&value.Kinematics.Position))
		{
			*position.mutable_exact() = Encode(*exact);
		}
		else
		{
			const auto& partial = std::get<Observations::Api::PartiallyLocalizedPosition>(value.Kinematics.Position);
			auto& encoded = *position.mutable_partial();
			if (partial.Latitude)
			{
				encoded.set_latitude_degrees(partial.Latitude->Value);
			}
			if (partial.Longitude)
			{
				encoded.set_longitude_degrees(partial.Longitude->Value);
			}
			if (partial.Elevation)
			{
				encoded.set_elevation_meters(partial.Elevation->Value);
			}

			if (partial.ErrorEllipse)
			{
				auto& ellipse = *encoded.mutable_error_ellipse();
				ellipse.set_major_axis_meters(partial.ErrorEllipse->MajorAxis.Value);
				ellipse.set_minor_axis_meters(partial.ErrorEllipse->MinorAxis.Value);
				ellipse.set_orientation_radians(partial.ErrorEllipse->Orientation.Value);
			}
		}
		if (value.Kinematics.Speed.IsKnown())
		{
			kinematics.set_speed_meters_per_second(value.Kinematics.Speed.Value().Value);
		}
		if (value.Kinematics.Heading.IsKnown())
		{
			kinematics.set_heading_radians(value.Kinematics.Heading.Value().Value);
		}
		if (value.Kinematics.CourseOverGround.IsKnown())
		{
			kinematics.set_course_radians(value.Kinematics.CourseOverGround.Value().Value);
		}

		if (value.Aircraft)
		{
			auto& aircraft = *result.mutable_aircraft();
			if (value.Aircraft->Airborne.IsKnown())
			{
				aircraft.set_airborne(value.Aircraft->Airborne.Value());
			}
		}
		for (const auto& contributor : value.Contributors)
		{
			auto& encoded = *result.add_contributors();
			encoded.set_observer_unit_id(contributor.Observer.Value);
			for (const auto& sensor : contributor.Sensors)
			{
				encoded.add_sensors(sensor.Value);
			}
		}
		return result;
	}

	Observations::Api::Instance Decode(const Wire::Observation& value)
	{
		Observations::Api::Instance result{};
		result.Identity.Value = value.id();
		result.ObservationName = value.name();
		result.ObserverFaction.Value = value.observer_faction();
		if (value.has_observed_faction())
		{
			result.ObservedFaction = decltype(result.ObservedFaction)::Known({value.observed_faction()});
		}
		if (value.has_type())
		{
			result.Type = decltype(result.Type)::Known(Units::Api::TypeId{value.type()});
		}
		if (value.has_sub_category())
		{
			result.Type = decltype(result.Type)::Known(
				DecodeEnum<Classification::Api::SubCategory>(value.sub_category(), static_cast<int>(Classification::Api::SubCategory::Other)));
		}
		else if (value.has_category())
		{
			result.Type = decltype(result.Type)::Known(
				DecodeEnum<Classification::Api::Category>(value.category(), static_cast<int>(Classification::Api::Category::Other)));
		}
		else if (value.has_dimension())
		{
			result.Type = decltype(result.Type)::Known(
				DecodeEnum<Classification::Api::Dimension>(value.dimension(), static_cast<int>(Classification::Api::Dimension::Other)));
		}
		if (value.has_current_loadout())
		{
			result.CurrentLoadout = decltype(result.CurrentLoadout)::Known({value.current_loadout()});
		}
		if (value.has_is_destroyed())
		{
			result.IsDestroyed = decltype(result.IsDestroyed)::Known(value.is_destroyed());
		}
		const auto& kinematics = value.kinematics();
		const auto& position = kinematics.position();
		if (position.has_exact())
		{
			result.Kinematics.Position = Decode(position.exact());
		}
		else if (position.has_partial())
		{
			Observations::Api::PartiallyLocalizedPosition partial;
			const auto& encoded = position.partial();
			if (encoded.has_latitude_degrees())
			{
				partial.Latitude = Physics::Api::Degrees{encoded.latitude_degrees()};
			}
			if (encoded.has_longitude_degrees())
			{
				partial.Longitude = Physics::Api::Degrees{encoded.longitude_degrees()};
			}
			if (encoded.has_elevation_meters())
			{
				partial.Elevation = Physics::Api::Meters{encoded.elevation_meters()};
			}

			if (encoded.has_error_ellipse())
			{
				const auto& ellipse = encoded.error_ellipse();
				partial.ErrorEllipse = Observations::Api::ObservedErrorEllipse{{ellipse.major_axis_meters()}, {ellipse.minor_axis_meters()}, {ellipse.orientation_radians()}};
			}
			result.Kinematics.Position = partial;
		}
		else
		{
			throw std::invalid_argument("missing_observation_position");
		}
		if (kinematics.has_speed_meters_per_second())
		{
			result.Kinematics.Speed = decltype(result.Kinematics.Speed)::Known(Physics::Api::MetersPerSecond{kinematics.speed_meters_per_second()});
		}
		if (kinematics.has_heading_radians())
		{
			result.Kinematics.Heading = decltype(result.Kinematics.Heading)::Known(Physics::Api::Radians{kinematics.heading_radians()});
		}
		if (kinematics.has_course_radians())
		{
			result.Kinematics.CourseOverGround = decltype(result.Kinematics.CourseOverGround)::Known(Physics::Api::Radians{kinematics.course_radians()});
		}

		if (value.has_aircraft())
		{
			result.Aircraft.emplace();
			if (value.aircraft().has_airborne())
			{
				result.Aircraft->Airborne = decltype(result.Aircraft->Airborne)::Known(value.aircraft().airborne());
			}
		}
		for (const auto& contributor : value.contributors())
		{
			Observations::Api::Contributor decoded{{contributor.observer_unit_id()}, {}};
			for (const auto& sensor : contributor.sensors())
			{
				decoded.Sensors.push_back({sensor});
			}
			result.Contributors.push_back(std::move(decoded));
		}
		return result;
	}

	Wire::Operations Encode(const Operations::Api::UnitState& value)
	{
		Wire::Operations result;
		if (std::holds_alternative<Operations::Api::Idle>(value.CurrentActivity))
		{
			result.mutable_idle();
		}
		if (auto moving = std::get_if<Operations::Api::MovingToWaypoint>(&value.CurrentActivity))
		{
			*result.mutable_moving_to_waypoint() = Encode(moving->TargetPosition);
		}
		if (auto engaging = std::get_if<Operations::Api::EngagingWith>(&value.CurrentActivity))
		{
			auto& encoded = *result.mutable_engaging();
			encoded.set_equipment_type(engaging->EquipmentType.Value);
			if (auto observation = std::get_if<Observations::Api::Id>(&engaging->Target))
			{
				encoded.set_observation_id(observation->Value);
			}
			if (auto position = std::get_if<Physics::Api::Position>(&engaging->Target))
			{
				*encoded.mutable_position() = Encode(*position);
			}
			if (auto bearing = std::get_if<Physics::Api::Radians>(&engaging->Target))
			{
				encoded.set_bearing_radians(bearing->Value);
			}
		}
		if (auto evading = std::get_if<Operations::Api::EvasiveManeuvers>(&value.CurrentActivity))
		{
			auto& encoded = *result.mutable_evading();
			if (evading->DangerSource.IsKnown())
			{
				encoded.set_danger_observation_id(evading->DangerSource.Value().Value);
			}
		}
		if (value.NavigationState)
		{
			auto& navigation = *result.mutable_navigation();
			for (const auto& waypoint : value.NavigationState->PlannedRoute)
			{
				auto& encoded = *navigation.add_route();
				*encoded.mutable_target() = Encode(waypoint.TargetPosition);
				encoded.set_label(waypoint.Label);
			}
		}
		if (value.AirOperationsState)
		{
			auto& air = *result.mutable_air_operations();
			const auto& source = *value.AirOperationsState;
			if (source.Inventory)
			{
				auto& inventory = *air.mutable_inventory();
				for (const auto& entry : *source.Inventory)
				{
					auto& encoded = *inventory.add_entries();
					encoded.set_type(entry.Type.Value);
					encoded.set_squadron(entry.Squadron);
					if (entry.UnreservedCount)
					{
						encoded.set_unreserved_count(*entry.UnreservedCount);
					}
					if (entry.Loadouts)
					{
						auto& loadouts = *encoded.mutable_loadouts();
						for (const auto& loadout : *entry.Loadouts)
						{
							loadouts.add_values(loadout.Value);
						}
					}
				}
			}
			if (source.PendingOperations)
			{
				air.set_pending_operations(*source.PendingOperations);
			}
			if (source.OnboardCount)
			{
				air.set_onboard_count(*source.OnboardCount);
			}
			if (source.ReservedCount)
			{
				air.set_reserved_count(*source.ReservedCount);
			}
			if (source.CooldownCount)
			{
				air.set_cooldown_count(*source.CooldownCount);
			}
		}
		return result;
	}

	Operations::Api::UnitState Decode(const Wire::Operations& value)
	{
		Operations::Api::UnitState result{};
		if (value.has_idle())
		{
			result.CurrentActivity = Operations::Api::Idle{};
		}
		if (value.has_moving_to_waypoint())
		{
			result.CurrentActivity = Operations::Api::MovingToWaypoint{Decode(value.moving_to_waypoint())};
		}
		if (value.has_engaging())
		{
			const auto& encoded = value.engaging();
			Operations::Api::EngagingWith activity{};
			activity.EquipmentType.Value = encoded.equipment_type();
			if (encoded.has_observation_id())
			{
				activity.Target = Observations::Api::Id{encoded.observation_id()};
			}
			if (encoded.has_position())
			{
				activity.Target = Decode(encoded.position());
			}
			if (encoded.has_bearing_radians())
			{
				activity.Target = Physics::Api::Radians{encoded.bearing_radians()};
			}
			result.CurrentActivity = activity;
		}
		if (value.has_evading())
		{
			Operations::Api::EvasiveManeuvers activity{};
			if (value.evading().has_danger_observation_id())
			{
				activity.DangerSource = decltype(activity.DangerSource)::Known({value.evading().danger_observation_id()});
			}
			result.CurrentActivity = activity;
		}
		if (value.has_navigation())
		{
			result.NavigationState.emplace();
			for (const auto& waypoint : value.navigation().route())
			{
				result.NavigationState->PlannedRoute.push_back({Decode(waypoint.target()), waypoint.label()});
			}
		}
		if (value.has_air_operations())
		{
			auto& air = result.AirOperationsState.emplace();
			const auto& source = value.air_operations();
			if (source.has_inventory())
			{
				air.Inventory.emplace();
				for (const auto& entry : source.inventory().entries())
				{
					Operations::Api::AircraftInventoryEntry decoded{{entry.type()}, entry.squadron(), {}, {}};
					if (entry.has_unreserved_count())
					{
						decoded.UnreservedCount = entry.unreserved_count();
					}
					if (entry.has_loadouts())
					{
						decoded.Loadouts.emplace();
						for (const auto& loadout : entry.loadouts().values())
						{
							decoded.Loadouts->push_back({loadout});
						}
					}
					air.Inventory->push_back(std::move(decoded));
				}
			}
			if (source.has_pending_operations())
			{
				air.PendingOperations = source.pending_operations();
			}
			if (source.has_onboard_count())
			{
				air.OnboardCount = source.onboard_count();
			}
			if (source.has_reserved_count())
			{
				air.ReservedCount = source.reserved_count();
			}
			if (source.has_cooldown_count())
			{
				air.CooldownCount = source.cooldown_count();
			}
		}
		return result;
	}

	Wire::UnitType Encode(const Units::Api::Type& value)
	{
		Wire::UnitType result;
		result.set_id(value.Id.Value);
		result.set_sub_category(EncodeEnum(value.UnitSubCategory, static_cast<int>(Classification::Api::SubCategory::Other)));
		if (value.DisplayName)
		{
			result.set_display_name(*value.DisplayName);
		}
		if (value.AlternativeDisplayName)
		{
			result.set_alternative_display_name(*value.AlternativeDisplayName);
		}
		for (const auto role : value.PlatformRoles)
		{
			result.add_platform_roles(EncodeEnum(role, static_cast<int>(Classification::Api::Role::Decoy)));
		}
		for (const auto& loadout : value.Loadouts)
		{
			auto& encoded = *result.add_loadouts();
			encoded.set_id(loadout.Id.Value);
			if (loadout.DisplayName)
			{
				encoded.set_display_name(*loadout.DisplayName);
			}
			for (const auto role : loadout.Roles)
			{
				encoded.add_roles(EncodeEnum(role, static_cast<int>(Classification::Api::Role::Decoy)));
			}
			for (const auto& fit : loadout.Equipment)
			{
				auto& equipment = *encoded.add_equipment();
				equipment.set_type(fit.TypeId.Value);
				if (fit.Resource)
				{
					equipment.mutable_resource()->set_resource_id(fit.Resource->ResourceId.Value);
					*equipment.mutable_resource()->mutable_capacity() = EncodeResource(fit.Resource->Capacity);
					*equipment.mutable_resource()->mutable_initial_inventory() = EncodeResource(fit.Resource->InitialInventory);
				}
			}
		}
		return result;
	}

	Units::Api::Type Decode(const Wire::UnitType& value)
	{
		if (!value.has_sub_category())
		{
			throw std::invalid_argument("missing_unit_sub_category");
		}
		Units::Api::Type result{};
		result.Id = {value.id()};
		if (value.has_display_name())
		{
			result.DisplayName = value.display_name();
		}
		if (value.has_alternative_display_name())
		{
			result.AlternativeDisplayName = value.alternative_display_name();
		}
		result.UnitSubCategory = DecodeEnum<Classification::Api::SubCategory>(value.sub_category(), static_cast<int>(Classification::Api::SubCategory::Other));
		for (const int role : value.platform_roles())
		{
			result.PlatformRoles.push_back(DecodeEnum<Classification::Api::Role>(role, static_cast<int>(Classification::Api::Role::Decoy)));
		}
		for (const auto& loadout : value.loadouts())
		{
			Equipment::Api::Loadout decoded{};
			decoded.Id = {loadout.id()};
			if (loadout.has_display_name())
			{
				decoded.DisplayName = loadout.display_name();
			}
			for (const int role : loadout.roles())
			{
				decoded.Roles.push_back(DecodeEnum<Classification::Api::Role>(role, static_cast<int>(Classification::Api::Role::Decoy)));
			}
			for (const auto& fit : loadout.equipment())
			{
				Equipment::Api::Fit equipment{{fit.type()}, {}};
				if (fit.has_resource())
				{
					equipment.Resource = Equipment::Api::Fit::ResourceProvision{
						{fit.resource().resource_id()}, DecodeResource(fit.resource().capacity()), DecodeResource(fit.resource().initial_inventory())};
				}
				decoded.Equipment.push_back(std::move(equipment));
			}
			result.Loadouts.push_back(std::move(decoded));
		}
		return result;
	}

	Wire::EquipmentType Encode(const Equipment::Api::Type& value)
	{
		Wire::EquipmentType result;
		result.set_id(value.Id.Value);
		if (value.DisplayName)
		{
			result.set_display_name(*value.DisplayName);
		}
		if (value.AlternativeDisplayName)
		{
			result.set_alternative_display_name(*value.AlternativeDisplayName);
		}
		for (const auto& effect : value.Effects)
		{
			auto& encoded = *result.add_effects();
			if (auto sensing = std::get_if<Equipment::Api::Effect::Sensing>(&effect.Behavior))
			{
				auto& behavior = *encoded.mutable_sensing();
				behavior.set_mode(static_cast<int>(sensing->Mode));
				if (sensing->NominalRange)
				{
					behavior.set_nominal_range_meters(sensing->NominalRange->Value);
				}
			}
			if (auto damage = std::get_if<Equipment::Api::Effect::Damage>(&effect.Behavior))
			{
				auto& behavior = *encoded.mutable_damage();
				if (damage->ProjectileGuidance)
				{
					behavior.set_guidance(static_cast<int>(*damage->ProjectileGuidance));
				}
			}
			if (std::holds_alternative<Equipment::Api::Effect::Jamming>(effect.Behavior))
			{
				encoded.mutable_jamming();
			}
			if (std::holds_alternative<Equipment::Api::Effect::Deception>(effect.Behavior))
			{
				encoded.mutable_deception();
			}
			if (std::holds_alternative<Equipment::Api::Effect::Communication>(effect.Behavior))
			{
				encoded.mutable_communication();
			}
			if (std::holds_alternative<Equipment::Api::Effect::Storage>(effect.Behavior))
			{
				encoded.mutable_storage();
			}
		}
		for (const auto& operation : value.Operations)
		{
			auto& encoded = *result.add_operations();
			encoded.set_accepted_targets(static_cast<unsigned>(operation.AcceptedTargets));
			encoded.set_invocation(static_cast<int>(operation.Invocation));
			if (operation.Resource)
			{
				encoded.mutable_resource()->set_use(static_cast<int>(operation.Resource->Use));
				encoded.mutable_resource()->set_resource_id(operation.Resource->ResourceId.Value);
				*encoded.mutable_resource()->mutable_amount() = EncodeResource(operation.Resource->Amount);
			}
		}
		return result;
	}

	Equipment::Api::Type Decode(const Wire::EquipmentType& value)
	{
		Equipment::Api::Type result{};
		result.Id = {value.id()};
		if (value.has_display_name())
		{
			result.DisplayName = value.display_name();
		}
		if (value.has_alternative_display_name())
		{
			result.AlternativeDisplayName = value.alternative_display_name();
		}
		for (const auto& effect : value.effects())
		{
			Equipment::Api::Effect decoded{};
			if (effect.has_sensing())
			{
				Equipment::Api::Effect::Sensing sensing{DecodeEnum<Equipment::Api::Effect::SensingMode>(effect.sensing().mode(), 3), {}};
				if (effect.sensing().has_nominal_range_meters())
				{
					sensing.NominalRange = Physics::Api::Meters{effect.sensing().nominal_range_meters()};
				}
				decoded.Behavior = sensing;
			}
			else if (effect.has_damage())
			{
				Equipment::Api::Effect::Damage damage;
				if (effect.damage().has_guidance())
				{
					damage.ProjectileGuidance = DecodeEnum<Equipment::Api::Effect::Damage::Guidance>(effect.damage().guidance(), 1);
				}
				decoded.Behavior = damage;
			}
			else if (effect.has_jamming())
			{
				decoded.Behavior = Equipment::Api::Effect::Jamming{};
			}
			else if (effect.has_deception())
			{
				decoded.Behavior = Equipment::Api::Effect::Deception{};
			}
			else if (effect.has_communication())
			{
				decoded.Behavior = Equipment::Api::Effect::Communication{};
			}
			else if (effect.has_storage())
			{
				decoded.Behavior = Equipment::Api::Effect::Storage{};
			}

			else
			{
				throw std::invalid_argument("missing_effect");
			}
			result.Effects.push_back(decoded);
		}
		for (const auto& operation : value.operations())
		{
			if (operation.accepted_targets() == 0 || operation.accepted_targets() > 15)
			{
				throw std::invalid_argument("invalid_target_mask");
			}
			Equipment::Api::Operation decoded{
				static_cast<Equipment::Api::Operation::TargetInput>(operation.accepted_targets()), DecodeEnum<Equipment::Api::Operation::Action>(operation.invocation(), 1), {}};
			if (operation.has_resource())
			{
				decoded.Resource = Equipment::Api::Operation::ResourceRequirement{DecodeEnum<Equipment::Api::Operation::ResourceUse>(operation.resource().use(), 1),
																				  {operation.resource().resource_id()},
																				  DecodeResource(operation.resource().amount())};
			}
			result.Operations.push_back(std::move(decoded));
		}
		return result;
	}

	Wire::ResourceDefinition Encode(const Resources::Api::Definition& value)
	{
		Wire::ResourceDefinition result;
		result.set_id(value.Id.Value);
		result.set_representation(static_cast<int>(value.Representation));
		result.set_unit(static_cast<int>(value.Unit));
		result.set_accounting(static_cast<int>(value.Accounting));
		return result;
	}

	Resources::Api::Definition Decode(const Wire::ResourceDefinition& value)
	{
		return {{value.id()},
				DecodeEnum<Resources::Api::Definition::NumericRepresentation>(value.representation(), 1),
				DecodeEnum<Resources::Api::Definition::MeasurementUnit>(value.unit(), 2),
				DecodeEnum<Resources::Api::Definition::AccountingSemantics>(value.accounting(), 1)};
	}

	Wire::InfrastructureError Encode(const Error::Api::Error& value)
	{
		Wire::InfrastructureError result;
		result.set_condition(static_cast<int>(value.Condition));
		if (value.HasCause())
		{
			if (value.Cause.category() != std::generic_category() && value.Cause.category() != std::system_category())
			{
				throw std::invalid_argument("unregistered_error_category");
			}
			result.set_cause_category(value.Cause.category() == std::generic_category() ? "generic" : "system");
			result.set_cause_value(value.Cause.value());
		}
		return result;
	}

	Error::Api::Error Decode(const Wire::InfrastructureError& value)
	{
		Error::Api::Error result{DecodeEnum<Error::Api::ErrorCondition>(value.condition(), 7), {}};
		if (value.cause_category() == "generic")
		{
			result.Cause = {value.cause_value(), std::generic_category()};
		}
		else if (value.cause_category() == "system")
		{
			result.Cause = {value.cause_value(), std::system_category()};
		}
		else if (!value.cause_category().empty() || value.cause_value() != 0)
		{
			throw std::invalid_argument("unregistered_error_category");
		}
		return result;
	}
}
