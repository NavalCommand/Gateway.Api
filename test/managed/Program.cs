using Google.Protobuf;
using NavalCommand.Gateway.Wire;
using System;
using System.IO;

if (args.Length != 2)
{
    return 2;
}

if (args[0] == "write")
{
    var request = new OrderRequest
    {
        SessionId = "session",
        RequestId = "conformance",
        Observer = new ObserverId { Value = "red" },
        Observation = new ObservationId { Value = "own-contact" },
        BasedOnVersion = 7,
        Waypoint = new Waypoint
        {
            LatitudeDeg = 61.96,
            LongitudeDeg = 0,
            RouteMode = RouteMode.AppendWaypoint
        }
    };

    using (var output = File.Create(args[1]))
    {
        request.WriteTo(output);
        var unknown = new CodedOutputStream(output, leaveOpen: true);
        unknown.WriteTag(123, WireFormat.WireType.Varint);
        unknown.WriteUInt32(9876);
        unknown.Flush();
    }

    var own = new UnitObservation
    {
        Identity = new ObservationId { Value = "own-contact" },
        Name = new StringKnowledge { Known = "Helo-1" },
        Type = new UnitTypeIdKnowledge
        {
            Estimated = new UnitTypeIdKnowledge.Types.Estimate
            {
                Value = new UnitTypeId { Value = "aircraft" },
                Source = "classification"
            }
        },
        Kinematics = new KinematicsObservation
        {
            Position = new PositionKnowledge
            {
                Known = new PositionObservation
                {
                    Coordinates = new GeoPoint
                    {
                        LatitudeDeg = 0,
                        LongitudeDeg = 0,
                        AltitudeM = 0
                    }
                }
            },
            SpeedMps = new DoubleKnowledge { Known = 0 },
            HeadingRad = new DoubleKnowledge { Known = 0 },
            CourseOverGroundRad = new DoubleKnowledge
            {
                Estimated = new DoubleKnowledge.Types.Estimate
                {
                    Value = 1,
                    Source = "motion"
                }
            }
        },
        Equipment = new ObservedEquipment { Coverage = CollectionCoverage.CoveragePartial },
        Aircraft = new AircraftObservation { Airborne = new BoolKnowledge { Known = false } },
        Navigation = new NavigationObservation
        {
            PlannedRoute = new RouteKnowledge { Known = new Route() }
        },
        AirOperations = new AirOperationsObservation
        {
            Inventory = new AircraftInventory { Coverage = CollectionCoverage.CoverageComplete }
        },
        Evidence = new ObservationMetadata
        {
            MeasuredAtSeconds = new DoubleKnowledge { Known = 42 }
        }
    };

    var equipment = new EquipmentObservation
    {
        TypeId = new EquipmentTypeIdKnowledge { Known = new EquipmentTypeId { Value = "stores" } },
        Active = new BoolKnowledge
        {
            Estimated = new BoolKnowledge.Types.Estimate
            {
                Value = false,
                Source = "sensor"
            }
        },
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
        Amount = new ResourceKnowledge
        {
            Estimated = new ResourceKnowledge.Types.Estimate
            {
                Value = new ResourceValue { Continuous = 12.5 },
                Source = "gauge"
            }
        }
    });
    equipment.Resources.Items.Add(new ResourceObservation
    {
        ResourceId = new ResourceId { Value = "channels" }
    });
    own.Equipment.Items.Add(equipment);

    own.AirOperations.Inventory.Items.Add(new AircraftInventoryEntry
    {
        Type = new UnitTypeIdKnowledge { Known = new UnitTypeId { Value = "fighter" } },
        Squadron = new StringKnowledge { Known = "VFA-1" },
        UnreservedCount = new UInt32Knowledge { Known = 2 },
        Loadouts = new ObservedLoadouts { Coverage = CollectionCoverage.CoverageComplete }
    });
    own.AirOperations.Inventory.Items[0].Loadouts.Items.Add(new LoadoutId { Value = "loadout" });

    var fused = new UnitObservation
    {
        Identity = new ObservationId { Value = "fused-contact" },
        Equipment = new ObservedEquipment { Coverage = CollectionCoverage.CoverageUnknown }
    };
    var snapshot = new ObserverSnapshot
    {
        SchemaVersion = 4,
        SessionId = "session",
        Observer = new ObserverId { Value = "red" },
        Ready = true,
        Version = 7,
        CapturedAtSeconds = 100,
        Memberships = new ObservedRelationships { Coverage = CollectionCoverage.CoverageComplete }
    };
    snapshot.Units.Add(own);
    snapshot.Units.Add(fused);
    snapshot.Memberships.Items.Add(new ObserverUnitRelationship
    {
        Observation = own.Identity,
        Faction = new FactionIdKnowledge { Known = new FactionId { Value = "friendly" } },
        TaskForce = new TaskForceIdKnowledge
        {
            Estimated = new TaskForceIdKnowledge.Types.Estimate
            {
                Value = new TaskForceId { Value = "group-1" },
                Source = "fusion"
            }
        }
    });

    var discovery = new OrderDiscovery
    {
        SessionId = "session",
        Observer = snapshot.Observer,
        Observation = own.Identity,
        BasedOnVersion = snapshot.Version,
        Eligibility = new OrderEligibilityState { Coverage = CollectionCoverage.CoverageComplete }
    };
    discovery.Eligibility.Items.Add(new OrderEligibility
    {
        Kind = OrderKind.ReplaceRoute,
        Availability = Availability.Executable
    });
    var envelope = new ProviderObservation { Snapshot = snapshot };
    envelope.OrderAvailability.Add(discovery);
    File.WriteAllBytes(args[1] + ".world", envelope.ToByteArray());
    Console.WriteLine("PASS C# serializes observer-scoped snapshot, typed relationships and order overlay");
}
else if (args[0] == "read")
{
    using var input = File.OpenRead(args[1]);
    var request = OrderRequest.Parser.ParseFrom(input);
    if (request.PayloadCase != OrderRequest.PayloadOneofCase.Waypoint || request.Waypoint.LongitudeDeg != -10.37 ||
        request.Observation.Value != "own-contact" || request.Observer.Value != "red")
    {
        return 3;
    }

    var envelope = ProviderObservation.Parser.ParseFrom(File.ReadAllBytes(args[1] + ".world"));
    var own = envelope.Snapshot.Units[0];
    var fused = envelope.Snapshot.Units[1];
    if (envelope.Snapshot.SchemaVersion != 4 || envelope.Snapshot.Units.Count != 2 ||
        own.Name.Known != "Helo-1" || fused.Name.StateCase != StringKnowledge.StateOneofCase.None ||
        own.Kinematics.SpeedMps.Known != 0 || own.Aircraft.Airborne.Known ||
        own.AirOperations.Inventory.Items[0].Squadron.Known != "VFA-1" ||
        own.AirOperations.Inventory.Items[0].Loadouts.Items[0].Value != "loadout" ||
        envelope.OrderAvailability.Count != 1)
    {
        return 4;
    }
    if (own.Equipment.Items[0].Resources.Items[0].Amount.Known.Discrete != 0 ||
        own.Equipment.Items[0].Resources.Items[1].Amount.Estimated.Value.Continuous != 12.5 ||
        own.Equipment.Items[0].Resources.Items[2].Amount.StateCase != ResourceKnowledge.StateOneofCase.None ||
        envelope.Snapshot.Memberships.Items[0].Faction.Known.Value != "friendly" ||
        envelope.Snapshot.Memberships.Items[0].TaskForce.Estimated.Value.Value != "group-1")
    {
        return 5;
    }
    Console.WriteLine("PASS C# reads observer-only roundtrip with name, typed memberships, zero, false and unknown");
}
else
{
    return 2;
}

return 0;
