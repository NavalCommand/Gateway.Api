#include <NavalCommand/Gateway/Api/Codec.h>
#include <gtest/gtest.h>
#include <utility>

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
		EXPECT_FALSE(unknown.IsDestroyed.IsKnown());
		EXPECT_FALSE(unknown.CurrentLoadout.IsKnown());
		source.Aircraft->Airborne = decltype(source.Aircraft->Airborne)::Known(false);
		source.IsDestroyed = decltype(source.IsDestroyed)::Known(false);
		source.CurrentLoadout = decltype(source.CurrentLoadout)::Known({"empty"});
		source.Kinematics.Speed = decltype(source.Kinematics.Speed)::Known({0});
		auto known = Decode(Encode(source));
		ASSERT_TRUE(known.Aircraft->Airborne.IsKnown());
		EXPECT_FALSE(known.Aircraft->Airborne.Value());
		ASSERT_TRUE(known.IsDestroyed.IsKnown());
		EXPECT_FALSE(known.IsDestroyed.Value());
		ASSERT_TRUE(known.CurrentLoadout.IsKnown());
		EXPECT_EQ(known.CurrentLoadout.Value().Value, "empty");
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
		source.Type = decltype(source.Type)::Known<Classification::Api::SubCategory>(Classification::Api::SubCategory::Submarine);
		auto decoded = Decode(Encode(source));
		const auto& partial = std::get<Observations::Api::PartiallyLocalizedPosition>(decoded.Kinematics.Position);
		ASSERT_TRUE(partial.Latitude);
		EXPECT_EQ(partial.Latitude->Value, 0);
		EXPECT_FALSE(partial.Longitude);
		EXPECT_EQ(partial.ErrorEllipse->MajorAxis.Value, 100);
		EXPECT_EQ(*decoded.Type.GetIf<Classification::Api::SubCategory>(), Classification::Api::SubCategory::Submarine);
	}

	TEST(Codec, ClassificationResolutionsRoundTripWithoutPromotion)
	{
		Observations::Api::Instance source{};
		source.Kinematics.Position = Observations::Api::PartiallyLocalizedPosition{};

		source.Type = decltype(source.Type)::Known<Units::Api::TypeId>({"wp_ss_kilo"});
		auto typeId = Decode(Encode(source));
		ASSERT_NE(typeId.Type.GetIf<Units::Api::TypeId>(), nullptr);
		EXPECT_EQ(typeId.Type.GetIf<Classification::Api::SubCategory>(), nullptr);

		source.Type = decltype(source.Type)::Known<Classification::Api::SubCategory>(Classification::Api::SubCategory::Whale);
		auto subCategory = Decode(Encode(source));
		EXPECT_EQ(*subCategory.Type.GetIf<Classification::Api::SubCategory>(), Classification::Api::SubCategory::Whale);
		EXPECT_EQ(subCategory.Type.GetIf<Classification::Api::Category>(), nullptr);

		source.Type = decltype(source.Type)::Known<Classification::Api::Category>(Classification::Api::Category::SeaAnimal);
		auto category = Decode(Encode(source));
		EXPECT_EQ(*category.Type.GetIf<Classification::Api::Category>(), Classification::Api::Category::SeaAnimal);
		EXPECT_EQ(category.Type.GetIf<Classification::Api::Dimension>(), nullptr);

		source.Type = decltype(source.Type)::Known<Classification::Api::Dimension>(Classification::Api::Dimension::Other);
		auto dimension = Decode(Encode(source));
		EXPECT_EQ(*dimension.Type.GetIf<Classification::Api::Dimension>(), Classification::Api::Dimension::Other);

		source.Type = decltype(source.Type)::Unknown();
		EXPECT_TRUE(Decode(Encode(source)).Type.IsUnknown());
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
		Units::Api::Type fixedWing{};
		fixedWing.Id = {"aircraft/f-4"};
		fixedWing.UnitSubCategory = Classification::Api::SubCategory::FixedWingAircraft;
		const auto fixedWingWire = Encode(fixedWing);
		ASSERT_TRUE(fixedWingWire.has_sub_category());
		EXPECT_EQ(Decode(fixedWingWire).UnitSubCategory, Classification::Api::SubCategory::FixedWingAircraft);

		Units::Api::Instance source{};
		source.CurrentLoadout = {"empty"};
		source.IsDestroyed = true;
		source.Equipment.push_back({{"radar"}, Equipment::Api::OperationalState::Active, Equipment::Api::Damage::Intact, {}});
		source.Equipment.push_back(
			{{"launcher"}, Equipment::Api::OperationalState::Ready, Equipment::Api::Damage::Intact, Equipment::Api::Instance::ResourceInventory{{"missiles"}, std::uint64_t{0}}});
		auto decoded = Decode(Encode(source));
		ASSERT_EQ(decoded.Equipment.size(), 2u);
		EXPECT_FALSE(decoded.Equipment[0].Inventory);
		ASSERT_TRUE(decoded.Equipment[1].Inventory);
		EXPECT_EQ(decoded.Equipment[1].Inventory->ResourceId.Value, "missiles");
		EXPECT_EQ(std::get<std::uint64_t>(decoded.Equipment[1].Inventory->Quantity), 0u);
		EXPECT_EQ(decoded.CurrentLoadout.Value, "empty");
		EXPECT_TRUE(decoded.IsDestroyed);

		Units::Api::Type type{};
		type.Id = {"ship"};
		type.UnitSubCategory = Classification::Api::SubCategory::SurfaceVessel;
		type.PlatformRoles.push_back(Classification::Api::Role::SurfaceWarfare);
		Equipment::Api::Loadout loadout{};
		loadout.Id = {"default"};
		loadout.Equipment = {
			{{"radar"}, {}},
			{{"launcher"}, Equipment::Api::Fit::ResourceProvision{{"missiles"}, std::uint64_t{8}, std::uint64_t{0}}}};
		loadout.Roles.push_back(Classification::Api::Role::AntiSubmarineWarfare);
		type.Loadouts.push_back(std::move(loadout));
		auto decodedType = Decode(Encode(type));
		EXPECT_EQ(decodedType.UnitSubCategory, Classification::Api::SubCategory::SurfaceVessel);
		ASSERT_EQ(decodedType.PlatformRoles.size(), 1);
		EXPECT_EQ(decodedType.PlatformRoles[0], Classification::Api::Role::SurfaceWarfare);
		ASSERT_EQ(decodedType.Loadouts[0].Roles.size(), 1);
		EXPECT_EQ(decodedType.Loadouts[0].Roles[0], Classification::Api::Role::AntiSubmarineWarfare);
		EXPECT_FALSE(decodedType.Loadouts[0].Equipment[0].Resource);
		ASSERT_TRUE(decodedType.Loadouts[0].Equipment[1].Resource);
		EXPECT_EQ(std::get<std::uint64_t>(decodedType.Loadouts[0].Equipment[1].Resource->InitialInventory), 0u);

		Equipment::Api::Type equipment{};
		equipment.Id = {"radar"};
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
		EXPECT_THROW(Decode(Wire::UnitType{}), std::invalid_argument);
		Wire::UnitType type;
		type.set_sub_category(999);
		EXPECT_THROW(Decode(type), std::invalid_argument);

		Wire::UnitType invalidRole;
		invalidRole.set_sub_category(static_cast<int>(Classification::Api::SubCategory::Other));
		invalidRole.add_platform_roles(999);
		EXPECT_THROW(Decode(invalidRole), std::invalid_argument);

		Wire::Observation invalidCategory;
		invalidCategory.set_category(999);
		EXPECT_THROW(Decode(invalidCategory), std::invalid_argument);
	}
}
