<?php

/**
 * Interfaces for documentation data mining from MySQL
 */

define (MAXCMDARGS, 9);
define (COMMANDSTABLEPREFIX, 'commands');
define (MAXVARENUMS, 15);
define (VARIABLESTABLEPREFIX, 'variables');
define (MANUALSTABLEPREFIX, 'manuals');
define (VARMGROUPSTABLE, 'variables_mgroups');
define (VARGROUPSTABLE, 'variables_groups');
define (VARSUPPORTTABLE, 'variables_support');
define (OPTIONSTABLEPREFIX, 'options');
define (INDEXTABLE, 'settings_index');

class DocsData
// abstract, common functions and structures for variables, commands, command-line options and manuals
{
    var $tblPrefix;     // table prefix
    var $detlistattrs;  // detailed list attributes
    var $detlistdescs;  // detailed list descriptions
    var $attrs;         // attributes of doc entry
    var $foreignkey;    // field name for referencing doc entry id
    
    function GetId($name)
    {   // returns ID number of entry with that name
        $name = addslashes($name);
        if (!strlen($name))
            return False;
            
        $r = my_mysql_query("SELECT id FROM {$this->tblPrefix} WHERE name = '{$name}' LIMIT 1");
        if (!r || !mysql_num_rows($r))
            return False;
        
        return mysql_result($r, 0);
    }

    function GetContent($id)
    {   // returns associated array with content of the entry with given ID number
        $ret = array();
        $id = (int) $id;
        if (!$id)
            return False;
            
        $r = my_mysql_query("SELECT * FROM {$this->tblPrefix} WHERE id = {$id} LIMIT 1");
        if (!$r || !mysql_num_rows($r))
            return False;
        
        $r = mysql_fetch_assoc($r);

        // i did some stripslashing here but later i found it's bahaviour is causing a flaw
        // so i removed it .. but couldn't remember why did i do that before - johnnycz

        return $r;
    }

    function AddBase($name, $valuessql, $userId)
    /* $name - entry name
       $valuessql - part of sql commands containing the values that should be inserted
                    in the same order as $this->attrs are
       $userId - ID of user doing this operation
    */
    {
        if ($this->GetId($name))
            return False;
        
        $sql = "INSERT INTO {$this->tblPrefix} (name, {$this->attrs}) VALUES (";
        $sql .= "{$valuessql}";
        $sql .= ")";
        
        if (!($r = my_mysql_query($sql)))
            return False;
            
        $lid = mysql_insert_id();
        
        $sql = "INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action) VALUES ({$lid}, {$userId}, 'created')";
        if (!my_mysql_query($sql))
            return False;
        
        return $lid;
    }

    function AddOrUpdate($data, $user = 1)
    {
        $cId = $this->GetId($data["name"]);
        if ($cId)   // variable does exist -> just update
            return $this->Update($cId, $data, $user);  
        else        // variable with given name doesn't exist -> add
            return $this->Add($data, $user);
    }

    function GetList($condition = 1, $custom = "")
    /* return an associated list of variables of pair id-name */
    {
        switch ($condition) // ternary operator would do the job too
        {
            case 2: $where = $custom; break;
            case 1: $where = "(active > 0)"; break;
            case 0: default: $where = "1";
        }
        
        $r = my_mysql_query("SELECT id, name FROM {$this->tblPrefix} WHERE {$where} ORDER BY name ASC");
        if (!$r && !mysql_num_rows($r))
            return False;
            
        $ret = array();
        while ($d = mysql_fetch_assoc($r))
            $ret[$d["id"]] = $d["name"];
            
        return $ret;
    }
    
    function GetDetailedList($orderby = "name", $order = "ASC", $where = "1")
    // returns list of documentation entries with more attributes, given by $this->detlistattrs
    {
        if (!isset($this->detlistattrs[$orderby])) $orderby = "name";
        
        if ($order != "ASC")
            $order = "DESC";    // ensure query security

        $select = "";
        foreach ($this->detlistattrs as $k => $v)
        {
            if ($select != "")
                $select .= ", ";

            $select .= $v." AS ".$k;
        }
        $query = "SELECT {$select} FROM {$this->tblPrefix} WHERE {$where} ORDER BY {$orderby} {$order}, name ASC";
        $r = my_mysql_query($query);
        if (!$r && !mysql_num_rows($r))
            return False;
            
        $ret = array();
        while ($d = mysql_fetch_assoc($r))
            $ret[$d["id"]] = $d;
            
        return $ret;
    }
    
    function Rename($oldname, $newname, $userId)
    {
        if (!($oldid = (int) $this->GetId($oldname)))  // old must exist
            return False;
            
        if ($this->GetId($newname))     // new cannot exist
            return False;
        
        // deactivate old command
        $query = "UPDATE {$this->tblPrefix} SET active = 0 WHERE id = {$oldid} LIMIT 1";
        if (!my_mysql_query($query)) return False;

        // create an active copy of it
        $newname = addslashes($newname);
        $query = "INSERT INTO {$this->tblPrefix} (name, {$this->attrs}, active) ";
        $query .= "SELECT '{$newname}', {$this->attrs}, 1 FROM {$this->tblPrefix} WHERE id = '{$oldid}' LIMIT 1";
        if (!my_mysql_query($query)) return False;
        $newid = mysql_insert_id();
        
        // update history log
        $userId = (int) $userId;
        $query = "INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action, id_renamedto) VALUES ";
        $query .= "({$oldid}, {$userId}, 'renamed', {$newid})";
        if (!my_mysql_query($query)) return False;
        
        return True;
    }
    
    function Remove($name, $physically, $userId)
    {
        $id = $this->GetId($name);
        if (!$id) return False;
        
        if ($physically)
        {
            $query = "DELETE FROM {$this->tblPrefix} WHERE id = {$id} LIMIT 1";
            if (!my_mysql_query($query)) return False;
            
            $query = "DELETE FROM {$this->tblPrefix}_history WHERE {$this->foreignkey} = {$id}";
            if (!my_mysql_query($query)) return False;
            
            $this->RemoveAssigned($id);
        }
        else
        {
            if (!($id = (int) $id))
                return False;
                
            $query = "UPDATE {$this->tblPrefix} SET active = 0 WHERE id = {$id} LIMIT 1";
            if (!my_mysql_query($query)) return False;
            
            $userId = (int) $userId;
            $query = "INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action) VALUES ({$id}, {$userId}, 'deleted')";
            if (!my_mysql_query($query)) return False;
        }
        return True;
    }
    
    function RemoveAssigned($id) {}

    function GetHistory($rows)
    { 
        if (($rows = (int) $rows) < 1) $rows = 20;
        $sql = "
        SELECT c.name as Entry, h.action as Action, u.name as User, DATE_FORMAT(h.time, '%Y-%m-%d %h:%i') as Time
        FROM {$this->tblPrefix}_history as h, users as u, {$this->tblPrefix} as c
        WHERE u.id = h.id_user && c.id = h.{$this->foreignkey}
        ORDER BY h.id DESC LIMIT {$rows};
        ";
        
        if (!($r = my_mysql_query($sql)))
            return False;
        
        $ret = array();
        while ($d = mysql_fetch_assoc($r))
            $ret[] = $d;
            
        return $ret;
    }
    
    function LastUpdate($id)
    {
        $id = (int) $id;
        $r = my_mysql_query("SELECT UNIX_TIMESTAMP(MAX(time)) FROM {$this->tblPrefix}_history WHERE {$this->foreignkey} = {$id}");
        return ($r ? mysql_result($r, 0) : False);
    }
    
    function GlobalLastUpdate()
    {
        $r = my_mysql_query("SELECT UNIX_TIMESTAMP(MAX(time)) FROM {$this->tblPrefix}_history");
        return ($r ? mysql_result($r, 0) : False);
    }
}

class CommandsData extends DocsData // interface for data storage
{       
    function CommandsData()
    {
        $this->tblPrefix = COMMANDSTABLEPREFIX;
        $this->detlistattrs = array(
            'id' => 'id',
            'name' => 'name',
            'deslen' => 'LENGTH(description)',
            'synlen' => 'LENGTH(syntax)',
            'remlen' => 'LENGTH(remarks)',
            'totlen' => 'LENGTH(description) + LENGTH(syntax) + LENGTH(remarks)',
            'active' => 'active'
        );
        $this->detlistdescs = array(
            'id' => 'id',
            'name' => 'name',
            'deslen' => 'description length',
            'synlen' => 'syntax length',
            'remlen' => 'remarks length',
            'totlen' => 'total documentation length',
            'active' => 'is command active?'        
        );
        $this->attrs = "description, syntax, remarks";
        $this->foreignkey = "id_command";
    }
    
    function Update($id, $data, $user = 1)
    {
        if (!($id = (int) $id))
            return False;
            
        $description = addslashes($data["description"]); 
        $remarks = addslashes($data["remarks"]);
        $syntax = addslashes($data["syntax"]);
        $args = $data["args"];
        $user = (int) $user;
        
        if (!($r = my_mysql_query("SELECT Count(*) FROM {$this->tblPrefix}_arguments WHERE {$this->foreignkey} = $id")))
            return False;
            
        $count_new = mysql_result($r, 0);
        $count_old = count($args);
        $change = $count_new == $count_old ? "updated" : "changed"; // how big change it was?
        
        $sql = "UPDATE {$this->tblPrefix} SET description = '{$description}', remarks = '{$remarks}', syntax = '{$syntax}' WHERE id = {$id} LIMIT 1";
        if (!my_mysql_query($sql))
            return False;
        
        if (!my_mysql_query("DELETE FROM {$this->tblPrefix}_arguments WHERE {$this->foreignkey} = {$id}"))
            return False;
        
        if (!$this->AssignArgs($id, $args))
            return False;
        
        if (!my_mysql_query("INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action) VALUES ({$id}, {$user}, '{$change}')"))
            return False;
        
        return True;
    }

    function Add($data, $user = 1)
    // $name, $description, $remarks, $syntax, $args
    {
        if ($this->GetId($data["name"])) return -1;
        
        $user = (int) $user;
        $name = addslashes($data["name"]); $description = addslashes($data["description"]); 
        $remarks = addslashes($data["remarks"]); $syntax = addslashes($data["syntax"]);
        $args = $data["args"];
        
        $sql = "INSERT INTO {$this->tblPrefix} (name, {$this->attrs}) VALUES ('{$name}', '{$description}', '{$syntax}', '{$remarks}')";
        $r = my_mysql_query($sql);
        if (!r)
            return False;
        $lid = mysql_insert_id();
        
        $this->AssignArgs($lid, $args);
        
        $sql = "INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action) VALUES ({$lid}, 1, 'created')";
        if (!my_mysql_query($sql))
            return False;
        
        return $lid;
    }
    
    function AssignArgs($cId, $args)
    {
        $cId = (int) $cId;
        if (!$cId) 
            return False;
        
        if (count($args))
        {
            foreach($args as $arg => $desc)
            {
                $arg = addslashes($arg);
                $desc = addslashes($desc);
                $sql = "INSERT INTO {$this->tblPrefix}_arguments ({$this->foreignkey}, name, description) VALUES ({$cId}, '{$arg}', '{$desc}')";
                if (!my_mysql_query($sql))
                    return False;
            }
        }        
        
        return True;
    }
    
    function GetCmd($id)
    {
        if (!($ret = $this->GetContent($id)))
            return False;
        
        $r = my_mysql_query("SELECT name, description FROM {$this->tblPrefix}_arguments WHERE {$this->foreignkey} = {$id}");
        if (!$r)
            return False;
        
        $args = array();
        while ($d = mysql_fetch_assoc($r))
            $args[stripslashes($d["name"])] = stripslashes($d["description"]);
        
        if (count($args))
            $ret["args"] = $args;
        
        return $ret;
    }
    
    function RemoveAssigned($id)
    {
        $query = "DELETE FROM {$this->tblPrefix}_arguments WHERE {$this->foreignkey} = {$id}";
        if (!my_mysql_query($query)) return False;
        return True;
    }
}

class VariablesData extends DocsData
{
    function VariablesData()
    {
        $this->tblPrefix = VARIABLESTABLEPREFIX;
        $this->detlistattrs = array(
            'id' => 'id',
            'name' => 'name',
            'deslen' => 'LENGTH(description)',
            'remlen' => 'LENGTH(remarks)',
            'totlen' => 'LENGTH(description) + LENGTH(remarks)',
            'type' => 'type',
            'vgroup' => 'id_group',
            'active' => 'active'
        );
        $this->detlistdescs = array(
            'id' => 'id',
            'name' => 'name',
            'deslen' => 'description length',
            'remlen' => 'remarks length',
            'totlen' => 'total documentation length',
            'type' => 'type',
            'vgroup' => 'group',
            'active' => 'is command active?'        
        );
        $this->attrs = "description, remarks, type";
        $this->foreignkey = "id_variable";
    }

    function Add($data, $userId = 1)
    {
        if ($this->GetId($data["name"])) return -1;
        
        $user = (int) $user;
        $name = addslashes($data["name"]);
        $description = addslashes($data["description"]); 
        $remarks = addslashes($data["remarks"]);
        $type = strtolower(trim($data["type"]));
        $valdesc = $data["valdesc"];
        
        $sql = "INSERT INTO {$this->tblPrefix} (name, {$this->attrs}) VALUES ('{$name}', '{$description}', '{$remarks}', '{$type}')";
        if (!($r = my_mysql_query($sql)))
            return False;
            
        $lid = mysql_insert_id();
        
        if (!$this->AddArgs($lid, $type, $valdesc))
            return False;
        
        $sql = "INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action) VALUES ({$lid}, {$userId}, 'created')";
        if (!my_mysql_query($sql))
            return False;
        
        return $lid;
    }
    
    function Update($id, $data, $userId)
    {
        if (!($id = (int) $id))
            return False;
            
        $description = addslashes($data["description"]); 
        $remarks = addslashes($data["remarks"]);
        $valdesc = $data["valdesc"];
        $type = addslashes($data["type"]);
        $user = (int) $user;
        
        if (!($r = my_mysql_query("SELECT type FROM {$this->tblPrefix} WHERE id = $id LIMIT 1")))
            return False;
            
        $oldtype = mysql_result($r, 0);
        $change = $oldtype == $data["type"] ? "updated" : "changed"; // how big change it was?
        
        $sql = "UPDATE {$this->tblPrefix} SET description = '{$description}', remarks = '{$remarks}', type = '{$type}' WHERE id = {$id} LIMIT 1";
        if (!my_mysql_query($sql))
            return False;
        
        if (!$this->RemoveAssigned($id))
            return False;
        
        if (!$this->AddArgs($id, $data["type"], $valdesc))
            return False;
        
        if (!my_mysql_query("INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action) VALUES ({$id}, {$user}, '{$change}')"))
            return False;
        
        return True;

    }
    
    function AddArgs($varId, $type, $args)
    {   // assigns detailed description of variable value(s)
        $type = strtolower(trim($type));
        $varId = (int) $varId;
        if (!$varId)
            return False;

        switch ($type)
        {
            case "float":
            case "integer":
            case "string":
                $desc = addslashes(trim($args));
                if (!strlen($desc))
                    return True;
                    
                $sql = "INSERT INTO {$this->tblPrefix}_values_other ({$this->foreignkey}, description) ";
                $sql .= " VALUES ({$varId}, '{$desc}')";
                if (!my_mysql_query($sql))
                    return False;
                    
                break;
            case "boolean":
                $true = addslashes(trim($args["true"]));
                $false = addslashes(trim($args["false"]));
                if (!strlen($true) && !strlen($false))
                    return True;
                    
                $sql = "INSERT INTO {$this->tblPrefix}_values_boolean ({$this->foreignkey}, true_desc, false_desc) ";
                $sql .= "VALUES ({$varId}, '{$true}', '{$false}')";
                if (!my_mysql_query($sql))
                    return False;
                
                break;
            case "enum":
                if (!is_array($args) || !count($args))
                    // no arguments given
                    return True;

                $sql = "INSERT INTO {$this->tblPrefix}_values_enum ({$this->foreignkey}, value, description) VALUES ";
                $c = 0;
                foreach ($args as $k => $v)
                {
                    if ($c++ > 0) $sql .= ", ";
                    $argname = addslashes(trim($k));
                    $argdesc = addslashes(trim($v));
                    $sql .= "({$varId}, '{$argname}', '{$argdesc}')";
                }
                if (!my_mysql_query($sql))
                    return False;
                    
                break;
        }
        return True;
    }
    
    function GetIds()
    /* we use this slow query function only where it saves a lot of small quick queries */
    {
        $sql = "SELECT id, name FROM {$this->tblPrefix} WHERE 1";
        if (!($r = my_mysql_query($sql)))
            return False;
            
        $ret = array();
        while ($d = mysql_fetch_assoc($r))
            $ret[$d["name"]] = $d["id"];
        
        return $ret;
    }
    
    function SetGroup($variable_ids, $id_group)
    /** Assigns given variables to given group.
        We expect more variables to be assigned in one query so $variable_ids is an array */
    {
        $c = 0; $vname = "";
        foreach ($variable_ids as $id_variable)
        {   
            if ($c++)
                $whr .= " || ";
            $whr .= " id = ".((int) $id_variable)." ";
        }

        $id_group = (int) $id_group;
        $sql = "UPDATE {$this->tblPrefix} SET id_group = {$id_group} WHERE {$whr} LIMIT {$c}";
        if (!my_mysql_query($sql))
            return False;
        
        return True;
    }
    
    function GetVar($id)
    {
        $id = (int) $id;
        if (!($ret = $this->GetContent($id)))
            return False;
        
        switch ($ret["type"])
        {
            case "boolean":
                if (!($r = my_mysql_query("SELECT true_desc, false_desc FROM {$this->tblPrefix}_values_boolean WHERE {$this->foreignkey} = {$id} LIMIT 1")))
                    return False;
                    
                if (!mysql_num_rows($r))
                {
                    $ret["valdesc"]["true"] = "";
                    $ret["valdesc"]["false"] = "";
                }
                else
                {
                    $d = mysql_fetch_assoc($r);
                    $ret["valdesc"]["true"] = $d["true_desc"];
                    $ret["valdesc"]["false"] = $d["false_desc"];
                }
                break;
            case "enum":
                if (!($r = my_mysql_query("SELECT value, description FROM {$this->tblPrefix}_values_enum WHERE {$this->foreignkey} = {$id} ORDER BY id ASC")))
                    return False;
                
                if (!mysql_num_rows($r))
                    $ret["valdesc"] = array();
                else
                    while($d = mysql_fetch_assoc($r))
                        $ret["valdesc"][$d["value"]] = $d["description"];

                break;
            case "float":
            case "string":
            case "integer":
            default:
                if (!($r = my_mysql_query("SELECT description FROM {$this->tblPrefix}_values_other WHERE {$this->foreignkey} = {$id} LIMIT 1")))
                    return False;
                
                $ret["valdesc"] = mysql_num_rows($r) ? mysql_result($r, 0) : "";
                break;
        }
        
        if (!$r = my_mysql_query("SELECT id_build, default_value FROM ".VARSUPPORTTABLE." WHERE id_variable = {$id}"))
            return False;
        
        $supp = array();
        while ($d = mysql_fetch_assoc($r))
            $supp[$d["id_build"]] = $d["default_value"];
            
        $ret["support"] = $supp;
        
        return $ret;
    }

    function RemoveAssigned($id)
    {
        if (!my_mysql_query("DELETE FROM {$this->tblPrefix}_values_other WHERE {$this->foreignkey} = {$id} LIMIT 1")) return False;
        if (!my_mysql_query("DELETE FROM {$this->tblPrefix}_values_enum WHERE {$this->foreignkey} = {$id}")) return False;
        if (!my_mysql_query("DELETE FROM {$this->tblPrefix}_values_boolean WHERE {$this->foreignkey} = {$id} LIMIT 1")) return False;
        return True;
    }
}

class ManualsData extends DocsData
{
    function ManualsData()
    {
        $this->tblPrefix = MANUALSTABLEPREFIX;
        $this->detlistattrs = array(
            'id' => 'id',
            'name' => 'name',
            'title' => 'title',
            'contlen' => 'LENGTH(content)',
            'active' => 'active'
        );
        $this->detlistdescs = array(
            'id' => 'id',
            'name' => 'Name',
            'title' => 'Title',
            'contlen' => 'Content length',
            'active' => 'Active'
        );
        $this->attrs = "title, content";
        $this->foreignkey = "id_manual";
    }
    
    function GetMan($id)
    {
        return $this->GetContent($id);
    }
    
    function Add($data, $userId = 1)
    { // todo: rewrite using $this->AddBase(...);
        if ($this->GetId($data["name"])) return -1;
        
        $user = (int) $user;
        $name = addslashes($data["name"]);
        $title = addslashes($data["title"]); 
        $content = addslashes($data["content"]);
        
        $sql = "INSERT INTO {$this->tblPrefix} (name, {$this->attrs}) VALUES ('{$name}', '{$title}', '{$content}')";
        if (!($r = my_mysql_query($sql)))
            return False;
            
        $lid = mysql_insert_id();
        
        $sql = "INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action) VALUES ({$lid}, {$userId}, 'created')";
        if (!my_mysql_query($sql))
            return False;
        
        return $lid;
    }

    function Update($id, $data, $user = 1)
    {
        if (!($id = (int) $id))
            return False;
            
        $title = addslashes($data["title"]); 
        $content = addslashes($data["content"]);
        $user = (int) $user;
        
        $change = "updated"; // how big change it was?
        
        $sql = "UPDATE {$this->tblPrefix} SET title = '{$title}', content = '{$content}' WHERE id = {$id} LIMIT 1";
        if (!my_mysql_query($sql))
            return False;
        
        if (!my_mysql_query("INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action) VALUES ({$id}, {$user}, '{$change}')"))
            return False;
        
        return True;
    }
}

class OptionsData extends DocsData
{
    function OptionsData()
    {
        $this->tblPrefix = OPTIONSTABLEPREFIX;
        $this->detlistattrs = array(
            'id' => 'id',
            'name' => 'name',
            'descr' => 'LENGTH(description)',
            'active' => 'active'
        );
        $this->detlistdescs = array(
            'id' => 'id',
            'name' => 'Name',
            'descr' => 'Description length',
            'active' => 'Active'
        );
        $this->attrs = "description, args, argsdesc, flags";
        $this->foreignkey = "id_option";
    }
    
    function GetFlagsNum($id)
    {
        $id = (int) $id;
        $sql = "SELECT floor(flags) FROM {$this->tblPrefix} WHERE id = {$id} LIMIT 1";
        if (!$r = my_mysql_query($sql))
            return False;
        
        return mysql_result($r, 0);
    }
    
    function GetOption($id)
    {
        $r = $this->GetContent($id);
        $r["flagnames"] = $r["flags"];
        $r["flags"] = $this->GetFlagsNum($id);
        return $r;
    }
    
    function Add($data, $userId = 1)
    {
        $name = addslashes($data["name"]);
        $description = addslashes($data["description"]);
        $args = addslashes($data["args"]);
        $argsdesc = addslashes($data["argsdesc"]);
        $flags = $data["flags"];
        $userId = (int) $userId;
        
        $sql .= "'{$name}', '{$description}', '{$args}', '{$argsdesc}', '{$flags}'";
        
        return $this->AddBase($name, $sql, $userId);
    }
    
    function Update($id, $data, $user)
    {
        if (!($id = (int) $id))
            return False;
            
        $description = addslashes($data["description"]);
        $args = addslashes($data["args"]);
        $argsdesc = addslashes($data["argsdesc"]);
        $flags = addslashes($data["flags"]);
        $user = (int) $user;
        
        $change = "updated"; // todo: proper check (look e.g. at 'args')
        
        $sql = "UPDATE {$this->tblPrefix} SET description = '{$description}', args = '{$args}', argsdesc = '{$argsdesc}', flags = '{$flags}' WHERE id = {$id} LIMIT 1";
        if (!my_mysql_query($sql))
            return False;
        
        if (!my_mysql_query("INSERT INTO {$this->tblPrefix}_history ({$this->foreignkey}, id_user, action) VALUES ({$id}, {$user}, '{$change}')"))
            return False;
        
        return True;
    }

    function GetFlagsList()
    {
        static $flg = array();  // cache
        
        if (count($flg))
            return $flg;
        
        $sql = "DESCRIBE {$this->tblPrefix} flags";
        if (!$r = my_mysql_query($sql))
            return False;
        
        $d = mysql_fetch_assoc($r);
        $set = $d["Type"];
        if (!strlen($set))
            return False;
        
        ereg("^set\((.*)\)$", $set, $enums);
        $flags = explode(",", $enums[1]);
        $i = 1;
        foreach ($flags as $flag)
        {
            $flg[$i] = trim($flag, "'\" ");
            $i *= 2;
        }

        return $flg;
    }
    
    function GetFlagsNames($setflags)
    {
        $avflags = $this->GetFlagsList();
        $flags = array();
        
        foreach ($avflags as $flagnum => $flagname)
        {   
            $flags[$flagnum]["name"] = $flagname;
            $flags[$flagnum]["set"] = $flagnum & $setflags;
        }
        
        return $flags;
    }

}


class BaseGroupsData
/** Abstract class. Common functions and structures for variable groups and major groups.
    Major group contains more groups. Variables are assigned to groups. */
{
    var $table;
    var $listorder;
    
    function GetId($name)
    {
        $name = addslashes($name);
        $sql = "SELECT id FROM {$this->table} WHERE name = '{$name}' LIMIT 1";
        if (!($r = my_mysql_query($sql)) || !mysql_num_rows($r))
            return False;
        
        return mysql_result($r, 0);        
    }
    
    function GetTitle($id)
    {
        $id = (int) $id;
        $sql = "SELECT title FROM {$this->table} WHERE id = {$id} LIMIT 1";
        if (!($r = my_mysql_query($sql)) || !mysql_num_rows($r))
            return False;
        
        return mysql_result($r, 0);
    }
    
    function Rename($old_id, $newname)
    {
        $title = addslashes($newname);
        $name = addslashes(IdSafe($newname));
        
        $old_id = (int) $old_id;
        
        if (!strlen($newname) || !$old_id)
            return False;
        
        $sql = "UPDATE {$this->table} SET title = '{$title}', name = '{$name}' WHERE id = {$old_id} LIMIT 1";
        if (!my_mysql_query($sql))
            return False;
        
        return True;
    }
    
    function GetIds()
    /* we use this slow query function only where it saves a lot of small quick queries */
    {
        $sql = "SELECT id, name FROM {$this->table} WHERE 1";
        if (!($r = my_mysql_query($sql)))
            return False;
            
        $ret = array();
        while ($d = mysql_fetch_assoc($r))
            $ret[$d["name"]] = $d["id"];
        
        return $ret;
    }

    
    function RemoveBase($gr_id)
    {   
        if (!($gr_id = (int) $gr_id))
            return False;
        
        $sql = "DELETE FROM {$this->table} WHERE id = {$gr_id} LIMIT 1";
        if (!my_mysql_query($sql))
            return False;
        
        return True;        
    }
    
    function Remove($gr_id)
    {
        return $this->RemoveBase($gr_id);
    }
    
    function AddBase($title)
    {
        $name = addslashes(IdSafe($title));
        $title = addslashes($title);
        
        $sql = "INSERT INTO {$this->table} (name, title) VALUES ('{$name}', '{$title}')";
        if (!my_mysql_query($sql))
            return False;
        
        return mysql_insert_id();
    }
    
    function GetList($listorder = 0, $where = 1)
    {   
        $listorder = $listorder ? "title ASC" : $this->listorder;
        // warning: $where is not addslashed, not clean object solution, expects valid input
        $sql = "SELECT * FROM {$this->table} WHERE ({$where}) ORDER BY {$listorder}"; // tables shouldn't have more than 30 rows, each table only about 4 fields, so it's ok
        if (!($d = my_mysql_query($sql)))
            return False;
        
        if (!mysql_num_rows($d))
            return False;
        
        $ret = array();
        while ($r = mysql_fetch_assoc($d))
            $ret[] = $r;
        
        return $ret;
    }
    
    function Add($title)
    {
        return $this->AddBase($title);
    }
    
    function GetIDAssocList()
    {
        $sql = "SELECT * FROM {$this->table} WHERE active > 0";
        if (!$r = my_mysql_query($sql))
            return False;
        
        $ret = array();
        while ($d = mysql_fetch_assoc($r))
            $ret[$d["id"]] = $d;
        
        return $ret;
    }
}

class GroupsData extends BaseGroupsData
{
    function GroupsData()
    {
        $this->table = "variables_groups";
        $this->listorder = "id_mgroup ASC, title ASC";
    }
    
    function Assign($id, $to)
    {
        $id = (int) $id; $to = (int) $to;
        $sql = "UPDATE {$this->table} SET id_mgroup = {$to} WHERE id = {$id} LIMIT 1";
        if (!my_mysql_query($sql))
            return False;
        
        return True;
    }
    
    function Add($title, $mgroup_id)
    {
        if (!($group_id = $this->AddBase($title)))
            return False;
        
        if (!$this->Assign($group_id, $mgroup_id))
            return False;
        
        return $group_id;
    }
    
    function GetGroupedList()
    {
        $sql = "SELECT * FROM {$this->table} WHERE active > 0 ORDER BY id_mgroup ASC, title ASC";
        if (!$r = my_mysql_query($sql))
            return False;
        
        $ret = array();
        while ($d = mysql_fetch_assoc($r))
            $ret[$d["id_mgroup"]][] = $d;
        
        return $ret;
    }
    
    function GetMGroupID($id)
    {
        $id = (int) $id;
        $sql = "SELECT id_mgroup FROM {$this->table} WHERE id = {$id} LIMIT 1";
        if ((!$r = my_mysql_query($sql)) || (!mysql_num_rows($r)))
            return False;
        
        return mysql_result($r, 0);
    }
}

class MGroupsData extends BaseGroupsData
{
    var $groupstable;
    
    function MGroupsData()
    {
        $this->table = VARMGROUPSTABLE;
        $this->groupstable = VARGROUPSTABLE;
        $this->listorder = "title ASC";
    }
    
    function Remove($gr_id, $move_asgn_to)
    {
        $gr_id = (int) $gr_id; $move_asgn_to = (int) $move_asgn_to;
        if (!$this->RemoveBase($gr_id))
            return False;
            
        $sql = "UPDATE {$this->groupstable} SET id_mgroup = {$move_asgn_to} WHERE id_mgroup = {$gr_id}";
        if (!my_mysql_query($sql))
            return False;
        
        return True;
    }
}

class SupportData
{
    var $tablebuilds;
    var $tablesupport;
    
    function SupportData()
    {
        $this->tablebuilds = "builds";
        $this->tablesupport = VARSUPPORTTABLE;
    }
    
    function ClearBuildSupport($id_build)
    {
        $id_build = (int) $id_build;
        $sql = "DELETE FROM variables_support WHERE id_build = {$id_build}";
        if (!my_mysql_query($sql))
            return False;
        
        return True;
    }
    
    function VarsSetDefaults($data)
    {
        $sql = "INSERT INTO {$this->tablesupport} (id_variable, id_build, default_value) VALUES ";
        $c = 0;
        foreach ($data as $d)
        {   
            $id_variable = (int) $d["id_variable"];
            $id_build = (int) $d["id_build"];
            $default = addslashes($d["default"]);
        
            if (!$id_variable || !$id_build)
                continue;
        
            if ($c++)
                $sql .= ", ";

            $sql .= " ({$id_variable}, {$id_build}, '{$default}')";
        }
        if (!my_mysql_query($sql))
            return False;

        return True;
    }
    
    function GetBuilds()
    {
        $sql = "SELECT id, abbr, shortname, title FROM builds WHERE 1";
        if (!($r = my_mysql_query($sql)))
            return False;
        
        $ret = array();
        
        while ($d = mysql_fetch_assoc($r))
            $ret[] = $d;    // zzz .. we don't want the last 'empty' line so i can't put it inside the condition
    
        return $ret;
    }
}

class IndexData
{
    var $tableindex;
    
    function IndexData()
    {
        $this->tableindex = INDEXTABLE; 
    }
    
    function FetchList(&$buffer)
    {
        $sql = "SELECT name, itype, desc1, desc2, desc3 FROM {$this->tableindex} ORDER BY name, itype ASC";
        if (!($r = my_mysql_query($sql)))
            return False;
        
        while ($d = mysql_fetch_assoc($r))
        {
            $new = array();
            $new["type"] = $d["itype"];
            $l1 = strlen($d["desc1"]);
            $l2 = strlen($d["desc2"]);
            $l3 = strlen($d["desc3"]);
            
            if ($l1 > $l2)
            {
                if ($l3 > $l1)
                    $desc = $d["desc3"];
                else
                    $desc = $d["desc1"];
            } else {
                if ($l2 > $l3)
                    $desc = $d["desc2"];
                else
                    $desc = $d["desc3"];
            }
            $new["desc"] = $desc;
            $buffer[$d["name"]] = $new; 
        }
        
        return True; 
    }
    
    function Refresh()
    {
        $sql = "DELETE FROM {$this->tableindex} WHERE 1";
        if (!my_mysql_query($sql))
            return False;
    
        $sql  = "INSERT INTO {$this->tableindex} (name, itype, desc1, desc2) ";
        $sql .= "SELECT name, \"variable\", description, remarks FROM ".VARIABLESTABLEPREFIX;
        if (!my_mysql_query($sql))
            return False;
        
        $sql  = "INSERT INTO {$this->tableindex} (name, itype, desc1, desc2, desc3) ";
        $sql .= "SELECT name, \"command\", description, remarks, syntax FROM ".COMMANDSTABLEPREFIX;
        if (!my_mysql_query($sql))
            return False;
        
        $sql  = "INSERT INTO {$this->tableindex} (name, itype, desc1, desc2) ";
        $sql .= "SELECT name, \"command-line option\", description, argsdesc FROM ".OPTIONSTABLEPREFIX;
        if (!my_mysql_query($sql))
            return False;
        
        return True;
    }
}

?>
