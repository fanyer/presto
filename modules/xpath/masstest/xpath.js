
var XPathJS = (function () {

var T_INVALID = "T_INVALID";
var T_END = "T_END";

var T_AXIS = "T_AXIS";
var T_NODETEST = "T_NODETEST";
var T_NAMETEST = "T_NAMETEST";
var T_FUNCTION = "T_FUNCTION";
var T_VARIABLE = "T_VARIABLE";
var T_STRING = "T_STRING";
var T_NUMBER = "T_NUMBER";
var T_OPERATOR = "T_OPERATOR";

var RE_AXIS = /^(ancestor|ancestor-or-self|attribute|child|descendant|descendant-or-self|following|following-sibling|namespace|parent|preceding|preceding-sibling|self)::(.*)$/;
var RE_STRING_S = "(\".*?\"|\'.*?\')";
var RE_STRING = new RegExp ("^" + RE_STRING_S + "(.*)$");
var RE_NUMBER = new RegExp ("^(\\.[0-9]+|[0-9]+(?:\\.[0-9]*)?)(.*)$")
var RE_QNAME_S = "((?:([A-Za-z_][A-Za-z0-9_\-]*):)?([A-Za-z_][A-Za-z0-9_\-]*|\\*))"
var RE_NODETEST = new RegExp ("^(node|text|comment|processing-instruction)\s*\\(\\s*" + RE_STRING_S + "?\\s*\\)(.*)$");
var RE_NAMETEST = new RegExp ("^" + RE_QNAME_S + "(.*)$");
var RE_OPERATOR = new RegExp ("^(and|or|div|mod|!=|<=|>=|[\-/()\\[\\]\\+\\*%/=|,])(.*)$");
var RE_FUNCTION = new RegExp ("^" + RE_QNAME_S + "(?=\\s*\\()(.*)$");
var RE_VARIABLE = new RegExp ("^\\$" + RE_QNAME_S + "(.*)$");

var RE_SPACE = new RegExp ("^\\s+(.*)$");

function tokenize (string)
{
  var tokens = [];

  while (string)
    {
      var last_type = tokens.length ? tokens[tokens.length - 1][0] : T_INVALID;
      var last_value = tokens.length ? tokens[tokens.length - 1][1] : "";
      var match;

      var expecting_binary_operator = last_type == T_NODETEST || last_type == T_NAMETEST || last_type == T_OPERATOR && (last_value == ")" || last_value == "]");

      if (match = RE_SPACE.exec (string))
        string = match[match.length - 1];
      else if (expecting_binary_operator && (match = RE_OPERATOR.exec (string)))
        {
          tokens.push ([T_OPERATOR, match[1]]);
          string = match[match.length - 1];
        }
      else if (match = RE_NODETEST.exec (string))
        {
          if (match[1] != "processing-instruction" && match[2])
            throw "confused now: " + string;
          if (last_type != T_AXIS)
            tokens.push ([T_AXIS, "child"]);
          tokens.push ([T_NODETEST, match[1], eval (match[2])]);
          string = match[match.length - 1];
        }
      else if (match = RE_FUNCTION.exec (string))
        {
          if (last_type == T_AXIS)
            throw "confused now: " + string;
          tokens.push ([T_FUNCTION, match[1], match[2], match[3]]);
          string = match[match.length - 1];
        }
      else if (match = RE_AXIS.exec (string))
        {
          tokens.push ([T_AXIS, match[1]]);
          string = match[match.length - 1];
        }
      else if ((match = RE_NAMETEST.exec (string)) && ((match[1] != "*" && match[1] != "div" && match[1] != "mod") || last_type == T_AXIS || last_type == T_OPERATOR && last_value == "/"))
        {
          if (last_type != T_AXIS)
            tokens.push ([T_AXIS, "child"]);
          tokens.push ([T_NAMETEST, match[1], match[2], match[3]]);
          string = match[match.length - 1];
        }
      else if (match = RE_STRING.exec (string))
        {
          tokens.push ([T_STRING, eval (match[1])]);
          string = match[match.length - 1];
        }
      else if (match = RE_NUMBER.exec (string))
        {
          tokens.push ([T_NUMBER, eval (match[1])]);
          string = match[match.length - 1];
        }
      else if (match = RE_VARIABLE.exec (string))
        {
          tokens.push ([T_VARIABLE, match[1], match[2], match[3]]);
          string = match[match.length - 1];
        }
      else if (string.indexOf ("//") == 0)
        {
          tokens.push ([T_OPERATOR, "/"]);
          tokens.push ([T_AXIS, "descendant-or-self"]);
          tokens.push ([T_NODETEST, "node"]);
          tokens.push ([T_OPERATOR, "/"]);
          string = string.substring (2);
        }
      else if (string.indexOf ("..") == 0)
        {
          tokens.push ([T_AXIS, "parent"]);
          tokens.push ([T_NODETEST, "node"]);
          string = string.substring (2);
        }
      else if (string.indexOf (".") == 0)
        {
          tokens.push ([T_AXIS, "self"]);
          tokens.push ([T_NODETEST, "node"]);
          string = string.substring (1);
        }
      else if (string.indexOf ("@") == 0)
        {
          tokens.push ([T_AXIS, "attribute"]);
          string = string.substring (1);
        }
      else if (match = RE_OPERATOR.exec (string))
        {
          tokens.push ([T_OPERATOR, match[1]]);
          string = match[match.length - 1];
        }
      else
        throw "confused now: " + string;
    }

  tokens.push ([T_END, ""]);
  return tokens;
}

var E_OR = "E_OR";
var E_AND = "E_AND";
var E_EQUALITY = "E_EQUALITY";
var E_RELATIONAL = "E_RELATIONAL";
var E_ADDITIVE = "E_ADDITIVE";
var E_MULTIPLICATIVE = "E_MULTIPLICATIVE";
var E_UNARY = "E_UNARY";
var E_UNION = "E_UNION";
var E_PATH = "E_PATH";

function parseExpression (expression, tokens)
{
  var expr = null;

  if (tokens[0][0] == T_OPERATOR && tokens[0][1] == "-")
    {
      tokens.shift ();
      expr = new Negate (parseExpression (E_UNARY, tokens));
    }
  else if (tokens[0][0] == T_FUNCTION)
    {
      var name = new QName (tokens[0][2], tokens[0][3]);
      tokens.shift ();
      if (tokens[0][0] != T_OPERATOR || tokens[0][1] != "(")
        throw "confused now: " + tokens.join (" => ");
      tokens.shift ();
      var args = [];
      while (tokens[0][0] != T_OPERATOR || tokens[0][1] != ")")
        {
          args.push (parseExpression (E_OR, tokens));
          if (tokens[0][0] != T_OPERATOR || tokens[0][1] != "," && tokens[0][1] != ")")
            throw "confused now: " + tokens.join (" => ");
          if (tokens[0][1] == ",")
            {
              tokens.shift ();
              if (tokens[0][0] == T_OPERATOR && tokens[0][1] == ")")
                throw "confused now: " + tokens.join (" => ");
            }
        }
      tokens.shift ();
      expr = new FunctionCall (name, args);
    }
  else if (tokens[0][0] == T_VARIABLE)
    {
      var name = new QName (tokens[0][2], tokens[0][3]);
      tokens.shift ();
      expr = new VariableReference (name);
    }
  else if (tokens[0][0] == T_AXIS || tokens[0][0] == T_OPERATOR && tokens[0][1] == "/")
    {
      var absolute;
      if (tokens[0][0] == T_OPERATOR && tokens[0][1] == "/")
        {
          absolute = true;
          tokens.shift ();
        }
      else
        absolute = false;
      expr = new Path (parsePath (tokens), absolute, null);
    }
  else if (tokens[0][0] == T_OPERATOR && tokens[0][1] == "(")
    {
      tokens.shift ();
      expr = parseExpression (E_OR, tokens);
      if (tokens[0][0] != T_OPERATOR || tokens[0][1] != ")")
        throw "confused now: " + tokens.join ("");
      tokens.shift ();
    }
  else if (tokens[0][0] == T_STRING || tokens[0][0] == T_NUMBER)
    {
      expr = new Literal (tokens[0][1]);
      tokens.shift ();
    }

  while (true)
    {
      /* Binary expression: */

      var operator = tokens[0][0] == T_OPERATOR ? tokens[0][1] : false;

      switch (expression)
        {
        case E_OR:
          if (operator == "or")
            {
              tokens.shift ();
              expr = new Or (expr, parseExpression (E_AND, tokens));
              continue;
            }

        case E_AND:
          if (operator == "and")
            {
              tokens.shift ();
              expr = new And (expr, parseExpression (E_EQUALITY, tokens));
              continue;
            }

        case E_EQUALITY:
          switch (operator)
            {
            case "=":
            case "!=":
              tokens.shift ();
              expr = new Equality (operator, expr, parseExpression (E_RELATIONAL, tokens));
              continue;
            }

        case E_RELATIONAL:
          switch (operator)
            {
            case "<":
            case ">":
            case "<=":
            case ">=":
              tokens.shift ();
              expr = new Relational (operator, expr, parseExpression (E_ADDITIVE, tokens));
              continue;
            }

        case E_ADDITIVE:
          switch (operator)
            {
            case "+":
            case "-":
              tokens.shift ();
              expr = new Additive (operator, expr, parseExpression (E_MULTIPLICATIVE, tokens));
              continue;
            }

        case E_MULTIPLICATIVE:
          switch (operator)
            {
            case "*":
            case "div":
            case "mod":
              tokens.shift ();
              expr = new Multiplicative (operator, expr, parseExpression (E_UNARY, tokens));
              continue;
            }

        case E_UNARY:
        case E_UNION:
          if (operator == "|")
            {
              tokens.shift ();
              expr = new Union (expr, parseExpression (E_PATH, tokens));
              continue;
            }

        default:
          if (operator == "[")
            {
              tokens.shift ();
              expr = new Predicate (expr, parseExpression (E_OR, tokens));
              if (tokens[0][0] != T_OPERATOR || tokens[0][1] != "]")
                throw "confused now: " + tokens.join (" => ");
              tokens.shift ();
              continue;
            }
          else if (operator == "/")
            {
              tokens.shift ();
              expr = new Path (parsePath (tokens), false, expr);
              continue;
            }
        }

      break;
    }

  return expr;
}

function QName (prefix, localpart)
{
  if (prefix)
    this.namespaceURI = prefix;
  else
    this.namespaceURI = "";

  this.prefix = prefix;
  this.localName = localpart;

  this.toString = function QName_toString ()
    {
      if (this.uri)
        return this.namespaceURI + " :: " + this.localName;
      else
        return this.localName;
    }
}

function boolean (value)
{
  switch (typeof value)
    {
    case "boolean":
      return value;

    case "number":
      return value != 0;

    case "string":
    case "object":
      return value.length != 0;
    }
}

function strip (string)
{
  return /^\s*(.*?)\s*$/.exec (string)[1];
}

function number (value)
{
  switch (typeof value)
    {
    case "boolean":
      return value ? 1 : 0;

    case "number":
      return value;

    case "string":
    case "object":
      var match = RE_NUMBER.exec (strip (string (value)));
      if (match)
        return eval (match[1]);
      else
        return NaN;
    }
}

function string (value)
{
  switch (typeof value)
    {
    case "boolean":
    case "number":
    case "string":
      return String (value);

    case "object":
      return value[0] ? stringValue (value[0]) : "";
    }
}

function stringValue (node)
{
  function inner (node)
    {
      switch (node.nodeType)
        {
        case Node.TEXT_NODE:
        case Node.CDATA_SECTION_NODE:
          return node.nodeValue;

        case Node.ELEMENT_NODE:
          var stringValue = "";
          for (var child = node.firstChild; child; child = child.nextSibling)
            stringValue += inner (child);
          return stringValue;
        }

      return "";
    }

  switch (node.nodeType)
    {
    case Node.TEXT_NODE:
    case Node.CDATA_SECTION_NODE:
      while (node.previousSibling && (node.previousSibling.nodeType == Node.TEXT_NODE || node.previousSibling.nodeType == Node.CDATA_SECTION_NODE))
        node = node.previousSibling;
      var stringValue = "";
      while (node && (node.nodeType == Node.TEXT_NODE || node.nodeType == Node.CDATA_SECTION_NODE))
        {
          stringValue += node.nodeValue;
          node = node.nextSibling;
        }
      return stringValue;

    case Node.ELEMENT_NODE:
    case Node.DOCUMENT_NODE:
    case Node.DOCUMENT_FRAGMENT_NODE:
      var stringValue = "";
      for (var child = node.firstChild; child; child = child.nextSibling)
        stringValue += inner (child);
      return stringValue;

    case Node.ATTRIBUTE_NODE:
    case Node.COMMENT_NODE:
    case XPathNamespace.XPATH_NAMESPACE_NODE:
      return node.nodeValue;
    }

  return "";
}

function Negate (expression)
{
  this.expression = expression;

  this.evaluate = function Negate_evaluate (context)
    {
      return -this.expression.evaluate (context);
    }
}

var nsresolver = null;

var functions =
  {
    "position": function (context, args)
      {
        if (args.length != 0)
          throw "invalid arguments to position: " + args.join (", ");
        return context.position;
      },

    "last": function (context, args)
      {
        if (args.length != 0)
          throw "invalid arguments to last: " + args.join (", ");
        return context.size;
      },

    "number": function (context, args)
      {
        if (args.length == 0)
          return number ([context.node]);
        else if (args.length == 1)
          return number (args[0]);
        else
          throw "invalid arguments to number: " + args.join (", ");
      },

    "boolean": function (context, args)
      {
        if (args.length == 1)
          return boolean (args[0]);
        else
          throw "invalid arguments to boolean: " + args.join (", ");
      },

    "string": function (context, args)
      {
        if (args.length == 0)
          return string ([context.node]);
        else if (args.length == 1)
          return string (args[0]);
        else
          throw "invalid arguments to string: " + args.join (", ");
      },

    "count": function (context, args)
      {
        if (args.length != 1 || typeof args[0] != "object")
          throw "invalid arguments to count: " + args.join (", ");
        return args[0].length;
      },

    "id": function (context, args)
      {
        if (args.length != 1)
          throw "invalid arguments to id: " + args.join (", ");
        else if (typeof args[0] == "object")
          {
            var nodeset = [];
            for (var index = 0; index < args[0].length; ++index)
              nodeset.push.apply (nodeset, arguments.callee.call (null, context, [stringValue (args[0][index])]));
            return sortNodeSet (nodeset);
          }
        else
          {
            var ids = string (args[0]).split (/\s+/), nodeset = [], node;
            for (var index = 0; index < ids.length; ++index)
              if (node = ownerDocument (context.node).getElementById (ids[index]))
                nodeset.push (node);
            return sortNodeSet (nodeset);
          }
      },

    "local-name": function (context, args)
      {
        if (args.length == 0)
          return localName (context.node);
        else if (args.length == 1)
          return args[0].length != 0 ? localName (args[0][0]) : "";
        else
          throw "invalid arguments to local-name: " + args.join (", ");
      },

    "namespace-uri": function (context, args)
      {
        if (args.length == 0)
          return namespaceURI (context.node);
        else if (args.length == 1)
          return args[0].length != 0 ? namespaceURI (args[0][0]) : "";
        else
          throw "invalid arguments to namespace-uri: " + args.join (", ");
      },

    "name": function (context, args)
      {
        if (args.length == 0)
          return qName (context.node);
        else if (args.length == 1)
          return args[0].length != 0 ? qName (args[0][0]) : "";
        else
          throw "invalid arguments to name: " + args.join (", ");
      },

    "concat": function (context, args)
      {
        if (args.length < 2)
          throw "too few arguments to concat: " + args.join (", ");
        else
          {
            var result = "";
            for (var index = 0; index < args.length; ++index)
              result += string (args[index]);
            return result;
          }
      },

    "starts-with": function (context, args)
      {
        if (args.length != 2)
          throw "invalid arguments to starts-with: " + args.join (", ");
        return string (args[0]).indexOf (string (args[1])) == 0;
      },

    "contains": function (context, args)
      {
        if (args.length != 2)
          throw "invalid arguments to contains: " + args.join (", ");
        return string (args[0]).indexOf (string (args[1])) != -1;
      },

    "substring-before": function (context, args)
      {
        if (args.length != 2)
          throw "invalid arguments to substring-before: " + args.join (", ");
        var arg0 = string (args[0]), arg1 = string (args[1]);
        if (arg0.indexOf (arg1) == -1)
          return "";
        else
          return arg0.substring (0, arg0.indexOf (arg1));
      },

    "substring-after": function (context, args)
      {
        if (args.length != 2)
          throw "invalid arguments to substring-after: " + args.join (", ");
        var arg0 = string (args[0]), arg1 = string (args[1]);
        if (arg0.indexOf (arg1) == -1)
          return "";
        else
          return arg0.substring (arg0.indexOf (arg1) + arg1.length);
      },

    "substring": function (context, args)
      {
        if (args.length != 2 && args.length != 3)
          throw "invalid arguments to substring: " + args.join (", ");
        var arg0 = string (args[0]), arg1 = number (args[1]), arg2 = args.length == 3 ? number (args[2]) : arg0.length;
        var start = Math.max (0, Math.min (arg0.length, Math.round (arg1) - 1)), length = Math.min (arg0.length - start, Math.round (arg1 + arg2) - 1);
        return arg0.substr (start, length);
      },

    "string-length": function (context, args)
      {
        if (args.length == 0)
          return stringValue (context.node).length;
        else if (args.length == 1)
          return string (args[0]).length;
        else
          throw "too many arguments to string-length: " + args.join (", ");
      },

    "normalize-space": function (context, args)
      {
        var result;
        if (args.length == 0)
          result = stringValue (context.node);
        else if (args.length == 1)
          result = string (args[0]);
        else
          throw "too many arguments to normalize-space: " + args.join (", ");

        return result.replace (/\s+/g, " ").replace (/^ /g, "").replace (/ $/g, "");
      },

    "true": function (context, args)
      {
        if (args.length != 0)
          throw "too many arguments to true: " + args.join (", ");
        else
          return true;
      },

    "false": function (context, args)
      {
        if (args.length != 0)
          throw "too many arguments to false: " + args.join (", ");
        else
          return false;
      },

    "not": function (context, args)
      {
        if (args.length != 1)
          throw "invalid arguments to not: " + args.join (", ");
        else
          return !boolean (args[0]);
      },

    "round": function (context, args)
      {
        if (args.length != 1)
          throw "invalid arguments to round: " + args.join (", ");
        else
          return Math.round (number (args[0]));
      },

    "floor": function (context, args)
      {
        if (args.length != 1)
          throw "invalid arguments to floor: " + args.join (", ");
        else
          return Math.floor (number (args[0]));
      },

    "ceiling": function (context, args)
      {
        if (args.length != 1)
          throw "invalid arguments to ceiling: " + args.join (", ");
        else
          return Math.ceil (number (args[0]));
      },

    "sum": function (context, args)
      {
        if (args.length != 1 || typeof args[0] != "object")
          throw "invalid arguments to sum: " + args.join (", ");
        else
          {
            var sum = 0;
            for (var index = 0; index < args[0].length; ++index)
              sum += number (args[0][index]);
            return sum;
          }
      },

    "translate": function (context, args)
      {
        if (args.length != 3)
          throw "invalid arguments to translate: " + args.join (", ");
        else
          {
            var arg0 = string (args[0]), arg1 = string (args[1]), arg2 = string (args[2]);
            for (var index = 0; index < arg1.length; ++index)
              arg0 = arg0.replace (arg1.charAt (index), arg2.substr (index, 1));
            return arg0;
          }
      },

    "lang": function (context, args)
      {
        throw "lang: function not supported.  Sorry.";
      }
  };

function FunctionCall (name, args)
{
  this.implementation = functions[name];
  if (!this.implementation)
    throw "unknown function: " + name;
  this.args = args;

  this.evaluate = function FunctionCall_evaluate (context)
    {
      var values = [];
      for (var index = 0; index < this.args.length; ++index)
        values.push (this.args[index].evaluate (context));
      return this.implementation (context, values);
    }
}

function VariableReference (name)
{
  throw "variable?  I don't have no variables!";
}

function Literal (value)
{
  this.value = value;

  this.evaluate = function Literal_evaluate (context)
    {
      return this.value;
    }
}

function Or (lhs, rhs)
{
  this.lhs = lhs;
  this.rhs = rhs;

  this.evaluate = function Or_evaluate (context)
    {
      return boolean (this.lhs.evaluate (context)) || boolean (this.rhs.evaluate (context));
    }
}

function And (lhs, rhs)
{
  this.lhs = lhs;
  this.rhs = rhs;

  this.evaluate = function And_evaluate (context)
    {
      return boolean (this.lhs.evaluate (context)) && boolean (this.rhs.evaluate (context));
    }
}

function Equality (operator, lhs, rhs)
{
  this.operator = operator;
  this.lhs = lhs;
  this.rhs = rhs;

  this.evaluate = function Equality_evaluate (context)
    {
      var lhsv = this.lhs.evaluate (context), rhsv = this.rhs.evaluate (context);
      if (typeof lhsv == "boolean" || typeof rhsv == "boolean")
        return boolean (lhsv) == boolean (rhsv);
      else if (typeof lhsv == "object" && typeof rhsv == "object")
        {
          var lhst = {};
          for (var index = 0; index < lhsv.length; ++index)
            lhst[stringValue (lhsv[index])] = null;
          for (var index = 0; index < rhsv.length; ++index)
            if (stringValue (rhsv[index]) in lhst)
              return this.operator == "=";
          return this.operator != "=";
        }
      else if (typeof lhsv == "object" || typeof rhsv == "object")
        {
          var nodeset, primitive;
          if (typeof lhsv == "object")
            {
              nodeset = lhsv;
              primitive = rhsv;
            }
          else
            {
              nodeset = rhsv;
              primitive = lhsv;
            }
          for (var index = 0; index < nodeset.length; ++index)
            if (typeof primitive == "number" ? primitive == number ([nodeset[index]]) : primitive == string ([nodeset[index]]))
              return this.operator == "=";
          return this.operator != "=";
        }
      else if (typeof lhsv == "boolean" || typeof rhsv == "boolean")
        return (boolean (lhsv) == boolean (rhsv)) == (this.operator == "=");
      else if (typeof lhsv == "number" || typeof rhsv == "number")
        return (number (lhsv) == number (rhsv)) == (this.operator == "=");
      else
        return (lhsv == rhsv) == (this.operator == "=");
    }
}

function Relational (operator, lhs, rhs)
{
  this.operator = operator;
  this.lhs = lhs;
  this.rhs = rhs;

  this.evaluate = function Relational_evaluate (context)
    {
      function compare (operator, lhsn, rhsn)
        {
          switch (operator)
            {
            case "<":
              return lhsn < rhsn;

            case ">":
              return lhsn > rhsn;

            case "<=":
              return lhsn <= rhsn;

            case ">=":
              return lhsn >= rhsn;
            }
        }

      var lhsv = this.lhs.evaluate (context), rhsv = this.rhs.evaluate (context);
      if (typeof lhsv == "object" && typeof rhsv == "object")
        {
          for (var index = 0; index < lhsv.length; ++index)
            lhsv[index] = number (lhsv[index]);
          for (var index = 0; index < rhsv.length; ++index)
            rhsv[index] = number (rhsv[index]);
          for (var lhsi = 0; lhsi < lhsv.length; ++lhsi)
            for (var rhsi = 0; rhsi < rhsv.length; ++rhsi)
              if (compare (this.operator, lhsv[lhsi], rhsv[rhsi]))
                return true;
          return false;
        }
      else if (typeof lhsv == "object" || typeof rhsv == "object")
        {
          var nodeset, primitive;
          if (typeof lhsv == "object")
            {
              nodeset = lhsv;
              primitive = rhsv;
            }
          else
            {
              nodeset = rhsv;
              primitive = lhsv;
            }
          for (var index = 0; index < nodes.length; ++index)
            if (compare (this.operator, primitive, number (nodes[index])))
              return true;
          return false;
        }
      else
        return compare (this.operator, number (lhsv), number (rhsv));
    }
}

function Additive (operator, lhs, rhs)
{
  this.operator = operator;
  this.lhs = lhs;
  this.rhs = rhs;

  this.evaluate = function Additive_evaluate (context)
    {
      var lhsv = number (this.lhs.evaluate (context)), rhsv = number (this.rhs.evaluate (context));
      switch (this.operator)
        {
        case "+":
          return lhsv + rhsv;
        case "-":
          return lhsv - rhsv;
        }
    }
}

function Multiplicative (operator, lhs, rhs)
{
  this.operator = operator;
  this.lhs = lhs;
  this.rhs = rhs;

  this.evaluate = function Multiplicative_evaluate (context)
    {
      var lhsv = number (this.lhs.evaluate (context)), rhsv = number (this.rhs.evaluate (context));
      switch (this.operator)
        {
        case "*":
          return lhsv * rhsv;
        case "div":
          return lhsv / rhsv;
        case "mod":
          return lhsv % rhsv;
        }
    }
}

function Union (lhs, rhs)
{
  this.lhs = lhs;
  this.rhs = rhs;

  this.evaluate = function Union_evaluate (context)
    {
      var lhsv = this.lhs.evaluate (context), rhsv = this.rhs.evaluate (context);
      if (typeof lhsv != "object" || typeof rhsv != "object")
        throw "non-node-set operand in union expression";
      var rhsl = rhsv.length;
      while (lhsv.length != 0)
        {
          var lhsn = lhsv.pop ();
          for (var rhsi = 0; rhsi < rhsl; ++rhsi)
            if (lhsn == rhsv[rhsi])
              {
                lhsn = null;
                break;
              }
          if (lhsn)
            rhsv.push (lhsn);
        }
      return rhsv;
    }
}

function Predicate (lhs, rhs)
{
  this.lhs = lhs;
  this.rhs = rhs;

  this.evaluate = function Predicate_evaluate (context)
    {
      var lhsv = this.lhs.evaluate (context), result = [];
      if (typeof lhsv != "object")
        throw "non-node-set operand in predicate expression";
      for (var lhsi = 0; lhsi < lhsv.length; ++lhsi)
        {
          var rhsv = this.rhs.evaluate ({ node: lhsv[lhsi], position: lhsi + 1, size: lhsv.length });

          if (typeof rhsv == "number")
            {
              if (rhsv == lhsi + 1)
                result.push (lhsv[lhsi]);
            }
          else if (boolean (rhsv))
            result.push (lhsv[lhsi]);
        }
      return result;
    }
}

function indexNodes (node, index)
{
  node.doIndex = ++index;
  node.namespaces = null;

  if (node.attributes)
    {
      var declaresNamespaces = false;

      for (var aindex = 0; aindex < node.attributes.length; ++aindex)
        if (node.attributes[aindex].namespaceURI != "http://www.w3.org/2000/xmlns/")
          node.attributes[aindex].doIndex = ++index;
        else
          declaresNamespaces = true;

      node.namespaces = [];

      if (declaresNamespaces)
        {
          var ancestor;

          ancestor = node;
          outerLoop: while (ancestor && ancestor.attributes)
            {
              for (var aindex = 0; aindex < ancestor.attributes.length; ++aindex)
                if (ancestor.attributes[aindex].namespaceURI == "http://www.w3.org/2000/xmlns/" && ancestor.attributes[aindex].prefix == null)
                  {
                    if (ancestor.attributes[aindex].value)
                      node.namespaces.push ({ nodeType: XPathNamespace.XPATH_NAMESPACE_NODE, nodeName: "#namespace", doIndex: ++index, localName: "", prefix: "", namespaceURI: ancestor.attributes[aindex].value, ownerElement: node, ownerDocument: node.ownerDocument, fakeNamespaceNode: true });
                    break outerLoop;
                  }
              ancestor = ancestor.parentNode;
            }

          var namespaces = {};

          ancestor = node;
          while (ancestor && ancestor.attributes)
            {
              for (var aindex = ancestor.attributes.length - 1; aindex >= 0; --aindex)
                if (ancestor.attributes[aindex].namespaceURI == "http://www.w3.org/2000/xmlns/" && ancestor.attributes[aindex].prefix)
                  {
                    var prefix = ancestor.attributes[aindex].localName, namespaceURI = ancestor.attributes[aindex].nodeValue;

                    if (prefix in namespaces)
                      continue;
                    namespaces[prefix] = true;

                    node.namespaces.push ({ nodeType: XPathNamespace.XPATH_NAMESPACE_NODE, nodeName: "#namespace", doIndex: ++index, localName: prefix, prefix: prefix, namespaceURI: namespaceURI, ownerElement: node, ownerDocument: node.ownerDocument, fakeNamespaceNode: true });
                  }

              ancestor = ancestor.parentNode;
            }
        }
      else if (node.parentNode && node.parentNode.namespaces)
        {
          var pnamespaces = node.parentNode.namespaces;

          for (var nindex = 0; nindex < pnamespaces.length; ++nindex)
            {
              var pnamespace = pnamespaces[nindex];
              node.namespaces.push ({ nodeType: XPathNamespace.XPATH_NAMESPACE_NODE, nodeName: "#namespace", doIndex: ++index, localName: pnamespace.localName, prefix: pnamespace.prefix, namespaceURI: pnamespace.namespaceURI, ownerElement: node, ownerDocument: node.ownerDocument, fakeNamespaceNode: true });
            }
        }
    }

  if (node.nodeType != Node.ATTRIBUTE_NODE)
    {
      var child = node.firstChild;

      while (child)
        {
          index = indexNodes (child, index);
          child = child.nextSibling;
        }
    }

  return index;
}

function sortNodeSet (nodeset)
{
  nodeset.sort (function (l, r)
    {
      if (l.doIndex < r.doIndex)
        return -1;
      else if (l.doIndex > r.doIndex)
        return +1;
      else
        if (l.doSubIndex < r.doSubIndex)
          return -1;
        else if (l.doSubIndex > r.doSubIndex)
          return +1;
        else
          return 0;
    });

  /* Remove duplicates. */
  for (var nindex = 1; nindex < nodeset.length;)
    if (compareNodes (nodeset[nindex - 1], nodeset[nindex]))
      nodeset.splice (nindex, 1);
    else
      ++nindex;

  return nodeset;
}

function parentNode (node)
{
  if (node.ownerElement)
    return node.ownerElement;
  else
    return node.parentNode;
}

function nextSibling (node)
{
  if (node.nodeType == Node.TEXT_NODE || node.nodeType == Node.CDATA_SECTION_NODE)
    while (node.nextSibling && (node.nextSibling.nodeType == Node.TEXT_NODE || node.nextSibling.nodeType == Node.CDATA_SECTION_NODE))
      node = node.nextSibling;
  return node.nextSibling;
}

function previousSibling (node)
{
  node = node.previousSibling;
  if (node && (node.nodeType == Node.TEXT_NODE || node.nodeType == Node.CDATA_SECTION_NODE))
    while (node.previousSibling && (node.previousSibling.nodeType == Node.TEXT_NODE || node.previousSibling.nodeType == Node.CDATA_SECTION_NODE))
      node = node.previousSibling;
  return node;
}

function ownerDocument (node)
{
  if (node.nodeType == Node.DOCUMENT_NODE)
    return node;
  else
    return node.ownerDocument;
}

function localName (node)
{
  if (node.nodeType == Node.PROCESSING_INSTRUCTION_NODE)
    return node.target;
  else if (node.localName)
    return node.localName;
  else
    return "";
}

function namespaceURI (node)
{
  if (node.nodeType == XPathNamespace.XPATH_NAMESPACE_NODE)
    return "";
  else if (node.namespaceURI)
    return node.namespaceURI;
  else
    return "";
}

function qName (node)
{
  if (node.nodeType == Node.ELEMENT_NODE || node.nodeType == Node.ATTRIBUTE_NODE)
    return node.prefix ? node.prefix + ":" + node.localName : node.localName;
  else
    return localName (node);
}

function compareNodes (node1, node2)
{
  if (node1 == node2)
    return true;
  else if (node1 && node2)
    if ((node1.nodeType == Node.TEXT_NODE || node1.nodeType == Node.CDATA_SECTION_NODE) && (node2.nodeType == Node.TEXT_NODE || node2.nodeType == Node.CDATA_SECTION_NODE))
      {
        if (node1.parentNode == node2.parentNode)
          {
            while (node1.previousSibling && (node1.previousSibling.nodeType == Node.TEXT_NODE || node1.previousSibling.nodeType == Node.CDATA_SECTION_NODE))
              node1 = node1.previousSibling;
            while (node2.previousSibling && (node2.previousSibling.nodeType == Node.TEXT_NODE || node2.previousSibling.nodeType == Node.CDATA_SECTION_NODE))
              node2 = node2.previousSibling;
            if (node1 == node2)
              return true;
          }
      }
    else if (node1.nodeType == XPathNamespace.XPATH_NAMESPACE_NODE && node2.nodeType == XPathNamespace.XPATH_NAMESPACE_NODE)
      {
        if (node1.ownerElement == node2.ownerElement && node1.prefix == node2.prefix && node1.namespaceURI == node2.namespaceURI)
          return true;
      }
  return false;
}

function nodeSetToArray (nodeset)
{
  var array = [];
  for (var doIndex in nodeset)
    array.push (nodeset[doIndex]);
  array.sort (function (left, right) { return left.doIndex < right.doIndex ? -1 : left.doIndex > right.doIndex ? 1 : 0; });
  return array;
}

function pushDescendants (nodeset, node)
{
  var size = 0;
  if (node = node.firstChild)
    do
      {
        nodeset[node.doIndex] = node;
        size += 1 + pushDescendants (nodeset, node);
      }
    while (node = nextSibling (node));
  return size;
}


function Path (steps, absolute, lhs)
{
  this.steps = steps;
  this.absolute = absolute;
  this.lhs = lhs;

  this.evaluate = function Path_evaluate (context)
    {
      var contextNodeSet = [];

      if (!this.lhs)
        {
          var contextNode = context.node;

          if (this.absolute && contextNode.ownerDocument)
            contextNode = contextNode.ownerDocument;

          contextNodeSet[contextNode.doIndex] = contextNode;
        }
      else
        {
          var contextNodeList = this.lhs.evaluate (context);

          if (typeof contextNodeList != "object")
            throw "non-node-set operand in location path expression";

          if (contextNodeList.length == 0)
            return [];

          for (var index = 0; index < contextNodeList.length; ++index)
            contextNodeSet[contextNodeList[index].doIndex] = contextNodeList[index];
        }

      for (var sindex = 0; sindex < this.steps.length; ++sindex)
        {
          var step = this.steps[sindex], stepFinal = [], size1 = 0;
          var simple = step.nodetestType == "NodeType" && step.nodetestNodeType == "node" && step.predicates.length == 0;

          for (var doIndex in contextNodeSet)
            {
              var intermediate1, node = contextNodeSet[doIndex];

              if (simple)
                intermediate1 = stepFinal;
              else
                intermediate1 = [];

              switch (step.axis)
                {
                case "ancestor":
                  while (node = parentNode (node))
                    {
                      intermediate1[node.doIndex] = node;
                      ++size1;
                    }
                  break;

                case "ancestor-or-self":
                  do
                    {
                      intermediate1[node.doIndex] = node;
                      ++size1;
                    }
                  while (node = parentNode (node));
                  break;

                case "attribute":
                  if (node.attributes)
                    for (var aindex = 0; aindex < node.attributes.length; ++aindex)
                      {
                        var attr = node.attributes[aindex];
                        if (attr.namespaceURI != "http://www.w3.org/2000/xmlns/")
                          {
                            intermediate1[attr.doIndex] = attr;
                            ++size1;
                          }
                      }
                  break;

                case "child":
                  if (node.nodeType != Node.ATTRIBUTE_NODE)
                    if (node = node.firstChild)
                      {
                        do
                          {
                            intermediate1[node.doIndex] = node;
                            ++size1;
                          }
                        while (node = nextSibling (node));
                      }
                  break;

                case "descendant":
                  if (!node.ownerElement)
                    size1 += pushDescendants (intermediate1, node);
                  break;

                case "descendant-or-self":
                  intermediate1[node.doIndex] = node;
                  ++size1;
                  if (!node.ownerElement)
                    size1 += pushDescendants (intermediate1, node);
                  break;

                case "following":
                  if (node.ownerElement)
                    {
                      node = node.ownerElement;
                      size1 += pushDescendants (intermediate1, node);
                    }
                  while (true)
                    if (nextSibling (node))
                      {
                        node = nextSibling (node);
                        intermediate1[node.doIndex] = node;
                        size1 += 1 + pushDescendants (intermediate1, node);
                      }
                    else if (parentNode (node))
                      node = parentNode (node);
                    else
                      break;
                  break;

                case "following-sibling":
                  while (node = nextSibling (node))
                    {
                      intermediate1[node.doIndex] = node;
                      ++size1;
                    }
                  break;

                case "namespace":
                  if (node.namespaces)
                    {
                      for (var index = 0, namespaces = node.namespaces; index < namespaces.length; ++index)
                        intermediate1[namespaces[index].doIndex] = namespaces[index];
                      size1 += namespaces.length;
                    }
                  break;

                case "parent":
                  if (node = parentNode (node))
                    {
                      intermediate1[node.doIndex] = node;
                      ++size1;
                    }
                  break;

                case "preceding":
                  while (true)
                    if (previousSibling (node))
                      {
                        node = previousSibling (node);
                        intermediate1[node.doIndex] = node;
                        size1 += 1 + pushDescendants (intermediate1, node);
                      }
                    else if (parentNode (node))
                      node = parentNode (node);
                    else
                      break;
                  break;

                case "preceding-sibling":
                  while (node = previousSibling (node))
                    {
                      intermediate1[node.doIndex] = node;
                      ++size1;
                    }
                  break;

                case "self":
                  intermediate1[node.doIndex] = node;
                  ++size1;
                  break;
                }

              if (!simple)
                {
                  var intermediate2 = [], size2 = 0;

                  if (step.nodetestType != "NodeType" || step.nodetestNodeType != "node")
                    for (var doIndex in intermediate1)
                      {
                        var node = intermediate1[doIndex];

                        if (step.nodetestType == "NodeType")
                          switch (step.nodetestNodeType)
                            {
                            case "element":
                              if (node.nodeType != Node.ELEMENT_NODE)
                                continue;
                              break;

                            case "text":
                              if (node.nodeType != Node.TEXT_NODE && node.nodeType != Node.CDATA_SECTION_NODE)
                                continue;
                              break;

                            case "comment":
                              if (node.nodeType != Node.COMMENT_NODE)
                                continue;
                              break;

                            case "processing-instruction":
                              if (node.nodeType != Node.PROCESSING_INSTRUCTION_NODE)
                                if (!step.nodetestPITarget || node.target != step.nodetestPITarget)
                                  continue;
                            }
                        else if (step.nodetestType == "NameTest")
                          {
                            switch (step.axis)
                              {
                              case "attribute":
                                if (node.nodeType != Node.ATTRIBUTE_NODE)
                                  continue;
                                break;

                              case "namespace":
                                if (node.nodeType != XPathNamespace.XPATH_NAMESPACE_NODE)
                                  continue;
                                break;

                              default:
                                if (node.nodeType != Node.ELEMENT_NODE)
                                  continue;
                                break;
                              }

                            if (step.nodetestName.namespaceURI || step.nodetestName.localName != "*")
                              if (step.nodetestName.namespaceURI != namespaceURI (node))
                                continue;
                              else if (step.nodetestName.localName != "*" && step.nodetestName.localName != node.localName)
                                continue;
                          }

                        intermediate2[doIndex] = node;
                        ++size2;
                      }
                  else
                    {
                      intermediate2 = intermediate1;
                      size2 = size1;
                    }

                  var intermediate = intermediate2, size = size2;

                  for (var pindex = 0; pindex < step.predicates.length; ++pindex)
                    {
                      var intermediateNext = [], sizeNext = 0;
                      var intermediateIndex = [];

                      for (var doIndex in intermediate)
                        intermediateIndex.push (Number (doIndex));

                      intermediateIndex.sort (function (x, y) { return x < y ? -1 : x > y ? 1 : 0; });

                      var reversed;

                      switch (step.axis)
                        {
                        case "ancestor":
                        case "ancestor-or-self":
                        case "preceding":
                        case "preceding-sibling":
                          reversed = true;
                          break;

                        default:
                          reversed = false;
                        }

                      for (var iindex = 0, posindex = 0; iindex < intermediateIndex.length; ++iindex, ++posindex)
                        {
                          var node = intermediate[intermediateIndex[iindex]];

                          if (node)
                            {
                              var position = reversed ? size - posindex : posindex + 1;

                              var presult = step.predicates[pindex].evaluate ({ node: node, position: position, size: size });

                              if (typeof presult == "number" ? presult == position : boolean (presult))
                                {
                                  intermediateNext[node.doIndex] = node;
                                  ++sizeNext;
                                }
                            }
                        }

                      intermediate = intermediateNext;
                      size = sizeNext;
                    }

                  for (var doIndex in intermediate)
                    stepFinal[doIndex] = intermediate[doIndex];
                }
            }

          contextNodeSet = stepFinal;
        }

      return nodeSetToArray (contextNodeSet);
    }
}

function parsePath (tokens)
{
  var steps = [];

  while (tokens[0][0] == T_AXIS)
    {
      var step = { axis: tokens[0][1], toString: function () { return this.axis + "::" + this.nodetestType; } };
      tokens.shift ();

      if (tokens[0][0] == T_NODETEST)
        {
          step.nodetestType = "NodeTest";
          step.nodetestNodeType = tokens[0][1];
          step.nodetestPITarget = tokens[0][2];
          tokens.shift ();
        }
      else if (tokens[0][0] = T_NAMETEST)
        {
          step.nodetestType = "NameTest";
          step.nodetestName = new QName (tokens[0][2], tokens[0][3]);
          tokens.shift ();
        }

      step.predicates = [];

      while (tokens[0][0] == T_OPERATOR && tokens[0][1] == "[")
        {
          tokens.shift ();
          step.predicates.push (parseExpression (E_OR, tokens));
          if (tokens[0][0] != T_OPERATOR || tokens[0][1] != "]")
            throw "confused now: " + tokens.join (" => ");
          tokens.shift ();
        }

      steps.push (step);

      if (tokens[0][0] == T_OPERATOR && tokens[0][1] == "/")
        tokens.shift ();
    }

  return steps;
}

function evaluate (source, contextNode, contextNodeDescription)
{
  if (!("doIndex" in ownerDocument (contextNode)))
    indexNodes (ownerDocument (contextNode), 0);

  return parseExpression (E_OR, tokenize (source)).evaluate ({ node: contextNode, position: 1, size: 1 });
}

function compareNodeSetsOrderedIterator (refresult, xpathresult)
{
  var got, expected;
  for (var index = 0; index < refresult.length; ++index)
    {
      got = xpathresult.iterateNext ();
      if (!compareNodes (got, (expected = refresult[index])))
        return "ORDERED_NODE_ITERATOR_TYPE; got [" + describeNode (got) + "] expected [" + describeNode (expected) + "] at index " + index;
    }
  if (got = xpathresult.iterateNext ())
    return "ORDERED_NODE_ITERATOR_TYPE; got [" + describeNode (got) + "] expected [" + describeNode (null) + "] at index " + index;
  return null;
}

function compareNodeSetsUnorderedIterator (refresult, xpathresult)
{
  var count = 0, node;
  outer: while (node = xpathresult.iterateNext ())
    {
      ++count;
      if ((refresult.x = node), refresult.accept)
        continue outer;
      for (var index = 0; index < refresult.length; ++index)
        if (compareNodes (node, refresult[index]))
          continue outer;
      return "UNORDERED_NODE_ITERATOR_TYPE; got unexpected [" + describeNode (node) + "] at index " + (count - 1);
    }
  if (count < refresult.length)
    return "UNORDERED_NODE_ITERATOR_TYPE; too few nodes in result (got " + count + " and expected " + refresult.length + ")";
  return null;
}

function compareNodeSetsOrderedSnapshot (refresult, xpathresult)
{
  var got, expected, length, lengthVerdict = "";

  if (xpathresult.snapshotLength < refresult.length)
    {
      length = xpathresult.snapshotLength;
      lengthVerdict = " (too few nodes in result)";
    }
  else
    {
      length = refresult.length;
      if (xpathresult.snapshotLength > refresult.length)
        lengthVerdict = " (too many nodes in result)";
    }

  var expected = " [" +  + "]";

  for (var index = 0; index < length; ++index)
    if (!compareNodes ((got = xpathresult.snapshotItem (index)), (expected = refresult[index])))
      return "ORDERED_NODE_SNAPSHOT_TYPE; got [" + describeNode (got) + "] expected [" + describeNode (expected) + "] at index " + index + lengthVerdict;
  if (xpathresult.snapshotLength < refresult.length)
    return "ORDERED_NODE_SNAPSHOT_TYPE; too few nodes in result, got [" + describeNodes (xpathresult).join (" ") + "] expected [" + describeNodes (refresult).join (" ") + "]";
  else if (xpathresult.snapshotLength > refresult.length)
    return "ORDERED_NODE_SNAPSHOT_TYPE; too many nodes in result, got [" + describeNodes (xpathresult).join (" ") + "] expected [" + describeNodes (refresult).join (" ") + "]";
  return null;
}

function compareNodeSetsUnorderedSnapshot (refresult, xpathresult, contextNode)
{
  if (xpathresult.snapshotLength < refresult.length)
    return "UNORDERED_NODE_SNAPSHOT_TYPE; too few nodes in result";
  outer: for (var sindex = 0; sindex < xpathresult.snapshotLength; ++sindex)
    {
      var node = xpathresult.snapshotItem (sindex);
      if ((refresult.x = node), refresult.accept)
        continue outer;
      for (var rindex = 0; rindex < refresult.length; ++rindex)
        {
          var rnode = refresult[rindex];
          if (compareNodes (node, rnode))
            continue outer;
        }
      return "UNORDERED_NODE_SNAPSHOT_TYPE; got unexpected [" + describeNode (node) + "] expected [" + describeNodes (refresult) + "]";
    }
  return null;
}

function compareNodeSetsOrderedSingle (refresult, xpathresult)
{
  if (compareNodes (xpathresult.singleNodeValue, refresult[0]))
    return null;
  return "FIRST_ORDERED_NODE_TYPE; got [" + describeNode (xpathresult.singleNodeValue) + "] expected [" + describeNode (refresult[0]) + "]";
}

function compareNodeSetsUnorderedSingle (refresult, xpathresult)
{
  if (!xpathresult.singleNodeValue && refresult.length == 0)
    return null;
  if (xpathresult.singleNodeValue && ((refresult.x = xpathresult.singleNodeValue), refresult.accept))
    return null;
  for (var index = 0; index < refresult.length; ++index)
    if (compareNodes (xpathresult.singleNodeValue, refresult[index]))
      return null;
  return "ANY_UNORDERED_NODE_TYPE; got unexpected [" + describeNode (xpathresult.singleNodeValue) + "]";
}

function testExpression (refresult, xpathexpression, contextNode, type, nodesettype, cnindex)
{
  var doc = ownerDocument (contextNode);

  if (!("doIndex" in ownerDocument (contextNode)))
    indexNodes (ownerDocument (contextNode), 0);

  if (typeof refresult == "number")
    {
      var xpathresult = xpathexpression.evaluate (contextNode, XPathResult.NUMBER_TYPE, null);
      return xpathresult.numberValue == refresult ? null : "got " + xpathresult.numberValue + " expected " + refresult;
    }
  else if (typeof refresult == "boolean")
    {
      var xpathresult = xpathexpression.evaluate (contextNode, XPathResult.BOOLEAN_TYPE, null);
      return xpathresult.booleanValue == refresult ? null : "got " + xpathresult.booleanValue + " expected " + refresult;
    }
  else if (typeof refresult == "string")
    {
      var xpathresult = xpathexpression.evaluate (contextNode, XPathResult.STRING_TYPE, null);
      return xpathresult.stringValue == refresult ? null : "got \"" + xpathresult.stringValue + "\" expected \"" + refresult + "\"";
    }
  else
    {
      switch (nodesettype)
        {
        case "ordered-iterator":
          if (result = compareNodeSetsOrderedIterator (refresult, xpathexpression.evaluate (contextNode, XPathResult.ORDERED_NODE_ITERATOR_TYPE, null, cnindex)))
            return result;
          break;

        case "unordered-iterator":
          if (result = compareNodeSetsUnorderedIterator (refresult, xpathexpression.evaluate (contextNode, XPathResult.UNORDERED_NODE_ITERATOR_TYPE, null, cnindex)))
            return result;
          break;

        case "ordered-snapshot":
          if (result = compareNodeSetsOrderedSnapshot (refresult, xpathexpression.evaluate (contextNode, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null, cnindex)))
            return result;
          break;

        case "unordered-snapshot":
          if (result = compareNodeSetsUnorderedSnapshot (refresult, xpathexpression.evaluate (contextNode, XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, null, cnindex), contextNode))
            return result;
          break;

        case "ordered-single":
          if (result = compareNodeSetsOrderedSingle (refresult, xpathexpression.evaluate (contextNode, XPathResult.FIRST_ORDERED_NODE_TYPE, null, cnindex)))
            return result;
          break;

        case "unordered-single":
          if (result = compareNodeSetsUnorderedSingle (refresult, xpathexpression.evaluate (contextNode, XPathResult.ANY_UNORDERED_NODE_TYPE, null, cnindex)))
            return result;
          break;
        }

      return null;
    }
}

function testExpressionCatch ()
{
  try
    {
      return testExpression.apply (this, arguments);
    }
  catch (e)
    {
      return "Exception: " + (e instanceof String) ? e.toString () : Object.prototype.toString.call (e) + (e.message ? " (" + /(.*)(\s*Backtrace:.*)?/m.exec (e.message)[1] + ")" : "");
    }
}

function describeNode (node)
{
  function inner (node)
    {
      function quote (string)
        {
          return string.replace (/\n/g, "\\n").replace (/\r/g, "\\r").replace (/\t/g, "\\t");
        }

      if (node.nodeType == Node.TEXT_NODE || node.nodeType == Node.CDATA_SECTION_NODE)
        {
          var sibling = node.previousSibling, position = 1;
          while (sibling)
            {
              while (sibling && (sibling.nodeType == Node.TEXT_NODE || sibling.nodeType == Node.CDATA_SECTION_NODE))
                sibling = sibling.previousSibling;
              while (sibling && sibling.nodeType != Node.TEXT_NODE && sibling.nodeType != Node.CDATA_SECTION_NODE)
                sibling = sibling.previousSibling;
              if (sibling && (sibling.nodeType == Node.TEXT_NODE || sibling.nodeType == Node.CDATA_SECTION_NODE))
                ++position;
            }
          return "text(\"" + quote (stringValue (node)) + "\")[" + position + "]";
        }
      else if (node.nodeType == Node.COMMENT_NODE)
        return "comment(\"" + quote (node.nodeValue) + "\")";
      else if (node.nodeType == Node.PROCESSING_INSTRUCTION_NODE)
        return "processing-instruction(" + node.target + ", \"" + quote (node.data) + "\")";
      else if (node.nodeType == Node.ELEMENT_NODE)
        {
          var sibling = node.previousSibling, position = 1;
          while (sibling)
            {
              if (sibling.prefix == node.prefix && sibling.localName == node.localName)
                ++position;
              sibling = sibling.previousSibling;
            }
          return (node.prefix ? node.prefix + ":" : "") + node.localName + "[" + position + "]";
        }
      else if (node.nodeType == Node.ATTRIBUTE_NODE)
        return "@" + node.localName;
      else if (node.nodeType == XPathNamespace.XPATH_NAMESPACE_NODE)
        return "namespace::" + (node.localName ? node.localName : node.namespaceURI ? "<default>" : "<undefault>");
      else
        return "";
    }

  var description = "";

  if (!node)
    return "<no node>";

  if (node.nodeType == Node.DOCUMENT_NODE || node.nodeType == Node.DOCUMENT_FRAGMENT_NODE)
    return "/";

  while (true)
    {
      description = inner (node) + description;
      if (node.parentNode || node.ownerElement)
        {
          description = "/" + description;
          node = node.parentNode ? node.parentNode : node.ownerElement;
        }
      else
        break;
    }

  return description;
}

function describeNodes (nodes)
{
  var descriptions = [];
  if ("snapshotLength" in nodes)
    for (var index = 0; index < nodes.snapshotLength; ++index)
      descriptions.push (describeNode (nodes.snapshotItem (index)));
  else
    for (var index = 0; index < nodes.length; ++index)
      descriptions.push (describeNode (nodes[index]));
  return descriptions;
}

function doAutomaticTest (doc, reportStart, reportProgress, reportError)
{
  if (doc instanceof Event)
    doc = document;

  var child = doc.firstChild;
  var context = null, tests = [], namespaces = {};

  if (!doc.evaluate0)
    doc.evaluate0 = evaluate;

  do
    {
      var nextSibling = child.nextSibling;
      if (child.nodeType == Node.TEXT_NODE)
        continue;
      else if (child.nodeType == Node.COMMENT_NODE)
        {
          var match;
          if (match = /^\s*context:\s+(.*?)\s*$/.exec (child.nodeValue))
            context = match[1];
          else if (match = /^\s*(number|boolean|string|nodeset):\s+(.*?)\s*$/.exec (child.nodeValue))
            tests.push ([match[2], match[1]]);
          else
            continue;
          child.parentNode.removeChild (child);
        }
      else
        break;
    }
  while (child = nextSibling);

  if (tests.length != 0)
    {
      var result = [];
      var nsresolver = { lookupNamespaceURI: function (prefix) { return prefix; } };
      var successful = 0;
      var cache = {};
      var contextNodes;

      if (context == null)
        contextNodes = [doc];
      else
        contextNodes = doc.evaluate0 (context, doc, "/");

      if (typeof contextNodes != "object" || contextNodes.length == 0)
        alert ("No context nodes!");
      else
        {
          var before = (new Date).getTime (), failure, progressTotal = 0;
          for (var tindex = 0; tindex < tests.length; ++tindex)
            if (tests[tindex][1] == "nodeset")
              progressTotal += contextNodes.length * 6;
            else
              progressTotal += contextNodes.length;
          if (reportStart)
            reportStart (contextNodes.length, tests.length, progressTotal);
          for (var nindex = 0; nindex < contextNodes.length; ++nindex)
            for (var tindex = 0; tindex < tests.length; ++tindex)
              {
                var contextNode = contextNodes[nindex];
                var realContextNode;

                if (contextNode.nodeType == XPathNamespace.XPATH_NAMESPACE_NODE && contextNode.fakeNamespaceNode)
                  {
                    realContextNode = doc.evaluate ("namespace::node()[local-name()='" + contextNode.localName + "']", contextNode.ownerElement, null, XPathResult.ANY_UNORDERED_NODE_TYPE, null).singleNodeValue;
                    if (!realContextNode || !compareNodes (contextNode, realContextNode))
                      return "failed to translate fake namespace node to native namespace node!";

                    if (ownerDocument (realContextNode) != ownerDocument (contextNode))
                      opera.postError ("Ouch!");
                    if (ownerDocument (realContextNode) != ownerDocument (contextNode.ownerElement))
                      opera.postError ("What?");
                  }
                else
                  realContextNode = contextNode;

                var doc = ownerDocument (contextNodes[nindex]);
                var testtype = tests[tindex][1], nodesettype;
                var xpathexpression = doc.createExpression (tests[tindex][0], nsresolver);

                if (testtype == "nodeset")
                  {
                    var refresult = doc.evaluate0 (tests[tindex][0], realContextNode, describeNode (realContextNode));

                    for (nodesettype in { "ordered-iterator":0, "unordered-iterator":0, "ordered-snapshot":0, "unordered-snapshot":0, "ordered-single":0, "unordered-single":0 })
                      {
                        if ((failure = testExpressionCatch (refresult, xpathexpression, realContextNode, testtype, nodesettype, nindex)) != null)
                          if (reportError)
                            reportError ("FAIL: " + tests[tindex][0] + " with context node " + describeNode (contextNodes[nindex]) + " and type nodeset/" + nodesettype + ": " + failure);
                          else
                            result.push ("FAIL: " + tests[tindex][0] + " with context node " + describeNode (contextNodes[nindex]) + " and type nodeset/" + nodesettype + ": " + failure);
                        else
                          ++successful;

                        if (reportProgress)
                          reportProgress ();
                      }
                  }
                else
                  {
                    var refresult = doc.evaluate0 (type + "(" + tests[tindex][0] + ")", realContextNode, describeNode (realContextNode));

                    if ((failure = testExpressionCatch (refresult, xpathexpression, realContextNode, testtype, "", nindex)) != null)
                      if (reportError)
                        reportError ("FAIL: " + tests[tindex][0] + " with context node " + describeNode (contextNodes[nindex]) + " and type " + testtype + ": " + failure);
                      else
                        result.push ("FAIL: " + tests[tindex][0] + " with context node " + describeNode (contextNodes[nindex]) + " and type " + testtype + ": " + failure);
                    else
                      ++successful;

                    if (reportProgress)
                      reportProgress ();
                  }
              }
          var elapsed = (new Date).getTime () - before;
        }

      if (!parent.XPathJSFramework)
        {
          while (doc.firstChild)
            doc.removeChild (doc.firstChild);

          var html = doc.appendChild (doc.createElementNS ("http://www.w3.org/1999/xhtml", "html"));
          var body = html.appendChild (doc.createElementNS ("http://www.w3.org/1999/xhtml", "body"));

          if (result.length != 0)
            {
              for (var index = 0; index < result.length; ++index)
                {
                  var p1 = body.appendChild (doc.createElementNS ("http://www.w3.org/1999/xhtml", "p"));
                  p1.setAttributeNS (null, "class", "white-space: pre");
                  p1.appendChild (doc.createTextNode (result[index]));
                }
              var p2 = body.appendChild (doc.createElementNS ("http://www.w3.org/1999/xhtml", "p"));
              p2.appendChild (doc.createTextNode ("Time: " + String (elapsed) + " ms"));
            }
          else
            {
              var p1 = body.appendChild (doc.createElementNS ("http://www.w3.org/1999/xhtml", "p"));
              var b = p1.appendChild (doc.createElementNS ("http://www.w3.org/1999/xhtml", "b"));
              b.appendChild (doc.createTextNode ("PASS"));

              var p2 = body.appendChild (doc.createElementNS ("http://www.w3.org/1999/xhtml", "p"));
              p2.appendChild (doc.createTextNode (successful + " successful test" + (successful > 1 ? "s" : "") + " run with " + (contextNodes.length > 1 ? contextNodes.length + " different context nodes" : " a single context node")));
              var p3 = body.appendChild (doc.createElementNS ("http://www.w3.org/1999/xhtml", "p"));
              p3.appendChild (doc.createTextNode ("Time: " + String (elapsed) + " ms"));
            }
        }
    }
}

var is_userjs = false;

try
  {
    defineMagicVariable ("is_userjs", function () { return true; });
  }
catch (e)
  {
  }

if (is_userjs)
  document.addEventListener ("load", doAutomaticTest, false);

function startAutomaticTest (doc, reportStart, reportProgress, reportError)
  {
    doc.defaultView.setTimeout (function () { doAutomaticTest (doc, reportStart, reportProgress, reportError); }, 0);
  }

return { evaluate: evaluate, testExpression: testExpression, startAutomaticTest: startAutomaticTest };

}) ();
