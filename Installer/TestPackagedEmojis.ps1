param(
	[Parameter(Mandatory = $true)]
	[string]$Payload,
	[Parameter(Mandatory = $true)]
	[string]$TestExecutable
)
$ErrorActionPreference = 'Stop'
$payloadPath = (Resolve-Path -LiteralPath $Payload).Path
$testPath = (Resolve-Path -LiteralPath $TestExecutable).Path
& "$PSScriptRoot/TestEmojiRuntimeData.ps1" -Path (Join-Path $payloadPath 'emojis/emojis.json')

# A disposable copy tests runtime lookup beside the deployed DLLs/data. The
# test executable is never copied into the actual distribution or its manifest.
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('lean-emoji-qa-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $scratch | Out-Null
try {
	$isolated = Join-Path $scratch 'payload'
	Copy-Item -LiteralPath $payloadPath -Destination $isolated -Recurse
	$probe = Join-Path $isolated 'EmojiRuntimeTests.exe'
	Copy-Item -LiteralPath $testPath -Destination $probe
	# Do not let Qt find DLLs/plugins from the build's SDK through inherited paths.
	$savedPath = $env:PATH
	$savedPluginPath = $env:QT_PLUGIN_PATH
	try {
		$env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
		$env:QT_PLUGIN_PATH = ''
		Push-Location $scratch
		try {
			Write-Output "Package-isolated emoji acceptance: $payloadPath -> $isolated"
			& $probe --package
			if ($LASTEXITCODE -ne 0) { throw 'Packaged emoji runtime lookup/parser failed.' }

			# Even a valid source-tree decoy in cwd must not hide a missing Release asset.
			$decoy = Join-Path $scratch 'Submodules/emojilib'
			New-Item -ItemType Directory -Path $decoy -Force | Out-Null
			Move-Item -LiteralPath (Join-Path $isolated 'emojis/emojis.json') -Destination (Join-Path $decoy 'emojis.json')
			& $probe --missing-package
			if ($LASTEXITCODE -ne 0) { throw 'Release emoji lookup used the source-tree fallback.' }
		}
		finally { Pop-Location }
	}
	finally {
		$env:PATH = $savedPath
		$env:QT_PLUGIN_PATH = $savedPluginPath
	}
}
finally {
	# Only this invocation's uniquely named temporary fixture is removed.
	Remove-Item -LiteralPath $scratch -Recurse -Force
}
