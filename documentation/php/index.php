<?php
  $sekce = each($_GET);
	$sekce = $sekce['key'];
	if ($sekce=="") { $sekce=$_SERVER["argv"][0]; }
	$maxfilectime = 0;
	include("sections.php");
	if ($sct[$sekce]['title'] == "") $sekce = "main-page"; 
	
	if(stristr($_SERVER["HTTP_ACCEPT"],"application/xhtml+xml"))	{ 
  	header("Content-Type: application/xhtml+xml; charset=utf-8"); 
    $cthdr = "<meta http-equiv=\"Content-Type\" content=\"application/xhtml+xml; charset=utf-8\" />";
    echo('<?xml version="1.0" encoding="utf-8"?>');
  	echo('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" 
  	"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">'); 
  } else { 
  	header("Content-Type: text/html; charset=windows-1250"); 
    $cthdr = "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />";
  	echo ('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" 
  	"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">'); 
  }
	
?>

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
  <?=$cthdr?>
  <meta name="keywords" content="ezQuake, manual, guide, setting, quake, quakeworld, client, help, readme, install" />
  <meta name="description" content="The Complete guide to using QuakeWorld &trade; client ezQuake" />
  <title>ezQuake Manual: <?=$sct[$sekce]['title']?></title>
	<link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
<h1><a href=".">ezQuake Manual</a>: <?=$sct[$sekce]['title']?></h1>
<?php
  include("docs/".$sct[$sekce]['inc']);
?>

<p id="last-update">Last update: 
<?php
  if($maxfilectime == 0) { 
    echo(date("d.m.Y H:i",filemtime("docs/".$sct[$sekce]['inc'])));
	} else {
	  echo(date("d.m.Y H:i",$maxfilectime));
	} 
?> CET</p>
</body>
</html>
