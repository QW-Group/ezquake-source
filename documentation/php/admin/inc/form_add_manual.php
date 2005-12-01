  <script type="text/javascript">
  <!--
  function SetValue (elem, val) {
    var x = document.getElementById(elem);
    x.value = val;
  }
  function FillIn() {
    SetValue('m_name', 'hud');
    SetValue('m_content', 'Blah blah blah.');
    SetValue('m_title', 'Head Up Display');
  }
  -->
  </script>

<?php 
    if (strlen($name))
    {
?>

<ul><li><a href="#edit">Edit</a></li><li><a href="#rename">Rename</a></li><li><a href="#remove">Remove</a></li></ul>

<h2 id="edit">Edit manual</h2>
<p>Note: If you change manual's page name, it will be saved with a new name but the old one will remain active.<br />If you only want to <a href="#rename">rename</a> the manual page, use the <a href="#rename">form below</a>. This form is for creating new manual pages or editing properties of existing ones.</p>



<?php
    }
    else
    {
?>
<h2>Add new manual page</h2>
<p>Warning: If you type name of already existing manual page, it's content will be overwritten without asking for confirmation.</p>
<?php
    }
?>
<p><button onclick="FillIn()">Example fill out</button></p>
<form action="index.php" method="post" id="mainform">
<div><input type="hidden" name="action" id="action" value="addman" /></div>
<fieldset>
  <legend>Manual page informations</legend>
  <label for="m_title">Title:<br /><input type="text" name="m_title" id="m_title" size="50" value="<?=$title?>" /></label><br />
  <label for="m_name">Name:<br /><input type="text" name="m_name" id="m_name" value="<?=$name?>" /></label><br />
  <p>Name is used only in URL, it should be lower cases, without spaces or any other special characters. Use comma '-' for separating words rather than the underscore '_'.<br />
  Example: Title is "The Bogus &amp; Great Feature" so you should choose name like "bogus-and-great-feature".</p>
  <label for="m_remarks">Remarks:<br /><textarea name="m_content" id="m_content" cols="60" rows="30"><?=$content?></textarea></label><br />
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

<?php
    if (strlen($name)) {
?>

<h2 id="rename">Rename manual page</h2>
<form action="index.php" method="post" id="renameform">
<div><input type="hidden" name="m_oldname" value="<?=$name?>" /><input type="hidden" name="action" value="renamemanual" /></div>
<p><label>Rename <?=$name?> to: <input type="text" name="m_newname" value="" /></label></p>
<div><input type="submit" value="Submit" /></div>
<p>Make sure there's no manual page with the name you are renaming this manual page to.</p>
</form>

<h2 id="remove">Remove manual page</h2>
<form action="index.php" method="post" id="removeform">
<div><input type="hidden" name="m_name" value="<?=$name?>" /><input type="hidden" name="action" value="removemanual" /></div>
<p><label><input type="checkbox" name="m_physremove" /> Physically remove *</label></p>
<div><input type="submit" value="Remove" /></div>
<p>* Use 'Physically remove' only for invalid manual pages; not for outdated or renamed ones.</p>
</form>

<?php	
    }
?>
