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
	
	include_once("data/groups.php");
	
	foreach($group as $grp) { 
	  $sct['vars-'.urlencode(strtolower($grp))]['title'] = $grp;
		$sct['vars-'.urlencode(strtolower($grp))]['inc'] = "vars/".urlencode(strtolower($grp)).".inc";
	}
	
?>
