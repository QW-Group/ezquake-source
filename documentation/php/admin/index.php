<?php

/**
 * ezQuake Docs administration tool main script
 */

    require_once("../settings.php");            // text constants, documentation system settings
    require_once("../inc/mysql_access.php");    // create mysql connection resource
    require_once("../inc/common.php");          // text and html processing functions
    require_once("../inc/common-xmlparse.php"); // xml-parsing functions
    require_once("../inc/common-txtparse.php"); // quakeworld configuration files parsing functions
    require_once("../inc/describe.php");        // mysql->xml export functions
    require_once("../inc/downloader.php");      // forwards files from /tmp/persistent/ezquake/...
    require_once("inc/session.php");            // session authentication & authorization handling
    require_once("inc/forms_commands.php");     // form data processing classes

    $session = new Session;
    
    $action = trim($_REQUEST["action"]);
    if (!strlen($action)) $action = "loginform";

    if (get_magic_quotes_gpc())
    {   // we do addslashing manually
        foreach($_REQUEST as $g_var => $g_val)
            $_REQUEST["$g_var"] = stripslashes($_REQUEST["$g_var"]);
    }

    $loginSuccess = True;
    if ($action == "login") 
        $loginSuccess = $session->Login($_REQUEST["login"], md5($_REQUEST["password"]));
        
    if ($action == "logout" && $session->access)
        $session->Logout();
    
    /* Request to download archives with variables and commands describe files.
       This requst can be made without authorization so it's possible to have publicly
       working link to these files.
       But since sourceforge.net /tmp/persistent/ezquake
       isn't too secure place to store sensitive data, we won't publish this link anywhere
    */
    if (strlen($_REQUEST["download"]))  // we do want to transfer the file before any other data so it must go here
        new Downloader($_REQUEST["download"]);

    include("inc/html_head.php");

    if (!$session->access)
    {   // user's not logged in
       if (!$loginSuccess)
            echo("<p>Wrong login informations.</p>");
            
        $action = "loginform";
        echo("<p>Cookies must be enabled.</p>");
        include("inc/forms_user.php");
    }
    else    // for authorized users
    {
        // forms manipulation classes
        $cmdForms = new CommandsForms;
        $varForms = new VariablesForms;
        $manForms = new ManualsForms;
        $grpForms = new GroupsForms;
        $supForms = new SupportForms;
        $optForms = new OptionsForms;
        $indForms = new IndexForms;
        $schForms = new SearchHitsForms;
        
        echo '<div class="menus">';
        if ($session->access > 1)
            include("inc/menu_headadmin.php");
            
        if ($session->access > 1);  // for now, release access level is the same as admin access
            include("inc/menu_release.php");
        
      	include("inc/menu_user.php");
        echo '<div class="menu"><h2>User</h2><ul class="mainmenu">';
            echo '<li>Name: '.$session->userName.'</li>';
            echo '<li>Access: '.$session->access.'</li>';
            echo '<li><a href="?action=listusers">List Users</a></li>';
            echo '<li><a href="?action=changepswform">Change password</a></li>';
            echo '<li><a href="?action=logout">Log-out</a></li>';
            echo '</ul>';
        echo '</div>';
        echo '</div>';
                
        echo '<div class="main">';
        
        if ($session->access > 1)   // admin access
        {        
            switch ($action)
            {
                case "adduserform":
                case "edituserform":
                case "removeuserform":
                    include("inc/forms_user.php");
                    break;
                case "adduser":
                    $session->AddUser($_REQUEST["login"], $_REQUEST["name"], $_REQUEST["access"]);
                    break;
                case "removeuser":
                    $session->RemoveUser($_REQUEST["login"]);
                    break;
                case "edituser":
                    $session->EditUser($_REQUEST["login"], $_REQUEST["newpassword1"], $_REQUEST["newpassword2"], $_REQUEST["access"]);
                    break;
            }
        }   // end of admin access
        
        if ($session->access > 1)   // release access
        {
            echo '<pre><code>';
            switch ($action) {
                case "releasecommands": Archive("command"); break; // describe.php
                case "releasevariables": Archive("variable"); break; // describe.php
            }
            echo '</code></pre>';
        }   // end of release access
        
    	switch ($action)   // normal registered user access
    	{
            case "addcmdform": $cmdForms->FormCommand(); break;
            case "addvarform": $varForms->FormVariable(); break;
            case "addmanform": $manForms->FormManual(); break;
            case "addoptform": $optForms->FormOption(); break;
            case "addcmd": $cmdForms->ReadAddCmd($session->userId); break;
            case "addvar": $varForms->ReadAddVar($session->userId); break;
            case "addman": $manForms->ReadAddMan($session->userId); break;
            case "addopt": $optForms->ReadAddOpt($session->userId); break;
            case "renamecommand":
                $cmdForms->Rename($_REQUEST["c_oldname"], $_REQUEST["c_newname"], $session->userId);
                $indForms->Update();
                break;
            case "removecommand":
                $cmdForms->Remove($_REQUEST["c_name"], $_REQUEST["c_physremove"], $session->userId);
                $indForms->Update();
                break;
            case "renamevariable":
                $varForms->Rename($_REQUEST["v_oldname"], $_REQUEST["v_newname"], $session->userId);
                $indForms->Update();
                break;
            case "removevariable":
                $varForms->Remove($_REQUEST["v_name"], $_REQUEST["v_physremove"], $session->userId);
                $indForms->Update();
                break;
            case "renamemanual":
                $manForms->Rename($_REQUEST["m_oldname"], $_REQUEST["m_newname"], $session->userId);
                $indForms->Update();
                break;
            case "removemanual":
                $manForms->Remove($_REQUEST["m_name"], $_REQUEST["m_physremove"], $session->userId);
                break;
            case "renameoption":
                $optForms->Rename($_REQUEST["o_oldname"], $_REQUEST["o_newname"], $session->userId);
                $indForms->Update();
                break;
            case "removeoption":
                $optForms->Remove($_REQUEST["o_name"], $_REQUEST["o_physremove"], $session->userId);
                $indForms->Update();
                break;
            case "vargroups": include "inc/form_groups.php"; break;
    	    case "markuphelp": include("inc/howto_xhtml.php"); break;
            case "changepswform": include("inc/forms_user.php"); break;
            case "changepassword":
                $session->ChangePassword($_REQUEST["password"], $_REQUEST["newpassword1"], $_REQUEST["newpassword2"]);
                break;
            case "editcommands":
                $cmdForms->PrintList((int) $_REQUEST["detailed"], $_REQUEST["orderby"], $_REQUEST["order"]);
                break;
            case "editcommand":
                $cmdForms->FormCommand($_REQUEST["id"]);
                $indForms->Update();
                break;
            case "editvariables":
                $varForms->PrintList((int) $_REQUEST["detailed"], $_REQUEST["orderby"], $_REQUEST["order"]);
                break;
            case "editvariable": $varForms->FormVariable(); $indForms->Update(); break;
            case "editmanuals":
                $manForms->PrintList((int) $_REQUEST["detailed"], $_REQUEST["orderby"], $_REQUEST["order"]);
                break;
            case "editmanual":
                $manForms->FormManual($_REQUEST["id"]);
                break;
            case "editoptions":
                $optForms->PrintList((int) $_REQUEST["detailed"], $_REQUEST["orderby"], $_REQUEST["order"]);
                break;
            case "editoption":
                $optForms->FormOption($_REQUEST["id"]);
                $indForms->Update();
                break;
            // variable groups
            case "addmgroup": $grpForms->AddMG(); break;
            case "renamemgroup": $grpForms->RenameMG(); break;
            case "removemgroup": $grpForms->RemoveMG(); break;
            case "addgroup": $grpForms->AddG(); break;
            case "renamegroup": $grpForms->RenameG(); break;
            case "removegroup": $grpForms->RemoveG(); break;
            case "assigngroups": $grpForms->MultiAssign(); break;
            // variables support
            case "varsupport": include("inc/form_support.php"); break;
            case "varssetdefaults": $supForms->ReadVarsSetDefaults(); break;
            // index
            case "updateindex": $indForms->Update(); break;
            // history
            case "history":
                $varForms->PrintHistory();
                $cmdForms->PrintHistory();
                $manForms->PrintHistory();
                $optForms->PrintHistory();
                break;
            // list users
            case "listusers": $session->ListUsers(); break;
            // view top search queries
            case "searchqueries": $schForms->PrintTopQueries(); break;
        }   // end of normal user access
        
        echo '</div>';
    }   // end of any authorized user access

    include("inc/html_foot.php");
    
?>
