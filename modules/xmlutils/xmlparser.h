/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef XMLPARSER_H
#define XMLPARSER_H

#include "modules/xmlutils/xmltypes.h"
#include "modules/xmlutils/xmlvalidator.h"
#include "modules/xmlutils/xmlnames.h"

#ifdef OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE

/** XML parser data format requirements.

    XMLUTILS_PARSE_RAW_DATA

      If defined, the parser needs raw, undecoded data.  Use the
      function XMLParser::Parse(const char *, unsigned, BOOL).

    XMLUTILS_PARSE_UTF8_DATA

      If defined, the parser can parse utf-8 encoded data.  Use the
      function XMLParser::Parse(const char *, unsigned, BOOL).

    XMLUTILS_PARSE_UTF16_DATA

      If defined, the parser can parse utf-16 encoded data.  Use the
      function XMLParser::Parse(const uni_char *, unsigned, BOOL).

    XMLUTILS_PARSE_NOT_INCREMENTAL

      If defined, the parser must be given all data in one call to
      XMLParser::Parse.  That is, the "more" argument must be FALSE.

    If XMLUTILS_PARSE_RAW_DATA is defined, neither of the other will
    be.

    If both XMLUTILS_PARSE_UTF8_DATA and XMLUTILS_PARSE_UTF16_DATA are
    defined, and utf-8 encoded data is available, it is more efficient
    to use it instead of converting it to utf-16 before giving it to
    the parser, since the parser will just have to convert it back
    again. */

# define XMLUTILS_PARSE_UNICODE_DATA

class XMLDocumentInformation;
class XMLErrorReport;
class FramesDocument;
class MessageHandler;
class XMLToken;
class XMLTokenHandler;
class XMLDoctype;
class URL;

/**

  An XML parser wrapper class.  Parses data from a URL or string data
  and calls an XMLTokenHandler with the resulting tokens.  Can be used
  either with a document, in which case the base URL and/or any
  external entities loaded will be loaded as inlines in that document,
  or standalone.

  <h3>Basic usage</h3>

  <b>Step 1: creating the parser</b>

  You create an XMLParser using one of the XMLParser::Make variants.
  One is for a parser connected to a FramesDocument, and one is for a
  parser not connected to a FramesDocument.  The difference is mainly
  in how the parser loads URLs, if it loads any URLs.

  Before you create the parser you need an XMLTokenHandler, and
  possibly an XMLParser::Listener object (see "To listen or not to
  listen" below for more information on that subject.)  The
  XMLTokenHandler is either something you implement yourself or
  something someone else provides for you (such as
  XMLLanguageParser::MakeTokenHandler.)

  Example:

  <pre>
    XMLParser::Listener *listener = new MyParserListener();
    XMLTokenHandler *tokenhandler = new MyTokenHandler();

    XMLParser *parser;
    RETURN_IF_ERROR(XMLParser::Make(parser, listener, document_or_messagehandler, tokenhandler, url));
  </pre>

  <b>Step 2: configuring the parser</b>

  After you have created the parser, you can customize its behaviour.
  Perhaps you want it to parse an XML fragment (roughly something that
  can be inside an element) instead of a whole XML document?  Perhaps
  you want it to load external entities?  Perhaps you want it to split
  character data nodes into conveniently sized pieces?

  Example that does all of the above:

  <pre>
    XMLParser::Configuration configuration;
    configuration.parse_mode = XMLParser::PARSEMODE_FRAGMENT;
    configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_YES;
    configuration.preferred_literal_part_length = convenient_size;
    parser->SetConfiguration(configuration);
  </pre>

  If the defaults listed in the description of
  XMLParser::Configuration::Configuration works for you, you don't
  need to set a configuration.

  <b>Step 3a: loading and parsing a URL</b>

  Now you are ready to start parsing.  One way is to ask the parser to
  load the URL you supplied in the call to XMLParser::Make on its own
  and parse the data.  This mode is sometimes (in the implementation,
  mainly) called "standalone."  To do this you need to have supplied
  an active Document or a MessageHandler in the call to
  XMLParser::Make, and of course a valid URL.  You probably also want
  to have a listener, since otherwise you won't know if the progress
  stopped because of some error.

  Example:

  <pre>
    RETURN_IF_ERROR(parser->Load(referrer_url));
  </pre>

  <b>Step 3b: parsing text</b>

  Instead of having the parser load a URL, you can load it yourself,
  or use data from some other source.  In this case, you feed the
  parser with the text to parse directly.  This is of course a bit
  more flexible than having the parser load a URL for you, but is also
  more inconvenient if the parsing is asynchronous because of external
  entity loading or a blocking XMLTokenHandler.  In this mode, the
  parser won't parse unless you tell it to by calling the
  XMLParser::Parse function, even if you have already called it with
  all the source text to parse.  The parser will use the
  XMLParser::Listener::Continue function to tell you to continue, if
  it needs to.  If it needs to, and you didn't supply a listener, the
  parsing will stall indefinitely.

  Simple example that does not handle asynchronous situations:

  <pre>
    for (unsigned index = 0;
         !parser->IsFinished() && !parser->IsFailed() && index < count;
         ++index)
    {
      BOOL more = index < count - 1;
      RETURN_IF_ERROR(parser->Parse(blocks[index].text, blocks[index].length, more));
    }

    if (!parser->IsFailed())
      return OpStatus::ERR;
  </pre>

  Normally, XMLParser::Parse consumes all data you give it.  This
  means it either parses it and calls the XMLTokenHandler with the
  resulting tokens, or copies the data into an internal buffer.  You
  can however supply an extra argument to XMLParser::Parse,
  'consumed', that causes the parser to only consume as much as it
  managed to parse, and tell you, via the 'consumed' argument, how
  much it did consume.  This may lead to less allocation and copying
  of memory, and might be a good idea.  On the other hand, unless your
  XMLTokenHandler blocks, or you are loading external entities, the
  parser will typically always parse all the data anyway, so you might
  not care to bother.

  <b>Step 4: stopping the parser</b>

  When you are finished, or when you want to stop the parser before it
  has finished, you delete it.  You can always delete it whenever you
  want to (except if you are being called from the parser, of course.)
  When deleted, the parser aborts everything it was doing, frees all
  its allocated resources, and goes away.  It will not call
  XMLParser::Listener::Stopped when you delete it, but it will call
  XMLTokenHandler::ParsingStopped() on its token handler, if it hasn't
  been called before.  By default, the parser does not delete the
  XMLParser::Listener or the XMLTokenHandler objects, but it can be
  configured to do so using the functions SetOwnsListener() and
  SetOwnsTokenHandler().

  <h3>To listen or not to listen</h3>

  The XMLParser::Listener class is used by the client code to listen
  to the certain events from the XMLParser.  Those are currently

    <ul>
      <li>
        The parser is ready to continue; the client code should call
        XMLParser::Parse again when appriopriate (for the client code).
      </li>
      <li>
        The parser has stopped (finished or failed).
      </li>
      <li>
        The URL loaded by the parser was redirected to another URL.
      </li>
    </ul>

  It may be okay not to use a listener.  Essentially, the first event
  is only ever sent if XMLParser::Parse was used, and an external
  entity was loaded, or the token handler told the parser to block.
  If the client code used XMLParser::Load, or disabled external entity
  loading and used a token handler that never blocks, it will never be
  sent.  The second event will always be sent, but the client code
  might not need to know.  If the client code used XMLParser::Parse,
  it can always check itself if the parser has stopped, after each
  call to XMLParser::Parse, instead of using a listener.  If the
  client code used XMLParser::Load, the stopped event is the only way
  for the client code to know that the parsing failed, if it did.

  It is always safe (as in "won't crash") to use a parser with a NULL
  listener.

  <h3>Error report generation</h3>

  When an error occurs, the parser will specify where the error
  occured in the source code, as an XMLRange (see
  XMLParser::GetErrorPosition).  It can also generate more complete
  error information, in the form of an XMLErrorReport.  Such a report
  also contains the text on the line where the error occured, and
  optionally the text on the lines surrounding the error.  It can even
  contain and highlight some other text that might be of interest,
  such as the relevant start tag when an incorrect end tag is found.

  When an error report is generated, the parser needs to process the
  source code again.  If the XMLParser::Load was used to start the
  parser, the parser just reads data from the URL again internally.
  If XMLParser::Parse was used, the caller needs to provide the data
  again, through an ErrorDataProvider object.

  Such an object must be set before the last call to XMLParser::Parse
  (the call that fails, that is) using the function
  XMLParser::SetErrorDataProvider.  It can of course, and should, be
  set before the first call to XMLParser::Parse.  If it has not been
  set by the time the error report is supposed to be generated, the
  error report will be empty.

  The ErrorDataProvider is supposed to provide as much data as it can,
  right then, if it can provide any at all.  If it can't provide
  enough data for the error report generation, the error report will
  lack context lines, or lack more.  But no big deal.

  <h3>External entities</h3>

  The XML parser may load and parse external entities as it parses the
  data provided by the caller.  Whether it does so depends on the
  parser's configuration
  (XMLParser::Configuration::load_external_entities), preferences
  ([User Prefs]/XML Load External Entities) and the feature
  FEATURE_XMLENTITIES_INI which enables an extra configuration file
  (OPFILE_INI_FOLDER/xmlentities.ini).

  External entities from other domains than the document's will not be
  loaded, unless the exact URL (the entity's system ID) is mentioned
  in the xmlentities.ini configuration file.

  The xmlentities.ini file can also specify a local file that
  represents a certain public ID or system ID (or both) and that can
  be used instead of loading an entity from the network.  An entry in
  the ini file can specify that the local file should always be used,
  even if loading of external entities is enabled.  If it doesn't, the
  local file is only used when loading of external entities is
  disabled.

  Loading of external entities can change the parser's behaviour
  rather radically.  For instance, even if the entire document entity
  is provided in a single call to XMLParser::Parse, the parser might
  become asynchronous, forcing the client to use a listener and
  continue parsing later by calling XMLParser::Parse again.  The
  client code needs to support that, or simply disable external entity
  loading by setting XMLParser::Configuration::load_external_entities
  to XMLParser::LOADEXTERNALENTITIES_NO.  The parser may still local
  files specified by the xmlentities.ini configuration file, but the
  parser will behave completely synchronous, as long as the token
  handler doesn't tell it to block.

*/
class XMLParser
#ifdef XML_VALIDATING
	: public XMLValidator
#endif // XML_VALIDATING
{
public:
	/** Parser listener.  See the section 'To listen or not to listen'
	    in the description of XMLParser for more information. */
	class Listener
	{
	public:
		virtual ~Listener() {}
		/**< The XMLParser doesn't delete its listener unless
		     configured to do so using the function
		     XMLParser::SetOwnsListener. */

		virtual void Continue(XMLParser *parser) = 0;
		/**< Signals that XMLParser::Parse should be called again.
		     Note: XMLParser::Parse can not be called directly.  This
		     function will only be called if XMLParser::Parse was used
		     to start the parsing.  If XMLParser::Load was used, the
		     parser will continue on its own.  If no more data will be
		     available, XMLParser::Parse should be called with no data
		     and a FALSE 'more' argument.  In special cases, several
		     such calls to XMLParser::Parse can be necessary.

		     This happens only when external entities are loaded, when
		     parsing of an external entity has finished and parsing of
		     the document entity should continue.

		     Calling XMLParser::Parse because more of the document
		     entity is available is always okay (including when the
		     parser is currently parsing an external entity and will not
		     use the new data immediately.) */

		virtual void Stopped(XMLParser *parser) = 0;
		/**< Signals that parsing stopped.  XMLParser::IsFinished,
		     XMLParser::IsFailed and XMLParser::IsOutOfMemory can be
		     used to determine the reason it stopped. */

		virtual BOOL Redirected(XMLParser *parser) { return TRUE; }
		/**< Signals that the URL loaded by the XML parser was
		     redirected.  This function will only be called if
		     XMLParser::Load was used to start the parsing, that is, if
		     the parser is loading the URL itself.

		     XMLParser::GetURL can be used to retrieve the URL initially
		     requested.  The redirect target can be retrieved from that
		     URL using the common URL API.

		     If this function returns FALSE, loading is stopped, and the
		     parser will claim to have failed.  XMLParser::IsFailed will
		     return TRUE.  If this function returns TRUE, loading
		     continues as usual.  The default implementation of this
		     function returns TRUE.

		     @return TRUE to allow the redirect, FALSE to abort
		             loading. */
	};

	static OP_STATUS Make(XMLParser *&parser, Listener *listener, FramesDocument *document, XMLTokenHandler *tokenhandler, const URL &url);
	/**< Create a parser that is used in a document.  Any external
	     entities loaded will be loaded as inlines in that document.

	     By default the XMLParser does not delete any of its arguments
	     when it is destroyed; they are owned by the caller or someone
	     else.  The caller must make sure all object remain valid for
	     as long as the XMLParser object is alive.  This means that
	     the caller must make sure to destroy the XMLParser object
	     before any of 'listener', 'document' or 'tokenhandler' are
	     destroyed, and naturally also that none of them can be
	     destroyed during a call from the XMLParser object.

	     The XMLParser can optionally be made the owner of the
	     listener (set SetOwnsListener()) and the token handler (see
	     SetOwnsTokenHandler()). In this case, the caller does not
	     need to care about when or how they are destroyed, as long as
	     the XMLParser object is destroyed somehow, and must not
	     destroy any of the objects of which the XMLParser object has
	     been made the owner.

	     @param parser Set to the create parser on success.
	     @param listener Optional listener.  Can be NULL.
	     @param document Document in the context of which this parser
	                     is used.  Can not be NULL.
	     @param tokenhandler Token handler.  Can not be NULL.
	     @param url URL that will be parsed.  Can be empty unless
	                XMLParser::Load will be used.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS Make(XMLParser *&parser, Listener *listener, MessageHandler *mh, XMLTokenHandler *tokenhandler, const URL &url);
	/**< Create a parser that is not used in a document.  URL:s loaded
	     by this parser will be loaded using 'mh'.  Note: if 'mh' is
	     NULL, external entities will not be loaded, regardless of
	     configuration.

	     By default the XMLParser does not delete any of its arguments
	     when it is destroyed; they are owned by the caller or someone
	     else.  The caller must make sure all objects remain valid for
	     as long as the XMLParser object is alive.  This means that
	     the caller must make sure to destroy the XMLParser object
	     before any of 'listener', 'mh' or 'tokenhandler' are
	     destroyed, and naturally also that none of them can be
	     destroyed during a call from the XMLParser object.

	     The XMLParser can optionally be made the owner of the
	     listener (set SetOwnsListener()) and the token handler (see
	     SetOwnsTokenHandler()). In this case, the caller does not
	     need to care about when or how they are destroyed, as long as
	     the XMLParser object is destroyed somehow, and must not
	     destroy any of the objects of which the XMLParser object has
	     been made the owner.

	     @param parser Set to the create parser on success.
	     @param listener Optional listener.  Can be NULL.
	     @param mh Message handler to use when loading URL or posting
	               messages.  Can be NULL.
	     @param tokenhandler Token handler.  Can not be NULL.
	     @param url URL that will be parsed.  Can be empty unless
	                XMLParser::Load will be used.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual ~XMLParser();
	/**< Destructor. */

	/** Says whether external entities (the external subset of the DTD
	    and any external entities referenced from the DTD or markup)
	    should be loaded.  Note that even when disabled, some external
	    entities may be loaded from local files.  However, this is
	    transparent to the caller, and will particulary not make the
	    parser more asynchronous or make it act differently in any
	    way observable by the client code. */
	enum LoadExternalEntities
	{
		LOADEXTERNALENTITIES_DEFAULT,
		/**< Let the preference "User Prefs/XML Load External
		     Entities" decide. */

		LOADEXTERNALENTITIES_YES,
		/**< Load external entities. */

		LOADEXTERNALENTITIES_NO
		/**< Do not load external entities. */
	};

	/** Parse mode. */
	enum ParseMode
	{
		PARSEMODE_DOCUMENT,
		/**< Parse data as a complete XML document.  Top level
		     character data is not allowed, and there must be exactly
		     one top level element (the root element.) */

		PARSEMODE_FRAGMENT
		/**< Parse data as an XML fragment.  This means that character
		     data is allowed at top level, and zero or more top level
		     elements are allowed.  To disallow XML declarations or
		     document type declarations, use the appropriate fields in
		     the Configuration structure. */
	};

#ifdef XML_VALIDATING
	/** Validation mode.  If validation is selected, a validation
	    token handler must also be used or not all validation errors
	    will be detected.

	    @see XMLParser::MakeValidatingTokenHandler */
	enum ValidationMode
	{
		VALIDATIONMODE_NONE,
		/**< No validation is performed. */

		VALIDATIONMODE_STRICT,
		/**< Validation is performed and validation errors are treated
		     the same way as well-formedness errors. */

		VALIDATIONMODE_REPORT,
		/**< Validation is performed and validation errors are
		     reported but parsing will continue as if there were no
		     errors. */
	};
#endif // XML_VALIDATING

	/** Parser configuration.  Used to customize the parser.  The
	    default values are the ones used by the parser if no
	    configuration is set at all. */
	class Configuration
	{
	public:
		Configuration();
		/**< Constructor that sets the following default values:

		     <pre>
		       load_external_entities:        LOADEXTERNALENTITIES_DEFAULT
		       parse_mode:                    PARSEMODE_DOCUMENT
		       allow_xml_declaration:         TRUE
		       allow_doctype:                 TRUE
		       store_internal_subset:         FALSE
		       validation_mode:               VALIDATIONMODE_NONE
		       preferred_literal_part_length: 0
		       generate_error_report:         FALSE
		       error_report_context_lines:    7
		       nsdeclaration:                 0
		       max_tokens_per_call:           4096
		       max_nested_entity_references:  XMLUTILS_DEFAULT_MAX_NESTED_ENTITY_REFERENCES
		     </pre>

		     Note: the default for 'max_nested_entity_references' is defined by
		     the tweak TWEAK_XMLUTILS_MAX_NESTED_ENTITY_REFERENCES. */

		Configuration(const Configuration &other);
		/**< Copy constructor. */

		void operator= (const Configuration &other);
		/**< Assignment constructor. */

		LoadExternalEntities load_external_entities;
		/**< Controls whether external entities are loaded. */

		ParseMode parse_mode;
		/**< Selects parse mode. */

		BOOL allow_xml_declaration;
		/**< If FALSE, the presence of an XML declaration will be
		     considered a fatal error. */

		BOOL allow_doctype;
		/**< If FALSE, the presence of a document type declaration
		     will be considered a fatal error. */

		BOOL store_internal_subset;
		/**< If TRUE, the internal DTD subset (if any) is stored in
		     the XMLDocumentInformation object. */

#ifdef XML_VALIDATING
		ValidationMode validation_mode;
		/**< Selects validation mode. */
#endif // XML_VALIDATING

		unsigned preferred_literal_part_length;
		/**< Informs the parser of how long literal parts the client
		     is capable of handling.  If non-zero,
		     XMLToken::GetLiteral can be used to retrieve long
		     literals in suitably long parts. */

#ifdef XML_ERRORS
		BOOL generate_error_report;
		/**< If TRUE, an XMLErrorReport object is created when a parse
		     error is encountered. */

		unsigned error_report_context_lines;
		/**< If 'generate_error_report' is TRUE, this number of lines
		     of context is used in the generated report.  A value of
		     7, for instance, means 3 lines above the line containing
		     the first error character and 3 lines below the line
		     containing last error character, resulting in a total of
		     7 lines for a one-line error location. */
#endif // XML_ERRORS

		XMLNamespaceDeclaration::Reference nsdeclaration;
		/**< Namespace declarations in scope (a pointer to the nearest
		     namespace declaration.)  This is only used when
		     'parse_mode' is PARSEMODE_FRAGMENT.  To be used when the
		     fragment is parsed into an element in an existing
		     document and the namespace declarations in scope on that
		     element should apply to the parsed fragment. */

		unsigned max_tokens_per_call;
		/**< Limits the max number of tokens the parser will process
		     in each call to XMLParser::Parse.  This will limit the
		     number of times the token listener is called but also the
		     number of entity expansions that is performed during one
		     call (expanding an entity doesn't necessarily trigger a
		     call to the token listener, so this limit can be reached
		     during one call without the token listener being called
		     even once.)

		     If the value is zero the limit is disabled.  Don't use
		     this when parsing untrusted XML!  If you don't want to
		     deal with it "properly," set a reasonably high limit
		     (2^20 is probably always enough, and often too much) and
		     fail if the parser pauses. */

		unsigned max_nested_entity_references;
		/**< Limits the number of nested entity references the parser
		     will process before aborting the parsing.  This limit is
		     used to catch maliciously constructed XML documents with
		     massively nested entity references/definitions that ends
		     up producing near-infinite amounts of output from the
		     parser.

		     If the value is zero the limit is disabled. */
	};

	virtual void SetConfiguration(const Configuration &configuration) = 0;
	/**< Update the parser's configuration.  Can be called any number
	     of times, but should be called before parsing is started
	     (changing the configuration after that may lead to unexpected
	     results.)  The parser's default configuration the same as the
	     default values set by Configuration's constructor. */

	virtual void SetOwnsListener() = 0;
	/**< Makes the parser delete the listener (if one was provided)
	     when the parser is destroyed (and never earlier.)  By default
	     the listener is owned by the calling code and is not deleted
	     by the parser. */

	virtual void SetOwnsTokenHandler() = 0;
	/**< Makes the parser delete the token handler when the parser is
	     destroyed (and never earlier.)  By default the token handler
	     is owned by the calling code and is not deleted by the
	     parser. */

	virtual OP_STATUS Load(const URL &referrer_url, BOOL delete_when_finished = FALSE, unsigned load_timeout = 0, BOOL bypass_proxy = FALSE) = 0;
	/**< Load and parse the URL used to create the parser (the 'url'
	     argument to XMLParser::Make.)  The parser must either have
	     been created with a document or with a non-NULL message
	     handler, or it will have no way to load the URL.  Parse
	     progress is signalled through the token handler and by
	     calling the function XMLParser::Listener::Stopped, if there
	     was a listener.

	     If the argument 'delete_when_finished' is TRUE, the parser
	     will automatically delete itself after the parsing has
	     stopped (whether finished or failed.)  If so, the owner
	     should clear all their references to the parser once the
	     function Listener::Stopped() has been called.  Note: if this
	     function fails (returns something other than OpStatus::OK,)
	     the parser will not have deleted itself and must be deleted
	     by the caller.  Also note: the owner can still delete the
	     parser themselves, as long as this happens before
	     Listener::Stopped has been called.

		 If the load_timeout argument is non-zero then the loading will abort
		 after the given amount of time, Listener::Stopped() will be called,
		 and the parser will have a failed status.  If any data is
		 received from the server no timeout will occure, though
		 regular loading failures may still happen.

	     @param referrer_url URL to use as the referrer URL in the URL
	                         request.  Can be empty.
	     @param delete_when_finished If TRUE, the parser deletes itself
	                                 when it has stopped parsing.
		 @param load_timeout If no reply is received to the load request
		                     within this time loading is aborted.  Time in ms.
		                     If 0 then there is no timeout and loading will
		                     wait "forever" for a reply.
	     @param bypass_proxy If TRUE, the request will not be sent through any configured proxy.

	     @return OpStatus::OK, OpStatus::ERR if the parser was
	             incorrectly created or if the URL request failed or
	             OpStatus::ERR_NO_MEMORY. */

#if defined XMLUTILS_PARSE_RAW_DATA || defined XMLUTILS_PARSE_UTF8_DATA
	virtual OP_STATUS Parse(const char *data, unsigned data_length, BOOL more, unsigned *consumed = 0) = 0;
	/**< Parse XML from a string.  All data is consumed, unless the
	     'consumed' argument is non-NULL (in which case all data might
	     still be consumed, of course.)

	     If all data is not consumed, the unconsumed data should be
	     included in the next call to Parse, while the consumed data
	     should not.  Note that the parser does not remember anything
	     about the unconsumed data, nor depend in any way on it, so it
	     is allowed to use completely different data in the next call
	     to Parse, if that is appropriate.

	     @param data Data.  Does not need to be null-terminated.
	     @param data_length Number of (8-bit) characters in data.
	     @param more TRUE if more data may come, FALSE otherwise.
	     @param consumed If not NULL, set to the number of characters
	                     out of 'data_length' that was consumed.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY (note: parse
	             errors are not signalled by the return value from
	             this function.) */
#endif // XMLUTILS_PARSE_RAW_DATA || XMLUTILS_PARSE_UTF8_DATA

#ifdef XMLUTILS_PARSE_UNICODE_DATA
	virtual OP_STATUS Parse(const uni_char *data, unsigned data_length, BOOL more, unsigned *consumed = 0) = 0;
	/**< Parse XML from a string.  All data is consumed, unless the
	     'consumed' argument is non-NULL (in which case all data might
	     still be consumed, of course.)

	     If all data is not consumed, the unconsumed data should be
	     included in the next call to Parse, while the consumed data
	     should not.  Note that the parser does not remember anything
	     about the unconsumed data, nor depend in any way on it, so it
	     is allowed to use completely different data in the next call
	     to Parse, if that is appropriate.

	     @param data Data.  Does not need to be null-terminated.
	     @param data_length Number of (16-bit) characters in data.
	     @param more TRUE if more data may come, FALSE otherwise.
	     @param consumed If not NULL, set to the number of characters
	                     out of 'data_length' that was consumed.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY (note: parse
	             errors are not signalled by the return value from this
	             function.) */
#endif // XMLUTILS_PARSE_UNICODE_DATA

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
	virtual OP_STATUS ProcessToken(XMLToken &token) = 0;
	/**< Process the specified token.  Equivalent to calling one of
	     the other Parse() variants with a string that corresponds to
	     the specified token.  In particular, calls to this function
	     and the other Parse() variants can be mixed freely.  If the
	     parser was started using Load(), this function cannot be
	     used.

	     Normal well-formedness constraints apply; for instance, if
	     the specified token is an end-tag it must match the current
	     element.  If the specified token is (non-CDATA section)
	     character data, all characters in it are assumed to have been
	     properly escaped using character references and no further
	     processing of it is applied.

	     The purpose of this function is basically to support passing
	     a stream of tokens through the XML parser for well-formedness
	     checking, possibly interleaved with "raw strings" that are
	     parsed as XML source code.  One use-case is the result of an
	     XSLT transformation, where most of the result is generated as
	     tokens and "raw strings" are used for character data with the
	     "disable-output-escaping" flag set.

	     If the specified token is character data (its type is
	     XMLToken::TYPE_Text) it is merged with any uncommitted
	     character data currently buffered by the parser, and will not
	     be reported to the parser's token handler until before the
	     parser reports another type of token (because the parser will
	     also merge it with subsequent character data.)  If the
	     specified token is not character data, the parser may report
	     a character data token before the specified token.

	     Processing a token with type TYPE_Finished will end the
	     document (or cause an "unexpected end-of-file" error.)  It is
	     equivalent to calling Parse() with no data and the 'more'
	     argument FALSE.  After that, neither ProcessToken() or Parse()
	     should be called again on this parser.

	     Processing a token with type TYPE_XMLDecl or TYPE_DOCTYPE will
	     update parser's XMLDocumentInformation object with information
	     from the token's XMLDocumentInformation object, if one is
	     supplied.  One should be, since otherwise such a token is
	     pretty pointless in this context.

	     @param token Token to process.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY (note: parse
	             errors are not signalled by the return value from this
	             function.) */
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

	virtual OP_STATUS SignalInvalidEncodingError() = 0;
	/**< Trigger an error saying that the next character to parse was
	     not valid in the used character encoding.  Should be used
	     directly after a call to Parse() that parses up to but not
	     including the invalid character.  (The invalid character will
	     typically be represented as U+FFFD in the decoded character
	     stream.)

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual BOOL IsFinished() = 0;
	/**< Returns TRUE if the parsing has finished successfully.

	     @return TRUE if the parsing has finished successfully. */

	virtual BOOL IsFailed() = 0;
	/**< Returns TRUE if the parsing has failed due to anything but an
	     out of memory condition.

	     @return TRUE if the parsing has failed due to anything but an
	             out of memory condition. */

	virtual BOOL IsOutOfMemory() = 0;
	/**< Returns TRUE if the parsing has failed due to an out of
	     memory condition.

	     @return TRUE if the parsing has failed due to an out of
	             memory condition. */

	virtual BOOL IsPaused() = 0;
	/**< Returns TRUE if the last call to Parse returned simply
	     because parsing was paused, not because it ran out of data to
	     parse, is waiting for an external entity or because the token
	     handler told it to block parsing.

	     @return TRUE if the parser paused parsing. */

	virtual BOOL IsBlockedByTokenHandler() = 0;
	/**< Returns TRUE if the last call to the parser's token handler
	     resulted in it returning XMLTokenHandler::RESULT_BLOCK.

	     @return TRUE if the token handler has blocked parsing. */

	virtual URL GetURL() = 0;
	/**< Returns the URL passed to XMLParser::Make when this parser
	     was created.

	     @return The parser's URL. */

	virtual int GetLastHTTPResponse() = 0;
	/**< Returns the last HTTP response code recorded
	 *   Returns 0 if we haven't received any HTTP response codes yet

	     @return The HTTP response code or 0 */

	virtual const XMLDocumentInformation &GetDocumentInformation() = 0;
	/**< Returns information about the document and document type.

	     @return Document information. */

	virtual XMLVersion GetXMLVersion() = 0;
	/**< Returns the XML version of document entity.

	     @return The XML version of document entity. */

	virtual XMLStandalone GetXMLStandalone() = 0;
	/**< Returns the standalone declaration value.

	     @return The standalone declaration value. */

#ifdef XML_ERRORS
	virtual const XMLRange &GetErrorPosition() = 0;
	/**< Returns the position of the error, if IsFailed() is TRUE.
	     If IsFailed() is FALSE, returns an invalid range.

	     @return A range. */

	virtual void GetErrorDescription(const char *&error, const char *&uri, const char *&fragment_id) = 0;
	/**< Fetches a string describing the error, a string containing
	     the URI of the specification the error relates to, and an
	     optional fragment identifier for the URI.  The fragment
	     identifier is set to NULL if no specific part of the
	     specification describes the cause of the error.  If
	     IsFailed() is FALSE, all three strings are set to NULL.

	     @param error Set to a string describing the error.
	     @param uri Set to the URI of a specification.
	     @param fragment_id Set to a fragment identifier or NULL. */

	virtual const XMLErrorReport *GetErrorReport() = 0;
	/**< Returns the generated error report, if one was generated.
	     The object is owned by the parser and cannot be deleted by
	     caller.

	     @return An error report, owned by the parser, or NULL. */

	virtual XMLErrorReport *TakeErrorReport() = 0;
	/**< Returns the generated error report, if one was generated.
	     The object is owned by and must be deleted by the caller.
	     Once this function has been called, subsequent calls to it
	     and to GetErrorReport will return NULL.

	     @return An error report, owned by the caller, or NULL. */

	/** Class used to fetch the data of the document (from the
	    beginning) when an error report is generated after parsing
	    failed.  If all document data is not available, this object
	    should just return as much as possible.  If no data is
	    available, the function IsAvailable should return FALSE, in
	    which case no error report is generated.  An object of this
	    class need only be provided if an error report is wanted and
	    XMLParser::Parse is used (as opposed to XMLParser::Load.) */
	class ErrorDataProvider
	{
	public:
		virtual BOOL IsAvailable() = 0;
		/**< Should return TRUE if any data can be provided for the
		     error report, and FALSE otherwise.

		     @return TRUE or FALSE. */

		virtual OP_STATUS RetrieveData(const uni_char *&data, unsigned &data_length) = 0;
		/**< Called to retrieve more data.  Will be called until
		     data_length is set to zero.

		     @param data Should be set to point at the beginning of a
		                 block of data.
		     @param data_length Should be set to the number of
		                        characters in the block pointed at by
		                        data, or zero if there is no more
		                        data available.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	protected:
		virtual ~ErrorDataProvider() {}
		/**< XMLParser doesn't delete its error data provider. */
	};

	virtual void SetErrorDataProvider(ErrorDataProvider *provider) = 0;
	/**< Set the error data provider.  The object needs to live at
	     least past the last call to XMLParser::Parse.

	     @param provider Error data provider. */
#endif // XML_ERRORS

	virtual URL GetCurrentEntityUrl() = 0;
	/**< Returns the URL of the entity the parser is currently
	     parsing.  This is the URL used to construct the parser,
	     unless the parser is currently parsing an external entity.

	     @return The URL of the current entity. */

#ifdef XML_VALIDATING
	virtual OP_STATUS MakeValidatingTokenHandler(XMLTokenHandler *&tokenhandler, XMLTokenHandler *secondary, XMLValidator::Listener *listener) = 0;
	/**< Creates a token handler that validates the document according
	     to the document type declaration in the document (which is
	     required in a valid document.)  The secondary token handler
	     will be passed unmodified tokens.

	     For this to be supported, the parser must first be configured
	     with these settings:

	       load_external_entities:        LOADEXTERNALENTITIES_YES
	       parse_mode:                    PARSEMODE_DOCUMENT
	       allow_xml_declaration:         TRUE
	       allow_doctype:                 TRUE
	       validation_mode:               VALIDATIONMODE_STRICT or VALIDATIONMODE_REPORT

	     @param tokenhandler Set to the newly created token handler.
	     @param secondary Secondary tokenhandler that will receive all
	                      tokens this token handler receives.
	     @param listener Validation listener.
	     @return OpStatus::OK, OpStatus::ERR if this parser is not
	             configured appropriately or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS MakeValidatingSerializer(XMLSerializer *&serializer, XMLValidator::Listener *listener) = 0;
	/**< Not supported.

	     @param serializer Not touched.
	     @param listener Not used.
	     @return OpStatus::ERR. */
#endif // XML_VALIDATING

#ifdef OPERA_CONSOLE
	virtual OP_STATUS ReportConsoleError() = 0;
	/**< If parsing failed, report an error in the message console.
	     Will only have effect if IsFailed() is TRUE and
	     g_console->IsLogging() is TRUE.  To get a more detailed and
	     useful message,
	     XMLParser::Configuration::generate_error_report needs to be
	     TRUE.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS ReportConsoleMessage(OpConsoleEngine::Severity severity, const XMLRange &position, const uni_char *message) = 0;
	/**< Intended mainly for internal use.  Reports a message in the
	     message console with apropriate context information for the
	     resource parsed by this XML parser. */
#endif // OPERA_CONSOLE
};

#endif // XMLPARSER_H
