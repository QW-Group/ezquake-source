# Microsoft Developer Studio Project File - Name="ezquake" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=ezquake - Win32 GLDebug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ezquake.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ezquake.mak" CFG="ezquake - Win32 GLDebug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ezquake - Win32 GLDebug" (based on "Win32 (x86) Application")
!MESSAGE "ezquake - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "ezquake - Win32 GLRelease" (based on "Win32 (x86) Application")
!MESSAGE "ezquake - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug-gl"
# PROP BASE Intermediate_Dir ".\Debug-gl"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug-gl"
# PROP Intermediate_Dir ".\Debug-gl"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /ML /W3 /WX /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "id386" /D "GLQUAKE" /D "WITH_JPEG" /D "WITH_PNG" /D "WITH_KEYMAP" /Fr"$(IntDir)/" /GZ PRECOMP_VC7_TOBEREMOVED /c
# ADD CPP /nologo /G6 /ML /W3 /WX /GX /ZI /Od /D "WIN32" /D "WITH_KEYMAP" /D "_DEBUG" /D "_WINDOWS" /D "id386" /D "GLQUAKE" /D "WITH_JPEG" /D "WITH_PNG" /Fr"$(IntDir)/" /GZ PRECOMP_VC7_TOBEREMOVED /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libjpeg.lib libpng.lib dxguid.lib opengl32.lib wsock32.lib winmm.lib /nologo /subsystem:windows /pdb:".\Debug-gl\ezquake-gl.pdb" /debug /machine:I386 /out:"$(Outdir)\ezquake.exe" /pdbtype:sept
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libjpeg.lib libpng.lib dxguid.lib opengl32.lib wsock32.lib winmm.lib /nologo /subsystem:windows /pdb:".\Debug-gl\ezquake-gl.pdb" /debug /machine:I386 /out:"$(Outdir)\ezquake.exe" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /W3 /WX /GX /Oi /Oy /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "id386" /Fr"$(IntDir)/" /GF PRECOMP_VC7_TOBEREMOVED /c
# ADD CPP /nologo /G6 /W3 /WX /GX /Oi /Oy /Ob2 /D "WIN32" /D "WITH_KEYMAP" /D "NDEBUG" /D "_WINDOWS" /D "id386" /Fr"$(IntDir)/" /GF PRECOMP_VC7_TOBEREMOVED /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mglfx.lib dxguid.lib wsock32.lib winmm.lib /nologo /subsystem:windows /machine:I386 /nodefaultlib:"libcmt" /out:"$(Outdir)\ezquake.exe" /pdbtype:sept /release
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mglfx.lib dxguid.lib wsock32.lib winmm.lib /nologo /subsystem:windows /machine:I386 /nodefaultlib:"libcmt" /out:"$(Outdir)\ezquake.exe" /pdbtype:sept /release
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release-gl"
# PROP BASE Intermediate_Dir ".\Release-gl"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release-gl"
# PROP Intermediate_Dir ".\Release-gl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /W3 /WX /GX /Ot /Og /Oi /Oy /Ob2 /D "WIN32" /D "WITH_KEYMAP" /D "NDEBUG" /D "_WINDOWS" /D "id386" /D "GLQUAKE" /D "WITH_JPEG" /D "WITH_PNG" /Fr"$(IntDir)/" /GF PRECOMP_VC7_TOBEREMOVED /c
# ADD CPP /nologo /G6 /W3 /WX /GX /Ot /Og /Oi /Oy /Ob2 /D "WIN32" /D "WITH_KEYMAP" /D "NDEBUG" /D "_WINDOWS" /D "id386" /D "GLQUAKE" /D "WITH_JPEG" /D "WITH_PNG" /GF /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libjpeg.lib libpng.lib dxguid.lib opengl32.lib wsock32.lib winmm.lib /nologo /subsystem:windows /machine:I386 /out:"$(Outdir)\ezquake-gl.exe" /pdbtype:sept /release
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libjpeg.lib libpng.lib dxguid.lib opengl32.lib wsock32.lib winmm.lib /nologo /subsystem:windows /machine:I386 /out:"C:\quake\sopa.exe" /pdbtype:sept /release
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /ML /W3 /WX /GX /ZI /Od /I "d:\quake-work\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "id386" /Fr"$(IntDir)/" /GZ PRECOMP_VC7_TOBEREMOVED /c
# ADD CPP /nologo /G6 /ML /W3 /WX /GX /ZI /Od /I "d:\quake-work\include" /D "WIN32" /D "WITH_KEYMAP" /D "_DEBUG" /D "_WINDOWS" /D "id386" /Fr"$(IntDir)/" /GZ PRECOMP_VC7_TOBEREMOVED /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mglfx.lib dxguid.lib wsock32.lib winmm.lib /nologo /subsystem:windows /debug /machine:I386 /nodefaultlib:"libcmt" /out:"$(Outdir)\ezquake.exe" /pdbtype:sept /libpath:"d:\quake-work\lib"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mglfx.lib dxguid.lib wsock32.lib winmm.lib /nologo /subsystem:windows /debug /machine:I386 /nodefaultlib:"libcmt" /out:"$(Outdir)\ezquake.exe" /pdbtype:sept /libpath:"d:\quake-work\lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "ezquake - Win32 GLDebug"
# Name "ezquake - Win32 Release"
# Name "ezquake - Win32 GLRelease"
# Name "ezquake - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Group "OpenGL"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\gl_draw.c
DEP_CPP_GL_DR=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_image.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\sbar.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\version.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_DR=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_mesh.c
DEP_CPP_GL_ME=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_ME=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_model.c
DEP_CPP_GL_MO=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\crc.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\fmod.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_MO=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_ngraph.c
DEP_CPP_GL_NG=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_NG=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_refrag.c
DEP_CPP_GL_RE=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_RE=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_rlight.c
DEP_CPP_GL_RL=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_RL=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# ADD CPP /nologo /GX /GZ

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# ADD CPP /nologo /GX /Oy

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Intermediate_Dir ".\Debug/"
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_rmain.c
DEP_CPP_GL_RM=\
	".\anorm_dots.h"\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_RM=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_rmisc.c
DEP_CPP_GL_RMI=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\rulesets.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_RMI=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=gl_rpart.c
DEP_CPP_GL_RP=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_RP=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_rsurf.c
DEP_CPP_GL_RS=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_RS=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_screen.c
DEP_CPP_GL_SC=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_image.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\image.h"\
	".\keys.h"\
	".\mathlib.h"\
	".\menu.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\sbar.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_SC=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=gl_texture.c
DEP_CPP_GL_TE=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\crc.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_image.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_TE=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_warp.c
DEP_CPP_GL_WA=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\gl_warp_sin.h"\
	".\image.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_WA=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "System"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\sys_win.c
DEP_CPP_SYS_W=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_SYS_W=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# End Group
# Begin Group "Sound"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\cd_win.c
DEP_CPP_CD_WI=\
	".\bspfile.h"\
	".\cdaudio.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CD_WI=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\snd_dma.c
DEP_CPP_SND_D=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_SND_D=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\snd_mem.c
DEP_CPP_SND_M=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\fmod.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_SND_M=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\snd_mix.c
DEP_CPP_SND_MI=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_SND_MI=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\snd_win.c
DEP_CPP_SND_W=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_SND_W=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# End Group
# Begin Group "Input"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\cl_input.c
DEP_CPP_CL_IN=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\input.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CL_IN=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\in_win.c
DEP_CPP_IN_WI=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\input.h"\
	".\keys.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\movie.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_IN_WI=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\keys.c
DEP_CPP_KEYS_=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\keys.h"\
	".\mathlib.h"\
	".\menu.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_KEYS_=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# End Group
# Begin Group "Server"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\sv_ccmds.c
DEP_CPP_SV_CC=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_SV_CC=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sv_ents.c
DEP_CPP_SV_EN=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_SV_EN=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sv_init.c
DEP_CPP_SV_IN=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\crc.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_SV_IN=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sv_main.c
DEP_CPP_SV_MA=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\version.h"\
	".\zone.h"\
	
NODEP_CPP_SV_MA=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sv_move.c
DEP_CPP_SV_MO=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_SV_MO=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sv_nchan.c
DEP_CPP_SV_NC=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_SV_NC=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sv_phys.c
DEP_CPP_SV_PH=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_SV_PH=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=sv_save.c
DEP_CPP_SV_SA=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_SV_SA=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sv_send.c
DEP_CPP_SV_SE=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_SV_SE=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sv_user.c
DEP_CPP_SV_US=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_SV_US=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sv_world.c
DEP_CPP_SV_WO=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_SV_WO=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# End Group
# Begin Group "Network"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\net_chan.c
DEP_CPP_NET_C=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_NET_C=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\net_wins.c
DEP_CPP_NET_W=\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\mathlib.h"\
	".\net.h"\
	".\protocol.h"\
	".\sys.h"\
	".\winquake.h"\
	".\zone.h"\
	
# End Source File
# End Group
# Begin Group "VM"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\pr_cmds.c
DEP_CPP_PR_CM=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_PR_CM=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\pr_edict.c
DEP_CPP_PR_ED=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\crc.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_PR_ED=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\pr_exec.c
DEP_CPP_PR_EX=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\qwsvdef.h"\
	".\r_model.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sv_world.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_PR_EX=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# End Group
# Begin Group "Misc"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\cmd.c
DEP_CPP_CMD_C=\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\mathlib.h"\
	".\net.h"\
	".\protocol.h"\
	".\sys.h"\
	".\zone.h"\
	
# End Source File
# Begin Source File

SOURCE=.\common.c
DEP_CPP_COMMO=\
	".\cmd.h"\
	".\common.h"\
	".\crc.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\mathlib.h"\
	".\net.h"\
	".\protocol.h"\
	".\sys.h"\
	".\zone.h"\
	
# End Source File
# Begin Source File

SOURCE=.\console.c
DEP_CPP_CONSO=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\ignore.h"\
	".\keys.h"\
	".\logging.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CONSO=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\crc.c
DEP_CPP_CRC_C=\
	".\cmd.h"\
	".\common.h"\
	".\crc.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\mathlib.h"\
	".\net.h"\
	".\protocol.h"\
	".\sys.h"\
	".\zone.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cvar.c
DEP_CPP_CVAR_=\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\mathlib.h"\
	".\net.h"\
	".\protocol.h"\
	".\sys.h"\
	".\utils.h"\
	".\zone.h"\
	
# End Source File
# Begin Source File

SOURCE=.\host.c
DEP_CPP_HOST_=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\version.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_HOST_=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\image.c
DEP_CPP_IMAGE=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\image.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_IMAGE=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\mathlib.c
DEP_CPP_MATHL=\
	".\bspfile.h"\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\r_model.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\zone.h"\
	
NODEP_CPP_MATHL=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\mdfour.c
DEP_CPP_MDFOU=\
	".\mdfour.h"\
	
# End Source File
# Begin Source File

SOURCE=.\menu.c
DEP_CPP_MENU_=\
	".\bspfile.h"\
	".\cl_slist.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\input.h"\
	".\keys.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\mp3_player.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\server.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\version.h"\
	".\vid.h"\
	".\wad.h"\
	".\winamp.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_MENU_=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\pmove.c
DEP_CPP_PMOVE=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_PMOVE=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\pmovetst.c
DEP_CPP_PMOVET=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_PMOVET=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\sbar.c
DEP_CPP_SBAR_=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pr_comp.h"\
	".\progdefs.h"\
	".\progs.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\rulesets.h"\
	".\sbar.h"\
	".\screen.h"\
	".\server.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_SBAR_=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\screen.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\skin.c
DEP_CPP_SKIN_=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\image.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_SKIN_=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\teamplay.c
DEP_CPP_TEAMP=\
	".\auth.h"\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\ignore.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\rulesets.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\utils.h"\
	".\version.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_TEAMP=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\version.c
DEP_CPP_VERSI=\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\mathlib.h"\
	".\net.h"\
	".\protocol.h"\
	".\sys.h"\
	".\version.h"\
	".\zone.h"\
	
# End Source File
# Begin Source File

SOURCE=.\wad.c
DEP_CPP_WAD_C=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_WAD_C=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\zone.c
DEP_CPP_ZONE_=\
	".\cmd.h"\
	".\common.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\mathlib.h"\
	".\net.h"\
	".\protocol.h"\
	".\sys.h"\
	".\zone.h"\
	
# End Source File
# End Group
# Begin Group "Draw"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\r_aclip.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_alias.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_bsp.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_draw.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_edge.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_efrag.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_light.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_main.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_misc.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_model.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_part.c
DEP_CPP_R_PAR=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\d_iface.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_local.h"\
	".\r_model.h"\
	".\r_shared.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_R_PAR=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\r_rast.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_sky.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_sprite.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_surf.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_vars.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Software"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\d_edge.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_fill.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_init.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_modech.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_part.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_polyse.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_scan.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_sky.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_sprite.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_surf.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_vars.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_zpoint.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Video"

# PROP Default_Filter ""
# Begin Source File

SOURCE=vid_common_gl.c
DEP_CPP_VID_C=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_VID_C=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\vid_wgl.c
DEP_CPP_VID_W=\
	".\bspfile.h"\
	".\cdaudio.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\keys.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\mw_hook.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\sbar.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_VID_W=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\vid_win.c

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Client"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\cl_cam.c
DEP_CPP_CL_CA=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\sbar.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CL_CA=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cl_cmd.c
DEP_CPP_CL_CM=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\config_manager.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\menu.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\version.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_CL_CM=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cl_demo.c
DEP_CPP_CL_DE=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\movie.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_CL_DE=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cl_ents.c
DEP_CPP_CL_EN=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CL_EN=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cl_main.c
DEP_CPP_CL_MA=\
	".\auth.h"\
	".\bspfile.h"\
	".\cdaudio.h"\
	".\cl_slist.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\config_manager.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\fchecks.h"\
	".\fmod.h"\
	".\gl_model.h"\
	".\ignore.h"\
	".\input.h"\
	".\keys.h"\
	".\logging.h"\
	".\mathlib.h"\
	".\menu.h"\
	".\modelgen.h"\
	".\modules.h"\
	".\movie.h"\
	".\mp3_player.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\rulesets.h"\
	".\sbar.h"\
	".\screen.h"\
	".\security.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\version.h"\
	".\vid.h"\
	".\wad.h"\
	".\winamp.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_CL_MA=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cl_parse.c
DEP_CPP_CL_PA=\
	".\auth.h"\
	".\bspfile.h"\
	".\cdaudio.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\config_manager.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\fchecks.h"\
	".\gl_model.h"\
	".\ignore.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\sbar.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\utils.h"\
	".\version.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CL_PA=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cl_pred.c
DEP_CPP_CL_PR=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CL_PR=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cl_slist.c
DEP_CPP_CL_SL=\
	".\bspfile.h"\
	".\cl_slist.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CL_SL=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cl_tent.c
DEP_CPP_CL_TE=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\sound.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CL_TE=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\cl_view.c
DEP_CPP_CL_VI=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\d_iface.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\pmove.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_local.h"\
	".\r_model.h"\
	".\r_shared.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CL_VI=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# End Group
# Begin Group "Fuh"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\auth.c
DEP_CPP_AUTH_=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\modules.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\security.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\winquake.h"\
	".\zone.h"\
	
NODEP_CPP_AUTH_=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=config_manager.c
DEP_CPP_CONFI=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\input.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\teamplay.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_CONFI=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\fchecks.c
DEP_CPP_FCHEC=\
	".\auth.h"\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\fmod.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\modules.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\quotes.h"\
	".\r_model.h"\
	".\render.h"\
	".\rulesets.h"\
	".\screen.h"\
	".\security.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\version.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_FCHEC=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\fmod.c
DEP_CPP_FMOD_=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\fchecks.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\modules.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\security.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_FMOD_=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\gl_image.c
DEP_CPP_GL_IM=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_local.h"\
	".\gl_model.h"\
	".\gl_texture.h"\
	".\image.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_GL_IM=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\jpeglib.h"\
	".\lbmlib.h"\
	".\png.h"\
	".\scriplib.h"\
	".\trilib.h"\
	

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ignore.c
DEP_CPP_IGNOR=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_IGNOR=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\logging.c
DEP_CPP_LOGGI=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_LOGGI=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=modules.c
DEP_CPP_MODUL=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\modules.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\security.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\version.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_MODUL=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\movie.c
DEP_CPP_MOVIE=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_MOVIE=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=mp3_player.c
DEP_CPP_MP3_P=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\mp3_player.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\winamp.h"\
	".\zone.h"\
	
NODEP_CPP_MP3_P=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=rulesets.c
DEP_CPP_RULES=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_RULES=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# Begin Source File

SOURCE=.\utils.c
DEP_CPP_UTILS=\
	".\bspfile.h"\
	".\cl_view.h"\
	".\client.h"\
	".\cmd.h"\
	".\common.h"\
	".\console.h"\
	".\cvar.h"\
	".\cvar_groups.h"\
	".\draw.h"\
	".\gl_model.h"\
	".\mathlib.h"\
	".\modelgen.h"\
	".\net.h"\
	".\protocol.h"\
	".\quakedef.h"\
	".\r_model.h"\
	".\render.h"\
	".\screen.h"\
	".\spritegn.h"\
	".\sys.h"\
	".\utils.h"\
	".\vid.h"\
	".\wad.h"\
	".\zone.h"\
	
NODEP_CPP_UTILS=\
	".\cmdlib.h"\
	".\dictlib.h"\
	".\lbmlib.h"\
	".\scriplib.h"\
	".\trilib.h"\
	
# End Source File
# End Group
# Begin Source File

SOURCE=.\winquake.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Group "Misc_h"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\adivtab.h
# End Source File
# Begin Source File

SOURCE=.\anorm_dots.h
# End Source File
# Begin Source File

SOURCE=.\anorms.h
# End Source File
# Begin Source File

SOURCE=.\bothdefs.h
# End Source File
# Begin Source File

SOURCE=.\bspfile.h
# End Source File
# Begin Source File

SOURCE=.\cmd.h
# End Source File
# Begin Source File

SOURCE=.\common.h
# End Source File
# Begin Source File

SOURCE=.\crc.h
# End Source File
# Begin Source File

SOURCE=.\cvar.h
# End Source File
# Begin Source File

SOURCE=.\image.h
# End Source File
# Begin Source File

SOURCE=.\input.h
# End Source File
# Begin Source File

SOURCE=.\keys.h
# End Source File
# Begin Source File

SOURCE=.\mathlib.h
# End Source File
# Begin Source File

SOURCE=.\modelgen.h
# End Source File
# Begin Source File

SOURCE=.\sbar.h
# End Source File
# Begin Source File

SOURCE=.\screen.h
# End Source File
# Begin Source File

SOURCE=.\spritegn.h
# End Source File
# Begin Source File

SOURCE=.\sys.h
# End Source File
# Begin Source File

SOURCE=.\teamplay.h
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# Begin Source File

SOURCE=.\vid.h
# End Source File
# Begin Source File

SOURCE=.\wad.h
# End Source File
# Begin Source File

SOURCE=.\winquake.h
# End Source File
# Begin Source File

SOURCE=.\zone.h
# End Source File
# End Group
# Begin Group "fuh_h"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\auth.h
# End Source File
# Begin Source File

SOURCE=auth_allowip.h
# End Source File
# Begin Source File

SOURCE=config_manager.h
# End Source File
# Begin Source File

SOURCE=cvar_groups.h
# End Source File
# Begin Source File

SOURCE=.\fchecks.h
# End Source File
# Begin Source File

SOURCE=.\fmod.h
# End Source File
# Begin Source File

SOURCE=.\gl_image.h
# End Source File
# Begin Source File

SOURCE=gl_texture.h
# End Source File
# Begin Source File

SOURCE=.\ignore.h
# End Source File
# Begin Source File

SOURCE=.\logging.h
# End Source File
# Begin Source File

SOURCE=modules.h
# End Source File
# Begin Source File

SOURCE=.\movie.h
# End Source File
# Begin Source File

SOURCE=mp3_player.h
# End Source File
# Begin Source File

SOURCE=.\quotes.h
# End Source File
# Begin Source File

SOURCE=rulesets.h
# End Source File
# Begin Source File

SOURCE=.\utils.h
# End Source File
# End Group
# Begin Group "OpenGL_h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gl_local.h
# End Source File
# Begin Source File

SOURCE=.\gl_model.h
# End Source File
# Begin Source File

SOURCE=.\gl_warp_sin.h
# End Source File
# End Group
# Begin Group "Software_h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\d_iface.h
# End Source File
# Begin Source File

SOURCE=.\d_ifacea.h
# End Source File
# Begin Source File

SOURCE=.\d_local.h
# End Source File
# End Group
# Begin Group "Main_h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\quakedef.h
# End Source File
# End Group
# Begin Group "Client_h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\cl_view.h
# End Source File
# Begin Source File

SOURCE=.\client.h
# End Source File
# Begin Source File

SOURCE=.\console.h
# End Source File
# Begin Source File

SOURCE=.\menu.h
# End Source File
# End Group
# Begin Group "Server_h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\pmove.h
# End Source File
# End Group
# Begin Group "Sound_h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\cdaudio.h
# End Source File
# Begin Source File

SOURCE=.\sound.h
# End Source File
# End Group
# Begin Group "Draw_h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\draw.h
# End Source File
# Begin Source File

SOURCE=.\r_local.h
# End Source File
# Begin Source File

SOURCE=.\r_model.h
# End Source File
# Begin Source File

SOURCE=.\r_shared.h
# End Source File
# Begin Source File

SOURCE=.\render.h
# End Source File
# End Group
# Begin Group "Net_h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\net.h
# End Source File
# Begin Source File

SOURCE=.\protocol.h
# End Source File
# End Group
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\ezquake.bmp
# End Source File
# Begin Source File

SOURCE=.\ezquake.ico
# End Source File
# End Group
# Begin Group "Asm Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\cl_math.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug-gl
InputPath=.\cl_math.s
InputName=cl_math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "GLQUAKE" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\cl_math.s
InputName=cl_math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release-gl
InputPath=.\cl_math.s
InputName=cl_math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "GLQUAKE" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\cl_math.s
InputName=cl_math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_draw.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\d_draw.s
InputName=d_draw

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_draw.s
InputName=d_draw

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_draw16.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\d_draw16.s
InputName=d_draw16

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_draw16.s
InputName=d_draw16

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_parta.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\d_parta.s
InputName=d_parta

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_parta.s
InputName=d_parta

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_polysa.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\d_polysa.s
InputName=d_polysa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_polysa.s
InputName=d_polysa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_scana.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\d_scana.s
InputName=d_scana

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_scana.s
InputName=d_scana

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_spr8.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\d_spr8.s
InputName=d_spr8

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_spr8.s
InputName=d_spr8

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_varsa.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\d_varsa.s
InputName=d_varsa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_varsa.s
InputName=d_varsa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\math.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug-gl
InputPath=.\math.s
InputName=math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "GLQUAKE" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\math.s
InputName=math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release-gl
InputPath=.\math.s
InputName=math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "GLQUAKE" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\math.s
InputName=math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_aclipa.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\r_aclipa.s
InputName=r_aclipa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_aclipa.s
InputName=r_aclipa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_aliasa.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\r_aliasa.s
InputName=r_aliasa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_aliasa.s
InputName=r_aliasa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_drawa.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\r_drawa.s
InputName=r_drawa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_drawa.s
InputName=r_drawa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_edgea.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\r_edgea.s
InputName=r_edgea

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_edgea.s
InputName=r_edgea

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_varsa.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\r_varsa.s
InputName=r_varsa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_varsa.s
InputName=r_varsa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\snd_mixa.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug-gl
InputPath=.\snd_mixa.s
InputName=snd_mixa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "GLQUAKE" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\snd_mixa.s
InputName=snd_mixa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release-gl
InputPath=.\snd_mixa.s
InputName=snd_mixa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "GLQUAKE" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\snd_mixa.s
InputName=snd_mixa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\surf16.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\surf16.s
InputName=surf16

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\surf16.s
InputName=surf16

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\surf8.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\surf8.s
InputName=surf8

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\surf8.s
InputName=surf8

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sys_x86.s

!IF  "$(CFG)" == "ezquake - Win32 GLDebug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug-gl
InputPath=.\sys_x86.s
InputName=sys_x86

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "GLQUAKE" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=.\sys_x86.s
InputName=sys_x86

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 GLRelease"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release-gl
InputPath=.\sys_x86.s
InputName=sys_x86

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "GLQUAKE" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ezquake - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=.\sys_x86.s
InputName=sys_x86

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	gas2masm < $(OUTDIR)\$(InputName).spp >$(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\worlda.s
# PROP Exclude_From_Build 1
# End Source File
# End Group
# End Target
# End Project
