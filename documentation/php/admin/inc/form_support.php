<?php

    $select_builds = "<select name=\"id_build\">".$supForms->OptionsBuilds()."</select>";
?>

<p>Use following lines to create <code>defaults.cfg</code> config to be uploaded here:</p>
<pre><code>cfg_reset
cfg_save_aliases 0
cfg_save_binds 0
cfg_save_cmdline 0
cfg_save_cmds 0
cfg_save_sysinfo 0
cfg_save_cvars 1
cfg_save_unchanged 1
cfg_save_userinfo 2
cfg_save defaults</code></pre>

<form enctype="multipart/form-data" action="index.php" method="post">
<div><input type="hidden" name="action" value="varssetdefaults" />
<input type="hidden" name="MAX_FILE_SIZE" value="300000" /></div>
<label>Build: <?=$select_builds?></label><br />
<label>Config with default values: <input name="userfile" type="file" /></label>
<p><input type="submit" value="Submit" /></p>
</form>
