#pragma once

#include <NavalCommand/Equipment/Api/Type.h>
#include <NavalCommand/Error/Api/Result.h>
#include <NavalCommand/Factions/Api/Alignment.h>
#include <NavalCommand/Observations/Api/Instance.h>
#include <NavalCommand/Operations/Api/UnitState.h>
#include <NavalCommand/Resources/Api/Definition.h>
#include <NavalCommand/Units/Api/Instance.h>
#include <NavalCommand/Units/Api/Type.h>
#include <navalcommand/gateway/v2/domain.pb.h>

namespace NavalCommand::Gateway::Api
{
	namespace Wire = navalcommand::gateway::v2;
	Wire::Position Encode(const Physics::Api::Position& value);
	Physics::Api::Position Decode(const Wire::Position& value);
	Wire::Unit Encode(const Units::Api::Instance& value);
	Units::Api::Instance Decode(const Wire::Unit& value);
	Wire::Observation Encode(const Observations::Api::Instance& value);
	Observations::Api::Instance Decode(const Wire::Observation& value);
	Wire::Operations Encode(const Operations::Api::UnitState& value);
	Operations::Api::UnitState Decode(const Wire::Operations& value);
	Wire::UnitType Encode(const Units::Api::Type& value);
	Units::Api::Type Decode(const Wire::UnitType& value);
	Wire::EquipmentType Encode(const Equipment::Api::Type& value);
	Equipment::Api::Type Decode(const Wire::EquipmentType& value);
	Wire::ResourceDefinition Encode(const Resources::Api::Definition& value);
	Resources::Api::Definition Decode(const Wire::ResourceDefinition& value);
	Wire::InfrastructureError Encode(const Error::Api::Error& value);
	Error::Api::Error Decode(const Wire::InfrastructureError& value);
}
