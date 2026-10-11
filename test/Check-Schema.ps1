param(
    [string]$Root = (Split-Path $PSScriptRoot),
    [Parameter(Mandatory)][string]$Protoc
)
$ErrorActionPreference = 'Stop'
$protoRoot = Join-Path $Root 'proto'
$outputDirectory = Join-Path $Root 'o/schema-check'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$protoFiles = Get-ChildItem -LiteralPath $protoRoot -Recurse -Filter *.proto
& $Protoc "--proto_path=$protoRoot" "--descriptor_set_out=$outputDirectory/canonical.pb" --include_imports @($protoFiles.FullName)
if ($LASTEXITCODE) {
    throw 'Canonical protobuf compilation failed'
}
$world = Get-Content -LiteralPath "$protoRoot/navalcommand/gateway/v1/world.proto" -Raw
if ($world -match 'message\s+(Unit|Track)\s*\{' -or $world -match '\b(own_units|tracks)\s*=') {
    throw 'Obsolete split world representation'
}
foreach ($required in @('repeated UnitInstance instances', 'oneof state', 'CollectionCoverage coverage')) {
    if (!$world.Contains($required)) {
        throw "Missing canonical composition: $required"
    }
}
$observation = Get-Content -LiteralPath "$protoRoot/navalcommand/gateway/v1/observation.proto" -Raw
if (!$observation.Contains('oneof identity')) {
    throw 'Missing tagged subject identity'
}
'PASS canonical compilation, unified identities, knowledge and coverage'
