<?php

  /*
    Common functions
  */
  
  function HasExtension($f, $e, $ignorecase = True)   
  { // checks if given file has given file extension
    $f = substr($f, -strlen($e), strlen($e));
    if($ignorecase) {
      $f = strtolower($f);
      $e = strtolower($e);
    }
    return $f == $e;
  }
  
  function StripExtension($f)   
  { // cuts fileextension from given filename
    $i = strrpos($f, ".");
    return $i ? substr($f, 0, $i) : $f;
  }
  
  function GetExtension($f)   
  { // gets extension from filename (the part after last dot)
    $i = strrpos($f, ".");
    return $i ? substr($f, $i - strlen($f)) : "";
  }

  function IdSafe($s)
  { // makes string safe to use in url, id attribute and wherever
    return str_replace(array("+", "%26"), array("-", "and"), urlencode(strtolower(trim($s))));
  }

  function ScanDir($sdir, $ext = "", $recursive = False)  
  { // returns array of files inside given dir
    $a = array();
    $handle = opendir($sdir);
    while ($file = readdir($handle)) {
      if (($file!=".") && ($file!=".."))
        if (is_dir($sdir."/".$file)) {
          if ($recursive) $a += ScanDir($sdir."/".$file);
        } else {
          if ($ext == "" || HasExtension($file,$ext,False)) {
            $a[] = $sdir."/".$file;
          }
        }
    }
    closedir($handle);
    ksort($a);
    return $a;
  }

?>
