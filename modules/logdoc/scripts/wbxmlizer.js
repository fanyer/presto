var debugPrint = "";

function dumpDebug()
{
    var debugElm = document.getElementById("debug");
	debugElm.value = debugPrint;
}

function escOut(decNum)
{
	if (decNum < 0x10)
		return "%0" + decNum.toString(16);
	else
		return "%" + decNum.toString(16);
}

function init()
{
    updateDoctype(document.getElementById("doctype"));
}

function updateDoctype(select)
{
    if (select.selectedIndex == 0) {
		document.getElementById("source").value = "<wml><card title=\"fisk\"><p><do/>PASS</p></card></wml>";
    }
    else {
		document.getElementById("source").value = "<?xml version=\"1.0\"?><root>";
    }
}

function addString()
{
	var newString = prompt("Enter String");
	if (newString.length > 0) {
		var tbl = document.getElementById("strtbl");

		debugPrint = "rows.length(" + tbl.rows.length + "), ";
		var nextOffset = 0;
		var lastIdx = tbl.rows.length - 1;
		if (lastIdx > 0) {
			var lastRow = tbl.rows[lastIdx];
			if (lastRow)
				nextOffset = parseInt(lastRow.cells[0].firstChild.data) + lastRow.cells[1].firstChild.data.length + 1;
		}
		debugPrint += "lastIdx(" + lastIdx + "), nextOffset(" + nextOffset + ")";

		var newRow = tbl.insertRow(lastIdx + 1);
		var newCell = newRow.insertCell(0);
		var text = document.createTextNode(nextOffset);
		newCell.appendChild(text);

		newCell = newRow.insertCell(1);
		text = document.createTextNode(newString);
		newCell.appendChild(text);
	}
	//	dumpDebug();
}

function parseText(text)
{
    var retStr = "";

	debugPrint += "[parseText(\"" + text + "\")]\n";
    if (text) {
		var firstPos = text.indexOf("%");
		while (firstPos != -1) {
			var beforeText = text.substr(0, firstPos);
			if (beforeText.length > 0) {
				debugPrint += "before %(\"" + encodeURIComponent(beforeText) + "\")\n";
				retStr += "%03" + encodeURIComponent(beforeText) + "%00";
			}

			var numStrPos = firstPos + 1;

			debugPrint += "numStrPos(" + numStrPos + "), ";

			if (text[numStrPos] != "%") {
				// FIXME: lookup string
				var offset = parseInt(text[numStrPos]);
				debugPrint += "offset(" + offset + "), ";
				retStr += "%83" + escOut(offset);

				while (numStrPos < text.length && text.charAt(numStrPos) < "9" && text.charAt(numStrPos) >= "0") numStrPos++;

				text = text.substr(numStrPos);
			}
			else {
				retStr += "%";
				debugPrint += "%, ";
				text = text.slice(0, firstPos + 1);
			}

			debugPrint += "rest(" + text + ")\n";
			firstPos = text.indexOf("%");
		}

		if (text.length > 0)
			retStr += "%03" + encodeURIComponent(text) + "%00";
	}

    return retStr;
}

function parseAttr(attr, first, last)
{
	debugPrint += "  attr[" + attr + "]\n";

	var useAttr = attr;
	if (last && useAttr[useAttr.length - 1] == "/")
		useAttr = useAttr.substr(0, useAttr.length - 1);

	var tokenGroup = 1;
	var attrToken = "";

	while (useAttr.length > 0) {
		for (var i = 0; i < tokens[tokenGroup][1].length; i++) {
			var matchToken = tokens[tokenGroup][1][i][0];
			//		debugPrint += " matching (" + matchToken + ") with (" + useAttr + ")\n";

			var subPattern = useAttr.substr(0, matchToken.length);
			//		debugPrint += "sub (" + subPattern + ")\n";
			if (subPattern == matchToken) {
				debugPrint += " => found it (" + subPattern + ", " + tokens[tokenGroup][1][i][1] + ")\n";
				attrToken += "%" + tokens[tokenGroup][1][i][1];
			
				useAttr = useAttr.substr(subPattern.length); // parse rest of value
				tokenGroup = 2; // switch to non-attr-start
				continue;
			}
		}

		break;
	}

	if (useAttr.length > 0) {
		// put in a literal token
		debugPrint += " need to add (" + useAttr + ") \n";
		attrToken += parseText(useAttr.substr(0, useAttr.length - 1));
	}

	return attrToken;
}

function parseTag(tag)
{
    var retStr = "";
    var attrs = tag.split(" ");

    if (attrs[0][0] == "/") {
		debugPrint += "endTag\n";
		retStr += "%01"; // end token
    }
    else {
		if (attrs[0][0] == "%") {
			debugPrint += "table[" + attrs[0].substr(1) + "]\n";
			// FIXME: lookup string in string table
			retStr += "";
		}
		else {
			var token = "";

			var has_attrs = attrs.length > 1;
			var has_content = true;

			var lastAttr = attrs[attrs.length - 1];
			// check the last character in the tag to see
			// if it is an empty tag (<tag/>)
			if (lastAttr[lastAttr.length - 1] == "/") {
				has_content = false;
			}

			var useTag = attrs[0];
			if (useTag.indexOf("/") != -1)
				useTag = useTag.replace("/", "");

			for (var i = 0; i < tokens[0][1].length; i++) {
				if (useTag == tokens[0][1][i][0]) {
					token = tokens[0][1][i][1];
					break;
				}
			}

			debugPrint += "tagStart: " + useTag + " [" + token + "]\n";

			var tmpToken = parseInt("0x" + token);
			if (has_attrs) {
				debugPrint += " has_attrs " + escOut(tmpToken);
				tmpToken |= parseInt("0x80");
				debugPrint += " => " + escOut(tmpToken) + "\n";
			}

			if (has_content) {
				debugPrint += " has_content " + escOut(tmpToken);
				tmpToken |= parseInt("0x40");
				debugPrint += " => " + escOut(tmpToken) + "\n";
			}
			
			retStr += escOut(tmpToken);
		}

		for (var i = 1; i < attrs.length; i++) {
			retStr += parseAttr(attrs[i], i == 1, i == attrs.length - 1);
		}

		if (attrs.length > 1) {
			retStr += "%01"; // terminate the attributes
		}
    }

	debugPrint += "\n";
    return retStr;
}

function parseSrc(src)
{
    var outString = "data:application/vnd.wap.wmlc,%02%09%6a";
    var elmArray = src.split("<");
	debugPrint = "";

	var strTbl = document.getElementById("strtbl");
	var strTblLen = strTbl.rows.length;
	if (strTblLen > 1) {
		var strTblSize = parseInt(strTbl.rows[strTblLen - 1].cells[0].firstChild.data)
			+ strTbl.rows[strTblLen - 1].cells[1].firstChild.data.length + 1;

		debugPrint += "strTbl: size(" + strTblSize + ")\n\n";
		outString += escOut(strTblSize);

		for (var row = 1; row < strTblLen; row++) {
			outString += strTbl.rows[row].cells[1].firstChild.data + "%00";
		}
	}
	else
		outString += "%00";

    for (var i = 0; i < elmArray.length; i++) {
		var elm = elmArray[i];
		if (elm.length > 0) {
			var elmContent = elm.split(">");

			outString += parseTag(elmContent[0]);
			
			for (var j = 1; j < elmContent.length; j++) {
				outString += parseText(elmContent[j]);
			}
		}
    }

    return outString;
}

function generateWbxml()
{
    var srcStr = document.getElementById("source").value;
    var destElm = document.getElementById("generated");
	
    var wbxmlStr = parseSrc(srcStr);

    destElm.value = wbxmlStr;

	dumpDebug();
}