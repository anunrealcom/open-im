Unicode true
!include "MUI2.nsh"

Name "YiHuanIM"
OutFile "YiHuanIM-Setup.exe"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\YiHuanIM"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"

Section "Main" SEC01
  SetOutPath "$INSTDIR"
  File /r "dist\*.*"

  CreateDirectory "$SMPROGRAMS\YiHuanIM"
  CreateShortCut "$SMPROGRAMS\YiHuanIM\YiHuanIM.lnk" "$INSTDIR\YiHuanIM.exe"
  CreateShortCut "$DESKTOP\YiHuanIM.lnk" "$INSTDIR\YiHuanIM.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\YiHuanIM" "DisplayName" "YiHuanIM"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\YiHuanIM" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\YiHuanIM" "DisplayIcon" "$INSTDIR\YiHuanIM.exe"
SectionEnd

Section "Uninstall"
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\YiHuanIM\YiHuanIM.lnk"
  RMDir "$SMPROGRAMS\YiHuanIM"
  Delete "$DESKTOP\YiHuanIM.lnk"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\YiHuanIM"
SectionEnd
