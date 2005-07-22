  <ul>
    <li><a href="#input---joystick">Input - Joystick</a></li>
    <li><a href="#input---keyboard">Input - Keyboard</a></li>
    <li><a href="#input---misc">Input - Misc</a></li>
    <li><a href="#input---mouse">Input - Mouse</a></li>
  </ul>
<h2 id="input---joystick">Input - Joystick</h2>
  <h3 id="joystick">joystick&nbsp;<span>[<a href="#joystick">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Toggles usage of joystick device.</p>
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
        <td>Do not use joystick.</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>Use joystick input.</td>
      </tr>
    </tbody>
  </table>
  </div>

<h2 id="input---keyboard">Input - Keyboard</h2>
  <h3 id="allow_scripts">allow_scripts&nbsp;<span>[<a href="#allow_scripts">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>


Possibility to prove, that you do NOT use scripts. Of course, values 0 and 1 will block 'rotate' command.
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
        <td>Same as 1, but also block fast +lookdown/+lookup,
                so even simple rjump will be impossible. Blocks KTPro /kfjump and /krjump aliases.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Allow only simple scripts - it will block left/right
                turning, so fjump (for example) will be impossible,
                but normal rjump (+lookdown;+attack;+jump) will be
                possible. Blocks KTPro alias /kfjump too.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Allow scripts (default)</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>


Your current 'allow_scripts' setting will be reported to
        'f_scripts' query and anyone will be able to check it. If
        anyone said 'f_scripts' since last mapchange, the client will also
        autoreport any change of allow_scripts (just like reporting
        f_skins). So if you don't use scripts, turn them off to prove
        you don't use them. And remember - scripts _are allowed_,
        so don't call ppl who use them 'cheaters'.

You can't set this variable to 2 when a command line parameter -noscripts is used.

</p>
</div>

  <h3 id="cl_anglespeedkey">cl_anglespeedkey&nbsp;<span>[<a href="#cl_anglespeedkey">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1.5</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable sets multiplier by which your "cl_yawspeed" (how fast you turn) is
multiplied when running (+speed).

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="cl_backspeed">cl_backspeed&nbsp;<span>[<a href="#cl_backspeed">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>400</span></p>
<div>
  <h4>Description</h4>
  <p>

This allows you to set your backward speed. Obviously this is also limited by
the server, usually to "320".

</p>
</div>

  <h3 id="cl_forwardspeed">cl_forwardspeed&nbsp;<span>[<a href="#cl_forwardspeed">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>400</span></p>
<div>
  <h4>Description</h4>
  <p>

This allows you to set your forward speed. Obviously this is also limited by the
server, usually to "320".

</p>
</div>

  <h3 id="cl_keypad">cl_keypad&nbsp;<span>[<a href="#cl_keypad">#</a>]</span></h3>
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
        <td>QW compatibility where the keypad keys act the same key name, eg KP_8 acts like uparrow would.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>You will be able to bind such keys as capslock, numlock, and all numpad keys.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="cl_movespeedkey">cl_movespeedkey&nbsp;<span>[<a href="#cl_movespeedkey">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2.0</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable is the multiplier for how fast you move when running (+speed) in
relation to when walking (-speed).

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="cl_pitchspeed">cl_pitchspeed&nbsp;<span>[<a href="#cl_pitchspeed">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>150</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable determines how fast you you turn up/down when using "+lookup" and
"+lookdown".

</p>
</div>

  <h3 id="cl_showkeycodes">cl_showkeycodes&nbsp;<span>[<a href="#cl_showkeycodes">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>


This variable enables/disables the output of informations to the currently pressed and/or released keys.
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
        <td>Switch off output of scancode/keymapping information (Default)</td>
      </tr>
      <tr>
        <td>1</td>
        <td>information about keys will be printed when the keys are pressed.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>information about keys will be printed when releasing the keys.</td>
      </tr>
      <tr>
        <td>3</td>
        <td>information about keys will be printed when the keys are pressed and also when they are released.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="cl_sidespeed">cl_sidespeed&nbsp;<span>[<a href="#cl_sidespeed">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>350</span></p>
<div>
  <h4>Description</h4>
  <p>

This allows you to set your strafe speed. Obviously this is also limited by the
server, usually to "320".

</p>
</div>

  <h3 id="cl_smartjump">cl_smartjump&nbsp;<span>[<a href="#cl_smartjump">#</a>]</span></h3>
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
        <td>Will convert +jump commands to +moveup commands when in liquid (water, lava, slime).</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="cl_upspeed">cl_upspeed&nbsp;<span>[<a href="#cl_upspeed">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>200</span></p>
<div>
  <h4>Description</h4>
  <p>
This allows you to set the speed with which you move up and down in liquids or in spectator mode.
Obviously this is also limited by the server, usually to 320*0.7 = 224 when being in liquid and to 500 in spectator mode.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="cl_yawspeed">cl_yawspeed&nbsp;<span>[<a href="#cl_yawspeed">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>140</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable defines how quickly you turn left (+left) or right (+right).

</p>
</div>

<h2 id="input---misc">Input - Misc</h2>
  <h3 id="freelook">freelook&nbsp;<span>[<a href="#freelook">#</a>]</span></h3>
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
        <td>Both -mlook and freelook 0 must be specified to disable free look mode.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enables free look mode, that is, moving the mouse forward and back moves your view </td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>

direction up or down.


</p>
</div>

  <h3 id="lookspring">lookspring&nbsp;<span>[<a href="#lookspring">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable toggles the centering of the screen after the -klook command.
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
        <td>Disable automatic force_centerview.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable automatic force_centerview.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="lookstrafe">lookstrafe&nbsp;<span>[<a href="#lookstrafe">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable toggles the automatic strafing when the +klook command is used.
When set to "1" and the player used the +klook command, the keys that are bound 
to the +left and +right commands will now act as if they were bound to +moveleft 
and +moveright. This command was put it in order to allow keyboard player to 
combine the +strafe and +klook commands into one.
This command also has effect on the mouse controls. When set to "1" moving the 
mouse left and right will make the player move left and right instead of making 
him turn left and right.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="sensitivity">sensitivity&nbsp;<span>[<a href="#sensitivity">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>5</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable sets the sensitivity of the mouse, it is one of the most important 
variables in the whole game.

</p>
</div>

<h2 id="input---mouse">Input - Mouse</h2>
  <h3 id="m_accel">m_accel&nbsp;<span>[<a href="#m_accel">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

Sets mouse acceleration like in q3.

</p>
</div>

  <h3 id="m_filter">m_filter&nbsp;<span>[<a href="#m_filter">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable toggles mouse input filtering. When set to "1", the values which
are received from the mouse's input will first be averaged together and then 
that value will be used in the game. 
The reason for this command is that some mice had problems with sending sporadic
coordinates which make the input from the mouse jerky, also when using a serial 
or PS/2 mouse, the Windows operating system will only sample mouse input every 
25ms, that is 40 times a second (for USB mice the sample rate is 125 Hz, that is
every 8 ms). When set to "1" this variable will smooth out the input but it will 
cause latency between the movement of the mouse and the actual response in the 
game. When using a PS/2 mouse it is thus first recommended to try to increase 
the sampling rate either by changing it via your mouse driver or by using the 
ps2rate program which can be downloaded at ps2rate homepage . 
If you are playing the game at frame rates above 40 FPS and if you can't 
increase the sampling rate of your PS/2 rate or if you are playing with a serial 
mouse it is recommended that you enable this toggle.

</p>
</div>

  <h3 id="m_forcewheel">m_forcewheel&nbsp;<span>[<a href="#m_forcewheel">#</a>]</span></h3>
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
        <td>Disable.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>If you have problems to get MWHEELUP and MWHEELDOWN working set this to 1.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="m_forward">m_forward&nbsp;<span>[<a href="#m_forward">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable controls how fast the player should move forward and back when the 
mouse is moved forward and back. This command has no effect if the +mlook 
command is in effect because when the mouse is moved forward and back the 
player looks up and down instead of moving forward and back. Some players might 
want to set this variable to "0" if they happen not to use +mlook constantly and 
they only want to use the mouse to turn the player. Setting this variable to "0"
will prevent the inadvertent movement of the player forward and back while 
trying to make precise turns with the mouse.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="m_pitch">m_pitch&nbsp;<span>[<a href="#m_pitch">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0.022</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable sets the level of precision when the mouse is used to make the 
player look up and down while the +mlook command is in effect. By default this 
variable is set in such a way that moving the mouse forward makes the player 
look up and moving the mouse backward makes the player look down. Some people 
prefer to have this movement inverted just like it is inverted for airplane 
controls. If you wish to use this inverted mouse movement then you should set 
this variable to a negative value (for example "-0.022"). It is a matter of 
preference which movement method is used by players. Also lowering the value 
for this variable will increase the level of precision when the mouse is used 
to make the player look up and down. This variable can be used separately from 
the sensitivity variable to provide greater control over the mouse sensitivity 
for movement along the pitch. It is advisable to keep the value for this 
variable constant at 0.022 or -0.022 and instead use the sensitivity variable 
to change the overall sensitivity of the mouse. Also, some script writers lower 
the value for this variable along with a lowered value for the fov variable in 
order to provide more precision when the fov variable is used to zoom.

</p>
</div>

  <h3 id="m_rate">m_rate&nbsp;<span>[<a href="#m_rate">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>125</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable should be set to your mouse rate (in Hz).
Note: need -m_smooth and -dinput to commandline.

</p>
</div>

  <h3 id="m_showrate">m_showrate&nbsp;<span>[<a href="#m_showrate">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
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
        <td>Disable</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Print current mouse rate</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>

Note: need -m_smooth and -dinput to commandline.


</p>
</div>

  <h3 id="m_side">m_side&nbsp;<span>[<a href="#m_side">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0.8</span></p>
<div>
  <h4>Description</h4>
  <p>
When the +strafe command is active or when "lookspring" is set to "1" this 
variable is used to control the sensitivity when the mouse is moved left and 
right to make the player move left and right.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="m_yaw">m_yaw&nbsp;<span>[<a href="#m_yaw">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0.022</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable controls how fast the player turns left and right when the mouse 
is moved left and right. It is recommended that this variable be left alone and
instead the "sensitivity" variable is used to change the level of precision. 
If you set this variable to a negative value you will reverse the mouse 
movement. Some script writers use this variable to increase the level of 
precision when the "fov" variable is used to zoom the screen.

</p>
</div>

