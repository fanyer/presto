/* Bindings:
 *
 *  createNodeIterator: Document.prototype.createNodeIterator
 *  SHOW_TEXT:          NodeFilter.SHOW_TEXT
 *  nextNode:           NodeIterator.prototype.nextNode
 *  removeChild:        Node.prototype.removeChild
 *  TEXT_NODE:          Node.TEXT_NODE
 *  appendData:         CharacterData.prototype.appendData
 */

function Node.prototype.normalize ()
{
  var nodes = createNodeIterator.apply (this.ownerDocument, [this, SHOW_TEXT, null, false]), node;

  while (node = nextNode.apply (nodes))
    {
      if (node.nodeValue.length == 0)
        removeChild.apply (node.parentNode, [node]);
      else
        {
          var next, text = "";

          while ((next = node.nextSibling) && next.nodeType == TEXT_NODE)
            {
              text += next.nodeValue;
              removeChild.apply (next.parentNode, [next]);
            }

          if (text.length != 0)
            appendData.apply (node, [text]);
        }
    }
}
