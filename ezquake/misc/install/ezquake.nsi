; example2.nsi
;
; This script is based on example1.nsi, but it remember the directory, 
; has uninstall support and (optionally) installs start menu shortcuts.
;
; It will install makensisw.exe into a directory that the user selects,

;--------------------------------

; The name of the installer
Name "ezQuake"

; The file to write
OutFile "ezQuake-installer.exe"

; The default installation directory
InstallDir $PROGRAMFILES\Quake

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\ezQuake" "Install_dir"

LicenseData licdata.txt

;--------------------------------

; Pages

Page license
Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "ezQuake client (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put file there
  File "ezquake-gl.exe"
  File "ezquake.exe"
  File "ezstart.exe"

  CreateDirectory $INSTDIR\ezquake
  SetOutPath $INSTDIR\ezquake
  File "ezquake\pak0.pak"

  CreateDirectory $INSTDIR\ezquake\cfg
  SetOutPath $INSTDIR\ezquake\cfg
  File "ezquake\cfg\gfx_*.cfg"  

  SetOutPath $SYSDIR
  File "lib\libjpeg.dll"
  File "lib\libpng.dll"
  File "lib\zlib.dll"

  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\ezQuake "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "DisplayName" "NSIS Example2"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "NoRepair" 1
  WriteUninstaller "uninstall.exe"

SectionEnd

Section "Extra Logitech Mice Support"
  SetOutPath $INSTDIR
  File "lib\mw_hook.dll"
SectionEnd

Section "Example configs"
  SetOutPath $INSTDIR\ezquake\cfg
  File "ezquake\cfg\*.cfg"
SectionEnd

Section "ezQuake Manual"
  CreateDirectory $INSTDIR\ezquake\manual
  SetOutPath $INSTDIR\ezquake\manual
  File /r "ezquake\manual\*.*"
SectionEnd

Section "Inbuilt QuakeWorld Guide"
  SetOutPath $SYSDIR
  File "lib\libexpat.dll"
  CreateDirectory $INSTDIR\ezquake\help
  CreateDirectory $INSTDIR\ezquake\help\manual
  CreateDirectory $INSTDIR\ezquake\help\xsd
  CreateDirectory $INSTDIR\ezquake\help\xsl
  SetOutPath $INSTDIR\ezquake\help\manual
  File /r "ezquake\help\manual\*.*"
  SetOutPath $INSTDIR\ezquake\help\xsd
  File "ezquake\help\xsd\*.*"
  SetOutPath $INSTDIR\ezquake\help\xsl
  File "ezquake\help\xsl\*.*"
SectionEnd

Section "Inbuilt Variables and Commands manual"
  SetOutPath $SYSDIR
  File "lib\libexpat.dll"
  CreateDirectory $INSTDIR\ezquake\help
  CreateDirectory $INSTDIR\ezquake\help\variables
  CreateDirectory $INSTDIR\ezquake\help\commands
  CreateDirectory $INSTDIR\ezquake\help\xsd
  CreateDirectory $INSTDIR\ezquake\help\xsl
  SetOutPath $INSTDIR\ezquake\help\variables
  File "ezquake\help\variables\*.*"
  SetOutPath $INSTDIR\ezquake\help\commands
  File "ezquake\help\commands\*.*"
  SetOutPath $INSTDIR\ezquake\help\xsd
  File "ezquake\help\xsd\*.*"
  SetOutPath $INSTDIR\ezquake\help\xsl
  File "ezquake\help\xsl\*.*"
SectionEnd

Section "Example Keymaps"
  CreateDirectory $INSTDIR\ezquake\keymap
  SetOutPath $INSTDIR\ezquake\keymap
  File "ezquake\keymaps\*.*"
SectionEnd

Section "Server Browser Source Servers List"
SectionSetText 4 "This client allows you to change standard keyboard layout. See keymap manual for more info."
  CreateDirectory $INSTDIR\ezquake\sb
  SetOutPath $INSTDIR\ezquake\sb
  File "ezquake\sb\sources.txt"
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"
  SetOutPath $INSTDIR
  CreateDirectory "$SMPROGRAMS\ezQuake"
  CreateShortCut "$SMPROGRAMS\ezQuake\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\ezQuake\ezQuake OpenGL.lnk" "$INSTDIR\ezquake-gl.exe" "" "$INSTDIR\ezquake-gl.exe" 0
  CreateShortCut "$SMPROGRAMS\ezQuake\ezQuake Software.lnk" "$INSTDIR\ezquake.exe" "" "$INSTDIR\ezquake.exe" 0
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake"
  DeleteRegKey HKLM SOFTWARE\ezQuake

  ; Remove files and uninstaller
  Delete $INSTDIR\ezquake.exe
  Delete $INSTDIR\ezquake-gl.exe

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\ezQuake\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\ezQuake"

  ; Don't remove install dir because it's Quake root dir!

SectionEnd