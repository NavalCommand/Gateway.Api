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
'PASS canonical protobuf compilation'
