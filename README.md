# Gateway.Api 0.1.0

Project-wide documentation is maintained in the separate **Docs Git repository**. Before changing this module, explicitly read [Docs/AGENTS.md](../Docs/AGENTS.md), [architecture](../Docs/ARCHITECTURE.md), [module/build policy](../Docs/MODULE_STRUCTURE.md), [local sources](../Docs/LOCAL_SOURCES.md), and [code style](../Docs/CODE_STYLE.md) with its [owner export](../Docs/CodeStyle.xml). The shared instructions are not discovered automatically across repository boundaries; do not create a competing AGENTS.md in the parent workspace. Historical reports and reproduction guides are indexed under [Docs/Tasks/](../Docs/Tasks/README.md), with run receipts under [Docs/Evidence/](../Docs/Evidence/README.md).


Canonical payloads, domain ↔ wire codec and host-neutral provider interfaces. Operational C++ contracts are owned by the independent Domain.Api repository. No game or gateway implementation dependency. C# and C++ generate from the same canonical files under `proto/navalcommand/gateway/v1` using protoc 33.4; managed Google.Protobuf is 3.33.4. **Schema version 3 is a direct breaking replacement**, not a compatibility projection. The package path is retained; producers and consumers must use the same checkout graph.

The native production library/codec is defined in `src/CMakeLists.txt`. Gateway-facing headers expose required Physics.Api, Uncertainty.Api, Catalog.Api, Units.Api, Orders.Api and World.Api dependencies PUBLIC. `schema-roundtrip.exe` and `api-tests.exe` are built beneath the selected `o/` directory. Managed build: `dotnet build managed/Gateway.Api.csproj -c Release` after CMake configure. Bindings, including the gateway composition envelope, are generated only by the existing native/managed build steps into ignored output directories.

## Build structure

Root CMakeLists.txt initializes/composes only. Shared policy/resolution lives in the independent Build.CMake repository, fetched as ncbuild. Production libraries are defined in src/, mocks in mock/, tests in test/, applications in app/. Namespace paths and existing target names are preserved.

Enter a VS 2026 x64 developer shell, set standard VCPKG_ROOT, and use CMake 4.2+ and Ninja:

```powershell
cmake --preset windows-msvc-release -DFETCHCONTENT_SOURCE_DIR_NCBUILD=/absolute/path/to/Build.CMake
cmake --build o/windows-msvc-release --parallel 8
ctest --test-dir o/windows-msvc-release --output-on-failure --no-tests=error
```

The bootstrap option is unnecessary once Build.CMake is available at its declared Git URL. Optional ignored Build.Local.cmake registers canonical module URLs with NavalCommandSetDependencyPath(URL, absoluteCheckout); it is read once at the top level before declaration and applies transitively. Without a registration the declared Git URL and main reference are used. There is no workspace-root environment variable or implicit sibling lookup. External dependencies remain source-built vcpkg manifest packages. An already provisioned prefix can use standard -DVCPKG_INSTALLED_DIR=... -DVCPKG_MANIFEST_INSTALL=OFF cache options. See the separate [Docs repository build policy](../Docs/MODULE_STRUCTURE.md) for current commands and limits; dated measurements belong to the [task archive](../Docs/Tasks/README.md).

Presets contain configurePresets only, windows-msvc-debug/release and linux-clang-debug/release where applicable. Output is o/${presetName}. C++26 policy is centralized; MSVC uses /std:c++latest for available features. No full C++26 conformance claim. Build outputs, Build.Local.cmake and CMakeUserPresets.json are ignored. No proprietary game binaries are included.

Windows target and host dependencies use `x64-windows-static`, matching `/MT` in Release and `/MTd` in Debug. Reconfigure with `--fresh` and relink when migrating earlier dynamic-CRT artifacts.


## Managed build integration

Follow the authoritative [managed build policy](../Docs/MANAGED_BUILD.md). One CMake configure resolves internal source dependencies and persists each managed repository's ignored `o/managed/Build.Context.props`. Then ordinary `.csproj` builds in Visual Studio/Rider or `dotnet build ... -c Debug/Release` use those sources through real ProjectReferences without extra properties or another configure. Change source/tool/game-reference locations by reconfiguring, not by editing generated projects. Conflicting contexts require explicit `-DNAVALCOMMAND_REPLACE_MANAGED_CONTEXT=ON`; existing CMake consumers must then be reconfigured.

All repository-owned generated files stay under `o/`: native preset directories, per-project MSBuild `o/managed/<project>/{bin,obj}/` directories and mod `o/package`/`o/harness` stages. MSBuild preserves configuration/platform/framework dimensions. The shared helper discovers managed/CMakeLists.txt and invokes only the requested top-level project; MSBuild owns NuGet and transitive compilation. Game-reference absence blocks only requested game-dependent projects.

## Composed world contracts

`Units.Api` exposes one `UnitInstance` shape; `World.Api::View::Instances` aggregates all observer-visible records. Every record and order reference uses the same opaque observer/session-scoped `UnitId`. Identity is not command authority: the provider must revalidate current control, side membership, session and protected native automation at dispatch. Catalog references are strong IDs. Native physical values are strongly typed; protobuf field names specify degrees, radians and SI units at the serialization boundary.

Known/Estimated/Unknown knowledge uses oneof presence, including explicit estimated scalar presence. Known zero, false and empty routes remain meaningful. Unknown routes mean uninspected/unknown. Runtime equipment and resources each have independent Unknown/Partial/Complete coverage; complete-empty resources mean confirmed not applicable, not unknown. Resource values preserve discrete `uint64` counts versus continuous quantities; catalog resource definitions own units/accounting semantics. Snapshot capture time never substitutes for a contact's genuine last measurement time, and estimate source text is provenance rather than a fabricated sensor identity.

Carrier/airfield inventory is an attached `AirOperations` unit component, with observed inventory coverage, strong type/loadout selectors, known/unknown counts and lifecycle observations. Inventory contains no eligibility. Faction/task-force memberships live in a World relationship overlay keyed by UnitId. No separate public facility, stock, own-unit/contact ID variants or duplicate equipment physical model remains.

`Orders.Api` owns typed order kinds and per-unit availability separately. Requests contain exactly one route or aircraft-launch payload. Selector-specific launch eligibility rows carry dynamic reasons separately from inventory. Unsupported route modes remain unchanged through decoding so the provider can return its grounded operational rejection receipt.

`WorldSnapshot` does not import orders. The gateway-only `ProviderObservation` composition envelope combines `Snapshot` with independent `OrderAvailability` rows for the managed/native bridge; external world observation and order discovery remain separate. The codec rejects mismatched sessions, duplicate/orphan identities, inconsistent versions, malformed knowledge and Unknown collections containing members. Lifecycle enum values and direct generated-message unknown fields remain preservable; domain conversion does not promise preservation of arbitrary unknown protobuf fields. Environment, catalog retrieval and Sea Power equipment extraction are not invented by this schema.

Maintained tests cover native strong-value codecs, independent transitive consumption and C# → C++ domain → C# conformance, including zero/false, unknown values, collection coverage, discrete/continuous resources, genuine measurement versus capture time, attached inventory, relationship and order overlays. `test/Check-Schema.ps1 -Protoc <resolved-protoc>` compiles the canonical schema rather than testing incidental source spelling. Run the documented build/CTest commands; no live game is required.
