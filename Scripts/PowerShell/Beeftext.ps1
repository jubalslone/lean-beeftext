# This script contain useful PowerShell functions for Beeftext
#
# Author: Xavier Michelon
#
# Copyright (c) Xavier Michelon. All rights reserved.
# Licensed under the MIT License. See LICENSE file in the project root for full license information.


. (Join-Path $PSScriptRoot "..\..\Submodules\XMiLib\Scripts\PowerShell\Utils.ps1")

#***********************************************************************************************************************
# useful variable declarations
#***********************************************************************************************************************
$solutionDir = absolutePath $PSScriptRoot "..\.."
$rcFilePath = absolutePath $solutionDir "Beeftext\Beeftext.rc"


#***********************************************************************************************************************
# Retrieve Lean Beeftext's three-part public product version.
#***********************************************************************************************************************
function getLeanBeeftextProductVersion
{
   $srcFile = absolutePath $solutionDir "Beeftext\BeeftextConstants.cpp"
   $major, $minor, $patch = Select-String -Path $srcFile -Pattern 'kProductVersion\s*=\s*"(\d+)\.(\d+)\.(\d+)";' |
      ForEach-Object {$_.Matches} | ForEach-Object { $_.Groups[1].Value, $_.Groups[2].Value, $_.Groups[3].Value }
   if ([string]::IsNullOrEmpty($major) -Or [string]::IsNullOrEmpty($minor) -Or [string]::IsNullOrEmpty($patch))
   {
      Write-Error "Could not parse the Lean Beeftext product version"
   }
   @($major, $minor, $patch)
}


#***********************************************************************************************************************
# Update the version number in the RC file
#***********************************************************************************************************************
function updateVersionNumberInRcFile
{
    $major, $minor, $patch = getLeanBeeftextProductVersion
    (Get-Content $rcFilePath) `
        -replace '^(#define\s+VERSION_NUMBER)\s+\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*$', ('$1 ' + $major + ',' + $minor + ',' + $patch + ',0') `
        -replace '^(#define\s+VERSION_STRING)\s+"\d+\.\d+(?:\.\d+)?\\0"\s*$', ('$1 "' + $major + '.' + $minor + '.' + $patch + '\0"') |
        Out-File $rcFilePath -Encoding "ASCII"
        # Note we use ASCII for output  as UTF8 would cause the insertion of a 3-bytes BOM at the beginning of the file
        # that will screw the RC file for VS.
}
