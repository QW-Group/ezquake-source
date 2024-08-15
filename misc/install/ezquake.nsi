;This is NSIS Script file.
;See http://nsis.sf.net/
;
;You are required to place it into same dir with these files:
;  ezquake.nsi (this file)
;  ezquake-gl.exe (GLRelease)
;  ezquake.exe (Release)
;  gnu.txt (GNU GENERAL PUBLIC LICENSE, Version 2, June 1991)
;  qw/ (dir)
;    fragfile.dat (CVS/ezquake/misc/fragfile/fragfile.dat)
;    skins/ (dir)
;      (CVS/media/game/skins/ go here)
;  ezquake/ (dir)
;    base.pk3 (pak)
;      / (from cvs/media/game except for cvs/media/game/textures/wad and cvs/media/game/skins/)
;    hud.pk3 (pak)
;      /textures/wad (from cvs/media/textures/wad)
;    help.pk3
;      /help/variables/ (from documentation system)
;      /help/commands/ (from documentation system)
;    progs.pk3 (pak)
;      qwprogs.dat (http://zquake.frag.ru/files/qwprogs.dat)
;      spprogs.dat (http://zquake.frag.ru/files/spprogs.dat)
;    locs.pk3 (pak) (CVS/media/game/locs)
;    cfg/ (dir, see CVS/ezquake/misc/cfg/)
;    help/ (dir, see CVS/documentation)
;      manual/ (cvs/documentation/manual)
;      xsd/ (cvs/documentation/xsd)
;      xsl/ (cvs/documentation/xsl)
;      index.xml (cvs/documentation/index.xml)
;    manual/ (dir, link to online version at http://ezquake.sourceforge.net/docs/)
;    sb/ (dir, see CVS/ezquake/misc/sb)
;  inst_gfx/ (dir)
;    top.bmp (cuky)
;    left.bmp (cuky)
;    ezinst.ico (cuky)
;
;Using NSIS, files listed above and this install script you are able to create Windows installer for ezQuake.
;
;--------------------------------

; We go for Modern UI
!include "MUI.nsh"
!include "FileFunc.nsh"
!insertmacro DirState
!define MUI_ICON "inst_gfx\ezinst.ico"
!define MUI_UNICON "inst_gfx\ezinst.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "inst_gfx\top.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "inst_gfx\top.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "inst_gfx\left.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "inst_gfx\left.bmp"
!define MUI_HEADERIMAGE_BITMAP_NOSTRETCH
!define MUI_FINISHPAGE_NOAUTOCLOSE
SetCompressor /SOLID /FINAL lzma

; The name of the installer
Name "ezQuake"

; The file to write
OutFile "ezquake_win32_xxxx.exe"

; The default installation directory
InstallDir "C:\Quake\"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\ezQuake" "Install_dir"


;--------------------------------
; Pages
!define MUI_WELCOMEPAGE_TITLE "Welcome to ezQuake installation"
!insertmacro MUI_PAGE_WELCOME

;!insertmacro MUI_PAGE_LICENSE readme.txt

!insertmacro MUI_PAGE_LICENSE gnu.txt

!insertmacro MUI_PAGE_COMPONENTS

!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Choose your Quake directory. It should contain id1 subdirectory."
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE CheckId1Presence
!insertmacro MUI_PAGE_DIRECTORY

!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_SHOWREADME http://ezquake.sourceforge.net/docs/?setup
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Show Readme"
!define MUI_FINISHPAGE_LINK_LOCATION http://ezquake.sourceforge.net/docs/?changelog
!define MUI_FINISHPAGE_LINK "Changelog"
!insertmacro MUI_PAGE_FINISH

; uninstaller
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES


;--------------------------------
; Language
!insertmacro MUI_LANGUAGE "English"


;--------------------------------
; The stuff to install
Section "!ezQuake client" Main

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath "$INSTDIR"
  
  ; Put file there
  File "ezquake-gl.exe"

  CreateDirectory $INSTDIR\ezquake
  SetOutPath $INSTDIR\ezquake
  File "ezquake\base.pk3"
  File "ezquake\help.pk3"
  File "ezquake\locs.pk3"
  File "ezquake\levelshots.pk3"
  File "ezquake\pak.lst"

  CreateDirectory $INSTDIR\ezquake\cfg
  SetOutPath $INSTDIR\ezquake\cfg
  File "ezquake\cfg\*.cfg"

  CreateDirectory $INSTDIR\ezquake\help
  CreateDirectory $INSTDIR\ezquake\help\manual
  CreateDirectory $INSTDIR\ezquake\help\xsd
  CreateDirectory $INSTDIR\ezquake\help\xsl
  SetOutPath $INSTDIR\ezquake\help
  File /r "ezquake\help\index.xml"
  SetOutPath $INSTDIR\ezquake\help\manual
  File /r "ezquake\help\manual\*.*"
  SetOutPath $INSTDIR\ezquake\help\xsd
  File "ezquake\help\xsd\*.*"
  SetOutPath $INSTDIR\ezquake\help\xsl
  File "ezquake\help\xsl\*.*"

  CreateDirectory $INSTDIR\ezquake\sb
  SetOutPath $INSTDIR\ezquake\sb
  File "ezquake\sb\au-sv.txt"
  File "ezquake\sb\ctf.txt"
  File "ezquake\sb\eu-sv.txt"
  File "ezquake\sb\global.txt"
  File "ezquake\sb\na-sv.txt"
  File "ezquake\sb\qizmo.txt"
  File "ezquake\sb\sa-sv.txt"
  File "ezquake\sb\tf.txt"
  File "ezquake\sb\eu-4on4.txt"
  ; here we make sure user always has some server browser sources file even if he unchecks next section and doesn't have any sources.txt yet
  SetOverwrite off
  File "ezquake\sb\sources.txt"
  SetOverwrite on

  CreateDirectory $INSTDIR\qw
  SetOutPath $INSTDIR\qw
  File "qw\fragfile.dat"
  CreateDirectory $INSTDIR\qw\skins
  SetOutPath $INSTDIR\qw\skins
  File "qw\skins\*.*"
  
  CreateDirectory $INSTDIR\ezquake\manual
  SetOutPath $INSTDIR\ezquake\manual
  File /r "ezquake\manual\*.*"  

  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\ezQuake "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "ezQuake" "QuakeWorld client"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "UninstallString" '"$INSTDIR\ezuninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ezQuake" "NoRepair" 1
  WriteUninstaller "ezuninstall.exe"

SectionEnd

Section "Server Browser Index" SBIndex
  SetOutPath $INSTDIR\ezquake\sb
  File "ezquake\sb\sources.txt"
SectionEnd

Section "QuakeWorld Progs" Progs
  SetOutPath $INSTDIR\ezquake
  File "ezquake\progs.pk3"
SectionEnd

Section /o "Modern HUD Icons" HUDIcons
  SetOutPath $INSTDIR\ezquake
  File "ezquake\hud.pk3"
SectionEnd

Section /o "Software rendering" Software
  SetOutPath $INSTDIR
  File "ezquake.exe"
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts" StartMenu
  SetOutPath $INSTDIR
  CreateDirectory "$SMPROGRAMS\ezQuake"
  CreateShortCut "$SMPROGRAMS\ezQuake\Uninstall.lnk" "$INSTDIR\ezuninstall.exe" ""
  CreateShortCut "$SMPROGRAMS\ezQuake\ezQuake.lnk" "$INSTDIR\ezquake-gl.exe" "" "$INSTDIR\ezquake-gl.exe"
  CreateShortCut "$SMPROGRAMS\ezQuake\Manual.lnk" "http://ezquake.sourceforge.net/docs/" ""
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
  Delete "$INSTDIR\ezuninstall.exe"
  RMDir /r "$INSTDIR\ezquake\help"
  RMDir /r "$INSTDIR\ezquake\manual"
  RMDir /r "$INSTDIR\ezquake\sb"
  Delete "$INSTDIR\ezquake\base.pk3"
  Delete "$INSTDIR\ezquake\hud.pk3"
  Delete "$INSTDIR\ezquake\locs.pk3"
  Delete "$INSTDIR\ezquake\progs.pk3"
  Delete "$INSTDIR\ezquake\help.pk3"
  Delete "$INSTDIR\ezquake\levelshots.pk3"
  Delete "$INSTDIR\ezquake\pak.lst"

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\ezQuake\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\ezQuake"

  ; Don't remove install dir because it's Quake root dir!
  ; Don't remove ezquake\configs because users may have use of their configs in future!

SectionEnd

Function CheckId1Presence
	${DirState} "$INSTDIR\id1" $R0	
	StrCmp $R0 -1 0 +2
		MessageBox MB_OK|MB_ICONEXCLAMATION "The id1 subdirectory was not found. Probably you won't be able to run this client. You have to install this client into the same directory where id1 subdirectory from Quake 1 installation is."
FunctionEnd

LangString DESC_Section1 ${LANG_ENGLISH} "Main client data"
LangString DESC_Section2 ${LANG_ENGLISH} "If you wish to keep your own Server Browser sources list, uncheck this. Note: offline server lists we provide will always get updated with this installer."
LangString DESC_Section3 ${LANG_ENGLISH} "Modern stylish head up display icons."
LangString DESC_Section4 ${LANG_ENGLISH} "Will install executable with software rendering support. Install this if your computer's graphics card is really old or is missing OpenGL acceleration."
LangString DESC_Section5 ${LANG_ENGLISH} "Check in case you want to bind extra mouse buttons when using Logitech mouse with MouseWare drivers."
LangString DESC_Section6 ${LANG_ENGLISH} "Start menu shortcuts"
LangString DSC_SCT_PGS ${LANG_ENGLISH} "You need this to host a Multiplayer game or start a Singleplayer game."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${Main} $(DESC_Section1)
  !insertmacro MUI_DESCRIPTION_TEXT ${SBIndex} $(DESC_Section2)
  !insertmacro MUI_DESCRIPTION_TEXT ${HUDIcons} $(DESC_Section3)
  !insertmacro MUI_DESCRIPTION_TEXT ${Software} $(DESC_Section4)
  !insertmacro MUI_DESCRIPTION_TEXT ${StartMenu} $(DESC_Section6)
  !insertmacro MUI_DESCRIPTION_TEXT ${Progs} $(DSC_SCT_PGS)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
