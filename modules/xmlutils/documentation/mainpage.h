/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/**

 @mainpage XML Utils module

  This is the auto-generated API documentation for the XML Utils
  module.  For more information about the module, see the module's

   <a href="http://wiki/developerwiki/index.php/Modules/xmlutils">Wiki page</a>.

 @section intro Introduction

  The XML Utils module contains various XML related code.  API:s for
  completing various XML related tasks.  The API:s themselves contain
  most of the documentation; this page is mostly a brief summary of
  the tasks and the API:s that are useful for completing them.

 @subsection parsing Parsing

  For parsing, there is first and foremost the XMLParser class.  It is
  actually mainly a wrapper around an actual XML parser, such as the
  one in the
   <a href="../../../xmlparser/documentation/index.html">xmlparser module<a/>,
  or whatever XML parser Opera might be using in the future.

  For processing the parsed data, client code either implements the
  XMLTokenHandler interface or the XMLLanguageParser interface.  The
  XMLTokenHandler interface is a little lower level and is intended
  for uses such as the main document parsing, where exactly all
  information from the parsed XML document needs to be accessible.
  The XMLLanguageParser interface is a little higher level, providing
  some additional convenience functionality, and is intended for
  parsing XML based languages, such as XSLT or XML Schema.

  See also "Fragment," for a primarily convenient, if not so
  efficient, way of parsing smaller XML resources.

 @subsection serializing Serializing

  Serializing is the act of translating a final parse result, such as
  an HTML_Element tree or an XMLFragment object, into data in a less
  final form, such as a string of XML source code, or input to a
  XMLLanguageParser object.

  To serialize you use the XMLSerializer interface.  To create an
  object for serializing to a string, use
  XMLSerializer::MakeToStringSerializer.  To create an object for
  serializing into an XMLLanguageParser, use
  XMLLanguageParser::MakeSerializer.

 @subsection fragment Fragment

  An "XML fragment" is a block of XML that is slightly less strictly
  constrained than an XML document.  There doesn't have to be a single
  root element.  There can be several, or none.  Character data can be
  anywhere, inside or outside of elements.  Of course, an XML fragment
  can be a well-formed XML document.

  The class XMLFragment represents an XML fragment.  The XML fragment
  can be parsed from a string, a ByteBuffer or a file, or can be built
  up using function calls.  Or both.  And it can be serialized into a
  string, or a ByteBuffer, or inspected through function calls.  Or
  both.

  Note that since parsing an XML resource into an XMLFragment object
  means reading all the information and storing it in memory in a
  secondary data structure, it is not the most efficient way of
  parsing XML, if the target is a third data structure.  Other ways of
  parsing (see above) supports incremental parsing that typically is
  more memory efficient.

 @subsection names Names (expanded, qualified, et.c.)

  Dealing with names in XML can be an ordeal.  They are often
  specified in the form "name" or "prefix:localname", but are usually
  supposed to be interpreted as
   <code>{ namespace URI, localpart }</code>,
  where "namespace URI" is calculated from "prefix" (or the absence of
  a prefix,) using a set of namespace declarations in scope.

  The XML Utils module has a range of classes, XMLExpandedName,
  XMLCompleteName, XMLExpandedNameN and XMLCompleteNameN, that can
  help in some situations.  See
   <a href="names.html">this page</a>
  for more information.  There is also a number of functions in the
  XMLUtils class for determining whether a string represents a valid
  Name, NmToken, QName or NCName, that may come in handy.

 @subsection namespaces Namespaces

  Keeping track of XML namespace declarations while parsing and
  finding XML namespaces in scope at an HTML_Element.  Uhm.  Right.
  The class is named XMLNamespaceDeclaration.  See
   <a href="namespaces.html">this page</a>
  for more information.

 @subsection cclass Character Classification

  The class XMLUtils provides some functions for classifying
  characters in some ways that are meaningful in the context of XML,
  such as "is this a space character?" and "is this character valid in
  a Name in this version of XML?"  For instance, the XML parser in the
   <a href="../../../xmlparser/documentation/index.html">xmlparser module<a/>
  uses those functions for most character classification.

  It also contains functions for retrieving the next 32-bit character
  from a string, handling surrogate pairs, and retrieving the next
  line from a string, according to the XML line breaking rules in a
  certain version of XML (they changed from 1.0 to 1.1.)

*/
