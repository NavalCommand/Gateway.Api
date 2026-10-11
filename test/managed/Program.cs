using Google.Protobuf;
using NavalCommand.Gateway.Wire;
using System;
using System.IO;
using System.Linq;

if (args[0] == "write")
{
    var request = new OrderRequest
    {
        SessionId = "session-µ",
        RequestId = "conformance",
        UnitId = new UnitId { Value = "opaque-α" },
        BasedOnVersion = 9007199254740993UL,
        Waypoint = new Waypoint { LatitudeDeg = 61.96, LongitudeDeg = 0, RouteMode = RouteMode.AppendWaypoint }
    };
    using var output = File.Create(args[1]);
    request.WriteTo(output);
    using var unknownFieldWriter = new CodedOutputStream(output, leaveOpen: true);
    unknownFieldWriter.WriteTag(123, WireFormat.WireType.Varint);
    unknownFieldWriter.WriteUInt32(9876);
    unknownFieldWriter.Flush();

    var snapshot = new WorldSnapshot
    {
        Session = new Session { SchemaVersion = 3, SessionId = "composed", Ready = true },
        ObservationVersion = 7,
        OperationalSeconds = 100
    };
    var own = new UnitInstance
    {
        UnitId = new UnitId { Value = "own" },
        Display = new IdentityState
        {
            Name = new StringKnowledge { Known = "Aircraft" },
            TypeId = new UnitTypeIdKnowledge { Known = new UnitTypeId { Value = "aircraft" } }
        },
        Kinematics = new KinematicsState
        {
            Position = new PositionKnowledge { Known = new PositionObservation { Coordinates = new GeoPoint { LatitudeDeg = 0, LongitudeDeg = 0, AltitudeM = 0 } } },
            SpeedMps = new DoubleKnowledge { Known = 0 },
            HeadingRad = new DoubleKnowledge { Known = 0 },
            CourseOverGroundRad = new DoubleKnowledge { Estimated = new DoubleKnowledge.Types.Estimate { Value = 1, Source = "motion" } }
        },
        Observation = new ObservationMetadata { MeasuredAtSeconds = new DoubleKnowledge { Known = 42 } },
        Aircraft = new AircraftState { Airborne = new BoolKnowledge { Known = false } },
        Navigation = new NavigationState { PlannedRoute = new RouteKnowledge { Known = new Route() } },
        Equipment = new ObservedEquipment { Coverage = CollectionCoverage.CoveragePartial },
        AirOperations = new AirOperations
        {
            Inventory = new AircraftInventory { Coverage = CollectionCoverage.CoverageComplete },
            OnboardCount = new UInt32Knowledge { Known = 0 },
            MeasuredAtSeconds = new DoubleKnowledge { Known = 41 }
        }
    };
    var inventory = new AircraftInventoryEntry
    {
        UnitTypeId = new UnitTypeId { Value = "aircraft" },
        Squadron = "squadron",
        UnreservedCount = new UInt32Knowledge { Known = 0 }
    };
    inventory.Loadouts.Add(new LoadoutId { Value = "loadout" });
    own.AirOperations.Inventory.Items.Add(inventory);
    own.AirOperations.Inventory.Items.Add(new AircraftInventoryEntry
    {
        UnitTypeId = new UnitTypeId { Value = "other" },
        Squadron = "unknown-stock",
        UnreservedCount = new UInt32Knowledge()
    });
    var equipment = new EquipmentInstance
    {
        Active = new BoolKnowledge { Estimated = new BoolKnowledge.Types.Estimate { Value = false, Source = "observer" } },
        TypeId = new EquipmentTypeIdKnowledge { Known = new EquipmentTypeId { Value = "missile" } },
        Resources = new ObservedResources { Coverage = CollectionCoverage.CoveragePartial }
    };
    equipment.Resources.Items.Add(new ResourceObservation
    {
        ResourceId = new ResourceId { Value = "missiles" },
        Amount = new ResourceKnowledge { Known = new ResourceValue { Discrete = 0 } }
    });
    equipment.Resources.Items.Add(new ResourceObservation
    {
        ResourceId = new ResourceId { Value = "fuel" },
        Amount = new ResourceKnowledge { Estimated = new ResourceKnowledge.Types.Estimate { Value = new ResourceValue { Continuous = 12.5 }, Source = "gauge" } }
    });
    equipment.Resources.Items.Add(new ResourceObservation { ResourceId = new ResourceId { Value = "channels" }, Amount = new ResourceKnowledge() });
    equipment.Resources.Items.Add(new ResourceObservation
    {
        ResourceId = new ResourceId { Value = "empty-tank" },
        Amount = new ResourceKnowledge { Known = new ResourceValue { Continuous = 0 } }
    });
    equipment.Resources.Items.Add(new ResourceObservation
    {
        ResourceId = new ResourceId { Value = "large-discrete" },
        Amount = new ResourceKnowledge { Known = new ResourceValue { Discrete = 9007199254740993UL } }
    });
    own.Equipment.Items.Add(equipment);
    own.Equipment.Items.Add(new EquipmentInstance { Resources = new ObservedResources { Coverage = CollectionCoverage.CoverageComplete } });
    own.Equipment.Items.Add(new EquipmentInstance { Resources = new ObservedResources { Coverage = CollectionCoverage.CoverageUnknown } });
    snapshot.Instances.Add(own);
    snapshot.Instances.Add(new UnitInstance
    {
        UnitId = new UnitId { Value = "contact" },
        Kinematics = new KinematicsState
        {
            Position = new PositionKnowledge { Estimated = new PositionKnowledge.Types.Estimate
            {
                Value = new PositionObservation { Coordinates = new GeoPoint { LatitudeDeg = 1, LongitudeDeg = 2 } }, Source = "contact-provenance"
            } }
        },
        Navigation = new NavigationState { PlannedRoute = new RouteKnowledge() },
        Equipment = new ObservedEquipment { Coverage = CollectionCoverage.CoverageUnknown }
    });
    snapshot.Instances.Add(new UnitInstance { UnitId = new UnitId { Value = "empty" }, Equipment = new ObservedEquipment { Coverage = CollectionCoverage.CoverageComplete } });
    snapshot.Memberships = new ObservedMemberships { Coverage = CollectionCoverage.CoveragePartial };
    snapshot.Memberships.Items.Add(new UnitMembership
    {
        UnitId = new UnitId { Value = "own" },
        TaskForce = new StringKnowledge { Known = "group" }
    });
    var observation = new ProviderObservation { Snapshot = snapshot };
    var discovery = new OrderDiscovery
    {
        SessionId = "composed",
        UnitId = new UnitId { Value = "own" },
        BasedOnVersion = 7,
        Eligibility = new OrderEligibilityState { Coverage = CollectionCoverage.CoverageComplete }
    };
    discovery.Eligibility.Items.Add(new OrderEligibility
    {
        Kind = OrderKind.LaunchAircraft,
        Availability = Availability.Blocked,
        Reason = "resources_unavailable",
        LaunchOption = new AircraftLaunch
        {
            UnitTypeId = new UnitTypeId { Value = "aircraft" }, Squadron = "squadron", LoadoutId = new LoadoutId { Value = "loadout" }, Count = 1
        }
    });
    observation.OrderAvailability.Add(discovery);
    File.WriteAllBytes(args[1] + ".world", observation.ToByteArray());
    Console.WriteLine("PASS C# serialize canonical payload, UTF-8, uint64 and direct oneof with unknown field");
}
else
{
    var serializedRequest = File.ReadAllBytes(args[1]);
    var request = OrderRequest.Parser.ParseFrom(serializedRequest);
    if (request.SessionId != "session-µ" || request.UnitId.Value != "opaque-α" || request.BasedOnVersion != 9007199254740993UL
        || request.PayloadCase != OrderRequest.PayloadOneofCase.Waypoint || request.Waypoint.LongitudeDeg != -10.37
        || request.Waypoint.RouteMode != RouteMode.AppendWaypoint)
    {
        throw new Exception("conformance");
    }
    using var roundtripStream = new MemoryStream();
    request.WriteTo(roundtripStream);
    using (var unknownFieldWriter = new CodedOutputStream(roundtripStream, leaveOpen: true))
    {
        unknownFieldWriter.WriteTag(123, WireFormat.WireType.Varint);
        unknownFieldWriter.WriteUInt32(9876);
        unknownFieldWriter.Flush();
    }
    if (request.CalculateSize() == roundtripStream.Length || !request.ToByteArray().SequenceEqual(serializedRequest))
    {
        throw new Exception("unknown field preservation");
    }
    var optional = new OrderResult { DisplacementM = 0 };
    if (!OrderResult.Parser.ParseFrom(optional.ToByteArray()).HasDisplacementM)
    {
        throw new Exception("optional zero");
    }
    var enumValue = new OrderResult { Stage = (Stage)99 };
    if ((int)OrderResult.Parser.ParseFrom(enumValue.ToByteArray()).Stage != 99)
    {
        throw new Exception("enum preservation");
    }
    var observation = ProviderObservation.Parser.ParseFrom(File.ReadAllBytes(args[1] + ".world"));
    var world = observation.Snapshot;
    var own = world.Instances[0];
    var contact = world.Instances[1];
    var resources = own.Equipment.Items[0].Resources;
    if (world.Session.SchemaVersion != 3 || world.Instances.Count != 3 || own.UnitId.Value != "own" || contact.UnitId.Value != "contact"
        || own.Aircraft.Airborne.StateCase != BoolKnowledge.StateOneofCase.Known || own.Aircraft.Airborne.Known
        || own.Kinematics.SpeedMps.StateCase != DoubleKnowledge.StateOneofCase.Known || own.Kinematics.SpeedMps.Known != 0
        || own.Kinematics.HeadingRad.StateCase != DoubleKnowledge.StateOneofCase.Known || own.Kinematics.HeadingRad.Known != 0
        || own.Kinematics.CourseOverGroundRad.Estimated.Value != 1 || own.Observation.MeasuredAtSeconds.Known != 42
        || own.Equipment.Coverage != CollectionCoverage.CoveragePartial || resources.Coverage != CollectionCoverage.CoveragePartial
        || resources.Items[0].Amount.Known.QuantityCase != ResourceValue.QuantityOneofCase.Discrete || resources.Items[0].Amount.Known.Discrete != 0
        || resources.Items[1].Amount.Estimated.Value.Continuous != 12.5 || resources.Items[1].Amount.Estimated.Source != "gauge"
        || resources.Items[2].Amount.StateCase != ResourceKnowledge.StateOneofCase.None
        || resources.Items[3].Amount.Known.QuantityCase != ResourceValue.QuantityOneofCase.Continuous || resources.Items[3].Amount.Known.Continuous != 0
        || resources.Items[4].Amount.Known.QuantityCase != ResourceValue.QuantityOneofCase.Discrete || resources.Items[4].Amount.Known.Discrete != 9007199254740993UL
        || own.Equipment.Items[1].Resources.Coverage != CollectionCoverage.CoverageComplete || own.Equipment.Items[1].Resources.Items.Count != 0
        || own.Equipment.Items[2].Resources.Coverage != CollectionCoverage.CoverageUnknown
        || own.Equipment.Items[0].Active.StateCase != BoolKnowledge.StateOneofCase.Estimated || own.Equipment.Items[0].Active.Estimated.Value
        || own.Equipment.Items[0].Active.Estimated.Source != "observer"
        || own.Navigation.PlannedRoute.StateCase != RouteKnowledge.StateOneofCase.Known || own.Navigation.PlannedRoute.Known.Points.Count != 0
        || contact.Navigation.PlannedRoute.StateCase != RouteKnowledge.StateOneofCase.None
        || contact.Kinematics.Position.StateCase != PositionKnowledge.StateOneofCase.Estimated
        || contact.Kinematics.Position.Estimated.Source != "contact-provenance" || contact.Observation.MeasuredAtSeconds.StateCase != DoubleKnowledge.StateOneofCase.None
        || contact.Equipment.Coverage != CollectionCoverage.CoverageUnknown || world.Instances[2].Equipment.Coverage != CollectionCoverage.CoverageComplete
        || own.AirOperations.Inventory.Items[0].UnreservedCount.StateCase != UInt32Knowledge.StateOneofCase.Known || own.AirOperations.Inventory.Items[0].UnreservedCount.Known != 0
        || own.AirOperations.Inventory.Items[1].UnreservedCount.StateCase != UInt32Knowledge.StateOneofCase.None
        || own.AirOperations.OnboardCount.StateCase != UInt32Knowledge.StateOneofCase.Known || own.AirOperations.OnboardCount.Known != 0
        || own.AirOperations.ReservedCount.StateCase != UInt32Knowledge.StateOneofCase.None
        || world.Memberships.Coverage != CollectionCoverage.CoveragePartial || world.Memberships.Items[0].UnitId.Value != "own"
        || world.Memberships.Items[0].TaskForce.Known != "group" || world.Memberships.Items[0].Faction.StateCase != StringKnowledge.StateOneofCase.None
        || observation.OrderAvailability.Count != 1 || observation.OrderAvailability[0].UnitId.Value != own.UnitId.Value
        || observation.OrderAvailability[0].Eligibility.Items[0].Kind != OrderKind.LaunchAircraft
        || observation.OrderAvailability[0].Eligibility.Items[0].LaunchOption.LoadoutId.Value != "loadout"
        || observation.OrderAvailability[0].Eligibility.Items[0].Reason != "resources_unavailable")
    {
        throw new Exception("composed cross-language conformance");
    }
    Console.WriteLine("PASS C# -> C++ domain -> C# IDs, overlays, measurements, resource variants, inventory, zero/false and coverage");
    Console.WriteLine("PASS direct C++ -> C# unknown fields, optional zero and unknown lifecycle enum");
}
