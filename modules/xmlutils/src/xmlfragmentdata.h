/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLFRAGMENTDATA_H
#define XMLFRAGMENTDATA_H

#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT

#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/util/opstring.h"
#include "modules/url/url2.h"

class XMLFragmentData
{
public:
	class Element;
	class Text;
#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	class Comment;
	class ProcessingInstruction;
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	class Content
	{
	public:
		Content(XMLFragment::ContentType type)
			: type(type),
			  parent(NULL)
		{
		}

		XMLFragment::ContentType type;
		Element *parent;

		static void Delete(Element *element) { OP_DELETE(element); }
		static void Delete(Text *text) { OP_DELETE(text); }

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
		static void Delete(Comment *comment) { OP_DELETE(comment); }
		static void Delete(ProcessingInstruction *pi) { OP_DELETE(pi); }
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

		static void Delete(Content *content)
		{
			if (content)
				switch (content->type)
				{
				case XMLFragment::CONTENT_ELEMENT:
					Delete((Element *) content);
					break;

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
				case XMLFragment::CONTENT_COMMENT:
					Delete((Comment *) content);
					break;

				case XMLFragment::CONTENT_PROCESSING_INSTRUCTION:
					Delete((ProcessingInstruction *) content);
					break;
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

				default:
					Delete((Text *) content);
				}
		}
	};

	class Element
		: public Content
	{
	public:
		class Attribute
		{
		public:
			Attribute()
				: id(FALSE)
			{
			}

			XMLCompleteName name;
			OpString value;
			BOOL id;
		};

		Element()
			: Content(XMLFragment::CONTENT_ELEMENT),
			  attributes(NULL),
			  children(NULL),
			  attribute_index(0),
			  wshandling(XMLWHITESPACEHANDLING_DEFAULT)
		{
		}

		~Element();

		XMLCompleteName name;
		void **attributes;
		XMLNamespaceDeclaration::Reference declaration;
		void **children;
		unsigned attribute_index;
		XMLWhitespaceHandling wshandling;

		OP_STATUS AddAttribute(Attribute *attribute);
		OP_STATUS AddChild(Content *item, unsigned at = ~0u);
		Content *GetLastItem();
	};

	class Text
		: public Content
	{
	public:
		Text()
			: Content(XMLFragment::CONTENT_TEXT)
		{
		}

		OpString data;
	};

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	class Comment
		: public Content
	{
	public:
		Comment()
			: Content(XMLFragment::CONTENT_COMMENT)
		{
		}

		OpString data;
	};

	class ProcessingInstruction
		: public Content
	{
	public:
		ProcessingInstruction()
			: Content(XMLFragment::CONTENT_PROCESSING_INSTRUCTION)
		{
		}

		OpString target, data;
	};
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	XMLFragmentData();
	~XMLFragmentData();

	XMLVersion version;
	URL url;
	XMLDocumentInformation documentinfo;

	Element *root, *current;
	unsigned *position, depth, max_depth;
	XMLFragment::GetXMLOptions::Scope scope;
};

#endif // XMLUTILS_XMLFRAGMENT_SUPPORT
#endif // XMLFRAGMENTDATA_H
