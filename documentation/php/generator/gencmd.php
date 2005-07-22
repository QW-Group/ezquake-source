<?php
  define('MAXARGS', 9);
  $c_name = $_POST["c_name"];
  $c_desc = $_POST["c_desc"];
  $c_syntax = $_POST["c_syntax"];
  $c_remarks = $_POST["c_remarks"];
  if (!strlen($c_name)) {
    echo("Error - No name given.");
    die;
  }
  
  function Cvrt($string) {
    if (!get_magic_quotes_gpc()) {
      return htmlspecialchars($string);
    } else {
      return stripslashes(htmlspecialchars($string));
    }
  }
  
  $c_fname = "commands/".$c_name.".xml";
  $f = fopen($c_fname, "w");
  $hdr = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  $hdr .= "<?xml-stylesheet type=\"text/xsl\" href=\"../xsl/command.xsl\"?>\n";
  $hdr .= "<command xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"../xsd/command.xsd\">\n";
	fwrite($f, $hdr);
	fwrite($f, "  <name>".Cvrt($c_name)."</name>\n");
	if(strlen($c_desc)) {
    fwrite($f, "  <description>".Cvrt($c_desc)."</description>\n");
  }
  if(strlen($c_syntax)) {
    fwrite($f, "  <syntax>".Cvrt($c_syntax)."</syntax>\n");
  }
	if(strlen($c_arg_0_name))
	{
	  fwrite($f, "  <arguments>\n");
	  for($i = 0; $i <= MAXARGS; $i++) {
      if(strlen($_POST["c_arg_".$i."_name"])) {
        fwrite($f, "    <argument>\n");
        fwrite($f, "      <name>".Cvrt($_POST["c_arg_".$i."_name"])."</name>\n");
        fwrite($f, "      <description>".Cvrt($_POST["c_arg_".$i."_desc"])."</description>\n");
        fwrite($f, "    </argument>\n");
      }
    }
	  fwrite($f, "  </arguments>\n");
	}
	if(strlen($c_remarks)) {
    fwrite($f, "  <remarks>".Cvrt($c_remarks)."</remarks>\n");
  }
	fwrite($f, "</command>\n");
	fclose($f);
	$mpath = substr($_SERVER["PHP_SELF"], 0, strrpos($_SERVER["PHP_SELF"], "/"));
  header("Location: http://$_SERVER[SERVER_NAME]$mpath/$c_fname", true, 303);
?>
