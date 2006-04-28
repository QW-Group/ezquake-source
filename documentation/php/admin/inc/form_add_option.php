  <script type="text/javascript">
  <!--
  function SetValue (elem, val) {
    var x = document.getElementById(elem);
    x.value = val;
  }
  -->
  </script>

<?php 
    if (strlen($name))
    {
?>

<ul><li><a href="#edit">Edit</a></li><li><a href="#rename">Rename</a></li><li><a href="#remove">Remove</a></li></ul>

<h2 id="edit">Edit option</h2>
<p>Note: If you change option's name, it will be saved with a new name but the old one will remain active.<br />If you only want to <a href="#rename">rename</a> the option, use the <a href="#rename">form below</a>. This form is for creating new option or editing properties of existing ones.</p>



<?php
    }
    else
    {
?>
<h2>Add new option</h2>
<p>Warning: If you type name of already existing option, it's properties will be overwritten without asking for confirmation.</p>
<p>Always use name without the leading dash '-'.</p>
<?php
    }
?>
<form action="index.php" method="post" id="mainform">
<div><input type="hidden" name="action" id="action" value="addopt" /></div>
<fieldset>
  <legend>Option informations</legend>
  <label for="o_name">Name:<br /><input type="text" name="o_name" id="o_name" value="<?=$name?>" /></label><br />
  <label for="o_description">Description:<br /><textarea name="o_description" id="o_description" rows="8" cols="60"><?=$description?></textarea></label><br />
  <label for="o_args">Arguments:<br /><input type="text" name="o_args" id="o_args" value="<?=$args?>" size="60" /></label><br />
  <label for="o_argsdesc">Arguments description:<br /><textarea name="o_argsdesc" id="o_argsdesc" rows="8" cols="60"><?=$argsdesc?></textarea></label><br />
  <label>Flags:</label><ul>
  <?=$flags?>
  </ul>
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

<?php
    if (strlen($name)) {
?>

<h2 id="rename">Rename option</h2>
<form action="index.php" method="post" id="renameform">
<div><input type="hidden" name="o_oldname" value="<?=$name?>" /><input type="hidden" name="action" value="renameoption" /></div>
<p><label>Rename <?=$name?> to: <input type="text" name="o_newname" value="" /></label></p>
<div><input type="submit" value="Submit" /></div>
<p>Make sure there's no option with the name you are renaming this option to.</p>
</form>

<h2 id="remove">Remove option</h2>
<form action="index.php" method="post" id="removeform">
<div><input type="hidden" name="o_name" value="<?=$name?>" /><input type="hidden" name="action" value="removeoption" /></div>
<p><label><input type="checkbox" name="o_physremove" /> Physically remove *</label></p>
<div><input type="submit" value="Remove" /></div>
<p>* Use 'Physically remove' only for invalid options; not for outdated or renamed ones.</p>
</form>

<?php	
    }
?>
