; Optional parts of the install.

Var UseVulkan

Section "Desktop and Start menu shortcuts" SecShortcut
  ReadRegStr $0 HKCU "Software\Valve\Steam" "SteamExe"
  ${If} $0 == ""
    ReadRegStr $0 HKCU "Software\Valve\Steam" "SteamPath"
    StrCpy $0 "$0\steam.exe"
  ${EndIf}
  ; Steam stores these with forward slashes (c:/program files (x86)/steam/steam.exe),
  ; which a shortcut target doesn't accept.
  ${WordReplace} "$0" "/" "\" "+" $0
  ; Original renderer by default; Vulkan only if the player asks for it.
  StrCpy $UseVulkan ""
  MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "The game uses its original DirectX renderer.$\r$\n$\r$\nWould you prefer Vulkan instead? It can run faster on some PCs, especially with 3 or more players. You can change this later in the shortcut's properties (the -vulkan option).$\r$\n$\r$\nNot sure? Choose No." /SD IDNO IDNO novulkan
    StrCpy $UseVulkan " -vulkan"
  novulkan:
  SetShellVarContext current
  ${If} ${FileExists} "$INSTDIR\resource\game.ico"
    StrCpy $1 "$INSTDIR\resource\game.ico"
  ${Else}
    StrCpy $1 "$0"
  ${EndIf}
  CreateShortCut "$DESKTOP\${GAME_TITLE_FILE}.lnk" "$0" '-applaunch 243750 -game "$INSTDIR"$UseVulkan' "$1" 0
  CreateDirectory "$SMPROGRAMS\${GAME_TITLE_FILE}"
  CreateShortCut "$SMPROGRAMS\${GAME_TITLE_FILE}\${GAME_TITLE_FILE}.lnk" "$0" '-applaunch 243750 -game "$INSTDIR"$UseVulkan' "$1" 0
  CreateShortCut "$SMPROGRAMS\${GAME_TITLE_FILE}\Uninstall Split Screen Mod.lnk" "$INSTDIR\${BACKUP_DIR}\Uninstall Split Screen Mod.exe"
SectionEnd

LangString DESC_SecCore ${LANG_ENGLISH} "The split screen game files. Replaced files are backed up first."
LangString DESC_SecShortcut ${LANG_ENGLISH} "Shortcuts that start the game through Steam. Setup asks whether you'd like the original renderer (default) or Vulkan."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} $(DESC_SecCore)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcut} $(DESC_SecShortcut)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
