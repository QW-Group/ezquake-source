<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
 "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">

<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title></title>
</head>
<body>
<ol>
<?php

  set_time_limit(60);

  /**
	  script vyrobi mnozstvi HTML souboru	  
    jeden soubor = grupa (group, e.g. 'Input - mouse')
		(more groups = ubergroup (e.g. 'Graphics')
	*/

	// $Parser
	$sPath = "../data/varsdesc.xml";
	include("parse-cvars-descs.php");
	
	// $Parser2
	$sPath2 = "../data/cvarlist.xml";
	include("parse-cvars-list.php");

	include("../data/groups.php");
	 
/**	
	foreach($Parser->cmdall as $k => $v) {
	  echo("<li>".$k."--".$v."<ol>");
		foreach($v as $k2 => $v2) {
		  echo("<li>".$k2."--".$v2."</li>");
		}
		echo("</ol></li>");
	}
*/

  $AllVars = Array();

	foreach($Parser2->elms as $k => $v) {
	  echo("<li>".$k."--".$v."<ol>");
		foreach($v as $k2 => $v2) {
		  echo("<li>".$k2."</li>");
			// add descriptions, remarks, value+description pairs
			$AllVars[$k][$k2]['desc'] = $Parser->cmdall[$k2];
			$AllVars[$k][$k2]['supp'] = $Parser2->supportall[$k2];
		}
		echo("</ol></li>");
	}
  
	$Parser2 = Null;
	$Parser = Null;
	
	// Parsing all uber-groups
	foreach($group as $uberGroupIndex => $uberGroupName) {
	  $fp = fopen("../docs/vars/".strtolower($uberGroupName).".inc","w");
		fwrite($fp,"\n<ul>\n");

		// Parsing groups (Quickly = menu)
		foreach($ubergr[$uberGroupIndex] as $groupName) {
		  fwrite($fp, "<li><a href=\"#".urlencode(strtolower($groupName))."\">".htmlspecialchars($groupName)."</a></li>\n");
		}
		fwrite($fp,"</ul>\n");
				
		// Parsing groups (Deeply)
		foreach($ubergr[$uberGroupIndex] as $groupName) {
		  fwrite($fp, "\n<h2 id=\"".urlencode(strtolower($groupName))."\">".htmlspecialchars($groupName)."</h2>\n");
					
  		// Parsing variables
			foreach($AllVars[$groupName] as $varName => $varData) {
        fwrite($fp, "\n<h3 id=\"".urlencode(strtolower($varName))."\">".htmlspecialchars($varName)."</h3>\n");
				fwrite($fp, "<p class=\"support\">");
				foreach($suppList as $envAbbr => $envTitle) {
				  // SUPPORT: Yes / No
				  if(!is_null($varData['supp'][$envAbbr])) {
					  fwrite($fp, "<span class=\"$envAbbr supported\">$envTitle</span> ");
					  $varDef = $varData['supp'][$envAbbr];
				  } else {
					  fwrite($fp, "<span class=\"$envAbbr unsupported\">$envTitle</span> ");
						// GET Default value
					}
				}
				fwrite($fp, "</p>");
				// Default value, description, range
				fwrite($fp, "<p class=\"default\">Default: <span>\"".htmlspecialchars($varDef)."\"</span></p>");
				fwrite($fp, "<p class=\"description\">".htmlspecialchars($varData['desc']['desc']['desc'])."</p>");
				if(!is_null($varData['desc']['range']) || !($varData['desc']['range']=="")) {
  				fwrite($fp, "<p class=\"range\">Range: <span>".htmlspecialchars($varData['desc']['range'])."</span></p>");
			  }
				
				// Values: Value+Description pairs (if we have these data)
				if(is_array($varData['desc']['vals'])) {
  				$bAny = False;
  				foreach($varData['desc']['vals'] as $varValVal => $varValDesc) {
  				  if (!$bAny) { fwrite($fp, "<table class=\"values\"><thead><tr><td>Val</td><td>Description</td></tr></thead><tbody>"); $bAny = True; }
  				  fwrite($fp, "<tr><td>".htmlspecialchars($varValVal)."</td>");
  					fwrite($fp, "<td>".htmlspecialchars($varValDesc)."</td></tr>");
  				}
  				if ($bAny) { fwrite($fp, "</tbody></table>"); }
        }				
				// Remarks
				fwrite($fp, "<p class=\"remarks\">".htmlspecialchars($varData['desc']['desc']['remarks'])."</p>");
			}
		}
		fclose($fp);
	}
	
  /**
	  co potrebujeme:
		  name,  - data/cvarlist.xml    p1+2
			group, - data/cvarlist.xml     p2
			default, - data/cvarlist.xml   p2
			support, - data/cvarlist.xml   p2
			desc - data/varsdesc.xml      p1
			  - range
        - description
			  - value + description pair
				- remarks
	*/

?>
</ol>
</body>
</html>
