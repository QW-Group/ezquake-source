<?php

function GetXMLVar($id, &$db)
/**
 * Generates XML content for game in-built help system
 * $id - ID no. of the variable in the database
 * $db - database interface
 */
{
    $ret = ""; // return buffer

    $var = $db->GetVar($id);
    if (!$var) return;
    
    $xmlclose = "?".">"; // those two char confuses my PHP editor :(
    
    $ret = "<?xml version=\"1.0\" encoding=\"UTF-8\"{$xmlclose}\n";
    $ret .= "<?xml-stylesheet type=\"text/xsl\" href=\"../xsl/variable.xsl\"{$xmlclose}\n";
    $ret .= "<variable xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"../xsd/variable.xsd\">\n";
    $ret .= "  <name>".Cvrt($var["name"])."</name>\n";
    if(strlen($var["description"]))
        $ret .= "  <description>".Cvrt($var["description"])."</description>\n";
    
    $ret .= "  <value>\n";
    switch($var["type"]) 
    {
        case 'string': $ret .= "    <string>".Cvrt($var["valdesc"])."</string>\n"; break;
        case 'integer': $ret .= "    <integer>".Cvrt($var["valdesc"])."</integer>\n"; break;
        case 'float': $ret .= "    <float>".Cvrt($var["valdesc"])."</float>\n"; break;
        case 'boolean':
            $ret .= "    <boolean>\n";
            $ret .= "      <true>".Cvrt($var["valdesc"]["true"])."</true>\n";
            $ret .= "      <false>".Cvrt($var["valdesc"]["false"])."</false>\n";
            $ret .= "    </boolean>\n";
            break;
        case 'enum':
            $ret .= "    <enum>\n";
            foreach ($var["valdesc"] as $argname => $argdesc)
            {
                $ret .= "    <value>\n";
                $ret .= "      <name>".Cvrt($argname)."</name>\n";
                $ret .= "      <description>".Cvrt($argdesc)."</description>\n";
                $ret .= "    </value>\n";
            }
        	$ret .= "    </enum>\n";
        	break;
    }
    $ret .= "  </value>\n";
    
    if(strlen($var["remarks"]))
        $ret .= "  <remarks>".Cvrt($var["remarks"])."</remarks>\n";
    
    $ret .= "</variable>\n";
    
    return $ret;

}

?>
