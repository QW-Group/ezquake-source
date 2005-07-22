  <ul>
    <li><a href="#chat-settings">Chat Settings</a></li>
    <li><a href="#console-settings">Console Settings</a></li>
    <li><a href="#mqwcl-hud">MQWCL HUD</a></li>
    <li><a href="#status-bar-and-scoreboard">Status Bar and Scoreboard</a></li>
  </ul>
<h2 id="chat-settings">Chat Settings</h2>
  <h3 id="auth_viewcrc">auth_viewcrc&nbsp;<span>[<a href="#auth_viewcrc">#</a>]</span></h3>
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
        <td>the client hides the authentication CRC in f_version responses.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>the client shows you the authentication CRC in f_version responses.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="cl_chatsound">cl_chatsound&nbsp;<span>[<a href="#cl_chatsound">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Controls usage of con_sound_* variables.</p>
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
        <td>Do not use con_sound_* variables settings.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Use settings from con_sound_* for char sounds.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Only play sounds in mm2.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="cl_floodprot">cl_floodprot&nbsp;<span>[<a href="#cl_floodprot">#</a>]</span></h3>
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
        <td>Disable floodprot.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable floodprot.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cl_fp_messages">cl_fp_messages&nbsp;<span>[<a href="#cl_fp_messages">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>4</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable is used in conjunction with the variable "cl_fp_persecond" to
define when the floodprot protection should be triggered (if "cl_floodprot"
is set to "1").

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="cl_fp_persecond">cl_fp_persecond&nbsp;<span>[<a href="#cl_fp_persecond">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>4</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable is used in conjunction with the variable "cl_fp_messages" to
define when the floodprot protection should be triggered (if "cl_floodprot"
is set to "1").

</p>
</div>

  <h3 id="cl_nofake">cl_nofake&nbsp;<span>[<a href="#cl_nofake">#</a>]</span></h3>
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
        <td>No unfaking.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Unfake other players messages.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Unfake messages from enemies.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cl_parsefunchars">cl_parseFunChars&nbsp;<span>[<a href="#cl_parsefunchars">#</a>]</span></h3>
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
        <td>Off.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>You can use Ocrana LED's by hand right out of the console and chat anonymously through "$\" without showing your name (chat messages that are seen without your name at the beginning).</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>


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
</div>

  <h3 id="cl_parsesay">cl_parseSay&nbsp;<span>[<a href="#cl_parsesay">#</a>]</span></h3>
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
        <td>Disable %-macros (%a %h %b etc...)</td>
      </tr>
      <tr>
        <td>1</td>
        <td>You can use macros such as %a %h %l (like in Qizmo).</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cl_parsewhitetext">cl_parseWhiteText&nbsp;<span>[<a href="#cl_parsewhitetext">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Convert text between { and } to white or not in chat/team chat.
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
        <td>Convert none.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Convert all (chat and team chat).</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Convert team chat only.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="ignore_flood">ignore_flood&nbsp;<span>[<a href="#ignore_flood">#</a>]</span></h3>
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
        <td>Off.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Ignores repeated 'say' and spectator messages.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Ignores repeated 'say', 'say_team' and spectator messages.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="ignore_flood_duration">ignore_flood_duration&nbsp;<span>[<a href="#ignore_flood_duration">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>4</span></p>
<div>
  <h4>Description</h4>
  <p>
You can change the 4 second cooldown with the 'ignore_flood_duration' variable.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="ignore_mode">ignore_mode&nbsp;<span>[<a href="#ignore_mode">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

Someone is on your ignore list, you won't see any messagemode (/say hello) messages from them 
(even if they are a spec).
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
        <td>Ignore say only.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Ignore say_team too.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="ignore_opponents">ignore_opponents&nbsp;<span>[<a href="#ignore_opponents">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p></p>
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
        <td>Do not ignore opponent team.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Alway ignore opponent team.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Only ignore opponent team during the match.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="ignore_qizmo_spec">ignore_qizmo_spec&nbsp;<span>[<a href="#ignore_qizmo_spec">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Ignores all Qizmo spectators (= observers). Very useful on big matches with 100 and more spectators.

</p>
  <h4>Remarks</h4>
  <p>


</p>
</div>

  <h3 id="ignore_spec">ignore_spec&nbsp;<span>[<a href="#ignore_spec">#</a>]</span></h3>
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
        <td>Off</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Ignore spec chat unless you are a spectator.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Ignore spec chat even if you are a spectator.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="msg_filter">msg_filter&nbsp;<span>[<a href="#msg_filter">#</a>]</span></h3>
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
        <td>Filters nothing (default)</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Filters mm1 and spec messages</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Filters mm2 messages</td>
      </tr>
      <tr>
        <td>3</td>
        <td>Filters mm1, mm2, and spec messages</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

<h2 id="console-settings">Console Settings</h2>
  <h3 id="cl_chatmode">cl_chatmode&nbsp;<span>[<a href="#cl_chatmode">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>
Console chat mode.
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
        <td>Text in the console is always treated as a command and in order to chat you have to use messagemode/messagemode2 or use the say/say_team commands.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Everything you type in the console goes into chat, in order to issue a command, prefix it with a slash (/).</td>
      </tr>
      <tr>
        <td>2</td>
        <td>If the first word in the line is a command, it is executed, otherwise the line is sent as chat.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="cl_newcompletion">cl_newCompletion&nbsp;<span>[<a href="#cl_newcompletion">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


Toggles the new console [tab] completion algorithm on when set to 1. 
It cycles through the available options.

</p>
</div>

  <h3 id="cl_savehistory">cl_savehistory&nbsp;<span>[<a href="#cl_savehistory">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Save console commands history to .ezquake_history.
Loads history from this file while starting ezquake.</p>
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
        <td>don't save</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>save</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="con_clearnotify">con_clearnotify&nbsp;<span>[<a href="#con_clearnotify">#</a>]</span></h3>
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
        <td>Messages stay even when you toggle console.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Messages won't say when you toggle console.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="con_highlight">con_highlight&nbsp;<span>[<a href="#con_highlight">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>

  <h3 id="con_highlight_mark">con_highlight_mark&nbsp;<span>[<a href="#con_highlight_mark">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>

  <h3 id="con_notifylines">con_notifylines&nbsp;<span>[<a href="#con_notifylines">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>4</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable sets the number of notify lines (default 4, max 20) to be used at the top of the screen.
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
        <td>Use no notify lines at all.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Use one notify line.</td>
      </tr>
      <tr>
        <td>20</td>
        <td>Use 20 notify lines.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>


...



</p>
</div>

  <h3 id="con_notifytime">con_notifytime&nbsp;<span>[<a href="#con_notifytime">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
How long console messages stay on screen.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="con_shift">con_shift&nbsp;<span>[<a href="#con_shift">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>


Adjusts vertical offset of background of the console.

</p>
</div>

  <h3 id="con_sound_mm1_file">con_sound_mm1_file&nbsp;<span>[<a href="#con_sound_mm1_file">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>misc/talk.wav</span></p>
<div>
  <h4>Description</h4>
  <p>


You can specify notification sound for messagemode1 (/messagemode or /say foo) messages.

</p>
</div>

  <h3 id="con_sound_mm1_volume">con_sound_mm1_volume&nbsp;<span>[<a href="#con_sound_mm1_volume">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


You can specify volume of notification sound for messagemode1 (/messagemode or /say foo) messages.

</p>
</div>

  <h3 id="con_sound_mm2_file">con_sound_mm2_file&nbsp;<span>[<a href="#con_sound_mm2_file">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>misc/talk.wav</span></p>
<div>
  <h4>Description</h4>
  <p>


You can specify notification sound for messagemode2 (/messagemode2 or /say_team foo) messages.

</p>
</div>

  <h3 id="con_sound_mm2_volume">con_sound_mm2_volume&nbsp;<span>[<a href="#con_sound_mm2_volume">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


You can specify volume of notification sound for messagemode2 (/messagemode2 or /say_team foo) messages.

</p>
</div>

  <h3 id="con_sound_other_file">con_sound_other_file&nbsp;<span>[<a href="#con_sound_other_file">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>misc/talk.wav</span></p>
<div>
  <h4>Description</h4>
  <p>


You can specify notification sound for other messages (than messagemode, messagemode2 and from spectators).

</p>
</div>

  <h3 id="con_sound_other_volume">con_sound_other_volume&nbsp;<span>[<a href="#con_sound_other_volume">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
You can specify volume of notification sound for other messages (than messagemode, messagemode2 and from spectators).

</p>
  <h4>Remarks</h4>
  <p>


</p>
</div>

  <h3 id="con_sound_spec_file">con_sound_spec_file&nbsp;<span>[<a href="#con_sound_spec_file">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>misc/talk.wav</span></p>
<div>
  <h4>Description</h4>
  <p>


You can specify notification sound for spectator messages.

</p>
</div>

  <h3 id="con_sound_spec_volume">con_sound_spec_volume&nbsp;<span>[<a href="#con_sound_spec_volume">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


You can specify volume of notification sound for spectator messages.

</p>
</div>

  <h3 id="con_timestamps">con_timestamps&nbsp;<span>[<a href="#con_timestamps">#</a>]</span></h3>
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
        <td>Console timesptamps turned OFF</td>
      </tr>
      <tr>
        <td>1</td>
        <td>will enable timestamps for chat text in
            console, every chat message will begin with timestamp
            (hour:minute) followed by a message</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="con_wordwrap">con_wordwrap&nbsp;<span>[<a href="#con_wordwrap">#</a>]</span></h3>
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
        <td>Disable word wrapping.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable word wrapping.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="cvar_viewdefault">cvar_viewdefault&nbsp;<span>[<a href="#cvar_viewdefault">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
When you type a cvar name into console (like 'gl_gamma' or 'r_rocketlight'), the client will tell you the default value of the cvar.
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
        <td>Off.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>On.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="gl_consolefont">gl_consolefont&nbsp;<span>[<a href="#gl_consolefont">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>original</span></p>
<div>
  <h4>Description</h4>
  <p>
Changes your console font.
Put all your charset images in qw/textures/charsets/*.png (and *.tga) and use /gl_loadcharset XXX to load XXX.png (or tga). "/gl_consolefont original" will restore the 8bit font in your gfx.wad (this is default).
Note: Loadcharset is an alias for gl_consolefont.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="gl_smoothfont">gl_smoothfont&nbsp;<span>[<a href="#gl_smoothfont">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

Smoothens out the font (which = good). But in some cases the font wasn't designed to be smoothened (sometimes the case of custom console fonts etc).
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
        <td>Off.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>On.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="log_dir">log_dir&nbsp;<span>[<a href="#log_dir">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
The logging dir.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="log_readable">log_readable&nbsp;<span>[<a href="#log_readable">#</a>]</span></h3>
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
        <td>Off.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Will convert all non printable characters to printable characters in your log. This makes your log readable because all the weird ascii characters that usually occur because of different coloured text in the client (and because of weird symbols - ocrana led's etc) are converted into printable characters.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="scr_conalpha">scr_conalpha&nbsp;<span>[<a href="#scr_conalpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1 or 0.8 or 0.8 or 0.8 or 0.8</span></p>
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
        <td>Transparent.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Normal.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="scr_consize">scr_consize&nbsp;<span>[<a href="#scr_consize">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0.75</span></p>
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
        <td>Minimal console height.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Maximum console height.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="scr_conspeed">scr_conspeed&nbsp;<span>[<a href="#scr_conspeed">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1000</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable controls the speed at which the console screen scrolls up and down.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="scr_printspeed">scr_printspeed&nbsp;<span>[<a href="#scr_printspeed">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>8</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable controls how fast the text is displayed at the end of the single 
player episodes.

</p>
</div>

<h2 id="mqwcl-hud">MQWCL HUD</h2>
  <h3 id="hud_ammo1_align">hud_ammo1_align&nbsp;<span>[<a href="#hud_ammo1_align">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>Sets align of number of shells for both horizontal and vertical direction.</p>
</div>

  <h3 id="hud_ammo1_align_x">hud_ammo1_align_x&nbsp;<span>[<a href="#hud_ammo1_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of number of shells</p>
</div>

  <h3 id="hud_ammo1_align_y">hud_ammo1_align_y&nbsp;<span>[<a href="#hud_ammo1_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of number of shells</p>
</div>

  <h3 id="hud_ammo1_digits">hud_ammo1_digits&nbsp;<span>[<a href="#hud_ammo1_digits">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets number of digits for number of shells</p>
</div>

  <h3 id="hud_ammo1_frame">hud_ammo1_frame&nbsp;<span>[<a href="#hud_ammo1_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for number of shells</p>
</div>

  <h3 id="hud_ammo1_place">hud_ammo1_place&nbsp;<span>[<a href="#hud_ammo1_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for number of shells</p>
</div>

  <h3 id="hud_ammo1_pos_x">hud_ammo1_pos_x&nbsp;<span>[<a href="#hud_ammo1_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of number of shells</p>
</div>

  <h3 id="hud_ammo1_pos_y">hud_ammo1_pos_y&nbsp;<span>[<a href="#hud_ammo1_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of number of shells</p>
</div>

  <h3 id="hud_ammo1_scale">hud_ammo1_scale&nbsp;<span>[<a href="#hud_ammo1_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of number of shells</p>
</div>

  <h3 id="hud_ammo1_show">hud_ammo1_show&nbsp;<span>[<a href="#hud_ammo1_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of number of shells</p>
</div>

  <h3 id="hud_ammo1_style">hud_ammo1_style&nbsp;<span>[<a href="#hud_ammo1_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of number of shells</p>
</div>

  <h3 id="hud_ammo2_align">hud_ammo2_align&nbsp;<span>[<a href="#hud_ammo2_align">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets align of number of nails</p>
</div>

  <h3 id="hud_ammo2_align_x">hud_ammo2_align_x&nbsp;<span>[<a href="#hud_ammo2_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of number of nails</p>
</div>

  <h3 id="hud_ammo2_align_y">hud_ammo2_align_y&nbsp;<span>[<a href="#hud_ammo2_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of number of nails</p>
</div>

  <h3 id="hud_ammo2_digits">hud_ammo2_digits&nbsp;<span>[<a href="#hud_ammo2_digits">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets number of digits for number of nails</p>
</div>

  <h3 id="hud_ammo2_frame">hud_ammo2_frame&nbsp;<span>[<a href="#hud_ammo2_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for number of nails</p>
</div>

  <h3 id="hud_ammo2_place">hud_ammo2_place&nbsp;<span>[<a href="#hud_ammo2_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for number of nails</p>
</div>

  <h3 id="hud_ammo2_pos_x">hud_ammo2_pos_x&nbsp;<span>[<a href="#hud_ammo2_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of number of nails</p>
</div>

  <h3 id="hud_ammo2_pos_y">hud_ammo2_pos_y&nbsp;<span>[<a href="#hud_ammo2_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of number of nails</p>
</div>

  <h3 id="hud_ammo2_scale">hud_ammo2_scale&nbsp;<span>[<a href="#hud_ammo2_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of number of nails</p>
</div>

  <h3 id="hud_ammo2_show">hud_ammo2_show&nbsp;<span>[<a href="#hud_ammo2_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of number of nails</p>
</div>

  <h3 id="hud_ammo2_style">hud_ammo2_style&nbsp;<span>[<a href="#hud_ammo2_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of number of nails</p>
</div>

  <h3 id="hud_ammo3_align">hud_ammo3_align&nbsp;<span>[<a href="#hud_ammo3_align">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets align of number of rockets</p>
</div>

  <h3 id="hud_ammo3_align_x">hud_ammo3_align_x&nbsp;<span>[<a href="#hud_ammo3_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of number of rockets</p>
</div>

  <h3 id="hud_ammo3_align_y">hud_ammo3_align_y&nbsp;<span>[<a href="#hud_ammo3_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of number of rockets</p>
</div>

  <h3 id="hud_ammo3_digits">hud_ammo3_digits&nbsp;<span>[<a href="#hud_ammo3_digits">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets number of digits for </p>
</div>

  <h3 id="hud_ammo3_frame">hud_ammo3_frame&nbsp;<span>[<a href="#hud_ammo3_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for </p>
</div>

  <h3 id="hud_ammo3_place">hud_ammo3_place&nbsp;<span>[<a href="#hud_ammo3_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for </p>
</div>

  <h3 id="hud_ammo3_pos_x">hud_ammo3_pos_x&nbsp;<span>[<a href="#hud_ammo3_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of number of rockets</p>
</div>

  <h3 id="hud_ammo3_pos_y">hud_ammo3_pos_y&nbsp;<span>[<a href="#hud_ammo3_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of number of rockets</p>
</div>

  <h3 id="hud_ammo3_scale">hud_ammo3_scale&nbsp;<span>[<a href="#hud_ammo3_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of number of rockets</p>
</div>

  <h3 id="hud_ammo3_show">hud_ammo3_show&nbsp;<span>[<a href="#hud_ammo3_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of number of rockets</p>
</div>

  <h3 id="hud_ammo3_style">hud_ammo3_style&nbsp;<span>[<a href="#hud_ammo3_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of number of rockets</p>
</div>

  <h3 id="hud_ammo4_align">hud_ammo4_align&nbsp;<span>[<a href="#hud_ammo4_align">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets align of number of cells</p>
</div>

  <h3 id="hud_ammo4_align_x">hud_ammo4_align_x&nbsp;<span>[<a href="#hud_ammo4_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of number of cells</p>
</div>

  <h3 id="hud_ammo4_align_y">hud_ammo4_align_y&nbsp;<span>[<a href="#hud_ammo4_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of number of cells</p>
</div>

  <h3 id="hud_ammo4_digits">hud_ammo4_digits&nbsp;<span>[<a href="#hud_ammo4_digits">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets number of digits for number of cells</p>
</div>

  <h3 id="hud_ammo4_frame">hud_ammo4_frame&nbsp;<span>[<a href="#hud_ammo4_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for number of cells</p>
</div>

  <h3 id="hud_ammo4_place">hud_ammo4_place&nbsp;<span>[<a href="#hud_ammo4_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for number of cells</p>
</div>

  <h3 id="hud_ammo4_pos_x">hud_ammo4_pos_x&nbsp;<span>[<a href="#hud_ammo4_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of number of cells</p>
</div>

  <h3 id="hud_ammo4_pos_y">hud_ammo4_pos_y&nbsp;<span>[<a href="#hud_ammo4_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of number of cells</p>
</div>

  <h3 id="hud_ammo4_scale">hud_ammo4_scale&nbsp;<span>[<a href="#hud_ammo4_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of number of cells</p>
</div>

  <h3 id="hud_ammo4_show">hud_ammo4_show&nbsp;<span>[<a href="#hud_ammo4_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of number of cells</p>
</div>

  <h3 id="hud_ammo4_style">hud_ammo4_style&nbsp;<span>[<a href="#hud_ammo4_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of number of cells</p>
</div>

  <h3 id="hud_ammo_align">hud_ammo_align&nbsp;<span>[<a href="#hud_ammo_align">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets align of current ammo value</p>
</div>

  <h3 id="hud_ammo_align_x">hud_ammo_align_x&nbsp;<span>[<a href="#hud_ammo_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of current ammo value</p>
</div>

  <h3 id="hud_ammo_align_y">hud_ammo_align_y&nbsp;<span>[<a href="#hud_ammo_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of current ammo value</p>
</div>

  <h3 id="hud_ammo_digits">hud_ammo_digits&nbsp;<span>[<a href="#hud_ammo_digits">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets number of digits for current ammo value</p>
</div>

  <h3 id="hud_ammo_frame">hud_ammo_frame&nbsp;<span>[<a href="#hud_ammo_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for current ammo value</p>
</div>

  <h3 id="hud_ammo_place">hud_ammo_place&nbsp;<span>[<a href="#hud_ammo_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for current ammo value</p>
</div>

  <h3 id="hud_ammo_pos_x">hud_ammo_pos_x&nbsp;<span>[<a href="#hud_ammo_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of current ammo value</p>
</div>

  <h3 id="hud_ammo_pos_y">hud_ammo_pos_y&nbsp;<span>[<a href="#hud_ammo_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of current ammo value</p>
</div>

  <h3 id="hud_ammo_scale">hud_ammo_scale&nbsp;<span>[<a href="#hud_ammo_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of current ammo value</p>
</div>

  <h3 id="hud_ammo_show">hud_ammo_show&nbsp;<span>[<a href="#hud_ammo_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of current ammo value</p>
</div>

  <h3 id="hud_ammo_style">hud_ammo_style&nbsp;<span>[<a href="#hud_ammo_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of current ammo value</p>
</div>

  <h3 id="hud_armor_align">hud_armor_align&nbsp;<span>[<a href="#hud_armor_align">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets align of armor level</p>
</div>

  <h3 id="hud_armor_align_x">hud_armor_align_x&nbsp;<span>[<a href="#hud_armor_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of armor level</p>
</div>

  <h3 id="hud_armor_align_y">hud_armor_align_y&nbsp;<span>[<a href="#hud_armor_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of armor level</p>
</div>

  <h3 id="hud_armor_digits">hud_armor_digits&nbsp;<span>[<a href="#hud_armor_digits">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets number of digits for armor level</p>
</div>

  <h3 id="hud_armor_frame">hud_armor_frame&nbsp;<span>[<a href="#hud_armor_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for armor level</p>
</div>

  <h3 id="hud_armor_place">hud_armor_place&nbsp;<span>[<a href="#hud_armor_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for armor level</p>
</div>

  <h3 id="hud_armor_pos_x">hud_armor_pos_x&nbsp;<span>[<a href="#hud_armor_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of armor level</p>
</div>

  <h3 id="hud_armor_pos_y">hud_armor_pos_y&nbsp;<span>[<a href="#hud_armor_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of armor level</p>
</div>

  <h3 id="hud_armor_scale">hud_armor_scale&nbsp;<span>[<a href="#hud_armor_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of armor level</p>
</div>

  <h3 id="hud_armor_show">hud_armor_show&nbsp;<span>[<a href="#hud_armor_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of armor level</p>
</div>

  <h3 id="hud_armor_style">hud_armor_style&nbsp;<span>[<a href="#hud_armor_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of armor level</p>
</div>

  <h3 id="hud_armordamage_align">hud_armordamage_align&nbsp;<span>[<a href="#hud_armordamage_align">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>Sets armordamage HUD element alignment.</p>
  <h4>Value</h4>
    <p>String. horizontal: left, center, right, before, after; vertical: top, center, bottom, console, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_armordamage_align_x">hud_armordamage_align_x&nbsp;<span>[<a href="#hud_armordamage_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>Sets armordamage HUD element horizontal alignment.</p>
  <h4>Value</h4>
    <p>String. left, center, right, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_armordamage_align_y">hud_armordamage_align_y&nbsp;<span>[<a href="#hud_armordamage_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>before</span></p>
<div>
  <h4>Description</h4>
  <p>Sets armordamage HUD element vertical alignment.</p>
  <h4>Value</h4>
    <p>String. top, center, bottom, console, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_armordamage_digits">hud_armordamage_digits&nbsp;<span>[<a href="#hud_armordamage_digits">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>Sets highest possible number of digits displayed in HUD element armordamage.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_armordamage_duration">hud_armordamage_duration&nbsp;<span>[<a href="#hud_armordamage_duration">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0.8</span></p>
<div>
  <h4>Description</h4>
  <p>Sets how long armordamage should be visible after last damage to armor has been done.</p>
  <h4>Value</h4>
    <p>Float. Float number</p>
  </div>

  <h3 id="hud_armordamage_frame">hud_armordamage_frame&nbsp;<span>[<a href="#hud_armordamage_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets frame visibility and style for this HUD element.</p>
  <h4>Value</h4>
    <p>Float. Use floating point values from range 0..1 to get alpha transparent background or 2 to get bevel frame.</p>
  </div>

  <h3 id="hud_armordamage_place">hud_armordamage_place&nbsp;<span>[<a href="#hud_armordamage_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>armor</span></p>
<div>
  <h4>Description</h4>
  <p>Sets placement for this HUD element. You can align some elements relative to other elements.</p>
  <h4>Value</h4>
    <p>String. to place this element inside element B set this variable to "@B", to place it outside B element set this variable to "B"</p>
  <h4>Remarks</h4>
  <p>First you have to decide, if the element that you are locating now (element B) is to be positioned inside another element (element A) or outside it. See HUD manual for more info.</p>
</div>

  <h3 id="hud_armordamage_pos_x">hud_armordamage_pos_x&nbsp;<span>[<a href="#hud_armordamage_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets horizontal position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_armordamage_pos_y">hud_armordamage_pos_y&nbsp;<span>[<a href="#hud_armordamage_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets vertical position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_armordamage_scale">hud_armordamage_scale&nbsp;<span>[<a href="#hud_armordamage_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Sets overall size of this HUD element.</p>
  <h4>Value</h4>
    <p>Float. Use positive floating point numbers.</p>
  </div>

  <h3 id="hud_armordamage_show">hud_armordamage_show&nbsp;<span>[<a href="#hud_armordamage_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles visibility of this HUD element.</p>
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
        <td>Do not draw this element</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw this element</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_armordamage_style">hud_armordamage_style&nbsp;<span>[<a href="#hud_armordamage_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles between different numbers styles.</p>
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
        <td>large font, texture</td>
      </tr>
      <tr>
        <td>1</td>
        <td>small font, gfxwad</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_clock_align_x">hud_clock_align_x&nbsp;<span>[<a href="#hud_clock_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of clock</p>
</div>

  <h3 id="hud_clock_align_y">hud_clock_align_y&nbsp;<span>[<a href="#hud_clock_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>console</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of clock</p>
</div>

  <h3 id="hud_clock_big">hud_clock_big&nbsp;<span>[<a href="#hud_clock_big">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches unsing larger version of clock</p>
</div>

  <h3 id="hud_clock_blink">hud_clock_blink&nbsp;<span>[<a href="#hud_clock_blink">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches blinking colon of clock</p>
</div>

  <h3 id="hud_clock_frame">hud_clock_frame&nbsp;<span>[<a href="#hud_clock_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for clock</p>
</div>

  <h3 id="hud_clock_place">hud_clock_place&nbsp;<span>[<a href="#hud_clock_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for clock</p>
</div>

  <h3 id="hud_clock_pos_x">hud_clock_pos_x&nbsp;<span>[<a href="#hud_clock_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of clock</p>
</div>

  <h3 id="hud_clock_pos_y">hud_clock_pos_y&nbsp;<span>[<a href="#hud_clock_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of clock</p>
</div>

  <h3 id="hud_clock_show">hud_clock_show&nbsp;<span>[<a href="#hud_clock_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of clock</p>
</div>

  <h3 id="hud_clock_style">hud_clock_style&nbsp;<span>[<a href="#hud_clock_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of clock</p>
</div>

  <h3 id="hud_face_align_x">hud_face_align_x&nbsp;<span>[<a href="#hud_face_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of player face</p>
</div>

  <h3 id="hud_face_align_y">hud_face_align_y&nbsp;<span>[<a href="#hud_face_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of player face</p>
</div>

  <h3 id="hud_face_frame">hud_face_frame&nbsp;<span>[<a href="#hud_face_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for player face</p>
</div>

  <h3 id="hud_face_place">hud_face_place&nbsp;<span>[<a href="#hud_face_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for player face</p>
</div>

  <h3 id="hud_face_pos_x">hud_face_pos_x&nbsp;<span>[<a href="#hud_face_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of player face</p>
</div>

  <h3 id="hud_face_pos_y">hud_face_pos_y&nbsp;<span>[<a href="#hud_face_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of player face</p>
</div>

  <h3 id="hud_face_scale">hud_face_scale&nbsp;<span>[<a href="#hud_face_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of player face</p>
</div>

  <h3 id="hud_face_show">hud_face_show&nbsp;<span>[<a href="#hud_face_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of player face</p>
</div>

  <h3 id="hud_fps_align_x">hud_fps_align_x&nbsp;<span>[<a href="#hud_fps_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of fps counter</p>
</div>

  <h3 id="hud_fps_align_y">hud_fps_align_y&nbsp;<span>[<a href="#hud_fps_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>bottom</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of fps counter</p>
</div>

  <h3 id="hud_fps_frame">hud_fps_frame&nbsp;<span>[<a href="#hud_fps_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for fps counter</p>
</div>

  <h3 id="hud_fps_place">hud_fps_place&nbsp;<span>[<a href="#hud_fps_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for fps counter</p>
</div>

  <h3 id="hud_fps_pos_x">hud_fps_pos_x&nbsp;<span>[<a href="#hud_fps_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of fps counter</p>
</div>

  <h3 id="hud_fps_pos_y">hud_fps_pos_y&nbsp;<span>[<a href="#hud_fps_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of fps counter</p>
</div>

  <h3 id="hud_fps_show">hud_fps_show&nbsp;<span>[<a href="#hud_fps_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of fps counter</p>
</div>

  <h3 id="hud_fps_show_min">hud_fps_show_min&nbsp;<span>[<a href="#hud_fps_show_min">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of fps counter</p>
</div>

  <h3 id="hud_fps_title">hud_fps_title&nbsp;<span>[<a href="#hud_fps_title">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches displaying the text "fps" of fps counter</p>
</div>

  <h3 id="hud_frags_align_x">hud_frags_align_x&nbsp;<span>[<a href="#hud_frags_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of frags bar</p>
</div>

  <h3 id="hud_frags_align_y">hud_frags_align_y&nbsp;<span>[<a href="#hud_frags_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of frags bar</p>
</div>

  <h3 id="hud_frags_cell_height">hud_frags_cell_height&nbsp;<span>[<a href="#hud_frags_cell_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>8</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets cell height of frags bar</p>
</div>

  <h3 id="hud_frags_cell_width">hud_frags_cell_width&nbsp;<span>[<a href="#hud_frags_cell_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>32</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets cell width of frags bar</p>
</div>

  <h3 id="hud_frags_cols">hud_frags_cols&nbsp;<span>[<a href="#hud_frags_cols">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>4</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets number of columns of frags bar</p>
</div>

  <h3 id="hud_frags_fliptext">hud_frags_fliptext&nbsp;<span>[<a href="#hud_frags_fliptext">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles alignment of players nick and team name in frags HUD element.</p>
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
        <td>Rows aligned to the left side, names and team tags are on the right side of frag counts.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Rows aligned to the right side, names and team tags are on the left side of frag counts.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>Use 'frags shownames 1' and/or 'frags showteams 1' to show names and team names of players.</p>
</div>

  <h3 id="hud_frags_frame">hud_frags_frame&nbsp;<span>[<a href="#hud_frags_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for frags bar</p>
</div>

  <h3 id="hud_frags_padtext">hud_frags_padtext&nbsp;<span>[<a href="#hud_frags_padtext">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles text padding in 'frags' HUD element.</p>
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
        <td>No spaces between text fields.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Adds padding so frags, names, team tags aligned into columns.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>Use 'frags shownames 1' and/or 'frags showteams 1'.</p>
</div>

  <h3 id="hud_frags_place">hud_frags_place&nbsp;<span>[<a href="#hud_frags_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for frags bar</p>
</div>

  <h3 id="hud_frags_pos_x">hud_frags_pos_x&nbsp;<span>[<a href="#hud_frags_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of frags bar</p>
</div>

  <h3 id="hud_frags_pos_y">hud_frags_pos_y&nbsp;<span>[<a href="#hud_frags_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of frags bar</p>
</div>

  <h3 id="hud_frags_rows">hud_frags_rows&nbsp;<span>[<a href="#hud_frags_rows">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets number of rows in frags bar</p>
</div>

  <h3 id="hud_frags_show">hud_frags_show&nbsp;<span>[<a href="#hud_frags_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of frags bar</p>
</div>

  <h3 id="hud_frags_shownames">hud_frags_shownames&nbsp;<span>[<a href="#hud_frags_shownames">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Draws players names next to frag counts in 'frags' HUD element.</p>
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
        <td>Don't display players' names.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Display players' names.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_frags_showself_always">hud_frags_showself_always&nbsp;<span>[<a href="#hud_frags_showself_always">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Forces the client to show the part of frags table where you are.</p>
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
        <td>Display highest frags only.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Display the part of table where you are.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_frags_showteams">hud_frags_showteams&nbsp;<span>[<a href="#hud_frags_showteams">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Draws players' team tags next to frag counts in 'frags' HUD element.</p>
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
        <td>Do not display players' team tags.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Display players' team tags.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_frags_space_x">hud_frags_space_x&nbsp;<span>[<a href="#hud_frags_space_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical spacing of frags bar</p>
</div>

  <h3 id="hud_frags_space_y">hud_frags_space_y&nbsp;<span>[<a href="#hud_frags_space_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal spacing of frags bar</p>
</div>

  <h3 id="hud_frags_strip">hud_frags_strip&nbsp;<span>[<a href="#hud_frags_strip">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches stripped version of frags bar</p>
</div>

  <h3 id="hud_frags_style">hud_frags_style&nbsp;<span>[<a href="#hud_frags_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets drawing style of 'frags' HUD element.</p>
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
        <td>Golden brackets around your own frags field.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Arrow pointing to your own frags field.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Red rectangle around your own frags fiels.</td>
      </tr>
      <tr>
        <td>3</td>
        <td>Similar to 1.</td>
      </tr>
      <tr>
        <td>4</td>
        <td>Sets background color for your own field to 'teamcolor'.</td>
      </tr>
      <tr>
        <td>5</td>
        <td>Sets background color for all field to teamcolors enemycolors, all fields 50% transparent, your own field not transparent. Red rectangle around your own field including name and team tag.</td>
      </tr>
      <tr>
        <td>6</td>
        <td>Red rectangle around your own field including name and team tag. Background color only for your own field and set to 'teamcolor'.</td>
      </tr>
      <tr>
        <td>7</td>
        <td>Background color for whole table 50% transparent red, your own field not transparent.</td>
      </tr>
      <tr>
        <td>8</td>
        <td>Red background color only for your own field including name and team tag.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_frags_teamsort">hud_frags_teamsort&nbsp;<span>[<a href="#hud_frags_teamsort">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches sorting by teams in frags bar</p>
</div>

  <h3 id="hud_frags_vertical">hud_frags_vertical&nbsp;<span>[<a href="#hud_frags_vertical">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches vertical rendering of frags bar</p>
</div>

  <h3 id="hud_group1_align_x">hud_group1_align_x&nbsp;<span>[<a href="#hud_group1_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grouping object 1</p>
</div>

  <h3 id="hud_group1_align_y">hud_group1_align_y&nbsp;<span>[<a href="#hud_group1_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grouping object 1</p>
</div>

  <h3 id="hud_group1_alpha">hud_group1_alpha&nbsp;<span>[<a href="#hud_group1_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets level of transparency of grouping object 1</p>
</div>

  <h3 id="hud_group1_frame">hud_group1_frame&nbsp;<span>[<a href="#hud_group1_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>.5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grouping object 1</p>
</div>

  <h3 id="hud_group1_height">hud_group1_height&nbsp;<span>[<a href="#hud_group1_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of grouping object 1</p>
</div>

  <h3 id="hud_group1_name">hud_group1_name&nbsp;<span>[<a href="#hud_group1_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>group1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets name of grouping object 1</p>
</div>

  <h3 id="hud_group1_picture">hud_group1_picture&nbsp;<span>[<a href="#hud_group1_picture">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Sets background picture of grouping object 1</p>
</div>

  <h3 id="hud_group1_place">hud_group1_place&nbsp;<span>[<a href="#hud_group1_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grouping object 1</p>
</div>

  <h3 id="hud_group1_pos_x">hud_group1_pos_x&nbsp;<span>[<a href="#hud_group1_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grouping object 1</p>
</div>

  <h3 id="hud_group1_pos_y">hud_group1_pos_y&nbsp;<span>[<a href="#hud_group1_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grouping object 1</p>
</div>

  <h3 id="hud_group1_show">hud_group1_show&nbsp;<span>[<a href="#hud_group1_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grouping object 1</p>
</div>

  <h3 id="hud_group1_tile">hud_group1_tile&nbsp;<span>[<a href="#hud_group1_tile">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches tiling of background picture of grouping object 1</p>
</div>

  <h3 id="hud_group1_width">hud_group1_width&nbsp;<span>[<a href="#hud_group1_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of grouping object 1</p>
</div>

  <h3 id="hud_group2_align_x">hud_group2_align_x&nbsp;<span>[<a href="#hud_group2_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grouping object 2</p>
</div>

  <h3 id="hud_group2_align_y">hud_group2_align_y&nbsp;<span>[<a href="#hud_group2_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grouping object 2</p>
</div>

  <h3 id="hud_group2_alpha">hud_group2_alpha&nbsp;<span>[<a href="#hud_group2_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets transparency level of grouping object 2</p>
</div>

  <h3 id="hud_group2_frame">hud_group2_frame&nbsp;<span>[<a href="#hud_group2_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>.5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grouping object 2</p>
</div>

  <h3 id="hud_group2_height">hud_group2_height&nbsp;<span>[<a href="#hud_group2_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of grouping object 2</p>
</div>

  <h3 id="hud_group2_name">hud_group2_name&nbsp;<span>[<a href="#hud_group2_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>group2</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets name of grouping object 2</p>
</div>

  <h3 id="hud_group2_picture">hud_group2_picture&nbsp;<span>[<a href="#hud_group2_picture">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Sets background picture of grouping object 2</p>
</div>

  <h3 id="hud_group2_place">hud_group2_place&nbsp;<span>[<a href="#hud_group2_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grouping object 2</p>
</div>

  <h3 id="hud_group2_pos_x">hud_group2_pos_x&nbsp;<span>[<a href="#hud_group2_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grouping object 2</p>
</div>

  <h3 id="hud_group2_pos_y">hud_group2_pos_y&nbsp;<span>[<a href="#hud_group2_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grouping object 2</p>
</div>

  <h3 id="hud_group2_show">hud_group2_show&nbsp;<span>[<a href="#hud_group2_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grouping object 2</p>
</div>

  <h3 id="hud_group2_tile">hud_group2_tile&nbsp;<span>[<a href="#hud_group2_tile">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches tiling of background picture of grouping object 2</p>
</div>

  <h3 id="hud_group2_width">hud_group2_width&nbsp;<span>[<a href="#hud_group2_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of grouping object 2</p>
</div>

  <h3 id="hud_group3_align_x">hud_group3_align_x&nbsp;<span>[<a href="#hud_group3_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grouping object 3</p>
</div>

  <h3 id="hud_group3_align_y">hud_group3_align_y&nbsp;<span>[<a href="#hud_group3_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grouping object 3</p>
</div>

  <h3 id="hud_group3_alpha">hud_group3_alpha&nbsp;<span>[<a href="#hud_group3_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets transparency level of grouping object 3</p>
</div>

  <h3 id="hud_group3_frame">hud_group3_frame&nbsp;<span>[<a href="#hud_group3_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>.5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grouping object 3</p>
</div>

  <h3 id="hud_group3_height">hud_group3_height&nbsp;<span>[<a href="#hud_group3_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of grouping object 3</p>
</div>

  <h3 id="hud_group3_name">hud_group3_name&nbsp;<span>[<a href="#hud_group3_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>group3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets name of grouping object 3</p>
</div>

  <h3 id="hud_group3_picture">hud_group3_picture&nbsp;<span>[<a href="#hud_group3_picture">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Sets background picture of grouping object 3</p>
</div>

  <h3 id="hud_group3_place">hud_group3_place&nbsp;<span>[<a href="#hud_group3_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grouping object 3</p>
</div>

  <h3 id="hud_group3_pos_x">hud_group3_pos_x&nbsp;<span>[<a href="#hud_group3_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grouping object 3</p>
</div>

  <h3 id="hud_group3_pos_y">hud_group3_pos_y&nbsp;<span>[<a href="#hud_group3_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grouping object 3</p>
</div>

  <h3 id="hud_group3_show">hud_group3_show&nbsp;<span>[<a href="#hud_group3_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grouping object 3</p>
</div>

  <h3 id="hud_group3_tile">hud_group3_tile&nbsp;<span>[<a href="#hud_group3_tile">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches tiling of background picture of grouping object 3</p>
</div>

  <h3 id="hud_group3_width">hud_group3_width&nbsp;<span>[<a href="#hud_group3_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of grouping object 3</p>
</div>

  <h3 id="hud_group4_align_x">hud_group4_align_x&nbsp;<span>[<a href="#hud_group4_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grouping object 4</p>
</div>

  <h3 id="hud_group4_align_y">hud_group4_align_y&nbsp;<span>[<a href="#hud_group4_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grouping object 4</p>
</div>

  <h3 id="hud_group4_alpha">hud_group4_alpha&nbsp;<span>[<a href="#hud_group4_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets transparency level of grouping object 4</p>
</div>

  <h3 id="hud_group4_frame">hud_group4_frame&nbsp;<span>[<a href="#hud_group4_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>.5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grouping object 4</p>
</div>

  <h3 id="hud_group4_height">hud_group4_height&nbsp;<span>[<a href="#hud_group4_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of grouping object 4</p>
</div>

  <h3 id="hud_group4_name">hud_group4_name&nbsp;<span>[<a href="#hud_group4_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>group4</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets name of grouping object 4</p>
</div>

  <h3 id="hud_group4_picture">hud_group4_picture&nbsp;<span>[<a href="#hud_group4_picture">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Sets background picture of grouping object 4</p>
</div>

  <h3 id="hud_group4_place">hud_group4_place&nbsp;<span>[<a href="#hud_group4_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grouping object 4</p>
</div>

  <h3 id="hud_group4_pos_x">hud_group4_pos_x&nbsp;<span>[<a href="#hud_group4_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grouping object 4</p>
</div>

  <h3 id="hud_group4_pos_y">hud_group4_pos_y&nbsp;<span>[<a href="#hud_group4_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grouping object 4</p>
</div>

  <h3 id="hud_group4_show">hud_group4_show&nbsp;<span>[<a href="#hud_group4_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grouping object 4</p>
</div>

  <h3 id="hud_group4_tile">hud_group4_tile&nbsp;<span>[<a href="#hud_group4_tile">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches tiling of background picture of grouping object 4</p>
</div>

  <h3 id="hud_group4_width">hud_group4_width&nbsp;<span>[<a href="#hud_group4_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of grouping object 4</p>
</div>

  <h3 id="hud_group5_align_x">hud_group5_align_x&nbsp;<span>[<a href="#hud_group5_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grouping object 5</p>
</div>

  <h3 id="hud_group5_align_y">hud_group5_align_y&nbsp;<span>[<a href="#hud_group5_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grouping object 5</p>
</div>

  <h3 id="hud_group5_alpha">hud_group5_alpha&nbsp;<span>[<a href="#hud_group5_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets transparency level of grouping object 5</p>
</div>

  <h3 id="hud_group5_frame">hud_group5_frame&nbsp;<span>[<a href="#hud_group5_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>.5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grouping object 5</p>
</div>

  <h3 id="hud_group5_height">hud_group5_height&nbsp;<span>[<a href="#hud_group5_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of grouping object 5</p>
</div>

  <h3 id="hud_group5_name">hud_group5_name&nbsp;<span>[<a href="#hud_group5_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>group5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets name of grouping object 5</p>
</div>

  <h3 id="hud_group5_picture">hud_group5_picture&nbsp;<span>[<a href="#hud_group5_picture">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Sets background picture of grouping object 5</p>
</div>

  <h3 id="hud_group5_place">hud_group5_place&nbsp;<span>[<a href="#hud_group5_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grouping object 5</p>
</div>

  <h3 id="hud_group5_pos_x">hud_group5_pos_x&nbsp;<span>[<a href="#hud_group5_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grouping object 5</p>
</div>

  <h3 id="hud_group5_pos_y">hud_group5_pos_y&nbsp;<span>[<a href="#hud_group5_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grouping object 5</p>
</div>

  <h3 id="hud_group5_show">hud_group5_show&nbsp;<span>[<a href="#hud_group5_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grouping object 5</p>
</div>

  <h3 id="hud_group5_tile">hud_group5_tile&nbsp;<span>[<a href="#hud_group5_tile">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches tiling of background picture of grouping object 5</p>
</div>

  <h3 id="hud_group5_width">hud_group5_width&nbsp;<span>[<a href="#hud_group5_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of grouping object 5</p>
</div>

  <h3 id="hud_group6_align_x">hud_group6_align_x&nbsp;<span>[<a href="#hud_group6_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grouping object 6</p>
</div>

  <h3 id="hud_group6_align_y">hud_group6_align_y&nbsp;<span>[<a href="#hud_group6_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grouping object 6</p>
</div>

  <h3 id="hud_group6_alpha">hud_group6_alpha&nbsp;<span>[<a href="#hud_group6_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets transparency level of grouping object 6</p>
</div>

  <h3 id="hud_group6_frame">hud_group6_frame&nbsp;<span>[<a href="#hud_group6_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>.5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grouping object 6</p>
</div>

  <h3 id="hud_group6_height">hud_group6_height&nbsp;<span>[<a href="#hud_group6_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of grouping object 6</p>
</div>

  <h3 id="hud_group6_name">hud_group6_name&nbsp;<span>[<a href="#hud_group6_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>group6</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets name of grouping object 6</p>
</div>

  <h3 id="hud_group6_picture">hud_group6_picture&nbsp;<span>[<a href="#hud_group6_picture">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Sets background picture of grouping object 6</p>
</div>

  <h3 id="hud_group6_place">hud_group6_place&nbsp;<span>[<a href="#hud_group6_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grouping object 6</p>
</div>

  <h3 id="hud_group6_pos_x">hud_group6_pos_x&nbsp;<span>[<a href="#hud_group6_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grouping object 6</p>
</div>

  <h3 id="hud_group6_pos_y">hud_group6_pos_y&nbsp;<span>[<a href="#hud_group6_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grouping object 6</p>
</div>

  <h3 id="hud_group6_show">hud_group6_show&nbsp;<span>[<a href="#hud_group6_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grouping object 6</p>
</div>

  <h3 id="hud_group6_tile">hud_group6_tile&nbsp;<span>[<a href="#hud_group6_tile">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches tiling of background picture of grouping object 6</p>
</div>

  <h3 id="hud_group6_width">hud_group6_width&nbsp;<span>[<a href="#hud_group6_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of grouping object 6</p>
</div>

  <h3 id="hud_group7_align_x">hud_group7_align_x&nbsp;<span>[<a href="#hud_group7_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grouping object 7</p>
</div>

  <h3 id="hud_group7_align_y">hud_group7_align_y&nbsp;<span>[<a href="#hud_group7_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grouping object 7</p>
</div>

  <h3 id="hud_group7_alpha">hud_group7_alpha&nbsp;<span>[<a href="#hud_group7_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets transparency level of grouping object 7</p>
</div>

  <h3 id="hud_group7_frame">hud_group7_frame&nbsp;<span>[<a href="#hud_group7_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>.5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grouping object 7</p>
</div>

  <h3 id="hud_group7_height">hud_group7_height&nbsp;<span>[<a href="#hud_group7_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of grouping object 7</p>
</div>

  <h3 id="hud_group7_name">hud_group7_name&nbsp;<span>[<a href="#hud_group7_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>group7</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets name of grouping object 7</p>
</div>

  <h3 id="hud_group7_picture">hud_group7_picture&nbsp;<span>[<a href="#hud_group7_picture">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Sets background picture of grouping object 7</p>
</div>

  <h3 id="hud_group7_place">hud_group7_place&nbsp;<span>[<a href="#hud_group7_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grouping object 7</p>
</div>

  <h3 id="hud_group7_pos_x">hud_group7_pos_x&nbsp;<span>[<a href="#hud_group7_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grouping object 7</p>
</div>

  <h3 id="hud_group7_pos_y">hud_group7_pos_y&nbsp;<span>[<a href="#hud_group7_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grouping object 7</p>
</div>

  <h3 id="hud_group7_show">hud_group7_show&nbsp;<span>[<a href="#hud_group7_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grouping object 7</p>
</div>

  <h3 id="hud_group7_tile">hud_group7_tile&nbsp;<span>[<a href="#hud_group7_tile">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches tiling of background picture of grouping object 7</p>
</div>

  <h3 id="hud_group7_width">hud_group7_width&nbsp;<span>[<a href="#hud_group7_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of grouping object 7</p>
</div>

  <h3 id="hud_group8_align_x">hud_group8_align_x&nbsp;<span>[<a href="#hud_group8_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grouping object 8</p>
</div>

  <h3 id="hud_group8_align_y">hud_group8_align_y&nbsp;<span>[<a href="#hud_group8_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grouping object 8</p>
</div>

  <h3 id="hud_group8_alpha">hud_group8_alpha&nbsp;<span>[<a href="#hud_group8_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets transparency level of grouping object 8</p>
</div>

  <h3 id="hud_group8_frame">hud_group8_frame&nbsp;<span>[<a href="#hud_group8_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>.5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grouping object 8</p>
</div>

  <h3 id="hud_group8_height">hud_group8_height&nbsp;<span>[<a href="#hud_group8_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of grouping object 8</p>
</div>

  <h3 id="hud_group8_name">hud_group8_name&nbsp;<span>[<a href="#hud_group8_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>group8</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets name of grouping object 8</p>
</div>

  <h3 id="hud_group8_picture">hud_group8_picture&nbsp;<span>[<a href="#hud_group8_picture">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Sets background picture of grouping object 8</p>
</div>

  <h3 id="hud_group8_place">hud_group8_place&nbsp;<span>[<a href="#hud_group8_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grouping object 8</p>
</div>

  <h3 id="hud_group8_pos_x">hud_group8_pos_x&nbsp;<span>[<a href="#hud_group8_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grouping object 8</p>
</div>

  <h3 id="hud_group8_pos_y">hud_group8_pos_y&nbsp;<span>[<a href="#hud_group8_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grouping object 8</p>
</div>

  <h3 id="hud_group8_show">hud_group8_show&nbsp;<span>[<a href="#hud_group8_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grouping object 8</p>
</div>

  <h3 id="hud_group8_tile">hud_group8_tile&nbsp;<span>[<a href="#hud_group8_tile">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches tiling of background picture of grouping object 8</p>
</div>

  <h3 id="hud_group8_width">hud_group8_width&nbsp;<span>[<a href="#hud_group8_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of grouping object 8</p>
</div>

  <h3 id="hud_group9_align_x">hud_group9_align_x&nbsp;<span>[<a href="#hud_group9_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grouping object 9</p>
</div>

  <h3 id="hud_group9_align_y">hud_group9_align_y&nbsp;<span>[<a href="#hud_group9_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grouping object 9</p>
</div>

  <h3 id="hud_group9_alpha">hud_group9_alpha&nbsp;<span>[<a href="#hud_group9_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets transparency level of grouping object 9</p>
</div>

  <h3 id="hud_group9_frame">hud_group9_frame&nbsp;<span>[<a href="#hud_group9_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>.5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grouping object 9</p>
</div>

  <h3 id="hud_group9_height">hud_group9_height&nbsp;<span>[<a href="#hud_group9_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of grouping object 9</p>
</div>

  <h3 id="hud_group9_name">hud_group9_name&nbsp;<span>[<a href="#hud_group9_name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>group9</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets name of grouping object 9</p>
</div>

  <h3 id="hud_group9_picture">hud_group9_picture&nbsp;<span>[<a href="#hud_group9_picture">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Sets background picture of grouping object 9</p>
</div>

  <h3 id="hud_group9_place">hud_group9_place&nbsp;<span>[<a href="#hud_group9_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grouping object 9</p>
</div>

  <h3 id="hud_group9_pos_x">hud_group9_pos_x&nbsp;<span>[<a href="#hud_group9_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grouping object 9</p>
</div>

  <h3 id="hud_group9_pos_y">hud_group9_pos_y&nbsp;<span>[<a href="#hud_group9_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grouping object 9</p>
</div>

  <h3 id="hud_group9_show">hud_group9_show&nbsp;<span>[<a href="#hud_group9_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grouping object 9</p>
</div>

  <h3 id="hud_group9_tile">hud_group9_tile&nbsp;<span>[<a href="#hud_group9_tile">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches tiling of background picture of grouping object 9</p>
</div>

  <h3 id="hud_group9_width">hud_group9_width&nbsp;<span>[<a href="#hud_group9_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of grouping object 9</p>
</div>

  <h3 id="hud_gun2_align_x">hud_gun2_align_x&nbsp;<span>[<a href="#hud_gun2_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of shotgun icon</p>
</div>

  <h3 id="hud_gun2_align_y">hud_gun2_align_y&nbsp;<span>[<a href="#hud_gun2_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>bottom</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of shotgun icon</p>
</div>

  <h3 id="hud_gun2_frame">hud_gun2_frame&nbsp;<span>[<a href="#hud_gun2_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for shotgun icon</p>
</div>

  <h3 id="hud_gun2_place">hud_gun2_place&nbsp;<span>[<a href="#hud_gun2_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for shotgun icon</p>
</div>

  <h3 id="hud_gun2_pos_x">hud_gun2_pos_x&nbsp;<span>[<a href="#hud_gun2_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of shotgun icon</p>
</div>

  <h3 id="hud_gun2_pos_y">hud_gun2_pos_y&nbsp;<span>[<a href="#hud_gun2_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of shotgun icon</p>
</div>

  <h3 id="hud_gun2_scale">hud_gun2_scale&nbsp;<span>[<a href="#hud_gun2_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of shotgun icon</p>
</div>

  <h3 id="hud_gun2_show">hud_gun2_show&nbsp;<span>[<a href="#hud_gun2_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of shotgun icon</p>
</div>

  <h3 id="hud_gun2_style">hud_gun2_style&nbsp;<span>[<a href="#hud_gun2_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of shotgun icon</p>
</div>

  <h3 id="hud_gun3_align_x">hud_gun3_align_x&nbsp;<span>[<a href="#hud_gun3_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of super shotgun icon</p>
</div>

  <h3 id="hud_gun3_align_y">hud_gun3_align_y&nbsp;<span>[<a href="#hud_gun3_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>center</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of super shotgun icon</p>
</div>

  <h3 id="hud_gun3_frame">hud_gun3_frame&nbsp;<span>[<a href="#hud_gun3_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for super shotgun icon</p>
</div>

  <h3 id="hud_gun3_place">hud_gun3_place&nbsp;<span>[<a href="#hud_gun3_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>gun2</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for super shotgun icon</p>
</div>

  <h3 id="hud_gun3_pos_x">hud_gun3_pos_x&nbsp;<span>[<a href="#hud_gun3_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of super shotgun icon</p>
</div>

  <h3 id="hud_gun3_pos_y">hud_gun3_pos_y&nbsp;<span>[<a href="#hud_gun3_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of super shotgun icon</p>
</div>

  <h3 id="hud_gun3_scale">hud_gun3_scale&nbsp;<span>[<a href="#hud_gun3_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of super shotgun icon</p>
</div>

  <h3 id="hud_gun3_show">hud_gun3_show&nbsp;<span>[<a href="#hud_gun3_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of super shotgun icon</p>
</div>

  <h3 id="hud_gun3_style">hud_gun3_style&nbsp;<span>[<a href="#hud_gun3_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of super shotgun icon</p>
</div>

  <h3 id="hud_gun4_align_x">hud_gun4_align_x&nbsp;<span>[<a href="#hud_gun4_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of nailgun icon</p>
</div>

  <h3 id="hud_gun4_align_y">hud_gun4_align_y&nbsp;<span>[<a href="#hud_gun4_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>center</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of nailgun icon</p>
</div>

  <h3 id="hud_gun4_frame">hud_gun4_frame&nbsp;<span>[<a href="#hud_gun4_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for nailgun icon</p>
</div>

  <h3 id="hud_gun4_place">hud_gun4_place&nbsp;<span>[<a href="#hud_gun4_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>gun3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for nailgun icon</p>
</div>

  <h3 id="hud_gun4_pos_x">hud_gun4_pos_x&nbsp;<span>[<a href="#hud_gun4_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of nailgun icon</p>
</div>

  <h3 id="hud_gun4_pos_y">hud_gun4_pos_y&nbsp;<span>[<a href="#hud_gun4_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of nailgun icon</p>
</div>

  <h3 id="hud_gun4_scale">hud_gun4_scale&nbsp;<span>[<a href="#hud_gun4_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of nailgun icon</p>
</div>

  <h3 id="hud_gun4_show">hud_gun4_show&nbsp;<span>[<a href="#hud_gun4_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of nailgun icon</p>
</div>

  <h3 id="hud_gun4_style">hud_gun4_style&nbsp;<span>[<a href="#hud_gun4_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of nailgun icon</p>
</div>

  <h3 id="hud_gun5_align_x">hud_gun5_align_x&nbsp;<span>[<a href="#hud_gun5_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of super nailgun icon</p>
</div>

  <h3 id="hud_gun5_align_y">hud_gun5_align_y&nbsp;<span>[<a href="#hud_gun5_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>center</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of super nailgun icon</p>
</div>

  <h3 id="hud_gun5_frame">hud_gun5_frame&nbsp;<span>[<a href="#hud_gun5_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for super nailgun icon</p>
</div>

  <h3 id="hud_gun5_place">hud_gun5_place&nbsp;<span>[<a href="#hud_gun5_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>gun4</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for super nailgun icon</p>
</div>

  <h3 id="hud_gun5_pos_x">hud_gun5_pos_x&nbsp;<span>[<a href="#hud_gun5_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of super nailgun icon</p>
</div>

  <h3 id="hud_gun5_pos_y">hud_gun5_pos_y&nbsp;<span>[<a href="#hud_gun5_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of super nailgun icon</p>
</div>

  <h3 id="hud_gun5_scale">hud_gun5_scale&nbsp;<span>[<a href="#hud_gun5_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of super nailgun icon</p>
</div>

  <h3 id="hud_gun5_show">hud_gun5_show&nbsp;<span>[<a href="#hud_gun5_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of super nailgun icon</p>
</div>

  <h3 id="hud_gun5_style">hud_gun5_style&nbsp;<span>[<a href="#hud_gun5_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of super nailgun icon</p>
</div>

  <h3 id="hud_gun6_align_x">hud_gun6_align_x&nbsp;<span>[<a href="#hud_gun6_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of grenade launcher icon</p>
</div>

  <h3 id="hud_gun6_align_y">hud_gun6_align_y&nbsp;<span>[<a href="#hud_gun6_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>center</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of grenade launcher icon</p>
</div>

  <h3 id="hud_gun6_frame">hud_gun6_frame&nbsp;<span>[<a href="#hud_gun6_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for grenade launcher icon</p>
</div>

  <h3 id="hud_gun6_place">hud_gun6_place&nbsp;<span>[<a href="#hud_gun6_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>gun5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for grenade launcher icon</p>
</div>

  <h3 id="hud_gun6_pos_x">hud_gun6_pos_x&nbsp;<span>[<a href="#hud_gun6_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of grenade launcher icon</p>
</div>

  <h3 id="hud_gun6_pos_y">hud_gun6_pos_y&nbsp;<span>[<a href="#hud_gun6_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of grenade launcher icon</p>
</div>

  <h3 id="hud_gun6_scale">hud_gun6_scale&nbsp;<span>[<a href="#hud_gun6_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of grenade launcher icon</p>
</div>

  <h3 id="hud_gun6_show">hud_gun6_show&nbsp;<span>[<a href="#hud_gun6_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of grenade launcher icon</p>
</div>

  <h3 id="hud_gun6_style">hud_gun6_style&nbsp;<span>[<a href="#hud_gun6_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of grenade launcher icon</p>
</div>

  <h3 id="hud_gun7_align_x">hud_gun7_align_x&nbsp;<span>[<a href="#hud_gun7_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of rocket launcher icon</p>
</div>

  <h3 id="hud_gun7_align_y">hud_gun7_align_y&nbsp;<span>[<a href="#hud_gun7_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>center</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of rocket launcher icon</p>
</div>

  <h3 id="hud_gun7_frame">hud_gun7_frame&nbsp;<span>[<a href="#hud_gun7_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for rocket launcher icon</p>
</div>

  <h3 id="hud_gun7_place">hud_gun7_place&nbsp;<span>[<a href="#hud_gun7_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>gun6</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for rocket launcher icon</p>
</div>

  <h3 id="hud_gun7_pos_x">hud_gun7_pos_x&nbsp;<span>[<a href="#hud_gun7_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of rocket launcher icon</p>
</div>

  <h3 id="hud_gun7_pos_y">hud_gun7_pos_y&nbsp;<span>[<a href="#hud_gun7_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of rocket launcher icon</p>
</div>

  <h3 id="hud_gun7_scale">hud_gun7_scale&nbsp;<span>[<a href="#hud_gun7_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of rocket launcher icon</p>
</div>

  <h3 id="hud_gun7_show">hud_gun7_show&nbsp;<span>[<a href="#hud_gun7_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of rocket launcher icon</p>
</div>

  <h3 id="hud_gun7_style">hud_gun7_style&nbsp;<span>[<a href="#hud_gun7_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of rocket launcher icon</p>
</div>

  <h3 id="hud_gun8_align_x">hud_gun8_align_x&nbsp;<span>[<a href="#hud_gun8_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of lightning gun icon</p>
</div>

  <h3 id="hud_gun8_align_y">hud_gun8_align_y&nbsp;<span>[<a href="#hud_gun8_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>center</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of lightning gun icon</p>
</div>

  <h3 id="hud_gun8_frame">hud_gun8_frame&nbsp;<span>[<a href="#hud_gun8_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for lightning gun icon</p>
</div>

  <h3 id="hud_gun8_place">hud_gun8_place&nbsp;<span>[<a href="#hud_gun8_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>gun7</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for lightning gun icon</p>
</div>

  <h3 id="hud_gun8_pos_x">hud_gun8_pos_x&nbsp;<span>[<a href="#hud_gun8_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of lightning gun icon</p>
</div>

  <h3 id="hud_gun8_pos_y">hud_gun8_pos_y&nbsp;<span>[<a href="#hud_gun8_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of lightning gun icon</p>
</div>

  <h3 id="hud_gun8_scale">hud_gun8_scale&nbsp;<span>[<a href="#hud_gun8_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of lightning gun icon</p>
</div>

  <h3 id="hud_gun8_show">hud_gun8_show&nbsp;<span>[<a href="#hud_gun8_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of lightning gun icon</p>
</div>

  <h3 id="hud_gun8_style">hud_gun8_style&nbsp;<span>[<a href="#hud_gun8_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of lightning gun icon</p>
</div>

  <h3 id="hud_gun8_wide">hud_gun8_wide&nbsp;<span>[<a href="#hud_gun8_wide">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches wide and short of version of lightning gun icon</p>
</div>

  <h3 id="hud_gun_align_x">hud_gun_align_x&nbsp;<span>[<a href="#hud_gun_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>center</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of current weapon icon</p>
</div>

  <h3 id="hud_gun_align_y">hud_gun_align_y&nbsp;<span>[<a href="#hud_gun_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>bottom</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of current weapon icon</p>
</div>

  <h3 id="hud_gun_frame">hud_gun_frame&nbsp;<span>[<a href="#hud_gun_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for current weapon icon</p>
</div>

  <h3 id="hud_gun_place">hud_gun_place&nbsp;<span>[<a href="#hud_gun_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for current weapon icon</p>
</div>

  <h3 id="hud_gun_pos_x">hud_gun_pos_x&nbsp;<span>[<a href="#hud_gun_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of current weapon icon</p>
</div>

  <h3 id="hud_gun_pos_y">hud_gun_pos_y&nbsp;<span>[<a href="#hud_gun_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of current weapon icon</p>
</div>

  <h3 id="hud_gun_scale">hud_gun_scale&nbsp;<span>[<a href="#hud_gun_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of current weapon icon</p>
</div>

  <h3 id="hud_gun_show">hud_gun_show&nbsp;<span>[<a href="#hud_gun_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of current weapon icon</p>
</div>

  <h3 id="hud_gun_style">hud_gun_style&nbsp;<span>[<a href="#hud_gun_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of current weapon icon</p>
</div>

  <h3 id="hud_gun_wide">hud_gun_wide&nbsp;<span>[<a href="#hud_gun_wide">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches between wide and short version of current weapon icon</p>
</div>

  <h3 id="hud_health_align">hud_health_align&nbsp;<span>[<a href="#hud_health_align">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets align of health level</p>
</div>

  <h3 id="hud_health_align_x">hud_health_align_x&nbsp;<span>[<a href="#hud_health_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of health level</p>
</div>

  <h3 id="hud_health_align_y">hud_health_align_y&nbsp;<span>[<a href="#hud_health_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of health level</p>
</div>

  <h3 id="hud_health_digits">hud_health_digits&nbsp;<span>[<a href="#hud_health_digits">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets number of digits for health level</p>
</div>

  <h3 id="hud_health_frame">hud_health_frame&nbsp;<span>[<a href="#hud_health_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for health level</p>
</div>

  <h3 id="hud_health_place">hud_health_place&nbsp;<span>[<a href="#hud_health_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for health level</p>
</div>

  <h3 id="hud_health_pos_x">hud_health_pos_x&nbsp;<span>[<a href="#hud_health_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of health level</p>
</div>

  <h3 id="hud_health_pos_y">hud_health_pos_y&nbsp;<span>[<a href="#hud_health_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of health level</p>
</div>

  <h3 id="hud_health_scale">hud_health_scale&nbsp;<span>[<a href="#hud_health_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of health level</p>
</div>

  <h3 id="hud_health_show">hud_health_show&nbsp;<span>[<a href="#hud_health_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of health level</p>
</div>

  <h3 id="hud_health_style">hud_health_style&nbsp;<span>[<a href="#hud_health_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of health level</p>
</div>

  <h3 id="hud_healthdamage_align">hud_healthdamage_align&nbsp;<span>[<a href="#hud_healthdamage_align">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>Sets healthdamage HUD element alignment.</p>
  <h4>Value</h4>
    <p>String. horizontal: left, center, right, before, after; vertical: top, center, bottom, console, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_healthdamage_align_x">hud_healthdamage_align_x&nbsp;<span>[<a href="#hud_healthdamage_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>Sets healthdamage HUD element horizontal alignment.</p>
  <h4>Value</h4>
    <p>String. left, center, right, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_healthdamage_align_y">hud_healthdamage_align_y&nbsp;<span>[<a href="#hud_healthdamage_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>before</span></p>
<div>
  <h4>Description</h4>
  <p>Sets healthdamage HUD element vertical alignment.</p>
  <h4>Value</h4>
    <p>String. top, center, bottom, console, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_healthdamage_digits">hud_healthdamage_digits&nbsp;<span>[<a href="#hud_healthdamage_digits">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>Sets highest possible number of digits displayed in HUD element healthdamage.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_healthdamage_duration">hud_healthdamage_duration&nbsp;<span>[<a href="#hud_healthdamage_duration">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0.8</span></p>
<div>
  <h4>Description</h4>
  <p>Sets how long healthdamage should be visible after last damage to health has been done.</p>
  <h4>Value</h4>
    <p>Float. Float number</p>
  </div>

  <h3 id="hud_healthdamage_frame">hud_healthdamage_frame&nbsp;<span>[<a href="#hud_healthdamage_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets frame visibility and style for this HUD element.</p>
  <h4>Value</h4>
    <p>Float. Use floating point values from range 0..1 to get alpha transparent background or 2 to get bevel frame.</p>
  </div>

  <h3 id="hud_healthdamage_place">hud_healthdamage_place&nbsp;<span>[<a href="#hud_healthdamage_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>health</span></p>
<div>
  <h4>Description</h4>
  <p>Sets placement for this HUD element. You can align some elements relative to other elements.</p>
  <h4>Value</h4>
    <p>String. to place this element inside element B set this variable to "@B", to place it outside B element set this variable to "B"</p>
  <h4>Remarks</h4>
  <p>First you have to decide, if the element that you are locating now (element B) is to be positioned inside another element (element A) or outside it. See HUD manual for more info.</p>
</div>

  <h3 id="hud_healthdamage_pos_x">hud_healthdamage_pos_x&nbsp;<span>[<a href="#hud_healthdamage_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets horizontal position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_healthdamage_pos_y">hud_healthdamage_pos_y&nbsp;<span>[<a href="#hud_healthdamage_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets vertical position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_healthdamage_scale">hud_healthdamage_scale&nbsp;<span>[<a href="#hud_healthdamage_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Sets overall size of this HUD element.</p>
  <h4>Value</h4>
    <p>Float. Use positive floating point numbers.</p>
  </div>

  <h3 id="hud_healthdamage_show">hud_healthdamage_show&nbsp;<span>[<a href="#hud_healthdamage_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles visibility of this HUD element.</p>
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
        <td>Do not draw this element</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw this element</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_healthdamage_style">hud_healthdamage_style&nbsp;<span>[<a href="#hud_healthdamage_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles between different numbers styles.</p>
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
        <td>large font, texture</td>
      </tr>
      <tr>
        <td>1</td>
        <td>small font, gfxwad</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_iammo1_align_x">hud_iammo1_align_x&nbsp;<span>[<a href="#hud_iammo1_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of shells icon</p>
</div>

  <h3 id="hud_iammo1_align_y">hud_iammo1_align_y&nbsp;<span>[<a href="#hud_iammo1_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of shells icon</p>
</div>

  <h3 id="hud_iammo1_frame">hud_iammo1_frame&nbsp;<span>[<a href="#hud_iammo1_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for shells icon</p>
</div>

  <h3 id="hud_iammo1_place">hud_iammo1_place&nbsp;<span>[<a href="#hud_iammo1_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for shells icon</p>
</div>

  <h3 id="hud_iammo1_pos_x">hud_iammo1_pos_x&nbsp;<span>[<a href="#hud_iammo1_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of shells icon</p>
</div>

  <h3 id="hud_iammo1_pos_y">hud_iammo1_pos_y&nbsp;<span>[<a href="#hud_iammo1_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of shells icon</p>
</div>

  <h3 id="hud_iammo1_scale">hud_iammo1_scale&nbsp;<span>[<a href="#hud_iammo1_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of shells icon</p>
</div>

  <h3 id="hud_iammo1_show">hud_iammo1_show&nbsp;<span>[<a href="#hud_iammo1_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of shells icon</p>
</div>

  <h3 id="hud_iammo1_style">hud_iammo1_style&nbsp;<span>[<a href="#hud_iammo1_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of shells icon</p>
</div>

  <h3 id="hud_iammo2_align_x">hud_iammo2_align_x&nbsp;<span>[<a href="#hud_iammo2_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of nails icon</p>
</div>

  <h3 id="hud_iammo2_align_y">hud_iammo2_align_y&nbsp;<span>[<a href="#hud_iammo2_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of nails icon</p>
</div>

  <h3 id="hud_iammo2_frame">hud_iammo2_frame&nbsp;<span>[<a href="#hud_iammo2_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for nails icon</p>
</div>

  <h3 id="hud_iammo2_place">hud_iammo2_place&nbsp;<span>[<a href="#hud_iammo2_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for nails icon</p>
</div>

  <h3 id="hud_iammo2_pos_x">hud_iammo2_pos_x&nbsp;<span>[<a href="#hud_iammo2_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of nails icon</p>
</div>

  <h3 id="hud_iammo2_pos_y">hud_iammo2_pos_y&nbsp;<span>[<a href="#hud_iammo2_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of nails icon</p>
</div>

  <h3 id="hud_iammo2_scale">hud_iammo2_scale&nbsp;<span>[<a href="#hud_iammo2_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of nails icon</p>
</div>

  <h3 id="hud_iammo2_show">hud_iammo2_show&nbsp;<span>[<a href="#hud_iammo2_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of nails icon</p>
</div>

  <h3 id="hud_iammo2_style">hud_iammo2_style&nbsp;<span>[<a href="#hud_iammo2_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of nails icon</p>
</div>

  <h3 id="hud_iammo3_align_x">hud_iammo3_align_x&nbsp;<span>[<a href="#hud_iammo3_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of rockets icon</p>
</div>

  <h3 id="hud_iammo3_align_y">hud_iammo3_align_y&nbsp;<span>[<a href="#hud_iammo3_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of rockets icon</p>
</div>

  <h3 id="hud_iammo3_frame">hud_iammo3_frame&nbsp;<span>[<a href="#hud_iammo3_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for rockets icon</p>
</div>

  <h3 id="hud_iammo3_place">hud_iammo3_place&nbsp;<span>[<a href="#hud_iammo3_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for rockets icon</p>
</div>

  <h3 id="hud_iammo3_pos_x">hud_iammo3_pos_x&nbsp;<span>[<a href="#hud_iammo3_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of rockets icon</p>
</div>

  <h3 id="hud_iammo3_pos_y">hud_iammo3_pos_y&nbsp;<span>[<a href="#hud_iammo3_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of rockets icon</p>
</div>

  <h3 id="hud_iammo3_scale">hud_iammo3_scale&nbsp;<span>[<a href="#hud_iammo3_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of rockets icon</p>
</div>

  <h3 id="hud_iammo3_show">hud_iammo3_show&nbsp;<span>[<a href="#hud_iammo3_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of rockets icon</p>
</div>

  <h3 id="hud_iammo3_style">hud_iammo3_style&nbsp;<span>[<a href="#hud_iammo3_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of rockets icon</p>
</div>

  <h3 id="hud_iammo4_align_x">hud_iammo4_align_x&nbsp;<span>[<a href="#hud_iammo4_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of cells icon</p>
</div>

  <h3 id="hud_iammo4_align_y">hud_iammo4_align_y&nbsp;<span>[<a href="#hud_iammo4_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of cells icon</p>
</div>

  <h3 id="hud_iammo4_frame">hud_iammo4_frame&nbsp;<span>[<a href="#hud_iammo4_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for cells icon</p>
</div>

  <h3 id="hud_iammo4_place">hud_iammo4_place&nbsp;<span>[<a href="#hud_iammo4_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for cells icon</p>
</div>

  <h3 id="hud_iammo4_pos_x">hud_iammo4_pos_x&nbsp;<span>[<a href="#hud_iammo4_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of cells icon</p>
</div>

  <h3 id="hud_iammo4_pos_y">hud_iammo4_pos_y&nbsp;<span>[<a href="#hud_iammo4_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of cells icon</p>
</div>

  <h3 id="hud_iammo4_scale">hud_iammo4_scale&nbsp;<span>[<a href="#hud_iammo4_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of cells icon</p>
</div>

  <h3 id="hud_iammo4_show">hud_iammo4_show&nbsp;<span>[<a href="#hud_iammo4_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of cells icon</p>
</div>

  <h3 id="hud_iammo4_style">hud_iammo4_style&nbsp;<span>[<a href="#hud_iammo4_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of cells icon</p>
</div>

  <h3 id="hud_iammo_align_x">hud_iammo_align_x&nbsp;<span>[<a href="#hud_iammo_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of current ammo icon</p>
</div>

  <h3 id="hud_iammo_align_y">hud_iammo_align_y&nbsp;<span>[<a href="#hud_iammo_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of current ammo icon</p>
</div>

  <h3 id="hud_iammo_frame">hud_iammo_frame&nbsp;<span>[<a href="#hud_iammo_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for current ammo icon</p>
</div>

  <h3 id="hud_iammo_place">hud_iammo_place&nbsp;<span>[<a href="#hud_iammo_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for current ammo icon</p>
</div>

  <h3 id="hud_iammo_pos_x">hud_iammo_pos_x&nbsp;<span>[<a href="#hud_iammo_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of current ammo icon</p>
</div>

  <h3 id="hud_iammo_pos_y">hud_iammo_pos_y&nbsp;<span>[<a href="#hud_iammo_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of current ammo icon</p>
</div>

  <h3 id="hud_iammo_scale">hud_iammo_scale&nbsp;<span>[<a href="#hud_iammo_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of current ammo icon</p>
</div>

  <h3 id="hud_iammo_show">hud_iammo_show&nbsp;<span>[<a href="#hud_iammo_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of current ammo icon</p>
</div>

  <h3 id="hud_iammo_style">hud_iammo_style&nbsp;<span>[<a href="#hud_iammo_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of current ammo icon</p>
</div>

  <h3 id="hud_iarmor_align_x">hud_iarmor_align_x&nbsp;<span>[<a href="#hud_iarmor_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of armor icon</p>
</div>

  <h3 id="hud_iarmor_align_y">hud_iarmor_align_y&nbsp;<span>[<a href="#hud_iarmor_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of armor icon</p>
</div>

  <h3 id="hud_iarmor_frame">hud_iarmor_frame&nbsp;<span>[<a href="#hud_iarmor_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for armor icon</p>
</div>

  <h3 id="hud_iarmor_place">hud_iarmor_place&nbsp;<span>[<a href="#hud_iarmor_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for armor icon</p>
</div>

  <h3 id="hud_iarmor_pos_x">hud_iarmor_pos_x&nbsp;<span>[<a href="#hud_iarmor_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of armor icon</p>
</div>

  <h3 id="hud_iarmor_pos_y">hud_iarmor_pos_y&nbsp;<span>[<a href="#hud_iarmor_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of armor icon</p>
</div>

  <h3 id="hud_iarmor_scale">hud_iarmor_scale&nbsp;<span>[<a href="#hud_iarmor_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of armor icon</p>
</div>

  <h3 id="hud_iarmor_show">hud_iarmor_show&nbsp;<span>[<a href="#hud_iarmor_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of armor icon</p>
</div>

  <h3 id="hud_iarmor_style">hud_iarmor_style&nbsp;<span>[<a href="#hud_iarmor_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of armor icon</p>
</div>

  <h3 id="hud_key1_align_x">hud_key1_align_x&nbsp;<span>[<a href="#hud_key1_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of silver key</p>
</div>

  <h3 id="hud_key1_align_y">hud_key1_align_y&nbsp;<span>[<a href="#hud_key1_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of silver key</p>
</div>

  <h3 id="hud_key1_frame">hud_key1_frame&nbsp;<span>[<a href="#hud_key1_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for silver key</p>
</div>

  <h3 id="hud_key1_place">hud_key1_place&nbsp;<span>[<a href="#hud_key1_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for silver key</p>
</div>

  <h3 id="hud_key1_pos_x">hud_key1_pos_x&nbsp;<span>[<a href="#hud_key1_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of silver key</p>
</div>

  <h3 id="hud_key1_pos_y">hud_key1_pos_y&nbsp;<span>[<a href="#hud_key1_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>64</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of silver key</p>
</div>

  <h3 id="hud_key1_scale">hud_key1_scale&nbsp;<span>[<a href="#hud_key1_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of silver key</p>
</div>

  <h3 id="hud_key1_show">hud_key1_show&nbsp;<span>[<a href="#hud_key1_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of silver key</p>
</div>

  <h3 id="hud_key1_style">hud_key1_style&nbsp;<span>[<a href="#hud_key1_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of silver key</p>
</div>

  <h3 id="hud_key2_align_x">hud_key2_align_x&nbsp;<span>[<a href="#hud_key2_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of gold key</p>
</div>

  <h3 id="hud_key2_align_y">hud_key2_align_y&nbsp;<span>[<a href="#hud_key2_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of gold key</p>
</div>

  <h3 id="hud_key2_frame">hud_key2_frame&nbsp;<span>[<a href="#hud_key2_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for gold key</p>
</div>

  <h3 id="hud_key2_place">hud_key2_place&nbsp;<span>[<a href="#hud_key2_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>key1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for gold key</p>
</div>

  <h3 id="hud_key2_pos_x">hud_key2_pos_x&nbsp;<span>[<a href="#hud_key2_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of gold key</p>
</div>

  <h3 id="hud_key2_pos_y">hud_key2_pos_y&nbsp;<span>[<a href="#hud_key2_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of gold key</p>
</div>

  <h3 id="hud_key2_scale">hud_key2_scale&nbsp;<span>[<a href="#hud_key2_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of gold key</p>
</div>

  <h3 id="hud_key2_show">hud_key2_show&nbsp;<span>[<a href="#hud_key2_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of gold key</p>
</div>

  <h3 id="hud_key2_style">hud_key2_style&nbsp;<span>[<a href="#hud_key2_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of gold key</p>
</div>

  <h3 id="hud_mp3_time_align_x">hud_mp3_time_align_x&nbsp;<span>[<a href="#hud_mp3_time_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>Sets mp3_time HUD element horizontal alignment.</p>
  <h4>Value</h4>
    <p>String. left, center, right, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_mp3_time_align_y">hud_mp3_time_align_y&nbsp;<span>[<a href="#hud_mp3_time_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>bottom</span></p>
<div>
  <h4>Description</h4>
  <p>Sets mp3_time HUD element vertical alignment.</p>
  <h4>Value</h4>
    <p>String. top, center, bottom, console, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_mp3_time_frame">hud_mp3_time_frame&nbsp;<span>[<a href="#hud_mp3_time_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets frame visibility and style for this HUD element.</p>
  <h4>Value</h4>
    <p>Float. Use floating point values from range 0..1 to get alpha transparent background or 2 to get bevel frame.</p>
  </div>

  <h3 id="hud_mp3_time_on_scoreboard">hud_mp3_time_on_scoreboard&nbsp;<span>[<a href="#hud_mp3_time_on_scoreboard">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles whether HUD element 'mp3_time' will be drawn even if scoreboard is displayed.</p>
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
        <td>Hides mp3_time HUD element when scoreboard is ON.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draws mp3_time HUD element even if scoreboard is ON.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_mp3_time_place">hud_mp3_time_place&nbsp;<span>[<a href="#hud_mp3_time_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>Sets placement for this HUD element. You can align some elements relative to other elements.</p>
  <h4>Value</h4>
    <p>String. to place this element inside element B set this variable to "@B", to place it outside B element set this variable to "B"</p>
  <h4>Remarks</h4>
  <p>First you have to decide, if the element that you are locating now (element B) is to be positioned inside another element (element A) or outside it. See HUD manual for more info.</p>
</div>

  <h3 id="hud_mp3_time_pos_x">hud_mp3_time_pos_x&nbsp;<span>[<a href="#hud_mp3_time_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets horizontal position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_mp3_time_pos_y">hud_mp3_time_pos_y&nbsp;<span>[<a href="#hud_mp3_time_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets vertical position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_mp3_time_show">hud_mp3_time_show&nbsp;<span>[<a href="#hud_mp3_time_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles visibility of this HUD element.</p>
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
        <td>Do not draw this element</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw this element</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_mp3_time_style">hud_mp3_time_style&nbsp;<span>[<a href="#hud_mp3_time_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Changes displayed informations for 'mp3_time' HUD element.</p>
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
        <td>Played time / total time of track, golden brackets around.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Remaining time / total time of track, golden brackets around.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Remaining time only, golden brackets around.</td>
      </tr>
      <tr>
        <td>3</td>
        <td>Played time only, golden brackets around.</td>
      </tr>
      <tr>
        <td>4</td>
        <td>Same as 0, without golden brackets.</td>
      </tr>
      <tr>
        <td>5</td>
        <td>Same as 1, without golden brackets.</td>
      </tr>
      <tr>
        <td>6</td>
        <td>Same as 2, without golden brackets.</td>
      </tr>
      <tr>
        <td>7</td>
        <td>Same as 3, without golden brackets.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>Note that display of seconds is disabled in ruleset smackdown.</p>
</div>

  <h3 id="hud_mp3_title_align_x">hud_mp3_title_align_x&nbsp;<span>[<a href="#hud_mp3_title_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>right</span></p>
<div>
  <h4>Description</h4>
  <p>Sets mp3_title HUD element horizontal alignment.</p>
  <h4>Value</h4>
    <p>String. left, center, right, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_mp3_title_align_y">hud_mp3_title_align_y&nbsp;<span>[<a href="#hud_mp3_title_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>bottom</span></p>
<div>
  <h4>Description</h4>
  <p>Sets mp3_title HUD element vertical alignment.</p>
  <h4>Value</h4>
    <p>String. top, center, bottom, console, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_mp3_title_frame">hud_mp3_title_frame&nbsp;<span>[<a href="#hud_mp3_title_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets frame visibility and style for this HUD element.</p>
  <h4>Value</h4>
    <p>Float. Use floating point values from range 0..1 to get alpha transparent background or 2 to get bevel frame.</p>
  </div>

  <h3 id="hud_mp3_title_height">hud_mp3_title_height&nbsp;<span>[<a href="#hud_mp3_title_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>8</span></p>
<div>
  <h4>Description</h4>
  <p>Sets height of 'mp3_title' HUD element.</p>
  <h4>Value</h4>
    <p>Integer. 8, 16, 24, 32 - one, two, three, four rows of text usually.</p>
  </div>

  <h3 id="hud_mp3_title_on_scoreboard">hud_mp3_title_on_scoreboard&nbsp;<span>[<a href="#hud_mp3_title_on_scoreboard">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles whether HUD element 'mp3_title' will be drawn even if scoreboard is displayed.</p>
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
        <td>Hides mp3_title HUD element when scoreboard is ON.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draws mp3_title HUD element even if scoreboard is ON.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_mp3_title_place">hud_mp3_title_place&nbsp;<span>[<a href="#hud_mp3_title_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>Sets placement for this HUD element. You can align some elements relative to other elements.</p>
  <h4>Value</h4>
    <p>String. to place this element inside element B set this variable to "@B", to place it outside B element set this variable to "B"</p>
  <h4>Remarks</h4>
  <p>First you have to decide, if the element that you are locating now (element B) is to be positioned inside another element (element A) or outside it. See HUD manual for more info.</p>
</div>

  <h3 id="hud_mp3_title_pos_x">hud_mp3_title_pos_x&nbsp;<span>[<a href="#hud_mp3_title_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets horizontal position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_mp3_title_pos_y">hud_mp3_title_pos_y&nbsp;<span>[<a href="#hud_mp3_title_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets vertical position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_mp3_title_scroll">hud_mp3_title_scroll&nbsp;<span>[<a href="#hud_mp3_title_scroll">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles scrolling of mp3_title text.</p>
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
        <td>Scrolling disabled.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Scrolling enabled.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_mp3_title_scroll_delay">hud_mp3_title_scroll_delay&nbsp;<span>[<a href="#hud_mp3_title_scroll_delay">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0.5</span></p>
<div>
  <h4>Description</h4>
  <p>Sets speed of text scrolling in mp3_title HUD element.</p>
  <h4>Value</h4>
    <p>Float. The lower the value is the faster scrolling you get.</p>
  <h4>Remarks</h4>
  <p>Use 'hud mp3_title scroll 1'.</p>
</div>

  <h3 id="hud_mp3_title_show">hud_mp3_title_show&nbsp;<span>[<a href="#hud_mp3_title_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles visibility of this HUD element.</p>
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
        <td>Do not draw this element</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw this element</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_mp3_title_style">hud_mp3_title_style&nbsp;<span>[<a href="#hud_mp3_title_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>Sets format of text of 'mp3_title' HUD element.</p>
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
        <td>Shows just winamp song information.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Same as 0 but shows 'Playing: ' before the song information.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Same as 0 but shows 'Playing' in golden brackets before the song information.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_mp3_title_width">hud_mp3_title_width&nbsp;<span>[<a href="#hud_mp3_title_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>512</span></p>
<div>
  <h4>Description</h4>
  <p>Sets width of mp3_title HUD element.</p>
  <h4>Value</h4>
    <p>Integer. 8 = one char with standard console settings.</p>
  </div>

  <h3 id="hud_mp3_title_wordwrap">hud_mp3_title_wordwrap&nbsp;<span>[<a href="#hud_mp3_title_wordwrap">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles word wrapping inside mp3_title element.</p>
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
        <td>Don't wrap words, scroll if necessary and allowed.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Wrap words if more lines are allowed.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>See 'mp3_title height'.</p>
</div>

  <h3 id="hud_net_align_x">hud_net_align_x&nbsp;<span>[<a href="#hud_net_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of net statistics</p>
</div>

  <h3 id="hud_net_align_y">hud_net_align_y&nbsp;<span>[<a href="#hud_net_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>center</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of net statistics</p>
</div>

  <h3 id="hud_net_frame">hud_net_frame&nbsp;<span>[<a href="#hud_net_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0.2</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for net statistics</p>
</div>

  <h3 id="hud_net_period">hud_net_period&nbsp;<span>[<a href="#hud_net_period">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets period for updating net statistics</p>
</div>

  <h3 id="hud_net_place">hud_net_place&nbsp;<span>[<a href="#hud_net_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for net statistics</p>
</div>

  <h3 id="hud_net_pos_x">hud_net_pos_x&nbsp;<span>[<a href="#hud_net_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of net statistics</p>
</div>

  <h3 id="hud_net_pos_y">hud_net_pos_y&nbsp;<span>[<a href="#hud_net_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of net statistics</p>
</div>

  <h3 id="hud_net_show">hud_net_show&nbsp;<span>[<a href="#hud_net_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of net statistics</p>
</div>

  <h3 id="hud_netgraph_align_x">hud_netgraph_align_x&nbsp;<span>[<a href="#hud_netgraph_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of everything about net</p>
</div>

  <h3 id="hud_netgraph_align_y">hud_netgraph_align_y&nbsp;<span>[<a href="#hud_netgraph_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>bottom</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of everything about net</p>
</div>

  <h3 id="hud_netgraph_alpha">hud_netgraph_alpha&nbsp;<span>[<a href="#hud_netgraph_alpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets transparency level of everything about net</p>
</div>

  <h3 id="hud_netgraph_frame">hud_netgraph_frame&nbsp;<span>[<a href="#hud_netgraph_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for everything about net</p>
</div>

  <h3 id="hud_netgraph_full">hud_netgraph_full&nbsp;<span>[<a href="#hud_netgraph_full">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
When turned on, netgraph is more detailed this way, but looks ugly; in GL this can be fixed with a lower alpha value like "0.4"</p>
</div>

  <h3 id="hud_netgraph_height">hud_netgraph_height&nbsp;<span>[<a href="#hud_netgraph_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>32</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical size of everything about net</p>
</div>

  <h3 id="hud_netgraph_inframes">hud_netgraph_inframes&nbsp;<span>[<a href="#hud_netgraph_inframes">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
setting this to "1" lets you measure your latency in an alternate way every level of netgraph will mean one frame of delay, between sending it to server and getting answer. On local/lan server you'll always get one frame of delay, even with low FPS.</p>
</div>

  <h3 id="hud_netgraph_lostscale">hud_netgraph_lostscale&nbsp;<span>[<a href="#hud_netgraph_lostscale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
lets you cut down those red, yellow, blue and gray bars, which are always full-height; possible values are the range from 0 to 1</p>
</div>

  <h3 id="hud_netgraph_place">hud_netgraph_place&nbsp;<span>[<a href="#hud_netgraph_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for everything about net</p>
</div>

  <h3 id="hud_netgraph_ploss">hud_netgraph_ploss&nbsp;<span>[<a href="#hud_netgraph_ploss">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
print packet loss or not everything about net</p>
</div>

  <h3 id="hud_netgraph_pos_x">hud_netgraph_pos_x&nbsp;<span>[<a href="#hud_netgraph_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of everything about net</p>
</div>

  <h3 id="hud_netgraph_pos_y">hud_netgraph_pos_y&nbsp;<span>[<a href="#hud_netgraph_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of everything about net</p>
</div>

  <h3 id="hud_netgraph_scale">hud_netgraph_scale&nbsp;<span>[<a href="#hud_netgraph_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>256</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of everything about net</p>
</div>

  <h3 id="hud_netgraph_show">hud_netgraph_show&nbsp;<span>[<a href="#hud_netgraph_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of everything about net</p>
</div>

  <h3 id="hud_netgraph_swap_x">hud_netgraph_swap_x&nbsp;<span>[<a href="#hud_netgraph_swap_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
reverse horizontally, like for placing at left edge of the screen</p>
</div>

  <h3 id="hud_netgraph_swap_y">hud_netgraph_swap_y&nbsp;<span>[<a href="#hud_netgraph_swap_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
reverse vertically, like for top edge</p>
</div>

  <h3 id="hud_netgraph_width">hud_netgraph_width&nbsp;<span>[<a href="#hud_netgraph_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>256</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal size of everything about net</p>
</div>

  <h3 id="hud_pent_align_x">hud_pent_align_x&nbsp;<span>[<a href="#hud_pent_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of pentagram icon</p>
</div>

  <h3 id="hud_pent_align_y">hud_pent_align_y&nbsp;<span>[<a href="#hud_pent_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of pentagram icon</p>
</div>

  <h3 id="hud_pent_frame">hud_pent_frame&nbsp;<span>[<a href="#hud_pent_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for pentagram icon</p>
</div>

  <h3 id="hud_pent_place">hud_pent_place&nbsp;<span>[<a href="#hud_pent_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>ring</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for pentagram icon</p>
</div>

  <h3 id="hud_pent_pos_x">hud_pent_pos_x&nbsp;<span>[<a href="#hud_pent_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of pentagram icon</p>
</div>

  <h3 id="hud_pent_pos_y">hud_pent_pos_y&nbsp;<span>[<a href="#hud_pent_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of pentagram icon</p>
</div>

  <h3 id="hud_pent_scale">hud_pent_scale&nbsp;<span>[<a href="#hud_pent_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of pentagram icon</p>
</div>

  <h3 id="hud_pent_show">hud_pent_show&nbsp;<span>[<a href="#hud_pent_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of pentagram icon</p>
</div>

  <h3 id="hud_pent_style">hud_pent_style&nbsp;<span>[<a href="#hud_pent_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of pentagram icon</p>
</div>

  <h3 id="hud_ping_align_x">hud_ping_align_x&nbsp;<span>[<a href="#hud_ping_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of small net statistics</p>
</div>

  <h3 id="hud_ping_align_y">hud_ping_align_y&nbsp;<span>[<a href="#hud_ping_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>bottom</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of small net statistics</p>
</div>

  <h3 id="hud_ping_blink">hud_ping_blink&nbsp;<span>[<a href="#hud_ping_blink">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Enable yellow blinking dot, which shows when your ping is recalculated</p>
</div>

  <h3 id="hud_ping_frame">hud_ping_frame&nbsp;<span>[<a href="#hud_ping_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for small net statistics</p>
</div>

  <h3 id="hud_ping_period">hud_ping_period&nbsp;<span>[<a href="#hud_ping_period">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
period of time between updates (minimum value is your frame time)</p>
</div>

  <h3 id="hud_ping_place">hud_ping_place&nbsp;<span>[<a href="#hud_ping_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for small net statistics</p>
</div>

  <h3 id="hud_ping_pos_x">hud_ping_pos_x&nbsp;<span>[<a href="#hud_ping_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of small net statistics</p>
</div>

  <h3 id="hud_ping_pos_y">hud_ping_pos_y&nbsp;<span>[<a href="#hud_ping_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of small net statistics</p>
</div>

  <h3 id="hud_ping_show">hud_ping_show&nbsp;<span>[<a href="#hud_ping_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of small net statistics</p>
</div>

  <h3 id="hud_ping_show_dev">hud_ping_show_dev&nbsp;<span>[<a href="#hud_ping_show_dev">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of small net statistics</p>
</div>

  <h3 id="hud_ping_show_max">hud_ping_show_max&nbsp;<span>[<a href="#hud_ping_show_max">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of small net statistics</p>
</div>

  <h3 id="hud_ping_show_min">hud_ping_show_min&nbsp;<span>[<a href="#hud_ping_show_min">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of small net statistics</p>
</div>

  <h3 id="hud_ping_show_pl">hud_ping_show_pl&nbsp;<span>[<a href="#hud_ping_show_pl">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of small net statistics</p>
</div>

  <h3 id="hud_quad_align_x">hud_quad_align_x&nbsp;<span>[<a href="#hud_quad_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of quad icon</p>
</div>

  <h3 id="hud_quad_align_y">hud_quad_align_y&nbsp;<span>[<a href="#hud_quad_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of quad icon</p>
</div>

  <h3 id="hud_quad_frame">hud_quad_frame&nbsp;<span>[<a href="#hud_quad_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for quad icon</p>
</div>

  <h3 id="hud_quad_place">hud_quad_place&nbsp;<span>[<a href="#hud_quad_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>suit</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for quad icon</p>
</div>

  <h3 id="hud_quad_pos_x">hud_quad_pos_x&nbsp;<span>[<a href="#hud_quad_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of quad icon</p>
</div>

  <h3 id="hud_quad_pos_y">hud_quad_pos_y&nbsp;<span>[<a href="#hud_quad_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of quad icon</p>
</div>

  <h3 id="hud_quad_scale">hud_quad_scale&nbsp;<span>[<a href="#hud_quad_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of quad icon</p>
</div>

  <h3 id="hud_quad_show">hud_quad_show&nbsp;<span>[<a href="#hud_quad_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of quad icon</p>
</div>

  <h3 id="hud_quad_style">hud_quad_style&nbsp;<span>[<a href="#hud_quad_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of quad icon</p>
</div>

  <h3 id="hud_ring_align_x">hud_ring_align_x&nbsp;<span>[<a href="#hud_ring_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of ring icon</p>
</div>

  <h3 id="hud_ring_align_y">hud_ring_align_y&nbsp;<span>[<a href="#hud_ring_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of ring icon</p>
</div>

  <h3 id="hud_ring_frame">hud_ring_frame&nbsp;<span>[<a href="#hud_ring_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for ring icon</p>
</div>

  <h3 id="hud_ring_place">hud_ring_place&nbsp;<span>[<a href="#hud_ring_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>key2</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for ring icon</p>
</div>

  <h3 id="hud_ring_pos_x">hud_ring_pos_x&nbsp;<span>[<a href="#hud_ring_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of ring icon</p>
</div>

  <h3 id="hud_ring_pos_y">hud_ring_pos_y&nbsp;<span>[<a href="#hud_ring_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of ring icon</p>
</div>

  <h3 id="hud_ring_scale">hud_ring_scale&nbsp;<span>[<a href="#hud_ring_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of ring icon</p>
</div>

  <h3 id="hud_ring_show">hud_ring_show&nbsp;<span>[<a href="#hud_ring_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of ring icon</p>
</div>

  <h3 id="hud_ring_style">hud_ring_style&nbsp;<span>[<a href="#hud_ring_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of ring icon</p>
</div>

  <h3 id="hud_sigil1_align_x">hud_sigil1_align_x&nbsp;<span>[<a href="#hud_sigil1_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of sigil 1 icon (rune)</p>
</div>

  <h3 id="hud_sigil1_align_y">hud_sigil1_align_y&nbsp;<span>[<a href="#hud_sigil1_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of sigil 1 icon (rune)</p>
</div>

  <h3 id="hud_sigil1_frame">hud_sigil1_frame&nbsp;<span>[<a href="#hud_sigil1_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for sigil 1 icon (rune)</p>
</div>

  <h3 id="hud_sigil1_place">hud_sigil1_place&nbsp;<span>[<a href="#hud_sigil1_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for sigil 1 icon (rune)</p>
</div>

  <h3 id="hud_sigil1_pos_x">hud_sigil1_pos_x&nbsp;<span>[<a href="#hud_sigil1_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of sigil 1 icon (rune)</p>
</div>

  <h3 id="hud_sigil1_pos_y">hud_sigil1_pos_y&nbsp;<span>[<a href="#hud_sigil1_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of sigil 1 icon (rune)</p>
</div>

  <h3 id="hud_sigil1_scale">hud_sigil1_scale&nbsp;<span>[<a href="#hud_sigil1_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of sigil 1 icon (rune)</p>
</div>

  <h3 id="hud_sigil1_show">hud_sigil1_show&nbsp;<span>[<a href="#hud_sigil1_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of sigil 1 icon (rune)</p>
</div>

  <h3 id="hud_sigil1_style">hud_sigil1_style&nbsp;<span>[<a href="#hud_sigil1_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of sigil 1 icon (rune)</p>
</div>

  <h3 id="hud_sigil2_align_x">hud_sigil2_align_x&nbsp;<span>[<a href="#hud_sigil2_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of sigil 2 icon</p>
</div>

  <h3 id="hud_sigil2_align_y">hud_sigil2_align_y&nbsp;<span>[<a href="#hud_sigil2_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of sigil 2 icon</p>
</div>

  <h3 id="hud_sigil2_frame">hud_sigil2_frame&nbsp;<span>[<a href="#hud_sigil2_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for sigil 2 icon</p>
</div>

  <h3 id="hud_sigil2_place">hud_sigil2_place&nbsp;<span>[<a href="#hud_sigil2_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>sigil1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for sigil 2 icon</p>
</div>

  <h3 id="hud_sigil2_pos_x">hud_sigil2_pos_x&nbsp;<span>[<a href="#hud_sigil2_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of sigil 2 icon</p>
</div>

  <h3 id="hud_sigil2_pos_y">hud_sigil2_pos_y&nbsp;<span>[<a href="#hud_sigil2_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of sigil 2 icon</p>
</div>

  <h3 id="hud_sigil2_scale">hud_sigil2_scale&nbsp;<span>[<a href="#hud_sigil2_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of sigil 2 icon</p>
</div>

  <h3 id="hud_sigil2_show">hud_sigil2_show&nbsp;<span>[<a href="#hud_sigil2_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of sigil 2 icon</p>
</div>

  <h3 id="hud_sigil2_style">hud_sigil2_style&nbsp;<span>[<a href="#hud_sigil2_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of sigil 2 icon</p>
</div>

  <h3 id="hud_sigil3_align_x">hud_sigil3_align_x&nbsp;<span>[<a href="#hud_sigil3_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of sigil 3 icon</p>
</div>

  <h3 id="hud_sigil3_align_y">hud_sigil3_align_y&nbsp;<span>[<a href="#hud_sigil3_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of sigil 3 icon</p>
</div>

  <h3 id="hud_sigil3_frame">hud_sigil3_frame&nbsp;<span>[<a href="#hud_sigil3_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for sigil 3 icon</p>
</div>

  <h3 id="hud_sigil3_place">hud_sigil3_place&nbsp;<span>[<a href="#hud_sigil3_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>sigil2</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for sigil 3 icon</p>
</div>

  <h3 id="hud_sigil3_pos_x">hud_sigil3_pos_x&nbsp;<span>[<a href="#hud_sigil3_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of sigil 3 icon</p>
</div>

  <h3 id="hud_sigil3_pos_y">hud_sigil3_pos_y&nbsp;<span>[<a href="#hud_sigil3_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of sigil 3 icon</p>
</div>

  <h3 id="hud_sigil3_scale">hud_sigil3_scale&nbsp;<span>[<a href="#hud_sigil3_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of sigil 3 icon</p>
</div>

  <h3 id="hud_sigil3_show">hud_sigil3_show&nbsp;<span>[<a href="#hud_sigil3_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of sigil 3 icon</p>
</div>

  <h3 id="hud_sigil3_style">hud_sigil3_style&nbsp;<span>[<a href="#hud_sigil3_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of sigil 3 icon</p>
</div>

  <h3 id="hud_sigil4_align_x">hud_sigil4_align_x&nbsp;<span>[<a href="#hud_sigil4_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of sigil 4 icon</p>
</div>

  <h3 id="hud_sigil4_align_y">hud_sigil4_align_y&nbsp;<span>[<a href="#hud_sigil4_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of sigil 4 icon</p>
</div>

  <h3 id="hud_sigil4_frame">hud_sigil4_frame&nbsp;<span>[<a href="#hud_sigil4_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for sigil 4 icon</p>
</div>

  <h3 id="hud_sigil4_place">hud_sigil4_place&nbsp;<span>[<a href="#hud_sigil4_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>sigil3</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for sigil 4 icon</p>
</div>

  <h3 id="hud_sigil4_pos_x">hud_sigil4_pos_x&nbsp;<span>[<a href="#hud_sigil4_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of sigil 4 icon</p>
</div>

  <h3 id="hud_sigil4_pos_y">hud_sigil4_pos_y&nbsp;<span>[<a href="#hud_sigil4_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of sigil 4 icon</p>
</div>

  <h3 id="hud_sigil4_scale">hud_sigil4_scale&nbsp;<span>[<a href="#hud_sigil4_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of sigil 4 icon</p>
</div>

  <h3 id="hud_sigil4_show">hud_sigil4_show&nbsp;<span>[<a href="#hud_sigil4_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of sigil 4 icon</p>
</div>

  <h3 id="hud_sigil4_style">hud_sigil4_style&nbsp;<span>[<a href="#hud_sigil4_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of sigil 4 icon</p>
</div>

  <h3 id="hud_speed_align_x">hud_speed_align_x&nbsp;<span>[<a href="#hud_speed_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>center</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of your current speed</p>
</div>

  <h3 id="hud_speed_align_y">hud_speed_align_y&nbsp;<span>[<a href="#hud_speed_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>bottom</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of your current speed</p>
</div>

  <h3 id="hud_speed_frame">hud_speed_frame&nbsp;<span>[<a href="#hud_speed_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for your current speed</p>
</div>

  <h3 id="hud_speed_place">hud_speed_place&nbsp;<span>[<a href="#hud_speed_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for your current speed</p>
</div>

  <h3 id="hud_speed_pos_x">hud_speed_pos_x&nbsp;<span>[<a href="#hud_speed_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of your current speed</p>
</div>

  <h3 id="hud_speed_pos_y">hud_speed_pos_y&nbsp;<span>[<a href="#hud_speed_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>-5</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of your current speed</p>
</div>

  <h3 id="hud_speed_show">hud_speed_show&nbsp;<span>[<a href="#hud_speed_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of your current speed</p>
</div>

  <h3 id="hud_speed_xyz">hud_speed_xyz&nbsp;<span>[<a href="#hud_speed_xyz">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets This toggles whether the speed is measured over the XY axis (xyz 0) or the XYZ axis (xyz 1)</p>
</div>

  <h3 id="hud_suit_align_x">hud_suit_align_x&nbsp;<span>[<a href="#hud_suit_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal align of suit icon</p>
</div>

  <h3 id="hud_suit_align_y">hud_suit_align_y&nbsp;<span>[<a href="#hud_suit_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>after</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical align of suit icon</p>
</div>

  <h3 id="hud_suit_frame">hud_suit_frame&nbsp;<span>[<a href="#hud_suit_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets frame visibility and style for suit icon</p>
</div>

  <h3 id="hud_suit_place">hud_suit_place&nbsp;<span>[<a href="#hud_suit_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>pent</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets relative positioning for suit icon</p>
</div>

  <h3 id="hud_suit_pos_x">hud_suit_pos_x&nbsp;<span>[<a href="#hud_suit_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets horizontal position of suit icon</p>
</div>

  <h3 id="hud_suit_pos_y">hud_suit_pos_y&nbsp;<span>[<a href="#hud_suit_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets vertical position of suit icon</p>
</div>

  <h3 id="hud_suit_scale">hud_suit_scale&nbsp;<span>[<a href="#hud_suit_scale">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets size of suit icon</p>
</div>

  <h3 id="hud_suit_show">hud_suit_show&nbsp;<span>[<a href="#hud_suit_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches showing of suit icon</p>
</div>

  <h3 id="hud_suit_style">hud_suit_style&nbsp;<span>[<a href="#hud_suit_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Switches graphical style of suit icon</p>
</div>

  <h3 id="hud_teamfrags_align_x">hud_teamfrags_align_x&nbsp;<span>[<a href="#hud_teamfrags_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>Sets teamfrags HUD element horizontal alignment.</p>
  <h4>Value</h4>
    <p>String. left, center, right, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_teamfrags_align_y">hud_teamfrags_align_y&nbsp;<span>[<a href="#hud_teamfrags_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>Sets teamfrags HUD element vertical alignment.</p>
  <h4>Value</h4>
    <p>String. top, center, bottom, console, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_teamfrags_cell_height">hud_teamfrags_cell_height&nbsp;<span>[<a href="#hud_teamfrags_cell_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>8</span></p>
<div>
  <h4>Description</h4>
  <p>Sets cell height for cells in 'teamfrags' HUD element.</p>
  <h4>Value</h4>
    <p>Integer. Use positive values.</p>
  </div>

  <h3 id="hud_teamfrags_cell_width">hud_teamfrags_cell_width&nbsp;<span>[<a href="#hud_teamfrags_cell_width">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>32</span></p>
<div>
  <h4>Description</h4>
  <p>Sets cell width for cells in 'teamfrags' HUD element.</p>
  <h4>Value</h4>
    <p>Integer. Use positive values.</p>
  </div>

  <h3 id="hud_teamfrags_cols">hud_teamfrags_cols&nbsp;<span>[<a href="#hud_teamfrags_cols">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>Sets number of columns used to draw table in 'teamfrags' HUD element.</p>
  <h4>Value</h4>
    <p>Integer. Use positive values.</p>
  </div>

  <h3 id="hud_teamfrags_fliptext">hud_teamfrags_fliptext&nbsp;<span>[<a href="#hud_teamfrags_fliptext">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles alignment of team names in 'teamfrags' HUD element.</p>
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
        <td>Rows aligned to the left side, team tags are on the right side of frag counts.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Rows aligned to the right side, team tags are on the left side of frag counts.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>Use 'teamfrags shownames 1' to show names of teams.</p>
</div>

  <h3 id="hud_teamfrags_frame">hud_teamfrags_frame&nbsp;<span>[<a href="#hud_teamfrags_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets frame visibility and style for this HUD element.</p>
  <h4>Value</h4>
    <p>Float. Use floating point values from range 0..1 to get alpha transparent background or 2 to get bevel frame.</p>
  </div>

  <h3 id="hud_teamfrags_padtext">hud_teamfrags_padtext&nbsp;<span>[<a href="#hud_teamfrags_padtext">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles text padding in 'frags' HUD element.</p>
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
        <td>No spaces between text fields.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Adds padding so frags and team tags are aligned into columns.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>Use 'teamfrags shownames 1'.</p>
</div>

  <h3 id="hud_teamfrags_place">hud_teamfrags_place&nbsp;<span>[<a href="#hud_teamfrags_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>screen</span></p>
<div>
  <h4>Description</h4>
  <p>Sets placement for this HUD element. You can align some elements relative to other elements.</p>
  <h4>Value</h4>
    <p>String. to place this element inside element B set this variable to "@B", to place it outside B element set this variable to "B"</p>
  <h4>Remarks</h4>
  <p>First you have to decide, if the element that you are locating now (element B) is to be positioned inside another element (element A) or outside it. See HUD manual for more info.</p>
</div>

  <h3 id="hud_teamfrags_pos_x">hud_teamfrags_pos_x&nbsp;<span>[<a href="#hud_teamfrags_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets horizontal position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_teamfrags_pos_y">hud_teamfrags_pos_y&nbsp;<span>[<a href="#hud_teamfrags_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets vertical position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_teamfrags_rows">hud_teamfrags_rows&nbsp;<span>[<a href="#hud_teamfrags_rows">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Sets number of rows used to draw table in 'teamfrags' HUD element.</p>
  <h4>Value</h4>
    <p>Integer. Use positive values.</p>
  </div>

  <h3 id="hud_teamfrags_show">hud_teamfrags_show&nbsp;<span>[<a href="#hud_teamfrags_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles visibility of this HUD element.</p>
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
        <td>Do not draw this element</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw this element</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_teamfrags_shownames">hud_teamfrags_shownames&nbsp;<span>[<a href="#hud_teamfrags_shownames">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Draws players' team tags next to frag counts in 'teamfrags' HUD element.</p>
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
        <td>Do not display team tags.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Display team tags.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_teamfrags_space_x">hud_teamfrags_space_x&nbsp;<span>[<a href="#hud_teamfrags_space_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Sets horizontal spacing for teamfrags fields.</p>
  <h4>Value</h4>
    <p>Integer. Use zero or positive values.</p>
  </div>

  <h3 id="hud_teamfrags_space_y">hud_teamfrags_space_y&nbsp;<span>[<a href="#hud_teamfrags_space_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Sets vertical spacing for teamfrags fields.</p>
  <h4>Value</h4>
    <p>Integer. Use zero or positive values.</p>
  </div>

  <h3 id="hud_teamfrags_strip">hud_teamfrags_strip&nbsp;<span>[<a href="#hud_teamfrags_strip">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles stripped formatting of teamfrags fields.</p>
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
        <td>Standard display.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Stripped format.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_teamfrags_style">hud_teamfrags_style&nbsp;<span>[<a href="#hud_teamfrags_style">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets drawing style of 'teamfrags' HUD element.</p>
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
        <td>Golden brackets around field with your team frags.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Arrow pointing to your own team frags field.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Red rectangle around your own team frags field.</td>
      </tr>
      <tr>
        <td>3</td>
        <td>Similar to 1.</td>
      </tr>
      <tr>
        <td>4</td>
        <td>Sets background color for your own team field to 'teamcolor'.</td>
      </tr>
      <tr>
        <td>5</td>
        <td>Sets background color for fields to teamcolors and enemycolors. Red rectangle around your own field including name and team tag.</td>
      </tr>
      <tr>
        <td>6</td>
        <td>Red rectangle around your own team field including team tag. Background color only for your own team field and set to 'teamcolor'.</td>
      </tr>
      <tr>
        <td>7</td>
        <td>Background color for whole table 50% transparent red, your own team field not transparent.</td>
      </tr>
      <tr>
        <td>8</td>
        <td>Red background color only for your own team field including team tag.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_teamfrags_vertical">hud_teamfrags_vertical&nbsp;<span>[<a href="#hud_teamfrags_vertical">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles vertical ordering of teamfrags fields.</p>
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
        <td>Order cells horizontally first.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Order cells vertically first.</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="hud_tracking_align_x">hud_tracking_align_x&nbsp;<span>[<a href="#hud_tracking_align_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>left</span></p>
<div>
  <h4>Description</h4>
  <p>Sets tracking HUD element horizontal alignment.</p>
  <h4>Value</h4>
    <p>String. left, center, right, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_tracking_align_y">hud_tracking_align_y&nbsp;<span>[<a href="#hud_tracking_align_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>bottom</span></p>
<div>
  <h4>Description</h4>
  <p>Sets tracking HUD element vertical alignment.</p>
  <h4>Value</h4>
    <p>String. top, center, bottom, console, before, after</p>
  <h4>Remarks</h4>
  <p>See HUD manual for more info.</p>
</div>

  <h3 id="hud_tracking_format">hud_tracking_format&nbsp;<span>[<a href="#hud_tracking_format">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>Tracking %t %n, [JUMP] for next</span></p>
<div>
  <h4>Description</h4>
  <p>Changes the format of descriptive text displayed when you are tracking someone as spectator or watching a demo where you can see player's team and name.</p>
  <h4>Value</h4>
    <p>String. Pattern %n will be replaced with tracked player's name, %t will be replaced with player's team.</p>
  </div>

  <h3 id="hud_tracking_frame">hud_tracking_frame&nbsp;<span>[<a href="#hud_tracking_frame">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets frame visibility and style for this HUD element.</p>
  <h4>Value</h4>
    <p>Float. Use floating point values from range 0..1 to get alpha transparent background or 2 to get bevel frame.</p>
  </div>

  <h3 id="hud_tracking_place">hud_tracking_place&nbsp;<span>[<a href="#hud_tracking_place">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>top</span></p>
<div>
  <h4>Description</h4>
  <p>Sets placement for this HUD element. You can align some elements relative to other elements.</p>
  <h4>Value</h4>
    <p>String. to place this element inside element B set this variable to "@B", to place it outside B element set this variable to "B"</p>
  <h4>Remarks</h4>
  <p>First you have to decide, if the element that you are locating now (element B) is to be positioned inside another element (element A) or outside it. See HUD manual for more info.</p>
</div>

  <h3 id="hud_tracking_pos_x">hud_tracking_pos_x&nbsp;<span>[<a href="#hud_tracking_pos_x">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets horizontal position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_tracking_pos_y">hud_tracking_pos_y&nbsp;<span>[<a href="#hud_tracking_pos_y">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Sets vertical position of this HUD element.</p>
  <h4>Value</h4>
  </div>

  <h3 id="hud_tracking_show">hud_tracking_show&nbsp;<span>[<a href="#hud_tracking_show">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles visibility of this HUD element.</p>
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
        <td>Do not draw this element</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw this element</td>
      </tr>
    </tbody>
  </table>
  </div>

<h2 id="status-bar-and-scoreboard">Status Bar and Scoreboard</h2>
  <h3 id="cl_hudswap">cl_hudswap&nbsp;<span>[<a href="#cl_hudswap">#</a>]</span></h3>
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
        <td>The inventory is drawn on the right side of the screen.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>The inventory is drawn on the left side of the screen.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="cl_sbar">cl_sbar&nbsp;<span>[<a href="#cl_sbar">#</a>]</span></h3>
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
        <td>Use the new transparent HUD.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Use the old status bar.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="r_damagestats">r_damagestats&nbsp;<span>[<a href="#r_damagestats">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Displays the amount of damage taken recently on the screen above your armour and health.
</p>
</div>

  <h3 id="scr_centersbar">scr_centerSbar&nbsp;<span>[<a href="#scr_centersbar">#</a>]</span></h3>
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
        <td>Off.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>The hud display will be centered.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="scr_compacthud">scr_compactHud&nbsp;<span>[<a href="#scr_compacthud">#</a>]</span></h3>
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
        <td>Compact huds off.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Will display armour/health/ammo/weapons *very* compactly.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Will display armour/health/ammo *very* compactly.</td>
      </tr>
      <tr>
        <td>3</td>
        <td>Displays only health/armour.</td>
      </tr>
      <tr>
        <td>4</td>
        <td>Is just like "2" except it also displays your current weapon's ammo in big numbers.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>







</p>
</div>

  <h3 id="scr_compacthudalign">scr_compactHudAlign&nbsp;<span>[<a href="#scr_compacthudalign">#</a>]</span></h3>
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
        <td>determine ammo amounts are aligned to the left.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>determine ammo amounts are aligned to the right.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="scr_drawhfrags">scr_drawHFrags&nbsp;<span>[<a href="#scr_drawhfrags">#</a>]</span></h3>
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
        <td>Turns frag display off.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Individual frags.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Teamfrags in teamplay mode.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="scr_drawvfrags">scr_drawVFrags&nbsp;<span>[<a href="#scr_drawvfrags">#</a>]</span></h3>
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
        <td>Turns frag display off.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Individual frags.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Teamfrags in teamplay mode.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



Note: scr_drawVFrags only does something if your console width is &gt;= 512 and &lt; 640.


</p>
</div>

  <h3 id="scr_newhud">scr_newhud&nbsp;<span>[<a href="#scr_newhud">#</a>]</span></h3>
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
        <td>Old standard status bar</td>
      </tr>
      <tr>
        <td>1</td>
        <td>New customizable status bar</td>
      </tr>
      <tr>
        <td>2</td>
        <td>First draws old status bar and then renders new status bar on the top of it.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="scr_sbar_drawammo">scr_sbar_drawammo&nbsp;<span>[<a href="#scr_sbar_drawammo">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing amount of ammo on/off.</p>
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
        <td>Do not draw ammo.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw ammo.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_sbar_drawammocounts">scr_sbar_drawammocounts&nbsp;<span>[<a href="#scr_sbar_drawammocounts">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing amounts of ammo on/off.</p>
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
        <td>Do not draw countrs of ammo.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw countrs of ammo.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_sbar_drawammoicon">scr_sbar_drawammoicon&nbsp;<span>[<a href="#scr_sbar_drawammoicon">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing of ammo icon on/off.</p>
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
        <td>Do not draw ammo icon.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw ammo icon.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_sbar_drawarmor">scr_sbar_drawarmor&nbsp;<span>[<a href="#scr_sbar_drawarmor">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing of armor count on/off.</p>
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
        <td>Do not draw armor count.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw armor count.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_sbar_drawarmoricon">scr_sbar_drawarmoricon&nbsp;<span>[<a href="#scr_sbar_drawarmoricon">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing of armor icon on/off.</p>
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
        <td>Do not draw armor icon.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw armor icon.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_sbar_drawfaceicon">scr_sbar_drawfaceicon&nbsp;<span>[<a href="#scr_sbar_drawfaceicon">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing of face icon on/off.</p>
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
        <td>Do not draw face icon.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw face icon.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_sbar_drawguns">scr_sbar_drawguns&nbsp;<span>[<a href="#scr_sbar_drawguns">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing of available guns on/off.</p>
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
        <td>Do not draw available guns.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw avialable guns.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_sbar_drawhealth">scr_sbar_drawhealth&nbsp;<span>[<a href="#scr_sbar_drawhealth">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing of health amount on/off.</p>
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
        <td>Do not draw health amount.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw health amount.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_sbar_drawitems">scr_sbar_drawitems&nbsp;<span>[<a href="#scr_sbar_drawitems">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing of items (powerups and keys) in status bar on/off.</p>
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
        <td>Do not draw items in status bar.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw items in status bar.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_sbar_drawsigils">scr_sbar_drawsigils&nbsp;<span>[<a href="#scr_sbar_drawsigils">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Turns drawing of sigils on/off.</p>
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
        <td>Do not draw sigils.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Draw sigils.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>This variable applies for old HUD &lt;= 'scr_newhud 0'.</p>
</div>

  <h3 id="scr_scoreboard_borderless">scr_scoreboard_borderless&nbsp;<span>[<a href="#scr_scoreboard_borderless">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Controls scoreboard border behavior</p>
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
        <td>no scoreboard border will be displayed</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>scoreboard border will be displayed</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="scr_scoreboard_centered">scr_scoreboard_centered&nbsp;<span>[<a href="#scr_scoreboard_centered">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>Controls X-position of scoreboard</p>
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
        <td>Scoreboard will not be centered</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Scoreboard will be centered</td>
      </tr>
    </tbody>
  </table>
  </div>

  <h3 id="scr_scoreboard_drawtitle">scr_scoreboard_drawtitle&nbsp;<span>[<a href="#scr_scoreboard_drawtitle">#</a>]</span></h3>
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
        <td>Disable scoreboard title.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable scoreboard title.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="scr_scoreboard_fadescreen">scr_scoreboard_fadescreen&nbsp;<span>[<a href="#scr_scoreboard_fadescreen">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>shadowing level, when scoreboard is displayed.</p>
  <h4>Value</h4>
    <p>Float. [0...1] 0=transparent scoreboard 1=black scoreboard</p>
  </div>

  <h3 id="scr_scoreboard_fillalpha">scr_scoreboard_fillalpha&nbsp;<span>[<a href="#scr_scoreboard_fillalpha">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0.7</span></p>
<div>
  <h4>Description</h4>
  <p>
Change scoreboard fillalpha.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="scr_scoreboard_fillcolored">scr_scoreboard_fillcolored&nbsp;<span>[<a href="#scr_scoreboard_fillcolored">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>

Modify scoreboard fillcolour.

</p>
</div>

  <h3 id="scr_scoreboard_forcecolors">scr_scoreboard_forcecolors&nbsp;<span>[<a href="#scr_scoreboard_forcecolors">#</a>]</span></h3>
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
        <td>When you overwrite team/enemy color it will use only those colors for skins</td>
      </tr>
      <tr>
        <td>1</td>
        <td>When you overwrite team/enemy color it will use only those colors for skins and scoreboard</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="scr_scoreboard_posy">scr_scoreboard_posy&nbsp;<span>[<a href="#scr_scoreboard_posy">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Controls Y-offset of scoreboard.</p>
  <h4>Value</h4>
    <p>Integer. if 0, scoreboard will be shown at top of screen, else will be shifted down by &lt;your.value&gt;</p>
  </div>

  <h3 id="scr_scoreboard_showfrags">scr_scoreboard_showfrags&nbsp;<span>[<a href="#scr_scoreboard_showfrags">#</a>]</span></h3>
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
        <td>Disable frags on the scoreboard.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable frags on the scoreboard.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="scr_scoreboard_teamsort">scr_scoreboard_teamsort&nbsp;<span>[<a href="#scr_scoreboard_teamsort">#</a>]</span></h3>
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
        <td>Frag sort.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Team sort.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

