CREATE TABLE commands
(
  id SMALLINT UNSIGNED AUTO_INCREMENT,
  name CHAR(32) NOT NULL DEFAULT '' UNIQUE,
  active TINYINT UNSIGNED NOT NULL DEFAULT 1,
  description TEXT,
  syntax TEXT,
  remarks TEXT,
  PRIMARY KEY(id)
);

CREATE TABLE commands_arguments
(
  id MEDIUMINT UNSIGNED AUTO_INCREMENT NOT NULL,
  id_cmd SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(16) NOT NULL DEFAULT '',
  description TEXT,
  PRIMARY KEY(id)
);

CREATE TABLE commands_history
(
  id INT UNSIGNED AUTO_INCREMENT NOT NULL,
  id_command SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  id_user SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  action ENUM("created", "updated", "changed", "renamed", "deleted"),
  id_renamedto SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  time TIMESTAMP(14),
  PRIMARY KEY(id)
);

CREATE TABLE variables
(
  id SMALLINT UNSIGNED AUTO_INCREMENT,
  id_vargroup SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  name CHAR(32) NOT NULL DEFAULT '' UNIQUE,
  active TINYINT UNSIGNED NOT NULL DEFAULT 1,
  description TEXT,
  type ENUM("integer", "float", "string", "boolean", "enum"),
  remarks TEXT,
  flags TINYINT UNSIGNED NOT NULL DEFAULT 0,
  id_group SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY(id)
);

CREATE TABLE variables_values_boolean
(
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_variable SMALLINT UNSIGNED NOT NULL DEFAULT 0 UNIQUE,
  true_desc text,
  false_desc text,
  PRIMARY KEY(id)
);

CREATE TABLE variables_values_enum
(
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_variable SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  value CHAR(32) NOT NULL DEFAULT 0,
  description text,
  PRIMARY KEY(id)
);

CREATE TABLE variables_values_other
(
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_variable SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  description text,
  PRIMARY KEY(id)
);

CREATE TABLE variables_history
(
  id INT UNSIGNED AUTO_INCREMENT NOT NULL,
  id_variable SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  id_user SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  action ENUM("created", "updated", "changed", "renamed", "deleted"),
  id_renamedto SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  time TIMESTAMP(14),
  PRIMARY KEY(id)
);

CREATE TABLE variables_support
(
    id INT UNSIGNED AUTO_INCREMENT,
    id_variable SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    id_build SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    default_value CHAR(32) NOT NULL DEFAULT '',
    PRIMARY KEY(id),
    UNIQUE(id_variable, id_build),
    INDEX(id_variable)
);

CREATE TABLE builds
(
  id TINYINT UNSIGNED AUTO_INCREMENT NOT NULL,
  abbr CHAR(4) NOT NULL DEFAULT '',
  shortname CHAR(12) NOT NULL DEFAULT '',
  title CHAR(32) NOT NULL DEFAULT '',
  PRIMARY KEY(id),
  UNIQUE(abbr),
  UNIQUE(shortname),
  UNIQUE(title)
);
INSERT INTO builds (abbr, shortname, title) VALUES ('W-GL', 'W32:OpenGL', 'Windows: OpenGL');
INSERT INTO builds (abbr, shortname, title) VALUES ('W-SW', 'W32:Soft', 'Windows: Software');
INSERT INTO builds (abbr, shortname, title) VALUES ('GLX', 'Lin:GLX', 'Linux: GLX');
INSERT INTO builds (abbr, shortname, title) VALUES ('X11', 'Lin:X11', 'Linux: X11');
INSERT INTO builds (abbr, shortname, title) VALUES ('SVGA', 'Lin:SVGA', 'Linux: SVGA');
INSERT INTO builds (abbr, shortname, title) VALUES ('MAC', 'Mac-OSX', 'Mac OS X');



CREATE TABLE variables_mgroups
(
  id SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,
  name VARCHAR(32) NOT NULL DEFAULT '',
  title VARCHAR(128) NOT NULL DEFAULT '',
  active TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY(id),
  UNIQUE(name),
  UNIQUE(title)
);

CREATE TABLE variables_groups
(
  id SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_mgroup SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(32),
  title VARCHAR(128),
  active TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY(id),
  UNIQUE(name),
  UNIQUE(title)
);

CREATE TABLE users
(
  id SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,
  login CHAR(16) NOT NULL DEFAULT '',
  name CHAR(32) NOT NULL DEFAULT '',
  password CHAR(32) NOT NULL DEFAULT '',
  access TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY(id),
  UNIQUE(login),
  UNIQUE(name)
);

CREATE TABLE sessions
(
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_user SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  ip CHAR(16) NOT NULL DEFAULT '127.0.0.1',
  host CHAR(255) NOT NULL DEFAULT '',
  browser CHAR(255) NOT NULL DEFAULT '',
  idstr CHAR(32) NOT NULL DEFAULT '' UNIQUE,
  lasthit TIMESTAMP(14),
  PRIMARY KEY(id)
);

CREATE TABLE manuals
(
  id MEDIUMINT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_group SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  name CHAR(32) NOT NULL DEFAULT '',
  title CHAR(128) NOT NULL DEFAULT '',
  content TEXT,
  active TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY(id),
  UNIQUE(name),
  UNIQUE(title)
);

CREATE TABLE manuals_groups
(
  id SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,
  abbr CHAR(32) NOT NULL DEFAULT '',
  name CHAR(128) NOT NULL DEFAULT '',
  PRIMARY KEY(id),
  UNIQUE(abbr),
  UNIQUE(name)
);

CREATE TABLE manuals_history
(
  id INT UNSIGNED AUTO_INCREMENT NOT NULL,
  id_manual SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  id_user SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  action ENUM("created", "updated", "changed", "renamed", "deleted"),
  id_renamedto SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  time TIMESTAMP(14),
  PRIMARY KEY(id)
);

CREATE TABLE commands_support
(
  id SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_command SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  id_build SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY(id)
);

---
CREATE TABLE manuals_history
(
  id INT UNSIGNED AUTO_INCREMENT NOT NULL,
  id_manual SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  id_user SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  action ENUM("created", "updated", "changed", "renamed", "deleted"),
  id_renamedto SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  time TIMESTAMP(14),
  PRIMARY KEY(id)
);

CREATE TABLE options
(
    id MEDIUMINT UNSIGNED AUTO_INCREMENT NOT NULL,
    name CHAR(64) NOT NULL DEFAULT '',
    description TEXT,
    args CHAR(128) NOT NULL DEFAULT '',
    argsdesc TEXT,
    flags SET('GL only', 'Linux only', 'Win32 only', 'Software only'),
    active TINYINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY(id),
    UNIQUE(name)
);

CREATE TABLE options_history
(
  id INT UNSIGNED AUTO_INCREMENT NOT NULL,
  id_option SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  id_user SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  action ENUM("created", "updated", "changed", "renamed", "deleted"),
  id_renamedto SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  time TIMESTAMP(14),
  PRIMARY KEY(id)
);

DROP TABLE manuals;

CREATE TABLE settings_index
(
  name CHAR(64) NOT NULL,
  itype ENUM("variable", "command", "command-line option"),
  desc1 char(255) NOT NULL DEFAULT '',
  desc2 char(255) NOT NULL DEFAULT '',
  desc3 char(255) NOT NULL DEFAULT '',
  igroup SMALLINT UNSIGNED NOT NULL DEFAULT 0,  
  PRIMARY KEY(name, itype)
);
