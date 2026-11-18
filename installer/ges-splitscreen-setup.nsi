; GoldenEye: Source Split Screen Mod - setup wizard (NSIS 3, Unicode)
;
; Build:  makensis -DVERSION=0.1.0 -DGAME_TITLE="GoldenEye: Source - Split Screen Mod" ges-splitscreen-setup.nsi
; Expects the files to install in .\payload\ laid out like the gesource folder
; (bin\client.dll, bin\server.dll, ..., resource\*.res, materials\..., gameinfo.txt, steam.inf).

Unicode true
ManifestDPIAware true
SetCompressor /SOLID lzma

!ifndef VERSION
  !define VERSION "0.1.0"
!endif
!ifndef GAME_TITLE
  !define GAME_TITLE "GoldenEye: Source - Split Screen Mod"
!endif
!define PRODUCT "GoldenEye: Source - Split Screen Mod"
!define GAME_TITLE_FILE "GoldenEye Source - Split Screen Mod"
!ifndef REPO_URL_PATH
  !define REPO_URL_PATH "T3chnobone/ges-code-8PlayerSplitscreen"
!endif
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\GESSplitScreenMod"
!define BACKUP_DIR "splitscreen_backup"

Name "${PRODUCT} ${VERSION}"
OutFile "GES-SplitScreenMod-Setup-${VERSION}.exe"
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES32\Steam\steamapps\sourcemods\gesource"
BrandingText "${PRODUCT} ${VERSION}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "FileDescription" "${PRODUCT} setup"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "GoldenEye: Source by Team GoldenEye: Source. Unofficial split screen build."

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "WordFunc.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_WELCOMEPAGE_TITLE "${PRODUCT} ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "This adds local split screen for up to 8 players with controllers to GoldenEye: Source, plus Big Head, Paintball, aim assist and the other new match options.$\r$\n$\r$\nYou need Steam, Source SDK Base 2013 Multiplayer (free on Steam) and GoldenEye: Source 5.0.6 installed.$\r$\n$\r$\nThe files this replaces are backed up first, and the uninstaller puts them back.$\r$\n$\r$\nPlease quit GoldenEye: Source before continuing."
!define MUI_DIRECTORYPAGE_TEXT_TOP "Setup found this GoldenEye: Source folder. If yours is somewhere else, pick the gesource folder that holds gameinfo.txt."
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "GoldenEye: Source folder (gesource)"
!define MUI_FINISHPAGE_SHOWREADME "https://github.com/${REPO_URL_PATH}/blob/ges-splitscreen/INSTALL_SPLITSCREEN.md#controller-quick-start"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Open the controller quick start guide"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_TEXT "${PRODUCT} is installed. Restart Steam so the library shows the new name, then start ${GAME_TITLE} from your Steam library."

!define MUI_PAGE_CUSTOMFUNCTION_LEAVE CheckPrereqs
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "payload_docs\LICENSE_GES.txt"
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Var SteamPath

Function .onInit
  ; Steam keeps the source mods folder in the user's registry.
  ReadRegStr $0 HKCU "Software\Valve\Steam" "SourceModInstallPath"
  ${If} $0 != ""
  ${AndIf} ${FileExists} "$0\gesource\gameinfo.txt"
    StrCpy $INSTDIR "$0\gesource"
  ${Else}
    ReadRegStr $SteamPath HKCU "Software\Valve\Steam" "SteamPath"
    ${If} ${FileExists} "$SteamPath\steamapps\sourcemods\gesource\gameinfo.txt"
      StrCpy $INSTDIR "$SteamPath\steamapps\sourcemods\gesource"
    ${EndIf}
  ${EndIf}
FunctionEnd

; Everything the mod needs: Steam, Source SDK Base 2013 Multiplayer, GoldenEye: Source 5.0.6.
Function CheckPrereqs
  ReadRegStr $SteamPath HKCU "Software\Valve\Steam" "SteamPath"
  ${If} $SteamPath == ""
    MessageBox MB_YESNO|MB_ICONEXCLAMATION "Steam isn't installed. GoldenEye: Source runs through Steam, so install Steam, sign in, then run this setup again.$\r$\n$\r$\nOpen the Steam download page now?" IDNO +2
      ExecShell "open" "https://store.steampowered.com/about/"
    Quit
  ${EndIf}
  ReadRegDWORD $0 HKCU "Software\Valve\Steam\Apps\243750" "Installed"
  ${If} $0 != 1
    MessageBox MB_YESNO|MB_ICONQUESTION "GoldenEye: Source needs Source SDK Base 2013 Multiplayer (free on Steam), and it isn't installed yet.$\r$\n$\r$\nInstall it through Steam now? Steam will download it while this setup carries on." IDNO +2
      ExecShell "open" "steam://install/243750"
  ${EndIf}
  ${IfNot} ${FileExists} "$INSTDIR\gameinfo.txt"
    MessageBox MB_YESNO|MB_ICONEXCLAMATION "Setup couldn't find GoldenEye: Source 5.0.6 in your Steam sourcemods folder.$\r$\n$\r$\nIt's a free download from the GoldenEye: Source website. Install it, restart Steam, then run this setup again. If it's installed somewhere else, choose No and pick its folder on the next pages.$\r$\n$\r$\nOpen the GoldenEye: Source download page now?" IDNO +3
      ExecShell "open" "https://www.geshl2.com/download/"
      Quit
  ${EndIf}
FunctionEnd

Function DirLeave
  ${IfNot} ${FileExists} "$INSTDIR\gameinfo.txt"
    MessageBox MB_ICONEXCLAMATION|MB_OK "There's no gameinfo.txt in this folder. Pick the gesource folder of your GoldenEye: Source install (usually Steam\steamapps\sourcemods\gesource)."
    Abort
  ${EndIf}
  FindWindow $0 "Valve001"
  ${If} $0 != 0
    MessageBox MB_ICONEXCLAMATION|MB_OK "A Source game is running. Quit it, then click Next."
    Abort
  ${EndIf}
FunctionEnd

; Back up a file once (first install keeps the original), or note that it was new.
; A file the mod added is marked under _added\, so installing again over the mod
; doesn't back up the mod's own copy as if it were an original.
!macro BackupFile REL
  ${If} ${FileExists} "$INSTDIR\${BACKUP_DIR}\_added\${REL}"
    ; added by an earlier install of the mod: nothing to back up
  ${ElseIf} ${FileExists} "$INSTDIR\${REL}"
    ${IfNot} ${FileExists} "$INSTDIR\${BACKUP_DIR}\${REL}"
      ${GetParent} "$INSTDIR\${BACKUP_DIR}\${REL}" $1
      CreateDirectory "$1"
      CopyFiles /SILENT "$INSTDIR\${REL}" "$INSTDIR\${BACKUP_DIR}\${REL}"
    ${EndIf}
  ${Else}
    FileOpen $9 "$INSTDIR\${BACKUP_DIR}\new_files.txt" a
    FileSeek $9 0 END
    FileWrite $9 "${REL}$\r$\n"
    FileClose $9
    ${GetParent} "$INSTDIR\${BACKUP_DIR}\_added\${REL}" $1
    CreateDirectory "$1"
    FileOpen $9 "$INSTDIR\${BACKUP_DIR}\_added\${REL}" w
    FileClose $9
  ${EndIf}
!macroend

Section "Split Screen Mod (required)" SecCore
  SectionIn RO
  CreateDirectory "$INSTDIR\${BACKUP_DIR}"
  !include "payload_backup.nsh"   ; generated: one BackupFile line per payload file

  SetOutPath "$INSTDIR"
  SetOverwrite on
  File /r "payload\*.*"


  SetOutPath "$INSTDIR\${BACKUP_DIR}"
  File /oname=SPLITSCREEN_README.md "payload_docs\INSTALL_SPLITSCREEN.md"
  WriteUninstaller "$INSTDIR\${BACKUP_DIR}\Uninstall Split Screen Mod.exe"

  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "T3chnobone"
  WriteRegStr HKLM "${UNINST_KEY}" "URLInfoAbout" "https://github.com/${REPO_URL_PATH}"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\${BACKUP_DIR}\Uninstall Split Screen Mod.exe"'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

!include "extras.nsh"   ; optional components (shortcut, Vulkan, ...), chosen with the user

Section "Uninstall"
  FindWindow $0 "Valve001"
  ${If} $0 != 0
    MessageBox MB_ICONEXCLAMATION|MB_OK "Quit GoldenEye: Source first, then run the uninstaller again."
    Abort
  ${EndIf}
  ; $INSTDIR is the backup folder here
  ${GetParent} "$INSTDIR" $R0
  ; Delete files the mod added
  ${If} ${FileExists} "$INSTDIR\new_files.txt"
    FileOpen $9 "$INSTDIR\new_files.txt" r
    loop:
      FileRead $9 $1
      StrCmp $1 "" done
      Push $1
      Call un.TrimNewline
      Pop $1
      Delete "$R0\$1"
      Goto loop
    done:
    FileClose $9
  ${EndIf}
  ; Put the originals back
  RMDir /r "$INSTDIR\_added"
  CopyFiles /SILENT "$INSTDIR\*.*" "$R0"
  Delete "$R0\new_files.txt"
  Delete "$R0\SPLITSCREEN_README.md"
  Delete "$R0\Uninstall Split Screen Mod.exe"
  !include "extras_uninstall.nsh"
  SetOutPath "$TEMP"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd

Function un.TrimNewline
  Exch $0
  loop:
    StrCpy $1 $0 1 -1
    StrCmp $1 "$\r" trim
    StrCmp $1 "$\n" trim
    Goto out
  trim:
    StrCpy $0 $0 -1
    Goto loop
  out:
  Exch $0
FunctionEnd
