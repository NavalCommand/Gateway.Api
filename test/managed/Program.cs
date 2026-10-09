using Google.Protobuf;
using NavalCommand.Gateway.Wire;
using System;
using System.IO;
using System.Linq;
if(args[0]=="write"){
 var r=new OrderRequest{SessionId="session-µ",RequestId="conformance",EntityId="opaque-α",BasedOnVersion=9007199254740993UL,Waypoint=new Waypoint{LatitudeDeg=61.96,LongitudeDeg=0}};
 using var output=File.Create(args[1]);r.WriteTo(output);
 using var coded=new CodedOutputStream(output,leaveOpen:true);coded.WriteTag(123,WireFormat.WireType.Varint);coded.WriteUInt32(9876);coded.Flush();
 Console.WriteLine("PASS C# serialize canonical payload, UTF-8, uint64 and oneof with unknown field");
}else{
 var bytes=File.ReadAllBytes(args[1]);var r=OrderRequest.Parser.ParseFrom(bytes);
 if(r.SessionId!="session-µ"||r.EntityId!="opaque-α"||r.BasedOnVersion!=9007199254740993UL||r.ArgumentsCase!=OrderRequest.ArgumentsOneofCase.Waypoint||r.Waypoint.LongitudeDeg!=-10.37)throw new Exception("conformance");
 using var data=new MemoryStream();r.WriteTo(data);using(var c=new CodedOutputStream(data,leaveOpen:true)){c.WriteTag(123,WireFormat.WireType.Varint);c.WriteUInt32(9876);c.Flush();}
 if(r.CalculateSize()==data.Length)throw new Exception("unknown field missing");
 var wire=r.ToByteArray();if(!wire.SequenceEqual(bytes))throw new Exception("unknown field preservation");
 var optional=new OrderResult{DisplacementM=0};if(!OrderResult.Parser.ParseFrom(optional.ToByteArray()).HasDisplacementM)throw new Exception("optional zero");
 var enumValue=new OrderResult{Stage=(Stage)99};if((int)OrderResult.Parser.ParseFrom(enumValue.ToByteArray()).Stage!=99)throw new Exception("enum preservation");
 Console.WriteLine("PASS C++ -> C#; unknown fields, optional zero and unknown enum preserved");
}

