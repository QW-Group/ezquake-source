<?php
  define('MAXARGS', 9);
  $v_name = $_POST["v_name"];
  $v_desc = $_POST["v_desc"];
  $v_remarks = $_POST["v_remarks"];
  $v_type = $_POST["v_type"];
  
  if (!strlen($v_name)) {
    echo("Error - No name given.");
    die;
  }

  function Cvrt($string) {
    if (!get_magic_quotes_gpc()) {
      return trim(htmlspecialchars($string));
    } else {
      return trim(stripslashes(htmlspecialchars($string)));
    }
  }
  
  $v_fname = "variables/".$v_name.".xml";
  $f = fopen($v_fname, "w");
  $hdr = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  $hdr .= "<?xml-stylesheet type=\"text/xsl\" href=\"../xsl/variable.xsl\"?>\n";
  $hdr .= "<variable xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"../xsd/variable.xsd\">\n";
	fwrite($f, $hdr);
	fwrite($f, "  <name>".Cvrt($v_name)."</name>\n");
	if(strlen($v_desc)) {
    fwrite($f, "  <description>".Cvrt($v_desc)."</description>\n");
  }
  
  fwrite($f, "  <value>\n");
  switch($v_type) {
    case 'string':
      fwrite($f, "    <string>".Cvrt($_POST["v_string"])."</string>\n");
      break;
    case 'integer':
      fwrite($f, "    <integer>".Cvrt($_POST["v_integer"])."</integer>\n");
      break;
    case 'float':
      fwrite($f, "    <float>".Cvrt($_POST["v_float"])."</float>\n");
      break;
    case 'boolean':
      fwrite($f, "    <boolean>\n");
      fwrite($f, "      <true>".Cvrt($_POST["v_boolean_true"])."</true>\n");
      fwrite($f, "      <false>".Cvrt($_POST["v_boolean_false"])."</false>\n");
      fwrite($f, "    </boolean>\n");
      break;
    case 'enum':
      fwrite($f, "    <enum>\n");
    	if(strlen($v_arg_0_name))
    	{
    	  for($i = 0; $i <= MAXARGS; $i++) {
          if(strlen($_POST["v_arg_".$i."_name"])) {
            fwrite($f, "    <value>\n");
            fwrite($f, "      <name>".Cvrt($_POST["v_arg_".$i."_name"])."</name>\n");
            fwrite($f, "      <description>".Cvrt($_POST["v_arg_".$i."_desc"])."</description>\n");
            fwrite($f, "    </value>\n");
          }
        }
    	}    
    	fwrite($f, "    </enum>\n");
    	break;
  }
  fwrite($f, "  </value>\n");
  
	if(strlen($v_remarks)) {
    fwrite($f, "  <remarks>".Cvrt($v_remarks)."</remarks>\n");
  }
	fwrite($f, "</variable>\n");
	fclose($f);
	$mpath = substr($_SERVER["PHP_SELF"], 0, strrpos($_SERVER["PHP_SELF"], "/"));
  header("Location: http://$_SERVER[SERVER_NAME]$mpath/$v_fname", true, 303);
?>
