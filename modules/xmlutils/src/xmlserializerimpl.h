/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLSERIALIZERIMPL_H
#define XMLSERIALIZERIMPL_H

#ifdef XMLUTILS_XMLSERIALIZER_SUPPORT

#define XMLUTILS_PRETTY_PRINTING_SUPPORT

#include "modules/xmlutils/xmlserializer.h"
#include "modules/xmlutils/xmllanguageparser.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/url/url2.h"

class OutputConverter;
class XMLFragment;

class XMLSerializerBackend
{
protected:
	TempBuffer buffer;
	/**< Content of CONTENT_TEXT or CONTENT_CDATA_SECTION, if the content was
	     not contained in a single block of memory.  Use data.content instead,
	     it points at this buffers content if necessary. */
	HTML_AttrIterator attributes;
	/**< Used for attribute iteration on elements. */

public:
	enum ContentType
	{
		CONTENT_ROOT,
		CONTENT_ELEMENT,
		CONTENT_ELEMENT_TARGET,
		CONTENT_TEXT,
		CONTENT_CDATA_SECTION,
		CONTENT_COMMENT,
		CONTENT_PROCESSING_INSTRUCTION,
		CONTENT_DOCUMENT_TYPE
	};

	enum StepType
	{
		STEP_REPEAT,
		/* Go nowhere, but reset attribute iteration and such. */

		STEP_FIRST_CHILD,
		STEP_NEXT_SIBLING,
		STEP_PARENT
	};

	XMLSerializerBackend();
	virtual ~XMLSerializerBackend();

	virtual BOOL StepL(StepType where, BOOL only_type = FALSE) = 0;
	/**< May LEAVE on OOM. */

	virtual BOOL NextAttributeL(XMLCompleteName &name, const uni_char *&value, BOOL &specified, BOOL &id) = 0;
	/**< May LEAVE on OOM. */

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	virtual OP_STATUS GetNamespacesInScope(XMLNamespaceDeclaration::Reference &nsscope) = 0;
#endif // XMLUTILS_CANONICAL_XML_SUPPORT

	ContentType type;
	/**< Type of last element. */
	XMLCompleteName name;
	/**< Name, if last type was CONTENT_ELEMENT or CONTENT_ELEMENT_TARGET. */
	XMLWhitespaceHandling wshandling;
	/**< Requested whitespace handling, if the last type was
	     CONTENT_ELEMENT or CONTENT_ELEMENT_TARGET. */
	const uni_char *content;
	/**< Content, if last type was CONTENT_TEXT, CONTENT_CDATA_SECTION,
	     CONTENT_COMMENT or CONTENT_PROCESSING_INSTRUCTION. */
	const uni_char *target;
	/**< Target, if last type was CONTENT_PROCESSING_INSTRUCTION. */
	BOOL is_html;
	/**< TRUE if type was CONTENT_ELEMENT or CONTENT_ELEMENT_TARGET
	     and the element was an HTML element. */
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	BOOL before_root_element;
	/**< TRUE if the current element is a sibling of the root element,
	     and precedes it in document order. */
	BOOL after_root_element;
	/**< TRUE if the current element is a sibling of the root element,
	     and succeeds it in document order. */
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
};

class XMLElementSerializerBackend
	: public XMLSerializerBackend
{
protected:
	HTML_Element *root_element, *current_element, *target_element;
	BOOL pretend_root;

public:
	XMLElementSerializerBackend(HTML_Element *root, HTML_Element *target);

	virtual BOOL StepL(StepType where, BOOL only_type);
	virtual BOOL NextAttributeL(XMLCompleteName &name, const uni_char *&value, BOOL &specified, BOOL &id);
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	virtual OP_STATUS GetNamespacesInScope(XMLNamespaceDeclaration::Reference &nsscope);
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
};

#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT
#include "modules/xmlutils/src/xmlfragmentdata.h"

class XMLFragmentSerializerBackend
	: public XMLSerializerBackend
{
protected:
	XMLFragmentData *data;
	unsigned *position, attribute;
	int depth;

	void Setup(XMLFragmentData::Element *&element, XMLFragmentData::Content *&current);

public:
	XMLFragmentSerializerBackend(XMLFragment *fragment);
	~XMLFragmentSerializerBackend();

	OP_STATUS Construct();

	virtual BOOL StepL(StepType where, BOOL only_type);
	virtual BOOL NextAttributeL(XMLCompleteName &name, const uni_char *&value, BOOL &specified, BOOL &id);
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	virtual OP_STATUS GetNamespacesInScope(XMLNamespaceDeclaration::Reference &nsscope);
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
};

#endif // XMLUTILS_XMLFRAGMENT_SUPPORT
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT

class XMLCanonicalizeSerializerBackend
	: public XMLSerializerBackend
{
protected:
	XMLSerializerBackend *backend;
	BOOL with_comments;

	class Attribute
		: public Link
	{
	public:
		XMLCompleteName name;
		OpString value;
		BOOL specified;
		BOOL id;
	};

	Head attributes;
	Attribute *next_attribute;

	static BOOL Less(const XMLCompleteName &name1, const XMLCompleteName &name2);

public:
	XMLCanonicalizeSerializerBackend(XMLSerializerBackend *backend, BOOL with_comments);
	~XMLCanonicalizeSerializerBackend();

	virtual BOOL StepL(StepType where, BOOL only_type);
	virtual BOOL NextAttributeL(XMLCompleteName &name, const uni_char *&value, BOOL &specified, BOOL &id);
	virtual OP_STATUS GetNamespacesInScope(XMLNamespaceDeclaration::Reference &nsscope);

	BOOL IsVisiblyUsing(const XMLNamespaceDeclaration *nsdeclaration);
};

#endif // XMLUTILS_CANONICAL_XML_SUPPORT
#ifdef XMLUTILS_XMLTOLANGUAGEPARSERSERIALIZER_SUPPORT

class XMLToLanguageParserSerializer
	: public XMLSerializer,
	  public XMLLanguageParser::SourceCallback,
	  public MessageObject
{
protected:
	XMLLanguageParser *parser;
	XMLSerializerBackend *backend;
	XMLDocumentInformation documentinfo;
	MessageHandler *mh;
	BOOL has_set_callback;
	URL url;
	Callback *callback;
	BOOL block, finished;

	void DoSerializeL(BOOL resume_after_continue);
	OP_STATUS DoSerialize(BOOL resume_after_continue);

public:
	XMLToLanguageParserSerializer(XMLLanguageParser *parser);
	virtual ~XMLToLanguageParserSerializer();
	virtual OP_STATUS SetConfiguration(const Configuration &configuration);
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	virtual OP_STATUS SetCanonicalize(CanonicalizeVersion version, BOOL with_comments);
	virtual OP_STATUS SetInclusiveNamespacesPrefixList(const uni_char *list);
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
	virtual OP_STATUS Serialize(MessageHandler *mh, const URL &url, HTML_Element *root, HTML_Element *target, Callback *callback);
#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT
	virtual OP_STATUS Serialize(MessageHandler *mh, const URL &url, XMLFragment *fragment, Callback *callback);
#endif // XMLUTILS_XMLFRAGMENT_SUPPORT
	virtual Error GetError();
	virtual void Continue(Status status);
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
};

#endif // XMLUTILS_XMLTOLANGUAGEPARSERSERIALIZER_SUPPORT

#ifdef XMLUTILS_XMLTOSTRINGSERIALIZER_SUPPORT

class XMLToStringSerializer
	: public XMLSerializer
{
protected:
	TempBuffer *buffer;
	XMLNamespaceNormalizer nsnormalizer;
	XMLDocumentInformation documentinfo;
	XMLWhitespaceHandling current_wshandling;
	BOOL split_cdata_sections;
	BOOL normalize_namespaces;
	BOOL discard_default_content;
	BOOL format_pretty_print, format_pretty_print_suspended;
	unsigned indentlevel, indentextra, linelength, preferredlinelength, level;
	BOOL add_xml_space_attributes;
	const char *charref_format;
	char *encoding;
	OutputConverter *converter;
	char *converter_buffer;
	unsigned converter_buffer_length;
	Error error;
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	BOOL canonicalize, canonicalize_with_comments;
	CanonicalizeVersion canonicalize_version;
	XMLNamespaceDeclaration::Reference nsstack;
	OpString inclusive_namespace_prefix_list;

	BOOL IncludeNSDeclaration(XMLCanonicalizeSerializerBackend *backend, XMLNamespaceDeclaration *nsdeclaration);
#endif // XMLUTILS_CANONICAL_XML_SUPPORT

	void AppendL(const char *text, BOOL first = FALSE, BOOL last = FALSE);
	void AppendL(const uni_char *text, unsigned length = ~0u);
#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
	void AppendIndentationL();
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT
	void AppendAttributeValueL(const uni_char *value);
	void AppendAttributeSpecificationL(const uni_char *prefix, const uni_char *localpart, const uni_char *value);
	void AppendAttributeSpecificationL(XMLNamespaceDeclaration *nsdeclaration);
	void AppendEncodedL(const char *prefix, const uni_char *&value, unsigned &value_length, const char *suffix);
	void AppendEscapedL(unsigned ch);
	void AppendCharacterDataL(const uni_char *value, unsigned value_length, BOOL is_cdata_section, BOOL is_comment, BOOL is_attrvalue);
	void AppendNameL(const uni_char *value, BOOL is_qname);
	void AppendCorrectlyQuotedL(const uni_char *string);
	void AppendDoctypeDeclarationL();
	void DoSerializeL(XMLSerializerBackend *backend);
	OP_STATUS DoSerialize(XMLSerializerBackend *backend);

public:
	XMLToStringSerializer(TempBuffer *buffer);
	virtual ~XMLToStringSerializer();
	virtual OP_STATUS SetConfiguration(const Configuration &configuration);
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	virtual OP_STATUS SetCanonicalize(CanonicalizeVersion version, BOOL with_comments);
	virtual OP_STATUS SetInclusiveNamespacesPrefixList(const uni_char *list);
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
	virtual OP_STATUS Serialize(MessageHandler *mh, const URL &url, HTML_Element *root, HTML_Element *target, Callback *callback);
#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT
	virtual OP_STATUS Serialize(MessageHandler *mh, const URL &url, XMLFragment *fragment, Callback *callback);
#endif // XMLUTILS_XMLFRAGMENT_SUPPORT
	virtual Error GetError();
};

#endif // XMLUTILS_XMLTOLANGUAGEPARSERSERIALIZER_SUPPORT
#endif // XMLUTILS_XMLSERIALIZER_SUPPORT
#endif // XMLSERIALIZERIMPL_H
