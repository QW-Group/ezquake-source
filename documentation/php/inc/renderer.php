<?php

/**
 * Functions and classes for HTML manual rendering.
 * Uses mysql data mining classes from mysql_commands.php
 */
 
define (VARGROUPSPREFIX, "vars-");
define (VARSUNASSIGNED, "vars-unassigned");
define (VARGROUPSPREFIXLEN, 5);

function GetRenderer($name, &$db)
/* returns an initialized object which can be accessed for
   page title, heading and content */
{
    $searchbit = 0;
    
    switch ($name)
    {
        case "commands": return new CommandsAllRendData(2, $db); return; break;
        case "command-line": return new OptionsRendData($db); return; break;
        case "main-page": return new MainPageRendData($db); return; break;
        case "index": return new IndexRendData($db); return; break;
        case "search": 
            $searchbit = 1; // remember we did a search
            $name = $_REQUEST["search"]; // treat the text of the search as it is some manual page
            if (!$_REQUEST["omitstats"]) // flag is added when admin wants to check what search results some text gives
                $db["search"]->Hit($name); // register hit for this search query text 
        break;
    }
    
    if (substr($name, 0, VARGROUPSPREFIXLEN) == VARGROUPSPREFIX)
    {   
        if ($mid = $db["mgroups"]->GetId(substr($name, VARGROUPSPREFIXLEN)))
            return new MGroupsRendData($mid, 2, $db);
        
        if ($name == VARSUNASSIGNED)
        {
            return new GroupsRendData(0, 2, $db);
        }
        else if ($mid = $db["groups"]->GetId(substr($name, VARGROUPSPREFIXLEN)))
            return new GroupsRendData($mid, 2, $db);
    }

    if ($mid = $db["manuals"]->GetId($name))
        return new ManualsRendData($mid, $db);
    
    if ($mid = $db["mgroups"]->GetId($name))
        return new MGroupsRendData($mid, 2, $db);

    if ($mid = $db["groups"]->GetId($name))
        return new GroupsRendData($mid, 3, $db);

    if ($mid = $db["commands"]->GetId($name))
    {
        $url = "http://".$_SERVER["HTTP_HOST"].FilePath($_SERVER["SCRIPT_NAME"])."/?commands#".IdSafe($name);
        RefreshPage($url);
    }
    
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
    {
        $url = "http://".$_SERVER["HTTP_HOST"].FilePath($_SERVER["SCRIPT_NAME"])."/?command-line#".IdSafe($name);
        RefreshPage($url);
    }

    if ($searchbit)
    {
        // we didn't find anything usefull, let's use google instead
        $url = "http://www.google.com/search?q=site:".urlencode(BASEURL)."+".urlencode($name);
        RefreshPage($url);
    }
    else // didn't run a search, just an non-legal URL, act like nothing happened
    {
        $url = "http://".$_SERVER["HTTP_HOST"].FilePath($_SERVER["SCRIPT_NAME"])."/";
        RefreshPage($url);
    }
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
        $this->lastupdate = $db["manuals"]->GlobalLastUpdate();
        $this->db = $db;
    }
    
    function RenderContent()
    {
        if (SHOW_SEARCH)
        {
            echo '
            <div id="search"><form method="get" action=".">
                <fieldset><legend>Search</legend>
                <input type="text" size="20" name="search" />
                <button type="submit">Submit</button>
                </fieldset>
            </form></div>';
        }
        
        echo "\n\n<dl id=\"main-page-list\">";
        
        if (SHOW_INSTALLATION)
        {
            echo "<dt>Installation</dt><dd>";
            $mid = $this->db["manuals"]->GetId("installation");
            $inst_index = new ManualsRendData($mid, $this->db);
            echo $inst_index->content;
            echo "</dd>\n";
        }
        
        if (SHOW_FEATURES)
        {
            echo "<dt id=\"features\">Features</dt><dd>";
            $mid = $this->db["manuals"]->GetId("features");
            $features_list = new ManualsRendData($mid, $this->db);
            echo $features_list->content;
            echo "</dd>\n";
        }

        if (SHOW_SETTINGS)
        {
            echo "\n<dt>Settings</dt><dd>";
            echo "<p><strong><a href=\"?index\">Index</a></strong> - Full list of variables, commands and command-line options</p>";
            echo "<p>Note that you can put the name of any variable, command, command-line option or manual page into the URL and you'll get corresponding manual page displayed. E.g. ".BASEURL."?cl_maxfps</p>";
            echo "<dl id=\"settings-list\">";
            
            if (SHOW_VARIABLES)
            {
                echo "<dt>Variables</dt><dd><dl>";
                $grplist = $this->db["groups"]->GetGroupedList();
                $mgrplist = $this->db["mgroups"]->GetIDAssocList();
                foreach ($mgrplist as $mgrp_id => $mgrp_data)
                {
                    echo "  <dt><a href=\"?vars-".$mgrp_data["name"]."\">".htmlspecialchars($mgrp_data["title"])."</a></dt>\n";
                    echo "  <dd>";
                    $c = 0;
                    foreach ($grplist[$mgrp_id] as $grp)
                    {
                        if ($c++)
                            echo ", ";
                        
                        echo "<a href=\"?vars-".$grp["name"]."\">".htmlspecialchars($grp["title"])."</a>";
                    }
                    echo "</dd>\n";
                }
                echo "  <dt><a href=\"?".VARSUNASSIGNED."\">Unassigned</a></dt>\n";
                echo "</dl></dd>\n";
            }
            
            if (SHOW_COMMANDS)
                echo "<dt><a href=\"?commands\">Commands</a></dt>\n";
            
            if (SHOW_OPTIONS)
                echo "<dt><a href=\"?command-line\">Command-Line Options</a></dt>\n";
                
            if (SHOW_TRIGGERS)
                echo "<dt><a href=\"?triggers\">Triggers</a></dt>\n";
                
            echo "</dl>\n"; // end of settings types list
            echo "</dd>\n"; // end of settings part
        }
        
        echo "</dl>\n";
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
        $this->id = (int) $id;
        $this->topheading = $topheading;
        $this->db = $db;
        if ($this->id)
            $this->title = $this->db["groups"]->GetTitle($id)." Variables";
        else
            $this->title = "Unassigned Variables";
            
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
        $this->db = new VariablesData;
        $this->id = $id;
        $this->var = $this->db->GetVar($this->id);
        $this->title = $this->var["name"];
        $this->heading = "Variable ".$this->title;
        $this->topheading = $topheading;
    }
    
    function RenderContent()
    {
        $db_support = new SupportData;
        $builds = $db_support->GetBuilds();
        $topheading = $this->topheading;

        if (strlen($this->var["description"]))
            echo "\n<h{$topheading}>Description</h{$topheading}><p class=\"description\">".htmlspecialchars($this->var["description"])."</p>";
        
        echo "<p class=\"support\">Support: ";
        foreach ($builds as $build)
        {   
            echo "<span class=\"".$build["abbr"]." ";
            if (isset($this->var["support"][$build["id"]]))
            {
                $default = $this->var["support"][$build["id"]];    // we take last default .. every build can have it's own default but who cares?
                echo "supported";
            }
            else
            {
                echo "unsupported";
            }
            echo "\">".$build["title"]."</span> ";
        }
        
        echo "</p>\n  <p class=\"default\">Default: <span>".htmlspecialchars($default)."</span></p>";
        
        switch ($this->var["type"])
        {
            case "string":
            case "float":
            case "integer":
                echo "\n<p>Variable is <strong>".$this->var["type"]."</strong>.<br />".$this->var["valdesc"]."</p>";
                break;
            case "enum":
            case "boolean":
                echo "<h{$topheading}>Values</h{$topheading}>";
                echo "\n<table class=\"values\"><thead><tr><td>value</td><td>description</td></tr></thead>\n<tbody>";
                foreach ($this->var["valdesc"] as $value_name => $value_desc)
                {   
                    $value_name = $this->var["type"] == "boolean" ? ($value_name == "true" ? "1" : "0") : $value_name;
                    echo "<tr><td>".htmlspecialchars($value_name)."</td><td>".htmlspecialchars($value_desc)."</td></tr>";
                }
                echo "</tbody></table>";
                break;
        }
        
        if (strlen($this->var["remarks"]))
            echo "\n<p class=\"remarks\">".htmlspecialchars($this->var["remarks"])."</p>";
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

class IndexRendData extends BaseRendData
{
    function IndexRendData(&$db)
    {
        $this->db = $db["index"];
        $this->heading = "Settings Index";
        $this->title = "Index";
        $this->lastupdate = $db["commands"]->GlobalLastUpdate();
    }
    
    function RenderContent()
    {
        $r = array();
        if (!$this->db->FetchList($r)) {
            echo "<p>Database error. Try again later.</p>";
            return;
        }
        
        $alphabet = "+-abcdefghijklmnopqrstuvwxyz";
        $alphlen = strlen($alphabet);
        echo "<p id=\"index\">";
        for ($i = 0; $i < $alphlen; $i++) {
            echo "<a href=\"#letter-".$alphabet{$i}."\">".$alphabet{$i}."</a>";
            if ($i < $alphlen-1)
                echo ", ";
        }
        
        echo "</p>";
        
        echo "<table id=\"index\">\n<col id=\"type\" /><col id=\"name\" /><col id=\"desc\" />\n";
        echo "<thead><tr><td>Type</td><td>Name</td><td>Short description</td></tr></thead>\n<tbody>\n";
        
        $oldletter = "";            
        foreach($r as $k => $v) {
            if ($k{0} != $oldletter) {
                echo "\n\t<tr class=\"section\"><td colspan=\"3\" id=\"letter-".$k{0}."\">&middot;{$k{0}}&middot;</td></tr>";
                echo "\n\t<tr><td colspan=\"3\"><a href=\"#top\">Back to the top</a></td></tr>";
                $oldletter = $k{0};
            }
            echo "\n\t<tr><td class=\"";
            switch ($v["type"]) {
                case "variable":
                    echo "var\"><span>var";
                    $urlprefix = $v["group"] ? "vars-".IdSafe($v["group"])."#" : "";
                break;
                case "command": 
                    echo "cmd\"><span>cmd"; 
                    $urlprefix = "commands#"; 
                break;
                default: case "command-line option": 
                // default shouldn't happen but we put it here so the page doesn't get messed up by this minor error 
                    echo "opt\"><span>opt";
                    $urlprefix = "command-line#"; 
                break;
            } 
            echo "</span></td><td><a href=\"?";
            echo $urlprefix.IdSafe($k);
            echo "\">$k</a></td><td>".htmlspecialchars(substr($v["desc"], 0, 100)).(strlen($v["desc"])>100 ? "..." : "")."</td></tr>";
        }
        
        echo "\n</tbody>\n</table>\n\n";    
    }
}

?>
