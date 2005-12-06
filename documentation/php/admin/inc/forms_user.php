<?php
    switch ($action)
    {
        case "adduserform":
            $newaction = "adduser";
            $heading = "Add user";
            $elements = array("login", "name", "access");
            break;
        case "edituserform":
            $newaction = "edituser";
            $heading = "Edit user";
            $elements = array("newaction", "login", "newpassword1", "newpassword2", "access");
            break;
        case "removeuserform":
            $newaction = "removeuser";
            $heading = "Remove user";
            $elements = array("login");
            break;
        case "changepswform":
            $newaction = "changepassword";
            $heading = "Change password";
            $elements = array("password", "newpassword1", "newpassword2");
            break;
        case "loginform":
        default: 
            $newaction = "login";
            $heading = "Log-in";
            $elements = array("login", "password");
            break;
    }
    
?>
<form action="index.php" method="post">
<div><input type="hidden" name="action" id="action" value="<?=$newaction?>" /></div>
<fieldset>
<legend><?=$heading?></legend>
<table>
<?php
    foreach($elements as $element)
    {
        switch ($element)
        {
            case "login":
                echo '<tr><td><label for="login">Login:</label></td><td><input type="text" size="12" name="login" id="login" /></td></tr>';
                break;
            case "name":
                echo '<tr><td><label for="name">Name:</label></td><td><input type="text" size="16" name="name" id="name" /></td></tr>';
                break;
            case "access":
                echo '<tr><td><label for="name">Access:</label></td><td><input type="text" size="3" maxlength="1" name="access" id="access" /></td></tr>';
                break;
            case "password":
                echo '<tr><td><label for="password">Password:</label></td><td><input type="password" size="16" name="password" id="password" /></td></tr>';
                break;
            case "newpassword1":
                echo '<tr><td><label for="newpsw1">New Password:</label></td><td><input type="password" size="16" name="newpassword1" id="newpassword1" /></td></tr>';
                break;
            case "newpassword2":
                echo '<tr><td><label for="newpsw2">New Password again:</label></td><td><input type="password" size="16" name="newpassword2" id="newpassword2" /></td></tr>';
                break;
        }
    }
?>
<tr><td colspan="2"><input type="submit" value="Submit" /></td></tr>
</table>
</fieldset>
</form>
