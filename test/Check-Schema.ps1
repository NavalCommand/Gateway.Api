param([string]$Root=(Split-Path $PSScriptRoot))
$ErrorActionPreference='Stop'
function Read-Schema([string]$text){
 $result=@{}
 foreach($m in [regex]::Matches($text,'message\s+(\w+)\s*\{')){
  $start=$m.Index+$m.Length
  $depth=1;$end=$start
  while($depth -gt 0 -and $end -lt $text.Length){if($text[$end] -eq '{'){$depth++};if($text[$end] -eq '}'){$depth--};$end++}
  if($depth -ne 0){throw 'Unbalanced schema'}
  $body=$text.Substring($start,$end-$start-1)
  $fields=@{}
  foreach($field in [regex]::Matches($body,'(?:optional\s+|repeated\s+)?(\w+)\s+(\w+)\s*=\s*(\d+)')){if($fields.ContainsKey($field.Groups[3].Value)){throw 'Duplicate field number'};$fields[$field.Groups[3].Value]=$field.Groups[1].Value+':'+$field.Groups[2].Value}
  $result[$m.Groups[1].Value]=$fields
 }
 return $result
}
# Compare declarations to reviewed golden field-number/type/name contract.
$gold=Get-Content "$Root/test/schema-baseline.json" -Raw | ConvertFrom-Json -AsHashtable
$seen=@{}
foreach($path in Get-ChildItem "$Root/proto" -Recurse -Filter *.proto){
 $text=Get-Content $path.FullName -Raw
 foreach($m in [regex]::Matches($text,'message\s+(\w+)')){if($seen.ContainsKey($m.Groups[1].Value)){throw 'Duplicate payload declaration'};$seen[$m.Groups[1].Value]=$true}
 foreach($entry in (Read-Schema $text).GetEnumerator()){
  if(!$gold.ContainsKey($entry.Key)){throw "Unreviewed message $($entry.Key)"}
  foreach($field in $entry.Value.GetEnumerator()){if(!$gold[$entry.Key].ContainsKey($field.Key)-or $gold[$entry.Key][$field.Key] -ne $field.Value){throw "Incompatible field $($entry.Key).$($field.Key)"}}
  foreach($field in $gold[$entry.Key].Keys){if(!$entry.Value.ContainsKey($field)){throw "Removed field needs reserved declaration review: $($entry.Key).$field"}}
 }
}
foreach($name in $gold.Keys){if(!$seen.ContainsKey($name)){throw "Missing canonical message $name"}}
# Negative regression: incompatible field-number reuse is demonstrably detected.
$bad=Read-Schema 'message Waypoint { string latitude_deg = 1; double longitude_deg = 2; }'
if($bad.Waypoint['1'] -eq $gold.Waypoint['1']){throw 'Mutation regression failed'}
"PASS canonical schema baseline; incompatible reuse detected"
