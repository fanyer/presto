/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef XMLLANGUAGEPARSER_H
#define XMLLANGUAGEPARSER_H

#ifdef XMLUTILS_XMLLANGUAGEPARSER_SUPPORT

#include "modules/xmlutils/xmltypes.h"

class XMLLanguageParserState;
class XMLCompleteNameN;
class XMLNamespaceDeclaration;
class XMLTokenHandler;
class XMLSerializer;
class XMLDocumentInformation;
class URL;

/**

  Interface for parsers of XML-based languages (such as XSLT or XML
  Schema.)

  The interface can optionally keep track of current base URI:s (from
  xml:base attributes or other sources,) current language (from
  xml:lang attributes,) current namespace declarations in scope and
  current whitespace handling (from xml:space attributes.)  To enable
  this, a subclass needs to call the functions HandleStartEntity,
  HandleStartElement, HandleAttribute, HandleEndElement and
  HandleEndEntity.  It is important that every call to
  HandleStartEntity is matched by a call to HandleEndEntity, and that
  every call to HandleStartElement is matched by a call to
  HandleEndElement, but other than that, the subclass can decide when
  and how to call these functions.

  A subclass can signal that parsing should be paused or stopped after
  any element, by setting the 'block' and 'finished' arguments to the
  function EndElement to TRUE.  If it pauses the parsing, the source
  (the caller) will set a callback using the SetSourceCallback that
  the subclass needs to call when parsing should continue.  Note that
  if the subclass signals that the parsing should be stopped, it only
  means that it will not receive any more regular callbacks, parsing
  will still continue until the end of the parsed document to check
  for parse errors.

  If an XML parse error is encountered by the source, the function
  XMLError is called.  After that, no other calls will be made.

  <h3>Using with an XMLParser</h3>

  To have an XMLLanguageParser based parser parse XML source code, one
  simply uses XMLLanguageParser::MakeTokenHandler to create a token
  handler and creates and uses an XMLParser as in any other case.  See
  the XMLParser description for more information.

  The 'fragment_id' argument to XMLLanguageParser::MakeTokenHandler
  can be used if only a parse of the parsed XML document is of
  interest.  The only effect of specifying a non-NULL 'fragment_id' is
  that the argument 'fragment_start' to
  XMLLanguageParser::StartElement is set to TRUE when an element with
  a matching ID attribute is found.  XMLLanguageParser::StartElement
  will still be called for all other elements in the document, unless
  the parsing is aborted.  The primary reason why content outside of
  the selected element is still reported to the XMLLanguageParser
  object is that it may affect the interpretation of the selected
  fragment, mainly through XML namespace declarations and attributes
  such as "xml:space" and "xml:lang".

  <h3>Using with an XMLSerializer</h3>

  An XMLLanguageParser based parser can also parse XML that has
  already been parsed into an HTML_Element tree or an XMLFragment.  To
  do that, use XMLLanguageParser::MakeSerializer to create an
  serializer, and serialize the source using it.  The result will be
  the same as if the source had been serialized into a string and then
  parsed with an XMLParser as described in the previous section.

  When parsing from an HTML_Element tree, a mechanism similar to the
  'fragment_id' argument to XMLLanguageParser::MakeTokenHandler is
  available in the 'target' argument to XMLSerializer::Serialize.

*/
class XMLLanguageParser
{
public:
	virtual OP_STATUS StartEntity(const URL &url, const XMLDocumentInformation &documentinfo, BOOL entity_reference);
	/**< Starts an XML entity.  The default implementation does
	     nothing. */

	virtual OP_STATUS StartElement(const XMLCompleteNameN &completename, BOOL fragment_start, BOOL &ignore_element);
	/**< Starts an element.  The default implementation does
	     nothing. */

	virtual OP_STATUS AddAttribute(const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length, BOOL specified, BOOL id);
	/**< Adds an attribute to the current element.  The default
	     implementation does nothing. */

	virtual OP_STATUS StartContent();
	/**< Signals the start of an element's content.  Called even if
	     the element is empty, as a signal that all attributes have
	     been added.  The default implementation does nothing. */

	/** Character data node type. */
	enum CharacterDataType
	{
		CHARACTERDATA_TEXT,
		CHARACTERDATA_TEXT_WHITESPACE,
		CHARACTERDATA_CDATA_SECTION
	};

	virtual OP_STATUS AddCharacterData(CharacterDataType type, const uni_char *value, unsigned value_length);
	/**< Adds a character data node to the current element.  The
	     default implementation does nothing. */

	virtual OP_STATUS AddProcessingInstruction(const uni_char *target, unsigned target_length, const uni_char *data, unsigned data_length);
	/**< Adds a processing instruction node to the current element.
	     The default implementation does nothing. */

	virtual OP_STATUS AddComment(const uni_char *value, unsigned value_length);
	/**< Adds a comment node to the current element.  The default
	     implementation does nothing. */

	virtual OP_STATUS EndElement(BOOL &block, BOOL &finished);
	/**< Ends the current element.  The default implementation does
	     nothing. */

	class SourceCallback
	{
	public:
		enum Status
		{
			STATUS_CONTINUE,
			/**< The source should continue. */

			STATUS_ABORT_FAILED,
			/**< The source should abort due to a failure. */

			STATUS_ABORT_OOM
			/**< The source should abort due to an OOM condition. */
		};

		virtual void Continue(Status status) = 0;
		/**< Signal the source to continue or abort. */

	protected:
		virtual ~SourceCallback() {}
		/**< XMLLanguageParser implementations shouldn't delete the
		     source callback. */
	};

	virtual void SetSourceCallback(SourceCallback *source_callback);
	/**< When EndElement() requests the parsing to be blocked, the
	     calling source sets a callback using this function that
	     should be called when the parsing needs to continue (or be
	     aborted.)  The default implementation just sets the
	     'source_callback' member variable. */

	virtual OP_STATUS EndEntity();
	/**< Ends the current element.  The default implementation does
	     nothing. */

	virtual OP_STATUS XMLError();
	/**< Signals that an XML parse error was encountered.  After this
	     function is called, no other function calls will be made. */

#ifdef XML_ERRORS
	virtual void SetLocation(const XMLRange &location);
	/**< Specifies the location in the current entity of the tag,
	     attribute or content that caused the next call to
	     StartElement(), AddAttribute(), AddCharacterData(),
	     AddProcessingInstruction() or EndElement().  Will be set to a
	     range with invalid end-points (see XMLPoint::IsValid()) if no
	     location information is available.  In this case, it might
	     not be called once per other function, so the implementation
	     might need to remember the last location specified.

	     The default implementation sets the 'location' member
	     variable. */
#endif // XML_ERRORS

	static OP_STATUS MakeTokenHandler(XMLTokenHandler *&tokenhandler, XMLLanguageParser *parser, const uni_char *fragment_id);
	/**< Creates an XMLTokenHandler object that can be used to parse
	     an XML document using an XMLParser object, feeding the result
	     into the given XMLLanguageParser object. */

#ifdef XMLUTILS_XMLTOLANGUAGEPARSERSERIALIZER_SUPPORT
	static OP_STATUS MakeSerializer(XMLSerializer *&serializer, XMLLanguageParser *parser);
	/**< Creates an XMLSerializer object that can be used to
	     "serialize" an existing tree of elements into the given
	     XMLLanguageParser object. */
#endif // XMLUTILS_XMLTOLANGUAGEPARSERSERIALIZER_SUPPORT

protected:
	friend class XMLLanguageParserState;

	XMLLanguageParserState *state;
	SourceCallback *source_callback;

#ifdef XML_ERRORS
	XMLRange location;
#endif // XML_ERRORS

	OP_STATUS HandleStartEntity(const URL &url, const XMLDocumentInformation &documentinfo, BOOL entity_reference);
	OP_STATUS HandleStartElement();
	OP_STATUS HandleAttribute(const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length);
	OP_STATUS HandleEndElement();
	OP_STATUS HandleEndEntity();

	XMLVersion GetCurrentVersion();
	XMLNamespaceDeclaration *GetCurrentNamespaceDeclaration();
	XMLWhitespaceHandling GetCurrentWhitespaceHandling(XMLWhitespaceHandling use_default = XMLWHITESPACEHANDLING_DEFAULT);
	const uni_char *GetCurrentLanguage();
	URL GetCurrentBaseUrl();

	XMLLanguageParser();
	virtual ~XMLLanguageParser();
};

#endif // XMLUTILS_XMLLANGUAGEPARSER_SUPPORT
#endif // XMLLANGUAGEPARSER_H
