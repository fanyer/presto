<xsl:stylesheet version='1.0' 
		xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

<xsl:template match="/">
  <html xmlns="http://www.w3.org/1999/xhtml">
    <head>
      <title>
	<xsl:value-of select="svg-objects/title"/>
      </title>
      <link rel="stylesheet" href="../../coredoc/coredoc.css" type="text/css"/>
      <link rel="stylesheet" href="objects.css" type="text/css"/>
    </head>

    <body>
      <h1>Objects</h1>
      <table width="100%" cellspacing="0" cellpadding="0" border="0">
	<tr>
	  <th>Name</th><th>Animatable</th><th>Interpolatable</th><th>Additive</th><th>Comments</th>
	</tr>
	<xsl:for-each select="svg-objects/object">
	  <tr>
	    <xsl:if test="position() mod 2 = 0">
	      <xsl:attribute name="class">even</xsl:attribute>
	    </xsl:if>

	    <td>
	      <xsl:value-of select="@name"/>
	    </td>
	    <td style="font-family: monospace;">
	      <xsl:if test="@animatable = 'yes'">
		<svg xmlns="http://www.w3.org/2000/svg" id="img_yes" width="16" height="16">
		  <circle cx="8" cy="8" r="4" fill="green"/>
		</svg>
	      </xsl:if>
	    </td>
	    <td>
	      <xsl:if test="@interpolatable = 'yes'">
		<svg xmlns="http://www.w3.org/2000/svg" id="img_yes" width="16" height="16">
		  <circle cx="8" cy="8" r="4" fill="green"/>
		</svg>
	      </xsl:if>
	    </td>
	    <td>
	      <xsl:if test="@additive = 'yes'">
		<svg xmlns="http://www.w3.org/2000/svg" id="img_yes" width="16" height="16">
		  <circle cx="8" cy="8" r="4" fill="green"/>
		</svg>
	      </xsl:if>
	    </td>
	    <td>
	      <xsl:value-of select="comments"/>
	    </td>
	  </tr>
	</xsl:for-each>
      </table>
    </body>
  </html>
</xsl:template>

</xsl:stylesheet> 
