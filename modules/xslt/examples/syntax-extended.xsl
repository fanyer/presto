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

          .doctype, .STag, .ETag, .EmptyElemTag {
            font-weight: bold;
            color: black
          }

          .doctype .name, .STag .name, .ETag .name, .EmptyElemTag .name {
            color: blue
          }

          .doctype .data, .STag .data, .EmptyElemTag .data {
            font-weight: normal;
            color: green
          }

          .text {
            color: green
          }

          .comment {
            color: red
          }

          .xmldecl, .processing-instruction {
            color: grey
          }

          .unspecified:after {
            font-weight: normal;
            font-size: smaller;
            position: relative;
            top: -0.5ex;
            left: 1px;
            content: "(default)"
          }

          .id:after {
            font-weight: normal;
            font-size: smaller;
            position: relative;
            top: -0.5ex;
            left: 1px;
            content: "(id)"
          }
        ]]></style>
      </head>

      <body>
        <xsl:if xmlns:opera="http://xmlns.opera.com/2006/xslt" test="opera:xmldecl-version()">
          <span class="xmldecl" xml:space="preserve">&lt;?xml version="<xsl:value-of select="opera:xmldecl-version()"/>"<xsl:if test="opera:xmldecl-encoding()"> encoding="<xsl:value-of select="opera:xmldecl-encoding()"/>"</xsl:if><xsl:if test="opera:xmldecl-standalone()"> standalone="<xsl:value-of select="opera:xmldecl-standalone()"/>"</xsl:if>?&gt;</span>
        </xsl:if>
        <xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>

  <xsl:template xmlns:opera="http://xmlns.opera.com/2006/xslt" match="opera:doctype()">
    <span class="doctype" xml:space="preserve">&lt;!DOCTYPE <span class="name"><xsl:value-of select="opera:doctype-name()"/></span><xsl:choose><xsl:when test="opera:doctype-public-id()"> PUBLIC "<span class="data"><xsl:value-of select="opera:doctype-public-id()"/></span>"<xsl:if test="opera:doctype-system-id()"> "<span class="data"><xsl:value-of select="opera:doctype-system-id()"/></span>"</xsl:if></xsl:when><xsl:when test="opera:doctype-system-id()"> SYSTEM "<span class="data"><xsl:value-of select="opera:doctype-system-id()"/></span>"</xsl:when></xsl:choose><xsl:if test="opera:doctype-internal-subset()"> [<span class="data"><xsl:value-of select="opera:doctype-internal-subset()"/></span>]</xsl:if>&gt;</span>
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
    <span xml:space="preserve"> <span xmlns:opera="http://xmlns.opera.com/2006/xslt"><xsl:attribute name="class" namespace="http://www.w3.org/1999/xhtml">attribute<xsl:if test="not(opera:attr-is-specified())"> unspecified</xsl:if><xsl:if test="opera:attr-is-id()"> id</xsl:if></xsl:attribute><span class="name"><xsl:value-of select="name()"/></span>="<span class="data"><xsl:value-of select="."/></span>"</span></span>
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
