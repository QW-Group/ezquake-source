//---------------------------------------------------------------------------

//#include <vcl.h>
#pragma hdrstop

#include "Unit1.h"
#include <objidl.h>
#include <shlobj.h>
#include "registry.hpp"
#define HOTKEY(modifier,key) ((((modifier)&0xff)<<8)|((key)&0xff)) 
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
 AnsiString s,cmd,bat,tmp;
 TSearchRec sr;
 int fh,fl;
 bool gl;
TForm1 *Form1;
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
        : TForm(Owner)
{
}
//---------------------------------------------------------------------------


void __fastcall TForm1::FormCreate(TObject *Sender)
{
 DEVMODE DevMode;
 int     rc, xmode;

Application->HintHidePause=60000;
Application->HintPause=0;
Application->ShowHint=0;

//Application->HintColor=clWindow;

gamedirs->Hint=\
"Description:\n\
This sets the directory containing the game that the server uses.\n\
Select the right folder here if you want to run some type of modification with\n\
ezQuake, e.g. Frogbot or TeamFortress. Otherwise just leave this value blank.\n\
\n\Command line parameter:\n\
-game [value]\n\
\n\Recommendation:\n\
See description.\n\
\n\Notes: -";
srvcfg->Hint=\
"Description:\n\
Choose the configuration file that you want to use for the server.\n\
\n\Command line parameter:\n\
+exec [value]\n\
\n\Recommendation:\n\
Only useful if you are running a server.\n\
\n\Notes:\n\
The server will automatically try to execute server.cfg in the directory\n\
which you can specify above.";
port->Hint=\
"Description:\n\
Sets the TCP port number the server opens for client connections.\n\
\n\Command line parameter:\n\
-port [value]\n\
\n\Recommendation:\n\
27500 (default)\n\
\n\Notes:\n\
If you chose another port than 27500, you'll have to specify that when\n\
you connect to the server (e.g. connect localhost:27501).";
ded->Hint=\
"Description:\n\
Check here if you want to run a dedicated server.\n\
\n\Command line parameter:\n\
-dedicated\n\
\n\Recommendation:\n\
See description.\n\
\n\Notes: -";
mem->Hint=\
"Description:\n\
Specifies the amount of memory in megabytes that ezQuake should allocate.\n\
\n\Command line parameter:\n\
-mem [value]\n\
\n\Recommendation:\n\
32 Mb should be enough for both OpenGL and software ezQuake.\n\
\n\Notes: -";
democache->Hint=\
"Description:\n\Specifies the amount of memory in megabytes for demo recording cache.\n\
\n\Command line parameter:\n\
-democache [value]\n\
\n\Recommendation:\n\
Not necessary. You can leave this value blank.\n\
\n\Notes: -";
zone->Hint=\
"Description:\n\
Allocates additional memory to the alias memory space.\n\
\n\Command line parameter:\n\
-zone [value]\n\
\n\Recommendation:\n\
512\n\
\n\Notes: -";
conbuf->Hint=\
"Description:\n\
Specifies console buffer size.\n\
\n\Command line parameter:\n\
-conbufsize [value]\n\
\n\Recommendation:\n\
1024\n\
\n\Notes: -";
condebug->Hint=\
"Description:\n\
Enables logging of the console text in the qconsole.log file in the quake/qw/ directory.\n\
\n\Command line parameter:\n\
-condebug\n\
\n\Recommendation:\n\
Not necessary. You can leave this box unchecked.\n\
\n\Notes: -";
nomouse->Hint=\
"Description:\n\
Disables mouse support. If you want to only play with keyboard or joystick then\n\
you can check this box.\n\
\n\Command line parameter:\n\
-nomouse\n\
\n\Recommendation:\n\
See description.\n\
\n\Notes: -";
dinput->Hint=\
"Description:\n\
Enable the use of DirectInput for mouse.\n\
\n\Command line parameter:\n\
-dinput\n\
\n\Recommendation:\n\
Yes.\n\
\n\Notes: -";
m_smooth->Hint=\
"Description:\n\
Smoothes your mouse movements and maximizes mouse responsiveness\n\
(don't forget to set m_rate to your real mouse rate).\n\
\n\Command line parameter:\n\
-m_smooth\n\
\n\Recommendation:\n\
Yes.\n\
\n\Notes:\n\
This requires you to have DirectInput (-dinput) enabled as well.";
mw_hook->Hint=\
"Description:\n\
Check this box if you are using a Logitech mouse. This will enable support for\n\
binding all buttons on Logitech mice.\n\
\n\Command line parameter:\n\
-m_mwhook\n\
\n\Recommendation:\n\
See description.\n\
\n\Notes: -";
nopar->Hint=\
"Description:\n\
Disable the forcing of mouse parameters (acceleration and speed) on startup.\n\
\n\Command line parameter:\n\
-noforcemparms\n\
\n\Recommendation:\n\
This parameter should be used if you experiance problems with the mouse.\n\
\n\Notes: -";
noacc->Hint=\
"Description:\n\
Disable the forcing of mouse acceleration on startup.\n\
\n\Command line parameter:\n\
-noforcemaccel\n\
\n\Recommendation:\n\
Not necessary. You can leave this box unchecked.\n\
\n\Notes: -";
nospd->Hint=\
"Description:\n\
Disable the forcing of mouse speed on startup.\n\
\n\Command line parameter:\n\
-noforcemspd\n\
\n\Recommendation:\n\
Not necessary. You can leave this box unchecked.\n\
\n\Notes: -";
vmode->Hint=\
"Description:\n\
This specifies the video resolution to run the game in.\n\
\n\Command line parameter:\n\
-width [value] -height [value]\n\
\n\Recommendation:\n\
640x480 should be enough for GL. Additionally, if you have a fast machine you\n\
can set this to 1024x768 or 1280x960 to make everything look a bit sharper.\n\
\n\Notes: -";
cmode->Hint=\
"Description:\n\
This specifies the video resolution for console.\n\
\n\Command line parameter:\n\
-conwidth [value] -conheight [value]\n\
\n\Recommendation:\n\
Conwidth and conheight values should be same, or half of width and height values.\n\
\n\Notes: -";
bpp->Hint=\
"Description:\n\
This allows you to specify how many bits per pixel should be used.\n\
Only available values are 16 and 32 bpp.\n\
\n\Command line parameter:\n\
-bpp [value]\n\
\n\Recommendation:\n\
32\n\
\n\Notes: -";
freq->Hint=\
"Description:\n\
Sets the display frequency the video adapter should switch into.\n\
\n\Command line parameter:\n\
+set vid_displayfrequency [value]\n\
\n\Recommendation:\n\
You can set a higher Hz setting for your monitor than default (60Hz),\n\
if your driver and monitor allows it.\n\
\n\Notes: -";
particles->Hint=\
"Description:\n\
This option specifies the maximum number of particles to be rendered\n\
on the screen at once.\n\
\n\Command line parameter:\n\
-particles [value]\n\
\n\Recommendation:\n\
Not necessary but you can try 512 value to disable particles basically when\n\
you out from teleporter.\n\
\n\Notes: -";
dibonly->Hint=\
"Description:\n\
Forces ezQuake to not use any direct hardware access video modes.\n\
\n\Command line parameter:\n\
-dibonly\n\
\n\Recommendation:\n\
Yes.\n\
\n\Notes: -";
window->Hint=\
"Description:\n\
Run ezQuake in a windowed mode.\n\
\n\Command line parameter:\n\
-window\n\
\n\Recommendation:\n\
Leave this box unchecked.\n\
\n\Notes: -";
current->Hint=\
"Description:\n\
Forces ezQuake to run in the current video mode.\n\
\n\Command line parameter:\n\
-current\n\
\n\Recommendation:\n\
Leave this box unchecked.\n\
\n\Notes: -";
no24bit->Hint=\
"Description:\n\
Disables 24bit textures. This will force ezQuake to not read any textures\n\
from 'textures' subdirectory.\n\
\n\Command line parameter:\n\
-no24bit\n\
\n\Recommendation:\n\
See description. This could also be used as a performance tweak.\n\
\n\Notes: -";
forcetex->Hint=\
"Description:\n\
Forces map textures reloading on mapchange.\n\
\n\Command line parameter:\n\
-forceTextureReload\n\
\n\Recommendation:\n\
See description. This could also be used as a performance tweak.\n\
\n\Notes: -";
nohwgamma->Hint=\
"Description:\n\
Disables hardware gamma control.\n\
\n\Command line parameter:\n\
-nohwgamma\n\
\n\Recommendation:\n\
Leave this box unchecked.\n\
\n\Notes: -";
nomtex->Hint=\
"Description:\n\
Disables multitexturing.\n\
\n\Command line parameter:\n\
-nomtex\n\
\n\Recommendation:\n\
Leave this box unchecked.\n\
\n\Notes: -";
maxtmu2->Hint=\
"Description:\n\
Use this if you get serious FPS drops or your textures look wrong/deformed.\n\
\n\Command line parameter:\n\
-maxtmu2\n\
\n\Recommendation:\n\
See description\n\
\n\Notes:\n\
If you have an ATI compatible graphics card, be sure to use this option\n\
as you may have problems watching mvd demos without this.";
skhz->Hint=\
"Description:\n\
The frequency you want to interpolate the sound to.\n\
\n\Command line parameter:\n\
+set s_khz [value]\n\
\n\Recommendation:\n\
You can try 22 or 44 value.\n\
\n\Notes:-";
nosound->Hint=\
"Description:\n\
Disables initializing of the sound system and sound output in ezQuake.\n\
\n\Command line parameter:\n\
-nosound\n\
\n\Recommendation:\n\
Leave this box unchecked.\n\
\n\Notes: -";
snoforceformat->Hint=\
"Description:\n\
Causes ezQuake to not force the sound hardware into 11KHz 16Bit mode.\n\
\n\Command line parameter:\n\
-snoforceformat\n\
\n\Recommendation:\n\
Leave this box unchecked.\n\
\n\Notes: -";
wavonly->Hint=\
"Description:\n\
Disable DirectSound support in favor of WAV playback.\n\
\n\Command line parameter:\n\
-wavonly\n\
\n\Recommendation:\n\
Leave this box unchecked.\n\
\n\Notes: -";
primarysound->Hint=\
"Description:\n\
Use DirectSound's primary buffer output instead of secondary buffer output.\n\
\n\Command line parameter:\n\
-primarysound\n\
\n\Recommendation:\n\
Leave this box unchecked.\n\
\n\Notes:\n\
Can speed up sound a bit, but causes glitches with many sound systems.";
nomp3->Hint=\
"Description:\n\
Stops ezQuake fiddling with winamp/xmms volume.\n\
\n\Command line parameter:\n\
-nomp3volumectrl\n\
\n\Recommendation:\n\
Leave this box unchecked.\n\
\n\Notes: -";
cdaudio->Hint=\
"Description:\n\
Enables ezQuake's ability to play music CDs.\n\
\n\Command line parameter:\n\
-cdaudio\n\
\n\Recommendation:\n\
Not necessary. You can leave this box unchecked.\n\
\n\Notes: -";
ruleset->Hint=\
"Description:\n\
This feature is used to enforce competition rules.\n\
At the moment there is only the 'default' ruleset and the 'smackdown' ruleset.\n\
The smackdown ruleset differs from the default ruleset in the following ways:\n\
*) The variables tp_triggers, tp_msgtriggers, cl_trueLightning, cl_clock,\n\
r_aliasstats are disabled.\n\
*) If a server has no maxfps serverinfo key, then up to 77 fps is allowed instead\n\
of the default 72 fps limit.\n\
*) When pressing tab to bring up the scoreboard, the seconds drawn in the clock are\n\
only accurate to 10 seconds.\n\
\n\Command line parameter:\n\
-ruleset [value]\n\
\n\Recommendation:\n\
smackdown.\n\
\n\Notes: -";
clcfg->Hint=\
"Description:\n\
Select your own user configuration file to be loaded automatically upon startup.\n\
Reads from the directory quake/ezquake/configs/.\n\
\n\Command line parameter:\n\
+cfg_load [value]\n\
\n\Recommendation:\n\
You can use this to avoid having to type 'cfg_load blabla' each time you start\n\
ezQuake for your saved settings (cfg_save blabla) to be loaded.\n\
\n\Notes: -";
nohwtimer->Hint=\
"Description:\n\
Turns off new precise timer and returns back old behaviour.\n\
\n\Command line parameter:\n\
-nohwtimer\n\
\n\Recommendation:\n\
Not necessary. You can leave this box unchecked.\n\
\n\Notes: -";
/*
noconfirmquit->Hint=\
"Description:\n\
Ask for your confirmation when exiting from ezQuake.\n\
\n\Command line parameter:\n\
+set cl_confirmquit 1\n\
\n\Recommendation:\n\
No.\n\
\n\Notes: -";
*/
other->Hint=\
"Description:\n\
Here you can type all the other command line parameters that you\n\
want to use at startup.\n\
\n\Command line parameter:\n\
[value]\n\
\n\Recommendation:\n\
See description.\n\
\n\Notes:\n\
Be sure to separate all parameters with a spaces.";

noscripts->Hint=\
"Description:\n\
Disables advanced scripting and adds \"noscripts\" to your userinfo.\n\
\n\Command line parameter:\n\
-noscripts\n\
\n\Recommendation:\n\
No.\n\
\n\Notes: -";
indphys->Hint=\
"Description:\n\
This experimental feature allows player to have different\n\
FPS number than the number of frames sent to the server.\n\
\n\Command line parameter:\n\
+set cl_independentPhysics 1\n\
\n\Recommendation:\n\
Yes.\n\
\n\Notes: -";
allowmulty->Hint=\
"Description:\n\
Allows to run one more instance of ezQuake.\n\
\n\Command line parameter:\n\
-allowmultiple\n\
\n\Recommendation:\n\
No.\n\
\n\Notes: -";


if (FileExists(ExtractFilePath(Application->ExeName)+"ezquake-gl.exe"))
 ver->Items->Add("OpenGL");
if (FileExists(ExtractFilePath(Application->ExeName)+"ezquake.exe"))
 ver->Items->Add("Software");
if (ver->Items->Count==0)
 { MessageBox(Application->Handle,"ezQuake executable file(s) not found in current directory","ezStart",MB_OK+MB_ICONWARNING);
   Application->Terminate();
 }
else
 ver->ItemIndex=0;

    rc = 1;
    xmode = 0;
    do  {
        rc = EnumDisplaySettings(NULL, xmode, &DevMode);
        if (rc != 0) {
          if (vmode->Items->IndexOf(IntToStr(DevMode.dmPelsWidth)+"x"+IntToStr(DevMode.dmPelsHeight))==-1)//
           { vmode->Items->Add (IntToStr(DevMode.dmPelsWidth)  + "x" +
                                  IntToStr(DevMode.dmPelsHeight));
             cmode->Items->Add (IntToStr(DevMode.dmPelsWidth)  + "x" +
                                  IntToStr(DevMode.dmPelsHeight));
           }
           if (freq->Items->IndexOf(IntToStr(DevMode.dmDisplayFrequency))==-1 && DevMode.dmDisplayFrequency>=60)
            freq->Items->Add(IntToStr(DevMode.dmDisplayFrequency));
            }
        xmode++;
        } while (rc != 0);

//gamedirs reading
s=ExtractFilePath(Application->ExeName)+"*";
  if (FindFirst(s, 63, sr) == 0)
    do
    {  if ((sr.Attr>=16 && sr.Attr<=23) || (sr.Attr>=48 && sr.Attr<=55))
       if (sr.Name!="." && sr.Name!="..")
         gamedirs->Items->Add(sr.Name);
    } while (FindNext(sr)==0);
  FindClose(sr);

//bats
s=ExtractFilePath(Application->ExeName)+"*.bat";
  if (FindFirst(s, 63, sr) == 0)
    do
    { if (sr.Name!="." && sr.Name!=".." && fbat->Items->IndexOf(sr.Name)==-1)
        fbat->Items->Add(sr.Name);
    } while (FindNext(sr) == 0);
  FindClose(sr);

//srvcfg
s=ExtractFilePath(Application->ExeName)+"id1\\*.cfg";
  if (FindFirst(s, 63, sr) == 0)
    do
    { if (sr.Name!="." && sr.Name!=".." && srvcfg->Items->IndexOf(sr.Name)==-1)
        srvcfg->Items->Add(sr.Name);
    } while (FindNext(sr) == 0);
  FindClose(sr);
s=ExtractFilePath(Application->ExeName)+"qw\\*.cfg";
  if (FindFirst(s, 63, sr) == 0)
    do
    { if (sr.Name!="." && sr.Name!=".." && srvcfg->Items->IndexOf(sr.Name)==-1)
        srvcfg->Items->Add(sr.Name);
    } while (FindNext(sr) == 0);
  FindClose(sr);

//clcfg
s=ExtractFilePath(Application->ExeName)+"ezquake\\configs\\*.cfg";
  if (FindFirst(s, 63, sr) == 0)
    do
    { if (sr.Name!="." && sr.Name!=".." && clcfg->Items->IndexOf(sr.Name)==-1)
        clcfg->Items->Add(sr.Name);
    } while (FindNext(sr) == 0);
  FindClose(sr);

checks();

}
//---------------------------------------------------------------------------

void __fastcall TForm1::cancelClick(TObject *Sender)
{
Close();
}
//---------------------------------------------------------------------------
void __fastcall TForm1::gamedirsChange(TObject *Sender)
{
s=ExtractFilePath(Application->ExeName)+gamedirs->Text+"\\*.cfg";
  if (FindFirst(s, 63, sr) == 0)
    do
    { if (sr.Name!="." && sr.Name!=".." && srvcfg->Items->IndexOf(sr.Name)==-1)
        srvcfg->Items->Add(sr.Name);
    } while (FindNext(sr) == 0);
  FindClose(sr);

}
//---------------------------------------------------------------------------
void __fastcall TForm1::m_smoothClick(TObject *Sender)
{
if (m_smooth->Checked)
 dinput->Checked=true;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::dinputClick(TObject *Sender)
{
if (!dinput->Checked)
 m_smooth->Checked=false;
if (dinput->Checked)
 { nopar->Checked=false;
   noacc->Checked=false;
   nospd->Checked=false;
 }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::nomtexClick(TObject *Sender)
{
if (nomtex->Checked)
maxtmu2->Checked=false;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::maxtmu2Click(TObject *Sender)
{
if (maxtmu2->Checked)
nomtex->Checked=false;

}
//---------------------------------------------------------------------------



void __fastcall TForm1::startClick(TObject *Sender)
{
   cmdcalc();
   fh=FileCreate(ExtractFilePath(Application->ExeName)+"ezStart.ini");
   FileClose(fh);
   fh=FileOpen(ExtractFilePath(Application->ExeName)+"ezStart.ini",fmOpenWrite);
   FileWrite(fh,cmd.c_str(),cmd.Length());
   FileClose(fh);

WinExec(cmd.c_str(),SW_SHOW);
Close();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::nosoundClick(TObject *Sender)
{
wavonly->Enabled=!nosound->Checked;
snoforceformat->Enabled=!nosound->Checked;
primarysound->Enabled=!nosound->Checked;
nomp3->Enabled=!nosound->Checked;
}
//---------------------------------------------------------------------------


void __fastcall TForm1::verClick(TObject *Sender)
{
checks();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::checks()
{
if (ver->Items->Strings[ver->ItemIndex]=="OpenGL")
 { gl=1;
   particles->Text=4096;
 }
else
 { gl=0;
   particles->Text=2048;
 }

vmode->Enabled=gl;
cmode->Enabled=gl;
bpp->Enabled=gl;
freq->Enabled=gl;
no24bit->Enabled=gl;
forcetex->Enabled=gl;
current->Enabled=gl;
nohwgamma->Enabled=gl;
nomtex->Enabled=gl;
maxtmu2->Enabled=gl;
dibonly->Enabled=!gl;
}

//---------------------------------------------------------------------------

void __fastcall TForm1::cmdcalc()
{
if (ver->Items->Strings[ver->ItemIndex]=="OpenGL")
 cmd="ezquake-gl.exe ";
else
 cmd="ezquake.exe ";

if (ded->Checked && ded->Enabled) cmd=cmd+"-dedicated ";
if (gamedirs->Text!="" && gamedirs->Enabled) cmd=cmd+"-game "+gamedirs->Text+" ";
if (port->Text!="" && port->Enabled) cmd=cmd+"-port "+port->Text+" ";
if (srvcfg->Text!="" && srvcfg->Enabled) cmd=cmd+"+exec "+srvcfg->Text+" ";
if (mem->Text.ToInt()!=16 && mem->Enabled) cmd=cmd+"-mem "+mem->Text+" ";
if (democache->Text.ToInt()!=0 && democache->Enabled) cmd=cmd+"-democache "+democache->Text.ToInt()*1024+" ";
if (zone->Text.ToInt()!=128 && zone->Enabled) cmd=cmd+"-zone "+zone->Text+" ";
if (conbuf->Text.ToInt()!=64 && conbuf->Enabled) cmd=cmd+"-conbufsize "+conbuf->Text+" ";
if (condebug->Checked && condebug->Enabled) cmd=cmd+"-condebug ";

if (ver->Items->Strings[ver->ItemIndex]=="OpenGL")
{
if (vmode->Text!="" && vmode->Enabled) cmd=cmd+"-width "+vmode->Text.SubString(1,vmode->Text.Pos("x")-1)+" -height "+vmode->Text.SubString(vmode->Text.Pos("x")+1,255)+" ";
if (cmode->Text!="" && cmode->Enabled) cmd=cmd+"-conwidth "+cmode->Text.SubString(1,cmode->Text.Pos("x")-1)+" -conheight "+cmode->Text.SubString(cmode->Text.Pos("x")+1,255)+" ";
if (bpp->Text!="" && bpp->Enabled) cmd=cmd+"-bpp "+bpp->Text+" ";
if (freq->Text!="" && freq->Enabled) cmd=cmd+"+set vid_displayfrequency "+freq->Text+" ";
if (no24bit->Checked && no24bit->Enabled) cmd=cmd+"-no24bit ";
if (forcetex->Checked && forcetex->Enabled) cmd=cmd+"-forceTextureReload ";
if (current->Checked && current->Enabled) cmd=cmd+"-current ";
if (nohwgamma->Checked && nohwgamma->Enabled) cmd=cmd+"-nohwgamma ";
if (nomtex->Checked && nomtex->Enabled) cmd=cmd+"-nomtex ";
if (maxtmu2->Checked && maxtmu2->Enabled) cmd=cmd+"-maxtmu2 ";
}
else
{
if (dibonly->Checked && dibonly->Enabled) cmd=cmd+"-dibonly ";
}
if (window->Checked && window->Enabled) cmd=cmd+"-window ";
if (particles->Text.ToInt()!=2048 && gl==0 && particles->Enabled)
 cmd=cmd+"-particles "+particles->Text+" ";
else if (particles->Text.ToInt()!=4096 && gl==1 && particles->Enabled)
 cmd=cmd+"-particles "+particles->Text+" ";
if (skhz->Text!="" && skhz->Enabled) cmd=cmd+"+set s_khz "+skhz->Text+" ";
if (nosound->Checked && nosound->Enabled) cmd=cmd+"-nosound ";
if (snoforceformat->Checked && snoforceformat->Enabled) cmd=cmd+"-snoforceformat ";
if (wavonly->Checked && wavonly->Enabled) cmd=cmd+"-wavonly ";
if (primarysound->Checked && primarysound->Enabled) cmd=cmd+"-primarysound ";
if (nomp3->Checked && nomp3->Enabled) cmd=cmd+"-nomp3volumectrl ";
if (cdaudio->Checked && cdaudio->Enabled) cmd=cmd+"-cdaudio ";


if (nomouse->Checked && nomouse->Enabled) cmd=cmd+"-nomouse ";
if (dinput->Checked && dinput->Enabled) cmd=cmd+"-dinput ";
if (m_smooth->Checked && m_smooth->Enabled) cmd=cmd+"-m_smooth ";
if (mw_hook->Checked && mw_hook->Enabled) cmd=cmd+"-m_mwhook ";
if (nopar->Checked && nopar->Enabled) cmd=cmd+"-noforcemparms ";
if (noacc->Checked && noacc->Enabled) cmd=cmd+"-noforcemaccel ";
if (nospd->Checked && nospd->Enabled) cmd=cmd+"-noforcemspd ";

if (ruleset->Text!="" && ruleset->Enabled) cmd=cmd+"-ruleset "+ruleset->Text+" ";
if (nohwtimer->Checked && nohwtimer->Enabled) cmd=cmd+"-nohwtimer ";
//if (noconfirmquit->Checked && noconfirmquit->Enabled) cmd=cmd+"+set cl_confirmquit 0 ";
if (noscripts->Checked && noscripts->Enabled) cmd=cmd+"-noscripts ";
if (indphys->Checked && indphys->Enabled) cmd=cmd+"+set cl_independentPhysics 1 ";
if (allowmulty->Checked && allowmulty->Enabled) cmd=cmd+" -allowmultiple ";
if (clcfg->Text!="" && clcfg->Enabled) cmd=cmd+"+cfg_load "+clcfg->Text+" ";
if (other->Text!="" && other->Enabled) cmd=cmd+other->Text+" ";

cmd.Delete(cmd.Length(),1);
}

void __fastcall TForm1::nomouseClick(TObject *Sender)
{
dinput->Enabled=!nomouse->Checked;
m_smooth->Enabled=!nomouse->Checked;
mw_hook->Enabled=!nomouse->Checked;
noacc->Enabled=!nomouse->Checked;
nopar->Enabled=!nomouse->Checked;
nospd->Enabled=!nomouse->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::noparClick(TObject *Sender)
{
if (nopar->Checked)
 { noacc->Checked=0;
   nospd->Checked=0;
   dinput->Checked=0;
 }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::windowClick(TObject *Sender)
{
if (window->Checked)
current->Checked=0;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::currentClick(TObject *Sender)
{
if (current->Checked)
window->Checked=0;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::noaccClick(TObject *Sender)
{
if (noacc->Checked)
 { nopar->Checked=0;
   dinput->Checked=0;
 }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::nospdClick(TObject *Sender)
{
if (nospd->Checked)
 { nopar->Checked=0;
   dinput->Checked=0;
 }
}
//---------------------------------------------------------------------------


void __fastcall TForm1::dedClick(TObject *Sender)
{
   nomouse->Enabled=!ded->Checked;
   dinput->Enabled=!ded->Checked;
   m_smooth->Enabled=!ded->Checked;
   mw_hook->Enabled=!ded->Checked;
   nopar->Enabled=!ded->Checked;
   noacc->Enabled=!ded->Checked;
   nospd->Enabled=!ded->Checked;
   vmode->Enabled=!ded->Checked;
   cmode->Enabled=!ded->Checked;
   bpp->Enabled=!ded->Checked;
   freq->Enabled=!ded->Checked;
   particles->Enabled=!ded->Checked;
   dibonly->Enabled=!ded->Checked;
   window->Enabled=!ded->Checked;
   current->Enabled=!ded->Checked;
   no24bit->Enabled=!ded->Checked;
   forcetex->Enabled=!ded->Checked;
   nohwgamma->Enabled=!ded->Checked;
   nomtex->Enabled=!ded->Checked;
   maxtmu2->Enabled=!ded->Checked;
   skhz->Enabled=!ded->Checked;
   nosound->Enabled=!ded->Checked;
   snoforceformat->Enabled=!ded->Checked;
   wavonly->Enabled=!ded->Checked;
   primarysound->Enabled=!ded->Checked;
   nomp3->Enabled=!ded->Checked;
   cdaudio->Enabled=!ded->Checked;
   ruleset->Enabled=!ded->Checked;
   clcfg->Enabled=!ded->Checked;
   nohwtimer->Enabled=!ded->Checked;
//   noconfirmquit->Enabled=!ded->Checked;
   ver->Enabled =!ded->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::loadClick(TObject *Sender)
{
char *buf;

//if (InputQuery(Caption,"Enter bat file name:",bat))
if (fbat->Text!="")
 bat=fbat->Text;
if (FileExists(ExtractFilePath(Application->ExeName)+bat))
 { fh=FileOpen(ExtractFilePath(Application->ExeName)+bat,fmOpenRead);
   fl=FileSeek(fh,0,2);
   FileSeek(fh,0,0);
   buf = new char [fl+1];
   FileRead(fh, buf, fl);
   FileClose(fh);

   cmd.sprintf(buf);
   cmd.Delete(fl+1,999);
   cmd=cmd.LowerCase();
   cmd.Delete(cmd.Pos("\n"),1);
   cmd.Delete(cmd.Pos("\r"),1);

clear();

while (cmd.Pos("  "))
 { cmd.Delete(cmd.Pos("  "),1);
 }

if (cmd.SubString(cmd.Length(),1)!=" ")
 cmd=cmd+" ";

   if (cmd.Pos("ezquake-gl.exe "))
    { ver->ItemIndex=ver->Items->IndexOf("OpenGL");
      cmd=cmd.Delete(1,cmd.Pos("ezquake-gl.exe ")+13);
    }
   if (cmd.Pos("ezquake-gl "))
    { ver->ItemIndex=ver->Items->IndexOf("OpenGL");
      cmd=cmd.Delete(1,cmd.Pos("ezquake-gl ")+9);
    }
   if (cmd.Pos("ezquake.exe "))
    { ver->ItemIndex=ver->Items->IndexOf("Software");
      cmd=cmd.Delete(1,cmd.Pos("ezquake.exe ")+10);
    }
   if (cmd.Pos("ezquake "))
    { ver->ItemIndex=ver->Items->IndexOf("Software");
      cmd=cmd.Delete(1,cmd.Pos("ezquake ")+6);
    }
   if (cmd.Pos(" -dedicated"))
    { ded->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -dedicated"),11);
    }
   if (cmd.Pos(" -condebug"))
    { condebug->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -condebug"),10);
    }
   if (cmd.Pos(" -nomouse"))
    { nomouse->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -nomouse"),9);
    }
   if (cmd.Pos(" -dinput"))
    { dinput->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -dinput"),8);
    }
   if (cmd.Pos(" -m_smooth"))
    { m_smooth->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -m_smooth"),10);
    }
   if (cmd.Pos(" -m_mwhook"))
    { mw_hook->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -m_mwhook"),9);
    }
   if (cmd.Pos(" -noforcemparms"))
    { nopar->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -noforcemparms"),15);
    }
   if (cmd.Pos(" -noforcemspd"))
    { nospd->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -noforcemspd"),13);
    }
   if (cmd.Pos(" -noforcemaccel"))
    { noacc->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -noforcemaccel"),15);
    }
   if (cmd.Pos(" -dibonly"))
    { dibonly->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -dibonly"),9);
    }
   if (cmd.Pos(" -window"))
    { window->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -window"),8);
    }
   if (cmd.Pos(" -current"))
    { current->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -current"),9);
    }
   if (cmd.Pos(" -no24bit"))
    { no24bit->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -no24bit"),9);
    }
   if (cmd.Pos(" -forcetexturereload"))
    { forcetex->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -forcetexturereload"),20);
    }
   if (cmd.Pos(" -nohwgamma"))
    { nohwgamma->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -nohwgamma"),11);
    }
   if (cmd.Pos(" -nomtex"))
    { nomtex->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -nomtex"),8);
    }
   if (cmd.Pos(" -maxtmu2"))
    { maxtmu2->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -maxtmu2"),9);
    }
   if (cmd.Pos(" -nosound"))
    { nosound->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -nosound"),9);
    }
   if (cmd.Pos(" -snoforceformat"))
    { snoforceformat->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -snoforceformat"),16);
    }
   if (cmd.Pos(" -wavonly"))
    { wavonly->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -wavonly"),9);
    }
   if (cmd.Pos(" -primarysound"))
    { primarysound->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -primarysound"),14);
    }
   if (cmd.Pos(" -nomp3volumectrl"))
    { nomp3->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -nomp3volumectrl"),17);
    }
   if (cmd.Pos(" -cdaudio"))
    { cdaudio->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -cdaudio"),9);
    }
   if (cmd.Pos(" -nohwtimer"))
    { nohwtimer->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -nohwtimer"),11);
    }
//   if (cmd.Pos(" +set cl_confirmquit 0"))
//    { noconfirmquit->Checked=1;
//      cmd=cmd.Delete(cmd.Pos(" +set cl_confirmquit 0"),22);
//    }
   if (cmd.Pos(" -noscripts"))
    { noscripts->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -noscripts"),11);
    }
   if (cmd.Pos(" +set cl_independentphysics 1"))
    { indphys->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" +set cl_independentphysics 1"),29);
    }
   if (cmd.Pos(" -allowmultiple"))
    { allowmulty->Checked=1;
      cmd=cmd.Delete(cmd.Pos(" -allowmultiple"),15);
    }

   if (cmd.Pos(" +gamedir "))
    { tmp=cmd.SubString(cmd.Pos(" +gamedir ")+10,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      gamedirs->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" +gamedir "),tmp.Length()+10);
    }
   if (cmd.Pos(" -game "))
    { tmp=cmd.SubString(cmd.Pos(" -game ")+7,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      gamedirs->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" -game "),tmp.Length()+7);
    }
   if (cmd.Pos(" +exec "))
    { tmp=cmd.SubString(cmd.Pos(" +exec ")+7,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      srvcfg->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" +exec "),tmp.Length()+7);
    }
   if (cmd.Pos(" -port "))
    { tmp=cmd.SubString(cmd.Pos(" -port ")+7,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      port->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" -port "),tmp.Length()+7);
    }
   if (cmd.Pos(" -heapsize "))
    { tmp=cmd.SubString(cmd.Pos(" -heapsize ")+11,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      mem->Text=tmp.ToInt()/1024;
      cmd=cmd.Delete(cmd.Pos(" -heapsize "),tmp.Length()+11);
    }
   if (cmd.Pos(" -mem "))
    { tmp=cmd.SubString(cmd.Pos(" -mem ")+6,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      mem->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" -mem "),tmp.Length()+6);
    }
   if (cmd.Pos(" -democache "))
    { tmp=cmd.SubString(cmd.Pos(" -democache ")+12,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      democache->Text=tmp.ToInt()/1024;
      cmd=cmd.Delete(cmd.Pos(" -democache "),tmp.Length()+12);
    }
   if (cmd.Pos(" -zone "))
    { tmp=cmd.SubString(cmd.Pos(" -zone ")+7,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      zone->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" -zone "),tmp.Length()+7);
    }
   if (cmd.Pos(" -conbufsize "))
    { tmp=cmd.SubString(cmd.Pos(" -conbufsize ")+13,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      conbuf->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" -conbufsize "),tmp.Length()+13);
    }
   if (cmd.Pos(" -width ") && cmd.Pos(" -height "))
    { tmp=cmd.SubString(cmd.Pos(" -width ")+8,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      vmode->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" -width "),tmp.Length()+8);

      tmp=cmd.SubString(cmd.Pos(" -height ")+9,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      vmode->Text=vmode->Text+"x"+tmp;
      cmd=cmd.Delete(cmd.Pos(" -height "),tmp.Length()+9);
    }
   if (cmd.Pos(" -conwidth ") && cmd.Pos(" -conheight "))
    { tmp=cmd.SubString(cmd.Pos(" -conwidth ")+11,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      cmode->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" -conwidth "),tmp.Length()+11);

      tmp=cmd.SubString(cmd.Pos(" -conheight ")+12,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      cmode->Text=cmode->Text+"x"+tmp;
      cmd=cmd.Delete(cmd.Pos(" -conheight "),tmp.Length()+12);
    }
   if (cmd.Pos(" -bpp "))
    { tmp=cmd.SubString(cmd.Pos(" -bpp ")+6,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      bpp->ItemIndex=bpp->Items->IndexOf(tmp);
      cmd=cmd.Delete(cmd.Pos(" -bpp "),tmp.Length()+6);
    }
   if (cmd.Pos(" +set vid_displayfrequency "))
    { tmp=cmd.SubString(cmd.Pos(" +set vid_displayfrequency ")+27,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      freq->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" +set vid_displayfrequency "),tmp.Length()+27);
    }
   if (cmd.Pos(" -particles "))
    { tmp=cmd.SubString(cmd.Pos(" -particles ")+12,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      particles->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" -particles "),tmp.Length()+12);
    }
   if (cmd.Pos(" +set s_khz "))
    { tmp=cmd.SubString(cmd.Pos(" +set s_khz ")+12,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      skhz->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" +set s_khz "),tmp.Length()+12);
    }
   if (cmd.Pos(" -ruleset "))
    { tmp=cmd.SubString(cmd.Pos(" -ruleset ")+10,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      ruleset->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" -ruleset "),tmp.Length()+10);
    }
   if (cmd.Pos(" +cfg_load "))
    { tmp=cmd.SubString(cmd.Pos(" +cfg_load ")+11,999);
      tmp=tmp.SubString(1,tmp.Pos(" ")-1);
      clcfg->Text=tmp;
      cmd=cmd.Delete(cmd.Pos(" +cfg_load "),tmp.Length()+11);
    }
    cmd=TrimLeft(cmd);
    cmd=TrimRight(cmd);
    other->Text=cmd;
 }
else
 MessageBox(Application->Handle,"Batch file not found in current directory","ezStart",MB_OK+MB_ICONWARNING);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::saveClick(TObject *Sender)
{
if (fbat->Text=="")
 MessageBox(Application->Handle,"You must enter bat file name","ezStart",MB_OK+MB_ICONWARNING);
else
 bat=fbat->Text;
if (bat!="")
 { cmdcalc();
   if (bat.SubString(bat.Length()-3,4)!=".bat")
    { bat=bat+".bat"; fbat->Text=bat; }
   fh=FileCreate(ExtractFilePath(Application->ExeName)+bat);
   FileClose(fh);
   fh=FileOpen(ExtractFilePath(Application->ExeName)+bat,fmOpenWrite);
   FileWrite(fh,("@start "+cmd).c_str(),cmd.Length()+7);
   FileClose(fh);

fbat->Clear();
s=ExtractFilePath(Application->ExeName)+"*.bat";
  if (FindFirst(s, 63, sr) == 0)
    do
    { if (sr.Name!="." && sr.Name!=".." && fbat->Items->IndexOf(sr.Name)==-1)
        fbat->Items->Add(sr.Name);
    } while (FindNext(sr) == 0);
  FindClose(sr);

 }

}
//---------------------------------------------------------------------------


void __fastcall TForm1::langClick(TObject *Sender)
{
if (lang->ItemIndex==1)
 Application->ShowHint=1;
else
 Application->ShowHint=0;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::clear()
{
ver->ItemIndex=0;
ded->Checked=0;
gamedirs->Text="";
srvcfg->Text="";
port->Text="";
mem->Text=16;
democache->Text="0";
zone->Text="128";
conbuf->Text="64";
condebug->Checked=0;
nomouse->Checked=0;
dinput->Checked=0;
m_smooth->Checked=0;
mw_hook->Checked=0;
nopar->Checked=0;
noacc->Checked=0;
nospd->Checked=0;
vmode->Text="";
cmode->Text="";
bpp->ItemIndex=-1;
freq->Text="";
particles->Text=2048+2048*(ver->Items->IndexOf("OpenGL")+1);
dibonly->Checked=0;
window->Checked=0;
current->Checked=0;
no24bit->Checked=0;
forcetex->Checked=0;
nohwgamma->Checked=0;
nomtex->Checked=0;
maxtmu2->Checked=0;
skhz->Text="";
nosound->Checked=0;
snoforceformat->Checked=0;
wavonly->Checked=0;
primarysound->Checked=0;
nomp3->Checked=0;
cdaudio->Checked=0;
ruleset->Text="";
clcfg->Text="";
nohwtimer->Checked=0;
//noconfirmquit->Checked=0;
other->Text="";
}
//---------------------------------------------------------------------------

void __fastcall TForm1::clearbClick(TObject *Sender)
{
clear();
}
//---------------------------------------------------------------------------


void __fastcall TForm1::FormShow(TObject *Sender)
{
if (FileExists(ExtractFilePath(Application->ExeName)+"ezStart.ini"))
 { bat="ezStart.ini";
   TForm1::loadClick(Sender);
   bat="";
 }
        
}
//---------------------------------------------------------------------------

void __fastcall TForm1::crlnkClick(TObject *Sender)
{
int t;
AnsiString lnk_target,create_path;
TRegistry *reg = new TRegistry();
WCHAR buffer[256];
MessageBeep(MB_ICONEXCLAMATION);
reg->RootKey = HKEY_CURRENT_USER;
reg->OpenKey("\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",false);
cmdcalc();
switch (lnkpath->ItemIndex)
 { case 0 : create_path=reg->ReadString("Desktop"); break;
   case 1 : create_path=reg->ReadString("AppData")+"\\Microsoft\\Internet Explorer\\Quick Launch"; break;
   case 2 : create_path=reg->ReadString("Start Menu"); break;
   default : break;
 }
lnk_target=ExtractFilePath(Application->ExeName);
if (ver->Items->Strings[ver->ItemIndex]=="OpenGL")
 lnk_target=lnk_target+"ezquake-gl.exe";
else
 lnk_target=lnk_target+"ezquake.exe";
CoInitialize(NULL);
CreateShortcut((create_path+"\\ezQuake.lnk").WideChar(buffer,255),\
                lnk_target.c_str(),\
                (ExtractFilePath(Application->ExeName)).c_str(),\
                (cmd.SubString(cmd.Pos(".exe")+5,cmd.Length())).c_str(),\
                NULL);
CoUninitialize();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::lnkpathChange(TObject *Sender)
{
crlnk->Enabled=(lnkpath->ItemIndex!=-1);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::CreateShortcut(LPCWSTR pwzShortCutFileName,
                    LPCTSTR pszPathAndFileName,
                    LPCTSTR pszWorkingDirectory,
                    LPCTSTR pszArguments,
                    WORD wHotKey) {
IShellLink * pSL;
IPersistFile * pPF;
HRESULT hRes;

hRes = CoCreateInstance(CLSID_ShellLink,0,CLSCTX_INPROC_SERVER,IID_IShellLink,(LPVOID*)&pSL);
 if( SUCCEEDED(hRes) )
  { hRes = pSL->SetPath(pszPathAndFileName);
    if( SUCCEEDED(hRes) )
     { hRes = pSL->SetArguments(pszArguments);
       if( SUCCEEDED(hRes) )
        { hRes = pSL->SetWorkingDirectory(pszWorkingDirectory);
          if( SUCCEEDED(hRes) )
           { hRes = pSL->SetHotkey(wHotKey);
             if( SUCCEEDED(hRes) )
              { hRes = pSL->QueryInterface(IID_IPersistFile,(LPVOID*)&pPF);
                if( SUCCEEDED(hRes) )
                 { hRes = pPF->Save(pwzShortCutFileName,TRUE);
                   pPF->Release();
                 }
              }
           }
        }
     }
  }
pSL->Release();

}
//---------------------------------------------------------------------------

void __fastcall TForm1::fbatChange(TObject *Sender)
{
load->Enabled=(fbat->Text.Length()>0 && FileExists(ExtractFilePath(Application->ExeName)+fbat->Text) );
save->Enabled=(fbat->Text.Length()>0);        
}
//---------------------------------------------------------------------------

