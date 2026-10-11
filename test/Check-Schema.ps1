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
$observation = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/observation.proto') -Raw
$orders = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/orders.proto') -Raw
$provider = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/provider.proto') -Raw
$session = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/session.proto') -Raw
$incidents = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/incidents.proto') -Raw
$recording = Get-Content -LiteralPath (Join-Path $protoRoot 'navalcommand/gateway/v1/recording.proto') -Raw
$allProto = $world + $observation + $orders + $provider + $session + $incidents + $recording
if ($allProto -match '\b(Knowledge|ObservedCollection|CollectionCoverage|Estimated|Estimate)\b') { throw 'Legacy knowledge/coverage/estimate protobuf wrappers are forbidden' }
if ($world -match '(?m)^\s*message\s+(UnitId|SimulationSnapshot|TrueUnitInstance|PrivilegedObservationAssociation)\b') { throw 'Privileged wire definitions must remain isolated in recording.proto' }

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
$unit = Get-ProtoMessageBody $world 'UnitObservation'
if ($unit -notmatch 'optional\s+string\s+name\s*=\s*9') { throw 'UnitObservation must preserve known-empty name presence' }
if ($unit -notmatch 'repeated\s+ObservationContributor\s+contributors\s*=\s*10') { throw 'UnitObservation must expose reported typed contributors' }
if ($unit -notmatch 'optional\s+double\s+measured_at_seconds\s*=\s*8') { throw 'UnitObservation must carry optional measurement time separate from snapshot capture time' }
if ($unit -match '\b(UnitId|unit_id)\b') { throw 'UnitObservation cannot expose true identity' }
$contributor = Get-ProtoMessageBody $observation 'ObservationContributor'
if ($contributor -notmatch 'ObservationId\s+observer\s*=\s*1' -or $contributor -notmatch 'repeated\s+EquipmentTypeId\s+sensors\s*=\s*2') {
    throw 'ObservationContributor must use an observation handle and source-supported typed sensor IDs'
}
if ($unit -match '\b(source|confidence|provenance|cache_path)\b') { throw 'UnitObservation cannot expose generic provenance/confidence fields' }
$position = Get-ProtoMessageBody $world 'ObservedPosition'
foreach ($field in @('optional\s+double\s+latitude_deg', 'optional\s+double\s+longitude_deg', 'ObservedErrorEllipse\s+error_ellipse')) {
    if ($position -notmatch $field) { throw "ObservedPosition is missing partial coordinate/error field '$field'" }
}
$ellipse = Get-ProtoMessageBody $world 'ObservedErrorEllipse'
foreach ($field in @('double\s+major_axis_m', 'double\s+minor_axis_m', 'double\s+orientation_rad')) {
    if ($ellipse -notmatch $field) { throw "ObservedErrorEllipse is missing geometry field '$field'" }
}
$kinematics = Get-ProtoMessageBody $world 'KinematicsObservation'
foreach ($field in @('oneof\s+position_state', 'GeoPoint\s+exact_position', 'ObservedPosition\s+partial_position', 'optional\s+double\s+speed_mps', 'optional\s+double\s+heading_rad', 'optional\s+double\s+course_over_ground_rad')) {
    if ($kinematics -notmatch $field) { throw "KinematicsObservation is missing exact/partial position or scalar field '$field'" }
}
$navigation = Get-ProtoMessageBody $world 'NavigationObservation'
if ($navigation -notmatch 'optional\s+Route\s+planned_route') { throw 'NavigationObservation must preserve known-empty routes by presence' }
$relationship = Get-ProtoMessageBody $world 'ObserverUnitRelationship'
if ($relationship -notmatch 'optional\s+FactionId\s+faction' -or $relationship -notmatch 'optional\s+TaskForceId\s+task_force') { throw 'Observer relationships require typed optional membership IDs' }
$snapshot = Get-ProtoMessageBody $world 'ObserverSnapshot'
if ($snapshot -notmatch '\buint32\s+schema_version\s*=\s*9') { throw 'ObserverSnapshot must carry schema_version field 9' }
if ($snapshot -match '\b(SimulationSnapshot|UnitId|PrivilegedObservationAssociation)\b') { throw 'ObserverSnapshot cannot expose privileged state' }
$sessionBody = Get-ProtoMessageBody $session 'Session'
if ($sessionBody -notmatch 'schema_version\s*=\s*1;\s*//\s*Current canonical wire schema version is 4') { throw 'Session schema version must be documented as 4' }
$inventory = Get-ProtoMessageBody $world 'AircraftInventoryEntry'
foreach ($field in @('optional\s+UnitTypeId\s+type', 'optional\s+string\s+squadron', 'optional\s+uint32\s+unreserved_count', 'repeated\s+LoadoutId\s+loadouts')) {
    if ($inventory -notmatch $field) { throw "AircraftInventoryEntry is missing presence/reported-items field '$field'" }
}
$orderState = Get-ProtoMessageBody $orders 'OrderEligibilityState'
if ($orderState -notmatch 'repeated\s+OrderEligibility\s+items\s*=\s*1' -or $orderState -match 'coverage') { throw 'OrderEligibilityState must contain only reported rows, without coverage' }
$launch = Get-ProtoMessageBody $orders 'LaunchAircraft'
if ($launch -notmatch 'AircraftSelection\s+selection') { throw 'LaunchAircraft must reuse AircraftSelection' }
$providerBody = Get-ProtoMessageBody $provider 'ProviderObservation'
if ($providerBody -notmatch '\bObserverSnapshot\s+snapshot\s*=') { throw 'ProviderObservation must carry ObserverSnapshot as its only snapshot' }
if ($providerBody -notmatch '\bOrderDiscovery\s+order_availability\s*=') { throw 'ProviderObservation must carry its separate order overlay' }
$request = Get-ProtoMessageBody $orders 'OrderRequest'
if ($request -notmatch '\bObserverId\s+observer\s*=') { throw 'OrderRequest lacks observer scope' }
if ($request -notmatch '\bObservationId\s+observation\s*=') { throw 'OrderRequest lacks observation handle' }
if ($request -notmatch 'oneof\s+payload') { throw 'OrderRequest must have exactly one typed payload oneof' }
$result = Get-ProtoMessageBody $orders 'OrderResult'
if ($result -notmatch '\bObservationId\s+spawned_observation_id\s*=') { throw 'OrderResult must expose only an observer-scoped spawned contact handle' }
$recordBody = Get-ProtoMessageBody $recording 'RecordedSession'
foreach ($field in @('SimulationSnapshot\s+simulation', 'ObserverSnapshot\s+perspectives', 'PrivilegedObservationAssociation\s+associations', 'OrderRequest\s+decisions', 'OrderResult\s+outcomes')) {
    if ($recordBody -notmatch $field) { throw "Privileged RecordedSession is missing required field '$field'" }
}
$membership = Get-ProtoMessageBody $recording 'AuthoritativeMembership'
if ($membership -notmatch 'UnitId\s+unit_id\s*=\s*1' -or $membership -match '\bsubject\b') {
    throw 'AuthoritativeMembership must name its truth identity with the typed unit_id field'
}
'PASS canonical protobuf compilation and Exact observer/truth schema boundary'
