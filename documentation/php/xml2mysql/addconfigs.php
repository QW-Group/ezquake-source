<?php

    // redundant script .. doesn't do anything usefull for the future
    
    // reads all cofigs and moves them inside the MySQL database
    // this script is one-time purpose
    // shouldn't be neccessary to use it more than once
    
    include("../generator/configs/index.php");
    include("../mysql_access.php");
    include("../admin/inc/inc.php");
    
    // keys on the left side of these assignments are in configs/index.php ..
    // keys on the right side of these assignments are in MySQL db
    $ostypes["windows-opengl"] = array("os" => "Windows", "type" => "Open GL");
    $ostypes["windows-software"] = array("os" => "Windows", "type" => "Software");
    $ostypes["linux-x11"] = array("os" => "Linux", "type" => "X11");
    $ostypes["linux-svga"] = array("os" => "Linux", "type" => "SVGA");
    $ostypes["linux-glx"] = array("os" => "Linux", "type" => "GLX");
    
    $query = '
        SELECT build_release.id, build_os.name, build_type.name 
        FROM build_type, build_os, build_release 
        WHERE build_type.id = build_release.id_type && build_os.id = build_release.id_os;
    ';    
    
    $res = my_mysql_query($query);
    if(!res || !mysql_num_rows($res))
    {
        echo "Couldn't access the db.";
        return;
    }
    
    while ($r = mysql_fetch_row($res))
    {
        $oss[$r[1]][$r[2]] = $r[0];
    }
    
    foreach($cfgs['cmds'] as $k => $v)
    {
        $cont = addslashes(file_get_contents("../generator/".$v));
        $sql = "INSERT INTO commands_dumps (id_build, content) VALUES (".$oss[$ostypes[$k]["os"]][$ostypes[$k]["type"]].", '$cont');";
        my_mysql_query($sql);
    }

?>
