
<ul>
<li><a href="#chat+settings">Chat Settings</a></li>
<li><a href="#console+settings">Console Settings</a></li>
<li><a href="#status+bar+and+scoreboard">Status Bar and Scoreboard</a></li>
</ul>

<h2 id="chat+settings">Chat Settings</h2>

<h3 id="auth_viewcrc">auth_viewcrc</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>FuhQuake hides the authentication crc in f_version responses.</td></tr><tr><td>1</td><td>FuhQuake show you the authentication crc in f_version responses.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="cl_chatsound">cl_chatsound</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_floodprot">cl_floodprot</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable floodprot.</td></tr><tr><td>1</td><td>Enable floodprot.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_fp_messages">cl_fp_messages</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"4"</span></p><p class="description">
This variable is used in conjunction with the variable &quot;cl_fp_persecond&quot; to
define when the floodprot protection should be triggered (if &quot;cl_floodprot&quot;
is set to &quot;1&quot;).

</p><p class="range">Range: 0..10</p><p class="remarks">
</p>
<h3 id="cl_fp_persecond">cl_fp_persecond</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"4"</span></p><p class="description">

This variable is used in conjunction with the variable &quot;cl_fp_messages&quot; to
define when the floodprot protection should be triggered (if &quot;cl_floodprot&quot;
is set to &quot;1&quot;).

</p><p class="range">Range: 0..10</p><p class="remarks"></p>
<h3 id="cl_nofake">cl_nofake</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>No unfaking.</td></tr><tr><td>1</td><td>Unfake other players messages.</td></tr><tr><td>2</td><td>Unfake messages from enemies.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_parsefunchars">cl_parseFunChars</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>You can use Ocrana LED's by hand right out of the console and chat anonymously through &quot;$\&quot; without showing your name (chat messages that are seen without your name at the beginning).</td></tr></tbody></table><p class="remarks">


Full list:
$R - red lamp
$G - green lamp
$B - blue lamp
$Y - yellow lamp
$\ - carridge return
$( - big left bracket
$= - big equal sign
$) - big right bracket
$. - red middle dot
$, - white dot (names only)
$&lt; - small left bracket
$- - small equal sign
$&gt; - small right bracket
$a - big grey block
$: - line feed
$b - filled red block
$d - right pointing red arrow
$[ - gold left square bracket
$] - gold right square bracket
$^ - white ^ (names only)
^x - red x (names only)
$0-9 - yellow number
$xyy - char with hex code yy
(In order to use the lamps, you'll need the Ocrana pak).

</p>
<h3 id="cl_parsesay">cl_parseSay</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable %-macros (%a %h %b etc...)</td></tr><tr><td>1</td><td>You can use macros such as %a %h %l (like in Qizmo).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_parsewhitetext">cl_parseWhiteText</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
Convert text between { and } to white or not in chat/team chat.
</p><p class="range">Range: 1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Convert none.</td></tr><tr><td>1</td><td>Convert all (chat and team chat).</td></tr><tr><td>2</td><td>Convert team chat only.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="ignore_flood">ignore_flood</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>Ignores repeated 'say' and spectator messages.</td></tr><tr><td>2</td><td>Ignores repeated 'say', 'say_team' and spectator messages.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="ignore_flood_duration">ignore_flood_duration</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"4"</span></p><p class="description">
You can change the 4 second cooldown with the 'ignore_flood_duration' variable.

</p><p class="remarks">
</p>
<h3 id="ignore_mode">ignore_mode</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

Someone is on your ignore list, you won't see any mm1 messages from them 
(even if they are a spec).
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Ignore say only.</td></tr><tr><td>1</td><td>Ignore say_team too.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="ignore_opponents">ignore_opponents</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Not ignore opponent team.</td></tr><tr><td>1</td><td>Ignore opponent team.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="ignore_qizmo_spec">ignore_qizmo_spec</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="ignore_spec">ignore_spec</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off</td></tr><tr><td>1</td><td>Ignore spec chat unless you are a spectator.</td></tr><tr><td>2</td><td>Ignore spec chat even if you are a spectator.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="msg_filter">msg_filter</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h2 id="console+settings">Console Settings</h2>

<h3 id="cl_chatmode">cl_chatmode</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description">
Console chat mode.
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Text in the console is always treated as a command and in order to chat you have to use messagemode/messagemode2 or use the say/say_team commands.</td></tr><tr><td>1</td><td>Everything you type in the console goes into chat, in order to issue a command, prefix it with a slash (/).</td></tr><tr><td>2</td><td>If the first word in the line is a command, it is executed, otherwise the line is sent as chat.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="cl_newcompletion">cl_newCompletion</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_clearnotify">con_clearnotify</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Messages stay even when you toggle console.</td></tr><tr><td>1</td><td>Messages won't say when you toggle console.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="con_notifylines">con_notifylines</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"4"</span></p><p class="description">
This variable sets the number of notify lines (default 4, max 20) to be used at the top of the screen.
</p><p class="range">Range: 0..20</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Use no notify lines at all.</td></tr><tr><td>1</td><td>Use one notify line.</td></tr><tr><td>20</td><td>Use 20 notify lines.</td></tr></tbody></table><p class="remarks">


...


</p>
<h3 id="con_notifytime">con_notifytime</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description">
How long console messages stay on screen.

</p><p class="remarks">
</p>
<h3 id="con_shift">con_shift</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_sound_mm1_file">con_sound_mm1_file</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"misc/talk.wav"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_sound_mm1_volume">con_sound_mm1_volume</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_sound_mm2_file">con_sound_mm2_file</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"misc/talk.wav"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_sound_mm2_volume">con_sound_mm2_volume</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_sound_other_file">con_sound_other_file</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"misc/talk.wav"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_sound_other_volume">con_sound_other_volume</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_sound_spec_file">con_sound_spec_file</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"misc/talk.wav"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_sound_spec_volume">con_sound_spec_volume</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_timestamps">con_timestamps</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="con_wordwrap">con_wordwrap</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable word wrapping.</td></tr><tr><td>1</td><td>Enable word wrapping.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="cvar_viewdefault">cvar_viewdefault</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
When you type a cvar name into console (like 'gl_gamma' or 'r_rocketlight'), FuhQuake will tell you the default value of the cvar.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>On.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_consolefont">gl_consolefont</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"original"</span></p><p class="description">
In older versions of FuhQuake (v0.30 and lower), FuhQuake looked for an image called charset.png (or .tga) and if it found it then that was made to be your console font. 
Starting from FuhQuake v0.31, you can change your console font from within FuhQuake. 
Put all your charset images in qw/textures/charsets/*.png (and *.tga) and use /gl_loadcharset XXX to load XXX.png (or tga). &quot;/gl_consolefont original&quot; will restore the 8bit font in your gfx.wad (this is default).
Note: Loadcharset is shortcut for gl_consolefont.

</p><p class="range">Range: filename</p><p class="remarks">
</p>
<h3 id="gl_smoothfont">gl_smoothfont</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

Smoothens out the font (which = good). But in some cases the font wasn't designed to be smoothened (sometimes the case of custom console fonts etc).
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>On.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="log_dir">log_dir</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">
The logging dir.

</p><p class="remarks">
</p>
<h3 id="log_readable">log_readable</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>Will convert all non printable characters to printable characters in your log. This makes your log readable because all the weird ascii characters that usually occur because of different coloured text in FuhQuake (and because of weird symbols - ocrana led's etc) are converted into printable characters.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="scr_conalpha">scr_conalpha</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Transparent.</td></tr><tr><td>1</td><td>Normal.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_consize">scr_consize</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.75"</span></p><p class="description">
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Minimal console height.</td></tr><tr><td>1</td><td>Maximum console height.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_conspeed">scr_conspeed</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1000"</span></p><p class="description">
This variable controls the speed at which the console screen scrolls up and down.

</p><p class="range">Range: 0..9999</p><p class="remarks">
</p>
<h3 id="scr_printspeed">scr_printspeed</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"8"</span></p><p class="description">

This variable controls how fast the text is displayed at the end of the single 
player episodes.

</p><p class="remarks"></p>
<h3 id="x">x</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"$x"</span></p><p class="description"></p><p class="remarks"></p>
<h2 id="status+bar+and+scoreboard">Status Bar and Scoreboard</h2>

<h3 id="cl_hudswap">cl_hudswap</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>The inventory is drawn on the right side of the screen.</td></tr><tr><td>1</td><td>The inventory is drawn on the left side of the screen.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="cl_sbar">cl_sbar</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Use the new transparent HUD.</td></tr><tr><td>1</td><td>Use the old status bar.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_damagestats">r_damagestats</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Displays the amount of damage taken recently on the screen above your armour and health.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="scr_centersbar">scr_centerSbar</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>The hud display will be centered.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_compacthud">scr_compactHud</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1/2/3/4</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Compact huds off.</td></tr><tr><td>1</td><td>Will display armour/health/ammo/weapons *very* compactly.</td></tr><tr><td>2</td><td>Will display armour/health/ammo *very* compactly.</td></tr><tr><td>3</td><td>Displays only health/armour.</td></tr><tr><td>4</td><td>Is just like &quot;2&quot; except it also displays your current weapon's ammo in big numbers.</td></tr></tbody></table><p class="remarks">






</p>
<h3 id="scr_compacthudalign">scr_compactHudAlign</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>determine ammo amounts are aligned to the left.</td></tr><tr><td>1</td><td>determine ammo amounts are aligned to the right.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_drawhfrags">scr_drawHFrags</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Turns frag display off.</td></tr><tr><td>1</td><td>Individual frags.</td></tr><tr><td>2</td><td>Teamfrags in teamplay mode.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="scr_drawvfrags">scr_drawVFrags</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Turns frag display off.</td></tr><tr><td>1</td><td>Individual frags.</td></tr><tr><td>2</td><td>Teamfrags in teamplay mode.</td></tr></tbody></table><p class="remarks">



Note: scr_drawVFrags only does something if your console width is &gt;= 512 and &lt; 640.

</p>
<h3 id="scr_scoreboard_borderless">scr_scoreboard_borderless</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="scr_scoreboard_drawtitle">scr_scoreboard_drawtitle</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable scoreboard title.</td></tr><tr><td>1</td><td>Enable scoreboard title.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_scoreboard_fillalpha">scr_scoreboard_fillalpha</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.7"</span></p><p class="description">
Change scoreboard fillalpha.

</p><p class="range">Range: 0..1</p><p class="remarks">
</p>
<h3 id="scr_scoreboard_fillcolored">scr_scoreboard_fillcolored</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description">

Modify scoreboard fillcolour.

</p><p class="remarks"></p>
<h3 id="scr_scoreboard_forcecolors">scr_scoreboard_forcecolors</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>When you overwrite team/enemy color it will use only those colors for skins</td></tr><tr><td>1</td><td>When you overwrite team/enemy color it will use only those colors for skins and scoreboard</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="scr_scoreboard_showfrags">scr_scoreboard_showfrags</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable frags on the scoreboard.</td></tr><tr><td>1</td><td>Enable frags on the scoreboard.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_scoreboard_teamsort">scr_scoreboard_teamsort</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Frag sort.</td></tr><tr><td>1</td><td>Team sort.</td></tr></tbody></table><p class="remarks">



</p>