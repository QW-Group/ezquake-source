
<ul>
<li><a href="#%23no+group%23">#No Group#</a></li>
<li><a href="#config+management">Config Management</a></li>
<li><a href="#system+settings">System Settings</a></li>
</ul>

<h2 id="%23no+group%23">#No Group#</h2>

<h3 id="allow_f_cmdline">allow_f_cmdline</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="allow_f_system">allow_f_system</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="auth_validate">auth_validate</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>If you don't want your client to validate other FuhQuake clients.</td></tr><tr><td>1</td><td>which performs the validation (but you need to use 'validate_clients'</td></tr></tbody></table><p class="remarks">


to see the results after a 'fuh_version').  If you have 
'auth_warninvalid 1' and someone gives a dirty hash in a version response 
(because they are using a hacked client that can't work out the right 
response), FuhQuake will print something like 
'Warning Invalid Client: playername (userid)' in your console ('auth_warninvalid 0' is default).

</p>
<h3 id="auth_warninvalid">auth_warninvalid</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Check auth_validate.

</p><p class="range">Range: 0/1</p><p class="remarks">
</p>
<h3 id="cl_cmdline">cl_cmdline</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_crypt_rcon">cl_crypt_rcon</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_loadfragfiles">cl_loadFragfiles</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Enable displaying team fortress related statistics in the scoreboard (flag touches, steals, caps, etc)</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="cl_parsefrags">cl_parseFrags</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Enable displaying team fortress related statistics in the scoreboard (flag touches, steals, caps, etc)</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_warncmd">cl_warncmd</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable the unknown command messages.</td></tr><tr><td>1</td><td>Enable the uknown command messages.</td></tr></tbody></table><p class="remarks">

Note: Not saved to config with cfg_save command.

</p>
<h3 id="developer">developer</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_strings">gl_strings</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="keymap_name">keymap_name</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"Default"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="localid">localid</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="mapname">mapname</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="password">password</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="qport">qport</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="rcon_address">rcon_address</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="rcon_password">rcon_password</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="registered">registered</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="spectator_password">spectator_password</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description"></p><p class="remarks"></p>
<h2 id="config+management">Config Management</h2>

<h3 id="cfg_backup">cfg_backup</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>No backup of your config.</td></tr><tr><td>1</td><td>Backs up your old config before overwriting it with cfg_save.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cfg_legacy_exec">cfg_legacy_exec</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1/2/3</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not execute config.cfg and frontend.cfg in gamedir ever.</td></tr><tr><td>1</td><td>Execute config.cfg and frontend.cfg in gamedir (unless gamedir = qw) like </td></tr><tr><td>2</td><td>Execute config.cfg and frontend.cfg even if gamedir = qw.</td></tr><tr><td>3</td><td>Execute config.cfg and frontend.cfg even if gamedir = qw.  </td></tr></tbody></table><p class="remarks">

qw 2.33.


If there is no config.cfg or frontend.cfg in gamedir, and gamedir != qw then try executing 
config.cfg and frontend.cfg in qw.

</p>
<h3 id="cfg_legacy_write">cfg_legacy_write</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Will not write config.cfg.</td></tr><tr><td>1</td><td>Will write a small config.cfg to your gamedir when quitting just like qw </td></tr></tbody></table><p class="remarks">

2.33.

</p>
<h3 id="cfg_save_aliases">cfg_save_aliases</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Won't save aliases.</td></tr><tr><td>1</td><td>cfg_save saves aliases.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cfg_save_binds">cfg_save_binds</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Won't save binds.</td></tr><tr><td>1</td><td>Toggles whether cfg_save saves binds.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cfg_save_cmdline">cfg_save_cmdline</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Won't save command line.</td></tr><tr><td>1</td><td>Writes your command line in configs made with cfg_save (commented out of </td></tr></tbody></table><p class="remarks">

course).

</p>
<h3 id="cfg_save_cmds">cfg_save_cmds</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Won't save commands.</td></tr><tr><td>1</td><td>Saves commands.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cfg_save_cvars">cfg_save_cvars</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Won't save variables.</td></tr><tr><td>1</td><td>Saves variables.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cfg_save_sysinfo">cfg_save_sysinfo</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Not implemented yet.

</p><p class="range">Range: 0/1</p><p class="remarks">
</p>
<h3 id="cfg_save_unchanged">cfg_save_unchanged</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Makes cfg_save only write variables that are not default valued to the </td></tr><tr><td>1</td><td>Makes cfg_save write all variables to the config file.</td></tr></tbody></table><p class="remarks">config file.


</p>
<h3 id="cfg_save_userinfo">cfg_save_userinfo</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description">
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Makes cfg_save not save userinfo variables.</td></tr><tr><td>1</td><td>Makes cfg_save save all userinfo variables except spectator/name/topcolor/bottomcolor/skin/team .</td></tr><tr><td>2</td><td>Makes cfg_save save all userinfo variables like name/team/teamcolor/rate/msg/skin/w_switch/b_switch/... .</td></tr></tbody></table><p class="remarks">



Note :'cfg_save_userinfo 1' is best for teamfortress so you don't get kicked for changing bottom color.
Note : cfg_save will never save the password variable, even though technically it is a userinfo variable.

</p>
<h2 id="system+settings">System Settings</h2>

<h3 id="sys_disablewinkeys">sys_disableWinKeys</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Enable.</td></tr><tr><td>1</td><td>Disable winkeys from alt-tabbing you (only works in NT/2K/XP, and only when FuhQuake is in focus).</td></tr></tbody></table><p class="remarks">


Note:Windows keys are now bindable (LWINKEY, RWINKEY, WINKEY, POPUPMENU). Obviously only useful on NT/2K/XP with sys_disableWinKeys 1 (and possibely useful in linux too!).

</p>
<h3 id="sys_highpriority">sys_highpriority</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
-1 = Sets process to idle/low priority.
</p><p class="range">Range: -1/0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Sets process to normal priority.</td></tr><tr><td>1</td><td>Sets process to high priority.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="sys_inactivesleep">sys_inactiveSleep</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Prevent freeing of CPU when FuhQuake is minimised or not in focus.</td></tr><tr><td>1</td><td>Enable inactive sleeping.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="sys_yieldcpu">sys_yieldcpu</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>FuhQuake eat more cpu.</td></tr><tr><td>1</td><td>Give more cpu for other programs.</td></tr></tbody></table><p class="remarks">



</p>