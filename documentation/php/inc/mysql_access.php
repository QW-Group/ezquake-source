<?php

    switch ($_SERVER["HTTP_HOST"])
    {
    case "jhn4":
        $mysql_server = "localhost";
        $mysql_user = "root";
        $mysql_password = "";
        $mysql_db = "ezquake_docs";
        break;
    default:
        $mysql_server = "mysql4-e";
        $mysql_user = "e117445rw";
        $mysql_password = "32e477gbyz3c";
        $mysql_db = "e117445_docs";
        break;
    }
    
    $db = mysql_connect($mysql_server, $mysql_user, $mysql_password);
    $mysql_password = "1234567890"; 
    $mysql_password = "";
    
    if (!$db)
        die("Connection to MySQL Server hasn't been established due to following error: "
            . mysql_errno(). " - ". mysql_error());

    mysql_select_db($mysql_db, $db);
    if (!db)
        die("Couldn't select database.");

function my_mysql_query($query)
{
    //echo("<p>$query</p>");
    static $queries = 0;
    
    $r = mysql_query($query);
    $queries++;
    
    if (!$r)
    {
        if ($_SERVER["HTTP_HOST"] == "jhn4")    // debugging on my home machine
            echo "<p><code>$query</code></p>";
            
        echo("<p class=\"error\">".mysql_error()."</p>");
        return False;
    }
    else
    return $r;
}

function dbSQL( $sql )
{
  // split sql into separate lines
  $sql_lines = explode( "\n", $sql );

  for( $i = 0; $i < count( $sql_lines ); $i++ ) {
    // remove comments
    if( substr( $sql_lines[ $i ], 0, 1 ) == "#" ) {
      array_splice( $sql_lines, $i, 1 );
      $i--;
    }
    // remove empty lines
    elseif( $sql_lines[ $i ] == "" ) {
      array_splice( $sql_lines, $i, 1 );
      $i--;
    }
  }

  // join the separate lines again
  $cleaned_sql = implode( "", $sql_lines );

  // split sql into separate queries
  $sql_queries = array();
  $quoted = false;
  $query = "";
  for( $i = 0; $i < strlen( $cleaned_sql ); $i++ ) {
    $pre_pre_char = $pre_char;
    $pre_char = $char;
    $char = substr( $cleaned_sql, $i, 1 );
    if( $char == "'" and !$quoted and ( $pre_char != "\\" or $pre_pre_char == "\\" ) ) {
      $quoted = true;
      $query .= $char;
    } elseif( $char == "'" and $quoted and ( $pre_char != "\\" or $pre_pre_char == "\\" ) ) {
      $quoted = false;
      $query .= $char;
    } elseif( $char == ";" and !$quoted ) {
      array_push( $sql_queries, $query );
      $query = "";
    } else {
      $query .= $char;
    }
  }

  // execute the queries
  for( $i = 0; $i < count( $sql_queries ); $i++ ) {
    my_mysql_query( $sql_queries[ $i ] );
  }
}
    
?>
