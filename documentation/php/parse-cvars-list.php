<?php

class ParseCvarsList
{
	var $elms;
  var $support;
	var $supportall;
	var $chardata;
	var $cvarname;

	function ParseCvarsList()
	{
    $this->chardata = "";
    $this->cvarname = "";
    $this->elms = array();
    $this->support = array();
    $this->supportall = array();
  }
	
  function startElement($parser, $name, $attrs) 
  {
    $this->chardata = "";
  }
  
  function endElement($parser, $name) 
  {
    if($name=="GROUP") { $this->elms[$this->chardata][$this->cvarname] = array(); }
  	if($name=="NAME") { $this->cvarname = $this->chardata; }
  	
  	if($name=="SOFTWARE") $this->support["windows-software"] = $this->chardata;
  	if($name=="OPENGL") $this->support["windows-opengl"] = $this->chardata;
  	if($name=="X11") $this->support["linux-x11"] = $this->chardata; 
  	if($name=="SVGA") $this->support["linux-svga"] = $this->chardata;
  	if($name=="GLX") $this->support["linux-glx"] = $this->chardata;
  	
  	if($name=="VARIABLE") {
  		$this->supportall[$this->cvarname] = $this->support;
  		$this->support = array();
  	}
  	$this->chardata = "";
  }
  
  function characterData($parser, $data) 
  {
   	$this->chardata .= $data;
  }
}

$Parser2 = new ParseCvarsList;
$Parser2->bArray = True;

function startElement2($arg1,$arg2,$arg3) {
  global $Parser2;
	return $Parser2->startElement($arg1,$arg2,$arg3);
}
function endElement2($arg1,$arg2) {
  global $Parser2;
	return $Parser2->endElement($arg1,$arg2);
}
function characterData2($arg1,$arg2) {
	global $Parser2;
	return $Parser2->characterData($arg1,$arg2);
}

$xml_parser2 = xml_parser_create();
// use case-folding so we are sure to find the tag in $map_array
xml_parser_set_option($xml_parser2, XML_OPTION_CASE_FOLDING, True);
xml_set_element_handler($xml_parser2, "startElement2", "endElement2");
xml_set_character_data_handler($xml_parser2, "characterData2");
if (!($fp = fopen($sPath2, "r"))) 
  die("could not open XML input");

while ($data = fread($fp, 4096)) {
    if (!xml_parse($xml_parser2, $data, feof($fp))) {
        die(sprintf("XML error: %s at line %d",
                    xml_error_string(xml_get_error_code($xml_parser2)),
                    xml_get_current_line_number($xml_parser2)));
    }
}

xml_parser_free($xml_parser2);

ksort($Parser2->elms, SORT_STRING);
reset($Parser2->elms);

if(!$Parser2->bArray) {
  echo("<h2>Index</h2><ul>");
  foreach($Parser2->elms as $a => $b) {
    echo("<li><a href=\"#".urlencode($a)."\">".htmlspecialchars($a)."</a></li>");  
  }
  echo("</ul>");
  
  foreach($Parser2->elms as $a =>$b) {
    echo("\n<h3 id=\"".urlencode($a)."\">".htmlspecialchars($a)."</h3><ul>");
  	asort($b);
  	while(list($r,$name) = each($b)) {
  		if(!is_null($_GET["env"])) {
        if(!is_null($Parser2->supportall[$name][$_GET["env"]])) 
  			  echo("<li>".htmlspecialchars($name)." (default: \"".htmlspecialchars($Parser2->supportall[$name][$_GET["env"]])."\")</li>");
  		} else {
  		  echo("<li>".htmlspecialchars($name)." (");
    		while(list($env,$Parser2->support) = each($Parser2->supportall[$name])) {
    		  echo("$env: [".htmlspecialchars($Parser2->$support)."] ");
    		}
  			echo(")</li>");
  		}
  	}
   	echo("</ul>\n");
  }
}


?>
