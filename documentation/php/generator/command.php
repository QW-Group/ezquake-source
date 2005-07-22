<?php echo('<?xml version="1.0" encoding="utf-8"?>
'); ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" 
        "http://www.w3.org/TR/2000/REC-xhtml1-20000126/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
 <head>
  <meta http-equiv="content-type" content="text/html; charset=utf-8" />
  <title>ezQuake Commands Description Generator</title>
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
  -->
  </style>
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
 </head>
 <body>
<h1>ezQuake Commands Description Generator</h1>
<p><button onclick="FillIn()">Example fill out</button></p>
<form action="gencmd.php" method="post" id="mainform">
<fieldset>
  <legend>Command information</legend>
  <label for="c_name">Name:<br /><input type="text" name="c_name" id="c_name" value="" /></label><br />
  <label for="c_desc">Description:<br /><textarea name="c_desc" id="c_desc" rows="5" cols="50"></textarea></label><br />
  <label for="c_syntax">Syntax:<br /><input type="text" name="c_syntax" id="c_syntax" value="" size="60" /></label><br />
  <fieldset>
    <legend>Arguments</legend>
    <table>
    <thead>
      <tr><td>name</td><td>description</td></tr>
    </thead>
    <tbody>
      <tr><td><input type="text" name="c_arg_0_name" id="c_arg_0_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_0_desc" id="c_arg_0_desc"></textarea></td></tr>
      <tr><td><input type="text" name="c_arg_1_name" id="c_arg_1_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_1_desc" id="c_arg_1_desc"></textarea></td></tr>
      <tr><td><input type="text" name="c_arg_2_name" id="c_arg_2_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_2_desc" id="c_arg_2_desc"></textarea></td></tr>
      <tr><td><input type="text" name="c_arg_3_name" id="c_arg_3_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_3_desc" id="c_arg_3_desc"></textarea></td></tr>
      <tr><td><input type="text" name="c_arg_4_name" id="c_arg_4_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_4_desc" id="c_arg_4_desc"></textarea></td></tr>
      <tr><td><input type="text" name="c_arg_5_name" id="c_arg_5_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_5_desc" id="c_arg_5_desc"></textarea></td></tr>
      <tr><td><input type="text" name="c_arg_6_name" id="c_arg_6_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_6_desc" id="c_arg_6_desc"></textarea></td></tr>
      <tr><td><input type="text" name="c_arg_7_name" id="c_arg_7_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_7_desc" id="c_arg_7_desc"></textarea></td></tr>
      <tr><td><input type="text" name="c_arg_8_name" id="c_arg_8_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_8_desc" id="c_arg_8_desc"></textarea></td></tr>
      <tr><td><input type="text" name="c_arg_9_name" id="c_arg_9_name" size="8" /></td><td><textarea cols="50" rows="3" name="c_arg_9_desc" id="c_arg_9_desc"></textarea></td></tr>
    </tbody>
    </table>
  </fieldset>
  <label for="c_remarks">Remarks:<br /><textarea name="c_remarks" id="c_remarks" cols="50" rows="5"></textarea></label><br />
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

 </body>
</html>

