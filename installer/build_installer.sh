#!/bin/sh
# Builds everything a release needs. Put the built DLLs in payload/bin first:
#   client.dll server.dll SDL2.dll shadereditor.dll game_shader_generic_procshader.dll
# Usage: ./build_installer.sh 0.2.0 ["GoldenEye: Source - Split Screen Mod"]
# Output (attach all of them to the GitHub release):
#   GES-SplitScreenMod.zip         the mod files; the web setup downloads this from the latest release
#   GES-SplitScreenMod.zip.sha256  its checksum, checked by the web setup
#   GES-SplitScreenMod-Setup.exe   web setup: small, always installs the newest release
#   GES-SplitScreenMod-Setup-<version>.exe  offline setup with this version's files inside
set -e
cd "$(dirname "$0")"
VERSION="${1:-0.3.0}"; TITLE="${2:-GoldenEye: Source - Split Screen Mod}"
for f in client.dll server.dll SDL2.dll shadereditor.dll game_shader_generic_procshader.dll; do
  [ -f "payload/bin/$f" ] || { echo "missing payload/bin/$f"; exit 1; }
done
cp -r ../splitscreen_resources/materials ../splitscreen_resources/resource payload/
cp ../splitscreen_resources/gameinfo.txt ../splitscreen_resources/steam.inf payload/
printf '%s\r\n' "$VERSION" > payload/splitscreen_version.txt
mkdir -p payload_docs
cp ../INSTALL_SPLITSCREEN.md payload_docs/
cp ../LICENSE_GES payload_docs/LICENSE_GES.txt
( cd payload && find . -type f | sed 's|^\./||' | sort | while read -r f; do
    printf '!insertmacro BackupFile "%s"\n' "$(echo "$f" | tr / '\\\\')"; done ) > payload_backup.nsh
[ -f extras.nsh ] || : > extras.nsh
[ -f extras_uninstall.nsh ] || : > extras_uninstall.nsh
# The mod zip (Windows paths inside, as Expand-Archive expects)
rm -f GES-SplitScreenMod.zip
if command -v python3 >/dev/null 2>&1 && python3 -c 1 >/dev/null 2>&1; then
python3 -I - <<'PY'
import os, zipfile
with zipfile.ZipFile('GES-SplitScreenMod.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk('payload'):
        for f in sorted(files):
            p = os.path.join(root, f)
            z.write(p, os.path.relpath(p, 'payload'))
PY
else
  # No Python (a plain Windows + Git Bash setup): build the same zip with .NET, '/' in the paths
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $root = (Resolve-Path "payload").Path.TrimEnd("\") + "\"
    $zip = [IO.Compression.ZipFile]::Open((Join-Path (Get-Location) "GES-SplitScreenMod.zip"), "Create")
    try {
      Get-ChildItem -LiteralPath $root -Recurse -File | Sort-Object FullName | ForEach-Object {
        $name = $_.FullName.Substring($root.Length).Replace("\", "/")
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $name, "Optimal")
      }
    } finally { $zip.Dispose() }'
fi
sha256sum GES-SplitScreenMod.zip | awk '{print $1 "  GES-SplitScreenMod.zip"}' > GES-SplitScreenMod.zip.sha256
makensis -V2 -DVERSION="$VERSION" -DGAME_TITLE="$TITLE" ges-splitscreen-setup.nsi
makensis -V2 -DWEB -DVERSION="$VERSION" -DGAME_TITLE="$TITLE" ges-splitscreen-setup.nsi
ls -l GES-SplitScreenMod*
