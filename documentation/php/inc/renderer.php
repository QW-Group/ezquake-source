<?php

/**
 * Functions and classes for HTML manual rendering.
 * Uses mysql data mining classes from mysql_commands.php
 */
 
 // to-do - all the classes are mad, no need for them
 //  - too much of 'new' constructs
 
define (VARGROUPSPREFIX, "vars-");
define (VARGROUPSPREFIXLEN, 5);
define (DEFAULTPAGE, "main-page");

function GetRenderer($name, &$db)
/* returns an initialized object which can be accessed for
   page title, heading and content */
{
    switch ($name)
    {
        case "commands": return new CommandsAllRendData(2); return; break;
        case "command-line": return new OptionsRendData(); return; break;
        case "main-page": return new MainPageRendData($db); return; break;
    }
    
    if (substr($name, 0, VARGROUPSPREFIXLEN) == VARGROUPSPREFIX)
    {   
        if ($mid = $db["mgroups"]->GetId(substr($name, VARGROUPSPREFIXLEN)))
            return new MGroupsRendData($mid);

        if ($mid = $db["groups"]->GetId(substr($name, VARGROUPSPREFIXLEN)))
            return new GroupsRendData($mid);
    }
    
    if ($mid = $db["manuals"]->GetId($name))
        return new ManualsRendData($mid);

    if ($mid = $db["mgroups"]->GetId($name))
        return new MGroupsRendData($mid, 2);

    if ($mid = $db["groups"]->GetId($name))
        return new GroupsRendData($mid, 3);

    if ($mid = $db["commands"]->GetId($name))
        return new CommandsRendData($mid);

    if ($mid = $db["variables"]->GetId($name))
        return new VariablesRendData($mid, 4);
    
    if ($mid = $db["options"]->GetId($name))
        return new OptionRendData($mid, $db["options"], 1);
    
    $default_id = $db["manuals"]->GetId(DEFAULTPAGE);
    return new ManualsRendData($default_id);
}

class BaseRendData // abstract class
{
    var $title;
    var $heading;
    var $content;
    var $lastupdate = 1131531351; // some very random constant chosen by johnny_cz
}

class ManualsRendData extends BaseRendData
{
    function ManualsRendData($id)
    {
        //echo $id;
        $mandata = new ManualsData;
        $d = $mandata->GetMan($id);
        $this->title = $d["title"];
        $this->heading = $this->title;
        $this->content = $d["content"];
    }
}

class MainPageRendData extends BaseRendData
/**
 * Returns content of homepage of docs
 * the structure of the docs homepage is hardcoded here
 */
{
    function MainPageRendData(&$db)
    {
        $this->heading = "Index";
        $this->title = "Index";
        
        /* installation */
        $this->content = "<dl id=\"main-page-list\"><dt>Installation</dt><dd>";
        $mid = $db["manuals"]->GetId("installation");
        $inst_index = new ManualsRendData($mid);
        $this->content .= $inst_index->content;
        
        /* features list */
        $this->content .= "</dd><dt>Features</dt><dd>";
        $mid = $db["manuals"]->GetId("features");
        $features_list = new ManualsRendData($mid);
        $this->content .= $features_list->content;

        /* settings */
        $this->content .= "</dd><dt>Settings</dt><dd><dl id=\"settings-list\"><dt>Variables</dt><dd>";
        
        /* variables menu */
        $this->content .= "<dl>";
        $grplist = $db["groups"]->GetGroupedList();
        $mgrplist = $db["mgroups"]->GetIDAssocList();
        foreach ($mgrplist as $mgrp_id => $mgrp_data)
        {
            $this->content .= "<dt><a href=\"?vars-".$mgrp_data["name"]."\">".htmlspecialchars($mgrp_data["title"])."</a></dt><dd>";
            $c = 0;
            foreach ($grplist[$mgrp_id] as $grp)
            {
                if ($c++)
                    $this->content .= ", ";
                
                $this->content .= "<a href=\"?vars-".$grp["name"]."\">".htmlspecialchars($grp["title"])."</a>";
            }
            $this->content .= "</dd>";
        }
        $this->content .= "</dl>";
        
        $this->content .= "</dd><dt><a href=\"?commands\">Commands</a></dt>";
        $this->content .= "<dt><a href=\"?command-line\">Command-Line Options</a></dt>";
        $this->content .= "<dt><a href=\"?triggers\">Triggers</a></dt>";
        $this->content .= "</dl></dd></dl>";
    }
}

class MGroupsRendData extends BaseRendData
{
    function MGroupsRendData($id, $topheading = 2)
    /* Initializes content variables - title and heading of the manual page
       and it's content 
       Should display variables major group - groups + variables of each group
    */
    {
        $topheading = (int) $topheading;
        if ($topheading < 1) $topheading = 1;
        if ($topheading > 6) $topheading = 6;
        $db = new MGroupsData;
        $this->title = $db->GetTitle($id)." Variables";
        $this->heading = $this->title;
        
        $groupdata = new GroupsData;
        if (!($d = $groupdata->GetList(0, "id_mgroup = {$id}")))
            return;
        
        foreach ($d as $k)
        {
            $index .= "<li><a href=\"#".$k["name"]."\">".htmlspecialchars($k["title"])."</a></li>";
            $grouprend = new GroupsRendData($k["id"], $topheading + 1);
            $this->content .= "\n<h{$topheading} class=\"vargroup\" id=\"".$k["name"]."\">".htmlspecialchars($grouprend->heading)."</h{$topheading}>\n";
            $this->content .= $grouprend->content;
        }
        
        $this->content = "<ul class=\"index\">".$index."</ul>".$this->content;
    }
}

class GroupsRendData extends BaseRendData
{
    function GroupsRendData($id, $topheading = 3)
    {
        $topheading = (int) $topheading;
        if ($topheading < 1) $topheading = 1;
        if ($topheading > 6) $topheading = 6;
        $vardata = new VariablesData;
        $groupdata = new GroupsData;
        $this->title = $groupdata->GetTitle($id)." Variables";
        $this->heading = $this->title;
        
        if (!($d = $vardata->GetList(2, "id_group = {$id}")))
            return;
        
        foreach ($d as $id => $name)
        {
            $varrend = new VariablesRendData($id, $topheading + 1);
            $this->content .= "<h{$topheading} class=\"variable\" id=\"{$name}\">{$varrend->title}&nbsp;<span>[<a href=\"?{$name}\">#</a>]</span></h{$topheading}>\n";
            $this->content .= $varrend->content;
        }
    }
}


class VariablesRendData extends BaseRendData
{
    function VariablesRendData($id, $topheading = 4)
    {
        $db = new VariablesData;
        $db_support = new SupportData;
        $builds = $db_support->GetBuilds();
        $var = $db->GetVar($id);
        
        $this->title = $var["name"];
        $this->heading = "Variable ".$this->title;
        if (strlen($var["description"]))
            $this->content = "\n<h{$topheading}>Description</h{$topheading}><p class=\"description\">".htmlspecialchars($var["description"])."</p>";
        
        $this->content .= "<p class=\"support\">Support: ";
        foreach ($builds as $build)
        {   
            $this->content .= "<span class=\"".$build["abbr"]." ";
            if (isset($var["support"][$build["id"]]))
            {
                $default = $var["support"][$build["id"]];    // we take last default .. every build can have it's own default but who cares?
                $this->content .= "supported";
            }
            else
            {
                $this->content .= "unsupported";
            }
            $this->content .= "\">".$build["title"]."</span> ";
        }
        
        $this->content .= "</p>\n  <p class=\"default\">Default: <span>".htmlspecialchars($default)."</span></p>";
        
        switch ($var["type"])
        {
            case "string":
            case "float":
            case "integer":
                $this->content .= "\n<p>Variable is <strong>".$var["type"]."</strong>.<br />".$var["valdesc"]."</p>";
                break;
            case "enum":
            case "boolean":
                $this->content .= "<h{$topheading}>Values</h{$topheading}>";
                $this->content .= "\n<table class=\"values\"><thead><tr><td>value</td><td>description</td></tr></thead>\n<tbody>";
                foreach ($var["valdesc"] as $value_name => $value_desc)
                {   
                    $value_name = $var["type"] == "boolean" ? ($value_name == "true" ? "1" : "0") : $value_name;
                    $this->content .= "<tr><td>".htmlspecialchars($value_name)."</td><td>".htmlspecialchars($value_desc)."</td></tr>";
                }
                $this->content .= "</tbody></table>";
                break;
        }
        
        if (strlen($var["remarks"]))
            $this->content .= "\n<p class=\"remarks\">".htmlspecialchars($var["remarks"])."</p>";
    }
}

class CommandsRendData extends BaseRendData
{
    function CommandsRendData($id)
    {
        $db = new CommandsData;
        $cmd = $db->GetCmd($id);
        $this->title = $cmd["name"];
        $this->heading = "Command ".$this->title;
        if (strlen($cmd["description"]))
            $this->content .= "\n<p class=\"description\">".htmlspecialchars($cmd["description"])."</p>\n";
        
        if (strlen($cmd["syntax"])) 
        {
            $this->content .= "\n<p class=\"syntax\">Syntax:<br /><strong>".htmlspecialchars($cmd["name"])."</strong>";
            $this->content .= " <code>".htmlspecialchars($cmd["syntax"])."</code></p>\n";
        }
        
        if (count($cmd["args"]))
        {
            $this->content .= "\n  <dl class=\"arguments\">";
            foreach ($cmd["args"] as $arg => $argdesc)
                $this->content .= "\n    <dt>{$arg}</dt>\n    <dd>".htmlspecialchars($argdesc)."</dd>";

            $this->content .= "\n  </dl>";
        }
        
        if (strlen($cmd["remarks"]))
            $this->content .= "\n<p class=\"remarks\">".htmlspecialchars($cmd["remarks"])."</p>\n";
    }
}

class CommandsAllRendData extends BaseRendData
{
    function CommandsAllRendData($topheading)
    {
        $db = new CommandsData;
        $cmds = $db->GetList();
        $this->heading = "Commands";
        $this->title = "Commands";
        
        $index = "";
        $let = ""; $oldlet = "";
        foreach ($cmds as $id => $name)
        {
            
            $cmd = new CommandsRendData($id);
            $let = substr($cmd->title, 0, 1);   // first letter of the command name
            if ($let != $oldlet)    // new letter
            {
                if ($oldlet != "") {
                    $this->content .= "</div>"; // close previous letter index
                    $index .= ", ";
                }
                
                $index .= "\n  <a href=\"#index-".IdSafe($let)."\">{$let}</a>";
                $this->content .= "<div id=\"index-".IdSafe($let)."\">\n\n";
            }
            $oldlet = $let;
            $this->content .= "\n<div class=\"command\" id=\"".IdSafe($name)."\">\n  <h{$topheading}>{$cmd->title}</h{$topheading}>\n";
            $this->content .= $cmd->content;
            $this->content .= "\n</div>\n"; // end of div.command
        }
        $this->content .= "\n</div>"; // end of div#index-x
        $this->content = "\n\n<p id=\"index\">{$index}\n</p>\n\n".$this->content;
    }
}

class OptionRendData extends BaseRendData
{
    function OptionRendData($id, &$db, $sideargs = 0)
    {
        $opt = $db->GetOption($id);
        $this->title = $opt["name"];
        $this->heading = $opt["name"]." Command-line option";
        
        $this->content .= "<div class=\"description\">".htmlspecialchars($opt["description"])."</div>";
        if (strlen($opt["args"]))
            if ($sideargs)
            	$this->content .= "<p>Arguments: <code>".htmlspecialchars($opt["args"])."</code></p>";
            else
                $this->title .= " <code>".htmlspecialchars($opt["args"])."</code>";
            
        if (strlen($opt["argsdesc"]))
            $this->content .= "<div><h3>Arguments</h3>".htmlspecialchars($opt["argsdesc"])."</div>";
        
        if (strlen($opt["flagnames"]))
            $this->content .= "<h3>Notes</h3><p>".$opt["flagnames"].".</p>";            
    }
}

class OptionsRendData extends BaseRendData
{
    
    function OptionsRendData()
    {
        $db = new OptionsData;
        $options = $db->GetList();
        $this->heading = "Command-line Options";
        $this->title = "Command-line Options";
        
        $index = "";
        $let = ""; $oldlet = "";
        foreach ($options as $id => $name)
        {
            $opt = new OptionRendData($id, $db);
            $let = substr($opt->title, 0, 1);   // first letter of the command name
            if ($let != $oldlet)    // new letter
            {
                if ($oldlet != "") {
                    $this->content .= "</div>"; // close previous letter index
                    $index .= ", ";
                }
                
                $index .= "\n  <a href=\"#index-".IdSafe($let)."\">{$let}</a>";
                $this->content .= "<div id=\"index-".IdSafe($let)."\">\n\n";
            }
            $oldlet = $let;

            $this->content .= "<div class=\"option\" id=\"".IdSafe($name)."\"><h2>-{$opt->title}</h2>\n";
            $this->content .= $opt->content;
            $this->content .= "\n</div>\n"; // end of div.command
        }
        $this->content .= "\n</div>"; // end of div#index-x
        $this->content = "\n\n<p id=\"index\">{$index}\n</p>\n\n".$this->content;
    }
}

?>
