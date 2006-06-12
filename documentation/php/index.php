<?php

	require_once("inc/mysql_access.php");
	require_once("inc/mysql_commands.php");
	require_once("inc/common.php");
	require_once("inc/renderer.php");
	require_once("settings.php");

    $db = array();
    $db["manuals"] = new ManualsData;
    $db["variables"] = new VariablesData;
    $db["groups"] = new GroupsData;
    $db["mgroups"] = new MGroupsData;
    $db["support"] = new SupportData;
    $db["commands"] = new CommandsData;
    $db["options"] = new OptionsData;
    $db["index"] = new IndexData;
    $db["search"] = new SearchHits;

    $sekce = each($_GET);
	$sekce = $sekce['key'];
	if ($sekce=="") { $sekce=$_SERVER["argv"][0]; }
	$maxfilectime = 0;
    if (!$sekce) $sekce = "main-page";

	$renderer = GetRenderer($sekce, $db);
	
	if(stristr($_SERVER["HTTP_ACCEPT"],"application/xhtml+xml"))	
    { 
      	header("Content-Type: application/xhtml+xml; charset=utf-8"); 
        $cthdr = "<meta http-equiv=\"Content-Type\" content=\"application/xhtml+xml; charset=utf-8\" />";
        echo('<?xml version="1.0" encoding="utf-8"?'.'>'."\n");
      	echo('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" 
      	"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">'); 
    }
    else
    { 
      	header("Content-Type: text/html; charset=utf-8"); 
        $cthdr = "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />";
      	echo ('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" 
      	"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">'); 
    }
?>

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
  <?=$cthdr?>
  <meta name="keywords" content="<?=META_KEYWORDS?>" />
  <meta name="description" content="<?=META_DESCRIPTION?>" />
  <meta name="author" content="http://sourceforge.net/users/johnnycz/" />
  <title><?=PROJECTNAME?> Manual: <?php echo(htmlspecialchars($renderer->title)); ?></title>
  <link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
<h1><a href="./"><?=PROJECTNAME?> Manual</a>: <?php echo(htmlspecialchars($renderer->heading)); ?></h1>


<?php $renderer->RenderContent(); ?>


<p id="last-update">Last update: <?=date("d.m.Y H:i T",$renderer->lastupdate)?>, made by <a href="http://sourceforge.net/users/johnnycz/">JohnNy_cz</a></p>
</body>
</html>
