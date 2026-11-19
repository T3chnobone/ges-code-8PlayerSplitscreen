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

!define MOD_ZIP_NAME "GES-SplitScreenMod.zip"
!ifndef MOD_ZIP_URL   ; a test build can point this at a local server
  !define MOD_ZIP_URL "https://github.com/${REPO_URL_PATH}/releases/latest/download/${MOD_ZIP_NAME}"
!endif
!ifdef WEB
  ; Web setup: always installs the newest release, so its name has no version
  Name "${PRODUCT}"
  OutFile "GES-SplitScreenMod-Setup.exe"
!else
  Name "${PRODUCT} ${VERSION}"
  OutFile "GES-SplitScreenMod-Setup-${VERSION}.exe"
!endif
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES32\Steam\steamapps\sourcemods\gesource"
BrandingText "${PRODUCT}"

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
!define MUI_WELCOMEPAGE_TITLE "${PRODUCT}"
!ifdef WEB
  !define WELCOME_EXTRA "Setup downloads the newest version of the mod from GitHub, so you need an internet connection.$\r$\n$\r$\n"
!else
  !define WELCOME_EXTRA ""
!endif
!define MUI_WELCOMEPAGE_TEXT "This adds local split screen for up to 8 players with controllers to GoldenEye: Source, plus Big Head, Paintball, aim assist and the other new match options.$\r$\n$\r$\nYou need Steam, Source SDK Base 2013 Multiplayer (free on Steam) and GoldenEye: Source 5.0.6 installed.$\r$\n$\r$\n${WELCOME_EXTRA}The files this replaces are backed up first, and the uninstaller puts them back.$\r$\n$\r$\nPlease quit GoldenEye: Source before continuing."
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

; Where setup looks for Steam and Source SDK Base 2013 Multiplayer. A test build can point
; these at keys that don't exist to try the "missing" paths:
;   makensis -DPREREQ_TEST -DSTEAM_REGKEY=Software\GESTest\NoSteam ...
!ifndef STEAM_REGKEY
  !define STEAM_REGKEY "Software\Valve\Steam"
!endif
!ifndef SDK_REGKEY
  !define SDK_REGKEY "Software\Valve\Steam\Apps\243750"
!endif
!define GES_DOWNLOAD_URL "https://www.geshl2.com/download/"
; Answers a silent run gives (a test build can change them)
!ifndef SD_OPEN
  !define SD_OPEN IDNO
!endif
!ifndef SD_SDK_INSTALL
  !define SD_SDK_INSTALL IDNO
!endif
!ifndef SD_SDK_WAIT
  !define SD_SDK_WAIT IDCANCEL
!endif

; Open a link (a test build logs it instead).
!macro OpenLink URL
  !ifdef PREREQ_TEST
    !insertmacro PrereqLog "open ${URL}"
  !else
    ExecShell "open" "${URL}"
  !endif
!macroend
!macro PrereqLog TEXT
  !ifdef PREREQ_TEST
    FileOpen $8 "$EXEDIR\prereq_test.log" a
    FileSeek $8 0 END
    FileWrite $8 "${TEXT}$\r$\n"
    FileClose $8
  !endif
!macroend

Function .onInit
  ; Steam keeps the source mods folder in the user's registry.
  ReadRegStr $0 HKCU "${STEAM_REGKEY}" "SourceModInstallPath"
  ${If} $0 != ""
  ${AndIf} ${FileExists} "$0\gesource\gameinfo.txt"
    StrCpy $INSTDIR "$0\gesource"
  ${Else}
    ReadRegStr $SteamPath HKCU "${STEAM_REGKEY}" "SteamPath"
    ${If} ${FileExists} "$SteamPath\steamapps\sourcemods\gesource\gameinfo.txt"
      StrCpy $INSTDIR "$SteamPath\steamapps\sourcemods\gesource"
    ${EndIf}
  ${EndIf}
  !ifdef PREREQ_TEST_NO_GES
    StrCpy $INSTDIR "$TEMP\GESTest\no_gesource_here"
  !endif
  !ifdef PREREQ_TEST
    ; Test build: run the checks straight away (answers come from /SD when run with /S)
    Call CheckPrereqs
    !insertmacro PrereqLog "checks passed, setup carries on"
    Quit
  !endif
  ; A silent install can't ask for the folder, so it needs GoldenEye: Source where Steam keeps it
  ${If} ${Silent}
  ${AndIfNot} ${FileExists} "$INSTDIR\gameinfo.txt"
    SetErrorLevel 2
    Quit
  ${EndIf}
FunctionEnd

; Everything the mod needs: Steam, Source SDK Base 2013 Multiplayer, GoldenEye: Source 5.0.6.
; (The mod's DLLs use the static C runtime, so no Visual C++ redistributable is needed.)
Function CheckPrereqs
  ReadRegStr $SteamPath HKCU "${STEAM_REGKEY}" "SteamPath"
  ${If} $SteamPath == ""
    !insertmacro PrereqLog "steam: missing"
    MessageBox MB_YESNO|MB_ICONEXCLAMATION "Steam isn't installed. GoldenEye: Source runs through Steam, so install Steam, sign in, then run this setup again.$\r$\n$\r$\nOpen the Steam download page now?" /SD ${SD_OPEN} IDNO steam_quit
      !insertmacro OpenLink "https://store.steampowered.com/about/"
    steam_quit:
    !insertmacro PrereqLog "steam: setup quits"
    Quit
  ${EndIf}
  !insertmacro PrereqLog "steam: found"

  ReadRegDWORD $0 HKCU "${SDK_REGKEY}" "Installed"
  ${If} $0 != 1
    !insertmacro PrereqLog "sdk: missing"
    MessageBox MB_YESNO|MB_ICONQUESTION "GoldenEye: Source needs Source SDK Base 2013 Multiplayer (free on Steam), and it isn't installed yet.$\r$\n$\r$\nInstall it through Steam now?" /SD ${SD_SDK_INSTALL} IDNO sdk_done
      !insertmacro OpenLink "steam://install/243750"
      ; Wait for Steam: it marks the app installed when the download finishes
      sdk_wait:
        MessageBox MB_RETRYCANCEL|MB_ICONINFORMATION "Steam is downloading Source SDK Base 2013 Multiplayer.$\r$\n$\r$\nClick Retry when Steam shows it as installed, or Cancel to carry on anyway (the game won't start until it's installed)." /SD ${SD_SDK_WAIT} IDCANCEL sdk_done
        !ifdef PREREQ_TEST
          Sleep 1000
        !endif
        ReadRegDWORD $0 HKCU "${SDK_REGKEY}" "Installed"
        !insertmacro PrereqLog "sdk: retry, installed=$0"
        StrCmp $0 "1" sdk_done sdk_wait
    sdk_done:
    !insertmacro PrereqLog "sdk: carrying on (installed=$0)"
  ${Else}
    !insertmacro PrereqLog "sdk: found"
  ${EndIf}

  ; GoldenEye: Source itself is only offered on ModDB (and by BitTorrent), neither of
  ; which can be downloaded without a browser, so open the official download page.
  ${IfNot} ${FileExists} "$INSTDIR\gameinfo.txt"
    !insertmacro PrereqLog "ges: missing"
    MessageBox MB_YESNO|MB_ICONEXCLAMATION "Setup couldn't find GoldenEye: Source 5.0.6 in your Steam sourcemods folder.$\r$\n$\r$\nIt's a free download from the GoldenEye: Source website. Install it, restart Steam, then run this setup again. If it's installed somewhere else, choose No and pick its folder on the next pages.$\r$\n$\r$\nOpen the GoldenEye: Source download page now?" /SD ${SD_OPEN} IDNO ges_done
      !insertmacro OpenLink "${GES_DOWNLOAD_URL}"
      !insertmacro PrereqLog "ges: setup quits"
      Quit
    ges_done:
    !insertmacro PrereqLog "ges: carrying on to pick the folder"
  ${Else}
    !insertmacro PrereqLog "ges: found at $INSTDIR"
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
; doesn't back up the mod's own copy as if it were an original.  $R1 = path relative to gesource.
Function BackupRel
  ${If} ${FileExists} "$INSTDIR\${BACKUP_DIR}\_added\$R1"
    ; added by an earlier install of the mod: nothing to back up
  ${ElseIf} ${FileExists} "$INSTDIR\$R1"
    ${IfNot} ${FileExists} "$INSTDIR\${BACKUP_DIR}\$R1"
      ${GetParent} "$INSTDIR\${BACKUP_DIR}\$R1" $1
      CreateDirectory "$1"
      CopyFiles /SILENT "$INSTDIR\$R1" "$INSTDIR\${BACKUP_DIR}\$R1"
    ${EndIf}
  ${Else}
    FileOpen $9 "$INSTDIR\${BACKUP_DIR}\new_files.txt" a
    FileSeek $9 0 END
    FileWrite $9 "$R1$\r$\n"
    FileClose $9
    ${GetParent} "$INSTDIR\${BACKUP_DIR}\_added\$R1" $1
    CreateDirectory "$1"
    FileOpen $9 "$INSTDIR\${BACKUP_DIR}\_added\$R1" w
    FileClose $9
  ${EndIf}
FunctionEnd
!macro BackupFile REL
  StrCpy $R1 "${REL}"
  Call BackupRel
!macroend

!ifdef WEB
Var ModVersion
; Download the newest mod zip from the latest GitHub release (or use a zip next to the exe),
; back up what it replaces and copy it in.
Function WebInstall
  InitPluginsDir
  File "/oname=$PLUGINSDIR\ges_fetch.ps1" "ges_fetch.ps1"
  retry:
  DetailPrint "Getting the newest Split Screen Mod files from GitHub..."
  nsExec::ExecToLog 'powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\ges_fetch.ps1" -Url "${MOD_ZIP_URL}" -Local "$EXEDIR\${MOD_ZIP_NAME}" -Out "$PLUGINSDIR\mod"'
  Pop $0
  ${If} $0 != 0
    MessageBox MB_ABORTRETRYIGNORE|MB_ICONEXCLAMATION "Setup couldn't get the Split Screen Mod files (error $0). Check your internet connection and click Retry.$\r$\n$\r$\nTo install without internet, download ${MOD_ZIP_NAME} from the Releases page and put it next to this setup. Click Ignore to open the Releases page." /SD IDABORT IDRETRY retry IDIGNORE openpage
    Abort "Couldn't get the mod files."
    openpage:
    ExecShell "open" "https://github.com/${REPO_URL_PATH}/releases/latest"
    Abort "Couldn't get the mod files."
  ${EndIf}
  StrCpy $ModVersion ""
  ${If} ${FileExists} "$PLUGINSDIR\mod\splitscreen_version.txt"
    FileOpen $9 "$PLUGINSDIR\mod\splitscreen_version.txt" r
    FileRead $9 $ModVersion
    FileClose $9
    Push $ModVersion
    Call TrimNewline
    Pop $ModVersion
  ${EndIf}
  DetailPrint "Installing Split Screen Mod $ModVersion"
  ; Back up every file the download will replace
  FileOpen $8 "$PLUGINSDIR\filelist.txt" r
  FileRead $8 $R1
  bk_loop:
    StrCmp $R1 "" bk_done
    Push $R1
    Call TrimNewline
    Pop $R1
    StrCmp $R1 "" +2
    Call BackupRel
    FileRead $8 $R1
    Goto bk_loop
  bk_done:
  FileClose $8
  CopyFiles /SILENT "$PLUGINSDIR\mod\*.*" "$INSTDIR"
FunctionEnd

Function TrimNewline
  Exch $0
  Push $1
  loop:
    StrCpy $1 $0 1 -1
    StrCmp $1 "$\r" trim
    StrCmp $1 "$\n" trim
    Goto out
  trim:
    StrCpy $0 $0 -1
    Goto loop
  out:
  Pop $1
  Exch $0
FunctionEnd
!endif

Section "Split Screen Mod (required)" SecCore
  SectionIn RO
  CreateDirectory "$INSTDIR\${BACKUP_DIR}"
!ifdef WEB
  Call WebInstall
!else
  !include "payload_backup.nsh"   ; generated: one BackupFile line per payload file

  SetOutPath "$INSTDIR"
  SetOverwrite on
  File /r "payload\*.*"
!endif


  SetOutPath "$INSTDIR\${BACKUP_DIR}"
  File /oname=SPLITSCREEN_README.md "payload_docs\INSTALL_SPLITSCREEN.md"
  WriteUninstaller "$INSTDIR\${BACKUP_DIR}\Uninstall Split Screen Mod.exe"

  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${PRODUCT}"
!ifdef WEB
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "$ModVersion"
!else
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
!endif
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
