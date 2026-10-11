using Google.Protobuf;
using NavalCommand.Gateway.Wire;
using System;
using System.IO;
using System.Linq;

if (args[0] == "write")
{
    var request = new OrderRequest
    {
        SessionId = "session-Вµ",
        RequestId = "conformance",
        Subject = new Subject { OwnUnitId = "opaque-О±" },
        BasedOnVersion = 9007199254740993UL,
        Waypoint = new Waypoint
        {
            LatitudeDeg = 61.96,
            LongitudeDeg = 0,
            RouteMode = RouteMode.AppendWaypoint
        }
    };
    using var output = File.Create(args[1]);
    request.WriteTo(output);
    using var unknownFieldWriter = new CodedOutputStream(output, leaveOpen: true);
    unknownFieldWriter.WriteTag(123, WireFormat.WireType.Varint);
    unknownFieldWriter.WriteUInt32(9876);
    unknownFieldWriter.Flush();
    var world = new WorldSnapshot { Session = new Session { SchemaVersion = 2, SessionId = "composed", Ready = true } };
    var own = new UnitInstance
    {
        Subject = new Subject { OwnUnitId = "same-id" },
        Display = new IdentityState { Name = new StringKnowledge { Known = "Aircraft" } },
        Kinematics = new KinematicsState
        {
            Position = new PositionKnowledge { Known = new PositionObservation { Coordinates = new GeoPoint { LatitudeDeg = 0, LongitudeDeg = 0, AltitudeM = 0 } } },
            SpeedMps = new DoubleKnowledge { Known = 0 }
        },
        Aircraft = new AircraftState { Airborne = new BoolKnowledge { Known = false } },
        Navigation = new NavigationState { Inspected = new BoolKnowledge { Known = true }, PlannedRoute = new RouteKnowledge { Known = new Route() } },
        Equipment = new ObservedEquipment { Coverage = CollectionCoverage.CoveragePartial }
    };
    var equipment = new EquipmentInstance
    {
        Active = new BoolKnowledge { Estimated = new BoolKnowledge.Types.Estimate { Value = false, Source = "observer" } },
        TypeId = new StringKnowledge { Known = "missile" },
        Physical = new PhysicalTraits { OrientationRad = 0 }
    };
    equipment.Resources.Add(new EquipmentInstance.Types.Resource { Kind = ResourceKind.Ammunition, Amount = new DoubleKnowledge { Known = 0 } });
    own.Equipment.Items.Add(equipment);
    world.Instances.Add(own);
    world.Instances.Add(new UnitInstance
    {
        Subject = new Subject { TrackId = "same-id" },
        Kinematics = new KinematicsState
        {
            Position = new PositionKnowledge { Estimated = new PositionKnowledge.Types.Estimate
            {
                Value = new PositionObservation { Coordinates = new GeoPoint { LatitudeDeg = 1, LongitudeDeg = 2 } }, Source = "contact-source"
            } }
        },
        Equipment = new ObservedEquipment { Coverage = CollectionCoverage.CoverageUnknown }
    });
    world.Instances.Add(new UnitInstance { Subject = new Subject { OwnUnitId = "empty" }, Equipment = new ObservedEquipment { Coverage = CollectionCoverage.CoverageComplete } });
    File.WriteAllBytes(args[1] + ".world", world.ToByteArray());
    Console.WriteLine("PASS C# serialize canonical payload, UTF-8, uint64 and oneof with unknown field");
}
else
{
    var serializedRequest = File.ReadAllBytes(args[1]);
    var request = OrderRequest.Parser.ParseFrom(serializedRequest);
    if (request.SessionId != "session-Вµ" || request.Subject.OwnUnitId != "opaque-О±" || request.BasedOnVersion != 9007199254740993UL
        || request.ArgumentsCase != OrderRequest.ArgumentsOneofCase.Waypoint || request.Waypoint.LongitudeDeg != -10.37
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

    if (request.CalculateSize() == roundtripStream.Length)
    {
        throw new Exception("unknown field missing");
    }

    var reserializedRequest = request.ToByteArray();
    if (!reserializedRequest.SequenceEqual(serializedRequest))
    {
        throw new Exception("unknown field preservation");
    }

    var optional = new OrderResult
    {
        DisplacementM = 0
    };
    if (!OrderResult.Parser.ParseFrom(optional.ToByteArray()).HasDisplacementM)
    {
        throw new Exception("optional zero");
    }

    var enumValue = new OrderResult
    {
        Stage = (Stage)99
    };
    if ((int)OrderResult.Parser.ParseFrom(enumValue.ToByteArray()).Stage != 99)
    {
        throw new Exception("enum preservation");
    }

    var world = WorldSnapshot.Parser.ParseFrom(File.ReadAllBytes(args[1] + ".world"));
    var own = world.Instances[0];
    var contact = world.Instances[1];
    if (world.Instances.Count != 3 || own.Subject.IdentityCase != Subject.IdentityOneofCase.OwnUnitId
        || contact.Subject.IdentityCase != Subject.IdentityOneofCase.TrackId
        || own.Aircraft.Airborne.StateCase != BoolKnowledge.StateOneofCase.Known || own.Aircraft.Airborne.Known
        || own.Kinematics.SpeedMps.StateCase != DoubleKnowledge.StateOneofCase.Known || own.Kinematics.SpeedMps.Known != 0
        || own.Equipment.Coverage != CollectionCoverage.CoveragePartial || own.Equipment.Items[0].Resources[0].Amount.Known != 0
        || own.Equipment.Items[0].Active.StateCase != BoolKnowledge.StateOneofCase.Estimated || own.Equipment.Items[0].Active.Estimated.Value
        || own.Equipment.Items[0].Active.Estimated.Source != "observer" || !own.Equipment.Items[0].Physical.HasOrientationRad
        || own.Navigation.PlannedRoute.StateCase != RouteKnowledge.StateOneofCase.Known || own.Navigation.PlannedRoute.Known.Points.Count != 0
        || contact.Kinematics.Position.StateCase != PositionKnowledge.StateOneofCase.Estimated
        || contact.Kinematics.Position.Estimated.Source != "contact-source" || contact.OrderEligibility != null
        || contact.Equipment.Coverage != CollectionCoverage.CoverageUnknown || world.Instances[2].Equipment.Coverage != CollectionCoverage.CoverageComplete)
    {
        throw new Exception("composed world cross-language conformance");
    }
    Console.WriteLine("PASS C# -> C++ domain -> C# composed world, knowledge, coverage, zero and false");
    Console.WriteLine("PASS C++ -> C#; unknown fields, optional zero and unknown enum preserved");
}
