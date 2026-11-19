#!/bin/sh
# Builds the setup exe. Put the built DLLs in payload/bin first:
#   client.dll server.dll SDL2.dll shadereditor.dll game_shader_generic_procshader.dll
# Usage: ./build_installer.sh 1.0.0 "GoldenEye: Source - Split Screen Mod"
set -e
cd "$(dirname "$0")"
VERSION="${1:-1.0.0}"; TITLE="${2:-GoldenEye: Source - Split Screen Mod}"
for f in client.dll server.dll SDL2.dll shadereditor.dll game_shader_generic_procshader.dll; do
  [ -f "payload/bin/$f" ] || { echo "missing payload/bin/$f"; exit 1; }
done
cp -r ../splitscreen_resources/materials ../splitscreen_resources/resource payload/
cp ../splitscreen_resources/gameinfo.txt ../splitscreen_resources/steam.inf payload/
mkdir -p payload_docs
cp ../INSTALL_SPLITSCREEN.md payload_docs/
cp ../LICENSE_GES payload_docs/LICENSE_GES.txt
( cd payload && find . -type f | sed 's|^\./||' | sort | while read -r f; do
    printf '!insertmacro BackupFile "%s"\n' "$(echo "$f" | tr / '\\\\')"; done ) > payload_backup.nsh
[ -f extras.nsh ] || : > extras.nsh
[ -f extras_uninstall.nsh ] || : > extras_uninstall.nsh
makensis -V2 -DVERSION="$VERSION" -DGAME_TITLE="$TITLE" ges-splitscreen-setup.nsi
