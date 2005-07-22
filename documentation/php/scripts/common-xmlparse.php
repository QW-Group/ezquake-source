<?php

  /*
    XML parsing methods
  */
  
  function VariableXML2Div($xmlpath) {
    if(!file_exists($xmlpath)) {
      echo("<p class=\"error\">Not found: $xmlpath</p>");
      return;
    }
    $arguments = array(
         '/_xml' => $xml = file_get_contents($xmlpath),
         '/_xsl' => $xsl = file_get_contents('../generator/xsl/var2div.xsl')
    );
    $xh = xslt_create();
    $result = xslt_process($xh, 'arg:/_xml', 'arg:/_xsl', NULL, $arguments); 
    xslt_free($xh); 
    if (!$result) {
      echo "XSL Transformation failed while processing $xmlpath: " . xslt_error($xh);
      echo "error code: " . xslt_errno($xh);
      return;
    } else return $result;
  }

?>
