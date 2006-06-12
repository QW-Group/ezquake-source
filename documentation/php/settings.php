<?php

// used in main heading, page title, ... 
define ("PROJECTNAME", "ezQuake");

// this will be used also in google search:
define ("BASEURL", "http://ezquake.sourceforge.net/docs/");

// put the info about the author of the documentation content here
// you can use html tags in here, will be displayed on the bottom of the page))
define ("AUTHOR_INFO", "<a href=\"http://sourceforge.net/users/johnnycz/\">JohnNy_cz</a>");

// html meta headers
// author (usually the same as author_info but you cannot use html tags in here
define ("META_AUTHOR", "http://sourceforge.net/users/johnnycz/");
// description
define ("META_DESCRIPTION", "The Complete guide to using QuakeWorld &trade; client ezQuake");
// there's no real use for this nowadays, feel free to leave it empty 
define ("META_KEYWORDS", "ezQuake, manual, guide, tutorial, how-to, howto, setting, quake, quakeworld, client, help, readme, install");

// show search form on the main page?
define  ("SHOW_SEARCH", true);
// show installation part on the main page? (requires 'installation' manual page in db!)
define ("SHOW_INSTALLATION", true);
// show features part on the main page? (requires 'features' manual page in db!)
define ("SHOW_FEATURES", true);
// show settings part on the main page? (false will also hide all following parts!)
define ("SHOW_SETTINGS", true);
// show link to the index
define ("SHOW_INDEX", true);
// show the description how to compose url to any manual page
define ("SHOW_URL_DESCRIPTION", true);
// show variables list on the main page?
define ("SHOW_VARIABLES", true);
// show link to unassigned variables 
define ("SHOW_UNASSIGNED_VARS", true);
// show commands link on the main page?
define ("SHOW_COMMANDS", true);
// show command-line options link on the main page?
define ("SHOW_OPTIONS", true);
// show triggers link on the main page?
define ("SHOW_TRIGGERS", true);


?>
