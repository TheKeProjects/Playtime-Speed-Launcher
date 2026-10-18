# Writes the changelog.txt published next to version.txt (read by the update window) from the
# in-app changelog in Source\Services\App\ChangelogData.cpp: every version up to the one in
# Resources\Version.h, newest first, so launchers several versions behind see everything they missed.
#
#   4.1.1 | 18/10/2026
#   ## Additions
#   - ...
param(
    [string]$ProjectDir = (Split-Path -Parent $PSScriptRoot),
    [string]$Output = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'Playtime Speed Launcher Source Code\changelog.txt')
)

$ErrorActionPreference = 'Stop'

function Parse-Version([string]$v) {
    $parts = ($v -replace '[^0-9.]', '').Split('.') | ForEach-Object { [int]$_ }
    while ($parts.Count -lt 3) { $parts += 0 }
    return [version]::new($parts[0], $parts[1], $parts[2])
}

$versionHeader = Get-Content (Join-Path $ProjectDir 'Resources\Version.h') -Raw
$released = Parse-Version ([regex]::Match($versionHeader, 'APP_VERSION_STRING\s+"([^"]+)"').Groups[1].Value)

$source = Get-Content (Join-Path $ProjectDir 'Source\Services\App\ChangelogData.cpp') -Raw -Encoding UTF8
$lines = New-Object System.Collections.Generic.List[string]
$entryRe = [regex]'(?s)\{\s*L"([^"]+)",\s*L"([^"]*)",\s*\{(.*?)\}\s*\}'
$stringRe = [regex]'L"((?:[^"\\]|\\.)*)"'
foreach ($m in $entryRe.Matches($source)) {
    $version = $m.Groups[1].Value
    if ((Parse-Version $version) -gt $released) { continue }   # not released yet
    if ($lines.Count) { $lines.Add('') }
    $lines.Add("$version | $($m.Groups[2].Value)")
    foreach ($s in $stringRe.Matches($m.Groups[3].Value)) {
        $text = $s.Groups[1].Value -replace '\\"', '"' -replace '\\\\', '\'
        if ($text.StartsWith('## ')) {
            if ($lines[$lines.Count - 1] -notmatch '\|') { $lines.Add('') }
            $lines.Add($text)
        } else {
            $lines.Add("- $text")
        }
    }
}

[System.IO.File]::WriteAllText($Output, ($lines -join "`n") + "`n", [System.Text.UTF8Encoding]::new($false))
Write-Host "Wrote $Output (versions up to $released)"
