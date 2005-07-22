<?php

  /**
      This script generates HTML Manual describing all variables.
      ---
      First it gets list of all variables, their groups, support (in what os/build
      do they exist) and default values.
      Then it generates .inc files, each file equals one group of more variable
      groups. Code uses many functions and data (variables) from external
      scripts.
  */

  require("../scripts/common.php");
  require("../scripts/common-txtparse.php");
  require("../scripts/common-xmlparse.php");
  require("configs/index.php");
  require("../groups.php");

  function Support($cfgkeys) {
    global $suppList;
    $r = '';
    $allsupport = isset($cfgkeys['universal']);
    foreach($suppList as $suppKey => $suppName) {
      $sp_class = $suppKey . " " . ((isset($cfgkeys[$suppKey]) || $allsupport) ? "supported" : "unsupported");
      $r .= "<span class=\"$sp_class\">$suppName</span> ";
    }
    return $r;
  }

  function Defaults($cfgkeys) {
    global $suppList;
    $s = $suppList + array('universal' => 'Universal');
    $r = '';
    foreach($s as $suppKey => $suppName)
      if(isset($cfgkeys[$suppKey])) {
        if($r == '')
          $r = $cfgkeys[$suppKey];  // we return first default we find
        elseif ($cfgkeys[$suppKey] != $r)
          $r .= " or ".$cfgkeys[$suppKey];
      }
    return $r;
  }

  $aVrs = array();
  
  foreach($cfgs['vars'] as $cfgkey => $config)  // for all configs - openGL, software, glx, x11, svga, ...
    foreach(ParseCvarsConfig($config) as $group_name => $vars)  // for all variable groups
      foreach($vars as $var_name => $var_default)
        $aVrs[$grp2ugrp[$group_name]][$group_name][$var_name][$cfgkey] = $var_default;
  
  foreach($aVrs as $ugrp => $group_vars) {
    echo("<p>Writing ".$group[$ugrp].".inc ...</p>"); flush();
    ksort($group_vars); reset($group_vars);
    $f = fopen("html_manual/".strtolower($group[$ugrp]).".inc", "w");
    fwrite($f, "  <ul>\n");
    foreach($group_vars as $group_name => $vars)
      fwrite($f, "    <li><a href=\"#".IdSafe($group_name)."\">".htmlspecialchars($group_name)."</a></li>\n");
    fwrite($f, "  </ul>\n");
    
    foreach($group_vars as $group_name => $vars) {
      fwrite($f, "<h2 id=\"".IdSafe($group_name)."\">".htmlspecialchars($group_name)."</h2>\n");
      ksort($vars); reset($vars);
      foreach($vars as $var_name => $cfgkeys) {
        fwrite($f, "  <h3 id=\"".IdSafe($var_name)."\">$var_name&nbsp;<span>[<a href=\"#".IdSafe($var_name)."\">#</a>]</span></h3>\n");
        fwrite($f, "  <p class=\"support\">Support: ".Support($cfgkeys)."</p>\n");
        fwrite($f, "  <p class=\"default\">Default: <span>".htmlspecialchars(Defaults($cfgkeys))."</span></p>\n");
        $xmldesc = "variables/$var_name.xml";
        fwrite($f, VariableXML2Div($xmldesc)."\n");
      }
    }
    
    fclose($f);
  }

?>
