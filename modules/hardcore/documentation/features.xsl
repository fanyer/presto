<?xml version="1.0"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:html="http://www.w3.org/1999/xhtml"> 
  <xsl:import href="utils.xsl"/>

  <xsl:template match="features">
    <html:html>
      <html:head>
        <html:title>Feature database for core version <xsl:value-of select="/features/@core-version"/></html:title>
        <html:style>
          table.matrix { width: 80% }
          tbody > tr > th { text-align: left }
          table.matrix td { width: 10%; text-align: center }
          td.enabled { background-color: lime; color: black }
          td.disabled { background-color: red; color: white }
          td.platform-defined { background-color: rgb(220,220,220) }
        </html:style>
      </html:head>
      <html:body>
        <html:h1>Feature/Profile matrix for core version <xsl:value-of select="/features/@core-version"/></html:h1>
        <xsl:apply-templates mode="matrix" select="current()"/>

        <html:h1>Feature list for core version <xsl:value-of select="/features/@core-version"/></html:h1>
        <xsl:apply-templates select="feature"/>
      </html:body>
    </html:html>
  </xsl:template>

  <xsl:template match="feature">
    <html:h2><html:a name="{@name}"><xsl:value-of select="@name"/></html:a></html:h2>

    <xsl:choose>
      <xsl:when test="@deprecated">
        <html:p>This feature is deprecated.</html:p>
      </xsl:when>
      <xsl:otherwise>
        <html:p>Owned by<xsl:apply-templates select="owners/owner"/>.</html:p>
      </xsl:otherwise>
    </xsl:choose>

    <html:pre>
      <xsl:value-of select="description"/>
    </html:pre>

    <xsl:if test="profiles/profile[@enabled = 'yes']">
      <html:p>Enabled for<xsl:apply-templates select="profiles/profile[@enabled = 'yes']"/></html:p>
    </xsl:if>

    <xsl:if test="profiles/profile[@enabled = 'no']">
      <html:p>Disabled for<xsl:apply-templates select="profiles/profile[@enabled = 'no']"/></html:p>
    </xsl:if>

    <xsl:apply-templates/>

    <html:hr/>
  </xsl:template>

  <xsl:template match="owners"/>

  <xsl:template match="owner">
    <xsl:text> </xsl:text><xsl:call-template name="link-to-developer"><xsl:with-param name="uid" select="string(@name)"/></xsl:call-template>
  </xsl:template>

  <xsl:template match="profiles"/>

  <xsl:template match="profile">
    <xsl:text> </xsl:text><xsl:call-template name="upper-case-profile"><xsl:with-param name="profile" select="string(@name)"/></xsl:call-template>
  </xsl:template>

  <xsl:template match="defines">
    <html:h3>Preprocessor contributions when enabled:</html:h3>
    <html:pre>
      <xsl:apply-templates/>
    </html:pre>
  </xsl:template>

  <xsl:template match="define">
    <xsl:text>#define </xsl:text><html:a href="defines.{/features/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a><xsl:if test="@value"><xsl:text> </xsl:text><xsl:value-of select="@value"/></xsl:if><xsl:text>
</xsl:text>
  </xsl:template>

  <xsl:template match="undefines">
    <html:h3>Preprocessor contributions when disabled:</html:h3>
    <html:pre>
      <xsl:apply-templates/>
    </html:pre>
  </xsl:template>

  <xsl:template match="undefine">
    <xsl:text>#define </xsl:text><html:a href="defines.{/features/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a><xsl:text>
</xsl:text>
  </xsl:template>

  <xsl:template match="dependencies">
    <html:h3>Depends on:</html:h3>
    <html:ul>
      <xsl:apply-templates/>
    </html:ul>
  </xsl:template>

  <xsl:template match="depends-on">
    <html:li><html:a href="#{@name}"><xsl:value-of select="@name"/></html:a></html:li>
  </xsl:template>

  <xsl:template match="requirements">
    <html:h3>Required by:</html:h3>
    <html:ul>
      <xsl:apply-templates/>
    </html:ul>
  </xsl:template>

  <xsl:template match="required-by">
    <html:li><html:a href="#{@name}"><xsl:value-of select="@name"/></html:a></html:li>
  </xsl:template>

  <xsl:template match="text()"/>

  <xsl:template mode="matrix" match="features">
    <html:table class="matrix">
      <html:tbody>
        <xsl:apply-templates mode="matrix" select="feature"/>
      </html:tbody>
    </html:table>
  </xsl:template>

  <xsl:template mode="matrix" match="feature">
    <html:tr>
      <html:th class="feature"><html:a href="#{@name}"><xsl:value-of select="@name"/></html:a></html:th>
      <xsl:call-template name="matrix-cell"><xsl:with-param name="profile" select="'desktop'"/></xsl:call-template>
      <xsl:call-template name="matrix-cell"><xsl:with-param name="profile" select="'tv'"/></xsl:call-template>
      <xsl:call-template name="matrix-cell"><xsl:with-param name="profile" select="'smartphone'"/></xsl:call-template>
      <xsl:call-template name="matrix-cell"><xsl:with-param name="profile" select="'minimal'"/></xsl:call-template>
      <xsl:call-template name="matrix-cell"><xsl:with-param name="profile" select="'mini'"/></xsl:call-template>
    </html:tr>
  </xsl:template>

  <xsl:template name="matrix-cell">
    <xsl:param name="profile"/>
    <html:td>
      <xsl:choose>
        <xsl:when test="profiles/profile[@name = $profile][@enabled = 'yes']">
          <xsl:attribute name="class">enabled</xsl:attribute>
        </xsl:when>
        <xsl:when test="profiles/profile[@name = $profile][@enabled = 'no']">
          <xsl:attribute name="class">disabled</xsl:attribute>
        </xsl:when>
        <xsl:otherwise>
          <xsl:attribute name="class">platform-defined</xsl:attribute>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:call-template name="upper-case-profile"><xsl:with-param name="profile" select="$profile"/></xsl:call-template>
    </html:td>
  </xsl:template>

  <xsl:template name="upper-case-profile">
    <xsl:param name="profile"/>
    <xsl:value-of select="translate($profile, 'desktop tv smartphone minimal mini', 'DESKTOP TV SMARTPHONE MINIMAL MINI')"/>
  </xsl:template>
</xsl:stylesheet>
