  <script type="text/javascript">
  <!--
  function SetValue (elem, val) {
    var x = document.getElementById(elem);
    x.value = val;
  }
  function Check (elem) {
    var x = document.getElementById(elem);
    x.checked = 1;
  }
  function Checked (elem) {
    var x = document.getElementById(elem);
    return x.checked;
  }
  function TurnVisibility (elem, visible) {
    var x = document.getElementById(elem);
    if(visible) {
      x.style.display = "";
    } else {
      x.style.display = "none";
    }
  }
  function SmartHide() {
    TurnVisibility('v_f_string', Checked('v_type_string'));
    TurnVisibility('v_f_integer', Checked('v_type_integer'));
    TurnVisibility('v_f_float', Checked('v_type_float'));
    TurnVisibility('v_f_boolean', Checked('v_type_boolean'));
    TurnVisibility('v_f_enum', Checked('v_type_enum'));
  }
  function FillIn() {
    SetValue('v_name', 'fps_explosiontype');
    SetValue('v_desc', 'Sets explosion type used for rocket and grenade explosions.');
    SetValue('v_arg_0_name', 'off');
    SetValue('v_arg_0_desc', 'disable explosion effect');
    SetValue('v_arg_1_name', 'normal');
    SetValue('v_arg_1_desc', 'use regular quake explosions');
    SetValue('v_arg_2_name', 'fire');
    SetValue('v_arg_2_desc', 'use regular quake explosion with no particles');
    SetValue('v_arg_3_name', 'tarbary');
    SetValue('v_arg_3_desc', 'use tarbary effect');
    
    SetValue('v_remarks', 'Note that you set explosion type for all rocket and grenade explosions, not only your own. \
		Now we added another line of remarks. There are line breaks and spaces. They should be \
		ignored when rendering. The goal of this description is to test, if this actually works.');
		Check('v_type_enum');
		SmartHide();
  }
  function Init() {
    Check('v_type_string');
    SmartHide();
  }
  -->
  </script>

<?php 
    if (strlen($name))
    {
?>

<ul><li><a href="#edit">Edit</a></li><li><a href="#rename">Rename</a></li><li><a href="#remove">Remove</a></li></ul>

<h2 id="edit">Edit variable</h2>
<p>Note: If you change variable's name, it will be saved with a new name but the old one will remain active.<br />If you only want to <a href="#rename">rename</a> the variable, use the <a href="#rename">form below</a>. This form is for creating new variables or editing properties of existing ones.</p>



<?php
    }
    else
    {
?>
<h2>Add new variable</h2>
<p>Warning: If you type name of already existing variable, it's properties will be overwritten without asking for confirmation.</p>
<?php
    }
?>

<p><button onclick="FillIn()">Example fill out</button></p>
<form action="index.php" method="post">
<div><input type="hidden" name="action" id="action" value="addvar" /></div>
<fieldset>
  <legend>Variable information</legend>
  <label for="v_name">Name:<br /><input type="text" name="v_name" id="v_name" value="<?=$name?>" size="24" /></label><br />
  <label for="v_desc">Description:<br /><textarea name="v_desc" id="v_desc" rows="5" cols="50"><?=$description?></textarea></label><br />
  <fieldset>
    <legend>Type</legend>
    <label><input type="radio" <?=$strchck?> name="v_type" id="v_type_string" value="string" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> String</label><br />
    <label><input type="radio" <?=$intchck?> name="v_type" id="v_type_integer" value="integer" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> Integer</label><br />
    <label><input type="radio" <?=$fltchck?> name="v_type" id="v_type_float" value="float" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> Float</label><br />
    <label><input type="radio" <?=$blnchck?> name="v_type" id="v_type_boolean" value="boolean" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> Boolean</label><br />
    <label><input type="radio" <?=$enuchck?> name="v_type" id="v_type_enum" value="enum" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> Enum</label><br />
    <div onload="SmartHide()"></div>
  </fieldset>
  <div id="details">
    <fieldset id="v_f_string">
      <legend>String</legend>
      <label>Closer description: <input type="text" name="v_string" id="v_string" size="60" value="<?=$strdesc?>" /></label>
    </fieldset>
    <fieldset id="v_f_integer">
      <legend>Integer</legend>
      <label>Closer description: <input type="text" name="v_integer" id="v_integer" size="60" value="<?=$intdesc?>" /></label>
    </fieldset>
    <fieldset id="v_f_float">
      <legend>Float</legend>
      <label>Closer description: <input type="text" name="v_float" id="v_float" size="60" value="<?=$fltdesc?>" /></label>
    </fieldset>
    <fieldset id="v_f_boolean">
      <legend>Boolean</legend>
      <table>
        <thead>
        <tr><td>value</td><td>description</td></tr>
      </thead>
      <tbody>
        <tr><td>0 = False</td><td><input type="text" name="v_boolean_false" id="v_boolean_false" size="60" value="<?=$false?>" /></td></tr>
        <tr><td>1 = True</td><td><input type="text" name="v_boolean_true" id="v_boolean_true" size="60" value="<?=$true?>" /></td></tr>
      </tbody>
      </table>
    </fieldset>
    <fieldset id="v_f_enum">
      <legend>Enum</legend>
      <table>
      <thead>
        <tr><td>value</td><td>description</td></tr>
      </thead>
      <tbody>
<?php
    for ($i = 0; $i <= MAXVARENUMS; $i++)
    {
        echo '<tr><td><input type="text" name="v_val_'.$i.'_name" id="v_val_'.$i.'_name" size="8" value="'.$valnames[$i].'" /></td><td><textarea cols="60" rows="2" name="v_val_'.$i.'_desc" id="v_val_'.$i.'_desc">'.$valdescs[$i].'</textarea></td></tr>';
    }
?>
      </tbody>
      </table>
    </fieldset>
  </div>
  <label for="v_remarks">Remarks:<br /><textarea name="v_remarks" id="v_remarks" cols="50" rows="5"><?=$remarks?></textarea></label><br />
</fieldset>
<p><button type="reset" onclick="Init()">Reset</button> <button type="submit">Submit</button></p>
</form>

<?php
    if (strlen($name)) {
?>

<h2 id="rename">Rename variable</h2>
<form action="index.php" method="post" id="renameform">
<div><input type="hidden" name="v_oldname" value="<?=$name?>" /><input type="hidden" name="action" value="renamevariable" /></div>
<p><label>Rename <?=$name?> to: <input type="text" name="v_newname" value="" /></label></p>
<div><input type="submit" value="Submit" /></div>
<p>Make sure there's no variable with the name you are renaming this command to.</p>
</form>

<h2 id="remove">Remove variable</h2>
<form action="index.php" method="post" id="removeform">
<div><input type="hidden" name="v_name" value="<?=$name?>" /><input type="hidden" name="action" value="removevariable" /></div>
<p><label><input type="checkbox" name="v_physremove" /> Physically remove *</label></p>
<div><input type="submit" value="Remove" /></div>
<p>* Use 'Physically remove' only for invalid variable; not for outdated or renamed ones.</p>
</form>

<?php	
    }
?>
