<stylesheet version="1.0" xmlns="http://www.w3.org/1999/XSL/Transform">
  <output method="xml"/>
  <template match="/">
    <copy-of select="document('loaded/pass.xml')"/>
  </template>
</stylesheet>
