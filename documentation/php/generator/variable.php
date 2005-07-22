<?php echo('<?xml version="1.0" encoding="utf-8"?>
'); ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" 
        "http://www.w3.org/TR/2000/REC-xhtml1-20000126/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
 <head>
  <meta http-equiv="content-type" content="text/html; charset=utf-8" />
  <title>ezQuake Variables Description Generator</title>
  <style type="text/css">
  <!--
  input, textarea {
    margin-bottom: 0.5em;
    margin-left: 0.5em;
  }
  label {
    margin-left: 0.5em;
  }
  thead tr td {
    padding-left: 0.5em;
  }
  fieldset {
    margin: 1em;
  }
  table tbody input { margin: 0 0 0 0.5em; }
  #details {
    position: relative;
  }
  #details fieldset {
    position: relative;
    top: 0;
    left: 0;
  }
  -->
  </style>
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
 </head>
 <body onload="Init()">
<h1>ezQuake Variables Description Generator</h1>
<p><button onclick="FillIn()">Example fill out</button></p>
<form action="genvar.php" method="post">
<fieldset>
  <legend>Variable information</legend>
  <label for="v_name">Name:<br /><input type="text" name="v_name" id="v_name" value="" size="24" /></label><br />
  <label for="v_desc">Description:<br /><textarea name="v_desc" id="v_desc" rows="5" cols="50"></textarea></label><br />
  <fieldset>
    <legend>Type</legend>
    <label><input type="radio" name="v_type" id="v_type_string" value="string" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> String</label><br />
    <label><input type="radio" name="v_type" id="v_type_integer" value="integer" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> Integer</label><br />
    <label><input type="radio" name="v_type" id="v_type_float" value="float" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> Float</label><br />
    <label><input type="radio" name="v_type" id="v_type_boolean" value="boolean" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> Boolean</label><br />
    <label><input type="radio" name="v_type" id="v_type_enum" value="enum" onchange="SmartHide()" onmouseup="SmartHide()" onclick="SmartHide()" /> Enum</label><br />
  </fieldset>
  <div id="details">
    <fieldset id="v_f_string">
      <legend>String</legend>
      <label>Closer description: <input type="text" name="v_string" id="v_string" size="60" /></label>
    </fieldset>
    <fieldset id="v_f_integer">
      <legend>Integer</legend>
      <label>Closer description: <input type="text" name="v_integer" id="v_integer" size="60" /></label>
    </fieldset>
    <fieldset id="v_f_float">
      <legend>Float</legend>
      <label>Closer description: <input type="text" name="v_float" id="v_float" size="60" /></label>
    </fieldset>
    <fieldset id="v_f_boolean">
      <legend>Boolean</legend>
      <table>
        <thead>
        <tr><td>value</td><td>description</td></tr>
      </thead>
      <tbody>
        <tr><td>0 = False</td><td><input type="text" name="v_boolean_false" id="v_boolean_false" size="60" /></td></tr>
        <tr><td>1 = True</td><td><input type="text" name="v_boolean_true" id="v_boolean_true" size="60" /></td></tr>
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
        <tr><td><input type="text" name="v_arg_0_name" id="v_arg_0_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_0_desc" id="v_arg_0_desc"></textarea></td></tr>
        <tr><td><input type="text" name="v_arg_1_name" id="v_arg_1_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_1_desc" id="v_arg_1_desc"></textarea></td></tr>
        <tr><td><input type="text" name="v_arg_2_name" id="v_arg_2_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_2_desc" id="v_arg_2_desc"></textarea></td></tr>
        <tr><td><input type="text" name="v_arg_3_name" id="v_arg_3_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_3_desc" id="v_arg_3_desc"></textarea></td></tr>
        <tr><td><input type="text" name="v_arg_4_name" id="v_arg_4_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_4_desc" id="v_arg_4_desc"></textarea></td></tr>
        <tr><td><input type="text" name="v_arg_5_name" id="v_arg_5_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_5_desc" id="v_arg_5_desc"></textarea></td></tr>
        <tr><td><input type="text" name="v_arg_6_name" id="v_arg_6_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_6_desc" id="v_arg_6_desc"></textarea></td></tr>
        <tr><td><input type="text" name="v_arg_7_name" id="v_arg_7_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_7_desc" id="v_arg_7_desc"></textarea></td></tr>
        <tr><td><input type="text" name="v_arg_8_name" id="v_arg_8_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_8_desc" id="v_arg_8_desc"></textarea></td></tr>
        <tr><td><input type="text" name="v_arg_9_name" id="v_arg_9_name" size="8" /></td><td><textarea cols="50" rows="3" name="v_arg_9_desc" id="v_arg_9_desc"></textarea></td></tr>
      </tbody>
      </table>
    </fieldset>
  </div>
  <label for="v_remarks">Remarks:<br /><textarea name="v_remarks" id="v_remarks" cols="50" rows="5"></textarea></label><br />
</fieldset>
<p><button type="reset" onclick="Init()">Reset</button> <button type="submit">Submit</button></p>
</form>

 </body>
</html>

