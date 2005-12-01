<ul>
<li>Major Groups<ul>
    <li><a href="#mgadd">Add</a></li>
    <li><a href="#mgrename">Rename</a></li>
    <li><a href="#mgremove">Remove</a></li>
</ul></li>
<li>Groups<ul>
    <li><a href="#gadd">Add</a></li>
    <li><a href="#grename">Rename</a></li>
    <li><a href="#gremove">Remove</a></li>
    <li><a href="#glist">List</a></li>
</ul></li>
</ul>


<?php

    $options_mgroups = $grpForms->OptionsListMG();
    $select_mgroups  = "<select name=\"mgroup_id\">".$options_mgroups."</select>";
    $select_groups = "<select name=\"group_id\">".$grpForms->OptionsListG()."</select>";
    
?>

<h2 id="mgadd">Add Major Group</h2>
<form action="index.php" method="post">
<div><input type="hidden" name="action" value="addmgroup" /></div>
<fieldset>
<label>Name: <input type="text" value="" name="mg_name" /></label><br />
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

<h2 id="mgrename">Rename Major Group</h2>
<form action="index.php" method="post">
<div><input type="hidden" name="action" value="renamemgroup" /></div>
<fieldset>
<label>Major group: <?=$select_mgroups?></label><br />
<label>New Name: <input type="text" value="" name="mg_name" /></label><br />
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

<h2 id="mgremove">Remove Major Group</h2>
<form action="index.php" method="post">
<div><input type="hidden" name="action" value="removemgroup" /></div>
<fieldset>
<label>Remove group: <?=$select_mgroups?></label><br />
<label>Move assigned to: <select name="to_mgroup_id"><?=$options_mgroups?></select></label><br />
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

<h2 id="gadd">Add Group</h2>
<form action="index.php" method="post">
<div><input type="hidden" name="action" value="addgroup" /></div>
<fieldset>
<label>Name: <input type="text" value="" name="g_name" /></label><br />
<label>Major group: <?=$select_mgroups?></label><br />
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

<h2 id="grename">Rename Group</h2>
<form action="index.php" method="post">
<div><input type="hidden" name="action" value="renamegroup" /></div>
<fieldset>
<label>Group: <?=$select_groups?></label><br />
<label>New Name: <input type="text" value="" name="g_name" /></label><br />
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

<h2 id="gremove">Remove Group</h2>
<form action="index.php" method="post">
<div><input type="hidden" name="action" value="removegroup" /></div>
<fieldset>
<label>Major group: <?=$select_groups?></label><br />
</fieldset>
<p><button type="submit">Submit</button> </p>
</form>

<h2 id="glist">Assignments</h2>
<form action="index.php" method="post">
<div><input type="hidden" name="action" value="assigngroups" /></div>
<?php

    $grpForms->PrintListG();

?>
<p><button type="submit">Submit</button> </p>
</form>
