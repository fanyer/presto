/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef XMLSERIALIZER_H
#define XMLSERIALIZER_H

#ifdef XMLUTILS_XMLSERIALIZER_SUPPORT

class XMLDocumentInformation;
class XMLFragment;
class MessageHandler;
class URL;
class HTML_Element;
class TempBuffer;

class XMLSerializer
{
public:
	class Callback
	{
	public:
		enum Status
		{
			STATUS_COMPLETED,
			/**< The serialization completed successfully. */

			STATUS_FAILED,
			/**< The serialization failed. */

			STATUS_OOM
			/**< The serialization failed with an OOM condition. */
		};

		virtual void Stopped(Status status) = 0;

	protected:
		virtual ~Callback() {}
		/**< XMLSerializer doesn't delete its callback. */
	};

	virtual ~XMLSerializer();
	/**< Destructor. */

	/** Serialization configuration.  Most options only apply when
	    serializing to a string, but the fields 'xml_version',
	    'xml_standalone' and the doctype information  */
	class Configuration
	{
	public:
		Configuration();
		/**< Constructor that sets the following default values:

		       document_information:            0
		       split_cdata_sections:            TRUE
		       normalize_namespaces:            TRUE
		       discard_default_content:         TRUE
		       format_pretty_print:             FALSE
			   preferred_line_length:           80
		       use_decimal_character_reference: FALSE
		       add_xml_space_attributes:        FALSE
		       encoding:                        0 */

		const XMLDocumentInformation *document_information;
		/**< Information to use when generating an XML declaration
		     and/or a document type declaration.  Such declarations
		     will only be generated if they are present in the
		     XMLDocumentInformation object. */

		BOOL split_cdata_sections;
		/**< If TRUE, and 'encoding' is non-NULL, CDATA sections
		     containing characters that cannot be represented in that
		     encoding will be split at those characters, with the
		     characters espaced using character references.  If FALSE,
		     and 'encoding' is non-NULL, such CDATA sections will
		     cause the serialization to fail. */

		BOOL normalize_namespaces;
		/**< If TRUE, namespace declarations and prefixes will be
		     added or modified as specified in Appendix B.1,
		     "Namespace normalization" in DOM 3 Core.  If FALSE, the
		     result may not be namespace well-formed. */

		BOOL discard_default_content;
		/**< If TRUE, attributes that were not specified but got there
		     value from a attribute default value declaration will not
		     be serialized.  If FALSE, all attributes will be
		     serialized. */

		BOOL format_pretty_print;
		/**< If TRUE, the result will be pretty printed by adding
		     whitespace character data, including in places where the
		     whitespace is not ignorable.  This may affect the
		     validity. */

		unsigned preferred_line_length;
		/**< If 'format_pretty_print' is TRUE, lines longer than this
		     may be broken.  Note: this is not a hard limit, lines may
		     be much longer than this. */

		BOOL use_decimal_character_reference;
		/**< If TRUE, use decimal (&#dddd;) character references
		     instead of hexadecimal (&#xhhhh;). */

		BOOL add_xml_space_attributes;
		/**< If TRUE, add xml:space attributes as needed. */

		const char *encoding;
		/**< If non-NULL, any characters that are not representable in
		     the named encoding will be escaped using character
		     references. */
	};

	virtual OP_STATUS SetConfiguration(const Configuration &configuration) = 0;
	/**< Set serialization configuration options.  Strings specified
	     in the configuration will be copied by this function, as will
	     the XMLDocumentInformation object if present.

	     @param configuration The new configuration.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	enum CanonicalizeVersion
	{
		CANONICALIZE_1_0,
		CANONICALIZE_1_0_EXCLUSIVE
	};

	virtual OP_STATUS SetCanonicalize(CanonicalizeVersion version, BOOL with_comments = FALSE) = 0;
	/**< Tell this serializer to output canonicalized XML as specified
	     by "Canonical XML".  In part, this function alters
	     configuration parameters set by SetConfiguration(), so
	     calling SetConfiguration() after calling this function is not
	     recommended; the serialization result may in fact not be
	     canonical.

	     "Canonical XML" is a XML source code encoded in UTF-8.  So
	     obviously, this setting does not apply to serializers not
	     serializing to text.  Also, while the serializer will be
	     configured to assume UTF-8 encoding of the end result, it
	     will still output UTF-16, so for a fully canonical result,
	     the caller needs to convert the result to UTF-8.

	     Enabled by importing API_XMLUTILS_CANONICAL_XML.

	     @param version Specification version.
	     @param with_comments If TRUE, comments are included in the
	                          result, otherwise they are removed.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS SetInclusiveNamespacesPrefixList(const uni_char *list) = 0;
	/**< Set a list of namespace prefixes that are output when
	     inherited from an ancestor of the serialized element even if
	     CANONICALIZE_1_0_EXCLUSIVE is used.  Other namespace prefixes
	     are only declared in the serialization output if they are
	     declared within the serialized fragment and visibly used
	     within it.

	     The parameter is a space-separated list of prefixes.  The
	     special prefix "#default" refers to the the default
	     namespace.

	     This function can only be used after SetCanonicalize() has
	     been called with version==CANONICALIZE_1_0_EXCLUSIVE.

	     @param list List of namespace prefixes.

	     @return OpStatus::OK, OpStatus::ERR if not appropriate, and
	             OpStatus::ERR_NO_MEMORY on OOM. */
#endif // XMLUTILS_CANONICAL_XML_SUPPORT

	virtual OP_STATUS Serialize(MessageHandler *mh, const URL &url, HTML_Element *root, HTML_Element *target, Callback *callback) = 0;
	/**< "Serialize" a tree of elements.  The element 'root' and all
	     its descendants will be serialized.  The element 'target' can
	     be NULL.  If it is not, it must be a descendant of 'root' and
	     should be the root of the tree fragment that is really being
	     used.  Its ancestors will be serialized for context
	     (namespace declarations, xml:lang, xml:base and so on) only.
	     The target can for instance be specified by a fragment
	     identifier on the URI reference used.  If 'target' is NULL,
	     'root' is considered to be the target element.

	     When the serialization has finished, the callback is called
	     (unless 'callback' is NULL) with the status.  This may happen
	     immediately (during the call to Serialize()), but may also
	     happen at a later time, if the serialization was not
	     completed immediately.

	     @param mh Message handler.  Can be NULL if the serializer is
	               guaranteed to finish immediately, like the
	               serializer created by MakeToStringSerializer().
	     @param url URL to pretend the serialized content originated
	                from.  Typically used to resolve relative URLs in
	                the serialized content.
	     @param fragment Fragment to serialize.
	     @param callback Callback or NULL.

	     @return OpStatus::OK, OpStatus::ERR on failure and
	             OpStatus::ERR_NO_MEMORY on OOM. */

#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT
	virtual OP_STATUS Serialize(MessageHandler *mh, const URL &url, XMLFragment *fragment, Callback *callback) = 0;
	/**< "Serialize" an XML fragment.  All content in the fragment
	     will be serialized.  Since fragments don't keep track of
	     document type declarations, the document type declaration
	     will always be output right after the XML declaration if one
	     is configured to be output at all.

	     All attribute in the fragment will be considered specified,
	     and thus output even if the serializer has been configured to
	     discard default content.  All character data is output as
	     text, never as CDATA sections.

	     When the serialization has finished, the callback is called
	     (unless 'callback' is NULL) with the status.  This may happen
	     immediately (during the call to Serialize()), but may also
	     happen at a later time, if the serialization was not
	     completed immediately.

	     @param mh Message handler.  Can be NULL if the serializer is
	               guaranteed to finish immediately, like the
	               serializer created by MakeToStringSerializer().
	     @param url URL to pretend the serialized content originated
	                from.  Typically used to resolve relative URLs in
	                the serialized content.
	     @param fragment Fragment to serialize.
	     @param callback Callback or NULL.

	     @return OpStatus::OK, OpStatus::ERR on failure and
	             OpStatus::ERR_NO_MEMORY on OOM.*/
#endif // XMLUTILS_XMLFRAGMENT_SUPPORT

	enum Error
	{
		ERROR_NONE,
		/**< No error. */

		ERROR_INVALID_QNAME_OR_NCNAME,
		/**< Signalled if a QName or NCName was not well-formed. */

		ERROR_INVALID_CHARACTER_IN_CDATA,
		/**< Signalled if Configuration::split_cdata_sections was set
		     to FALSE and a CDATA section contained the string "]]>"
		     or characters that could not be represented in the
		     requested encoding. */

		ERROR_INVALID_CHARACTER_IN_NAME,
		/**< Signalled if a Name (or QName/NCName) contained an
		     character that cannot be represented in the requested
		     encoding. */

		ERROR_INVALID_CHARACTER_IN_PROCESSING_INSTRUCTION,
		/**< Signalled if a processing instruction's data contained
		     the string "?>" or characters that were not representable
		     in the requested encoding. */

		ERROR_OOM,
		/**< Signalled for out of memory errors. */

		ERROR_OTHER
		/**< Signalled for all other errors. */
	};

	virtual Error GetError() = 0;
	/**< If serialization failed (Serialize() return OpStatus::ERR
	     and/or the callback was called with Callback::STATUS_FAILED,)
	     this function returns an error code describing what went
	     wrong.  If no error has occured (if serialization has not
	     been attempted, has not finished or finished successfully)
	     ERROR_NONE is returned. */

#ifdef XMLUTILS_XMLTOSTRINGSERIALIZER_SUPPORT
	static OP_STATUS MakeToStringSerializer(XMLSerializer *&serializer, TempBuffer *buffer);
	/**< Creates an XMLSerializer that serializes to a unicode string.
	     The specified encoding, if one is specified through
	     SetConfiguration(), will not be used to actually encode the
	     result, but rather to keep track of characters that can't be
	     represent (which are replaced with character references in
	     character data and attribute values, with question marks in
	     comments and signalled as errors if they occur elsewhere.)

	     Serialization using the created XMLSerializer will always
	     finish or fail immediately, thus no MessageHandler needs to
	     be passed to the Serializer() functions.

	     @param serializer Set to the created object.
	     @param buffer Buffer into which the result is written.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
#endif // XMLUTILS_XMLTOSTRINGSERIALIZER_SUPPORT
};

#endif // XMLUTILS_XMLSERIALIZER_SUPPORT
#endif // XMLSERIALIZER_H
