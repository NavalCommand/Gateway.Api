using Google.Protobuf;
using NavalCommand.Gateway.Wire;
using System;
using System.IO;

static Observation CreateObservation(string id)
{
    return new Observation
    {
        Id = id,
        ObserverFaction = "blue",
        Kinematics = new ObservationKinematics
        {
            SpeedMetersPerSecond = 0,
            Position = new ObservedPosition
            {
                Partial = new PartialPosition
                {
                    LatitudeDegrees = 0,
                    ErrorEllipse = new ErrorEllipse
                    {
                        MajorAxisMeters = 1200,
                        MinorAxisMeters = 300,
                        OrientationRadians = 0.5
                    }
                }
            }
        }
    };
}

if (args.Length != 2)
{
    return 2;
}

if (args[0] == "write")
{
    var publication = new Publication
    {
        Session = "managed-fixture",
        Revision = 7,
        CapturedAtSeconds = 42
    };
    var scope = new ScopedPublication { Scope = new Scope { Faction = "blue" } };

    var exactType = CreateObservation("contact-type");
    exactType.Type = "frigate";
    exactType.Aircraft = new Aircraft { Airborne = false };
    exactType.CurrentLoadout = "empty";
    exactType.IsDestroyed = false;

    var subCategory = CreateObservation("contact-subcategory");
    subCategory.SubCategory = 35;

    var category = CreateObservation("contact-category");
    category.Category = 12;

    var dimension = CreateObservation("contact-dimension");
    dimension.Dimension = 5;

    var unknown = CreateObservation("contact-unknown");

    foreach (var observation in new[] { exactType, subCategory, category, dimension, unknown })
    {
        scope.Entries.Add(new Entry
        {
            Query = new Query { Method = Query.Types.Method.Observation, Id = observation.Id },
            Result = new Result { Observation = observation }
        });
    }

    scope.Entries.Add(new Entry
    {
        Query = new Query { Method = Query.Types.Method.Operations, Id = "own" },
        Result = new Result
        {
            Operations = new Operations
            {
                Idle = new Empty(),
                Navigation = new Navigation(),
                AirOperations = new AirOperations
                {
                    Inventory = new AirOperations.Types.Inventory(),
                    OnboardCount = 0
                }
            }
        }
    });

    scope.Entries.Add(new Entry
    {
        Query = new Query { Method = Query.Types.Method.EquipmentType, Id = "missing" },
        Result = new Result
        {
            Failure = new Failure
            {
                Infrastructure = new InfrastructureError
                {
                    Condition = 5,
                    CauseCategory = "generic",
                    CauseValue = 13
                }
            }
        }
    });

    var unit = new Unit
    {
        Id = "own",
        Faction = "blue",
        CurrentLoadout = "empty",
        IsDestroyed = false
    };
    unit.Equipment.Add(new EquipmentInstance { Type = "radar", OperationalState = 1 });
    unit.Equipment.Add(new EquipmentInstance
    {
        Type = "launcher",
        Inventory = new EquipmentInstance.Types.ResourceInventory
        {
            ResourceId = "missiles",
            Quantity = new ResourceValue { Discrete = 0 }
        }
    });
    scope.Entries.Add(new Entry
    {
        Query = new Query { Method = Query.Types.Method.Unit, Id = "own" },
        Result = new Result { Unit = unit }
    });

    var unitType = new UnitType { Id = "ship", SubCategory = 32 };
    unitType.PlatformRoles.Add(4);
    var loadout = new Loadout { Id = "standard" };
    loadout.Roles.Add(5);
    unitType.Loadouts.Add(loadout);
    scope.Entries.Add(new Entry
    {
        Query = new Query { Method = Query.Types.Method.UnitType, Id = "ship" },
        Result = new Result { UnitType = unitType }
    });

    publication.Views.Add(scope);
    File.WriteAllBytes(args[1], publication.ToByteArray());
    return 0;
}

if (args[0] == "read")
{
    var publication = Publication.Parser.ParseFrom(File.ReadAllBytes(args[1]));
    var entries = publication.Views[0].Entries;
    var observations = new[]
    {
        entries[0].Result.Observation,
        entries[1].Result.Observation,
        entries[2].Result.Observation,
        entries[3].Result.Observation,
        entries[4].Result.Observation
    };
    var operations = entries[5].Result.Operations;
    var failure = entries[6].Result.Failure.Infrastructure;
    var unit = entries[7].Result.Unit;
    var unitType = entries[8].Result.UnitType;
    var position = observations[0].Kinematics.Position.Partial;

    if (publication.Session != "managed-fixture" || publication.Revision != 7 || publication.Views[0].Scope.Faction != "blue"
        || observations[0].Id != "contact-type" || observations[0].TypeResolutionCase.ToString() != "Type"
        || observations[1].TypeResolutionCase.ToString() != "SubCategory" || observations[1].SubCategory != 35
        || observations[2].TypeResolutionCase.ToString() != "Category" || observations[2].Category != 12
        || observations[3].TypeResolutionCase.ToString() != "Dimension" || observations[3].Dimension != 5
        || observations[4].TypeResolutionCase.ToString() != "None"
        || !observations[0].HasCurrentLoadout || observations[0].CurrentLoadout != "empty"
        || !observations[0].HasIsDestroyed || observations[0].IsDestroyed
        || observations[1].HasCurrentLoadout || observations[1].HasIsDestroyed
        || !observations[0].Aircraft.HasAirborne || observations[0].Aircraft.Airborne
        || !observations[0].Kinematics.HasSpeedMetersPerSecond || observations[0].Kinematics.SpeedMetersPerSecond != 0
        || observations[0].Kinematics.HasHeadingRadians || !position.HasLatitudeDegrees || position.HasLongitudeDegrees
        || position.ErrorEllipse.MajorAxisMeters != 1200 || operations.Navigation == null || operations.AirOperations.Inventory == null
        || !operations.AirOperations.HasOnboardCount || operations.AirOperations.OnboardCount != 0
        || operations.AirOperations.HasReservedCount || failure.Condition != 5 || failure.CauseCategory != "generic" || failure.CauseValue != 13
        || unit.CurrentLoadout != "empty" || unit.IsDestroyed || unit.Equipment[0].Inventory != null
        || unit.Equipment[1].Inventory == null || unit.Equipment[1].Inventory.ResourceId != "missiles"
        || unit.Equipment[1].Inventory.Quantity.ValueCase != ResourceValue.ValueOneofCase.Discrete
        || unit.Equipment[1].Inventory.Quantity.Discrete != 0
        || !unitType.HasSubCategory || unitType.SubCategory != 32 || unitType.PlatformRoles.Count != 1 || unitType.PlatformRoles[0] != 4
        || unitType.Loadouts.Count != 1 || unitType.Loadouts[0].Roles.Count != 1 || unitType.Loadouts[0].Roles[0] != 5)
    {
        throw new InvalidOperationException("cross_language_presence_or_value_mismatch");
    }

    Console.WriteLine("C# -> C++ domain codec -> C# conformance passed.");
    return 0;
}

return 2;
