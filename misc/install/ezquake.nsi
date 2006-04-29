;This is NSIS Script file.
;See http://nsis.sf.net/
;
;You are required to place it into same dir with these files:
;  ezquake.nsi (this file)
;  ezquake.exe (Release)
;  ezquake-gl.exe (GLRelease)
;  ezstart.exe (CVS/ezstart/)
;  gnu.txt (GNU GENERAL PUBLIC LICENSE, Version 2, June 1991)
;  readme.txt (CVS/ezquake/misc/install/readme.txt)
;  qw/ (dir)
;    qwprogs.dat (use the one delivered with ZQuake)
;    spprogs.dat (use the one delivered with ZQuake)
;    fragfile.dat (CVS/ezquake/misc/fragfile/fragfile.dat)
;  lib/ (dir)
;    mw_hook.dll
;  ezquake/ (dir)
;    pak0.pak (eyecandy, particles)
;    cfg/ (dir, see CVS/ezquake/misc/cfg/)
;    help/ (dir, see CVS/documentation)
;    keymaps/ (dir, see CVS/ezquake/misc/keymaps/)
;    manual/ (dir, offline version of http://ezQuake.SF.net/docs/)
;    sb/ (dir, see CVS/ezquake/misc/sb)
;
;Using NSIS, files listed above and this install script you are able to create Windows installer for ezQuake.
;
;--------------------------------

; The name of the installer
Name "ezQuake"

; The file to write
OutFile "ezInstall.exe"

; The default installation directory
InstallDir "C:\Quake\"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\ezQuake" "Install_dir"

LicenseData gnu.txt

;--------------------------------

; Pages

PageEx license
   LicenseText "This will install ezQuake QuakeWorld client on your computer."
   LicenseData readme.txt
 PageExEnd

Page license
Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "ezQuake client"

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
  File "ezquake\cfg\*.cfg"

  CreateDirectory $INSTDIR\ezquake\manual
  SetOutPath $INSTDIR\ezquake\manual
  File /r "ezquake\manual\*.*"

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

  CreateDirectory $INSTDIR\ezquake\keymaps
  SetOutPath $INSTDIR\ezquake\keymaps
  File "ezquake\keymaps\*.*"

  CreateDirectory $INSTDIR\ezquake\sb
  SetOutPath $INSTDIR\ezquake\sb
  File "ezquake\sb\*.*"

  CreateDirectory $INSTDIR\qw
  SetOutPath $INSTDIR\qw
  File "qw\fragfile.dat"

  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\ezQuake "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "ezQuake" "QuakeWorld client"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "UninstallString" '"$INSTDIR\ezuninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "NoRepair" 1
  WriteUninstaller "ezuninstall.exe"

SectionEnd

Section "Modern HUD and Console font"
  CreateDirectory $INSTDIR\ezquake\textures
  CreateDirectory $INSTDIR\ezquake\textures\wad
  SetOutPath $INSTDIR\ezquake\textures\wad
  File /r "ezquake\textures\wad\*.png"
  CreateDirectory $INSTDIR\ezquake\textures\charsets
  SetOutPath $INSTDIR\ezquake\textures\charsets
  File /r "ezquake\textures\charsets\*.png"
SectionEnd  

Section "Single Player and Multiplayer progs"
  CreateDirectory $INSTDIR\qw
  SetOutPath $INSTDIR\qw
  File "qw\qwprogs.dat"
  File "qw\spprogs.dat"
SectionEnd

Section "Extra Logitech Mice Support"
  SetOutPath $INSTDIR
  File "lib\mw_hook.dll"
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"
  SetOutPath $INSTDIR
  CreateDirectory "$SMPROGRAMS\ezQuake"
  CreateShortCut "$SMPROGRAMS\ezQuake\Uninstall.lnk" "$INSTDIR\ezuninstall.exe" "" "$INSTDIR\ezuninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\ezQuake\ezQuake.lnk" "$INSTDIR\ezstart.exe" "" "$INSTDIR\ezquake.exe" 0
  CreateShortCut "$SMPROGRAMS\ezQuake\Manual.lnk" "$INSTDIR\ezquake\manual\index.html" "" "$INSTDIR\ezquake\manual\index.html" 0
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake"
  DeleteRegKey HKLM "SOFTWARE\ezQuake"

  ; Remove files and uninstaller
  Delete "$INSTDIR\ezquake.exe"
  Delete "$INSTDIR\ezquake-gl.exe"
  Delete "$INSTDIR\ezquake-security.dll"
  Delete "$INSTDIR\ezstart.exe"
  Delete "$INSTDIR\ezuninstall.exe"
  RMDir /r "$INSTDIR\ezquake\help"
  RMDir /r "$INSTDIR\ezquake\keymaps"
  RMDir /r "$INSTDIR\ezquake\manual"
  RMDir /r "$INSTDIR\ezquake\sb"
  Delete "$INSTDIR\ezquake\pak0.pak"

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\ezQuake\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\ezQuake"

  ; Don't remove install dir because it's Quake root dir!
  ; Don't remove ezquake\configs because users may have use of their configs in future!

SectionEnd
