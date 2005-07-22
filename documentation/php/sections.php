<?php

  $sct['graphics']['title'] = "Graphic settings";
	$sct['graphics']['inc'] = "Graphics.html";
	
	$sct['keymap']['title'] = "Keymap support";
	$sct['keymap']['inc'] = "KeyMap.html";
	
	$sct['server-browser']['title'] = "Server Browser settings";
	$sct['server-browser']['inc'] = "ServerBrowser.html";
	
	$sct['credits']['title'] = "Credits";
	$sct['credits']['inc'] = "Credits.html";
	
	$sct['changelog']['title'] = "Changelog / Feature list";
	$sct['changelog']['inc'] = "Changelog.html";
	
	$sct['gnu-gpl']['title'] = "GNU General Public License";
  $sct['gnu-gpl']['inc'] = "GNUGPL.html";
	
	$sct['main-page']['title'] = "Main page";
	$sct['main-page']['inc'] = "MainPage.html";
	
	$sct['commands']['title'] = "Available commands";
	$sct['commands']['inc'] = "Commands.html";
	
	$sct['fuhq-changes']['title'] = "New commands since FuhQuake";
	$sct['fuhq-changes']['inc'] = "FuhquakeChanges.html";
	
	$sct['install']['title'] = "Installation";
	$sct['install']['inc'] = "Install.html";
	
	$sct['fpd']['title'] = "FPD Calculator";
	$sct['fpd']['inc'] = "FPD.html";
	
	$sct['hud']['title'] = "Head Up Display - HUD";
	$sct['hud']['inc'] = "HUD.html";
	
	$sct['rtc']['title'] = "RTC timer patch for Linux FuhQuake 0.31";
	$sct['rtc']['inc'] = "rtc.html";
	
	$sct['evdev']['title'] = "EVDEV support";
	$sct['evdev']['inc'] = "evdev.html";
	
	$sct['command-line']['title'] = "Command-Line Options";
	$sct['command-line']['inc'] = "Commandline.html";

	$sct['multiview']['title'] = "Multiview";
	$sct['multiview']['inc'] = "Multiview.html";

	$sct['independent-physics']['title'] = "Independent physics";
	$sct['independent-physics']['inc'] = "IndependentPhysics.html";
	
	$sct['scripting']['title'] = "Scripting";
	$sct['scripting']['inc'] = "Scripting.html";
	
	$sct['video-capture']['title'] = "Video stream capture";
	$sct['video-capture']['inc'] = "VideoCapture.html";
	
	$sct['triggers']['title'] = "Triggers";
	$sct['triggers']['inc'] = "Triggers.html";

  $sct['qc']['title'] = "QC Extensions";
  $sct['qc']['inc'] = "qc.html";
  
  $sct['linux']['title'] = "Linux Binaries";
  $sct['linux']['inc'] = "linux.html";
  
  $sct['crosshairs']['title'] = "Crosshairs";
  $sct['crosshairs']['inc'] = "crosshairs.html";
  
  $sct['config']['title'] = "Configuration manager";
  $sct['config']['inc'] = "config.html";
  
  $sct['logitech']['title'] = "Logitech Mice";
  $sct['logitech']['inc'] = "logitech.html";
  
  $sct['match-tools']['title'] = "Match tools";
  $sct['match-tools']['inc'] = "match_tools.html";
  
  $sct['mp3-player']['title'] = "MP3 Player control";
  $sct['mp3-player']['inc'] = "mp3_player.html";
  
  $sct['particles']['title'] = "Particle effects";
  $sct['particles']['inc'] = "particles.html";
  
  $sct['tp-pointing']['title'] = "Teamplay pointing";
  $sct['tp-pointing']['inc'] = "tp_pointing.html";
  
  $sct['tracking']['title'] = "Spectator Tracking";
  $sct['tracking']['inc'] = "tracking.html";
    
  include_once("groups.php");
	
	foreach($group as $grp) { 
	  $sct['vars-'.urlencode(strtolower($grp))]['title'] = $grp;
		$sct['vars-'.urlencode(strtolower($grp))]['inc'] = "vars/".urlencode(strtolower($grp)).".inc";
	}
	
?>
