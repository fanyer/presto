<?xml version="1.0"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:html="http://www.w3.org/1999/xhtml">
  <xsl:output method="xml"/>

  <xsl:template name="link-to-developer">
    <xsl:param name="uid"/>
    <html:a href="https://ssl.opera.com:8008/developerwiki/index.php/User:{$uid}"><xsl:value-of select="$uid"/></html:a>
  </xsl:template>

  <xsl:template match="tweaks">
    <html:html>
      <html:head>
        <html:title>Tweak database for core version <xsl:value-of select="/tweaks/@core-version"/></html:title>
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
        <html:h1>Tweaks by [ <html:a href="#by-module">module</html:a> | <html:a href="#by-category">category</html:a> | <html:a href="#by-name">name</html:a> ]  for core version <xsl:value-of select="/tweaks/@core-version"/></html:h1>
        <html:hr/>

        <html:h1>Tweak/Profile matrix</html:h1>
        <xsl:apply-templates mode="matrix" select="current()"/>

        <html:h1><html:a name="by-module">Tweaks by module:</html:a></html:h1>
        <html:p><xsl:for-each select="module"><xsl:sort select="@name"/><html:a href="#module-{@name}"><xsl:value-of select="@name"/></html:a><xsl:text> </xsl:text></xsl:for-each></html:p>
        <html:ul>
          <xsl:apply-templates mode='toc-by-module' select='module'><xsl:sort select="@name"/></xsl:apply-templates>
        </html:ul>
        <html:hr/>

        <html:h1><html:a name="by-category">Tweaks by category:</html:a></html:h1>
        <html:p><xsl:for-each select="module/tweak/categories/category[not(preceding::category/@name = @name)]"><xsl:sort select="@name"/><html:a href="#category-{@name}"><xsl:value-of select="@name"/></html:a><xsl:text> </xsl:text></xsl:for-each></html:p>
        <html:ul>
          <xsl:apply-templates select="module/tweak/categories/category[not(preceding::category/@name = @name)]" mode='toc-by-category'><xsl:sort select="@name"/></xsl:apply-templates>
        </html:ul>
        <html:hr/>

        <html:h1><html:a name="by-name">Tweaks by name:</html:a></html:h1>
        <xsl:apply-templates select="module/tweak"><xsl:sort select="@name"/></xsl:apply-templates>
      </html:body>
    </html:html>
  </xsl:template>

  <xsl:template match="module" mode="toc-by-module">
    <html:li>
      <html:h2><html:a name="module-{@name}"><xsl:value-of select="@name"/></html:a></html:h2>
      <html:ul>
        <xsl:apply-templates mode="toc-by-module" select='tweak'><xsl:sort select="@name"/></xsl:apply-templates>
      </html:ul>
    </html:li>
  </xsl:template>

  <xsl:template match="tweak" mode="toc-by-module">
    <html:li>
      <html:a href="#{@name}"><xsl:value-of select="@name"/></html:a>
    </html:li>
  </xsl:template>

  <xsl:template match="category" mode="toc-by-category">
    <html:li>
      <html:h2><html:a name="category-{@name}"><xsl:value-of select="@name"/></html:a></html:h2>
      <html:ul>
        <xsl:apply-templates select="ancestor::tweaks/module/tweak[categories/category/@name = current()/@name]" mode="toc-by-category"><xsl:sort select="@name"/></xsl:apply-templates>
      </html:ul>
    </html:li>
  </xsl:template>

  <xsl:template match="tweak" mode="toc-by-category">
    <html:li>
      <html:a href="#{@name}"><xsl:value-of select="@name"/></html:a>
    </html:li>
  </xsl:template>

  <xsl:template match="tweak">
    <html:h2><html:a name="{@name}"><xsl:value-of select="@name"/></html:a></html:h2>

    <xsl:choose>
      <xsl:when test="@deprecated">
        <html:p>This tweak is deprecated.</html:p>
      </xsl:when>
      <xsl:otherwise>
        <html:p>Owned by<xsl:apply-templates select="owners/owner"/>.</html:p>
      </xsl:otherwise>
    </xsl:choose>

    <html:pre>
      <xsl:value-of select="description"/>
    </html:pre>

    <xsl:apply-templates/>

    <html:hr/>
  </xsl:template>

  <xsl:template match="owners"/>

  <xsl:template match="owner">
    <xsl:text> </xsl:text><xsl:call-template name="link-to-developer"><xsl:with-param name="uid" select="string(@name)"/></xsl:call-template>
  </xsl:template>

  <xsl:template match="defines[define/@value]" priority="2">
    <html:h3>Preprocessor contributions when not tweaked:</html:h3>
    <html:pre>
      <xsl:apply-templates select="define"/>
    </html:pre>
    <xsl:if test="following-sibling::profiles/profile">
      <html:h3>Profile defaults:</html:h3>
      <xsl:for-each select="following-sibling::profiles/profile">
        <html:h4 style="margin-left: 0.5cm">Profile: <xsl:value-of select="translate(@name, 'desktop tv smartphone minimal mini', 'DESKTOP TV SMARTPHONE MINIMAL MINI')"/></html:h4>
        <html:pre style="margin-left: 1cm">
          <xsl:if test="@enabled = 'yes'">
            <xsl:text>#define </xsl:text><xsl:value-of select="ancestor::tweak/@name"/><xsl:text> YES
</xsl:text>
            <xsl:text>#define </xsl:text><html:a href="defines.{/tweaks/@core-version}.xml#{parent::profiles/preceding-sibling::defines/define[1]/@name}"><xsl:value-of select="parent::profiles/preceding-sibling::defines/define[1]/@name"/></html:a><xsl:text> </xsl:text><xsl:value-of select="@value"/><xsl:text>
</xsl:text>
          </xsl:if>
          <xsl:if test="@enabled = 'no'">
            <xsl:text>#define </xsl:text><xsl:value-of select="ancestor::tweak/@name"/><xsl:text> NO
</xsl:text>
          </xsl:if>
        </html:pre>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="defines" priority="1">
    <html:h3>Preprocessor contributions when enabled:</html:h3>
    <html:pre>
      <xsl:apply-templates select="define"/>
    </html:pre>
    <xsl:if test="following-sibling::profiles/profile">
      <html:h3>Profile defaults:</html:h3>
      <xsl:for-each select="following-sibling::profiles/profile">
        <html:h4 style="margin-left: 0.5cm">Profile: <xsl:value-of select="translate(@name, 'desktop tv smartphone minimal mini', 'DESKTOP TV SMARTPHONE MINIMAL MINI')"/></html:h4>
        <html:pre style="margin-left: 1cm">
          <xsl:if test="@enabled = 'yes'">
            <xsl:text>#define </xsl:text><xsl:value-of select="ancestor::tweak/@name"/><xsl:text> YES
</xsl:text>
          </xsl:if>
          <xsl:if test="@enabled = 'no'">
            <xsl:text>#define </xsl:text><xsl:value-of select="ancestor::tweak/@name"/><xsl:text> NO
</xsl:text>
          </xsl:if>
        </html:pre>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="define">
    <xsl:text>#define </xsl:text><html:a href="defines.{/tweaks/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a><xsl:if test="@value"><xsl:text> </xsl:text><xsl:value-of select="@value"/></xsl:if><xsl:text>
</xsl:text>
  </xsl:template>

  <xsl:template match="dependencies">
    <html:h3>Relevant if:</html:h3>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template mode="make-link" match="*[@type='define']">
    <html:a href="defines.{/tweaks/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a>
  </xsl:template>

  <xsl:template mode="make-link" match="*[@type='feature']">
    <html:a href="features.{/tweaks/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a>
  </xsl:template>

  <xsl:template mode="make-link" match="*[@type='tweak']">
    <html:a href="#{@name}"><xsl:value-of select="@name"/></html:a>
  </xsl:template>

  <xsl:template mode="make-link" match="*[@type='system']">
    <html:a href="system.xml#{@name}"><xsl:value-of select="@name"/></html:a>
  </xsl:template>

  <xsl:template match="dependencies/depends-on">
    <html:ul>
      <html:li><xsl:apply-templates mode="make-link" select="."/> is <xsl:choose><xsl:when test="@enabled='yes'">enabled</xsl:when><xsl:otherwise>disabled</xsl:otherwise></xsl:choose></html:li>
    </html:ul>
  </xsl:template>

  <xsl:template match="and/depends-on | or/depends-on">
    <html:li><xsl:apply-templates mode="make-link" select="."/> is <xsl:choose><xsl:when test="@enabled='yes'">enabled</xsl:when><xsl:otherwise>disabled</xsl:otherwise></xsl:choose></html:li>
  </xsl:template>

  <xsl:template match="dependencies/and" priority="2">
    <html:h4>All of:</html:h4>
    <html:ul>
      <xsl:apply-templates/>
    </html:ul>
  </xsl:template>

  <xsl:template match="dependencies/or" priority="2">
    <html:h4>One of:</html:h4>
    <html:ul>
      <xsl:apply-templates/>
    </html:ul>
  </xsl:template>

  <xsl:template match="and" priority="1">
    <html:li>
      <html:h4>All of:</html:h4>
      <html:ul>
        <xsl:apply-templates/>
      </html:ul>
    </html:li>
  </xsl:template>

  <xsl:template match="or" priority="1">
    <html:li>
      <html:h4>One of:</html:h4>
      <html:ul>
        <xsl:apply-templates/>
      </html:ul>
    </html:li>
  </xsl:template>

  <xsl:template match="text()"/>

  <xsl:template mode="matrix" match="tweaks">
    <html:table class="matrix">
      <html:tbody>
        <xsl:apply-templates mode="matrix" select="module/tweak"><xsl:sort select="@name"/></xsl:apply-templates>
      </html:tbody>
    </html:table>
  </xsl:template>

  <xsl:template mode="matrix" match="tweak">
    <html:tr>
      <html:th class="tweak"><html:a href="#{@name}"><xsl:value-of select="@name"/></html:a></html:th>
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
      <xsl:choose>
        <xsl:when test="defines/define/@value and profiles/profile[@name = $profile][@enabled = 'yes']">
          <xsl:value-of select="profiles/profile[@name = $profile]/@value"/>
        </xsl:when>
        <xsl:when test="defines/define/@value and profiles/profile[@name = $profile][@enabled = 'no']">
          <xsl:value-of select="defines/define/@value"/>
        </xsl:when>
        <xsl:when test="defines/define/@value"/>
        <xsl:otherwise>
          <xsl:call-template name="upper-case-profile"><xsl:with-param name="profile" select="$profile"/></xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </html:td>
  </xsl:template>

  <xsl:template name="upper-case-profile">
    <xsl:param name="profile"/>
    <xsl:value-of select="translate($profile, 'desktop tv smartphone minimal mini', 'DESKTOP TV SMARTPHONE MINIMAL MINI')"/>
  </xsl:template>
</xsl:stylesheet>
