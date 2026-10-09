# Gateway.Api 0.1.0
Canonical payload serialization and host-neutral provider communication contracts.

Operational meaning lives in the logical World.Api and Orders.Api header namespaces here; neither domain depends on generated types. They are small real contracts, not extra empty repositories. Codec is the transport boundary. IProvider returns owned domain values and runs on I/O threads; it must never call a simulator. Latitude/longitude use degrees, distance/height meters, speed meters/second, time seconds since session start.

All shared payloads are declared exactly once in proto/navalcommand/gateway/v1. C# and C++ codegen uses the same pinned source-built protoc 33.4 (vcpkg protobuf 6.33.4#2); managed runtime Google.Protobuf 3.33.4. Wire schema version 1: additions use fresh field numbers; removals require reserved names/numbers and reviewed baseline migration. Enum zero means unknown. Proto3 optional fields preserve absent versus zero. Unknown fields survive direct generated-code serialization; domain mapping deliberately understands schema 1 only.

Build C++: set VCPKG_ROOT, NC_VCPKG_INSTALLED_DIR, NC_CMAKE (CMake >=4.2); run ./scripts/Build-Native.ps1. C# independently: dotnet build managed/Gateway.Api.csproj -c Release -p:Protoc=ABSOLUTE_SOURCE_BUILT_PROTOC. Run test/Check-Schema.ps1 and test/managed/Conformance.csproj write/read plus schema-roundtrip.exe. vcpkg.json pins source dependencies; CPM.cmake 0.42.0 is vendored with its license header. No game or gateway implementation dependency.

Our targets request C++26, REQUIRED; Clang 19.1.5 uses explicit /clang:-std=c++2c with the MSVC 19.44 standard library. std::expected is a configure-time essential feature check. This does not imply that the selected toolchain implements the entire evolving C++26 library.

Generated files/build artifacts stay under ignored out/bin/obj. No game assemblies are distributed.


