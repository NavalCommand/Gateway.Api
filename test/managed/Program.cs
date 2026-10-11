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
    var snapshot = new ObserverSnapshot
    {
        SchemaVersion = 4,
        SessionId = "session",
        Observer = new ObserverId { Value = "blue" },
        Ready = true,
        Version = 7,
        CapturedAtSeconds = 100
    };
    var own = new UnitObservation
    {
        Identity = new ObservationId { Value = "own-contact" },
        Name = "",
        Type = new UnitTypeId { Value = "aircraft" },
        MeasuredAtSeconds = 42,
        Aircraft = new AircraftObservation { Airborne = false },
        Navigation = new NavigationObservation { PlannedRoute = new Route() },
        AirOperations = new AirOperationsObservation
        {
            PendingOperations = 0,
            OnboardCount = 0,
            ReservedCount = 0,
            CooldownCount = 0,
            MeasuredAtSeconds = 42
        }
    };
    own.Kinematics = new KinematicsObservation
    {
        ExactPosition = new GeoPoint { LatitudeDeg = 0, LongitudeDeg = 0 },
        SpeedMps = 0,
        HeadingRad = 0
    };
    own.Equipment.Add(new EquipmentObservation
    {
        TypeId = new EquipmentTypeId { Value = "stores" },
        Active = false
    });
    own.Equipment.Add(new EquipmentObservation());
    own.Equipment[0].Resources.Add(new ResourceObservation
    {
        ResourceId = new ResourceId { Value = "missiles" },
        Amount = new ResourceValue { Discrete = 0 }
    });
    own.AirOperations.Inventory.Add(new AircraftInventoryEntry
    {
        Type = new UnitTypeId { Value = "aircraft" },
        Squadron = "",
        UnreservedCount = 0
    });
    own.AirOperations.Inventory[0].Loadouts.Add(new LoadoutId { Value = "loadout" });
    own.Contributors.Add(new ObservationContributor { Observer = new ObservationId { Value = "sensor-unit" } });
    own.Contributors[0].Sensors.Add(new EquipmentTypeId { Value = "radar" });

    var contact = new UnitObservation
    {
        Identity = new ObservationId { Value = "contact" },
        Kinematics = new KinematicsObservation { PartialPosition = new ObservedPosition { LongitudeDeg = 0, ErrorEllipse = new ObservedErrorEllipse { MajorAxisM = 100, MinorAxisM = 25, OrientationRad = 0 } } }
    };
    snapshot.Units.Add(own);
    snapshot.Units.Add(contact);
    snapshot.Memberships.Add(new ObserverUnitRelationship
    {
        Observation = new ObservationId { Value = "own-contact" },
        Faction = new FactionId { Value = "blue" }
    });

    var discovery = new OrderDiscovery
    {
        SessionId = "session",
        Observer = snapshot.Observer,
        Observation = own.Identity,
        BasedOnVersion = snapshot.Version,
        Eligibility = new OrderEligibilityState()
    };
    discovery.Eligibility.Items.Add(new OrderEligibility
    {
        Kind = OrderKind.ReplaceRoute,
        Availability = Availability.Blocked,
        Reason = "not ready"
    });
    var request = new OrderRequest
    {
        SessionId = "session",
        RequestId = "conformance",
        Observer = new ObserverId { Value = "red" },
        Observation = new ObservationId { Value = "own-contact" },
        BasedOnVersion = 7,
        Waypoint = new Waypoint { LatitudeDeg = 61.96, LongitudeDeg = -10.37, RouteMode = RouteMode.AppendWaypoint }
    };
    File.WriteAllBytes(args[1], request.ToByteArray());
    var envelope = new ProviderObservation { Snapshot = snapshot };
    envelope.OrderAvailability.Add(discovery);
    File.WriteAllBytes(args[1] + ".world", envelope.ToByteArray());
}
else if (args[0] == "read")
{
    var request = OrderRequest.Parser.ParseFrom(File.ReadAllBytes(args[1]));
    if (request.PayloadCase != OrderRequest.PayloadOneofCase.Waypoint || request.Observer.Value != "red" || request.Observation.Value != "own-contact" || request.Waypoint.LongitudeDeg != -10.37)
        throw new InvalidDataException("C++ order request roundtrip failed");
    var envelope = ProviderObservation.Parser.ParseFrom(File.ReadAllBytes(args[1] + ".world"));
    var snapshot = envelope.Snapshot;
    var own = snapshot.Units[0];
    if (snapshot.SchemaVersion != 4 || snapshot.Observer.Value != "blue" || own.Name != "" || !own.HasName || snapshot.Units[1].HasName)
        throw new InvalidDataException("Known empty name or schema presence was lost");
    if (!own.Kinematics.HasSpeedMps || own.Kinematics.SpeedMps != 0 || !own.Aircraft.HasAirborne || own.Aircraft.Airborne)
        throw new InvalidDataException("Known zero/false scalar presence was lost");
    if (own.Kinematics.PositionStateCase != KinematicsObservation.PositionStateOneofCase.ExactPosition ||
        own.Kinematics.ExactPosition.LatitudeDeg != 0 || !own.Kinematics.ExactPosition.HasLatitudeDeg ||
        !own.Kinematics.ExactPosition.HasLongitudeDeg ||
        snapshot.Units[1].Kinematics.PositionStateCase != KinematicsObservation.PositionStateOneofCase.PartialPosition ||
        snapshot.Units[1].Kinematics.PartialPosition.ErrorEllipse is null ||
        snapshot.Units[1].Kinematics.PartialPosition.ErrorEllipse.MajorAxisM != 100 ||
        snapshot.Units[1].Kinematics.PartialPosition.ErrorEllipse.MinorAxisM != 25)
        throw new InvalidDataException("Exact own position and partial contact position/error geometry were not kept distinct");
    if (own.Navigation.PlannedRoute is null || own.Navigation.PlannedRoute.Points.Count != 0)
        throw new InvalidDataException("Known empty route presence was lost");
    if (own.Equipment.Count != 2 || own.Equipment[0].Resources.Count != 1 || own.Equipment[0].Resources[0].Amount.Discrete != 0 ||
        own.Equipment[1].TypeId is not null || own.Equipment[1].Resources.Count != 0)
        throw new InvalidDataException("Reported equipment, unknown type, or exact zero was lost");
    if (!own.HasMeasuredAtSeconds || own.MeasuredAtSeconds != 42 || snapshot.Units[1].HasMeasuredAtSeconds)
        throw new InvalidDataException("Separate optional measurement time was lost or fabricated for a contact");
    if (own.AirOperations.Inventory.Count != 1 || own.AirOperations.Inventory[0].Type is null ||
        !own.AirOperations.Inventory[0].HasSquadron || own.AirOperations.Inventory[0].Squadron != "" ||
        !own.AirOperations.Inventory[0].HasUnreservedCount || own.AirOperations.Inventory[0].UnreservedCount != 0 ||
        own.AirOperations.Inventory[0].Loadouts.Count != 1)
        throw new InvalidDataException("Grouped inventory exact presence or reported loadout choice was lost");
    if (own.Contributors.Count != 1 || own.Contributors[0].Observer.Value != "sensor-unit" || own.Contributors[0].Sensors[0].Value != "radar")
        throw new InvalidDataException("Typed observer/sensor contribution was lost");
    if (snapshot.Memberships.Count != 1 || snapshot.Memberships[0].Faction is null || snapshot.Memberships[0].TaskForce is not null)
        throw new InvalidDataException("Optional observer membership presence was lost");
    if (envelope.OrderAvailability.Count != 1 || envelope.OrderAvailability[0].Eligibility.Items.Count != 1)
        throw new InvalidDataException("Reported order-availability rows were lost");
}
else
{
    return 2;
}

return 0;
