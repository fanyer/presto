<html>
<head>
<style>
#strTable {
  border: 2px solid #888;
  width: 80%;
}
#strTable th {
  border: 1px solid #888;
}
td #token {
  width: 14em;
}
</style>
<script src="wbxmlizer_tokens.js"></script>
<script src="wbxmlizer.js"></script>
</head>

<body onload="init()">
<h1>WBXMLizer</h1>

Doctype:<br/>
<select>
  <option value="09">WML 1.3 Public ID</option>
</select>

<h3>String table</h3>

<input type="button" value="Add string" onclick="addString()" />
<div id="strTableDiv">No string table</div>

<h3>Content</h3>

<table id="tokenTable">
<tr id="activeRow">
  <td><select id="group" onchange="updateGroup()"></select></td>
  <td><select id="token" onchange="updateToken()"></select></td>
  <td>
    <select id="modifier">
      <option value="0">Empty</option>
      <option value="80">Attributes</option>
      <option value="40">Content</option>
      <option value="C0">Content & Attributes</option>
    </select>
	<input type="text" id="extraData" style="display: none" />
  </td>
  <td><input type="button" value="Add" onclick="addToken()" /></td>
</tr>
</table>

<br/>
<input type="button" value="Generate" onclick="generateUrl()" />

<hr />
<textarea cols="80" rows="25" id="output"></textarea>
</body>
</html>