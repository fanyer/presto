/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef XSLT_H
#define XSLT_H

#ifdef XSLT_SUPPORT

#include "modules/logdoc/logdocenum.h"
#include "modules/xmlutils/xmllanguageparser.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltreeaccessor.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xpath/xpath.h"
#include "modules/url/url2.h"

class XSLT_Stylesheet;

/** Parser for XSLT stylesheets.  Create using the Make() function, destroy
    using the Free() function, and fetch the parsed XSLT stylesheet using the
    GetStylesheet() function, after successful parsing.  See the documentation
    for XMLLanguageParser in the xmlutils module for details about how to
    parse using an XMLLanguageParser-based parser. */
class XSLT_StylesheetParser
	: public XMLLanguageParser
{
public:
	class Callback
	{
	public:
		virtual OP_STATUS LoadOtherStylesheet(URL stylesheet_url, XMLTokenHandler *token_handler, BOOL is_import);
		/**< Called when the stylesheet parser encounters an xsl:import
		     (is_import == TRUE) or xsl:include (is_import == FALSE) directive
		     and wants to load another stylesheet.

		     The implementation has two choices: start loading and parsing the
		     requested resource in such a way that the supplied token handler
		     receives the resource's tokens, or refuse.  Refusal is signalled
		     by returning OpStatus::ERR, in which case the whole stylesheet
		     parsing will fail.  OpStatus::OK signals that the requested
		     resource is being loaded.

		     If the request is refused, the XSLT stylesheet parser will *not*
		     report any error to the message console; it will just stop.

		     The default implementation refuses the request without reporting
		     any errors, and should normally not be used.

		     @param parser XSLT stylesheet parser.
		     @param stylesheet_url URL of additional stylesheet to load.
		     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

		virtual void CancelLoadOtherStylesheet(XMLTokenHandler *token_handler);
		/**< Cancel a previous call to CancelLoadOtherStylesheet().  The token
		     handler should be used to identify which call to cancel.  The
		     cancelling must be such that it is okay to delete the token
		     handler directly after this function returns.

		     @param token_handler Token handler. */

#ifdef XSLT_ERRORS
		/** Message type used for HandleMessage(). */
		enum MessageType
		{
			MESSAGE_TYPE_ERROR,
			/**< Fatal error that will cause the parsing to fail. */
			MESSAGE_TYPE_WARNING
			/**< Warning about an non-fatal error in the stylesheet. */
		};

		virtual OP_BOOLEAN HandleMessage(MessageType type, const uni_char *message);
		/**< Called when the stylesheet parser encounters an error, with an
		     error message describing it.  If the calling code wants to handle
		     the error message itself, it should return OpBoolean::IS_TRUE,
		     otherwise it should return OpBoolean::IS_FALSE, in which case the
		     stylesheet parser will post an error message to the message
		     console (if FEATURE_CONSOLE is enabled.)

		     This callback is enabled by TWEAK_XSLT_ERRORS.

		     @param type Message type.
		     @param message Error message.  No formatting to speak of, but
		                    sometimes contains linebreaks.

		     @return OpBoolean::IS_TRUE, OpBoolean::IS_FALSE or
		             OpStatus::ERR_NO_MEMORY. */
#endif // XSLT_ERRORS

		virtual OP_STATUS ParsingFinished(XSLT_StylesheetParser *parser);
		/**< Called when the stylesheet parser has finished parsing the
		     stylesheet.  The default implementation does nothing.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	protected:
		virtual ~Callback() {}
		/**< The callback is not owned by the XSL stylesheet parser, and must
		     be destroyed by the caller after the stylesheet parser has been
		     destroyed. */
	};

	static OP_STATUS Make(XSLT_StylesheetParser *&parser, Callback *callback);
	/**< Creates an XSLT parser. */

	static void Free(XSLT_StylesheetParser *parser);
	/**< Destroys an XSLT parser. */

	virtual OP_STATUS GetStylesheet(XSLT_Stylesheet *&stylesheet) = 0;
	/**< Retrieves the parsed stylesheet and resets the parser. */

protected:
	~XSLT_StylesheetParser();
	/**< Use XSLT_StylesheetParser::Free to free stylesheet parsers. */
};

class XSLT_Stylesheet
{
public:
	static void Free(XSLT_Stylesheet *stylesheet);
	/**< Destroys an XSLT stylesheet.  All transformations based on this
	     stylesheet must be stopped before the stylesheet is destroyed. */

	/** Class representing the input to a transformation, essentially the
	    source tree and any supplied parameter values.  The referenced
	    XMLTreeAccessor object is not owned by the Input object; it needs to
	    be freed separately by the calling code.  The 'parameters' array along
	    with any parameter values is however owned by the Input object and is
	    freed by its destructor. */
	class Input
	{
	public:
		Input();
		/**< Constructor.  Sets all members to NULL or zero. */

		~Input();
		/**< Destructor.  Deletes the 'parameters' array, but no other
		     objects. */

		XMLTreeAccessor *tree;
		/**< Source tree accessor. */

		XMLTreeAccessor::Node *node;
		/**< Source node. */

		class Parameter
		{
		public:
			Parameter();
			/**< Constructor.  Sets all members to NULL or zero. */

			class Value
			{
			public:
				virtual ~Value() {}
				static OP_STATUS MakeNumber(Value *&result, double value);
				static OP_STATUS MakeBoolean(Value *&result, BOOL value);
				static OP_STATUS MakeString(Value *&result, const uni_char *value);
				static OP_STATUS MakeNode(Value *&result, XPathNode *value);
				static OP_STATUS MakeNodeList(Value *&result);
				virtual OP_STATUS AddNodeToList(XPathNode *value) = 0;
			};

			XMLExpandedName name;
			/**< Parameter name. */

			Value *value;
			/**< Parameter value. */
		};

		Parameter *parameters;
		/**< Parameters to the transformation.  Can be NULL if there are no
		     parameters at all. */

		unsigned parameters_count;
		/**< Number of used elements in the 'parameters' array. */
	};

	/** Output characteristics specified by the stylesheet.  The
	    strings are owned by the stylesheet and are valid until the
	    stylesheet is destroyed. */
	class OutputSpecification
	{
	public:
		enum Method
		{
			METHOD_XML,
			METHOD_HTML,
			METHOD_TEXT,
			METHOD_UNKNOWN // HTML or XML depending on transform result
		};

		Method method;
		const uni_char *version;
		const uni_char *encoding;
		BOOL omit_xml_declaration;
		XMLStandalone standalone;
		const uni_char *doctype_public_id;
		const uni_char *doctype_system_id;
		const uni_char *media_type;
	};

	/** Output form.  Note: the availability and meaning of the output forms
	    varies somewhat depending on the output method specified in the
	    stylesheet, but they are otherwise independent.  Selecting a certain
	    output form is never an alternative to specifying a certian output
	    method in the stylesheet; the output form does not override the output
	    method of the stylesheet. */
	enum OutputForm
	{
		OUTPUT_XMLTOKENHANDLER,
		/**< Produce output the the form of XMLToken objects sent to a
		     supplied XMLTokenHandler object.  Only available for stylesheets
		     with the output method 'xml.'  An XMLTokenHandler must be set
		     trough a call to the Transformation::SetXMLTokenHandler()
		     function before the first call to Transformation::Transform(). */

		OUTPUT_STRINGDATA,
		/**< Produce output in the form of string data.  If the output method
		     of the stylesheet is 'text,' the resulting string data is the
		     result text, otherwise the string data is the serialization of
		     the result tree.  The string data is produced in chunks and sent
		     to a string collection callback that must be set through a call
		     to the Transformation::SetStringCollector() function before the
		     first call to Transformation::Transform(). */

		OUTPUT_DELAYED_DECISION
		/**< If the resulting type of the output can't be determined
		     statically this can be used, and neither a token handler nor a
		     string collector set. The call to Transform will then return with
		     TRANSFORM_NEEDS_OUTPUTHANDLER and the resulting type can be read
		     from the Transformation object. The user must then call
		     SetDelayedOutputForm() and set either a token handler or a string
		     collector. */
	};

	/** Interface representing an ongoing transformation. */
	class Transformation
	{
	public:
		/**< Transformation status codes. */
		enum Status
		{
			TRANSFORM_PAUSED,
			/**< The transformation paused but can be continued immediately.
			     If possible, the calling code should let Opera process
			     pending messages before it continues the transformation. */

			TRANSFORM_BLOCKED,
			/**< The transformation is blocked waiting for something external
			     to the XSLT transformation (such as the loading of an XML
			     document or the evaluation of an XPath expression.)  The
			     calling code should continue the transformation when its
			     callback is called.  It should set a callback if it hasn't
			     already done so. */

			TRANSFORM_NEEDS_OUTPUTHANDLER,
			/**< This can returned from Transform() if the tranformation was
			     started with the OUTPUT_DELAYED_DECISION outputform. */

			TRANSFORM_FINISHED,
			/**< The transformation has finished successfully. */

			TRANSFORM_FAILED,
			/**< The transformation failed.  An error will have been reported
			     to the message console (if it is enabled.) */

			TRANSFORM_NO_MEMORY
			/**< The transformation failed due to an OOM error. */
		};

		virtual Status Transform() = 0;
		/**< Start or continue the transformation.  If TRANSFORM_PAUSED is
		     returned, the transformation is ready to continue immediately
		     after a call. The same is true if TRANSFORM_NEEDS_OUTPUTHANDLER
		     is returned, but before any further calls to Transform a output
		     handler must be set. This return value will only be returned once
		     and only it the OUTPUT_DELAYED_DECISION outputform was specified.

			 If TRANSFORM_BLOCKED is returned, the transformation cannot
		     continue now, and the callback will be called when it can
		     continue.  Once TRANSFORM_FINISHED or TRANSFORM_FAILED has been
		     returned, the transformation has stopped, and this function will
		     return the same value if called again.  If TRANSFORM_NO_MEMORY is
		     returned, it may be possible to continue the transformation
		     later, assuming some memory has become available then.  If not,
		     STATUS_FAILED will be returned the next time this function is
		     called. */

		class Callback
		{
		public:
			virtual void ContinueTransformation(Transformation *transformation) = 0;
			/**< Called when a previously blocked transformation is ready to
			     be continued.  The transformation cannot be continued
			     immediately during this call however; a message should be
			     posted that continues the transformation. */

			virtual OP_STATUS LoadDocument(URL document_url, XMLTokenHandler *token_handler) = 0;
			/**< Called when the document() function is called and a document
			     needs to be loaded.  The document URL should be loaded and
			     parsed in such a way that the parsed tokens are fed into the
			     supplied token handler.  The token handler is owned by the
			     XSLT module internally, and should not be freed by anyone
			     else.

			     If access to the resource is not permitted, OpStatus::ERR can
			     be returned.  The document() function will then return an
			     empty nodeset.

			     @param document_url URL to load.
			     @param token_handler Token handler.
			     @param OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

			virtual void CancelLoadDocument(XMLTokenHandler *token_handler) {}
			/**< Called to cancel a previous call to LoadDocument().  The
			     token handler should be used to identify which call to
			     cancel.  The cancelling must be such that it is okay to
			     delete the token handler directly after this function
			     returns.

			     @param token_handler Token handler. */

#ifdef XSLT_ERRORS
			/** Message type used for HandleMessage(). */
			enum MessageType
			{
				MESSAGE_TYPE_ERROR,
				/**< Fatal error that will cause the transformation to be
				     aborted.  This includes messages triggered by
				     instantiating an xsl:message element whose 'terminate'
				     attribute has the value 'yes'. */
				MESSAGE_TYPE_WARNING,
				/**< Warning about an non-fatal error in the stylesheet. */
				MESSAGE_TYPE_MESSAGE
				/**< Message triggered by instantiating an xsl:message element
				     whose 'terminate' attribute does not have the value
				     'yes'. */
			};

			virtual OP_BOOLEAN HandleMessage(MessageType type, const uni_char *message);
			/**< Called when a transformation encounters a fatal error and
			     aborts, when it encounters a non-fatal error and warns about
			     it or when it instantiates an xsl:message element.  The
			     'type' argument indicates which.

			     If the calling code wants to handle the error message itself,
			     it should return OpBoolean::IS_TRUE, otherwise it should
			     return OpBoolean::IS_FALSE, in which case the XSLT engine
			     will post an error message to the message console (assuming
			     FEATURE_CONSOLE is enabled.)

			     This callback is enabled by TWEAK_XSLT_ERRORS.

			     @param type Message type.
			     @param message Message.  No formatting to speak of, but
			                    sometimes contains linebreaks.

			     @return OpBoolean::IS_TRUE, OpBoolean::IS_FALSE or
			             OpStatus::ERR_NO_MEMORY. */
#endif // XSLT_ERRORS

		protected:
			virtual ~Callback() {}
			/**< Destructor.  The callback object is not owned by the
			     transformation and is not deleted by it. */
		};

		virtual void SetCallback(Callback *callback) = 0;
		/**< Set a callback that is notified when the transformation is ready
		     be continued after being blocked.  The callback is not owned by
		     the transformation and will not be deleted by it. */


		/* Output handling. */
		virtual void SetDefaultOutputMethod(OutputSpecification::Method method) = 0;
		/**< Sets the default output method, if one isn't specified in the
		     stylesheet.  Normally, the default output method is determined
		     dynamicly depending on the root element in the result tree
		     according to rules specified in the XSLT specification.  If this
		     function is used to set an explicit default output method, those
		     rules are set aside, but an explicit output method specified in
		     the stylesheet is honoured if present. */

		virtual void SetDelayedOutputForm(OutputForm outputform) = 0;
		/**< This method must be used when transform was started with the
		     OUTPUT_DELAYED_DECISION outputform and the Transform() call
		     returned with TRANSFORM_NEEDS_OUTPUTHANDLER.  This should be
		     followed by a call to SetXMLTokenHandler or
		     SetStringDataCollector. */

		virtual OutputSpecification::Method GetOutputMethod() = 0;
		/**< Returns the actual output method.  Should be called after a
		     Transform with output form OUTPUT_DELAYED_DECISION has stopped
		     with TRANSFORM_NEEDS_OUTPUTHANDLER and will then be one of the
		     values in the enum, except for the METHOD_UNKNOWN. */

		virtual void SetXMLTokenHandler(XMLTokenHandler *tokenhandler, BOOL owned_by_transformation) = 0;
		/**< If the output form OUTPUT_XMLTOKENHANDLER was selected, this
		     function must be called before the first call to Transform() to
		     set the token handler to send the output to.  The token handler
		     is owned by the Transformation object if the parameter
		     'owned_by_transformation' is TRUE.  In that case it is deleted
		     when the transformation is stopped through a call to
		     XSLT_Stylesheet::StopTransformation.  Otherwise, the token
		     handler is owned by the calling code and must remain valid till
		     after the last call to Transform().  It can be deleted before the
		     transformation is stopped; the token handler is never called from
		     XSLT_Stylesheet::StopTransformation(). */

		virtual void SetXMLParseMode(XMLParser::ParseMode parsemode) = 0;
		/**< If the output form OUTPUT_XMLTOKENHANDLER was selected, this
		     function can be called (at the same time that
		     SetXMLTokenHandler() is called, typically) to set the parse mode
		     of the XML parser used to check the well-formedness of the
		     output.  The default is PARSEMODE_DOCUMENT.  See the
		     documentation for XMLParser::ParseMode for the exact meaning of
		     the different possible values.  The XSLT engine makes no
		     interpretation of them itself, it only passes them along to an
		     XMLParser object. */

		class StringDataCollector
		{
		public:
			virtual ~StringDataCollector() {}
			/**< Destructor. */

			virtual OP_STATUS CollectStringData(const uni_char *string, unsigned string_length) = 0;
			/**< Called to collect string data.  It is unspecified how often
			     it is called during a transformation; the result may or may
			     not be buffered internally before being sent to the
			     collector.  All data will have been sent to the collector
			     before the transformation reports it has been finished.  If
			     the collector runs out of memory, it can return
			     OpStatus::ERR_NO_MEMORY, which will cause the current call to
			     Transformation::Transform() to return STATUS_NO_MEMORY, and a
			     subsequent call to Transformation::Transform() to send the
			     same (or more) string data to the string collector again.

			     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

			virtual OP_STATUS StringDataFinished() = 0;
			/**< Called when all string data has been processed.

			     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
		};

		virtual void SetStringDataCollector(StringDataCollector *collector, BOOL owned_by_transformation) = 0;
		/**< If the output form OUTPUT_STRINGDATA, this function must be
		     called before the first call to Transform() to set the string
		     data collector to send output to.  The string data collector is
		     owned by the Transformation if the parameter
		     'owned_by_transformation' is TRUE.  In that case it is deleted
		     when the transformation is stopped through a call to
		     XSLT_Stylesheet::StopTransformation.  Otherwise, the string data
		     collector is owned by the calling code and must remain valid till
		     after the last call to Transform().  It can be deleted before the
		     transformation is stopped; the string data collector is never
		     called from XSLT_Stylesheet::StopTransformation(). */

	protected:
		virtual ~Transformation() {}
		/**< Destructor.  Use XSLT_Stylesheet::StopTransformation() to (stop
		     and) free a transformation. */
	};

	virtual const OutputSpecification *GetOutputSpecification() = 0;
	/**< Returns the output characteristics specified by the stylesheet.  The
	     returned object is owned by the stylesheet and is valid until the
	     stylesheet is destroyed. */

	virtual BOOL ShouldStripWhitespaceIn(const XMLExpandedName &name) = 0;
	/**< Returns TRUE if whitespace-only text nodes in elements name 'name'
	     should be stripped in the source (input) tree.  The lookup can be
	     considered quick enough that there is no need for caching of the
	     result (beyond the trivial.)

	     If 'name' is empty (as made by default constructor) TRUE is returned
	     whitespace should be stripped in any elements at all.  This can be
	     used to determine whether the source document needs to be traversed
	     at all.

	     @return TRUE or FALSE. */

	virtual OP_STATUS StartTransformation(Transformation *&transformation, const Input &input, OutputForm outputform) = 0;
	/**< Start a transformation that applies this stylesheet to the source
	     tree specified in 'input' and produces output in the form requested
	     through 'outputform.'  The transformation is only initialized by this
	     function, no output will be produced until the function
	     Transformation::Transform() is called. The stylesheet can be freed
	     after this has finished successfully. */

	static OP_STATUS StopTransformation(Transformation *transformation);
	/**< Stop a transformation and destroy it.  No output is produced during a
	     call to this function, and none of the callbacks set on the
	     transformation are called.  Any objects owned by the transformation
	     are destroyed. */

protected:
	virtual ~XSLT_Stylesheet();
	/**< Use XSLT_Stylesheet::Free() to free stylesheets. */
};


/* Enabled by API_XSLT_HANDLER. */
#ifdef XSLT_HANDLER_SUPPORT

/** Interface for handling XSLT inclusion and application while loading an XML
    document.  To use, sub-class this interface, create a token handler using
    XSLT_Handler::MakeTokenHandler() and parse the XML document using that
    token handler. */
class XSLT_Handler
{
public:
	static OP_STATUS MakeTokenHandler(XMLTokenHandler *&token_handler, XSLT_Handler *handler);

	virtual ~XSLT_Handler();
	/**< Destructor. */

	virtual URL GetDocumentURL() = 0;
	/**< Called to retrieve the URL of the XML document being parsed.  Needed
	     to resolve the 'href' attribute of any xml-stylesheet processing
	     instructions found. */

	enum ResourceType
	{
		RESOURCE_LINKED_STYLESHEET,
		/**< Stylesheet linked via an xml-stylesheet processing instruction in
		     the source document. */

		RESOURCE_IMPORTED_STYLESHEET,
		/**< Stylesheet imported by an xsl:import element in another
		     stylesheet already being parsed. */

		RESOURCE_INCLUDED_STYLESHEET,
		/**< Stylesheet included by an xsl:include element in another
		     stylesheet already being parsed. */

		RESOURCE_LOADED_DOCUMENT
		/**< Arbitrary XML document loaded via the document() function. */
	};

	virtual OP_STATUS LoadResource(ResourceType resource_type, URL resource_url, XMLTokenHandler *token_handler) = 0;
	/**< Called when an additional resource needs to be loaded.  The resource
	     URL should be loaded and parsed in such a way that the parsed tokens
	     are fed into the supplied token handler.  The token handler is owned
	     by the XSLT module internally, and should not be freed by anyone
	     else.

	     If access to the resource is not permitted, OpStatus::ERR can be
	     returned.  If the resource type is RESOURCE_LINKED_STYLESHEET, this
	     means the processing instruction is ignored, and the document is
	     loaded "unstyled" unless another processing instruction is found.  If
	     the resource type is RESOURCE_IMPORTED_STYLESHEET or
	     RESOURCE_INCLUDED_STYLESHEET, the parsing of the whole stylesheet
	     will fail.  If the resource type is RESOURCE_LOADED_DOCUMENT, the
	     effect will be the same as if the resource had been empty.

	     @param resource_type Type of resource to load.
	     @param resource_url URL of resource to load.
	     @param token_handler Token handler.
	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

	virtual void CancelLoadResource(XMLTokenHandler *token_handler)	{}
	/**< Called to cancel a previous call to LoadResource().  The token
	     handler should be used to identify which call to cancel.  The
	     cancelling must be such that it is okay to delete the token handler
	     directly after this function returns.

	     @param token_handler Token handler.*/

	/**< Tree collector interface.  Should be implemented together with the
	     XSLT_Handler interface. */
	class TreeCollector
	{
	public:
		virtual ~TreeCollector() {}
		/**< Destructor.  The tree collector will be destroyed when the tree
		     is no longer needed, or if XSLT processing is aborted early. */

		virtual XMLTokenHandler *GetTokenHandler() = 0;
		/**< Called to retrieve a token handler that will be called to process
		     the tokens that make up the tree to collect.  The token handler
		     is owned by the tree collector.

		     @return A token handler object. */

		virtual void StripWhitespace(XSLT_Stylesheet *stylesheet) = 0;
		/**< If the stylesheet included any declaration of elements in which
		     whitespace-only text nodes should be stripped, this function is
		     called prior to GetTreeAccessor() to perform the stripping.  The
		     implementation should use the function
		     XSLT_Stylesheet::ShouldStripWhitespaceIn() to determine whether
		     whitespace-only text nodes in an element should be stripped or
		     preserved, in addition to preserving such text nodes inside
		     elements with xml:space="preserve" attributes (and descendants of
		     such elements.) */

		virtual OP_STATUS GetTreeAccessor(XMLTreeAccessor *&tree_accessor) = 0;
		/**< Called once to retrieve a tree accessor accessing the collected
		     tree.  Will never be called before the token handler returned by
		     GetTokenHandler() has processed a XMLToken::TYPE_Finished token,
		     and thus should have finished collecting the tree.  The returned
		     tree accessor will be used throughout the XSLT handling, and must
		     remain valid as long as the tree collector objects is.  The tree
		     collector will be destroyed when the tree will no longer be used,
		     at which point the tree collector should destroy the tree
		     accessor and any other data allocated to represent the tree.

		     @param tree_accessor Should be set on success.
		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
	};

	virtual OP_STATUS StartCollectingSourceTree(TreeCollector *&tree_collector) = 0;
	/**< Called to create a tree collector for collecting the source tree,
	     when an XSL stylesheet has been found and is being loaded and
	     applied.  The created tree collector is owned by the caller, and will
	     be deleted when the tree it collects is no longer needed.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS OnXMLOutput(XMLTokenHandler *&tokenhandler, BOOL &destroy_when_finished);
	/**< Called when this token handler has determined that the actual output
	     is XML tokens.  This happens both when no xml-stylesheet processing
	     instructions linking XSL stylesheets are found, and when such are
	     found and the output of the XSL transformation is XML.

	     This function can either refuse to handle XML output by returning
	     OpStatus::ERR, or handle the XML tokens by creating its own token
	     handler, assign it to 'tokenhandler' and return OpStatus::OK.

	     If a token handler is assigned to 'tokenhandler' the argument
	     'destroy_when_finished' determines who owns it.  If TRUE, this object
	     owns the token handler and destroys it whenever it is destroyed.

	     The default implementation of this function returns OpStatus::ERR.

	     @param tokenhandler Should be set to a token handler if OpStatus::OK
	                         is returned.
	     @param destroy_when_finished If TRUE, the token handler set to
	                                  'tokenhandler' is owned by this object
	                                  and destroyed automatically.
	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS OnHTMLOutput(XSLT_Stylesheet::Transformation::StringDataCollector *&collector, BOOL &destroy_when_finished);
	/**< Called when this token handler has determined that the actual output
	     is HTML source code.  This happens only when a xml-stylesheet
	     processing instruction has been found and the output of the XSL
	     transformation is HTML.

	     This function can either refuse to handle HTML output by returning
	     OpStatus::ERR, or handle it by supplying a string data collector
	     that processes the HTML source code appropriately.

	     If a string data collector is assigned to 'collector' the argument
	     'destroy_when_finished' determines who owns it.  If TRUE, this object
	     owns the string data collector and destroys it whenever it is
	     destroyed.

	     The default implementation of this function returns OpStatus::ERR.

	     @param collector Should be set to a string data collector if
	                      OpStatus::OK is returned.
	     @param destroy_when_finished If TRUE, the string data collector set
	                                  to 'collector' is owned by this object
	                                  and destroyed automatically.
	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS OnTextOutput(XSLT_Stylesheet::Transformation::StringDataCollector *&collector, BOOL &destroy_when_finished);
	/**< Called when this token handler has determined that the actual output
	     is plain text.  This happens only when a xml-stylesheet processing
	     instruction has been found and the output of the XSL transformation
	     is text.

	     This function can either refuse to handle text output by returning
	     OpStatus::ERR, or handle it by supplying a string data collector that
	     processes the text data appropriately.

	     If a string data collector is assigned to 'collector' the argument
	     'destroy_when_finished' determines who owns it.  If TRUE, this object
	     owns the string data collector and destroys it whenever it is
	     destroyed.

	     The default implementation of this function returns OpStatus::ERR.

	     @param collector Should be set to a string data collector if
	                      OpStatus::OK is returned.
	     @param destroy_when_finished If TRUE, the string data collector set
	                                  to 'collector' is owned by this object
	                                  and destroyed automatically.
	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

	virtual void OnFinished() = 0;
	/**< Called when everything this token handler is going to do has been
	     done.  If a token handler created via OnXMLOutput() has been used, it
	     will have received its XMLToken::TYPE_Finished token by now.  If a
	     string data collector created via OnHTMLOutput() or OnTextOutput()
	     has been used, it will have received all data. */

	virtual void OnAborted() = 0;
	/**< Called if the loading, parsing or transforming performed by this
	     token handler is aborted. */

#ifdef XSLT_ERRORS
	/** Message type used for HandleMessage(). */
	enum MessageType
	{
		MESSAGE_TYPE_ERROR,
		/**< Fatal error that will cause the transformation to be aborted.
		     This includes messages triggered by instantiating an xsl:message
		     element whose 'terminate' attribute has the value 'yes'. */
		MESSAGE_TYPE_WARNING,
		/**< Warning about an non-fatal error in the stylesheet. */
		MESSAGE_TYPE_MESSAGE
		/**< Message triggered by instantiating an xsl:message element whose
		     'terminate' attribute does not have the value 'yes'. */
	};

	virtual OP_BOOLEAN HandleMessage(MessageType type, const uni_char *message);
	/**< Called when the XSLT engine encounters an error, with an error
	     message describing it.  If the calling code wants to handle the error
	     message itself, it should return OpBoolean::IS_TRUE, otherwise it
	     should return OpBoolean::IS_FALSE, in which case the XSLT engine will
	     post an error message to the message console (if FEATURE_CONSOLE is
	     enabled.)

	     This callback is enabled by TWEAK_XSLT_ERRORS.

	     @param type Message type.
	     @param message Error message.  No formatting to speak of, but
	                    sometimes contains linebreaks.

	     @return OpBoolean::IS_TRUE, OpBoolean::IS_FALSE or
	             OpStatus::ERR_NO_MEMORY. */
#endif // XSLT_ERRORS
};

#endif // XSLT_SOURCETREETOKENHANDLER_SUPPORT
#endif // XSLT_SUPPORT
#endif // XSLT_H
