<stylesheet version="1.0" xmlns="http://www.w3.org/1999/XSL/Transform">
  <output method="text"/>
  <template match="/">
    <for-each select="document('loaded/document5a.xml')">1: <value-of select="id('id1')"/><text>
</text></for-each>
    <for-each select="document('loaded/document5b.xml')">2: <value-of select="id('id2')"/><text>
</text></for-each>
    <for-each select="document('loaded/document5c.xml')">3: <value-of select="id('id3')"/><text>
</text></for-each>
  </template>
</stylesheet>
