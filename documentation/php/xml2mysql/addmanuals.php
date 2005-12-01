<?php

    include("../generator/configs/index.php");
    include("../mysql_access.php");
    include("../mysql_commands.php");
    include("../scripts/common-xmlparse.php");
    include("../scripts/common.php");
    include("../sections.php");
    
    $forms = new ManualsData;
    
    foreach ($sct as $sctname => $sctdata)
    {
        $data["content"] = file_get_contents("../docs/".$sctdata["inc"]);
        $data["name"] = $sctname;
        $data["title"] = $sctdata["title"];

        if (!$forms->AddOrUpdate($data, 1))
            echo "Manual page {$sctname} not added";
        else
            echo "Manual page {$sctname} has been added.";
    }

?>
