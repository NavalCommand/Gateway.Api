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
if ($LASTEXITCODE) { throw 'Canonical protobuf compilation failed' }

function Get-ProtoMessageBody([string]$Text, [string]$Name) {
    $match = [regex]::Match($Text, "(?m)^\s*message\s+$([regex]::Escape($Name))\s*\{")
    if (-not $match.Success) { throw "Required protobuf message '$Name' was not found" }
    $open = $Text.IndexOf('{', $match.Index)
    $depth = 0
    for ($index = $open; $index -lt $Text.Length; $index++) {
        if ($Text[$index] -eq '{') { $depth++ }
        elseif ($Text[$index] -eq '}') {
            $depth--
            if ($depth -eq 0) { return $Text.Substring($open + 1, $index - $open - 1) }
        }
    }
    throw "Unclosed protobuf message '$Name'"
}
$world = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/world.proto') -Raw
$orders = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/orders.proto') -Raw
$provider = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/provider.proto') -Raw
$session = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/session.proto') -Raw
$incidents = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/incidents.proto') -Raw
$recording = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/recording.proto') -Raw
$publicMessages = @(
    @{ Text = $world; Name = 'ObserverSnapshot' },
    @{ Text = $world; Name = 'UnitObservation' },
    @{ Text = $world; Name = 'ObserverUnitRelationship' },
    @{ Text = $world; Name = 'EquipmentObservation' },
    @{ Text = $provider; Name = 'ProviderObservation' },
    @{ Text = $orders; Name = 'OrderRequest' },
    @{ Text = $orders; Name = 'OrderResult' },
    @{ Text = $orders; Name = 'OrderDiscovery' },
    @{ Text = $orders; Name = 'OrderEligibility' },
    @{ Text = $session; Name = 'Query' },
    @{ Text = $session; Name = 'Session' },
    @{ Text = $incidents; Name = 'Incident' }
)
$forbidden = '\b(UnitId|SimulationSnapshot|TrueUnitInstance|PrivilegedObservationAssociation|RecordedSession)\b|\b(unit_id|spawned_unit_id|truth_to_observation|truth_id)\b'
foreach ($message in $publicMessages) {
    $body = Get-ProtoMessageBody $message.Text $message.Name
    if ($body -match $forbidden) { throw "Public protobuf message '$($message.Name)' exposes privileged truth identity or recording data" }
    if ($body -match '\bsubject\s*=') { throw "Public protobuf message '$($message.Name)' uses obsolete ambiguous subject field" }
}
$unitObservationBody = Get-ProtoMessageBody $world 'UnitObservation'
if ($unitObservationBody -notmatch 'ObservationMetadata\s+evidence\s*=\s*8') { throw 'UnitObservation must retain independent measurement evidence' }
if ($unitObservationBody -notmatch 'StringKnowledge\s+name\s*=\s*9') { throw 'UnitObservation must carry source-backed name knowledge' }
$relationshipBody = Get-ProtoMessageBody $world 'ObserverUnitRelationship'
if ($relationshipBody -notmatch 'FactionIdKnowledge\s+faction' -or $relationshipBody -notmatch 'TaskForceIdKnowledge\s+task_force') { throw 'Observer relationships require strongly typed uncertain membership IDs' }
$snapshotBody = Get-ProtoMessageBody $world 'ObserverSnapshot'
if ($snapshotBody -notmatch '\buint32\s+schema_version\s*=\s*9') { throw 'ObserverSnapshot must carry schema_version field 9' }
$sessionBody = Get-ProtoMessageBody $session 'Session'
if ($sessionBody -notmatch 'schema_version\s*=\s*1;\s*//\s*Current canonical wire schema version is 4') { throw 'Session schema version must be documented as 4' }
$inventoryBody = Get-ProtoMessageBody $world 'AircraftInventoryEntry'
$loadoutsBody = Get-ProtoMessageBody $world 'ObservedLoadouts'
foreach ($field in @('UnitTypeIdKnowledge\s+type', 'StringKnowledge\s+squadron', 'UInt32Knowledge\s+unreserved_count', 'ObservedLoadouts\s+loadouts')) {
    if ($inventoryBody -notmatch $field) { throw "AircraftInventoryEntry is missing grouped inventory field '$field'" }
}
if (-not $loadoutsBody.Contains('repeated LoadoutId items')) { throw 'ObservedLoadouts must expose typed loadout choices with coverage' }
$launchBody = Get-ProtoMessageBody $orders 'LaunchAircraft'
if ($launchBody -notmatch 'AircraftSelection\s+selection') { throw 'LaunchAircraft must reuse AircraftSelection' }
$providerBody = Get-ProtoMessageBody $provider 'ProviderObservation'
if ($providerBody -notmatch '\bObserverSnapshot\s+snapshot\s*=') { throw 'ProviderObservation must carry ObserverSnapshot as its only snapshot' }
if ($providerBody -notmatch '\bOrderDiscovery\s+order_availability\s*=') { throw 'ProviderObservation must carry its separate order overlay' }
$requestBody = Get-ProtoMessageBody $orders 'OrderRequest'
if ($requestBody -notmatch '\bObserverId\s+observer\s*=') { throw 'OrderRequest lacks observer scope' }
if ($requestBody -notmatch '\bObservationId\s+observation\s*=') { throw 'OrderRequest lacks observation handle' }
if ($requestBody -notmatch 'oneof\s+payload') { throw 'OrderRequest must have exactly one typed payload oneof' }
$resultBody = Get-ProtoMessageBody $orders 'OrderResult'
if ($resultBody -notmatch '\bObservationId\s+spawned_observation_id\s*=') { throw 'OrderResult must expose only an observer-scoped spawned contact handle' }
$recordBody = Get-ProtoMessageBody $recording 'RecordedSession'
foreach ($field in @('SimulationSnapshot\s+simulation', 'ObserverSnapshot\s+perspectives', 'PrivilegedObservationAssociation\s+associations', 'OrderRequest\s+decisions', 'OrderResult\s+outcomes')) {
    if ($recordBody -notmatch $field) { throw "Privileged RecordedSession is missing required field '$field'" }
}
'PASS canonical protobuf compilation and observer/truth schema boundary'
