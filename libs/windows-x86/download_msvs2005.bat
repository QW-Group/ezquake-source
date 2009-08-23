REM current version in the repo is compiled with msvs2008 which seems to have backwards incompatible format of the lib with msvs2005
REM this script therefore downloads older versions which were compiled with msvs2005 (and are compatible with both msvs2005 and msvs2008)

"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/*checkout*/ezquake/trunk/libs/windows-x86/libexpat.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/ezquake/trunk/libs/windows-x86/libjpeg.lib?revision=1363 -O libjpeg.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/*checkout*/ezquake/trunk/libs/windows-x86/libpcre.lib?revision=3464 -O libpcre.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/ezquake/trunk/libs/windows-x86/libpng.lib?revision=3585 -O libpng.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/ezquake/trunk/libs/windows-x86/libtcl.lib?revision=3464 -O libtcl.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/ezquake/trunk/libs/windows-x86/libz.lib?revision=1784 -O libz.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/*checkout*/ezquake/trunk/libs/windows-x86/mgllt.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/*checkout*/ezquake/trunk/libs/windows-x86/minizip.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/*checkout*/ezquake/trunk/libs/windows-x86/asmlibm.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/*checkout*/ezquake/trunk/libs/windows-x86/dinput.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/*checkout*/ezquake/trunk/libs/windows-x86/dsound.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/*checkout*/ezquake/trunk/libs/windows-x86/dxguid.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/ezquake/trunk/libs/windows-x86/libircclient-static.lib?revision=3592 -O libircclient-static.lib
"../../make/wget.exe" http://ezquake.svn.sourceforge.net/viewvc/*checkout*/ezquake/trunk/libs/windows-x86/glew32.lib
