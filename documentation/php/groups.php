<?php

$group = array( 
  0 => 'Miscellaneous', 
  1 => 'Input', 
  2 => 'Teamplay', 
  3 => 'Graphics', 
  4 => 'Multiplayer', 
  8 => 'Demos', 
  5 => 'Server', 
  6 => 'Sound', 
  7 => 'Hud'
);

$grp2ugrp = array(
  '#No Group#' => 0,
  'Unsorted Variables' => 0,
  'Chat Settings' => 7,
  'Config Management' => 0,
  'Console Settings' => 7,
  'Crosshair Settings' => 3,
  'Demo Handling' => 8,
  'FPS and EyeCandy Settings' => 3,
  'Input - Joystick' => 1,
  'Input - Keyboard' => 1,
  'Input - Misc' => 1,
  'Input - Mouse' => 1,
  'Item Names' => 2,
  'Item Need Amounts' => 2,
  'Lighting' => 3,
  'MP3 Settings' => 6,
  'Match Tools' => 8,
  'MQWCL HUD' => 7,
  'Network Settings' => 4,
  'OpenGL Rendering' => 3,
  'Particle Effects' => 3,
  'Player Settings' => 4,
  'Screen & Powerup Blends' => 3,
  'Screen Settings' => 3,
  'Screenshot Settings' => 3,
  'Server Browser' => 4,
  'Server Settings' => 5,
  'Serverinfo Keys' => 5,
  'Serverside Permissions' => 5,
  'Serverside Physics' => 5,
  'Skin Settings' => 4,
  'Software Rendering' => 3,
  'Sound Settings' => 6,
  'Spectator Tracking' => 4,
  'Status Bar and Scoreboard' => 7,
  'System Settings' => 0,
  'Teamplay Communications' => 2,
  'Texture Settings' => 3,
  'Video Settings' => 3,
  'View Settings' => 3,
  'Turbulency and Sky Settings' => 3,
  'Weapon View Model Settings' => 3
);

foreach($grp2ugrp as $grp => $ugrp) $ubergr[$ugrp][] = $grp;


$suppList = array(
  "windows-software" => "Windows: Software",
  "windows-opengl" => "Windows: OpenGL",
  "linux-x11" => "Linux: X11",
  "linux-glx" => "Linux: GLX",
  "linux-svga" => "Linux: SVGA"
);

?>
