<?php

function GetXMLCmd($id, &$db)
/**
 * Generates XML content for game in-built help system
 * $id - ID no. of the command in the database
 * $db - database interface
 */
{
    $ret = "";

    $var = $db->GetCmd($id);
    if (!$var) return;
    
    $xmlclose = "?".">"; // those two char confuses my PHP editor :(

    $ret .= "<?xml version=\"1.0\" encoding=\"UTF-8\"{$xmlclose}\n";
    $ret .= "<?xml-stylesheet type=\"text/xsl\" href=\"../xsl/command.xsl\"{$xmlclose}\n";
    $ret .= "<command xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"../xsd/command.xsd\">\n";
	$ret .= "  <name>".Cvrt($var["name"])."</name>\n";
	if(strlen($var["description"]))
        $ret .= "  <description>".Cvrt($var["description"])."</description>\n";

    if(strlen($var["syntax"]))
        $ret .= "  <syntax>".Cvrt($var["syntax"])."</syntax>\n";

	if(isset($var["args"]))
	{
	  $ret .= "  <arguments>\n";
	  foreach ($var["args"] as $argname => $argdesc)
	  {
        $ret .= "    <argument>\n";
        $ret .= "      <name>".Cvrt($argname)."</name>\n";
        $ret .= "      <description>".Cvrt($argdesc)."</description>\n";
        $ret .= "    </argument>\n";
      }
      $ret .= "  </arguments>\n";
    }
	  
	if(strlen($var["remarks"]))
        $ret .= "  <remarks>".Cvrt($var["remarks"])."</remarks>\n";
        
    $ret .= "</command>\n";

    return $ret;
}

?>
