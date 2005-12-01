<?php

    include("../mysql_access.php");
    include("../mysql_commands.php");
    include("../scripts/common-xmlparse.php");
    include("../scripts/common.php");
    include("../groups.php");

    $mgroups_d = new VarMGroupsData;
    $groups_d = new VarGroupsData;

    my_mysql_query("DELETE FROM variables_groups WHERE 1;");
    my_mysql_query("DELETE FROM variables_mgroups WHERE 1;");
    my_mysql_query("DELETE FROM variables_groups_history WHERE 1;");
    my_mysql_query("DELETE FROM variables_mgroups_history WHERE 1;");

    foreach ($ubergr as $mgroup_sid => $groups_s)
    {
        $mgroup_title = $group[$mgroup_sid];
        echo $mgroup_sid."|";
        echo $mgroup_title;
        $mgroup_name = IdSafe($mgroup_title);
        $mgroup_id = $mgroups_d->Add(array("name" => $mgroup_name, "title" => $mgroup_name), 1);
        if (!$mgroup_id) break;
        
        foreach($groups_s as $group_name)
        {
            if (!$groups_d->Add(array("name" => IdSafe($group_name), "title" => $group_name, "id_mgroup" => $mgroup_id)))
                break;
        }
    }

?>
