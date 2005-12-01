<?php

    require_once("../inc/mysql_access.php");
    require_once("../inc/common.php");
    require_once("../inc/common-xmlparse.php");
    require_once("../inc/common-txtparse.php");
    require_once("inc/session.php");
    require_once("inc/forms_commands.php");

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

    include("inc/html_head.php");

    if (!$session->access)
    {   // user's not logged in
       if (!$loginSuccess)
            echo("<p>Wrong login informations.</p>");
            
        $action = "loginform";
        echo("<p>Cookies must be enabled.</p>");
        include("inc/forms_user.php");
    }
    else
    {
        $cmdForms = new CommandsForms;
        $varForms = new VariablesForms;
        $manForms = new ManualsForms;
        $grpForms = new GroupsForms;
        $supForms = new SupportForms;
        
        echo '<div class="menus">';
        if ($session->access > 1)
            include("inc/menu_headadmin.php");
            
      	include("inc/menu_user.php");
        echo '<h3>'.$session->userName.'</h3>';
        echo '</div>';
        
        echo '<div class="main">';
        
        if ($session->access > 1)
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
        }
        
    	switch ($action)
    	{
            case "addcmdform": $cmdForms->FormCommand(); break;
            case "addvarform": $varForms->FormVariable(); break;
            case "addmanform": $manForms->FormManual(); break;
            case "addcmd": $cmdForms->ReadAddCmd($session->userId); break;
            case "addvar": $varForms->ReadAddVar($session->userId); break;
            case "addman": $manForms->ReadAddMan($session->userId); break;
            case "renamecommand":
                $cmdForms->Rename($_REQUEST["c_oldname"], $_REQUEST["c_newname"], $session->userId);
                break;
            case "removecommand":
                $cmdForms->Remove($_REQUEST["c_name"], $_REQUEST["c_physremove"], $session->userId);
                break;
            case "renamevariable":
                $varForms->Rename($_REQUEST["v_oldname"], $_REQUEST["v_newname"], $session->userId);
                break;
            case "removevariable":
                $varForms->Remove($_REQUEST["v_name"], $_REQUEST["v_physremove"], $session->userId);
                break;
            case "renamemanual":
                $manForms->Rename($_REQUEST["m_oldname"], $_REQUEST["m_newname"], $session->userId);
                break;
            case "removemanual":
                $manForms->Remove($_REQUEST["m_name"], $_REQUEST["m_physremove"], $session->userId);
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
                break;
            case "editvariables":
                $varForms->PrintList((int) $_REQUEST["detailed"], $_REQUEST["orderby"], $_REQUEST["order"]);
                break;
            case "editvariable":
                $varForms->FormVariable($_REQUEST["id"]);
                break;
            case "editmanuals":
                $manForms->PrintList((int) $_REQUEST["detailed"], $_REQUEST["orderby"], $_REQUEST["order"]);
                break;
            case "editmanual":
                $manForms->FormManual($_REQUEST["id"]);
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
            // list users
            case "listusers": $session->ListUsers(); break;
        }
        
        echo '</div>';
    }

    include("inc/html_foot.php");
    
?>
