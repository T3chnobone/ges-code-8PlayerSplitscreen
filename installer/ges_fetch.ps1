# Used by the web setup: gets the newest mod files and unpacks them for the installer.
#   -Url    the zip on the latest GitHub release
#   -Local  a zip next to the setup exe (offline install); used instead of downloading when present
#   -Out    folder to unpack into; writes filelist.txt (one relative path per line) next to it
param([string]$Url, [string]$Local, [string]$Out)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
try {
  [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
  $parent = Split-Path -Parent $Out
  $zip = Join-Path $parent 'mod.zip'
  if ($Local -and (Test-Path -LiteralPath $Local)) {
    Write-Output "Using $Local"
    Copy-Item -LiteralPath $Local -Destination $zip -Force
  } else {
    Write-Output "Downloading $Url"
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $zip
    # Check it against the published SHA-256 when the release has one
    try {
      $sum = (Invoke-WebRequest -UseBasicParsing -Uri "$Url.sha256").Content
      if ($sum -is [byte[]]) { $sum = [Text.Encoding]::ASCII.GetString($sum) }
      $want = ($sum.Trim() -split '\s+')[0].ToLower()
      $have = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLower()
      if ($want -and $want -ne $have) { Write-Output "Checksum mismatch"; exit 3 }
      Write-Output "Checksum OK"
    } catch [System.Net.WebException] { Write-Output "No checksum published" }
  }
  if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Recurse -Force }
  Expand-Archive -LiteralPath $zip -DestinationPath $Out -Force
  if (-not (Test-Path -LiteralPath (Join-Path $Out 'bin\client.dll'))) { Write-Output "The download doesn't contain bin\client.dll"; exit 4 }
  # Paths relative to $Out. .NET keeps the folder exactly as passed in; Get-ChildItem would
  # expand a short (8.3) temp path like C:\Users\ABCDEF~1 to the long name, and cutting a
  # fixed length off those paths gave wrong names (nothing got backed up).
  $root = $Out.TrimEnd('\')
  [IO.Directory]::EnumerateFiles($root, '*', [IO.SearchOption]::AllDirectories) |
    ForEach-Object { $_.Substring($root.Length + 1) } |
    Set-Content -LiteralPath (Join-Path $parent 'filelist.txt') -Encoding ASCII
  exit 0
} catch {
  Write-Output $_.Exception.Message
  exit 2
}
