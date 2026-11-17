  SetShellVarContext current
  Delete "$DESKTOP\${GAME_TITLE_FILE}.lnk"
  RMDir /r "$SMPROGRAMS\${GAME_TITLE_FILE}"
