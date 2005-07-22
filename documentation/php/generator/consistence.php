<?php 

  /*
    DOCUMENTATION CONSISTENCY CHECKER
    Compares list of variables and commands with list of documented vars & cmds.
    
    List of variables is taken from configs.
    List of commands is taken from consolelogs with output of 'cmdlist' command.
    
    List of documented vars & cmds is taken from 'variables/' and 'commands/'
    subdirs as list of .xml files present there.
  */

  $liclass = array(0 => "error", 1 => "undoc", 2 => "notex", 3 => "exists");
      
  require("../scripts/common.php");
  require("../scripts/common-txtparse.php");
  require("configs/index.php");

  echo('<?xml version="1.0" encoding="utf-8"?>');
  echo("\n");
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" 
        "http://www.w3.org/TR/2000/REC-xhtml1-20000126/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
 <head>
  <meta http-equiv="content-type" content="text/html; charset=utf-8" />
  <title>ezQuake documentation consitency checker</title>
  <style type="text/css"><!--
  
  body { background-color: white; }
  li { background-color: #aaa; }
  li.exists { background-color: white; }
  li.undoc { background-color: #f88; }
  li.error { background-color: black; }
  li.notex { background-color: yellow; }
  
  --></style>
 </head>
 <body>

  <h1>ezQuake: documentation consistency checker</h1>
  <h2>Guide:</h2>
  <ul>
    <li class="exists">variable present and documented OK</li>
    <li class="notex">variable not exists</li>
    <li class="undoc">variable is not documented yet</li>
  </ul>
  
  <h2>Options:</h2>
  <ul>
    <li><a href="?show=0">Show only conflicts</a></li>
    <li><a href="?show=1">Show all variables</a></li>
    <li><a href="#summary">Go to summarry</a></li>
  </ul>

  <h2>List:</h2>

<?php

  $show = $_GET["show"];
  if($show != 1 && $show != 0) $show = 0;
  $summary = array(); $vars = array(); $cmds = array();

  foreach($cfgs['vars'] as $config)  // for all configs - openGL, software, glx, x11, svga, ...
    foreach(ParseCvarsConfig($config) as $groupofvars)  // for all variable groups
      foreach($groupofvars as $varname => $vardefault) $vars[$varname] |= 1;   // for all variables
  
  foreach(ScanDir("variables", ".xml") as $v) $vars[StripExtension(basename($v))] |= 2;
  ksort($vars);
  
  foreach($cfgs['cmds'] as $logfile)  // for all cmdlist logfiles
    foreach(ParseCmdlistLog($logfile) as $cmd) $cmds[$cmd] |= 1;  // for all commands
  
  foreach(ScanDir("commands", ".xml") as $c) $cmds[StripExtension(basename($c))] |= 2;
  ksort($cmds);
  
  echo("  <h3>Variables</h3>\n    <ul>\n");
  foreach($vars as $varname => $varstate) {
    $summary[$varstate]++;
    if($show || $varstate != 3)
      echo("      <li class=\"".$liclass[$varstate]."\">".$varname."</li>\n"); 
  }
  echo("    </ul>\n  <h3>Commands</h3>\n    <ul>\n");
  foreach($cmds as $cmdname => $cmdstate) {
    $summary[$cmdstate]++;
    if($show || $cmdstate != 3)
      echo("      <li class=\"".$liclass[$cmdstate]."\">".$cmdname."</li>\n"); 
  }
  echo("    </ul>\n");
  
?>

  <h2 id="summary">Summary:</h2>
    <dl>
      <dt>Undocumented:</dt><dd><?=$summary[1]?></dd>
      <dt>Not existing:</dt><dd><?=$summary[2]?></dd>
      <dt>Valid:</dt><dd><?=$summary[3]?></dd>
      <dt>Total:</dt><dd><?=count($cmds)+count($vars)?></dd>
    </dl>

</body>
</html>
