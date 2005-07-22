  <ul>
    <li><a href="#config-management">Config Management</a></li>
    <li><a href="#system-settings">System Settings</a></li>
    <li><a href="#unsorted-variables">Unsorted Variables</a></li>
  </ul>
<h2 id="config-management">Config Management</h2>
  <h3 id="cfg_backup">cfg_backup&nbsp;<span>[<a href="#cfg_backup">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>No backup of your config.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Backs up your old config before overwriting it with cfg_save.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cfg_legacy_exec">cfg_legacy_exec&nbsp;<span>[<a href="#cfg_legacy_exec">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Do not execute config.cfg and frontend.cfg in gamedir ever.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Execute config.cfg and frontend.cfg in gamedir (unless gamedir = qw) like </td>
      </tr>
      <tr>
        <td>2</td>
        <td>Execute config.cfg and frontend.cfg even if gamedir = qw.</td>
      </tr>
      <tr>
        <td>3</td>
        <td>Execute config.cfg and frontend.cfg even if gamedir = qw.  </td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>

qw 2.33.


If there is no config.cfg or frontend.cfg in gamedir, and gamedir != qw then try executing 
config.cfg and frontend.cfg in qw.


</p>
</div>

  <h3 id="cfg_legacy_write">cfg_legacy_write&nbsp;<span>[<a href="#cfg_legacy_write">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Will not write config.cfg.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Will write a small config.cfg to your gamedir when quitting just like qw </td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>

2.33.


</p>
</div>

  <h3 id="cfg_save_aliases">cfg_save_aliases&nbsp;<span>[<a href="#cfg_save_aliases">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Won't save aliases.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>cfg_save saves aliases.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cfg_save_binds">cfg_save_binds&nbsp;<span>[<a href="#cfg_save_binds">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Won't save binds.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Toggles whether cfg_save saves binds.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cfg_save_cmdline">cfg_save_cmdline&nbsp;<span>[<a href="#cfg_save_cmdline">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Won't save command line.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Writes your command line in configs made with cfg_save (commented out of </td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>

course).


</p>
</div>

  <h3 id="cfg_save_cmds">cfg_save_cmds&nbsp;<span>[<a href="#cfg_save_cmds">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Won't save commands.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Saves commands.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cfg_save_cvars">cfg_save_cvars&nbsp;<span>[<a href="#cfg_save_cvars">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Won't save variables.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Saves variables.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cfg_save_sysinfo">cfg_save_sysinfo&nbsp;<span>[<a href="#cfg_save_sysinfo">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1 or 0 or 0 or 0</span></p>
<div>
  <h4>Description</h4>
  <p>
Not implemented yet.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="cfg_save_unchanged">cfg_save_unchanged&nbsp;<span>[<a href="#cfg_save_unchanged">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>

</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Makes cfg_save only write variables that are not default valued to the </td>
      </tr>
      <tr>
        <td>1</td>
        <td>Makes cfg_save write all variables to the config file.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>config file.



</p>
</div>

  <h3 id="cfg_save_userinfo">cfg_save_userinfo&nbsp;<span>[<a href="#cfg_save_userinfo">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2 or 0 or 0 or 0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Makes cfg_save not save userinfo variables.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Makes cfg_save save all userinfo variables except spectator/name/topcolor/bottomcolor/skin/team .</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Makes cfg_save save all userinfo variables like name/team/teamcolor/rate/msg/skin/w_switch/b_switch/... .</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



Note :'cfg_save_userinfo 1' is best for teamfortress so you don't get kicked for changing bottom color.
Note : cfg_save will never save the password variable, even though technically it is a userinfo variable.


</p>
</div>

<h2 id="system-settings">System Settings</h2>
  <h3 id="sys_disablewinkeys">sys_disableWinKeys&nbsp;<span>[<a href="#sys_disablewinkeys">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Enable.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Disable winkeys from alt-tabbing you (only works in NT/2K/XP, and only when the client is in focus).</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>


Note: Windows keys are now bindable (LWINKEY, RWINKEY, WINKEY, POPUPMENU). Obviously only useful on NT/2K/XP with sys_disableWinKeys 1 (and possibely useful in Linux too!).


</p>
</div>

  <h3 id="sys_highpriority">sys_highpriority&nbsp;<span>[<a href="#sys_highpriority">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
-1 = Sets process to idle/low priority.
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Sets process to normal priority.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Sets process to high priority.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="sys_inactivesleep">sys_inactiveSleep&nbsp;<span>[<a href="#sys_inactivesleep">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Prevent freeing of CPU when the client is minimized or not in focus.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable inactive sleeping.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="sys_yieldcpu">sys_yieldcpu&nbsp;<span>[<a href="#sys_yieldcpu">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>the client eats more CPU.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Give more CPU for other programs.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

<h2 id="unsorted-variables">Unsorted Variables</h2>
  <h3 id="allow_f_cmdline">allow_f_cmdline&nbsp;<span>[<a href="#allow_f_cmdline">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Your client will not respond to f_cmdline checks.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Your client will respond to f_cmdline checks with your current command line used to run current ezQuake instance.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="allow_f_system">allow_f_system&nbsp;<span>[<a href="#allow_f_system">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Your client will not respond to f_system checks.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Your client will respond to f_system checks displaying your PC configuration.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="auth_validate">auth_validate&nbsp;<span>[<a href="#auth_validate">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>If you don't want your client to validate other ezQuake clients.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>which performs the validation (but you need to use 'validate_clients'</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>


If you have 
'auth_warninvalid 1' and someone gives a dirty hash in a version response 
(because they are using a hacked client that can't work out the right 
response), the client will print something like 
'Warning Invalid Client: playername (userid)' in your console ('auth_warninvalid 0' is default).


</p>
</div>

  <h3 id="auth_warninvalid">auth_warninvalid&nbsp;<span>[<a href="#auth_warninvalid">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Check auth_validate.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="cl_independentphysics">cl_independentPhysics&nbsp;<span>[<a href="#cl_independentphysics">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
It enables independent physics. This means that you can achieve more FPS in game then the server allows.
Amount of FPS used to communicate with server is set by /cl_physfps and amount of graphics FPS is set by /cl_maxfps.
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>0</td>
        <td>
Independent physics disabled.
</td>
      </tr>
      <tr>
        <td>1</td>
        <td>
Independent physics enabled.
</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>
This variable can be set only using command line switch '+set cl_independentPhysics 1'.
</p>
</div>

  <h3 id="cl_loadfragfiles">cl_loadFragfiles&nbsp;<span>[<a href="#cl_loadfragfiles">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Disable.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable displaying team fortress related statistics in the scoreboard (flag touches, steals, caps, etc)</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="cl_parsefrags">cl_parseFrags&nbsp;<span>[<a href="#cl_parsefrags">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Disable.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable displaying team fortress related statistics in the scoreboard (flag touches, steals, caps, etc)</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cl_warncmd">cl_warncmd&nbsp;<span>[<a href="#cl_warncmd">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>Disable the unknown command messages.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable the uknown command messages.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>


Note: Not saved to config with cfg_save command.


</p>
</div>

  <h3 id="enemycolor">enemycolor&nbsp;<span>[<a href="#enemycolor">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>4</span></p>
<div>
  <h4>Description</h4>
  <p>
Set the enemies top- and bottomcolor
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
-1</td>
        <td>The color of enemies depends on theirs /color settings.</td>
      </tr>
      <tr>
        <td>0</td>
        <td>Light-gray color</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="help_files_interline">help_files_interline&nbsp;<span>[<a href="#help_files_interline">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>


Adjust interlining of file list in /help filebrowser.

</p>
</div>

  <h3 id="help_files_showdate">help_files_showdate&nbsp;<span>[<a href="#help_files_showdate">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


Shows column displaying file last modification date in /help file browser.

</p>
</div>

  <h3 id="help_files_showsize">help_files_showsize&nbsp;<span>[<a href="#help_files_showsize">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


Shows column displaying file size in /help file browser.

</p>
</div>

  <h3 id="help_files_showstatus">help_files_showstatus&nbsp;<span>[<a href="#help_files_showstatus">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


Show quick info about selected help file in /help file browser.

</p>
</div>

  <h3 id="help_files_showtime">help_files_showtime&nbsp;<span>[<a href="#help_files_showtime">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>


Shows column displaying file last modification time in /help file browser.

</p>
</div>

  <h3 id="help_files_sortmode">help_files_sortmode&nbsp;<span>[<a href="#help_files_sortmode">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


Switches sorting methods of files /help file browser.

</p>
</div>

  <h3 id="help_files_stripnames">help_files_stripnames&nbsp;<span>[<a href="#help_files_stripnames">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


Strips filenames of help files.

</p>
</div>

  <h3 id="hud_planmode">hud_planmode&nbsp;<span>[<a href="#hud_planmode">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles special hud-editing mode where all hud elements are being displayed.</p>
  <h4>Value</h4>
    <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>0 = False</td>
        <td>Plan-mode off.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Plan-mode on.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_tp_need">hud_tp_need&nbsp;<span>[<a href="#hud_tp_need">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles connection of tp_need_* variables with hud elements.</p>
  <h4>Value</h4>
    <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>0 = False</td>
        <td>Numbers in HUD elements get red when below standard values.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Numbers in HUD get red when they are lower than appropriate tp_need_* values.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>E.g.: Use with 'tp_need_health 40' and your health indicator will display red numbers if your health is 40 or lower.</p>
</div>

  <h3 id="keymap_name">keymap_name&nbsp;<span>[<a href="#keymap_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>Default</span></p>
<div>
  <h4>Description</h4>
  <p>


This holds the name of the current keymappings; it has (currently) only informational purposes. If no keymapping is active, it will contain the name "Default". If a keymapping will be active and no name has been set, "Custom" will be used as name. It can easily be set with "keymap_name &lt;layoutname&gt;"

</p>
</div>

  <h3 id="sv_paused">sv_paused&nbsp;<span>[<a href="#sv_paused">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>read-only variable that gives you current pause state (condition).</p>
  <h4>Value</h4>
    <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>0</td>
        <td>pause is off</td>
      </tr>
      <tr>
        <td>1</td>
        <td>normal pause that can be set by 'pause' command</td>
      </tr>
      <tr>
        <td>2</td>
        <td>auto pause (single player only) when going into menus</td>
      </tr>
      <tr>
        <td>3</td>
        <td>both (normal pause + auto pause)</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="teamcolor">teamcolor&nbsp;<span>[<a href="#teamcolor">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>13</span></p>
<div>
  <h4>Description</h4>
  <p>
Set the teammates top- and bottomcolor.
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
-1</td>
        <td>The color pf teammates depends on theirs /color settings.</td>
      </tr>
      <tr>
        <td>0</td>
        <td>Light-gray color</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="tp_pickup">tp_pickup&nbsp;<span>[<a href="#tp_pickup">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>powerups armor mh lg rl gl pack rockets</span></p>
<div>
  <h4>Description</h4>
  <p>
Set items you want to report ($took)
Item can be: quad, pent, ring, suit, ra, ya, ga, mh, health, lg, 
rl, gl, sng, ng, ssg, pack, cells, rockets, nails, shells, flag, 
armor, weapons, powerups, ammo, all, default, none

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="tp_point">tp_point&nbsp;<span>[<a href="#tp_point">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>powerups armor players mh lg rl gl sng ssg pack rockets</span></p>
<div>
  <h4>Description</h4>
  <p>

What to report for pointing (%x, %y and %t).
Item can be: powerups, quad, pent, ring, armor, ra, ya, ga,
weapons, lg, rl, gl, sng, ng, ssg,
ammo, cells, rockets, nails, shells,
players, eyes, teammate, enemy,
mh, health, pack, flag, all, default, none, suit
Item names can be customized with tp_name_item
Examples:
tp_point powerups flag
say_team I see %t!

</p>
</div>

  <h3 id="tp_took">tp_took&nbsp;<span>[<a href="#tp_took">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>powerups armor mh lg rl gl pack rockets</span></p>
<div>
  <h4>Description</h4>
  <p>

Customizes the f_took trigger.
Item can be: quad, pent, ring, suit, ra, ya, ga, mh, health, lg, 
rl, gl, sng, ng, ssg, pack, cells, rockets, nails, shells, flag, 
armor, weapons, powerups, ammo, all, default, none
Item names can be customized with tp_name_item
Examples:
tp_took powerups ra ya lg rl mh flag (same as "default")
tp_took all -ammo (report everything but ammo)
tp_took +gl (add grenade launcher to pickup list)
alias f_took "say_team took %i"

</p>
</div>

  <h3 id="watervis">watervis&nbsp;<span>[<a href="#watervis">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This server-side cvar is mirrored to serverinfo as a hint to clients on whether the server is using the so-called watervised maps.
</p>
  <h4>Value</h4>
  <table class="values">
    <thead>
      <tr>
        <td>value</td>
        <td>description</td>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>
0</td>
        <td>No watervis.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>GLQuake users can see through liquids if they set gl_turbalpha to a value &lt; 1.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>


It's a bad idea to use watervis 1 if the maps the server is using are not watervised. Liquids will appear 'transparent' but you won't be able to see any items/players/etc through it.  And it is a bad idea anyway because Quake maps were not designed for transparent water (or other liquids); enabling it changes gameplay, decreases fps and produces more network traffic and puts non-GL players at a disadvantage because the software renderer doesn't support transparent water (yet).
Not saved to config with cfg_save command.


</p>
</div>

