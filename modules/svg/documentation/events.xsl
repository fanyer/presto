<xsl:stylesheet version='1.0' 
		xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

<xsl:template match="/">
  <html xmlns="http://www.w3.org/1999/xhtml">
    <head>
      <title>
	<xsl:value-of select="supportdocument/title"/>
      </title>
      <link rel="stylesheet" href="transformed.css" type="text/css"/>
    </head>

    <body>
      <h1><xsl:value-of select="supportdocument/title"/></h1>
      <table class="main" cellspacing="0" cellpadding="0" border="0">
	<tr>
	  <th><xsl:value-of select="supportdocument/list/col1"/></th>
	  <th>Elements</th>
	  <th>Comments</th>
	  <th>Developer comments</th>
	  <th>Version(s)</th>
	  <th class="supported">Supported</th>
	</tr>
	<xsl:for-each select="supportdocument/list/event">
	  <xsl:sort select="name"/>
	  <tr>
	    <xsl:if test="position() mod 2 = 0">
	      <xsl:attribute name="class">even</xsl:attribute>
	    </xsl:if>

	    <td>
	      <xsl:value-of select="name"/>
	    </td>
	    <td>
	      <xsl:value-of select="elements"/>
	    </td>
		<td>
	      <xsl:value-of select="comments"/>
	    </td>
	    <td>
	      <xsl:value-of select="dev-comments"/>
	    </td>
	    <td align="center">
		  <xsl:choose>
		    <xsl:when test="@version">
		      <xsl:value-of select="@version"/>
			</xsl:when>
			<xsl:otherwise>
			  1.1
			</xsl:otherwise>
		  </xsl:choose>
		</td>
	    <td>
	      <xsl:if test="@supported = 'yes'">
		<xsl:attribute name="class">supported</xsl:attribute>
                Yes
	      </xsl:if>
	      <xsl:if test="@supported = 'no'">
		<xsl:attribute name="class">unsupported</xsl:attribute>
                No
	      </xsl:if>
	    </td>
	  </tr>
	</xsl:for-each>
      </table>
    </body>
  </html>
</xsl:template>

</xsl:stylesheet> 
