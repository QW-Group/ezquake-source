  <ul>
    <li><a href="#network-settings">Network Settings</a></li>
    <li><a href="#player-settings">Player Settings</a></li>
    <li><a href="#server-browser">Server Browser</a></li>
    <li><a href="#skin-settings">Skin Settings</a></li>
    <li><a href="#spectator-tracking">Spectator Tracking</a></li>
  </ul>
<h2 id="network-settings">Network Settings</h2>
  <h3 id="cl_c2simpulsebackup">cl_c2sImpulseBackup&nbsp;<span>[<a href="#cl_c2simpulsebackup">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
Used with cl_c2spps, it controls how many backup copies of packets with non-zero 
impulses are to be sent to the server. The recommended value is 3, 
but you can try 2 or even 1 to reduce traffic if you don't have any packet loss.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="cl_c2spps">cl_c2spps&nbsp;<span>[<a href="#cl_c2spps">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

Packet filtering (a la Qizmo's .c2spps command). Use this to reduce network traffic
if you're playing on a 28800 (or worse) connection and can't set cl_maxfps 72 
because it causes lags.

</p>
</div>

  <h3 id="cl_crypt_rcon">cl_crypt_rcon&nbsp;<span>[<a href="#cl_crypt_rcon">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>


Encrypts rcon messages sent to server.

</p>
</div>

  <h3 id="cl_nodelta">cl_nodelta&nbsp;<span>[<a href="#cl_nodelta">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

Control the network packet delta compression. When you get blue lines
in your netgraph, you should set 'cl_nodelta 1'. 
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
        <td>Enable delta compression.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Disable delta compression.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="cl_nopred">cl_nopred&nbsp;<span>[<a href="#cl_nopred">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable toggles client-side jump prediction. Unless you are having 
problems this variable should be left at "0".
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
        <td>Enable jump prediction.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Disable jump prediction.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cl_oldpl">cl_oldPL&nbsp;<span>[<a href="#cl_oldpl">#</a>]</span></h3>
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
        <td>Disable the new PL calculation algorithm.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Enable the new PL calculation algorithm.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cl_predictplayers">cl_predictPlayers&nbsp;<span>[<a href="#cl_predictplayers">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles the prediction for other players' movement. Unless you are having 
problems this variable should be left at "1".
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
        <td>Do not predict other players' movement.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Predict other players' movement.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cl_solidplayers">cl_solidPlayers&nbsp;<span>[<a href="#cl_solidplayers">#</a>]</span></h3>
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
        <td>When you run into other players you will appear to clip in them.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>When you run into other players they will appear to be solid.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="cl_timeout">cl_timeout&nbsp;<span>[<a href="#cl_timeout">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>60</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable defines the timeout value in seconds until the client considers
himself to be disconnected from the server.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="cl_useproxy">cl_useproxy&nbsp;<span>[<a href="#cl_useproxy">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether Qizmo should be used (if detected) to a server. When
enabled the server browser will use an existing connection to a Qizmo when 
connecting another server by using the ezQuake Server Browser.
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
        <td>do not use Qizmo to connect a server chosen in the Server Browser</td>
      </tr>
      <tr>
        <td>1</td>
        <td>use Qizmo to connect a server chosen in the Server Browser</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="pushlatency">pushlatency&nbsp;<span>[<a href="#pushlatency">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable adjusts client side prediction by negating the effects of latency.
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
        <td>automatic pushlatency adjusting</td>
      </tr>
      <tr>
        <td>-50</td>
        <td>Default QWCL value.</td>
      </tr>
      <tr>
        <td>-999</td>
        <td>Gives better results.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




Some people suggest to use the negative value of your ping to achieve the best 
results, however in most cases the default value of "-999" should be fine.


</p>
</div>

<h2 id="player-settings">Player Settings</h2>
  <h3 id="b_switch">b_switch&nbsp;<span>[<a href="#b_switch">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

This variable allows you to define the highest weapon that the client should switch 
to upon a backpack pickup. The possible arguments of "b_switch" refer to the 
impulse that is used to switch to a certain weapon:
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
1</td>
        <td>Axe</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Shotgun</td>
      </tr>
      <tr>
        <td>3</td>
        <td>Double-Barreled Shotgun</td>
      </tr>
      <tr>
        <td>4</td>
        <td>Nailgun</td>
      </tr>
      <tr>
        <td>5</td>
        <td>Super Nailgun</td>
      </tr>
      <tr>
        <td>6</td>
        <td>Grenade Launcher</td>
      </tr>
      <tr>
        <td>7</td>
        <td>Rocket Launcher</td>
      </tr>
      <tr>
        <td>8</td>
        <td>ThunderBolt</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>









</p>
</div>

  <h3 id="bottomcolor">bottomcolor&nbsp;<span>[<a href="#bottomcolor">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets the pants color.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="msg">msg&nbsp;<span>[<a href="#msg">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
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
        <td>Maximum reporting of messages.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Parsed reporting of messages.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Limited reporting of messages.</td>
      </tr>
      <tr>
        <td>3</td>
        <td>Minimal reporting of messages.</td>
      </tr>
      <tr>
        <td>4</td>
        <td>No reporting of messages.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>







</p>
</div>

  <h3 id="name">name&nbsp;<span>[<a href="#name">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>player</span></p>
<div>
  <h4>Description</h4>
  <p>
Player's name.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="noaim">noaim&nbsp;<span>[<a href="#noaim">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

This variable toggles whether a server-sided aiming-help should be used when 
shooting rockets (not possible when the server variable "sv_aim" is set to "0").
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
        <td>Enable server-side aiming help for rockets (if allowed by the server).</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Disable server-side aiming help for rockets.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="rate">rate&nbsp;<span>[<a href="#rate">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>5760</span></p>
<div>
  <h4>Description</h4>
  <p>
Sets the maximum amount of bytes per second that the server should send to the client.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="skin">skin&nbsp;<span>[<a href="#skin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

Sets the skin name for the player.

</p>
</div>

  <h3 id="spectator">spectator&nbsp;<span>[<a href="#spectator">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
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
        <td>Join to server as player.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Join to server as spectator.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="team">team&nbsp;<span>[<a href="#team">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Set the team name.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="topcolor">topcolor&nbsp;<span>[<a href="#topcolor">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>

Sets the shirt color.

</p>
</div>

  <h3 id="w_switch">w_switch&nbsp;<span>[<a href="#w_switch">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

This variable allows you to define the highest weapon that the client should switch 
to when picking it up. The possible arguments of "w_switch" refer to the impulse 
that is used to switch to a certain weapon:
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
1</td>
        <td>Axe</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Shotgun</td>
      </tr>
      <tr>
        <td>3</td>
        <td>Double-Barreled Shotgun</td>
      </tr>
      <tr>
        <td>4</td>
        <td>Nailgun</td>
      </tr>
      <tr>
        <td>5</td>
        <td>Super Nailgun</td>
      </tr>
      <tr>
        <td>6</td>
        <td>Grenade Launcher</td>
      </tr>
      <tr>
        <td>7</td>
        <td>Rocket Launcher</td>
      </tr>
      <tr>
        <td>8</td>
        <td>ThunderBolt</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>









</p>
</div>

<h2 id="server-browser">Server Browser</h2>
  <h3 id="sb_autohide">sb_autohide&nbsp;<span>[<a href="#sb_autohide">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


This toggles in which cases the server browser should automatically hide itself 
when connecting to a server.
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
        <td>Never hide server browser </td>
      </tr>
      <tr>
        <td>1</td>
        <td>This will cause the server browser to always hide after connecting</td>
      </tr>
      <tr>
        <td>2</td>
        <td>This will cause the server browser to hide after connecting only, if the     connected server is not a qizmo proxy, useful if you often connect to Qizmos
    only for rerouting features (with "useproxy 1")
</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="sb_autoupdate">sb_autoupdate&nbsp;<span>[<a href="#sb_autoupdate">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Enabled auto-updates (pings &amp; refreshes status) servers from actually marked server sources in Server Browser.</p>
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
        <td>do not refresh server list automatically</td>
      </tr>
      <tr>
        <td>1 = True</td>
        <td>auto-refresh server list when first entering Server Browser menu</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>Usefull with sb_starttab 1 and '+cfg_load myconfig +menu_slist' in command-line.</p>
</div>

  <h3 id="sb_hidedead">sb_hidedead&nbsp;<span>[<a href="#sb_hidedead">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should hide dead servers.
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
        <td>don't hide dead servers</td>
      </tr>
      <tr>
        <td>1</td>
        <td>hide dead servers</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="sb_hideempty">sb_hideempty&nbsp;<span>[<a href="#sb_hideempty">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should hide empty servers.
  
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
        <td>don't hide empty servers</td>
      </tr>
      <tr>
        <td>1</td>
        <td>hide empty servers</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="sb_hidefull">sb_hidefull&nbsp;<span>[<a href="#sb_hidefull">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should hide full servers.
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
        <td>don't hide full servers </td>
      </tr>
      <tr>
        <td>1</td>
        <td>hide full servers</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="sb_hidenotempty">sb_hidenotempty&nbsp;<span>[<a href="#sb_hidenotempty">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should hide empty servers.
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
        <td>don't hide empty servers</td>
      </tr>
      <tr>
        <td>1</td>
        <td>hide empty servers</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="sb_inforetries">sb_inforetries&nbsp;<span>[<a href="#sb_inforetries">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
This determines how often ezQuake should try to retrieve information from a server 
until it is considered to be not responding.

</p>
  <h4>Remarks</h4>
  <p>


</p>
</div>

  <h3 id="sb_infospersec">sb_infospersec&nbsp;<span>[<a href="#sb_infospersec">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>100</span></p>
<div>
  <h4>Description</h4>
  <p>


This determines how many serverinfos per second ezQuake should retrieve when 
scanning servers. When setting this value too high you will flood your line,
causing you to not receive information from servers or lagging your connect
to the server you are currently connected to.

</p>
</div>

  <h3 id="sb_infotimeout">sb_infotimeout&nbsp;<span>[<a href="#sb_infotimeout">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1000</span></p>
<div>
  <h4>Description</h4>
  <p>


This determines how long ezQuake will wait for a reply when trying to retrieve
information from a server until the attempt times out.
</p>
</div>

  <h3 id="sb_liveupdate">sb_liveupdate&nbsp;<span>[<a href="#sb_liveupdate">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>


This will determine how often ezQuake should refresh the serverinfo window, the
specified value sets the delay in seconds. Setting it to "0" will disable
automatic refreshing.

</p>
</div>

  <h3 id="sb_mastercache">sb_mastercache&nbsp;<span>[<a href="#sb_mastercache">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


This toggles whether ezQuake should cache the results of queries to master server 
(in directory qw/sb/cache). If you restart ezQuake and don't update sources or if 
a master server is down, ezQuake will use the cache.
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
        <td>don't use cache for master servers</td>
      </tr>
      <tr>
        <td>1</td>
        <td>use the cache for master servers when they are down or haven't been refreshed</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="sb_masterretries">sb_masterretries&nbsp;<span>[<a href="#sb_masterretries">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>
This determines how often ezQuake should try to retrieve information from a master 
server until it is considered to be not responding.

</p>
  <h4>Remarks</h4>
  <p>


</p>
</div>

  <h3 id="sb_mastertimeout">sb_mastertimeout&nbsp;<span>[<a href="#sb_mastertimeout">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1000</span></p>
<div>
  <h4>Description</h4>
  <p>


This determines how long ezQuake will wait for a reply when trying to retrieve
information from a master server until the attempt times out.

</p>
</div>

  <h3 id="sb_maxheight">sb_maxheight&nbsp;<span>[<a href="#sb_maxheight">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>480</span></p>
<div>
  <h4>Description</h4>
  <p>


This set the maximum height for the server browser menu in pixels.

</p>
</div>

  <h3 id="sb_maxwidth">sb_maxwidth&nbsp;<span>[<a href="#sb_maxwidth">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>512</span></p>
<div>
  <h4>Description</h4>
  <p>


This set the maximum width for the server browser menu in pixels.

</p>
</div>

  <h3 id="sb_pings">sb_pings&nbsp;<span>[<a href="#sb_pings">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>


This determines how many pings ezQuake will send to a server when trying to
determine your ping to it. A higher value will cause scanning servers to take
longer, but the result may be more exact. A lower value obviously makes scanning
faster, but pings may be inaccurate.

</p>
</div>

  <h3 id="sb_pingspersec">sb_pingspersec&nbsp;<span>[<a href="#sb_pingspersec">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>150</span></p>
<div>
  <h4>Description</h4>
  <p>


This determines how many pings per second ezQuake should sent out when scanning
servers. If you set this value too high the result will be that the pings will 
not be accurate because you overloaded your line. If you set it too low scanning 
servers will take very long especially when you have a large server list. 

</p>
</div>

  <h3 id="sb_pingtimeout">sb_pingtimeout&nbsp;<span>[<a href="#sb_pingtimeout">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1000</span></p>
<div>
  <h4>Description</h4>
  <p>


This determines how long ezQuake will wait for a reply when trying to ping a 
server until the attempt times out.

</p>
</div>

  <h3 id="sb_showaddress">sb_showaddress&nbsp;<span>[<a href="#sb_showaddress">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>


This toggles whether ezQuake should display the server IP column in the server
list.
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
        <td>do not show the server IP column</td>
      </tr>
      <tr>
        <td>1</td>
        <td>show the server IP column</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>



</p>
</div>

  <h3 id="sb_showcounters">sb_showcounters&nbsp;<span>[<a href="#sb_showcounters">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should display the number of servers or players in
the status line.
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
        <td>do not show the server/player counter in the status line</td>
      </tr>
      <tr>
        <td>1</td>
        <td>show the server/player counter in the status line</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="sb_showfraglimit">sb_showfraglimit&nbsp;<span>[<a href="#sb_showfraglimit">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should display the fraglimit column in the server
list.
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
        <td>do not show the fraglimit column</td>
      </tr>
      <tr>
        <td>1</td>
        <td>show the fraglimit column</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="sb_showgamedir">sb_showgamedir&nbsp;<span>[<a href="#sb_showgamedir">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should display the gamedir column in the server list,
for example to see which mod is being played.
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
        <td>do not show the gamedir column</td>
      </tr>
      <tr>
        <td>1</td>
        <td>show the gamedir column</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="sb_showmap">sb_showmap&nbsp;<span>[<a href="#sb_showmap">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should display the map column in the server list.
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
        <td>do not show the map column</td>
      </tr>
      <tr>
        <td>1</td>
        <td>show the map column</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="sb_showping">sb_showping&nbsp;<span>[<a href="#sb_showping">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should display the ping column in the server list.
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
        <td>do not show the ping column</td>
      </tr>
      <tr>
        <td>1</td>
        <td>show the ping column</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="sb_showplayers">sb_showplayers&nbsp;<span>[<a href="#sb_showplayers">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should display the players column in the server list
that shows how many players are on the server as well as how many players are 
allowed.
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
        <td>do not show the players column</td>
      </tr>
      <tr>
        <td>1</td>
        <td>show the players column</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="sb_showtimelimit">sb_showtimelimit&nbsp;<span>[<a href="#sb_showtimelimit">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>
This toggles whether ezQuake should display the timelimit column in the server 
list.
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
        <td>do not show the timelimit column</td>
      </tr>
      <tr>
        <td>1</td>
        <td>show the timelimit column</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="sb_sortplayers">sb_sortplayers&nbsp;<span>[<a href="#sb_sortplayers">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>92</span></p>
<div>
  <h4>Description</h4>
  <p>
This determines sort order in the players list. This uses the numbers from the
description of the columns. Check "Columns available in servers/players list"
above for an explanation of each number. A "-" in front of the value reverses 
the sort order for that value from ascending to descending.
address (ip:port).
</p>
  <h4>Remarks</h4>
  <p>


</p>
</div>

  <h3 id="sb_sortservers">sb_sortservers&nbsp;<span>[<a href="#sb_sortservers">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>32</span></p>
<div>
  <h4>Description</h4>
  <p>


This determines sort order in the servers list. This uses the numbers from the
description of the columns. Check "Columns available in servers/players list" 
above for an explanation of each number. A "-" in front of the value reverses 
the sort order for that value from ascending to descending.
server address (ip:port).
</p>
</div>

  <h3 id="sb_sortsources">sb_sortsources&nbsp;<span>[<a href="#sb_sortsources">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>3</span></p>
<div>
  <h4>Description</h4>
  <p>Ordering instructions for server browser sources list are stored in this variable. You can order source server in Server browser by pressing Ctrl+1, Ctrl+2 and Ctrl+3. Sequence of your desired combination will be stored in this variable, newest keys first.</p>
  <h4>Value</h4>
    <p>String. E.g.: 3-21 means you've pressed Ctrl+1, Ctrl+2, Ctrl+2 and Ctrl+3 so your sources are now ordered by server count, last update time and source name.</p>
  </div>

  <h3 id="sb_sourcevalidity">sb_sourcevalidity&nbsp;<span>[<a href="#sb_sourcevalidity">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>30</span></p>
<div>
  <h4>Description</h4>
  <p>


This sets the time of master servers validity in minutes. Master servers that
were updated within the specified time will not be refreshed when updating 
sources with [SPACE].
</p>
</div>

  <h3 id="sb_starttab">sb_starttab&nbsp;<span>[<a href="#sb_starttab">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>2</span></p>
<div>
  <h4>Description</h4>
  <p>Sets default tab in Server Browser. This tab will be displayed when you enter Server Browser for the first time of client instance.</p>
  <h4>Value</h4>
    <p>Integer. Possible values are 1, 2, 3 and 4.</p>
  <h4>Remarks</h4>
  <p>Usefull with sb_autoupdate 1 and '+cfg_load myconfig +menu_slist' in command-line.</p>
</div>

  <h3 id="sb_status">sb_status&nbsp;<span>[<a href="#sb_status">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>


This toggles whether the server status should be displayed in the two bottom 
lines of the server browser.
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
        <td>do not display the status</td>
      </tr>
      <tr>
        <td>1</td>
        <td>display the status</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>


</p>
</div>

<h2 id="skin-settings">Skin Settings</h2>
  <h3 id="baseskin">baseskin&nbsp;<span>[<a href="#baseskin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>base</span></p>
<div>
  <h4>Description</h4>
  <p>

Defines what skin u see other people using if you don't have their skin and don't have skin forcing on.

</p>
</div>

  <h3 id="enemybothskin">enemybothskin&nbsp;<span>[<a href="#enemybothskin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

Overrides the enemy quad pent skin so you can define the skin which applies to enemy quads pents.

</p>
</div>

  <h3 id="enemypentskin">enemypentskin&nbsp;<span>[<a href="#enemypentskin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

Overrides the enemy pent skin so you can define the skin which applies to enemy pents.

</p>
</div>

  <h3 id="enemyquadskin">enemyquadskin&nbsp;<span>[<a href="#enemyquadskin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

Overrides the enemy quad skin so you can define the skin which applies to enemy quads.

</p>
</div>

  <h3 id="enemyskin">enemyskin&nbsp;<span>[<a href="#enemyskin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

Overrides the enemies skin so you can define the skin which applies to enemies.

</p>
</div>

  <h3 id="noskins">noskins&nbsp;<span>[<a href="#noskins">#</a>]</span></h3>
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
        <td>Enable skins.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Disable skins.</td>
      </tr>
      <tr>
        <td>2</td>
        <td>Enable skins but do not download new skins.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>




</p>
</div>

  <h3 id="r_fullbrightskins">r_fullbrightSkins&nbsp;<span>[<a href="#r_fullbrightskins">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0 or 1 or 1 or 1 or 1</span></p>
<div>
  <h4>Description</h4>
  <p>
Determines the fullbright percentage of skins. Fullbright skins can always be used during demo playback. 
The f_skins response will indicate the brightness level being used as a percentage.
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
        <td>Being 0% fullbright.</td>
      </tr>
      <tr>
        <td>0.5</td>
        <td>Being 50% fullbright.</td>
      </tr>
      <tr>
        <td>1</td>
        <td>Being 100% fullbright.</td>
      </tr>
    </tbody>
  </table>
  <h4>Remarks</h4>
  <p>





</p>
</div>

  <h3 id="teambothskin">teambothskin&nbsp;<span>[<a href="#teambothskin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>
Overrides the team quad pent skin so you can define the skin which applies to team quads pents.

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="teampentskin">teampentskin&nbsp;<span>[<a href="#teampentskin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

Overrides the team pent skin so you can define the skin which applies to team pents.

</p>
</div>

  <h3 id="teamquadskin">teamquadskin&nbsp;<span>[<a href="#teamquadskin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

Overrides the team quad skin so you can define the skin which applies to team quads.

</p>
</div>

  <h3 id="teamskin">teamskin&nbsp;<span>[<a href="#teamskin">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span></span></p>
<div>
  <h4>Description</h4>
  <p>

Overrides the team skin so you can define the skin which applies to team mates.

</p>
</div>

<h2 id="spectator-tracking">Spectator Tracking</h2>
  <h3 id="cl_camera_death">cl_camera_death&nbsp;<span>[<a href="#cl_camera_death">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>Camera view above your body after death.Disabled in -ruleset smackdown
</p>
</div>

  <h3 id="cl_camera_tpp">cl_camera_tpp&nbsp;<span>[<a href="#cl_camera_tpp">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>0 = 1st person view
1 = 3rd person view ala Tomb Raider (such a fun!)
2 = 3rd person view ala cl_chasecam 0 (buggy)
Disabled in -ruleset smackdown
</p>
</div>

  <h3 id="cl_camera_tpp_distance">cl_camera_tpp_distance&nbsp;<span>[<a href="#cl_camera_tpp_distance">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>-56</span></p>
<div>
  <h4>Description</h4>
  <p>Sets distance of 3rd person camera from tracked player.</p>
  <h4>Value</h4>
    <p>Float. You can use negative values too.</p>
  <h4>Remarks</h4>
  <p>Set cl_camera_tpp 1 first. See cl_camera_tpp_height too.</p>
</div>

  <h3 id="cl_camera_tpp_height">cl_camera_tpp_height&nbsp;<span>[<a href="#cl_camera_tpp_height">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>24</span></p>
<div>
  <h4>Description</h4>
  <p>Sets vertical position of 3rd person camera.</p>
  <h4>Value</h4>
    <p>Float. You can use negative values too.</p>
  <h4>Remarks</h4>
  <p>Set cl_camera_tpp 1 first.</p>
</div>

  <h3 id="cl_chasecam">cl_chasecam&nbsp;<span>[<a href="#cl_chasecam">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>1</span></p>
<div>
  <h4>Description</h4>
  <p>
This variable toggles the spectator mode to eye view (1st person).

</p>
  <h4>Remarks</h4>
  <p>

</p>
</div>

  <h3 id="cl_hightrack">cl_hightrack&nbsp;<span>[<a href="#cl_hightrack">#</a>]</span></h3>
  <p class="support">Support: <span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p>
  <p class="default">Default: <span>0</span></p>
<div>
  <h4>Description</h4>
  <p>


Turns auto-tracking player with most frags ON when spectating or watching a demo.

</p>
</div>

