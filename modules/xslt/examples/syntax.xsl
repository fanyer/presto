<?xml version="1.0"?>
<?xml-stylesheet type="text/xsl" href=""?>

<!-- XML syntax-highlighting XSL Transform. -->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml">
  <xsl:preserve-space elements="*"/>

  <xsl:template match="/">
    <html>
      <head>
        <title>Syntax-highlighted XML</title>
        <style type="text/css"><![CDATA[
          body {
            font-family: monospace;
            white-space: pre
          }

          .element {
          }

          .STag, .ETag, .EmptyElemTag {
            font-weight: bold;
            color: black
          }

          .STag .name, .ETag .name, .EmptyElemTag .name {
            color: blue
          }

          .STag .data, .EmptyElemTag .data {
            font-weight: normal;
            color: green
          }

          .text {
            color: green
          }

          .comment {
            color: red
          }

          .processing-instruction {
            color: grey
          }
        ]]></style>
      </head>

      <body>
        <xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="*">
    <span class="element">
      <span class="STag">&lt;<span class="name"><xsl:value-of select="name()"/></span><xsl:apply-templates select="@*"/>&gt;</span>
      <span class="content">
        <xsl:apply-templates/>
      </span>
      <span class="ETag">&lt;/<span class="name"><xsl:value-of select="name()"/></span>&gt;</span>
    </span>
  </xsl:template>

  <xsl:template match="*[count(node())=0]">
    <span class="element">
      <span class="EmptyElemTag">&lt;<span class="name"><xsl:value-of select="name()"/></span><xsl:apply-templates select="@*"/>/&gt;</span>
    </span>
  </xsl:template>

  <xsl:template match="@*">
    <span class="attribute" xml:space="preserve"> <span class="name"><xsl:value-of select="name()"/></span>="<span class="data"><xsl:value-of select="."/></span>"</span>
  </xsl:template>

  <xsl:template match="text()">
    <span class="text"><xsl:value-of select="."/></span>
  </xsl:template>

  <xsl:template match="comment()">
    <span class="comment">&lt;!--<span class="data"><xsl:value-of select="."/></span>--&gt;</span>
  </xsl:template>

  <xsl:template match="processing-instruction()">
    <span class="processing-instruction" xml:space="preserve">&lt;?<span class="name"><xsl:value-of select="name()"/></span> <span class="data"><xsl:value-of select="."/></span>?&gt;</span>
  </xsl:template>
</xsl:stylesheet>
