<?xml version="1.0"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:html="http://www.w3.org/1999/xhtml">

  <xsl:template match="defines">
    <html:html>
      <html:head>
        <html:title>Define database for core version <xsl:value-of select="/defines/@core-version"/></html:title>
      </html:head>
      <html:body>
        <html:h1>Defines for core version <xsl:value-of select="/defines/@core-version"/></html:h1>
        <html:hr/>

        <html:dl>
          <xsl:for-each select="define"><xsl:sort select="@name"/>
          <html:dt><html:a name="{@name}"><xsl:value-of select="@name"/></html:a></html:dt>
          <xsl:for-each select="./item"><xsl:sort select="@name"/>
          <html:dd><xsl:apply-templates mode="item" select="."/></html:dd>
          </xsl:for-each>
          </xsl:for-each>
        </html:dl>
      </html:body>
    </html:html>
  </xsl:template>

  <xsl:template mode="item" match="*[@type='tweak']">
    <html:a href="tweaks.{/defines/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a>
  </xsl:template>

  <xsl:template mode="item" match="*[@type='feature']">
    <html:a href="features.{/defines/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a>
  </xsl:template>

  <xsl:template mode="item" match="*[@type='api-import']">
    <html:a href="apis.{/defines/@core-version}.xml#import-{@name}"><xsl:value-of select="@name"/></html:a>
  </xsl:template>

  <xsl:template mode="item" match="*[@type='api-export']">
    <html:a href="apis.{/defines/@core-version}.xml#export-{@name}"><xsl:value-of select="@name"/></html:a>
  </xsl:template>

  <xsl:template mode="item" match="*[@type='system']">
    <html:a href="system.{/defines/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a>
  </xsl:template>
</xsl:stylesheet>
