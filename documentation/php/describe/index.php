<?php

/**
 * Generator for XML based in-built game help system
 * Should be called with variable or command name as the first argument name
 * e.g. http://ezquake.sourceforge.net/docs/describe/?demo_capture
 */

	require_once("../inc/mysql_access.php");
	require_once("../inc/mysql_commands.php");
	require_once("../inc/common.php");
	require_once("desc_cmd.php");
	require_once("desc_var.php");

    $dbvar = new VariablesData;
    $dbcmd = new CommandsData;

    $name = each($_GET);
	$name = $name['key'];
	if ($name=="") { $name=$_SERVER["argv"][0]; }
    if (!$name) die;

    if ($id = $dbcmd->GetId($name))
    {
        echo GetXMLCmd($id, $dbcmd);
    }
    elseif ($id = $dbvar->GetId($name))
    {
        echo GetXMLVar($id, $dbvar);
    }

?>
