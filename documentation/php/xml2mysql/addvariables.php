<?php

    include("../generator/configs/index.php");
    include("../mysql_access.php");
    include("../mysql_commands.php");
    include("../scripts/common-xmlparse.php");
    include("../scripts/common.php");
    
    $vars = ScanDir("variables", "xml");
    $variables = new VariablesData;
    
    my_mysql_query ("DELETE FROM variables WHERE 1;");
    my_mysql_query ("DELETE FROM variables_values_boolean WHERE 1;");
    my_mysql_query ("DELETE FROM variables_values_enum WHERE 1;");
    my_mysql_query ("DELETE FROM variables_values_other WHERE 1;");
    my_mysql_query ("DELETE FROM variables_history WHERE 1;");
    
    foreach ($vars as $var)
    {
        $xml = XSLTransform($var, "../generator/xsl/varparse2.xsl");
        $fname = StripExtension(FileName($var));
        
        if (!xml)
        {
            echo $var.' not added<br />';
            continue;
        }

        $p = xml_parser_create();
        xml_parser_set_option($p, XML_OPTION_CASE_FOLDING, 0);
        xml_parse_into_struct($p, $xml, $vals, $index);
        xml_parser_free($p);
    
        $data = array();
        $data["name"] = trim($vals[$index["name"][0]]["value"]);
        if ($fname != $data["name"])
        {
            echo "<div>Name conflict in {$fname}. Skipping.</div>";
            continue;
        }
        
        $data["description"] = trim($vals[$index["description"][0]]["value"]);
        $data["remarks"] = trim($vals[$index["remarks"][0]]["value"]);
        $data["type"] = trim($vals[$index["type"][0]]["value"]);
        switch ($data["type"])
        {
            case "string":
            case "float":
            case "integer":
                $data["valdesc"] = trim($vals[$index["valdesc"][0]]["value"]);
            	break;
            case "enum":
                $args = array();
                if (isset($index["enumname"]))   // are there any arguments?
                {
                    foreach($index["enumname"] as $k => $v)
                        $args[trim($vals[$v]["value"])] = trim($vals[$index["enumdesc"][$k]]["value"]);
                }
                $data["valdesc"] = $args;
                break;
            case "boolean":
                $data["valdesc"]["true"] = trim($vals[$index["true"][0]]["value"]);
                $data["valdesc"]["false"] = trim($vals[$index["false"][0]]["value"]);
                break;
            default:
                $data["type"] = "float";
                break;
        }
        
        if (count($data["valdesc"]) == 2) 
        // some variables are boolean but their xml representation says it's enum
        {
            if (isset($data["valdesc"][0]) && isset($data["valdesc"][1]))
            {
                $data["type"] = "boolean";
                $data["valdesc"]["true"] = $data["valdesc"][1];
                $data["valdesc"]["false"] = $data["valdesc"][0];
                unset($data["valdesc"][1]);
                unset($data["valdesc"][0]);
            }
            elseif (isset($data["valdesc"]["false"]) && isset($data["valdesc"]["true"])) 
                    $data["type"] = "boolean";
        }
        
        if (!$variables->AddOrUpdate($data))
            echo "<div>Variable ".$data["name"]." hasn't been added!</div>";
        else 
            echo "<div>Variable ".$data["name"]." has been added.</div>";
    }
?>
