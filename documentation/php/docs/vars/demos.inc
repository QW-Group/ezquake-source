  <ul>
    <li><a href="#demo-handling">Demo Handling</a></li>
    <li><a href="#match-tools">Match Tools</a></li>
  </ul>
<h2 id="demo-handling">Demo Handling</h2>
  <h3 id="cl_demopinginterval">cl_demoPingInterval&nbsp;<span>[<a href="#cl_demopinginterval">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>5</span></p>
<div>
  <h4>Description</h4>
  <p>

How often to request ping updates when recording demos. This variable doesn't 
affect ping updates when the scoreboard is shown (they are always one update per 2 seconds).
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
        <td>Disable automatic ping requests.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>


</p>
</div>

  <h3 id="cl_demospeed">cl_demospeed&nbsp;<span>[<a href="#cl_demospeed">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Controls the speed of demo playback in percentage (can be changed during demo playback if you wish).

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="demo_capture_codec">demo_capture_codec&nbsp;<span>[<a href="#demo_capture_codec">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>


Determines what codes should be used for captured video stream compression. E.g. XVID, DIVX, ...

</p>
</div>

  <h3 id="demo_capture_dir">demo_capture_dir&nbsp;<span>[<a href="#demo_capture_dir">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>capture</span></p>
<div>
  <h4>Description</h4>
  <p>

Change the default capture directory.

</p>
</div>

  <h3 id="demo_capture_fps">demo_capture_fps&nbsp;<span>[<a href="#demo_capture_fps">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>30.0</span></p>
<div>
  <h4>Description</h4>
  <p>

Change the default capture fps.

</p>
</div>

  <h3 id="demo_capture_mp3">demo_capture_mp3&nbsp;<span>[<a href="#demo_capture_mp3">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>


When set to 1 .avi capturing captures sound compressed in MP3 format.
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
        <td>Captures uncompressed sound</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Captures sound in MP3 format</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>

See demo_capture_mp3_kbps too.


</p>
</div>

  <h3 id="demo_capture_mp3_kbps">demo_capture_mp3_kbps&nbsp;<span>[<a href="#demo_capture_mp3_kbps">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>128</span></p>
<div>
  <h4>Description</h4>
  <p>


Sets bitrate for captured sound stream when demo_capture_mp3 is set to 1.

</p>
</div>

  <h3 id="demo_capture_steadycam">demo_capture_steadycam&nbsp;<span>[<a href="#demo_capture_steadycam">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

Changes behaviour of keyboard/mouse input when capturing.

</p>
</div>

  <h3 id="demo_dir">demo_dir&nbsp;<span>[<a href="#demo_dir">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

Change the demos and autorecord directory.

</p>
</div>

  <h3 id="demo_getpings">demo_getpings&nbsp;<span>[<a href="#demo_getpings">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


This toggles whether the client should always record pings into the demo or only
when the player died and show(team)scores are being shown (QWCL default).
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
        <td>only update pings in the demo when the player died and show(team)scores
    are being shown</td>
      </tr>
      <tr>
        <td>1</td>
        <td>always update pings</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="qizmo_dir">qizmo_dir&nbsp;<span>[<a href="#qizmo_dir">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>qizmo</span></p>
<div>
  <h4>Description</h4>
  <p>

Change the default qizmo directory.

</p>
</div>

<h2 id="match-tools">Match Tools</h2>
  <h3 id="match_auto_logconsole">match_auto_logconsole&nbsp;<span>[<a href="#match_auto_logconsole">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>When set to 1 or 2, a temp console log will automatically be created when a match starts (usually when the countdown starts).</p>
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
        <td>Do not auto-log console.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Auto record console but requires manuall saving. See 'match_save'.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Automatically saves the console log after the match is completed.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="match_auto_minlength">match_auto_minlength&nbsp;<span>[<a href="#match_auto_minlength">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>30</span></p>
<div>
  <h4>Description</h4>
  <p>When using 'match_auto_record 2', temp demo's auto recorded won't be saved automatically if they are shorter than the number of seconds match_auto_minlength' is set to.</p>
  <h4>Value</h4>
    <p>Integer. number of seconds</p>
  <h4>Remarks</h4>
  <p>If a temp demo is too short to autosave, you can still save it manually with "match_save".</p>
</div>

  <h3 id="match_auto_record">match_auto_record&nbsp;<span>[<a href="#match_auto_record">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>When set to 1 or 2, a temp demo will automatically be recorded when a match starts (usually when the countdown starts).</p>
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
        <td>No auto recording.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Demo will be recorded but requires manuall saving.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Automatically saves the demo after the match is completed.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>See 'match_save' for manuall demo saving.</p>
</div>

  <h3 id="match_auto_spectating">match_auto_spectating&nbsp;<span>[<a href="#match_auto_spectating">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>When set to 1, auto recording will also occur when in spectator mode.</p>
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
        <td>Don't auto record demos in spectator mode.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Auto record demos even in spectator mode.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="match_auto_sshot">match_auto_sshot&nbsp;<span>[<a href="#match_auto_sshot">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Set to 1 to automatically take a screenshot of the final scoreboard when a match ends. If your console is down or you are in the menus, then the client will remove the console/menu for a split second so it can take a screenshot of the scoreboard without any interference.</p>
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
        <td>Do not take auto screenshot.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Take auto screenshot on match end.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="match_format_2on2">match_format_2on2&nbsp;<span>[<a href="#match_format_2on2">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2on2/%n - [%k%v%l] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "qw", 2 teams with at least 1 team having 2 people)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_3on3">match_format_3on3&nbsp;<span>[<a href="#match_format_3on3">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>tdm/%n - [%Oon%E_%t%v%e] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "qw", 2 teams with at least 1 team having 3 people)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_4on4">match_format_4on4&nbsp;<span>[<a href="#match_format_4on4">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>tdm/%n - [%Oon%E_%t%v%e] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "qw", 2 teams with at least 1 team having 4 people)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_arena">match_format_arena&nbsp;<span>[<a href="#match_format_arena">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>arena/%n - %p%v%e - [%F_frags] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "arena")</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_coop">match_format_coop&nbsp;<span>[<a href="#match_format_coop">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>coop/%n - [%C_player_coop] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "qw", at least 2 people, deathmatch is off)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_duel">match_format_duel&nbsp;<span>[<a href="#match_format_duel">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>duel/%n - %p%v%e - [dmm%D] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "qw", 2 players)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_ffa">match_format_ffa&nbsp;<span>[<a href="#match_format_ffa">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>ffa/%n - [%C_player_ffa] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "qw", more than 2 players, teamplay off)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_multiteam">match_format_multiteam&nbsp;<span>[<a href="#match_format_multiteam">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>tdm/%n - [%a_%b] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "qw", 3 or more teams)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_race">match_format_race&nbsp;<span>[<a href="#match_format_race">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>race/%n - [race] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = qw, serverinfo "race" key present and equal to mapname)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_solo">match_format_solo&nbsp;<span>[<a href="#match_format_solo">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>solo/%n - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (any gamedir, 1 player on a server, not in race mode)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_tdm">match_format_tdm&nbsp;<span>[<a href="#match_format_tdm">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>tdm/%n - [%Oon%E_%t%v%e] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "qw", 2 teams with at least 1 team having 5 people)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_tf_clanwar">match_format_tf_clanwar&nbsp;<span>[<a href="#match_format_tf_clanwar">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>tfwar/%n - [%Oon%E_%t%v%e] - [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "fortress", at least 2 teams)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_format_tf_duel">match_format_tf_duel&nbsp;<span>[<a href="#match_format_tf_duel">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>tfduel/%n - %p%v%e [%M]</span></p>
<div>
  <h4>Description</h4>
  <p>Each match category has a name format variable associated with it. This variable is called match_format_&lt;category&gt;. For example, there is match_format_duel, match_format_2on2, etc, etc. All these variables can contain macro's that are expanded according to the macro list given below.  You can also use the "match_format_macrolist" command inside the client to display a list of the macros and their meaning.</p>
  <h4>Value</h4>
    <p>String. This pattern will be used in following situation: (gamedir = "fortress", 2 players)</p>
  <h4>Remarks</h4>
  <p>See 'match_format_macrolist' command and Match tools manual for more info.</p>
</div>

  <h3 id="match_name_and">match_name_and&nbsp;<span>[<a href="#match_name_and">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>_&amp;_</span></p>
<div>
  <h4>Description</h4>
  <p>Used for separating names in %k and %l.</p>
  <h4>Value</h4>
    <p>String. </p>
  </div>

  <h3 id="match_name_nick">match_name_nick&nbsp;<span>[<a href="#match_name_nick">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>%n uses this if its not "", otherwise it uses your in game name.</p>
  <h4>Value</h4>
    <p>String. </p>
  <h4>Remarks</h4>
  <p>See match_format_macrolist.</p>
</div>

  <h3 id="match_name_on">match_name_on&nbsp;<span>[<a href="#match_name_on">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>on</span></p>
<div>
  <h4>Description</h4>
  <p>Used for separating numbers in %a.</p>
  <h4>Value</h4>
    <p>String. </p>
  </div>

  <h3 id="match_name_spec">match_name_spec&nbsp;<span>[<a href="#match_name_spec">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>(SPEC)</span></p>
<div>
  <h4>Description</h4>
  <p>This is placed after your nick when using %n and are in spec mode. Eg. if you use "match_name_nick foo" and leave match_name_spec default, then %n will be "foo(SPEC)" in spec mode and "foo" when not in spec mode.</p>
  <h4>Value</h4>
    <p>String. </p>
  <h4>Remarks</h4>
  <p>See match_format_macrolist.</p>
</div>

  <h3 id="match_name_versus">match_name_versus&nbsp;<span>[<a href="#match_name_versus">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>_vs_</span></p>
<div>
  <h4>Description</h4>
  <p>Used for separating names in %b.</p>
  <h4>Value</h4>
    <p>String. </p>
  </div>

