
<ul>
<li><a href="#crosshair+settings">Crosshair Settings</a></li>
<li><a href="#fps+and+eyecandy+settings">FPS and EyeCandy Settings</a></li>
<li><a href="#lighting">Lighting</a></li>
<li><a href="#opengl+rendering">OpenGL Rendering</a></li>
<li><a href="#particle+effects">Particle Effects</a></li>
<li><a href="#screen+%26+powerup+blends">Screen &amp; Powerup Blends</a></li>
<li><a href="#screen+settings">Screen Settings</a></li>
<li><a href="#screenshot+settings">Screenshot Settings</a></li>
<li><a href="#software+rendering">Software Rendering</a></li>
<li><a href="#texture+settings">Texture Settings</a></li>
<li><a href="#video+settings">Video Settings</a></li>
<li><a href="#view+settings">View Settings</a></li>
<li><a href="#water+and+sky+settings">Water and Sky Settings</a></li>
<li><a href="#weapon+view+model+settings">Weapon View Model Settings</a></li>
</ul>

<h2 id="crosshair+settings">Crosshair Settings</h2>

<h3 id="cl_crossx">cl_crossx</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable allows you to move the position of the crosshair on the X-axis
by the specified amount of pixels.

</p><p class="remarks"></p>
<h3 id="cl_crossy">cl_crossy</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable allows you to move the position of the crosshair on the Y-axis
by the specified amount of pixels.

</p><p class="remarks"></p>
<h3 id="crosshair">crosshair</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description">

</p><p class="range">Range: 0/1/2/3/4/5/6/7</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Crosshair off.</td></tr><tr><td>1</td><td>Conchar crosshair (messagemode1 +).</td></tr><tr><td>2</td><td>Quakeworld crosshair.</td></tr><tr><td>3</td><td>A small cross crosshair.</td></tr><tr><td>4</td><td>A dot crosshair.</td></tr><tr><td>5</td><td>Star wars crosshair *gl only*.</td></tr><tr><td>6</td><td>Happyface crosshair *gl only*.</td></tr><tr><td>7</td><td>F22 simulator crosshair *gl only*.</td></tr></tbody></table><p class="remarks">








</p>
<h3 id="crosshairalpha">crosshairalpha</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
This command regulates crosshair transparency, you can use any value from &quot;0&quot; 
to &quot;1&quot;.
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Transparent crosshair.</td></tr><tr><td>1</td><td>Opaque crosshair.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="crosshaircolor">crosshaircolor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"79"</span></p><p class="description">
This variable defines the color of the crosshair.

</p><p class="range">Range: 0..255</p><p class="remarks">
</p>
<h3 id="crosshairimage">crosshairimage</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">

To load the crosshair in FuhQuake use &quot;crosshairimage image_name&quot; where
'image_name' is the name of the crosshair image you put in the above directory.
Example, if I have c:\quake\fuhquake\crosshairs\cross.png I'd use
&quot;crosshairimage cross&quot; to load it.

</p><p class="remarks"></p>
<h3 id="crosshairsize">crosshairsize</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

Basically crosshairsize scales the size of the crosshair. 
Example:
crosshairsize 0.5 halves the size and crosshair 2 doubles it.

</p><p class="remarks"></p>
<h2 id="fps+and+eyecandy+settings">FPS and EyeCandy Settings</h2>

<h3 id="cl_deadbodyfilter">cl_deadbodyFilter</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Don't filter dead bodies.</td></tr><tr><td>1</td><td>Filter dead bodies.</td></tr><tr><td>2</td><td>Filter dead bodies instant.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_gibfilter">cl_gibFilter</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Gibs on.</td></tr><tr><td>1</td><td>Gibs are turned off.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_hidenails">cl_hidenails</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Toggles nails visibility.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="cl_hiderockets">cl_hiderockets</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Toggles rockets visibility. Variable cl_r2g must be 0 to cl_hiderockets 1 work.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="cl_independentphysics">cl_independentPhysics</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_maxfps">cl_maxfps</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
This variable sets the maximum limit for frames-per-second.

</p><p class="remarks">
</p>
<h3 id="cl_model_bobbing">cl_model_bobbing</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>Rotating models bob up and down like in Quake3.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="cl_muzzleflash">cl_muzzleflash</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>All turned off.</td></tr><tr><td>1</td><td>All turned on.</td></tr><tr><td>2</td><td>Own's flash off, all other on.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="cl_nolerp">cl_nolerp</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_physfps">cl_physfps</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_r2g">cl_r2g</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Normal rockets models.</td></tr><tr><td>1</td><td>Converts all rockets to grenades.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_truelightning">cl_trueLightning</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Smoothes out shaft movement, 0 means no smoothing at all, 1 - 100% smoothing;
a value of about 0.5 is recommended for modem players.

</p><p class="range">Range: 0..1</p><p class="remarks">
</p>
<h3 id="gl_caustics">gl_caustics</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

The underwater waves (multi-texturing is required for this variable)
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>On.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="gl_detail">gl_detail</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Turns on/off fine detailed textures.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>On (Looks awesome, but you will take a giant FPS hit if you don't have a good video card).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_waterfog">gl_waterfog</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_waterfogdensity">gl_waterfogDensity</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="r_drawentities">r_drawentities</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
This variable can be used to disable that any object entities are drawn. This 
command is useful to map authors who wish to take a look at their map without 
drawing any objects such as lifts, buttons, platforms or items.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Display no entities.</td></tr><tr><td>1</td><td>Display all entities.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_drawflame">r_drawflame</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Display no flames.</td></tr><tr><td>1</td><td>Display all flames.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_explosionlight">r_explosionLight</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="r_explosionlightcolor">r_explosionLightColor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Determines the colour of the explosion light.
</p><p class="range">Range: 0/1/2/3/4/5/6</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Normal</td></tr><tr><td>1</td><td>Red</td></tr><tr><td>2</td><td>Blue</td></tr><tr><td>3</td><td>Purple</td></tr><tr><td>4</td><td>Green</td></tr><tr><td>5</td><td>White</td></tr><tr><td>6</td><td>Random</td></tr></tbody></table><p class="remarks">








</p>
<h3 id="r_explosiontype">r_explosionType</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"7"</span></p><p class="description">
</p><p class="range">Range: 0/1/2/3/4/5/6/7</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Fire + sparks</td></tr><tr><td>1</td><td>Fire only</td></tr><tr><td>2</td><td>Teleport</td></tr><tr><td>3</td><td>Blood</td></tr><tr><td>4</td><td>Big blood</td></tr><tr><td>5</td><td>Double gunshot</td></tr><tr><td>6</td><td>Blob effect</td></tr><tr><td>7</td><td>Big explosion</td></tr></tbody></table><p class="remarks">









</p>
<h3 id="r_flagcolor">r_flagColor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1/2/3/4/5</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Normal</td></tr><tr><td>1</td><td>Red</td></tr><tr><td>2</td><td>Blue</td></tr><tr><td>3</td><td>Purple</td></tr><tr><td>4</td><td>Green</td></tr><tr><td>5</td><td>White</td></tr></tbody></table><p class="remarks">







</p>
<h3 id="r_grenadetrail">r_grenadeTrail</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Hides grenade trail.</td></tr><tr><td>1</td><td>Shows grenade trail.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_lerpframes">r_lerpframes</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable smoothing.</td></tr><tr><td>1</td><td>Enable smoothing (watch people moving in mvd demos with cl_demospeed 0.1).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_lerpmuzzlehack">r_lerpmuzzlehack</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>No fix for interpolated muzzle flashes.</td></tr><tr><td>1</td><td>Fix for interpolated muzzle flashes.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_lightflicker">r_lightflicker</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable rocket/explosion/flag/powerup light flickering.</td></tr><tr><td>1</td><td>Enable rocket/explosion/flag/powerup light flickering.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_powerupglow">r_powerupGlow</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Powerup glow is turned off on all players.</td></tr><tr><td>1</td><td>Powerup glow on.</td></tr><tr><td>2</td><td>Disables the powerup glow from yourself only.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="r_rocketlight">r_rocketLight</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Turn off dynamic lighting of rockets.</td></tr><tr><td>1</td><td>Turn on dynamic lighting of rockets.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_rocketlightcolor">r_rocketLightColor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Determines the colour of the rocket light.
</p><p class="range">Range: 0/1/2/3/4/5</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Normal</td></tr><tr><td>1</td><td>Red</td></tr><tr><td>2</td><td>Blue</td></tr><tr><td>3</td><td>Purple</td></tr><tr><td>4</td><td>Green</td></tr><tr><td>5</td><td>White</td></tr></tbody></table><p class="remarks">







</p>
<h3 id="r_rockettrail">r_rocketTrail</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"9"</span></p><p class="description">
</p><p class="range">Range: 0/1/2/3/4/5/6/7/8/9</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>No rocket trail</td></tr><tr><td>1</td><td>Normal</td></tr><tr><td>2</td><td>Grenade trail</td></tr><tr><td>3</td><td>Alt normal trail</td></tr><tr><td>4</td><td>Slight blood trail</td></tr><tr><td>5</td><td>Big blood trail</td></tr><tr><td>6</td><td>Tracer1 trail</td></tr><tr><td>7</td><td>Tracer2 trail</td></tr></tbody></table><p class="remarks">









</p>
<h2 id="lighting">Lighting</h2>

<h3 id="gl_coronas">gl_coronas</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Adds coronas to some effects:Explosions - Basically just a bright flash since the default explosions didn't feel powerful enough.
Muzzleflashes.
Lightning.
Fire - Torches have glows.
Rocket Lights.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_coronas_tele">gl_coronas_tele</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Allows you to turn on/off blue light when spawning.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_fb_bmodels">gl_fb_bmodels</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Might give you a couple more fps.</td></tr><tr><td>1</td><td>Enable &quot;fullbright&quot; colors on bsp models (World, health boxes, etc).</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="gl_fb_models">gl_fb_models</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Might also give you more fps.</td></tr><tr><td>1</td><td>Enable &quot;fullbright&quot; colors on alias models (grenades, player models, etc).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_flashblend">gl_flashblend</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Use proper lighting for light sources (powerups etc.).</td></tr><tr><td>1</td><td>Just draw a shaded ball of the color of the light.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_lighting_colour">gl_lighting_colour</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Alias models are effected by coloured lights around them.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_lighting_vertex">gl_lighting_vertex</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Alias models no longer have the same level of light on all sides. This may not work correctly if coloured lighting is disabled.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_lightmode">gl_lightmode</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description">
</p><p class="range">Range: 0/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Makes the map lighter, but fulbrights don't look as good.</td></tr><tr><td>2</td><td>Makes the map darker, but makes fullbrights stand out more (looks better).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_loadlitfiles">gl_loadlitfiles</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable lit files.</td></tr><tr><td>1</td><td>Load lit files.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_shaftlight">gl_shaftlight</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Dark shaft.</td></tr><tr><td>1</td><td>Light shaft.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_ambient">r_ambient</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
This variable controls the amount of ambient lighting on the map. When this 
variable is increased the map will become brighter and all walls and objects 
will be rendered in a lighter color.
It was removed in QWCL and has been re-introduced in FuhQuake now, however it 
can only be used when the server is running with cheats enabled (for example 
&quot;qwsv -cheats&quot;), because it could be used as a cheat by making it easier to see
players in dark corners when no fullbright skins are used.

</p><p class="range">Range: 0/1</p><p class="remarks">
</p>
<h3 id="r_dynamic">r_dynamic</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display dynamic lighting.</td></tr><tr><td>1</td><td>Display dynamic lighting.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="r_fullbright">r_fullbright</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not fullbright the map.</td></tr><tr><td>1</td><td>Enable fullbright display of the map.</td></tr></tbody></table><p class="remarks">


This variable toggles the use of lights at full brightness on the map. When this 
variable is set to &quot;1&quot; the renderer system will render all textures on the map 
at full brightness. This will allow map authors to remove all of the lighting 
effects from the map. 
This variable was removed in QWCL and has been re-introduced in FuhQuake now, 
however it can only be used when the server is running with cheats enabled (for 
example &quot;qwsv -cheats&quot;), because it could be used as a cheat by making it easier
to see players in dark corners when no fullbright skins are used.

</p>
<h3 id="r_lightmap">r_lightmap</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable light sources on the map.</td></tr><tr><td>1</td><td>Enable light sources on the map.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_shadows">r_shadows</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display shadows for entities.</td></tr><tr><td>1</td><td>Display shadows for entities.</td></tr></tbody></table><p class="remarks">



</p>
<h2 id="opengl+rendering">OpenGL Rendering</h2>

<h3 id="gl_affinemodels">gl_affinemodels</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Makes texture rendering quality better if set to 1.
Note: 3dfx thing, not sure. //fuh check this.

</p><p class="range">Range: 0/1</p><p class="remarks">
</p>
<h3 id="gl_amfdebug">gl_amfdebug</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_buildingsparks">gl_buildingsparks</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Buildings that are destroyed in TF will continue to throw sparks until they disapear.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_clear">gl_clear</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable will toggle the clearing of the screen between each frame. This
can be helpful when specing a game and flying out of the map and during map 
development when the map maker must fly outside of the map to look has his map
from the outside. Enabling this toggle will clear the areas which are not 
rendered outside of the map thus preventing the flickering effect when images 
repeat themselves over and over. However it causes problems when playing and 
going through the surfaces of a liquid (for example when diving into water), as
you will see a red flash where the liquid texture would normally be repeated.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>On.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="gl_clearcolor">gl_clearColor</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0 0 0"</span></p><p class="description">
Sets clear color (gl_clear 1)

</p><p class="remarks">
</p>
<h3 id="gl_cull">gl_cull</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

This variable toggles the use of internal OpenGL functions for removing covered 
objects. When this variable is set to &quot;1&quot; the game will depend on the OpenGL 
driver to use it's culling functions to remove certain objects from the 
calculations because they are covered up and would not be seen anyways. It is a 
good idea to leave this toggle enabled because it will increase rendering 
performance.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not use culling to increase rendering performance.</td></tr><tr><td>1</td><td>Use the OpenGL driver's culling functions.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="gl_cutf_tesla_effect">gl_cutf_tesla_effect</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">When a shambler is preparing to throw lightning, or a tesla coil (TF) is charging to fire, a particle effect is generated.</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_detpacklights">gl_detpacklights</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">A little green light appears on the detpack, and when the timer reaches 5, the light changes to red. gl_coronas must be turned on for this to work.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_extratrails">gl_extratrails</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Misc trails that appear on objects that frequently move, such as TF grenades, caltrops, etc.
Railguns in TF also leave behind unique trails.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_finish">gl_finish</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
This variable toggles the calling of the gl_finish() OpenGL function after each 
rendered frame.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not call the gl_finish() function after each frame.</td></tr><tr><td>1</td><td>Call the gl_finish() function after each frame.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_fogblue">gl_fogblue</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.4"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_fogenable">gl_fogenable</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_fogend">gl_fogend</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"800.0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_foggreen">gl_foggreen</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.5"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_fogred">gl_fogred</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.6"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_fogsky">gl_fogsky</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_fogstart">gl_fogstart</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"50.0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_inferno_speed">gl_inferno_speed</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1000"</span></p><p class="description">Changes speed of gl_inferno missile trail.
</p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_inferno_trail">gl_inferno_trail</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description">Changes type of gl_inferno missile trail.
</p><p class="range">Range:  0,1,2,3,4,5</p><p class="remarks"></p>
<h3 id="gl_lightning">gl_lightning</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">0 - Turns off coronas on lightning beams
1 - Turns on coronas on lightning beams.
</p><p class="range">Range:  0,1,X</p><p class="remarks"></p>
<h3 id="gl_lightning_size">gl_lightning_size</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description">Adjusts size of ligtning particle beam.
</p><p class="range">Range:  1-X</p><p class="remarks"></p>
<h3 id="gl_lightning_sparks">gl_lightning_sparks</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.4"</span></p><p class="description">Sparks fly from walls when hit by lightning gun. 
</p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_motiontrails">gl_motiontrails</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">When enabled, flags and keys leave behind a kind of after-image.
Fiends also leave trails when they pounce.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_motiontrails_wtf">gl_motiontrails_wtf</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Same as gl_motiontrails but works for all players and creatures.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_nailtrail">gl_nailtrail</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Adds a white trail onto nails as they fly around.
This feature wont work on servers not running with sv_nailhack set to 1.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_nailtrail_plasma">gl_nailtrail_plasma</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Adds plazma trail after nails.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_nailtrail_water">gl_nailtrail_water</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Switches between two type of bubbles nails leave behind in water. 
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_nocolors">gl_nocolors</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Normal.</td></tr><tr><td>1</td><td>Don't use top/bottom colors.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_smoothmodels">gl_smoothmodels</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not smooth the textures on models.</td></tr><tr><td>1</td><td>Smooth the textures on models.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_surface_lava">gl_surface_lava</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Adds boiling bubbles over the lava.
</p><p class="range">Range:  0,1,2</p><p class="remarks"></p>
<h3 id="gl_surface_slime">gl_surface_slime</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Adds bubbles and fumes of slime over the slime.
</p><p class="range">Range:  0,1,2</p><p class="remarks"></p>
<h3 id="gl_triplebuffer">gl_triplebuffer</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
Triple buffering of video memory related stuff (Triplebuffer = good).
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>On.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_underwater_trails">gl_underwater_trails</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">This gives rockets/grenades/nails and other things an alternate underwater trail.
Very nice to have nailgun fights underwater...
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_waterripple">gl_waterripple</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Put your life vests on, waves are coming!
</p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_weather_rain">gl_weather_rain</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Turns on rain out of doors, the density of rain is equal to whatever gl_weather_rain is set to.
If you set gl_weather_rain_fast to 1, you can turn off all splashes, if you set it to 2, you will turn off only the water splashes.
Works on all non-iD maps except death32c, dakyne and some others.
</p><p class="range">Range:  0,1,2,X</p><p class="remarks"></p>
<h3 id="gl_weather_rain_fast">gl_weather_rain_fast</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Adjusts speed of rain.
</p><p class="range">Range:  0,X</p><p class="remarks"></p>
<h3 id="gl_ztrick">gl_ztrick</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
This variables toggles the use of a trick to prevent the clearning of the 
z-buffer between frames. When this variable is set to &quot;1&quot;, the game will not 
clear the z-buffer between frames. This will result in increased performance 
but might cause problems for some display hardware.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not use the z-buffer trick.</td></tr><tr><td>1</td><td>Use the z-buffer trick.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_farclip">r_farclip</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"4096"</span></p><p class="description">
Can be used to overcome vision being limited to 4096 units in GL clients (good for xpress2 etc).

</p><p class="remarks">
</p>
<h2 id="particle+effects">Particle Effects</h2>

<h3 id="gl_bounceparticles">gl_bounceparticles</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Controls whether sparks rebound off walls.  </td></tr></tbody></table><p class="remarks">

Bouncing particles look nicer, but may eat up CPU.

</p>
<h3 id="gl_clipparticles">gl_clipparticles</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>this will limit the number of blended particles close to you. This gives a big FPS increase when blended particles take up a big proportion of your screen and challenge your video card's fillrate.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_part_blobs">gl_part_blobs</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Determines particles used for blob explosions (EMP's)</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_part_blood">gl_part_blood</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Determines particles used for blood effects.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_part_explosions">gl_part_explosions</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Determines particles used for explosions.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_part_gunshots">gl_part_gunshots</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Determines particles used for gunshots.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_part_inferno">gl_part_inferno</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Determines particles used for pyro flames in TF.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_part_lavasplash">gl_part_lavasplash</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Determines particles used for lava splashes (Spy Gren).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_part_spikes">gl_part_spikes</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Determines particles used for spikes (nailgun etc).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_part_telesplash">gl_part_telesplash</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Determines particles used for teleport splashes.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_part_trails">gl_part_trails</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Determines particles used for (rocket etc) trails.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_particle_blobs">gl_particle_blobs</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.1"</span></p><p class="description"></p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_particle_blood">gl_particle_blood</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_blood_color">gl_particle_blood_color</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Changes colour of blood.
</p><p class="range">Range:  0,1,X</p><p class="remarks"></p>
<h3 id="gl_particle_blood_type">gl_particle_blood_type</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Chooses among types of blood particles.
</p><p class="range">Range:  1,X</p><p class="remarks"></p>
<h3 id="gl_particle_deatheffect">gl_particle_deatheffect</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_explosions">gl_particle_explosions</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Turns particle alternatives to each r_explosiontype on or off.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_fasttrails">gl_particle_fasttrails</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_fire">gl_particle_fire</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Replaces torches with a particle flame effect. Includes people being burnt by the pyro in TF.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_gibtrails">gl_particle_gibtrails</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_gunshots">gl_particle_gunshots</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_particle_gunshots_type">gl_particle_gunshots_type</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="range">Range:  1,2,3,4</p><p class="remarks"></p>
<h3 id="gl_particle_muzzleflash">gl_particle_muzzleflash</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">Adds particle effect when firing a weapon.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_shockwaves">gl_particle_shockwaves</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">0 - Turns off explosion shockwaves.
1 - Turns on explosion shockwaves.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_shockwaves_flat">gl_particle_shockwaves_flat</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_sparks">gl_particle_sparks</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Trails appear more beam-like and don't look stupid when they bounce.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="gl_particle_spikes">gl_particle_spikes</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.1"</span></p><p class="description"></p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_particle_spikes_type">gl_particle_spikes_type</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="range">Range:  1,2,3,4</p><p class="remarks"></p>
<h3 id="gl_particle_trail_detail">gl_particle_trail_detail</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_particle_trail_lenght">gl_particle_trail_lenght</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">X - Multiplies the length of the trail on particle trails.
</p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_particle_trail_time">gl_particle_trail_time</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">X - Multiplies the length of time the particle bounces around for.
</p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_particle_trail_type">gl_particle_trail_type</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="range">Range:  0,1,X</p><p class="remarks"></p>
<h3 id="gl_particle_trail_width">gl_particle_trail_width</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description">Changes width of trail particle e.g.: wall hitted by nail, explosion particles trail, etc. 
</p><p class="range">Range:  0-X</p><p class="remarks"></p>
<h3 id="gl_solidparticles">gl_solidparticles</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Might make tele particles faster for 3dfx wicked gl drivers.</td></tr></tbody></table><p class="remarks">



</p>
<h2 id="screen+%26+powerup+blends">Screen &amp; Powerup Blends</h2>

<h3 id="cl_bonusflash">cl_bonusflash</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Controls weapon and item pickup flash.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>On.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_cshiftpercent">gl_cshiftpercent</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"100"</span></p><p class="description">
This variable sets the percentage value for palette shifting effects (damage
flash, powerup blend).

</p><p class="remarks">
</p>
<h3 id="gl_hwblend">gl_hwblend</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>OpenGL routines.</td></tr><tr><td>1</td><td>Hardware palette changing.</td></tr></tbody></table><p class="remarks">

Note: 1 can be slow on wintNT/2K.

</p>
<h3 id="gl_polyblend">gl_polyblend</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable palette shifting when submerged or poweruped.</td></tr><tr><td>1</td><td>Enable palette shifting when submerged or poweruped.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="v_contentblend">v_contentblend</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off (no palette shifting when you're in liquid).</td></tr><tr><td>1</td><td>On  (palette shifts when you're in liquid).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="v_damagecshift">v_damagecshift</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Damage filter on.</td></tr><tr><td>0.5</td><td>50% Damage filter on.</td></tr><tr><td>1</td><td>Damage filter off.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="v_dlightcshift">v_dlightcshift</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Turns off light blend when you're inside a light bubble (with gl_flashblend 1).</td></tr><tr><td>1</td><td>Turns onn light blebd when you're inside a light bubble (with gl_flashblend 1).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="v_pentcshift">v_pentcshift</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
Flash coefficient for when you take the pent.

</p><p class="range">Range: 0..1</p><p class="remarks">
</p>
<h3 id="v_quadcshift">v_quadcshift</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

Flash coefficient for when you take the quad.

</p><p class="range">Range: 0..1</p><p class="remarks"></p>
<h3 id="v_ringcshift">v_ringcshift</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

Flash coefficient for when you take the ring.

</p><p class="range">Range: 0..1</p><p class="remarks"></p>
<h3 id="v_suitcshift">v_suitcshift</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

Flash coefficient for when you take the suit.

</p><p class="range">Range: 0..1</p><p class="remarks"></p>
<h2 id="screen+settings">Screen Settings</h2>

<h3 id="cl_clock">cl_clock</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>Shows the time you spent on server (you'll also see it in the status bar when you press TAB).</td></tr><tr><td>2</td><td>Shows the time of day.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="cl_clock_x">cl_clock_x</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Horizontal coordinates of the clock.

</p><p class="remarks">
</p>
<h3 id="cl_clock_y">cl_clock_y</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"-1"</span></p><p class="description">

Vertical coordinates of the clock.
If &lt; 0, the coordinates are calculated from bottom up, e.g. -1 means the screen line just above the scoreboard.

</p><p class="remarks"></p>
<h3 id="cl_confirmquit">cl_confirmquit</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_democlock">cl_democlock</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

A clock showing how much time has elapsed since the start of the demo.
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>On.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="cl_democlock_x">cl_democlock_x</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Determine where the democlock is positioned on your screen on the X co-ordinate.

</p><p class="remarks">
</p>
<h3 id="cl_democlock_y">cl_democlock_y</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"-2"</span></p><p class="description">

Determine where the democlock is positioned on your screen on the Y co-ordinate.

</p><p class="remarks"></p>
<h3 id="cl_shownet">cl_shownet</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable toggles the display of current net info.
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display any information.</td></tr><tr><td>1</td><td>Print current incoming packet size in byte.</td></tr><tr><td>2</td><td>Print information about the current in coming packet and it's size.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="draw_fps">draw_fps</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_contrast">gl_contrast</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
Change contrast.

</p><p class="remarks">
</p>
<h3 id="gl_gamma">gl_gamma</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

Change GL gamma.

</p><p class="remarks"></p>
<h3 id="r_glstats">r_glstats</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">When enabled, it creates a window in the top right of the screen showing the number of particles and etc. in use.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="r_netgraph">r_netgraph</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display the netgraph.</td></tr><tr><td>1</td><td>Display the netgraph.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="r_tracker_flags">r_tracker_flags</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Everytime you take, capture or drop a flag, the number of times is displayed on the screen.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="r_tracker_frags">r_tracker_frags</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Everytime you make a kill or die, the number of kills/deaths/suicides with that weapon is displayed on the screen.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="r_tracker_messages">r_tracker_messages</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"10"</span></p><p class="description"></p><p class="range">Range:  0,1,X</p><p class="remarks"></p>
<h3 id="r_tracker_streaks">r_tracker_streaks</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">Everytime a player makes a set number of consecutive kills, it will display a message showing they are on a streak, when the player is killed, it will display the name of the person who ended that streak.
</p><p class="range">Range:  0,1</p><p class="remarks"></p>
<h3 id="r_tracker_time">r_tracker_time</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"4"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="scr_autoid">scr_autoid</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Hide players nick as spectator.</td></tr><tr><td>1</td><td>Show players nick as spectator.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_centermenu">scr_centerMenu</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>Centers the menu vertically (has no effect if you're playing in 320x200 mode, same applies to scr_centersbar).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_centertime">scr_centertime</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description">
This variable sets the amount of time in seconds that centerprinted messages 
stay on the screen. 
</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display any centerprints.</td></tr><tr><td>1</td><td>Display centerprints for 1 second.</td></tr><tr><td>2</td><td>Display centerprints for 2 seconds.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="scr_coloredtext">scr_coloredText</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable rainbow spectator text on the scoreboard.</td></tr><tr><td>1</td><td>Enable rainbow spectator text on the scoreboard.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_menualpha">scr_menualpha</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.7"</span></p><p class="description">
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Transparent mainmenu.</td></tr><tr><td>1</td><td>Black mainmenu.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="scr_spectatormessage">scr_spectatorMessage</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="scr_tracking">scr_tracking</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="show_fps">show_fps</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disables the display of the frames-per-second value.</td></tr><tr><td>1</td><td>Enables the display of the frames-per-second value.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="show_fps_x">show_fps_x</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"-5"</span></p><p class="description">
Determine where the show_fps is positioned on your screen on the X co-ordinate.

</p><p class="remarks">
</p>
<h3 id="show_fps_y">show_fps_y</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"-1"</span></p><p class="description">

Determine where the show_fps is positioned on your screen on the Y co-ordinate.

</p><p class="remarks"></p>
<h3 id="show_speed">show_speed</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

Displays a speed counter in the top right corner in FuhQuake units (Horizontal velocity). (320 - Normal walking speed, 440 - Accel jump, 450 - Bunnyhopping)
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>More accurate counter (Best used when LPB).</td></tr><tr><td>2</td><td>Predicted speed (Less laggy when HPB).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="show_speed_x">show_speed_x</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"-1"</span></p><p class="description">
Determine where the show_speed is positioned on your screen on the X co-ordinate.

</p><p class="remarks">
</p>
<h3 id="show_speed_y">show_speed_y</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

Determine where the show_speed is positioned on your screen on the Y co-ordinate.

</p><p class="remarks"></p>
<h3 id="showdrop">showdrop</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable output how dropped packets.</td></tr><tr><td>1</td><td>Enable output how dropped packets.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="showpackets">showpackets</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display information about all network packets.</td></tr><tr><td>1</td><td>Display information about all network packets.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="showpause">showpause</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Never display the pause icon.</td></tr><tr><td>1</td><td>Display that the pause icon is displayed when the game is paused.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="showram">showram</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Never display the ram icon</td></tr><tr><td>1</td><td>Display the ram icon when running out of memory</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="showturtle">showturtle</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Never display the turtle icon.</td></tr><tr><td>1</td><td>Display the turtle icon when your frame rate drops below 10.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="sw_contrast">sw_contrast</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

Change contrast.

</p><p class="remarks"></p>
<h3 id="sw_gamma">sw_gamma</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

Change gamma.

</p><p class="remarks"></p>
<h2 id="screenshot+settings">Screenshot Settings</h2>

<h3 id="image_jpeg_quality_level">image_jpeg_quality_level</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"75"</span></p><p class="description">

You can set quality of jpeg screenshots with 'image_jpeg_quality_level x' 
where x is an integer from 0 to 100 inclusive. The larger x is, 
the better the quality of a jpeg screenshot, but also the bigger the size.

</p><p class="remarks"></p>
<h3 id="image_png_compression_level">image_png_compression_level</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

You can set the amount of png compression with 'image_png_compression_level x' 
where x is an integer from 0 to 9 inclusive. 0 gives no compression and 9 gives 
maximum compression (and slowest writing time).

</p><p class="remarks"></p>
<h3 id="scr_allowsnap">scr_allowsnap</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable</td></tr><tr><td>1</td><td>Allow screenshot uploads to be requested by server.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="sshot_dir">sshot_dir</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">
Change the screenshot directory.

</p><p class="remarks">
</p>
<h3 id="sshot_format">sshot_format</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"pcx"</span></p><p class="description">

tga = Tga screenshots (gl only)
png = Png screenshots
jpg = Jpg screenshots (gl only)
pcx = Pcx screenshots (software only)

</p><p class="range">Range: tga/png/pcx/jpg</p><p class="remarks"></p>
<h2 id="software+rendering">Software Rendering</h2>

<h3 id="cl_multiview">cl_multiview</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_mvdisplayhud">cl_mvdisplayhud</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_mvinset">cl_mvinset</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_mvinsetcrosshair">cl_mvinsetcrosshair</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="cl_mvinsethud">cl_mvinsethud</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="r_aliastransadj">r_aliastransadj</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"100"</span></p><p class="description">

Not much is known about this variable except for that increasing the value will
cause the player models to be displayed smaller.

</p><p class="remarks"></p>
<h3 id="r_aliastransbase">r_aliastransbase</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"200"</span></p><p class="description">

Not much is known about this variable except for that increasing the value will
cause the player models to be displayed smaller.

</p><p class="remarks"></p>
<h3 id="r_clearcolor">r_clearcolor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description">

This variable can be used to change the color for the areas which are not part 
of the map when free floating outside of the map as spectator.

</p><p class="range">Range: 0..254</p><p class="remarks"></p>
<h3 id="r_draworder">r_draworder</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="r_dspeeds">r_dspeeds</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display renderer speed statistics.</td></tr><tr><td>1</td><td>Display renderer speed statistics.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="r_graphheight">r_graphheight</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"15"</span></p><p class="description">
This variable used to set the number of lines displayed in the &quot;r_timegraph&quot; 
command.

</p><p class="remarks">
</p>
<h3 id="r_maxedges">r_maxedges</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable sets the maximum number of plane surface edges to be rendered.

</p><p class="remarks"></p>
<h3 id="r_maxsurfs">r_maxsurfs</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable sets the maximum number of plane surfaces to be rendered.

</p><p class="remarks"></p>
<h3 id="r_numedges">r_numedges</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not print information about the number of edges.</td></tr><tr><td>1</td><td>Print information about the number of edges.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="r_numsurfs">r_numsurfs</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not print information about the number of surfaces.</td></tr><tr><td>1</td><td>Print information about the number of surfaces.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_polymodelstats">r_polymodelstats</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not print polygon model statistics.</td></tr><tr><td>1</td><td>Print polygon model statistics.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_reportedgeout">r_reportedgeout</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not report when running out of edges.</td></tr><tr><td>1</td><td>Report when running out of edges.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_reportsurfout">r_reportsurfout</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Toggle the display of how many surfaces where not displayed. This
shouldn't happen during normal game play, only when in noclip mode.

</p><p class="range">Range: 0/1</p><p class="remarks">
</p>
<h3 id="r_speeds">r_speeds</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

Toggles the displaying of drawing time and stats of what is
currently being viewed.
Example:
32.7 ms 267/196/ 74 poly   3 surf
38.6 ms 267/196/ 74 poly  20 surf
60.4 ms 267/196/ 74 poly  18 surf
38.2 ms 267/196/ 73 poly  18 surf

</p><p class="range">Range: 0/1</p><p class="remarks"></p>
<h3 id="r_timegraph">r_timegraph</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display the timegraph.</td></tr><tr><td>1</td><td>Display the timegraph.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="r_zgraph">r_zgraph</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display the z-graph.</td></tr><tr><td>1</td><td>Display the z-graph.</td></tr></tbody></table><p class="remarks">



</p>
<h2 id="texture+settings">Texture Settings</h2>

<h3 id="d_mipcap">d_mipcap</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1/2/3</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>High texture detail.</td></tr><tr><td>1</td><td>Medium texture detail.</td></tr><tr><td>2</td><td>Low texture detail.</td></tr><tr><td>3</td><td>Minimum texture detail.</td></tr></tbody></table><p class="remarks">




</p>
<h3 id="d_mipscale">d_mipscale</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1/2/3</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Full object detail</td></tr><tr><td>1</td><td>Some object detail</td></tr><tr><td>2</td><td>Medium object detail</td></tr><tr><td>3</td><td>Low object detail</td></tr></tbody></table><p class="remarks">





</p>
<h3 id="d_subdiv16">d_subdiv16</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Precise texture mapping (slower, more accurate).</td></tr><tr><td>1</td><td>Perspective correction only every 16 pixels (faster, less accurate).</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_ext_texture_compression">gl_ext_texture_compression</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Enable reduce memory held by textures on cards that support it (but </td></tr></tbody></table><p class="remarks">


textures loaded slower and may have less quality).

</p>
<h3 id="gl_externaltextures_bmodels">gl_externalTextures_bmodels</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable loading of external 24bit textures for</td></tr><tr><td>1</td><td>Enable</td></tr></tbody></table><p class="remarks">

non-world .bsp models (any .bsp that isn't the actual map.  Ex. health and ammo boxes).


</p>
<h3 id="gl_externaltextures_world">gl_externalTextures_world</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable loading of external 24bit textures for the</td></tr><tr><td>1</td><td>Enable.</td></tr></tbody></table><p class="remarks">

world .bsp model (i.e. the actual map).


</p>
<h3 id="gl_lerpimages">gl_lerpimages</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Faster loading maps (not more fps).</td></tr><tr><td>1</td><td>Better texture quality.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_lumatextures">gl_lumaTextures</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="gl_max_size">gl_max_size</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"4096"</span></p><p class="description">
This variable determines the detail level for loaded textures. When set to &quot;1&quot;,
the objects and walls will be textured with 1x1 pixel textures.

</p><p class="remarks">
</p>
<h3 id="gl_miptexlevel">gl_miptexLevel</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

It is essentially a GL 'equivalent'
to d_mipcap in software.  It has no affect when loading external 24bit textures.

</p><p class="range">Range: 0/1/2/3</p><p class="remarks"></p>
<h3 id="gl_picmip">gl_picmip</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable determines the the level of detail for textures used on walls. You
can use this variable to lower the texture detail used on walls thus increasing
the game's performance.
</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Original size.</td></tr><tr><td>1</td><td>Half the dimensions.</td></tr><tr><td>2</td><td>One-fourth the dimensions.</td></tr></tbody></table><p class="remarks">


X = etc..

</p>
<h3 id="gl_playermip">gl_playermip</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
Determines the level of detail of player models. This variable is useful if you are 
experience game slowdown during multiplayer games when there are lots of players on 
your screen by shrinking the texture detail, but this will make the player models more blurry.
</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Full detail.</td></tr><tr><td>1</td><td>Medium detail.</td></tr><tr><td>2</td><td>Low detail.</td></tr></tbody></table><p class="remarks">



X = etc.

</p>
<h3 id="gl_scalemodeltextures">gl_scaleModelTextures</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable scale model textures.</td></tr><tr><td>1</td><td>Enable</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_scaleturbtextures">gl_scaleTurbTextures</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable gl_picmip/gl_max_size/gl_miptexLevel affect turb textures (lava, water, slime, teleports).</td></tr><tr><td>1</td><td>Enable.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="gl_subdivide_size">gl_subdivide_size</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"128"</span></p><p class="description">
This variable sets the division value for the sky brushes. The higher the value 
the better the performance, but the smoothness of the sky suffers.

</p><p class="remarks">
</p>
<h3 id="gl_texturemode">gl_texturemode</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"GL_LINEAR_MIPMAP_NEAREST"</span></p><p class="description">

</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
GL_NEAREST</td><td>Point sampled</td></tr><tr><td>GL_NEAREST_MIPMAP_NEAREST</td><td></td></tr><tr><td>GL_LINEAR</td><td></td></tr><tr><td>GL_LINEAR_MIPMAP_NEAREST</td><td></td></tr><tr><td>GL_LINEAR_MIPMAP_LINEAR</td><td>Trilinear interpolation</td></tr></tbody></table><p class="remarks">





</p>
<h3 id="r_max_size_1">r_max_size_1</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>Blend map textures.</td></tr><tr><td>2</td><td>Blend map textures and bsp entities (ammo, health etc.)</td></tr></tbody></table><p class="remarks">




</p>
<h2 id="video+settings">Video Settings</h2>

<h3 id="_vid_default_mode">_vid_default_mode</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable sets the default video mode that FuhQuake uses.

</p><p class="remarks"></p>
<h3 id="_vid_default_mode_win">_vid_default_mode_win</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description">

This variable sets the default video mode that FuhQuake uses when windowed.

</p><p class="remarks"></p>
<h3 id="_windowed_mouse">_windowed_mouse</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable windowed mouse support so you can use the mouse in other applications.</td></tr><tr><td>1</td><td>Enabled windowed mouse support so you can use the mouse in FuhQuake.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="block_switch">block_switch</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not block task-switching</td></tr><tr><td>1</td><td>Block task-switching</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="vid_config_x">vid_config_x</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"800"</span></p><p class="description">
This variable sets the width of the vid_mode 2 window, the minimum possible
value is &quot;640&quot;.

</p><p class="remarks">
</p>
<h3 id="vid_config_y">vid_config_y</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"600"</span></p><p class="description">

This variable sets the width of the vid_mode 2 window, the minimum possible
value is &quot;400&quot;

</p><p class="remarks"></p>
<h3 id="vid_displayfrequency">vid_displayfrequency</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="vid_flashonactivity">vid_flashonactivity</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="vid_fullscreen_mode">vid_fullscreen_mode</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"3"</span></p><p class="description">

This variable sets the full screen video mode that the game will switch to when
the &quot;vid_fullscreen&quot; command is executed.

</p><p class="remarks"></p>
<h3 id="vid_hwgammacontrol">vid_hwgammacontrol</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

</p><p class="range">Range: 0/1/2</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disables hardware gamma control. </td></tr><tr><td>1</td><td>Enables hardware gamma control.</td></tr><tr><td>2</td><td>Enables hwgamma in windowed mode.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="vid_mode">vid_mode</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"5"</span></p><p class="description">
This variables sets the next video mode to be used, any change of this variable
will cause the video mode to be changed immediately.

</p><p class="remarks">
</p>
<h3 id="vid_nopageflip">vid_nopageflip</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable toggles the use of page-flipping during supported video modes. By 
default the game will allow for page-flipping for video modes that support this 
feature. For example if a given VESA mode can support page flipping, then it 
defaults to page-flipped operation.  A VESA mode can be forced to 
non-page-flipped operation by setting the &quot;vid_nopageflip&quot; variable to &quot;1&quot;, then
setting the mode (note that &quot;vid_nopageflip&quot; takes operation on the next, 
not the current video mode, and note that it then stays in effect permanently, 
even when FuhQuake is exited and restarted, unless it is manually set back to &quot;0&quot;).
If there is not enough memory for two pages in a VESA mode, or if the adapter 
doesn't support page flipping, then the mode will automatically be 
non-page-flipped.  
Page-flipping works by drawing multiple virtual game screens at the same time in 
the video memory and then switching among them to display the current 
gamescreen. Page-flipped modes thus use less system memory than non-page-flipped 
modes, but require more video memory. When using page-flipping, visual quality 
may be higher, but performance of the game can change to better or worse, 
depending on the graphics adapter and other hardware (see the discussion of the 
Pentium Pro below, for a discussion of why page flipping can be faster but is 
sometimes much slower on that processor). However in most cases the performance 
(and thus frame rate) will be increased, but for people with hardware that has 
problems with page-flipping &quot;vid_nopageflip 1&quot; may help.
The Pentium Pro is an example for such a hardware: it is a fast FuhQuake platform, 
but it has one weak spot, it is by default very slow on writes to video memory.  
This means that in default hardware configurations, you are usually much better 
off setting &quot;vid_nopageflip&quot; to &quot;1&quot; if you use VESA modes, so drawing is done to 
system memory instead of to video memory. Remember that you must set the mode
after setting &quot;vid_nopageflip&quot; to &quot;1&quot; in order to get &quot;vid_nopageflip&quot; to take
effect. &quot;vid_nopageflip 1&quot; can sometimes be faster on a Pentium too, but not by 
nearly as much in general, and it's often slower.

</p><p class="range">Range: 0/1</p><p class="remarks"></p>
<h3 id="vid_redrawfull">vid_redrawfull</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="vid_ref">vid_ref</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"soft"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="vid_resetonswitch">vid_resetonswitch</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Enable (This is a workaround for the bug in some NVidia drivers, when you alt-tab out of </td></tr></tbody></table><p class="remarks">

FuhQuake and the screen gets garbaged. When enabled, FuhQuake will reset your Windows video mode 
every time you press alt-tab. Note that some NVidia drivers lock up your system completely 
when you alt-tab. vid_resetonswitch will not help in this case (try starting FuhQuake with -dibonly).

</p>
<h3 id="vid_stretch_by_2">vid_stretch_by_2</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable.</td></tr><tr><td>1</td><td>Enable (This variable toggles whether in &quot;vid_mode 2&quot; the picture should be rendered at </td></tr></tbody></table><p class="remarks">


half-resolution in each direction and stretched up to the specified size.)

</p>
<h3 id="vid_vsync">vid_vsync</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>""</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable vsync.</td></tr><tr><td>1</td><td>Enable vsync.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="vid_waitforrefresh">vid_waitforrefresh</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description"></p><p class="remarks"></p>
<h3 id="vid_window_x">vid_window_x</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable sets the x-axis location of the top left corner of the game window 
on the desktop.

</p><p class="remarks"></p>
<h3 id="vid_window_y">vid_window_y</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable sets the y-axis location of the top left corner of the game window 
on the desktop.

</p><p class="remarks"></p>
<h3 id="vid_windowed_mode">vid_windowed_mode</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
This variable sets the windowed video mode that the game will switch to when the 
&quot;vid_windowed&quot; command is executed.

</p><p class="remarks">
</p>
<h2 id="view+settings">View Settings</h2>

<h3 id="cl_rollangle">cl_rollangle</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"2.0"</span></p><p class="description">
This variable controls how much your screen tilts when strafing.

</p><p class="remarks">
</p>
<h3 id="cl_rollspeed">cl_rollspeed</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"200"</span></p><p class="description">

This variable controls how quickly you and other players straighten out after 
strafing.

</p><p class="remarks"></p>
<h3 id="default_fov">default_fov</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

Very useful in TeamFortress, and other mods that reset your fov when you spawn to 90. 
Basically, when the server sends you &quot;fov 90&quot;, your fov is set to default_fov.

</p><p class="remarks"></p>
<h3 id="fov">fov</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"90"</span></p><p class="description">

This variable defines your field of vision, which determines how close your 
vision field is to the objects.

</p><p class="remarks"></p>
<h3 id="v_centermove">v_centermove</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.15"</span></p><p class="description">
This variable sets the distance that the player must move before the screen 
automatically centers itself when &quot;lookspring&quot; is set to &quot;1&quot;. This variable is 
only useful to people who only play with the keyboard, have &quot;lookspring&quot; set to
&quot;1&quot; and do not have &quot;+klook&quot; or &quot;+mlook&quot; enabled at that time. When the player 
moves the distance specified in this variable the screen will pop back to the 
center position.

</p><p class="remarks">
</p>
<h3 id="v_centerspeed">v_centerspeed</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"500"</span></p><p class="description">

This variable sets how quickly your view returns to the center after looking up
or down and moving after that and &quot;lookspring&quot; is set to &quot;1&quot;. This variable is 
only useful to people who only play with the keyboard, have &quot;lookspring&quot; set to
&quot;1&quot; and do not have &quot;+klook&quot; or &quot;+mlook&quot; enabled at that time. When the player 
moves the distance specified in this variable the screen will pop back to the 
center position.

</p><p class="remarks"></p>
<h3 id="v_gunkick">v_gunkick</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

Kickback effect when the weapon is fired.

</p><p class="range">Range: 0..1</p><p class="remarks"></p>
<h3 id="v_idlescale">v_idlescale</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

This variable controls the amount that the screen should sway automatically.
When this variable is set to a value above &quot;0&quot; the screen will start swaying in 
all directions, similar as if the player was drunk of had a concussion. If you 
want to have some real fun, set this variable to a value of &quot;100&quot; and run around 
some maps. The larger the value for this variable the more the screen will sway 
from side to side.
</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable automatic swaying.</td></tr></tbody></table><p class="remarks">

</p>
<h3 id="v_ipitch_cycle">v_ipitch_cycle</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
This variable controls the speed at which the screen should sway up and down
when &quot;v_idlescale&quot; is set to a value above &quot;0&quot;.

</p><p class="remarks">
</p>
<h3 id="v_ipitch_level">v_ipitch_level</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.3"</span></p><p class="description">

This variable controls the distance that the screen should sway up and down when &quot;v_idlescale&quot; is set to a value above &quot;0&quot;.

</p><p class="remarks"></p>
<h3 id="v_iroll_cycle">v_iroll_cycle</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.5"</span></p><p class="description">

This variable controls the speed at which the screen should roll clockwise and 
counter clockwise when &quot;v_idlescale&quot; is set to a value above &quot;0&quot;.

</p><p class="remarks"></p>
<h3 id="v_iroll_level">v_iroll_level</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.1"</span></p><p class="description">

This variable controls the distance the screen should roll clockwise and counter
clockwise when &quot;v_idlescale&quot; is set to a value above &quot;0&quot;.

</p><p class="remarks"></p>
<h3 id="v_iyaw_cycle">v_iyaw_cycle</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"2"</span></p><p class="description">

Sets how quickly you look left and right when v_idlescale is active.

</p><p class="remarks"></p>
<h3 id="v_iyaw_level">v_iyaw_level</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.3"</span></p><p class="description">

Sets how far you look left and right when v_idlescale is active.

</p><p class="remarks"></p>
<h3 id="v_kickpitch">v_kickpitch</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.6"</span></p><p class="description">

This variable controls the distance that the screen should move up or down when 
the player is shot. Most players set this variable to &quot;0&quot; in order to remove the 
screen tilting effect when they are injured by a shot. This allows them to keep 
a perfect aim while being fired upon.

</p><p class="remarks"></p>
<h3 id="v_kickroll">v_kickroll</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.6"</span></p><p class="description">

This variable controls the distance that the screen should roll clockwise or 
counter clockwise when the player is shot. Most players set this variable to &quot;0&quot;
in order to remove the screen tilting effect when they are injured by a shot. 
This allows them to keep a perfect aim while being fired upon.

</p><p class="remarks"></p>
<h3 id="v_kicktime">v_kicktime</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.5"</span></p><p class="description">

This variable controls the amount of time that the screen should remain tilted 
after the player has been shot. Most players set this variable to &quot;0&quot; in order 
to remove the screen tilting effect when they are injured by a shot. This allows 
them to keep a perfect aim while being fired upon.

</p><p class="remarks"></p>
<h3 id="v_viewheight">v_viewheight</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

New variable v_viewheight cam be used to get the effect of negative/zero values 
of cl_bobcycle etc in qw 2.33 . Negative values of v_viewheight = lower viewheight, 
positive = higher. Can't go below -7 or above 4 because that's the range you can get 
with qw 2.33 by exploiting the bob cycle bug. Using v_viewheight automatically 
turns off weapon model bobbing (cl_bob*).

</p><p class="remarks"></p>
<h3 id="viewsize">viewsize</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"100"</span></p><p class="description">

This variable determines how large the viewable screen size is.
</p><p class="range">Range: 30-120</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
30</td><td>Minimum screen size</td></tr><tr><td>100</td><td>Normal screen size.</td></tr><tr><td>110</td><td>Large screen size, without the weapons status bar.</td></tr><tr><td>120</td><td>Maximum screen size, without any status bars.</td></tr></tbody></table><p class="remarks">
...




</p>
<h2 id="water+and+sky+settings">Water and Sky Settings</h2>

<h3 id="r_fastsky">r_fastsky</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable fastsky. </td></tr><tr><td>1</td><td>Enable fastsky, helps fps a bit on maps with large sky areas.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="r_fastturb">r_fastturb</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>All turbulences will be displayed normally.</td></tr><tr><td>1</td><td>All turbulences will be replaced by single-coloured surfaces.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_lavacolor">r_lavacolor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"73"</span></p><p class="description">Changes colour of lava when r_fastturb set to 1
</p><p class="range">Range:  0-255 0-255 0-255</p><p class="remarks"></p>
<h3 id="r_novis">r_novis</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">
This variable toggles the use of VIS information from the map data. When this 
variable is set to &quot;1&quot; the game will calculate it's own VIS information on the 
fly instead of using the information stored in the map data, this will cause 
a severe performance penalty in the game. When &quot;r_novis&quot; is set to &quot;1&quot;, the 
variable &quot;r_wateralpha&quot; has a value lower than &quot;1&quot; and the server allows the
client to display water transparently the player will be able to see through 
liquids. This is a nice effect to see but it is not recommended to keep this 
variable set to &quot;1&quot; because of the huge performance penalty. 
It is possible to enable transparent water and still keep this variable set to
&quot;0&quot; (and thus not experience such a severe performance drop) by using maps that
are VISed accordingly. You can visit the official Water VIS site at 
&quot;http://www.sod.net/&quot; and download the vispatch program along with the patch 
data files which you would use to update the VIS data for all of your game maps.
There are also map packs available that contain VISed versions of the original
ID maps. 
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not calculate VIS information for unVISed maps.</td></tr><tr><td>1</td><td>Calculate VIS information for unVISed maps.</td></tr></tbody></table><p class="remarks">



</p>
<h3 id="r_skycolor">r_skycolor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"172"</span></p><p class="description">Changes colour of sky when r_fastsky set to 1
</p><p class="range">Range:  0-255 0-255 0-255</p><p class="remarks"></p>
<h3 id="r_skyname">r_skyname</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"bloody-marvelous512"</span></p><p class="description">

If you wan't to use skybox name snow type r_skyname snow, disable skybox with r_skyname &quot;&quot;

</p><p class="remarks"></p>
<h3 id="r_slimecolor">r_slimecolor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"53"</span></p><p class="description">Changes colour of slime when r_fastturb set to 1
</p><p class="range">Range:  0-255 0-255 0-255</p><p class="remarks"></p>
<h3 id="r_telecolor">r_telecolor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"26"</span></p><p class="description">Changes colour of teleport when r_fastturb set to 1
</p><p class="range">Range:  0-255 0-255 0-255</p><p class="remarks"></p>
<h3 id="r_wateralpha">r_wateralpha</h3>
<p class="support"><span class="windows-software unsupported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 unsupported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga unsupported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">

This variable determines the level of opacity for liquids, which requires
the use of maps that were VISed with transparent water. When using maps that
have not been VISed in such a way, you have to use &quot;r_novis 1&quot;. If &quot;r_novis&quot; is
set to &quot;1&quot; or when using a map VISed with transparent water and using setting
&quot;r_wateralpha&quot; to a value lower than &quot;1&quot;, the player will be able to see objects 
and walls through liquids if the server allows that. This is a very nice feature 
to have in the game, for more information refer to the &quot;r_novis&quot; command. You 
can use any value between &quot;0&quot; and &quot;1&quot; for &quot;r_wateralpha&quot;, here are some 
Examples:
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0.00</td><td>Transparent.</td></tr><tr><td>0.25</td><td>Low opacity.</td></tr><tr><td>0.50</td><td>Medium opacity.</td></tr><tr><td>0.75</td><td>High opacity.</td></tr><tr><td>1.00</td><td>Solid.</td></tr></tbody></table><p class="remarks">





</p>
<h3 id="r_watercolor">r_watercolor</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"36"</span></p><p class="description">Changes colour of lava when r_fastturb set to 1
</p><p class="range">Range:  0-255 0-255 0-255</p><p class="remarks"></p>
<h3 id="r_waterwarp">r_waterwarp</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl unsupported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx unsupported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Disable warping of surfaces.</td></tr><tr><td>1</td><td>Enable warping of surfaces.</td></tr></tbody></table><p class="remarks">



</p>
<h2 id="weapon+view+model+settings">Weapon View Model Settings</h2>

<h3 id="cl_bob">cl_bob</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.02"</span></p><p class="description">
This variable controls how much your weapon moves up and down when walking.

</p><p class="range">Range: 0..1</p><p class="remarks">
</p>
<h3 id="cl_bobcycle">cl_bobcycle</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.6"</span></p><p class="description">

This variable determines how quickly your weapon moves up and down when walking.

</p><p class="range">Range: 0..1</p><p class="remarks"></p>
<h3 id="cl_bobup">cl_bobup</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0.5"</span></p><p class="description">

This variable controls how long your weapon stays up before cycling when 
walking.

</p><p class="range">Range: 0..1</p><p class="remarks"></p>
<h3 id="cl_filterdrawviewmodel">cl_filterdrawviewmodel</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"0"</span></p><p class="description">

</p><p class="range">Range: 0/1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Off.</td></tr><tr><td>1</td><td>Prevents r_drawviewmodel from being changed by servers like Rocket Arena.</td></tr></tbody></table><p class="remarks">


</p>
<h3 id="r_drawviewmodel">r_drawviewmodel</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
</p><p class="range">Range: 0..1</p><table class="values"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody><tr><td>
0</td><td>Do not display the weapon model. </td></tr><tr><td>0.5</td><td>50% transparented.</td></tr><tr><td>1</td><td>Display the weapon model.</td></tr></tbody></table><p class="remarks">



Note: Transparency works in GL only.

</p>
<h3 id="r_viewmodelsize">r_viewmodelSize</h3>
<p class="support"><span class="windows-software supported">Windows: Software</span> <span class="windows-opengl supported">Windows: OpenGL</span> <span class="linux-x11 supported">Linux: X11</span> <span class="linux-glx supported">Linux: GLX</span> <span class="linux-svga supported">Linux: SVGA</span> </p><p class="default">Default: <span>"1"</span></p><p class="description">
This allows you to change the size of the viewmodel (the model of your active 
weapon displayed on the center of your screen) a little, useful if you prefer
to use &quot;r_drawviewmodel 1&quot; and higher fov values (for demos for example).
For &quot;fov 110&quot; you could try &quot;r_viewmodelscale 0.8&quot; or 0.85 for example.

</p><p class="range">Range: 0..1</p><p class="remarks">
</p>