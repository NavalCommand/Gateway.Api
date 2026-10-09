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
        EntityId = "opaque-О±",
        BasedOnVersion = 9007199254740993UL,
        Waypoint = new Waypoint
        {
            LatitudeDeg = 61.96,
            LongitudeDeg = 0
        }
    };
    using var output = File.Create(args[1]);
    request.WriteTo(output);
    using var unknownFieldWriter = new CodedOutputStream(output, leaveOpen: true);
    unknownFieldWriter.WriteTag(123, WireFormat.WireType.Varint);
    unknownFieldWriter.WriteUInt32(9876);
    unknownFieldWriter.Flush();
    Console.WriteLine("PASS C# serialize canonical payload, UTF-8, uint64 and oneof with unknown field");
}
else
{
    var serializedRequest = File.ReadAllBytes(args[1]);
    var request = OrderRequest.Parser.ParseFrom(serializedRequest);
    if (request.SessionId != "session-Вµ" || request.EntityId != "opaque-О±" || request.BasedOnVersion != 9007199254740993UL
        || request.ArgumentsCase != OrderRequest.ArgumentsOneofCase.Waypoint || request.Waypoint.LongitudeDeg != -10.37)
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

    Console.WriteLine("PASS C++ -> C#; unknown fields, optional zero and unknown enum preserved");
}
