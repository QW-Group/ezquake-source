  <script type="text/javascript">
  <!--
  function SetValue (elem, val) {
    var x = document.getElementById(elem);
    x.value = val;
  }
  function FillIn() {
    SetValue('c_name', 'ignore');
    SetValue('c_desc', 'Ignores given user, which prevents you from getting his/her chat messages.');
    SetValue('c_syntax', '[user]');
    SetValue('c_arg_0_name', 'user');
    SetValue('c_arg_0_desc', 'user which should be ignored. You can give user nickname (ie. lakerman), id (ie. 4324) or number (ie. #5). If you skip this argument, current ignore list will be printed');
    SetValue('c_remarks', 'Ignoring a user lasts until map changes or until you explicitely unignore \
 if you want to ignore user for longer time, you have to repeat this command after every map change.');
  }
  -->
  </script>

<?php 
    if (strlen($name))
    {
?>

<ul><li><a href="#edit">Edit</a></li><li><a href="#rename">Rename</a></li><li><a href="#remove">Remove</a></li></ul>

<h2 id="edit">Edit command</h2>
<p>Note: If you change command's name, it will be saved with a new name but the old one will remain active.<br />If you only want to <a href="#rename">rename</a> the command, use the <a href="#rename">form below</a>. This form is for creating new commands or editing properties of existing ones.</p>



<?php
    }
    else
    {
?>
<h2>Add new command</h2>
<p>Warning: If you type name of already existing command, it's properties will be overwritten without asking for confirmation.</p>
<?php
    }
?>
<p><button onclick="FillIn()">Example fill out</button></p>
<form action="index.php" method="post" id="mainform">
<div><input type="hidden" name="action" id="action" value="addcmd" /></div>
<fieldset>
  <legend>Command informations</legend>
  <label for="c_name">Name:<br /><input type="text" name="c_name" id="c_name" value="<?=$name?>" /></label><br />
  <label for="c_desc">Description:<br /><textarea name="c_desc" id="c_desc" rows="8" cols="60"><?=$description?></textarea></label><br />
  <label for="c_syntax">Syntax:<br /><input type="text" name="c_syntax" id="c_syntax" value="<?=$syntax?>" size="60" /></label><br />
  <fieldset>
    <legend>Arguments</legend>
    <table>
    <thead>
      <tr><td>name</td><td>description</td></tr>
    </thead>
    <tbody>
    <?php
        for ($i = 0; $i <= MAXCMDARGS; $i++)
        {
            echo '<tr><td><input type="text" name="c_arg_'.$i.'_name" id="c_arg_'.$i.'_name" size="8" value="'.$argnames[$i].'" /></td><td><textarea cols="50" rows="3" name="c_arg_'.$i.'_desc" id="c_arg_'.$i.'_desc">'.$argdescs[$i].'</textarea></td></tr>';
        }
    ?>
    </tbody>
    </table>
  </fieldset>
  <label for="c_remarks">Remarks:<br /><textarea name="c_remarks" id="c_remarks" cols="50" rows="5"><?=$remarks?></textarea></label><br />
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

<?php
    if (strlen($name)) {
?>

<h2 id="rename">Rename command</h2>
<form action="index.php" method="post" id="renameform">
<div><input type="hidden" name="c_oldname" value="<?=$name?>" /><input type="hidden" name="action" value="renamecommand" /></div>
<p><label>Rename <?=$name?> to: <input type="text" name="c_newname" value="" /></label></p>
<div><input type="submit" value="Submit" /></div>
<p>Make sure there's no command with the name you are renaming this command to.</p>
</form>

<h2 id="remove">Remove command</h2>
<form action="index.php" method="post" id="removeform">
<div><input type="hidden" name="c_name" value="<?=$name?>" /><input type="hidden" name="action" value="removecommand" /></div>
<p><label><input type="checkbox" name="c_physremove" /> Physically remove *</label></p>
<div><input type="submit" value="Remove" /></div>
<p>* Use 'Physically remove' only for invalid commands; not for outdated or renamed ones.</p>
</form>

<?php	
    }
?>
