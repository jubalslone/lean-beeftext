param(
	[Parameter(Mandatory = $true)]
	[string]$Path
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
	throw "Required emoji runtime asset is missing: $Path"
}
$data = Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json -AsHashtable
if ($data -isnot [System.Collections.IDictionary] -or $data.Count -eq 0) {
	throw "Emoji runtime data must be a non-empty JSON object: $Path"
}
foreach ($key in $data.Keys) {
	$entry = $data[$key]
	if ([string]::IsNullOrEmpty($key) -or $entry -isnot [System.Collections.IDictionary] -or
		$entry['char'] -isnot [string] -or [string]::IsNullOrEmpty($entry['char'])) {
		throw "Invalid emoji runtime entry: $key"
	}
}
if ($data['grinning']['char'] -cne [char]::ConvertFromUtf32(0x1F600)) {
	throw 'Emoji runtime acceptance check failed: grinning must resolve to U+1F600.'
}
$file = Get-Item -LiteralPath $Path
$hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Output "Emoji runtime data validated: $($file.Length) bytes; $($data.Count) entries; SHA256 $hash"
