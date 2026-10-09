param([string]$Root = (Split-Path $PSScriptRoot))
$ErrorActionPreference = 'Stop'
function Read-Schema([string]$schemaText) {
    $result = @{}
    foreach ($messageMatch in [regex]::Matches($schemaText, 'message\s+(\w+)\s*\{')) {
        $bodyStart = $messageMatch.Index + $messageMatch.Length
        $braceDepth = 1
        $bodyEnd = $bodyStart
        while ($braceDepth -gt 0 -and $bodyEnd -lt $schemaText.Length) {
            if ($schemaText[$bodyEnd] -eq '{') {
                $braceDepth++
            }
            if ($schemaText[$bodyEnd] -eq '}') {
                $braceDepth--
            }
            $bodyEnd++
        }
        if ($braceDepth -ne 0) {
            throw 'Unbalanced schema'
        }
        $messageBody = $schemaText.Substring($bodyStart, $bodyEnd - $bodyStart - 1)
        $fields = @{}
        foreach ($field in [regex]::Matches($messageBody, '(?:optional\s+|repeated\s+)?(\w+)\s+(\w+)\s*=\s*(\d+)')) {
            if ($fields.ContainsKey($field.Groups[3].Value)) {
                throw 'Duplicate field number'
            }
            $fields[$field.Groups[3].Value] = $field.Groups[1].Value + ':' + $field.Groups[2].Value
        }
        $result[$messageMatch.Groups[1].Value] = $fields

    }
    return $result

}
# Compare declarations to reviewed golden field-number/type/name contract.
$baselineSchema = Get-Content "$Root/test/schema-baseline.json" -Raw | ConvertFrom-Json -AsHashtable
$seenMessages = @{}
foreach ($protoFile in Get-ChildItem "$Root/proto" -Recurse -Filter *.proto) {
    $schemaText = Get-Content $protoFile.FullName -Raw
    foreach ($messageMatch in [regex]::Matches($schemaText, 'message\s+(\w+)')) {
        if ($seenMessages.ContainsKey($messageMatch.Groups[1].Value)) {
            throw 'Duplicate payload declaration'
        }
        $seenMessages[$messageMatch.Groups[1].Value] = $true
    }
    foreach ($entry in (Read-Schema $schemaText).GetEnumerator()) {
        if (!$baselineSchema.ContainsKey($entry.Key)) {
            throw "Unreviewed message $($entry.Key)"
        }
        foreach ($field in $entry.Value.GetEnumerator()) {
            if (!$baselineSchema[$entry.Key].ContainsKey($field.Key) -or $baselineSchema[$entry.Key][$field.Key] -ne $field.Value) {
                throw "Incompatible field $($entry.Key).$($field.Key)"
            }
        }
        foreach ($field in $baselineSchema[$entry.Key].Keys) {
            if (!$entry.Value.ContainsKey($field)) {
                throw "Removed field needs reserved declaration review: $($entry.Key).$field"
            }
        }

    }

}
foreach ($name in $baselineSchema.Keys) {
    if (!$seenMessages.ContainsKey($name)) {
        throw "Missing canonical message $name"
    }
}
# Negative regression: incompatible field-number reuse is demonstrably detected.
$incompatibleSchema = Read-Schema 'message Waypoint { string latitude_deg = 1; double longitude_deg = 2; }'
if ($incompatibleSchema.Waypoint['1'] -eq $baselineSchema.Waypoint['1']) {
    throw 'Mutation regression failed'
}
"PASS canonical schema baseline; incompatible reuse detected"
