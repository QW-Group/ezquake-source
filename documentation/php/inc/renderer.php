<?php

/**
 * Functions and classes for HTML manual rendering.
 * Uses mysql data mining classes from mysql_commands.php
 */
 
 // to-do - all the classes are mad, no need for them
 //  - too much of 'new' constructs
 
define (VARGROUPSPREFIX, "vars-");
define (VARGROUPSPREFIXLEN, 5);

function GetRenderer($name, &$db)
/* returns an initialized object which can be accessed for
   page title, heading and content */
{
    switch ($name)
    {
        case "commands": return new CommandsAllRendData(2, $db); return; break;
        case "command-line": return new OptionsRendData($db); return; break;
        case "main-page": return new MainPageRendData($db); return; break;
    }
    
    if (substr($name, 0, VARGROUPSPREFIXLEN) == VARGROUPSPREFIX)
    {   
        if ($mid = $db["mgroups"]->GetId(substr($name, VARGROUPSPREFIXLEN)))
            return new MGroupsRendData($mid, 2, $db);

        if ($mid = $db["groups"]->GetId(substr($name, VARGROUPSPREFIXLEN)))
            return new GroupsRendData($mid, 2, $db);
    }

    if ($mid = $db["manuals"]->GetId($name))
        return new ManualsRendData($mid, $db);
    
    if ($mid = $db["mgroups"]->GetId($name))
        return new MGroupsRendData($mid, 2, $db);

    if ($mid = $db["groups"]->GetId($name))
        return new GroupsRendData($mid, 3, $db);

    if ($mid = $db["commands"]->GetId($name))
        return new CommandsRendData($mid, $db["commands"]);

    if ($mid = $db["variables"]->GetId($name)) //         return new VariablesRendData($mid, 4);
    // rendering every each variable on it's own page would be fine (and it's possible)
    // but we don't want to have too much URLs because of offline manual version
    // so we do a redirect to the page which contains all variables from 
    // the same major group
    { 
        $var = $db["variables"]->GetVar($mid);
        $group_id = (int) $var["id_group"];
        if (!$group_id)
            return new VariablesRendData($mid);

        $mgroup_id = $db["groups"]->GetMGroupID($group_id);
        $var_name = IdSafe($name);
        $mgroup_name = IdSafe($db["mgroups"]->GetTitle($mgroup_id));
        $url = "http://".$_SERVER["HTTP_HOST"].FilePath($_SERVER["SCRIPT_NAME"])."/?vars-".$mgroup_name."#".$var_name;
        RefreshPage($url);
    }
    
    if ($mid = $db["options"]->GetId($name))
        return new OptionRendData($mid, $db["options"], 1);

    $url = "http://".$_SERVER["HTTP_HOST"].FilePath($_SERVER["SCRIPT_NAME"])."/";
    RefreshPage($url);
}

class BaseRendData // abstract class
{
    var $title;
    var $heading;
    var $content;
    var $lastupdate = 1131531351; // some very random constant chosen by johnny_cz
    var $db;
    var $id;
    var $topheading;
    
    function RenderContent()
    {
        echo $this->content;
    }
}

class ManualsRendData extends BaseRendData
{
    function ManualsRendData($id, &$db)
    {
        $d = $db["manuals"]->GetMan($id);
        $this->title = $d["title"];
        $this->heading = $this->title;
        $this->lastupdate = $db["manuals"]->LastUpdate($id);
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
        $inst_index = new ManualsRendData($mid, $db);
        $this->content .= $inst_index->content;
        
        /* features list */
        $this->content .= "</dd><dt id=\"features\">Features</dt><dd>";
        $mid = $db["manuals"]->GetId("features");
        $features_list = new ManualsRendData($mid, $db);
        $this->content .= $features_list->content;

        /* settings */
        $this->content .= "</dd><dt>Settings</dt><dd>";
        $this->content .= "<p>Note that you can put the name of any variable, command, command-line option or manual page into the URL and you'll get corresponding manual page displayed. E.g. http://ezquake.sourceforge.net/docs/?cl_maxfps</p>";
        $this->content .= "<dl id=\"settings-list\"><dt>Variables</dt><dd>";
        
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
    function MGroupsRendData($id, $topheading, &$db)
    /* Initializes content variables - title and heading of the manual page
       and it's content 
       Should display variables major group - groups + variables of each group
    */
    {
        $topheading = (int) $topheading;
        if ($topheading < 1) $topheading = 1;
        if ($topheading > 6) $topheading = 6;
        $this->id = $id;
        $this->topheading = $topheading;
        $this->db = $db;
        $this->title = $this->db["mgroups"]->GetTitle($id)." Variables";
        $this->heading = $this->title;
    }
    
    function RenderContent()
    {
        if (!($d = $this->db["groups"]->GetList(0, "id_mgroup = {$this->id}")))
            return;
            
        echo "\n<ul class=\"index\">\n";
        foreach ($d as $k)
            echo "\n  <li><a href=\"#".$k["name"]."\">".htmlspecialchars($k["title"])."</a></li>";        
        
        echo "\n</ul>\n\n";
        
        foreach ($d as $k)
        {
            $grouprend = new GroupsRendData($k["id"], $this->topheading + 1, $this->db);
            echo "\n<h{$this->topheading} class=\"vargroup\" id=\"".$k["name"]."\">".htmlspecialchars($grouprend->heading)."</h{$this->topheading}>\n";
            $grouprend->RenderContent();
        }
    }
}

class GroupsRendData extends BaseRendData
{
    function GroupsRendData($id, $topheading, &$db)
    {
        $topheading = (int) $topheading;
        if ($topheading < 1) $topheading = 1;
        if ($topheading > 6) $topheading = 6;
        $this->id = $id;
        $this->topheading = $topheading;
        $this->db = $db;
        $this->title = $this->db["groups"]->GetTitle($id)." Variables";
        $this->heading = $this->title;        
    }
    
    function RenderContent()
    {
        if (!($d = $this->db["variables"]->GetList(2, "id_group = {$this->id}")))
            return;
        
        foreach ($d as $id => $name)
        {
            $varrend = new VariablesRendData($id, $this->topheading + 1);
            echo "<h{$this->topheading} class=\"variable\" id=\"{$name}\">{$varrend->title}&nbsp;";
            // no need for this URL - echo "<span>[<a href=\"?{$name}\">#</a>]</span>";
            echo "</h{$this->topheading}>\n";
            $varrend->RenderContent();
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
    function CommandsRendData($id, &$db)
    {
        $cmd = $db->GetCmd($id);
        $this->title = $cmd["name"];
        $this->heading = "Command ".$this->title;
        $this->lastupdate = $db->LastUpdate($id);
        
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
    function CommandsAllRendData($topheading, $db)
    {
        $this->db = $db["commands"];
        $this->heading = "Commands";
        $this->title = "Commands";
        $this->lastupdate = $this->db->GlobalLastUpdate();
        $this->topheading = $topheading;
    }
    
    function RenderContent()
    {
        $cmds = $this->db->GetList();
        $let = ""; $oldlet = "";

        echo "\n\n<p id=\"index\">\n";
        foreach ($cmds as $name)
        {
            $let = substr($name, 0, 1);
            if ($let != $oldlet)    // new letter
            {
                if ($oldlet != "") echo ", ";
                echo "\n  <a href=\"#index-".IdSafe($let)."\">{$let}</a>";
            }
            $oldlet = $let;
        }
        echo "\n</p>\n\n";
        
        $let = ""; $oldlet = "";
        foreach ($cmds as $id => $name)
        {
            $cmd = new CommandsRendData($id, $this->db);
            $let = substr($cmd->title, 0, 1);   // first letter of the command name
            if ($let != $oldlet)    // new letter
            {
                if ($oldlet != "") echo "</div>"; // close previous letter index
                echo "<div id=\"index-".IdSafe($let)."\">\n\n";
            }
            $oldlet = $let;
            echo "\n<div class=\"command\" id=\"".IdSafe($name)."\">\n  <h{$this->topheading}>{$cmd->title}</h{$this->topheading}>\n";
            $cmd->RenderContent();
            echo "\n</div>\n"; // end of div.command
        }
        echo "\n</div>"; // end of div#index-x
    }
}

class OptionRendData extends BaseRendData
{
    function OptionRendData($id, &$db, $sideargs = 0)
    {
        $opt = $db->GetOption($id);
        $this->title = $opt["name"];
        $this->heading = $opt["name"]." Command-line option";
        $this->lastupdate = $db->LastUpdate($id);
        
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
    
    function OptionsRendData(&$db)
    {
        $this->db = $db["options"];
        $this->heading = "Command-line Options";
        $this->title = "Command-line Options";
        $this->lastupdate = $this->db->GlobalLastUpdate();
    }
    
    function RenderContent()
    {
        $options = $this->db->GetList();

        echo "\n\n<p id=\"index\">\n";
        $let = ""; $oldlet = "";
        foreach ($options as $name)
        {
            $let = substr($name, 0, 1);   // first letter of the command name
            if ($let != $oldlet)    // new letter
            {
                if ($oldlet != "") echo ", ";
                echo "\n  <a href=\"#index-".IdSafe($let)."\">{$let}</a>";
            }
            $oldlet = $let;        
        }
        echo "\n</p>\n\n";
        
        $let = ""; $oldlet = "";
        foreach ($options as $id => $name)
        {
            $opt = new OptionRendData($id, $this->db);
            $let = substr($name, 0, 1);   // first letter of the command name
            if ($let != $oldlet)    // new letter
            {
                if ($oldlet != "") echo "</div>"; // close previous letter index
                echo "<div id=\"index-".IdSafe($let)."\">\n\n";
            }
            $oldlet = $let;

            echo "<div class=\"option\" id=\"".IdSafe($name)."\"><h2>-{$opt->title}</h2>\n";
            $opt->RenderContent();
            echo "\n</div>\n"; // end of div.command
        }
        echo "\n</div>"; // end of div#index-x
    }
}

?>
