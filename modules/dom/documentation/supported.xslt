<?xml version="1.0"?>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml" version="1.0">

  <xsl:template match="supported">
    <html>
      <head>
        <title>DOM support in <xsl:value-of select="@version"/> or newer</title>
        <link rel="stylesheet" type="text/css" href="supported.css"/>
        <script><![CDATA[
          function display (expression)
          {
            try
            {
              value = eval (expression);
            }
            catch (e)
            {
              value = "Exception: " + String (e);
            }

            alert (value);
          }
        ]]></script>
      </head>
      <body>
        <table class="toc">
          <caption>Contents:</caption>
          <tbody>
            <xsl:apply-templates mode="toc"/>
          </tbody>
        </table>
        <div class="normal">
          <xsl:apply-templates mode="normal"/>
        </div>
        <div class="footer">
          <div class="revision">Revision: <xsl:value-of select="@revision"/></div>
        </div>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="object" mode="toc">
    <xsl:param name="indentation" select="0"/>
    <tr>
      <td class="toc-index">
        <xsl:call-template name="index"/>
      </td>
      <td class="toc-object" style="padding-left: {$indentation}cm">
        <a href="#{generate-id(.)}">Object: <span class="name"><xsl:value-of select="@name"/></span></a>
      </td>
    </tr>
    <xsl:apply-templates mode="toc">
      <xsl:with-param name="indentation" select="$indentation + 1"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="class" mode="toc">
    <xsl:param name="indentation" select="0"/>
    <tr>
      <td class="toc-index">
        <xsl:call-template name="index"/>
      </td>
      <td class="toc-class" style="padding-left: {$indentation}cm">
        <a href="#{generate-id(.)}">Class: <span class="name"><xsl:value-of select="@name"/></span></a>
      </td>
    </tr>
    <xsl:apply-templates mode="toc">
      <xsl:with-param name="indentation" select="$indentation + 1"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="interface" mode="toc">
    <xsl:param name="indentation" select="0"/>
    <tr>
      <td class="toc-index">
        <xsl:call-template name="index"/>
      </td>
      <td class="toc-interface" style="padding-left: {$indentation}cm">
        <a href="#{generate-id(.)}">Interface: <span class="name"><xsl:value-of select="@name"/></span></a>
      </td>
    </tr>
    <xsl:apply-templates mode="toc">
      <xsl:with-param name="indentation" select="$indentation + 1"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="text()" mode="toc">
  </xsl:template>

  <xsl:template match="object" mode="normal">
    <xsl:param name="name-prefix"/>
    <xsl:param name="support-display" select="true()"/>
    <div class="object">
      <div class="name">
        <xsl:call-template name="index"/>
        <a name="{generate-id(.)}">Object:
          <span class="name">
            <xsl:if test="$name-prefix!=''"><xsl:value-of select="$name-prefix"/>.</xsl:if>
            <xsl:value-of select="@name"/>
          </span>
        </a>
      </div>
      <xsl:if test="@origin">
        <div class="origin">Origin: <xsl:call-template name="describe-origin-link"/></div>
      </xsl:if>
      <xsl:if test="@supported">
        <div class="supported"><xsl:call-template name="describe-supported"/></div>
      </xsl:if>
      <div class="members">
        <xsl:call-template name="members-with-prefix">
          <xsl:with-param name="name-prefix" select="$name-prefix"/>
          <xsl:with-param name="support-display" select="$support-display"/>
        </xsl:call-template>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="class" mode="normal">
    <xsl:param name="name-prefix"/>
    <div class="class">
      <div class="name">
        <xsl:call-template name="index"/>
        <a name="{generate-id(.)}">Class:
          <span class="name">
            <xsl:if test="$name-prefix!=''"><xsl:value-of select="$name-prefix"/>.</xsl:if>
            <xsl:value-of select="@name"/>
          </span>
        </a>
      </div>
      <xsl:for-each select="specification">
        <div class="specification">
          Specification: <a href="{@href}" title="Link to specification"><xsl:call-template name="describe-origin"/></a>
        </div>
      </xsl:for-each>
      <xsl:if test="@origin">
        <div class="origin">Origin: <xsl:call-template name="describe-origin-link"/></div>
      </xsl:if>
      <xsl:if test="@supported">
        <div class="supported"><xsl:call-template name="describe-supported"/></div>
      </xsl:if>
      <div class="members">
        <xsl:call-template name="members-with-prefix">
          <xsl:with-param name="name-prefix" select="$name-prefix"/>
        </xsl:call-template>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="interface" mode="normal">
    <xsl:param name="name-prefix"/>
    <div class="interface">
      <div class="name">
        <xsl:call-template name="index"/>
        <a name="{generate-id(.)}">Interface:
          <span class="name">
            <xsl:value-of select="@name"/>
          </span>
        </a>
      </div>
      <xsl:for-each select="specification">
        <div class="specification">
          Specification: <a href="{@href}" title="Link to specification"><xsl:call-template name="describe-origin"/></a>
        </div>
      </xsl:for-each>
      <xsl:if test="@origin">
        <div class="origin">Origin: <xsl:call-template name="describe-origin-link"/></div>
      </xsl:if>
      <xsl:if test="@supported">
        <div class="supported"><xsl:call-template name="describe-supported"/></div>
      </xsl:if>
      <div class="members">
        <xsl:apply-templates mode="normal">
          <xsl:with-param name="name-prefix" select="$name-prefix"/>
        </xsl:apply-templates>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="constructor" mode="normal">
    <xsl:param name="name-prefix"/>
    <xsl:param name="support-display" select="false()"/>
    <div class="constructor">
      <div class="name">Constructor:
        <span class="name">
          <xsl:value-of select="concat($name-prefix, '.', @name)"/>
        </span>
      </div>
      <xsl:if test="@origin">
        <div class="origin">Origin: <xsl:call-template name="describe-origin-link"/></div>
      </xsl:if>
      <xsl:if test="@supported">
        <div class="supported"><xsl:call-template name="describe-supported"/></div>
      </xsl:if>
      <div class="members">
        <xsl:call-template name="members-with-prefix">
          <xsl:with-param name="name-prefix" select="$name-prefix"/>
          <xsl:with-param name="support-display" select="$support-display"/>
        </xsl:call-template>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="property" mode="normal">
    <xsl:param name="name-prefix"/>
    <xsl:param name="support-display" select="false()"/>
    <div class="property">
      <div class="name">Property:
        <span class="name">
          <xsl:value-of select="concat($name-prefix, '.', @name)"/>
        </span>
        <xsl:if test="$support-display">
          <xsl:if test="not(@supported) or @supported != 'no'">
            <a class="display-value" href="javascript:display ('{concat($name-prefix, '.', @name)}');" title="Display value">Display value</a>
          </xsl:if>
        </xsl:if>
      </div>
      <xsl:if test="@origin">
        <div class="origin">Origin: <xsl:call-template name="describe-origin-link"/></div>
      </xsl:if>
      <xsl:if test="@supported">
        <div class="supported"><xsl:call-template name="describe-supported"/></div>
      </xsl:if>
    </div>
  </xsl:template>

  <xsl:template match="method" mode="normal">
    <xsl:param name="name-prefix"/>
    <div class="method">
      <div class="name">Method:
        <span class="name">
          <xsl:if test="$name-prefix!=''"><xsl:value-of select="$name-prefix"/>.</xsl:if>
          <xsl:value-of select="@name"/>
        </span>
      </div>
      <xsl:if test="@origin">
        <div class="origin">Origin: <xsl:call-template name="describe-origin-link"/></div>
      </xsl:if>
      <xsl:if test="@supported">
        <div class="supported"><xsl:call-template name="describe-supported"/></div>
      </xsl:if>
    </div>
  </xsl:template>

  <xsl:template match="text()" mode="normal">
  </xsl:template>

  <xsl:template name="index">
    <span class="index" xml:space="preserve"><xsl:number level="multiple" count="object|interface|class"/> </span>
  </xsl:template>

  <xsl:template name="members-with-prefix">
    <xsl:param name="name-prefix"/>
    <xsl:param name="support-display" select="false()"/>
    <xsl:choose>
      <xsl:when test="$name-prefix!=''">
        <xsl:apply-templates mode="normal">
          <xsl:with-param name="name-prefix" select="concat($name-prefix, '.', @name)"/>
          <xsl:with-param name="support-display" select="$support-display"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates mode="normal">
          <xsl:with-param name="name-prefix" select="string(@name)"/>
          <xsl:with-param name="support-display" select="$support-display"/>
        </xsl:apply-templates>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="describe-origin">
    <xsl:choose>
      <xsl:when test="@origin='dom2core'">DOM 2 Core</xsl:when>
      <xsl:when test="@origin='dom2events'">DOM 2 Events</xsl:when>
      <xsl:when test="@origin='dom2html'">DOM 2 HTML</xsl:when>
      <xsl:when test="@origin='dom2style'">DOM 2 Style</xsl:when>
      <xsl:when test="@origin='dom2css'">DOM 2 CSS</xsl:when>
      <xsl:when test="@origin='dom2travrange'">DOM 2 Traversal &amp; Range</xsl:when>
      <xsl:when test="@origin='dom2views'">DOM 2 Views</xsl:when>
      <xsl:when test="@origin='dom3core'">DOM 3 Core</xsl:when>
      <xsl:when test="@origin='dom3events'">DOM 3 Events</xsl:when>
      <xsl:when test="@origin='dom3loadsave'">DOM 3 Load &amp; Save</xsl:when>
      <xsl:when test="@origin='dom3xpath'">DOM 3 XPath</xsl:when>
      <xsl:when test="@origin='js13'">JavaScript Client 1.3</xsl:when>
      <xsl:when test="@origin='opera'">Opera</xsl:when>
      <xsl:when test="@origin='msie'">Microsoft Internet Explorer</xsl:when>
      <xsl:when test="@origin='firefox'">Mozilla / Firefox</xsl:when>
      <xsl:when test="@origin='various'">Various</xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="describe-origin-link">
    <xsl:choose>
      <xsl:when test="@origin='dom2core'"><a href="http://www.w3.org/TR/DOM-Level-2-Core">DOM 2 Core</a></xsl:when>
      <xsl:when test="@origin='dom2events'"><a href="http://www.w3.org/TR/DOM-Level-2-Events">DOM 2 Events</a></xsl:when>
      <xsl:when test="@origin='dom2html'"><a href="http://www.w3.org/TR/DOM-Level-2-HTML">DOM 2 HTML</a></xsl:when>
      <xsl:when test="@origin='dom2style'"><a href="http://www.w3.org/TR/DOM-Level-2-Style">DOM 2 Style</a></xsl:when>
      <xsl:when test="@origin='dom2css'"><a href="http://www.w3.org/TR/DOM-Level-2-Style">DOM 2 CSS</a></xsl:when>
      <xsl:when test="@origin='dom2travrange'"><a href="http://www.w3.org/TR/DOM-Level-2-Traversal-Range">DOM 2 Traversal &amp; Range</a></xsl:when>
      <xsl:when test="@origin='dom2views'"><a href="http://www.w3.org/TR/DOM-Level-2-Views">DOM 2 Views</a></xsl:when>
      <xsl:when test="@origin='dom3core'"><a href="http://www.w3.org/TR/DOM-Level-3-Core">DOM 3 Core</a></xsl:when>
      <xsl:when test="@origin='dom3events'"><a href="http://www.w3.org/TR/DOM-Level-3-Events">DOM 3 Events</a></xsl:when>
      <xsl:when test="@origin='dom3loadsave'"><a href="http://www.w3.org/TR/DOM-Level-3-LS">DOM 3 Load &amp; Save</a></xsl:when>
      <xsl:when test="@origin='dom3xpath'"><a href="http://www.w3.org/TR/DOM-Level-3-XPath">DOM 3 XPath</a></xsl:when>
      <xsl:when test="@origin='js13'">JavaScript Client 1.3</xsl:when>
      <xsl:when test="@origin='opera'">Opera</xsl:when>
      <xsl:when test="@origin='msie'">Microsoft Internet Explorer</xsl:when>
      <xsl:when test="@origin='firefox'">Mozilla / Firefox</xsl:when>
      <xsl:when test="@origin='various'">Various</xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="describe-supported">
    <xsl:choose>
      <xsl:when test="@supported='no'">
        <xsl:attribute name="class">supported not-supported</xsl:attribute>
        Not supported
      </xsl:when>
      <xsl:when test="@supported='optional'">
        <xsl:attribute name="class">supported optional</xsl:attribute>
        Optional (<xsl:apply-templates select="feature|macro" mode="supported"/>)
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="feature" mode="supported">
    <span class="feature"><xsl:value-of select="@name"/>==<xsl:choose><xsl:when test="@enabled='no'">NO</xsl:when><xsl:otherwise>YES</xsl:otherwise></xsl:choose></span>
    <xsl:if test="position()!=last()">, </xsl:if>
  </xsl:template>

  <xsl:template match="macro" mode="supported">
    <span class="macro"><xsl:if test="@defined='no'">!</xsl:if>defined(<xsl:value-of select="@name"/>)</span>
    <xsl:if test="position()!=last()">, </xsl:if>
  </xsl:template>

</xsl:stylesheet>
