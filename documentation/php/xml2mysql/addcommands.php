<?php

    include("../generator/configs/index.php");
    include("../mysql_access.php");
    include("../mysql_commands.php");
    include("../scripts/common-xmlparse.php");
    include("../scripts/common.php");
    
    $cmds = ScanDir("commands", "xml");
    $commands = new CommandsData;
    
    foreach ($cmds as $cmd)
    {
        $xml = XSLTransform($cmd, "../generator/xsl/cmdparse.xsl");
        
        if ($xml) 
        {
            $p = xml_parser_create();
            xml_parser_set_option($p, XML_OPTION_CASE_FOLDING, 0);
            xml_parse_into_struct($p, $xml, $vals, $index);
            xml_parser_free($p);
        
            $data = array();
            $data["name"] = trim($vals[$index["name"][0]]["value"]);
            $data["description"] = trim($vals[$index["description"][0]]["value"]);
            $data["remarks"] = trim($vals[$index["remarks"][0]]["value"]);
            $data["syntax"] = trim($vals[$index["syntax"][0]]["value"]);

            $data["args"] = array();
            
            if (isset($index["arg"]))   // are there any arguments?
            {
                foreach($index["arg"] as $k => $v)
                    $data["$args"][$vals[$v]["value"]] = $vals[$index["argdesc"][$k]]["value"];
            }
            
            $commands->AddOrUpdate($data, 1);
//            echo $r ? "$cmd inserted<br />" : "$cmd not inserted<br />";
        }
        else
        {
            echo $cmd.' not added<br />';
        }

    }

?>
