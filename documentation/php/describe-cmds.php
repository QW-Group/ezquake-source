<?php

$line[1] = '<?xml version="1.0" encoding="UTF-8"?>';
$line[2] = '<?xml-stylesheet type="text/xsl" href="../xsl/command.xsl"?>';
$line[3] = '<command xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="../xsd/command.xsd">';
$line[4] = '</command>';

$file = "../data/cmds.xml";
$cmd['name'] = "";
$cmd['args'] = "";
$cmd['desc'] = "";
$cmd['rmks'] = "";
$chardata = "";

function startElement($parser, $name, $attrs) 
{
  echo("");
}

function endElement($parser, $name) 
{
  global $cmd;
	global $chardata;

  switch ($name) {
	  case 'p':
		  $cmd['desc'] = $chardata;
			break;
		case 'name':
		  $cmd['name'] = trim($chardata);
			break;
		case 'arguments':
		  $cmd['args'] = $chardata;
			break;
		case 'remarks':
		  $cmd['rmks'] = $chardata;
			break;
	  case 'cmd':
		  WriteCmd($cmd);
			$cmd = array();
			break;
	}
	$chardata = "";
}

function WriteCmd($cmd) 
{
  global $line;
  $fp = fopen("../data/commands/".$cmd['name'].".xml","w");
	for($i=1;$i<=3;$i++)
	  fwrite($fp,$line[$i]."\n");
	fwrite($fp,"<name>".$cmd['name']."</name>");
	fwrite($fp,"<description>".$cmd['desc']."</description>");
	fwrite($fp,"<syntax>".$cmd['args']."</syntax>");
	fwrite($fp,"<remarks>".$cmd['rmks']."</remarks>");
	fwrite($fp,$line[$i]);
}

function characterData($parser, $data) 
{
  global $chardata;
	$chardata .= $data;
}

$xml_parser = xml_parser_create();
// use case-folding so we are sure to find the tag in $map_array
xml_parser_set_option($xml_parser, XML_OPTION_CASE_FOLDING, False);
xml_set_element_handler($xml_parser, "startElement", "endElement");
xml_set_character_data_handler($xml_parser, "characterData");
if (!($fp = fopen($file, "r"))) {
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
