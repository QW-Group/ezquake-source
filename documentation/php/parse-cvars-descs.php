<?php

class ParseCvarsDescs
{
  var $sPath;         // string: source XML file
	var $bArray;        // boolean: output is in array
	var $elements;      // array: output if bArray == True
	var $cmdall;        // array: collected data from XML (will be send to $elements)
	var $cmd;           // array: one field (record, row)
	var $vals;          // array: 
	var $valsval;       // array: one-dimensional, two rows... pair value, description
	var $chardata;      // string: for parser purposes, collects character data
	var $sDescFormat;   // string: "none" / "desc" / "value"
	var $line;          // xml header / footer
	
  function ParseCvarsDescs() 
	{
    $this->line = array();
    $this->line[1] = '<?xml version="1.0" encoding="US-ASCII"?>';
    $this->line[2] = '<?xml-stylesheet type="text/xsl" href="../xsl/variable.xsl"?>';
    $this->line[3] = '<variable xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="../xsd/variable.xsd">';
    $this->line[4] = '</variable>';
    
  	$this->cmdall = array();
    $this->cmd['name'] = "";
    $this->cmd['vals'] = array();
    $this->cmd['desc'] = array();
    $this->vals = array();
    $this->valsval = ""; 
    $this->chardata = "";
    $this->sDescFormat = "none";
  }

  function startElement($parser, $name, $attrs) 
  {
  	if($name=="item") { $this->sDescFormat = "none"; }
    if($name=="desc") { $this->sDescFormat = "desc"; }
    if($name=="value") { $this->sDescFormat = "value"; }
  }
  
  function endElement($parser, $name) 
  {
    switch ($name) {
      case 'name':
  		  $this->cmd['name'] = trim($this->chardata);
  			break;
  		case 'range':
  		  $this->cmd['range'] = $this->chardata;
  			break;
  		case 'val':
  		  $this->valsval = $this->chardata;
  			break;
  		case 'dsc':
  		  $this->vals[$this->valsval] = $this->chardata;
  			break;
  		case 'value':
  		  $this->sDescFormat = "remarks";
  			break;
  		case 'item':
  		  $this->cmd['vals'] = $this->vals;
  			$this->WriteCmd();
  			$this->cmd = array();
  			$this->vals = array();
  			break;
  	}
  	$this->chardata = "";
  }
  
  function characterData($parser, $data) 
  {
  	switch($this->sDescFormat) {
  	  case 'desc':
        $this->cmd['desc']['desc'] .= $data;
  			break;
  		case 'remarks':
  		  $this->cmd['desc']['remarks'] .= $data;
  			break;
  		default: 
  		  $this->chardata = $this->chardata.$data;
				break;
  	}
		$data = "";
  }
	
  function WriteCmd() 
  {
		if($this->bArray) {
		  $this->cmdall[$this->cmd['name']] = $this->cmd;
		} else {
      $fp = fopen($this->sPath.$this->cmd['name'].'.xml',"w");
    	for($i=1;$i<=3;$i++)
    	  fwrite($fp,$this->line[$i]."\n");
    	fwrite($fp,"\n<name>".$this->cmd['name']."</name>");
    	fwrite($fp,"\n<description>".htmlspecialchars($this->cmd['desc']['desc'])."</description>");
    	if(count($this->cmd['vals'])>0) {
      	fwrite($fp,"\n<value><enum>");
      	foreach($this->cmd['vals'] as $k => $v)
      	  fwrite($fp,"\n\t<value><name>".htmlspecialchars($k)."</name><description>".htmlspecialchars($v)."</description></value>");
      	fwrite($fp,"\n</enum></value>");
    	}
    	if($this->cmd['desc']['remarks']!="") fwrite($fp,"\n<remarks>".htmlspecialchars($this->cmd['desc']['remarks'])."\n</remarks>");
    	fwrite($fp,"\n".$this->line[$i]);
		}
  }
}

$Parser = new ParseCvarsDescs;
$Parser->bArray = True;

function startElement($arg1,$arg2,$arg3) {
  global $Parser;
	return $Parser->startElement($arg1,$arg2,$arg3);
}
function endElement($arg1,$arg2) {
  global $Parser;
	return $Parser->endElement($arg1,$arg2);
}
function characterData($arg1,$arg2) {
  global $Parser;
	return $Parser->characterData($arg1,$arg2);
} 
	
$xml_parser = xml_parser_create();
// use case-folding so we are sure to find the tag in $map_array
xml_parser_set_option($xml_parser, XML_OPTION_CASE_FOLDING, False);
xml_set_element_handler($xml_parser, "startElement", "endElement");
xml_set_character_data_handler($xml_parser, "characterData");
if (!($fp = fopen($sPath, "r"))) {
  die("could not open XML input");
}

while ($data = fread($fp, 4096)) {
  if (!xml_parse($xml_parser, $data, feof($fp))) {
    die(sprintf("XML error: %s at line %d",
      xml_error_string(xml_get_error_code($xml_parser)),
      xml_get_current_line_number($xml_parser)));
  }
}

xml_parser_free($xml_parser);

?>
