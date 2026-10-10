# Gateway.Api 0.1.0

Project-wide documentation is maintained in the separate **Docs Git repository**. Before changing this module, explicitly read [Docs/AGENTS.md](../Docs/AGENTS.md), [architecture](../Docs/ARCHITECTURE.md), [module/build policy](../Docs/MODULE_STRUCTURE.md), [local sources](../Docs/LOCAL_SOURCES.md), and [code style](../Docs/CODE_STYLE.md) with its [owner export](../Docs/CodeStyle.xml). The shared instructions are not discovered automatically across repository boundaries; do not create a competing AGENTS.md in the parent workspace. Historical reports and reproduction guides are indexed under [Docs/Tasks/](../Docs/Tasks/README.md), with run receipts under [Docs/Evidence/](../Docs/Evidence/README.md).


Canonical payloads, domain ↔ wire codec and host-neutral provider interfaces. Operational C++ value contracts are owned by the independent Domain.Api repository. No game or gateway implementation dependency. The schema remains unchanged from Task 004, under proto/navalcommand/gateway/v1. C# and C++ generate from the same files using protoc 33.4; managed Google.Protobuf is 3.33.4. Schema version 1, optional presence, zero unknown enums and direct unknown-field preservation remain unchanged. Domain mapping understands schema 1 only.

The native production library/codec is defined in src/CMakeLists.txt. Gateway-facing headers remain here; domain headers retain their include spellings through PUBLIC Orders.Api and World.Api dependencies resolved by Build.CMake. app/schema-roundtrip.exe and test/api-tests.exe are built beneath the selected o/ directory. Managed build: dotnet build managed/Gateway.Api.csproj -c Release (after CMake configure, no extra properties). Existing Check-Schema and managed Conformance tests are retained.

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

Public aggregate member spellings remain source compatible with existing clients. Domain.Api purpose-specific headers define individual types; `Order.h` and `View.h` retain their previous include surface. Legacy camelCase public fields are deliberate compatibility exceptions to the owner's PascalCase rule. Generated Protobuf types and methods retain generator-defined names.

## Managed build integration

Follow the authoritative [managed build policy](../Docs/MANAGED_BUILD.md). One CMake configure resolves internal source dependencies and persists each managed repository's ignored `o/managed/Build.Context.props`. Then ordinary `.csproj` builds in Visual Studio/Rider or `dotnet build ... -c Debug/Release` use those sources through real ProjectReferences without extra properties or another configure. Change source/tool/game-reference locations by reconfiguring, not by editing generated projects. Conflicting contexts require explicit `-DNAVALCOMMAND_REPLACE_MANAGED_CONTEXT=ON`; existing CMake consumers must then be reconfigured.

All repository-owned generated files stay under `o/`: native preset directories, per-project MSBuild `o/managed/<project>/{bin,obj}/` directories and mod `o/package`/`o/harness` stages. MSBuild preserves configuration/platform/framework dimensions. The shared helper discovers managed/CMakeLists.txt and invokes only the requested top-level project; MSBuild owns NuGet and transitive compilation. Game-reference absence blocks only requested game-dependent projects.

The new Domain.Api repository is initially local and unpublished. Add its canonical URL (`https://github.com/NavalCommand/Domain.Api.git`) and local checkout through NavalCommandSetDependencyPath in the ignored top-level Build.Local.cmake, including when configuring a transitive consumer. Public Git fetching remains blocked until the owner publishes that repository. No header shadows or forwarding copies remain in Gateway.Api.
