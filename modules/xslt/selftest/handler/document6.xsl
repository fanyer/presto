<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text"/>
  <xsl:key name="id" match="*" use="@id"/>
  <xsl:template match="/">
    <xsl:for-each select="document('loaded/document6a.xml')">1: <xsl:value-of select="key('id', 'id1')"/><xsl:text>
</xsl:text></xsl:for-each>
    <xsl:for-each select="document('loaded/document6b.xml')">2: <xsl:value-of select="key('id', 'id2')"/><xsl:text>
</xsl:text></xsl:for-each>
    <xsl:for-each select="document('loaded/document6c.xml')">3: <xsl:value-of select="key('id', 'id3')"/><xsl:text>
</xsl:text></xsl:for-each>
  </xsl:template>
</xsl:stylesheet>
