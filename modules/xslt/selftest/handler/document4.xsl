<stylesheet version="1.0" xmlns="http://www.w3.org/1999/XSL/Transform">
  <output method="text"/>
  <template match="/">
    <for-each select="document(root/item/@href)">PASS(<value-of select="string()"/>)<text>
</text></for-each>
  </template>
</stylesheet>
