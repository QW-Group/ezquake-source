<?php

  /*
    Common functions
  */
  
define ("ALPHABET", "abcdefghijklmnopqrstuvwxyz0123456789");

$foundOrCreated = 0;

function RefreshPage($page)
{
    header("HTTP/1.1 303 Moved Permanently");
    header("Location: index.php");
    header("Connection: $page");
    echo("<a href=\".\">Follow this link to continue</a>");
    die;
}

function RandomString($len)
{
    $len = (int) $len;
    $s = "";
    $alphabetlen = strlen(ALPHABET);
    
    if ($len < 1) return "";

    for ($i = 1; $i <= $len; $i++)
        $s .= substr(ALPHABET, rand(1, $alphabetlen) - 1, 1);

    return $s;
}

function GetOrCreate($table, $fields, $num = False)
{ // finds a row in a table $table or creates it if it doesn't exist and returns it's ID
    global $foundOrCreated;
    $condition = "";
    $q = $num ? "" : "'";
    foreach($fields as $k => $v) {
        if(strlen($condition)) $condition .= " && ";
        $condition .= "(".addslashes($k)." = $q".addslashes($v)."$q)";
    }

    $r = my_mysql_query("SELECT id FROM $table WHERE $condition LIMIT 1;");
    if (mysql_num_rows($r)) {
        $foundOrCreated = 0;
        return mysql_result($r, 0);
    } else {
        $foundOrCreated = 1;
        $fieldnames = ""; $fieldvalues = "";
        foreach($fields as $k => $v) 
        {
            if(strlen($fieldnames)) $fieldnames .= ", ";
            $fieldnames .= $k;
            if(strlen($fieldvalues)) $fieldvalues .= ", ";
            $fieldvalues .= $q.$v.$q;
        }
        my_mysql_query("INSERT INTO $table ($fieldnames) VALUES ($fieldvalues);");
        return mysql_insert_id();
    }
}

function FileName($path)
{
    $r = strrpos($path, "/");
    if (is_bool($r) && !$r)
        return $path;
        
    return substr($path, strrpos($path, "/") + 1);
}

  
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

function IsIdSafe($s)
{
    return $s == IdSafe($s);
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
    asort($a);
    return $a;
  }
  
function Cvrt($string) {
    if (!get_magic_quotes_gpc()) {
        return htmlspecialchars($string);
    } else {
        return stripslashes(htmlspecialchars($string));
    }
}

?>
