function serializeChildNodes(node)
{
  var result = "";
  for (var index = 0, length = node.childNodes.length; index < length; ++index)
    result += serializeNode(node.childNodes.item(index));
  return result;
}

function serializeNode(node)
{
  switch (node.nodeType)
  {
  case Node.ELEMENT_NODE:
    if (node.hasChildNodes())
      return "<" + node.nodeName + ">" + serializeChildNodes(node) + "</" + node.nodeName + ">";
    else
      return "<" + node.nodeName + "/>";

  case Node.TEXT_NODE:
    return node.nodeValue;

  case Node.CDATA_SECTION_NODE:
    return "<![CDATA[" + node.nodeValue + "]]>";

  case Node.COMMENT_NODE:
    return "<!--" + node.nodeValue + "-->";

  case Node.DOCUMENT_NODE:
  case Node.DOCUMENT_FRAGMENT_NODE:
    return serializeChildNodes(node);

  default:
    throw "serializeNode: invalid node";
  }
}
