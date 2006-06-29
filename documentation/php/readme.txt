ezQuake Documentation System
made by JohnNy_cz
GNU General Public License 2

Requirements:
Webserver running PHP 4 (or better) and MySQL 4.0 (or better)

Installation instructions:
1) Get the whole content of documentation root directory (directory where this readme is)
2) Create a MySQL database, run queries from 'tables.sql'
3) Fill in database connections details in inc/mysql_access.php
4) Perform
INSERT INTO users (login, name, password, access) VALUES ('admin', 'admin', MD5('yourpasswordhere'), 2)
to create an admin account
5) edit settings.php

Note:
Regular users have access level 1
Administrators have access level 2
Administrators can add/remove/change password of users and release .zip archives with XML exported docs
