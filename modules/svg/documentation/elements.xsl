<xsl:stylesheet version='1.0' 
		xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>
<xsl:output method = "html" />

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
	  <th>Interface name</th>
	  <th>Comments</th>
	  <th>Developer comments</th>
	  <th>Version</th>
    <th>1.1T</th>
    <th>1.1B</th>
    <th>1.1F</th>
    <th>1.2T</th>
	  <th class="supported">Supported</th>
	</tr>
	<xsl:for-each select="supportdocument/elementlist/element">
	  <xsl:sort select="name"/>
	  <tr>
	    <xsl:if test="position() mod 2 = 0">
	      <xsl:attribute name="class">even</xsl:attribute>
	    </xsl:if>

	    <td>
	      <xsl:value-of select="name"/>
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
    <xsl:choose>
      <xsl:when test="@profiles">
        <xsl:call-template name="output-tokens">
          <xsl:with-param name="list">
            <xsl:value-of select="@profiles"/>
          </xsl:with-param>
          <xsl:with-param name="fulllist">1.1T 1.1B 1.1F 1.2T</xsl:with-param>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <td align="center">✓</td>
        <td align="center">✓</td>
        <td align="center">✓</td>
        <td align="center">✓</td>
      </xsl:otherwise>
    </xsl:choose>
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

<xsl:template name="output-tokens">
  <xsl:param name="list"/>
  <xsl:param name="fulllist"/>
  <xsl:variable name="fullnewlist" select="concat(normalize-space($fulllist), ' ')"/>
  <xsl:variable name="fullfirst" select="substring-before($fullnewlist, ' ')"/>
  <xsl:variable name="fullremaining" select="substring-after($fullnewlist, ' ')"/>
  <!--(first=<xsl:value-of select="$fullfirst"/> fulllist=<xsl:value-of select="$fulllist"/> list=<xsl:value-of select="$list"/>)-->
  <xsl:choose>
    <xsl:when test="contains($list, $fullfirst)"><td align="center">✓</td></xsl:when>
    <xsl:otherwise><td align="center">✘</td></xsl:otherwise>
  </xsl:choose>
  <xsl:if test="$fullremaining">
    <xsl:call-template name="output-tokens">
      <xsl:with-param name="list" select="$list"/>
      <xsl:with-param name="fulllist" select="$fullremaining"/>
    </xsl:call-template>
  </xsl:if>
</xsl:template>

</xsl:stylesheet> 
