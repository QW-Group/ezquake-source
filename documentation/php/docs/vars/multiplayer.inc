
<ul>
<li><a href="#network+settings">Network Settings</a></li>
<li><a href="#player+settings">Player Settings</a></li>
<li><a href="#server+browser">Server Browser</a></li>
<li><a href="#skin+settings">Skin Settings</a></li>
<li><a href="#spectator+tracking">Spectator Tracking</a></li>
</ul>

<h2 id="network+settings">Network Settings</h2>

<h3 id="cl_c2simpulsebackup">cl_c2sImpulseBackup</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description">

Used with cl_c2spps, it controls how many backup copies of packets with non-zero 
impulses are to be sent to the server. The recommended value is 3, 
but you can try 2 or even 1 to reduce traffic if you don't have any packet loss.

</p><p class="range">Range: 0/1/2/3</p><p class="remarks"></p>
<h3 id="cl_c2spps">cl_c2spps</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

Packet filtering (a la Qizmo's .c2spps command). Use this to reduce network traffic
if you're playing on a 28800 (or worse) connection and can't set cl_maxfps 72 
because it causes lags.

</p><p class="range">Range: 0..72</p><p class="remarks"></p>
<h3 id="cl_nodelta">cl_nodelta</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

Control the network packet delta compression. When you get blue lines
in your netgraph, you should set 'cl_nodelta 1'. 
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Enable delta compression.</td></tr><tr><td>1</td><td>Disable delta compression.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="cl_nopred">cl_nopred</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
This variable toggles client-side jump prediction. Unless you are having 
problems this variable should be left at &quot;0&quot;.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Enable jump prediction.</td></tr><tr><td>1</td><td>Disable jump prediction.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_oldpl">cl_oldPL</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_predictplayers">cl_predictPlayers</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
This toggles the prediction for other players' movement. Unless you are having 
problems this variable should be left at &quot;1&quot;.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not predict other players' movement.</td></tr><tr><td>1</td><td>Predict other players' movement.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_solidplayers">cl_solidPlayers</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>When you run into other players you will appear to clip in them.</td></tr><tr><td>1</td><td>When you run into other players they will appear to be solid.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_timeout">cl_timeout</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"60"</span></p><p class="description">
This variable defines the timeout value in seconds until the client considers
himself to be disconnected from the server.

</p><p class="remarks">
</p>
<h3 id="cl_useproxy">cl_useproxy</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>You won't drop from qizmo if you are using connect, join and observe commands.</td></tr></tbody></table><p class="remarks">


</p>
<h2 id="player+settings">Player Settings</h2>

<h3 id="b_switch">b_switch</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

This variable allows you to define the highest weapon that FuhQuake should switch 
to upon a backpack pickup. The possible arguments of &quot;b_switch&quot; refer to the 
impulse that is used to switch to a certain weapon:
</p><p class="range">Range: 1/2/3/4/5/6/7/8</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
1</td><td>Axe</td></tr><tr><td>2</td><td>Shotgun</td></tr><tr><td>3</td><td>Double-Barreled Shotgun</td></tr><tr><td>4</td><td>Nailgun</td></tr><tr><td>5</td><td>Super Nailgun</td></tr><tr><td>6</td><td>Grenade Launcher</td></tr><tr><td>7</td><td>Rocket Launcher</td></tr><tr><td>8</td><td>ThunderBolt</td></tr></tbody></table><p class="remarks">








</p>
<h3 id="bottomcolor">bottomcolor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Sets the pants color.

</p><p class="range">Range: 0/1/2/3/4/5/6/7/8/9/10/11/12/13</p><p class="remarks">
</p>
<h3 id="msg">msg</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1/2/3/4</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Maximum reporting of messages.</td></tr><tr><td>1</td><td>Parsed reporting of messages.</td></tr><tr><td>2</td><td>Limited reporting of messages.</td></tr><tr><td>3</td><td>Minimal reporting of messages.</td></tr><tr><td>4</td><td>No reporting of messages.</td></tr></tbody></table><p class="remarks">






</p>
<h3 id="name">name</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"player"</span></p><p class="description">
Player's name.

</p><p class="remarks">
</p>
<h3 id="noaim">noaim</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable toggles whether a server-sided aiming-help should be used when 
shooting rockets (not possible when the server variable &quot;sv_aim&quot; is set to &quot;0&quot;).
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Enable server-side aiming help for rockets (if allowed by the server).</td></tr><tr><td>1</td><td>Disable server-side aiming help for rockets.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="rate">rate</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"5760"</span></p><p class="description">
Sets the maximum amount of bytes per second that the server should send to the client.

</p><p class="remarks">
</p>
<h3 id="skin">skin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

Sets the skin name for the player.

</p><p class="remarks"></p>
<h3 id="spectator">spectator</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Join to server as player.</td></tr><tr><td>1</td><td>Join to server as spectator.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="team">team</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">
Set the team name.

</p><p class="remarks">
</p>
<h3 id="topcolor">topcolor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

Sets the shirt color.

</p><p class="range">Range: 0/1/2/3/4/5/6/7/8/9/10/11/12/13</p><p class="remarks"></p>
<h3 id="w_switch">w_switch</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

This variable allows you to define the highest weapon that FuhQuake should switch 
to when picking it up. The possible arguments of &quot;w_switch&quot; refer to the impulse 
that is used to switch to a certain weapon:
</p><p class="range">Range: 1/2/3/4/5/6/7/8</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
1</td><td>Axe</td></tr><tr><td>2</td><td>Shotgun</td></tr><tr><td>3</td><td>Double-Barreled Shotgun</td></tr><tr><td>4</td><td>Nailgun</td></tr><tr><td>5</td><td>Super Nailgun</td></tr><tr><td>6</td><td>Grenade Launcher</td></tr><tr><td>7</td><td>Rocket Launcher</td></tr><tr><td>8</td><td>ThunderBolt</td></tr></tbody></table><p class="remarks">








</p>
<h2 id="server+browser">Server Browser</h2>

<h3 id="sb_autohide">sb_autohide</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_hidedead">sb_hidedead</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_hideempty">sb_hideempty</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_hidefull">sb_hidefull</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_hidenotempty">sb_hidenotempty</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_inforetries">sb_inforetries</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_infospersec">sb_infospersec</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"100"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_infotimeout">sb_infotimeout</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1000"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_liveupdate">sb_liveupdate</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_mastercache">sb_mastercache</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_masterretries">sb_masterretries</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_mastertimeout">sb_mastertimeout</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1000"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_maxheight">sb_maxheight</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"480"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_maxwidth">sb_maxwidth</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"512"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_pings">sb_pings</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_pingspersec">sb_pingspersec</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"150"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_pingtimeout">sb_pingtimeout</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1000"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_showaddress">sb_showaddress</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_showcounters">sb_showcounters</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_showfraglimit">sb_showfraglimit</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_showgamedir">sb_showgamedir</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_showmap">sb_showmap</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_showping">sb_showping</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_showplayers">sb_showplayers</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_showtimelimit">sb_showtimelimit</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_sortplayers">sb_sortplayers</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"92"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_sortservers">sb_sortservers</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"32"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_sortsources">sb_sortsources</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_sourcevalidity">sb_sourcevalidity</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"30"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="sb_status">sb_status</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h2 id="skin+settings">Skin Settings</h2>

<h3 id="baseskin">baseskin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"base"</span></p><p class="description">

Defines what skin u see other people using if you don't have their skin and don't have skin forcing on.

</p><p class="remarks"></p>
<h3 id="enemybothskin">enemybothskin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

Overrides the enemy quad pent skin so you can define the skin which applies to enemy quads pents.

</p><p class="remarks"></p>
<h3 id="enemypentskin">enemypentskin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

Overrides the enemy pent skin so you can define the skin which applies to enemy pents.

</p><p class="remarks"></p>
<h3 id="enemyquadskin">enemyquadskin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

Overrides the enemy quad skin so you can define the skin which applies to enemy quads.

</p><p class="remarks"></p>
<h3 id="enemyskin">enemyskin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

Overrides the enemies skin so you can define the skin which applies to enemies.

</p><p class="remarks"></p>
<h3 id="noskins">noskins</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Enable skins.</td></tr><tr><td>1</td><td>Disable skins.</td></tr><tr><td>2</td><td>Enable skins but do not download new skins.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_fullbrightskins">r_fullbrightSkins</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Determines the fullbright percentage of skins. Fullbright skins can always be used during demo playback. 
Finally, when the server allows fullbright skins, any user of FuhQuake using fullbright skins will respond 
to an f_skins request. There is no response by a user of FuhQuake if he/she is not using fullbright skins. 
The f_skins response will indicate the brightness level being used as a percentage.
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Being 0% fullbright.</td></tr><tr><td>0.5</td><td>Being 50% fullbright.</td></tr><tr><td>1</td><td>Being 100% fullbright.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="teambothskin">teambothskin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">
Overrides the team quad pent skin so you can define the skin which applies to team quads pents.

</p><p class="remarks">
</p>
<h3 id="teampentskin">teampentskin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

Overrides the team pent skin so you can define the skin which applies to team pents.

</p><p class="remarks"></p>
<h3 id="teamquadskin">teamquadskin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

Overrides the team quad skin so you can define the skin which applies to team quads.

</p><p class="remarks"></p>
<h3 id="teamskin">teamskin</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

Overrides the team skin so you can define the skin which applies to team mates.

</p><p class="remarks"></p>
<h2 id="spectator+tracking">Spectator Tracking</h2>

<h3 id="cl_camera_chase">cl_camera_chase</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">0 = 1st person view
1 = 3rd person view ala Tomb Raider (such a fun!)
2 = 3rd person view ala cl_chasecam 0 (buggy)
Disabled in -ruleset smackdown
</p><p class="range">Range:  0,1,2</p><p class="remarks"></p>
<h3 id="cl_camera_death">cl_camera_death</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Camera view above your body after death.Disabled in -ruleset smackdown
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="cl_chasecam">cl_chasecam</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
This variable toggles the spectator mode to eye view (1st person).

</p><p class="range">Range: 0/1</p><p class="remarks">
</p>