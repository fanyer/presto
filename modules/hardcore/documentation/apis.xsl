<?xml version="1.0"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:html="http://www.w3.org/1999/xhtml">
  <xsl:import href="utils.xsl"/>

  <xsl:template match="apis">
    <html:html>
      <html:head>
        <html:title>API database for core version <xsl:value-of select="/apis/@core-version"/></html:title>
      </html:head>
      <html:body>
        <html:h1>API [ <html:a href="#exports-by-module">exports</html:a> | <html:a href="#imports-by-module">imports</html:a> ] by module  for core version <xsl:value-of select="/apis/@core-version"/></html:h1>
        <html:hr/>

        <html:h1><html:a name="exports-by-module">Exported APIs by module:</html:a></html:h1>
        <html:p><xsl:for-each select="module[exported-apis]"><xsl:sort select="@name"/><html:a href="#exports-{@name}"><xsl:value-of select="@name"/></html:a><xsl:text> </xsl:text></xsl:for-each></html:p>
        <html:ul>
          <xsl:apply-templates mode='exports-toc-by-module'/>
        </html:ul>
        <html:hr/>
        <xsl:apply-templates select="module/exported-apis/exported-api"><xsl:sort select="@name"/></xsl:apply-templates>

        <html:h1><html:a name="imports-by-module">Imported APIs by module:</html:a></html:h1>
        <html:p><xsl:for-each select="module[imported-apis]"><xsl:sort select="@name"/><html:a href="#imports-{@name}"><xsl:value-of select="@name"/></html:a><xsl:text> </xsl:text></xsl:for-each></html:p>
        <html:ul>
          <xsl:apply-templates mode='imports-toc-by-module'/>
        </html:ul>
        <html:hr/>
        <xsl:apply-templates select="module/imported-apis/imported-api"><xsl:sort select="@name"/></xsl:apply-templates>
      </html:body>
    </html:html>
  </xsl:template>

  <xsl:template match="module[exported-apis]" mode="exports-toc-by-module" priority="2">
    <html:li>
      <html:h2><html:a name="exports-{@name}"><xsl:value-of select="@name"/></html:a></html:h2>
      <html:ul>
        <xsl:apply-templates select="exported-apis/exported-api" mode="exports-toc-by-module"><xsl:sort select="@name"/></xsl:apply-templates>
      </html:ul>
    </html:li>
  </xsl:template>

  <xsl:template match="module" mode="exports-toc-by-module" priority="1"/>

  <xsl:template match="exported-api" mode="exports-toc-by-module">
    <html:li>
      <html:a href="#export-{@name}"><xsl:value-of select="@name"/></html:a>
    </html:li>
  </xsl:template>

  <xsl:template match="module[imported-apis]" mode="imports-toc-by-module" priority="2">
    <html:li>
      <html:h2><html:a name="imports-{@name}"><xsl:value-of select="@name"/></html:a></html:h2>
      <html:ul>
        <xsl:apply-templates select="imported-apis/imported-api" mode="imports-toc-by-module"><xsl:sort select="@name"/></xsl:apply-templates>
      </html:ul>
    </html:li>
  </xsl:template>

  <xsl:template match="module" mode="imports-toc-by-module" priority="1"/>

  <xsl:template match="imported-api" mode="imports-toc-by-module">
    <html:li>
      <html:a href="#import-{ancestor::module/@name}-{@name}"><xsl:value-of select="@name"/></html:a>
    </html:li>
  </xsl:template>

  <xsl:template match="exported-api">
    <html:h2>Export: <html:a name="export-{@name}"><xsl:value-of select="@name"/></html:a></html:h2>

    <xsl:choose>
      <xsl:when test="@deprecated">
        <html:p>This API is deprecated.</html:p>
      </xsl:when>
      <xsl:otherwise>
        <html:p>Owned by<xsl:apply-templates select="owners/owner"/>.</html:p>
      </xsl:otherwise>
    </xsl:choose>

    <html:pre>
      <xsl:value-of select="description"/>
    </html:pre>

    <xsl:apply-templates/>

    <xsl:if test="ancestor::apis/module[imported-apis/imported-api[@name=current()/@name]]">
      <html:h3>Imported by:</html:h3>
      <html:p><xsl:for-each select="ancestor::apis/module[imported-apis/imported-api[@name=current()/@name]]"><xsl:sort select="@name"/><xsl:value-of select="@name"/><xsl:if test="position()!=last()">, </xsl:if></xsl:for-each></html:p>
    </xsl:if>

    <html:hr/>
  </xsl:template>

  <xsl:template match="imported-api">
    <html:h2>Import: <html:a name="import-{ancestor::module/@name}-{@name}"><xsl:value-of select="@name"/></html:a></html:h2>

    <html:p>Imported by <xsl:value-of select="ancestor::module/@name"/>.</html:p>
    
    <xsl:choose>
      <xsl:when test="@deprecated">
        <html:p>This API import is deprecated.</html:p>
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

  <xsl:template match="defines">
    <html:h3>Preprocessor contributions when imported:</html:h3>
    <html:pre>
      <xsl:apply-templates/>
    </html:pre>
  </xsl:template>

  <xsl:template match="define">
    <xsl:text>#define </xsl:text><html:a href="defines.{/apis/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a><xsl:text>
</xsl:text>
  </xsl:template>

  <xsl:template match="dependencies[parent::exported-api]">
    <html:h3>Depends on:</html:h3>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="dependencies[parent::imported-api]">
    <html:h3>Imported if:</html:h3>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template mode="make-link" match="*[@type='define']">
    <html:a href="defines.{/apis/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a> is defined
  </xsl:template>

  <xsl:template mode="make-link" match="*[@type='feature']">
    <html:a href="features.{/apis/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a> is enabled
  </xsl:template>

  <xsl:template mode="make-link" match="*[@type='tweak']">
    <html:a href="tweaks.{/apis/@core-version}.xml#{@name}"><xsl:value-of select="@name"/></html:a> is enabled
  </xsl:template>

  <xsl:template mode="make-link" match="*[@type='system']">
    <html:a href="system.xml#{@name}"><xsl:value-of select="@name"/></html:a> is enabled
  </xsl:template>

  <xsl:template match="dependencies/depends-on">
    <html:ul>
      <html:li><xsl:apply-templates mode="make-link" select="."/></html:li>
    </html:ul>
  </xsl:template>

  <xsl:template match="and/depends-on | or/depends-on">
    <html:li><xsl:apply-templates mode="make-link" select="."/></html:li>
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
</xsl:stylesheet>
