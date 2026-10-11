# Gateway.Api 0.1.0

Project-wide documentation is maintained in the separate **Docs Git repository**. Before changing this module, explicitly read [Docs/AGENTS.md](../Docs/AGENTS.md), [architecture](../Docs/ARCHITECTURE.md), [module/build policy](../Docs/MODULE_STRUCTURE.md), [local sources](../Docs/LOCAL_SOURCES.md), and [code style](../Docs/CODE_STYLE.md) with its [owner export](../Docs/CodeStyle.xml). The shared instructions are not discovered automatically across repository boundaries; do not create a competing AGENTS.md in the parent workspace. Historical reports and reproduction guides are indexed under [Docs/Tasks/](../Docs/Tasks/README.md), with run receipts under [Docs/Evidence/](../Docs/Evidence/README.md).


Canonical payloads, domain ↔ wire codec and host-neutral provider interfaces. Operational C++ contracts are owned by the independent Domain.Api repository. No game or gateway implementation dependency. C# and C++ generate from the same canonical files under `proto/navalcommand/gateway/v1` using protoc 33.4; managed Google.Protobuf is 3.33.4. **Schema version 4 is a direct breaking replacement** with observer-only operational snapshots and an isolated privileged recording envelope, not a compatibility projection. The package path is retained; producers and consumers must use the same checkout graph.

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

The canonical public operational contract is observer-only. `ObserverSnapshot` contains an `ObserverId`, independent per-observer `ObservationId` contacts, observer relationships keyed by `ObservationId`, and separate capture/measurement evidence. It never contains `Units.Api::UnitId`, `SimulationSnapshot`, authoritative membership, or truth/contact correlations. Friendly and hostile observations use the same `ObservationId`; false, duplicate, fused and many-to-many contacts need no matching truth entity.

`Exact<T>` is encoded using optional scalar/message presence: a present zero or false is Known, and absence is Unknown. The position field uses oneof alternatives for exact own/source position and specialized partial localization, with optional source-backed error-ellipse axes and orientation. Empty routes remain known-empty when their route field is present.

Observed equipment, contributor, inventory, and order-availability fields are reported-item lists, not exhaustive-coverage claims. An empty list means no items/rows were reported; it does not prove absence, disarmament, no contributors, or unsupported orders. Air inventory groups one count by type and squadron, with loadout choices separately listed and their own presence; counts are not repeated for each choice. A missing order row never grants execution. Contributors refer to observer-visible `ObservationId` handles and source-supported sensor types, never privileged `UnitId`.

`Gateway.Api` codecs validate observer/session scope and reject malformed identity/reference relationships. `world.proto` contains only operational observer contracts; privileged `UnitId`, truth snapshots, and truth-observation associations are confined to internal `recording.proto`, which is not imported by public service/provider schemas.

Maintained tests cover independent observer identities, false/fused contacts, projectile units without platform fields, privileged recording associations and decisions/outcomes, per-observation order overlays, and C# ↔ C++ schema conformance for Known(0), Known(false), Known-empty routes, Unknown values, exact versus partial positions, and reported-list semantics. `test/Check-Schema.ps1 -Protoc <resolved-protoc>` compiles the canonical schema and enforces the public truth/observation boundary. Run the documented build/CTest commands; no live game is required.
