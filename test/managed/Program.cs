using Google.Protobuf;
using NavalCommand.Gateway.Wire;
using System;
using System.IO;

if (args.Length != 2) { return 2; }
if (args[0] == "write")
{
    var publication = new Publication { Session = "managed-fixture", Revision = 7, CapturedAtSeconds = 42 };
    var scope = new ScopedPublication { Scope = new Scope { Faction = "blue" } };
    var observation = new Observation
    {
        Id = "contact", ObserverFaction = "blue", Classification = 2,
        Aircraft = new Aircraft { Airborne = false },
        Kinematics = new ObservationKinematics
        {
            SpeedMetersPerSecond = 0,
            Position = new ObservedPosition
            {
                Partial = new PartialPosition
                {
                    LatitudeDegrees = 0,
                    ErrorEllipse = new ErrorEllipse { MajorAxisMeters = 1200, MinorAxisMeters = 300, OrientationRadians = 0.5 }
                }
            }
        }
    };
    scope.Entries.Add(new Entry { Query = new Query { Method = Query.Types.Method.Observation, Id = "contact" }, Result = new Result { Observation = observation } });
    scope.Entries.Add(new Entry { Query = new Query { Method = Query.Types.Method.Operations, Id = "own" }, Result = new Result
    {
        Operations = new Operations
        {
            Idle = new Empty(), Navigation = new Navigation(),
            AirOperations = new AirOperations { Inventory = new AirOperations.Types.Inventory(), OnboardCount = 0 }
        }
    } });
    scope.Entries.Add(new Entry { Query = new Query { Method = Query.Types.Method.EquipmentType, Id = "missing" }, Result = new Result
    {
        Failure = new Failure { Infrastructure = new InfrastructureError { Condition = 5, CauseCategory = "generic", CauseValue = 13 } }
    } });
    publication.Views.Add(scope);
    var unit = new Unit { Id = "own", Faction = "blue" };
    unit.Equipment.Add(new EquipmentInstance { Type = "radar", OperationalState = 1 });
    unit.Equipment.Add(new EquipmentInstance
    {
        Type = "launcher", Inventory = new EquipmentInstance.Types.ResourceInventory
        {
            ResourceId = "missiles", Quantity = new ResourceValue { Discrete = 0 }
        }
    });
    scope.Entries.Add(new Entry { Query = new Query { Method = Query.Types.Method.Unit, Id = "own" }, Result = new Result { Unit = unit } });
    File.WriteAllBytes(args[1], publication.ToByteArray());
    return 0;
}
if (args[0] == "read")
{
    var publication = Publication.Parser.ParseFrom(File.ReadAllBytes(args[1]));
    var observation = publication.Views[0].Entries[0].Result.Observation;
    var position = observation.Kinematics.Position.Partial;
    var operations = publication.Views[0].Entries[1].Result.Operations;
    var failure = publication.Views[0].Entries[2].Result.Failure.Infrastructure;
    var equipment = publication.Views[0].Entries[3].Result.Unit.Equipment;
    if (publication.Session != "managed-fixture" || publication.Revision != 7 || publication.Views[0].Scope.Faction != "blue"
        || observation.Id != "contact" || observation.HasObservedFaction || observation.HasType || !observation.HasClassification
        || !observation.Aircraft.HasAirborne || observation.Aircraft.Airborne
        || !observation.Kinematics.HasSpeedMetersPerSecond || observation.Kinematics.SpeedMetersPerSecond != 0
        || observation.Kinematics.HasHeadingRadians || !position.HasLatitudeDegrees || position.HasLongitudeDegrees
        || position.ErrorEllipse.MajorAxisMeters != 1200 || operations.Navigation == null || operations.AirOperations.Inventory == null
        || !operations.AirOperations.HasOnboardCount || operations.AirOperations.OnboardCount != 0
        || operations.AirOperations.HasReservedCount || failure.Condition != 5 || failure.CauseCategory != "generic" || failure.CauseValue != 13
        || equipment[0].Inventory != null || equipment[1].Inventory == null || equipment[1].Inventory.ResourceId != "missiles"
        || equipment[1].Inventory.Quantity.ValueCase != ResourceValue.ValueOneofCase.Discrete || equipment[1].Inventory.Quantity.Discrete != 0)
    {
        throw new InvalidOperationException("cross_language_presence_or_value_mismatch");
    }
    Console.WriteLine("C# -> C++ domain codec -> C# conformance passed.");
    return 0;
}
return 2;
