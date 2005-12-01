<?php

  /*
    Common files parsing methods
    Parse configs with variables and console logs with 'cmlist' output.
  */
  
  function ParseCvarsConfig($cfgname) 
  {
    $vars = array();
    $handle = fopen($cfgname, "r");
    while (!feof($handle)) {
      $buffer = fgets($handle);
      list($v1) = sscanf($buffer, "%s"); // FIXME! need to read rest of the line into the 2nd var
      $v2 = trim(substr($buffer, strlen($v1)));
      if ($buffer[0] == "/") {
        if ($buffer[2] != "//") $group = substr($buffer, 2);
      } elseif (strlen(trim($buffer))) $vars[trim($group)][trim($v1)] = trim($v2, " \"\t");
    }
    return $vars;
  }
  
  function ParseCmdlistLog($logname)
  {
    $cmds = array();
    $handle = fopen($logname, "r");
    while (!feof($handle)) {
      $buffer = fgets($handle);
      if(strpos($buffer, "List of commands") !== false) continue;
      if(strpos($buffer, "----------") !== false) break;
      $cmds[] = trim($buffer);
    }
    return $cmds;
  }

?>
