<?xml version="1.0"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:html="http://www.w3.org/1999/xhtml">
  <xsl:template name="link-to-developer">
    <xsl:param name="uid"/>
    <html:a href="https://ssl.opera.com:8008/developerwiki/index.php/User:{$uid}"><xsl:value-of select="$uid"/></html:a>
  </xsl:template>
</xsl:stylesheet>
