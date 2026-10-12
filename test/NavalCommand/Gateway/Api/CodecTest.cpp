#include <NavalCommand/Gateway/Api/Codec.h>
#include <gtest/gtest.h>

namespace NavalCommand::Gateway::Api
{
	TEST(Codec, UnknownAndKnownFalseRemainDistinct)
	{
		Observations::Api::Instance source{};
		source.Identity.Value = "contact-7";
		source.ObserverFaction.Value = "blue";
		source.Kinematics.Position = Observations::Api::PartiallyLocalizedPosition{};
		source.Aircraft.emplace();
		auto unknown = Decode(Encode(source));
		EXPECT_FALSE(unknown.Aircraft->Airborne.IsKnown());
		source.Aircraft->Airborne = decltype(source.Aircraft->Airborne)::Known(false);
		source.Kinematics.Speed = decltype(source.Kinematics.Speed)::Known({0});
		auto known = Decode(Encode(source));
		ASSERT_TRUE(known.Aircraft->Airborne.IsKnown());
		EXPECT_FALSE(known.Aircraft->Airborne.Value());
		EXPECT_EQ(known.Kinematics.Speed.Value().Value, 0);
		EXPECT_EQ(known.Identity.Value, "contact-7");
		EXPECT_TRUE(std::holds_alternative<Observations::Api::PartiallyLocalizedPosition>(known.Kinematics.Position));
	}

	TEST(Codec, PartialCoordinatesNeverBecomeTruth)
	{
		Observations::Api::Instance source{};
		Observations::Api::PartiallyLocalizedPosition position;
		position.Latitude = Physics::Api::Degrees{0};
		position.ErrorEllipse = Observations::Api::ObservedErrorEllipse{{100}, {20}, {1}};
		source.Kinematics.Position = position;
		source.Type = decltype(source.Type)::Known(Units::Api::Classification::Submarine);
		auto decoded = Decode(Encode(source));
		const auto& partial = std::get<Observations::Api::PartiallyLocalizedPosition>(decoded.Kinematics.Position);
		ASSERT_TRUE(partial.Latitude);
		EXPECT_EQ(partial.Latitude->Value, 0);
		EXPECT_FALSE(partial.Longitude);
		EXPECT_EQ(partial.ErrorEllipse->MajorAxis.Value, 100);
		EXPECT_EQ(*decoded.Type.GetIf<Units::Api::Classification>(), Units::Api::Classification::Submarine);
	}

	TEST(Codec, KnownEmptyOperationsAndInventoryStayPresent)
	{
		Operations::Api::UnitState source{};
		source.NavigationState.emplace();
		source.AirOperationsState.emplace();
		source.AirOperationsState->Inventory.emplace();
		source.AirOperationsState->OnboardCount = 0;
		auto decoded = Decode(Encode(source));
		ASSERT_TRUE(decoded.NavigationState);
		EXPECT_TRUE(decoded.NavigationState->PlannedRoute.empty());
		ASSERT_TRUE(decoded.AirOperationsState->Inventory);
		EXPECT_TRUE(decoded.AirOperationsState->Inventory->empty());
		EXPECT_EQ(*decoded.AirOperationsState->OnboardCount, 0);
		EXPECT_FALSE(decoded.AirOperationsState->ReservedCount);
	}

	TEST(Codec, PreservesInfrastructureErrorCause)
	{
		Error::Api::Error source{Error::Api::ErrorCondition::CommunicationError, std::make_error_code(std::errc::timed_out)};
		EXPECT_EQ(Decode(Encode(source)), source);
	}

	TEST(Codec, EquipmentResourcesRemainOptionalAndZeroIsPresent)
	{
		Units::Api::Instance source{};
		source.Equipment.push_back({{"radar"}, Equipment::Api::OperationalState::Active, Equipment::Api::Damage::Intact, {}});
		source.Equipment.push_back(
			{{"launcher"}, Equipment::Api::OperationalState::Ready, Equipment::Api::Damage::Intact, Equipment::Api::Instance::ResourceInventory{{"missiles"}, std::uint64_t{0}}});
		auto decoded = Decode(Encode(source));
		ASSERT_EQ(decoded.Equipment.size(), 2u);
		EXPECT_FALSE(decoded.Equipment[0].Inventory);
		ASSERT_TRUE(decoded.Equipment[1].Inventory);
		EXPECT_EQ(decoded.Equipment[1].Inventory->ResourceId.Value, "missiles");
		EXPECT_EQ(std::get<std::uint64_t>(decoded.Equipment[1].Inventory->Quantity), 0u);

		Units::Api::Type type{{"ship"}, Units::Api::Classification::SurfaceVessel, {}};
		type.Loadouts.push_back({{"default"}, {{{"radar"}, {}}, {{"launcher"}, Equipment::Api::Fit::ResourceProvision{{"missiles"}, std::uint64_t{8}, std::uint64_t{0}}}}});
		auto decodedType = Decode(Encode(type));
		EXPECT_FALSE(decodedType.Loadouts[0].Equipment[0].Resource);
		ASSERT_TRUE(decodedType.Loadouts[0].Equipment[1].Resource);
		EXPECT_EQ(std::get<std::uint64_t>(decodedType.Loadouts[0].Equipment[1].Resource->InitialInventory), 0u);

		Equipment::Api::Type equipment{{"radar"}, {}, {}};
		equipment.Operations.push_back({Equipment::Api::Operation::TargetInput::NoTarget, Equipment::Api::Operation::Action::SetActive, {}});
		equipment.Operations.push_back({Equipment::Api::Operation::TargetInput::Unit,
										Equipment::Api::Operation::Action::Use,
										Equipment::Api::Operation::ResourceRequirement{Equipment::Api::Operation::ResourceUse::Reserve, {"channels"}, std::uint64_t{1}}});
		auto decodedEquipment = Decode(Encode(equipment));
		EXPECT_FALSE(decodedEquipment.Operations[0].Resource);
		ASSERT_TRUE(decodedEquipment.Operations[1].Resource);
		EXPECT_EQ(decodedEquipment.Operations[1].Resource->Use, Equipment::Api::Operation::ResourceUse::Reserve);
	}

	TEST(Codec, RejectsIncompletePresentResourceGroups)
	{
		Wire::Unit unit;
		unit.add_equipment()->mutable_inventory()->set_resource_id("missiles");
		EXPECT_THROW(Decode(unit), std::invalid_argument);
		Wire::UnitType type;
		type.add_loadouts()->add_equipment()->mutable_resource()->set_resource_id("missiles");
		EXPECT_THROW(Decode(type), std::invalid_argument);
		Wire::EquipmentType equipment;
		auto operation = equipment.add_operations();
		operation->set_accepted_targets(1);
		operation->mutable_resource()->set_resource_id("missiles");
		EXPECT_THROW(Decode(equipment), std::invalid_argument);
	}

	TEST(Codec, RejectsMalformedPositionAndEnumeration)
	{
		EXPECT_THROW(Decode(Wire::Observation{}), std::invalid_argument);
		Wire::UnitType type;
		type.set_classification(999);
		EXPECT_THROW(Decode(type), std::invalid_argument);
	}
}
