/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT

#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/src/xmlfragmentdata.h"
#include "modules/xmlutils/xmllanguageparser.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmlserializer.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlutils/xmltreeaccessor.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/url/url2.h"
#include "modules/formats/base64_decode.h"
#include "modules/util/tempbuf.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/adt/bytebuffer.h"

/* Finds the next free index in the array (which is a pointer to an
   array of pointers.)  The length of the array is always 2^n, n >= 3.
   The last used element is always NULL, so there can be at most
   length - 1 actual elements in the array.  When the NULL is found at
   an index that is 2^n - 1, the array is grown by allocating a twice
   as large array.  Upon return, the element at index + 1 is set to
   NULL, so that the caller only need insert one element at the
   returned index. */
static OP_STATUS
FindNextIndex(void ***array, unsigned &index)
{
	void **local = *array;
	index = 0;

	if (!local)
	{
		local = OP_NEWA(void *, 8);

		if (!local)
			return OpStatus::ERR_NO_MEMORY;

		*array = local;
	}
	else
	{
		unsigned limit = 8;

		while (local[index])
			if (++index == limit)
				limit += limit;

		if (index == limit - 1)
		{
			local = OP_NEWA(void *, limit + limit);

			if (!local)
				return OpStatus::ERR_NO_MEMORY;

			op_memcpy(local, *array, index * sizeof local[0]);
			OP_DELETEA(*array);

			*array = local;
		}
	}

	local[index + 1] = NULL;
	return OpStatus::OK;
}

XMLFragmentData::Element::~Element()
{
	if (attributes)
	{
		for (void **attribute0 = attributes; *attribute0; ++attribute0)
		{
			Attribute *attribute = static_cast<Attribute *>(*attribute0);
			OP_DELETE(attribute);
		}
		OP_DELETEA(attributes);
	}
	if (children)
	{
		for (void **child0 = children; *child0; ++child0)
			Delete(static_cast<Content *>(*child0));
		OP_DELETEA(children);
	}
}

OP_STATUS
XMLFragmentData::Element::AddAttribute(Attribute *attribute)
{
	unsigned index;

	if (OpStatus::IsMemoryError(FindNextIndex(&attributes, index)))
	{
		OP_DELETE(attribute);
		return OpStatus::ERR_NO_MEMORY;
	}

	attributes[index] = attribute;
	return OpStatus::OK;
}

OP_STATUS
XMLFragmentData::Element::AddChild(Content *item, unsigned at)
{
	unsigned index;

	if (OpStatus::IsMemoryError(FindNextIndex(&children, index)))
	{
		Delete(item);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (at != ~0u && at < index)
	{
		op_memmove(children + at + 1, children + at, sizeof children[0] * (index - at));
		children[at] = item;
	}
	else
		children[index] = item;

	item->parent = this;
	return OpStatus::OK;
}

XMLFragmentData::Content *
XMLFragmentData::Element::GetLastItem()
{
	if (children)
	{
		void **child = children;

		while (*child)
			++child;

		return (Content *) *--child;
	}

	return NULL;
}

XMLFragmentData::XMLFragmentData()
	: version(XMLVERSION_1_0),
	  root(NULL),
	  current(NULL),
	  position(NULL),
	  depth(1),
	  max_depth(1),
	  scope(XMLFragment::GetXMLOptions::SCOPE_WHOLE_FRAGMENT)
{
}

XMLFragmentData::~XMLFragmentData()
{
	OP_DELETE(root);
	OP_DELETEA(position);
}

static OP_STATUS
XMLFragment_AppendOrInsertNormalized(OpString &string, const uni_char *value, unsigned value_length, BOOL append = TRUE)
{
	unsigned length = 0;
	const uni_char *ptr = value, *ptr_end = value + value_length, *start;
	BOOL add_space = FALSE, add_trailing_space = FALSE;

	while (ptr != ptr_end && XMLUtils::IsSpace(*ptr))
		++ptr;

	start = ptr;

	if (append && start != value && !string.IsEmpty())
		add_space = TRUE;

	BOOL count_space = add_space;

	while (ptr != ptr_end)
	{
		if (count_space)
			++length;

		while (ptr != ptr_end && !XMLUtils::IsSpace(*ptr))
			++length, ++ptr;

		if (ptr != ptr_end)
		{
			add_trailing_space = !append;

			while (ptr != ptr_end && XMLUtils::IsSpace(*ptr))
				++ptr;

			count_space = TRUE;
		}
	}

	if (length != 0)
	{
		if (add_trailing_space)
			++length;

		unsigned existing = string.Length();
		uni_char *storage = string.Reserve(existing + length + 1);

		if (!storage)
			return OpStatus::ERR_NO_MEMORY;

		if (append)
			storage += existing;
		else
			op_memmove(storage + length, storage, existing * sizeof storage[0]);

		ptr = start;

		while (ptr != ptr_end)
		{
			if (add_space)
				*(storage++) = ' ';

			while (ptr != ptr_end && !XMLUtils::IsSpace(*ptr))
				*(storage++) = *(ptr++);

			while (ptr != ptr_end && XMLUtils::IsSpace(*ptr))
				++ptr;

			add_space = TRUE;
		}

		if (add_trailing_space)
			*(storage++) = ' ';

		*storage = 0;
	}

	return OpStatus::OK;
}

static OP_STATUS
XMLFragment_AddText(XMLFragmentData::Text *text, const uni_char *value, unsigned value_length, XMLWhitespaceHandling wshandling, BOOL append = TRUE)
{
	OpString &string = text->data;

	if (wshandling == XMLWHITESPACEHANDLING_PRESERVE)
		if (append)
			return string.Append(value, value_length);
		else
			return string.Insert(0, value, value_length);
	else
		return XMLFragment_AppendOrInsertNormalized(string, value, value_length, append);
}

static OP_STATUS
XMLFragment_SetAttribute(XMLFragmentData::Element::Attribute *attribute, const uni_char *value, unsigned value_length, XMLWhitespaceHandling wshandling)
{
	OpString &string = attribute->value;

	if (wshandling == XMLWHITESPACEHANDLING_PRESERVE)
		return string.Set(value, value_length);
	else
	{
		string.Empty();
		return XMLFragment_AppendOrInsertNormalized(string, value, value_length);
	}
}

class XMLFragmentImpl
	: public XMLLanguageParser
{
protected:
	XMLFragmentData *data;
	XMLWhitespaceHandling default_wshandling;
#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	BOOL store_comments_and_pis;
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

public:
#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	XMLFragmentImpl(XMLFragmentData *data, XMLWhitespaceHandling default_wshandling, BOOL store_comments_and_pis)
		: data(data),
		  default_wshandling(default_wshandling),
		  store_comments_and_pis(store_comments_and_pis)
	{
	}
#else // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	XMLFragmentImpl(XMLFragmentData *data, XMLWhitespaceHandling default_wshandling)
		: data(data),
		  default_wshandling(default_wshandling)
	{
	}
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	virtual OP_STATUS StartEntity(const URL &url, const XMLDocumentInformation &documentinfo, BOOL entity_reference)
	{
		RETURN_IF_ERROR(HandleStartEntity(url, documentinfo, entity_reference));

		if (!entity_reference)
		{
			data->version = GetCurrentVersion();
			data->url = url;
			return data->documentinfo.Copy(documentinfo);
		}
		else
			return OpStatus::OK;
	}

	virtual OP_STATUS StartElement(const XMLCompleteNameN &completename, BOOL fragment_start, BOOL &ignore_element)
	{
		called = TRUE;

		RETURN_IF_ERROR(HandleStartElement());

		XMLFragmentData::Element *element = OP_NEW(XMLFragmentData::Element, ());

		if (!element || OpStatus::IsMemoryError(element->name.Set(completename)))
		{
			OP_DELETE(element);
			return OpStatus::ERR_NO_MEMORY;
		}

		RETURN_IF_ERROR(data->current->AddChild(element));

		data->current = element;

		if (++data->depth > data->max_depth)
			++data->max_depth;

		return OpStatus::OK;
	}

	virtual OP_STATUS AddAttribute(const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length, BOOL specified, BOOL id)
	{
		RETURN_IF_ERROR(HandleAttribute(completename, value, value_length));

		XMLFragmentData::Element::Attribute *attribute = OP_NEW(XMLFragmentData::Element::Attribute, ());

		if (!attribute || OpStatus::IsMemoryError(attribute->name.Set(completename)) || OpStatus::IsMemoryError(attribute->value.Set(value, value_length)))
		{
			OP_DELETE(attribute);
			return OpStatus::ERR_NO_MEMORY;
		}

		RETURN_IF_ERROR(data->current->AddAttribute(attribute));

		attribute->id = id;

		return OpStatus::OK;
	}

	virtual OP_STATUS StartContent()
	{
		data->current->declaration = GetCurrentNamespaceDeclaration();
		return OpStatus::OK;
	}

	virtual OP_STATUS AddCharacterData(CharacterDataType type, const uni_char *value, unsigned value_length)
	{
		called = TRUE;

		XMLWhitespaceHandling wshandling = GetCurrentWhitespaceHandling(default_wshandling);

		if (type == CHARACTERDATA_TEXT_WHITESPACE && wshandling == XMLWHITESPACEHANDLING_DEFAULT)
			return OpStatus::OK;

		XMLFragmentData::Content *item = data->current->GetLastItem();

		if (!item || item->type != XMLFragment::CONTENT_TEXT)
		{
			XMLFragmentData::Text *text = OP_NEW(XMLFragmentData::Text, ());

			if (!text || OpStatus::IsMemoryError(data->current->AddChild(text)))
				return OpStatus::ERR_NO_MEMORY;

			item = text;
		}

		return XMLFragment_AddText((XMLFragmentData::Text *) item, value, value_length, wshandling);
	}

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	virtual OP_STATUS AddProcessingInstruction(const uni_char *target, unsigned target_length, const uni_char *content, unsigned content_length)
	{
		called = TRUE;

		if (store_comments_and_pis)
		{
			XMLFragmentData::ProcessingInstruction *pi = OP_NEW(XMLFragmentData::ProcessingInstruction, ());

			if (!pi || OpStatus::IsMemoryError(pi->target.Set(target, target_length)) || OpStatus::IsMemoryError(pi->data.Set(content, content_length)))
			{
				OP_DELETE(pi);
				return OpStatus::ERR_NO_MEMORY;
			}

			return data->current->AddChild(pi);
		}
		else
			return OpStatus::OK;
	}

	virtual OP_STATUS AddComment(const uni_char *value, unsigned value_length)
	{
		called = TRUE;

		if (store_comments_and_pis)
		{
			XMLFragmentData::Comment *comment = OP_NEW(XMLFragmentData::Comment, ());

			if (!comment || OpStatus::IsMemoryError(comment->data.Set(value, value_length)))
			{
				OP_DELETE(comment);
				return OpStatus::ERR_NO_MEMORY;
			}

			return data->current->AddChild(comment);
		}
		else
			return OpStatus::OK;
	}
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	virtual OP_STATUS EndElement(BOOL &block, BOOL &finished)
	{
		called = TRUE;

		RETURN_IF_ERROR(HandleEndElement());

		data->current = data->current->parent;

		--data->depth;

		return OpStatus::OK;
	}

	virtual OP_STATUS EndEntity()
	{
		return HandleEndEntity();
	}

	BOOL called;
};

OP_STATUS
XMLFragment::Construct()
{
	if (!data)
	{
		XMLFragmentData *local = OP_NEW(XMLFragmentData, ());

		if (!local)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<XMLFragmentData> anchor(local);

		if (!(local->root = OP_NEW(XMLFragmentData::Element, ())))
			return OpStatus::ERR_NO_MEMORY;

		local->current = local->root;
		local->position = OP_NEWA(unsigned, 9);

		if (!local->position)
			return OpStatus::ERR_NO_MEMORY;

		local->depth = 0;
		local->max_depth = 8;
		local->position[0] = 0;

		data = local;
		anchor.release();
	}

	return OpStatus::OK;
}

XMLFragment::XMLFragment()
	: data(NULL),
	  default_wshandling(XMLWHITESPACEHANDLING_DEFAULT)
#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	, store_comments_and_pis(FALSE)
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
#ifdef XML_ERRORS
	, error_description(NULL)
#endif // XML_ERRORS
{
}

XMLFragment::~XMLFragment()
{
	OP_DELETE(data);
}

static OP_STATUS
#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
XMLFragment_StartParsing(XMLFragmentData *&data, XMLParser **parser, XMLTokenHandler **tokenhandler, XMLFragmentImpl *&impl, XMLWhitespaceHandling default_wshandling, BOOL store_comments_and_pis)
#else // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
XMLFragment_StartParsing(XMLFragmentData *&data, XMLParser **parser, XMLTokenHandler **tokenhandler, XMLFragmentImpl *&impl, XMLWhitespaceHandling default_wshandling)
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
{
	if (!(data = OP_NEW(XMLFragmentData, ())))
		return OpStatus::ERR_NO_MEMORY;

	if (!(data->current = data->root = OP_NEW(XMLFragmentData::Element, ())))
		return OpStatus::ERR_NO_MEMORY;

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	if (!(impl = OP_NEW(XMLFragmentImpl, (data, default_wshandling, store_comments_and_pis))))
		return OpStatus::ERR_NO_MEMORY;
#else // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	if (!(impl = OP_NEW(XMLFragmentImpl, (data, default_wshandling))))
		return OpStatus::ERR_NO_MEMORY;
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	URL url;

	if (parser && tokenhandler)
	{
		RETURN_IF_ERROR(XMLLanguageParser::MakeTokenHandler(*tokenhandler, impl, NULL));
		RETURN_IF_ERROR(XMLParser::Make(*parser, NULL, (MessageHandler *) NULL, *tokenhandler, url));

		XMLParser::Configuration configuration;

		configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
		configuration.parse_mode = XMLParser::PARSEMODE_FRAGMENT;

		(*parser)->SetConfiguration(configuration);
	}

	return OpStatus::OK;
}

static OP_STATUS
XMLFragment_ParseSome(XMLParser *parser, XMLFragmentImpl *impl, const uni_char *text, unsigned text_length, BOOL more, BOOL &stopped)
{
	while (!parser->IsFinished() && !parser->IsFailed() && (!more || text_length > 0))
	{
		impl->called = FALSE;

		unsigned consumed;

		RETURN_IF_ERROR(parser->Parse(text, text_length, more, &consumed));

		if (parser->IsPaused() && !impl->called)
		{
			/* Seems we have a nasty entity expansion (4096 tokens or
			   entity expansions and not a single call to our
			   callback.)  Better fail than hang forever.  If this
			   happens and you think it shouldn't have, do let me
			   know.  (jl@opera.com)*/
			OP_ASSERT(!"read the comment in the source code!");
			return OpStatus::ERR;
		}

		text += consumed;
		text_length -= consumed;
	}

	if (parser->IsOutOfMemory())
		return OpStatus::ERR_NO_MEMORY;
	else if (parser->IsFailed())
		return OpStatus::ERR;

	stopped = parser->IsFinished();

	// Seems the 'stopped' argument is pretty silly, huh?
	OP_ASSERT(stopped == !more);

	return OpStatus::OK;
}

static OP_STATUS
XMLFragment_ConvertAndParseSome(XMLParser *parser, XMLFragmentImpl *impl, InputConverter *&converter, const char *charset, const char *data, unsigned data_length, unsigned &data_consumed, BOOL more, BOOL &stopped)
{
	if (!converter)
	{
		if (!charset)
		{
			CharsetDetector detector;
			charset = detector.GetXMLEncoding(data, data_length);

			if (!charset)
				charset = "UTF-8";
		}

		RETURN_IF_ERROR(InputConverter::CreateCharConverter(charset, &converter));

		if (!converter)
			return OpStatus::ERR;
	}

	unsigned text_length = MAX(1024, data_length * sizeof(uni_char));

	uni_char *text = OP_NEWA(uni_char, text_length);
	if (!text)
		return OpStatus::ERR_NO_MEMORY;
	ANCHOR_ARRAY(uni_char, text);

	do
	{
		int read, written = converter->Convert(data + data_consumed, data_length - data_consumed, text, text_length * sizeof(uni_char), &read);

		data_consumed += read;

		RETURN_IF_ERROR(XMLFragment_ParseSome(parser, impl, text, written / sizeof(uni_char), more || data_consumed < data_length, stopped));
	}
	while (!stopped && data_consumed < data_length);

	return OpStatus::OK;
}

static OP_STATUS
XMLFragment_FinishParsing(XMLFragmentData *data)
{
	if (!(data->position = OP_NEWA(unsigned, data->max_depth)))
		return OpStatus::ERR_NO_MEMORY;

	data->depth = 0;
	--data->max_depth;
	data->position[0] = 0;

	return OpStatus::OK;
}

OP_STATUS
XMLFragment::Parse(const uni_char *text, unsigned text_length)
{
	OP_DELETE(data);
	data = NULL;

	if (text_length == ~0u)
		text_length = uni_strlen(text);

	XMLFragmentData *local = NULL;
	XMLParser *parser = NULL;
	XMLTokenHandler *tokenhandler = NULL;
	XMLFragmentImpl *impl = NULL;

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, &parser, &tokenhandler, impl, default_wshandling, store_comments_and_pis);
#else // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, &parser, &tokenhandler, impl, default_wshandling);
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	OpAutoPtr<XMLFragmentData> data_anchor(local);
	OpAutoPtr<XMLTokenHandler> tokenhandler_anchor(tokenhandler);
	OpAutoPtr<XMLParser> parser_anchor(parser);
	OpAutoPtr<XMLFragmentImpl> impl_anchor(impl);

	RETURN_IF_ERROR(status);

	BOOL stopped = FALSE;

	status = XMLFragment_ParseSome(parser, impl, text, text_length, FALSE, stopped);

	if (OpStatus::IsError(status))
	{
#ifdef XML_ERRORS
		if (parser->IsFailed())
		{
			const char *uri, *fragment_id;

			parser->GetErrorDescription(error_description, uri, fragment_id);
			error_location = parser->GetErrorPosition();
		}
#endif // XML_ERRORS

		return status;
	}

	OP_ASSERT(stopped);

	RETURN_IF_ERROR(XMLFragment_FinishParsing(local));

	data = data_anchor.release();
	return OpStatus::OK;
}

OP_STATUS
XMLFragment::Parse(ByteBuffer *buffer, const char *charset)
{
	OP_DELETE(data);
	data = NULL;

	XMLFragmentData *local = NULL;
	XMLParser *parser = NULL;
	XMLTokenHandler *tokenhandler = NULL;
	XMLFragmentImpl *impl = NULL;

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, &parser, &tokenhandler, impl, default_wshandling, store_comments_and_pis);
#else // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, &parser, &tokenhandler, impl, default_wshandling);
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	OpAutoPtr<XMLFragmentData> data_anchor(local);
	OpAutoPtr<XMLTokenHandler> tokenhandler_anchor(tokenhandler);
	OpAutoPtr<XMLParser> parser_anchor(parser);
	OpAutoPtr<XMLFragmentImpl> impl_anchor(impl);

	RETURN_IF_ERROR(status);

	InputConverter *converter = NULL;
	OpAutoPtr<InputConverter> converter_anchor;

	BOOL stopped = FALSE;
	const char *chunk = NULL;
	unsigned chunk_index = 0, chunk_count = buffer->GetChunkCount(), chunk_size = 0;

	while (!stopped && chunk_index < chunk_count)
	{
		if (!chunk)
			chunk = buffer->GetChunk(chunk_index, &chunk_size);

		if (chunk_size != 0)
		{
			unsigned consumed = 0;

			OP_STATUS status = XMLFragment_ConvertAndParseSome(parser, impl, converter, charset, chunk, chunk_size, consumed, chunk_index < chunk_count - 1, stopped);

#ifdef XML_ERRORS
			if (parser->IsFailed())
			{
				const char *uri, *fragment_id;

				parser->GetErrorDescription(error_description, uri, fragment_id);
				error_location = parser->GetErrorPosition();
			}
#endif // XML_ERRORS

			converter_anchor.reset(converter);

			RETURN_IF_ERROR(status);

			if (consumed == chunk_size)
			{
				chunk = 0;
				++chunk_index;
			}
			else
			{
				chunk += consumed;
				chunk_size -= consumed;
			}
		}
	}

	RETURN_IF_ERROR(XMLFragment_FinishParsing(local));

	data = data_anchor.release();
	return OpStatus::OK;

}

OP_STATUS
XMLFragment::Parse(OpFileDescriptor *fd, const char *charset)
{
	if (!fd || !fd->IsOpen())
	{
		OP_ASSERT(!"invalid file descriptor, see the documentation");
		return OpStatus::ERR;
	}

	OP_DELETE(data);
	data = NULL;

	XMLFragmentData *local = NULL;
	XMLParser *parser = NULL;
	XMLTokenHandler *tokenhandler = NULL;
	XMLFragmentImpl *impl = NULL;

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, &parser, &tokenhandler, impl, default_wshandling, store_comments_and_pis);
#else // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, &parser, &tokenhandler, impl, default_wshandling);
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	OpAutoPtr<XMLFragmentData> data_anchor(local);
	OpAutoPtr<XMLTokenHandler> tokenhandler_anchor(tokenhandler);
	OpAutoPtr<XMLParser> parser_anchor(parser);
	OpAutoPtr<XMLFragmentImpl> impl_anchor(impl);

	RETURN_IF_ERROR(status);

#undef BUFFERSIZE
#define BUFFERSIZE 4096

	char *buffer = OP_NEWA(char, BUFFERSIZE);
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;
	ANCHOR_ARRAY(char, buffer);

	InputConverter *converter = NULL;
	OpAutoPtr<InputConverter> converter_anchor;

	unsigned buffer_length = 0;
	BOOL stopped = FALSE;

	while (!stopped)
	{
		if (!fd->Eof())
		{
			OpFileLength bytes_read;

			RETURN_IF_ERROR(fd->Read(buffer + buffer_length, BUFFERSIZE - buffer_length, &bytes_read));

			buffer_length += (unsigned) bytes_read;
		}

		unsigned buffer_consumed = 0;

		OP_STATUS status = XMLFragment_ConvertAndParseSome(parser, impl, converter, charset, buffer, buffer_length, buffer_consumed, !fd->Eof(), stopped);

#ifdef XML_ERRORS
		if (parser->IsFailed())
		{
			const char *uri, *fragment_id;

			parser->GetErrorDescription(error_description, uri, fragment_id);
			error_location = parser->GetErrorPosition();
		}
#endif // XML_ERRORS

		converter_anchor.reset(converter);

		RETURN_IF_ERROR(status);

		if (buffer_consumed == buffer_length)
			buffer_length = 0;
		else
		{
			buffer_length -= buffer_consumed;
			op_memmove(buffer, buffer + buffer_consumed, buffer_length);
		}
	}

	RETURN_IF_ERROR(XMLFragment_FinishParsing(local));

	data = data_anchor.release();
	return OpStatus::OK;
}

OP_STATUS
XMLFragment_RetrieveData(URL_DataDescriptor *dd, BOOL &more)
{
	TRAPD(status, dd->RetrieveDataL(more));
	return status;
}

OP_STATUS
XMLFragment::Parse(URL url)
{
	URL_DataDescriptor *dd = url.GetDescriptor(NULL, TRUE, FALSE, TRUE, NULL, URL_XML_CONTENT);
	if (!dd)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoPtr<URL_DataDescriptor> dd_anchor(dd);

	OP_DELETE(data);
	data = NULL;

	XMLFragmentData *local = NULL;
	XMLParser *parser = NULL;
	XMLTokenHandler *tokenhandler = NULL;
	XMLFragmentImpl *impl = NULL;

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, &parser, &tokenhandler, impl, default_wshandling, store_comments_and_pis);
#else // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, &parser, &tokenhandler, impl, default_wshandling);
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	OpAutoPtr<XMLFragmentData> data_anchor(local);
	OpAutoPtr<XMLTokenHandler> tokenhandler_anchor(tokenhandler);
	OpAutoPtr<XMLParser> parser_anchor(parser);
	OpAutoPtr<XMLFragmentImpl> impl_anchor(impl);

	RETURN_IF_ERROR(status);

	BOOL stopped = FALSE;

	while (!stopped)
	{
		BOOL more;

		RETURN_IF_ERROR(XMLFragment_RetrieveData(dd, more));

		const uni_char *text = (const uni_char *) dd->GetBuffer();
		unsigned text_length = dd->GetBufSize() / sizeof text[0];

		if (more && text_length == 0)
		{
			/* Seems this URL is expecting more data but doesn't have it
			   available now.  Invalid use of this function! */
			OP_ASSERT(FALSE);
			more = FALSE;
		}

		OP_STATUS status = XMLFragment_ParseSome(parser, impl, text, text_length, more, stopped);

		if (OpStatus::IsError(status))
		{
#ifdef XML_ERRORS
			if (parser->IsFailed())
			{
				const char *uri, *fragment_id;

				parser->GetErrorDescription(error_description, uri, fragment_id);
				error_location = parser->GetErrorPosition();
			}
#endif // XML_ERRORS

			return status;
		}

		dd->ConsumeData(text_length * sizeof text[0]);
	}

	RETURN_IF_ERROR(XMLFragment_FinishParsing(local));

	data = data_anchor.release();
	return OpStatus::OK;
}

OP_STATUS
XMLFragment::Parse(HTML_Element *element)
{
	OP_DELETE(data);
	data = NULL;

	XMLFragmentData *local = NULL;
	XMLFragmentImpl *impl = NULL;

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, NULL, NULL, impl, default_wshandling, store_comments_and_pis);
#else // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
	OP_STATUS status = XMLFragment_StartParsing(local, NULL, NULL, impl, default_wshandling);
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

	OpAutoPtr<XMLFragmentData> data_anchor(local);
	OpAutoPtr<XMLFragmentImpl> impl_anchor(impl);

	RETURN_IF_ERROR(status);

	XMLSerializer *serializer;

	RETURN_IF_ERROR(XMLLanguageParser::MakeSerializer(serializer, impl));

	OpAutoPtr<XMLSerializer> serializer_anchor(serializer);
	URL url;

	RETURN_IF_ERROR(serializer->Serialize(NULL, url, element, NULL, NULL));
	RETURN_IF_ERROR(XMLFragment_FinishParsing(local));

	data = data_anchor.release();
	return OpStatus::OK;
}

void
XMLFragment::RestartFragment()
{
	data->current = data->root;
	data->position[0] = 0;
	data->depth = 0;
}

BOOL
XMLFragment::HasMoreContent()
{
	return data->current->children && data->current->children[data->position[data->depth]];
}

BOOL
XMLFragment::HasMoreElements()
{
	if (void **children = data->current->children)
		for (unsigned index = data->position[data->depth]; children[index]; ++index)
			if (((XMLFragmentData::Content *) children[index])->type == XMLFragment::CONTENT_ELEMENT)
				return TRUE;

	return FALSE;
}

BOOL
XMLFragment::HasCurrentElement()
{
	return data->current != data->root;
}

XMLFragment::ContentType
XMLFragment::GetNextContentType()
{
	if (HasMoreContent())
		return ((XMLFragmentData::Content *) data->current->children[data->position[data->depth]])->type;
	else if (data->current != data->root)
		return XMLFragment::CONTENT_END_OF_ELEMENT;
	else
		return XMLFragment::CONTENT_END_OF_FRAGMENT;
}

BOOL
XMLFragment::SkipContent()
{
	if (data->current->children)
	{
		unsigned index = data->position[data->depth];
		if (data->current->children[index])
		{
			data->position[data->depth] = ++index;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL
XMLFragment::EnterElement(const XMLExpandedName &name)
{
	if (void **children = data->current->children)
		for (unsigned index = data->position[data->depth]; children[index]; ++index)
			if (((XMLFragmentData::Content *) children[index])->type == XMLFragment::CONTENT_ELEMENT)
			{
				XMLFragmentData::Element *element = (XMLFragmentData::Element *) (XMLFragmentData::Content *) children[index];

				if (name != element->name)
					break;

				data->position[data->depth] = index;
				data->position[++data->depth] = 0;
				data->current = element;
				return TRUE;
			}

	return FALSE;
}

BOOL
XMLFragment::EnterAnyElement()
{
	if (void **children = data->current->children)
		for (unsigned index = data->position[data->depth]; children[index]; ++index)
			if (((XMLFragmentData::Content *) children[index])->type == XMLFragment::CONTENT_ELEMENT)
			{
				data->position[data->depth] = index;
				data->position[++data->depth] = 0;
				data->current = (XMLFragmentData::Element *) (XMLFragmentData::Content *) children[index];
				return TRUE;
			}

	return FALSE;
}

void
XMLFragment::RestartCurrentElement()
{
	data->position[data->depth] = data->current->attribute_index = 0;
}

const XMLCompleteName &
XMLFragment::GetElementName()
{
	return data->current->name;
}

XMLNamespaceDeclaration *
XMLFragment::GetNamespaceDeclaration()
{
	return const_cast<const XMLNamespaceDeclaration::Reference &>(data->current->declaration);
}

static OP_STATUS
XMLFragment_CloneContent(XMLFragmentData::Content *&clone, XMLFragmentData::Content *content)
{
	if (content->type == XMLFragment::CONTENT_ELEMENT)
	{
		XMLFragmentData::Element *old_element = (XMLFragmentData::Element *) content, *new_element = OP_NEW(XMLFragmentData::Element, ());

		if (!new_element)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<XMLFragmentData::Element> anchor(new_element);

		if (void **attribute = old_element->attributes)
			for (; *attribute; ++attribute)
			{
				XMLFragmentData::Element::Attribute *local = OP_NEW(XMLFragmentData::Element::Attribute, ());

				if (!local || OpStatus::IsMemoryError(local->name.Set(((XMLFragmentData::Element::Attribute *) *attribute)->name)) || OpStatus::IsMemoryError(local->value.Set(((XMLFragmentData::Element::Attribute *) *attribute)->value)))
				{
					OP_DELETE(local);
					return OpStatus::ERR_NO_MEMORY;
				}

				RETURN_IF_ERROR(new_element->AddAttribute(local));
			}

		if (void **child = old_element->children)
			for (; *child; ++child)
			{
				XMLFragmentData::Content *local;

				RETURN_IF_ERROR(XMLFragment_CloneContent(local, (XMLFragmentData::Content *) *child));
				RETURN_IF_ERROR(new_element->AddChild(local));
			}

		clone = new_element;
		anchor.release();

		return OpStatus::OK;
	}
	else
	{
		XMLFragmentData::Text *old_text = (XMLFragmentData::Text *) content, *new_text = OP_NEW(XMLFragmentData::Text, ());

		if (!new_text || OpStatus::IsMemoryError(new_text->data.Append(old_text->data.CStr())))
			return OpStatus::ERR_NO_MEMORY;

		clone = new_text;
		return OpStatus::OK;
	}
}

OP_STATUS
XMLFragment::MakeSubFragment(XMLFragment *&fragment)
{
	XMLFragment *local = OP_NEW(XMLFragment, ());

	if (!local)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<XMLFragment> anchor(local);

	XMLFragmentData *local_data = OP_NEW(XMLFragmentData, ());

	if (!local_data)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<XMLFragmentData> anchor_data(local_data);

	if (!(local_data->root = OP_NEW(XMLFragmentData::Element, ())))
		return OpStatus::ERR_NO_MEMORY;

	local_data->current = local_data->root;

	if (data->current->children)
		for (void **child = data->current->children + data->position[data->depth]; *child; ++child)
		{
			XMLFragmentData::Content *local;

			RETURN_IF_ERROR(XMLFragment_CloneContent(local, (XMLFragmentData::Content *) *child));
			RETURN_IF_ERROR(local_data->current->AddChild(local));
		}

	fragment = local;
	anchor.release();

	fragment->data = local_data;
	anchor_data.release();

	return OpStatus::OK;
}

const uni_char *
XMLFragment::GetAttribute(const XMLExpandedName &name)
{
	if (void **attribute = data->current->attributes)
		for (; *attribute; ++attribute)
		{
			if (name == ((XMLFragmentData::Element::Attribute *) *attribute)->name)
			{
				const uni_char *value = ((XMLFragmentData::Element::Attribute *) *attribute)->value.CStr();
				return value ? value : UNI_L("");
			}
		}

	return NULL;
}

const uni_char *
XMLFragment::GetAttributeFallback(const XMLExpandedName &name, const uni_char *fallback)
{
	const uni_char *value = GetAttribute(name);
	if (value)
		return value;
	else
		return fallback;
}

const uni_char *
XMLFragment::GetId()
{
	if (void **attribute = data->current->attributes)
		for (; *attribute; ++attribute)
			if (((XMLFragmentData::Element::Attribute *) *attribute)->id)
			{
				const uni_char *value = ((XMLFragmentData::Element::Attribute *) *attribute)->value.CStr();
				return value ? value : UNI_L("");
			}

	return NULL;
}

BOOL
XMLFragment::GetNextAttribute(XMLCompleteName &name, const uni_char *&value)
{
	if (void **attributes = data->current->attributes)
		if (XMLFragmentData::Element::Attribute *attribute = (XMLFragmentData::Element::Attribute *) attributes[data->current->attribute_index])
		{
			++data->current->attribute_index;

			name = attribute->name;
			value = attribute->value.CStr();

			if (!value)
				value = UNI_L("");

			return TRUE;
		}

	return FALSE;
}

static OP_STATUS
XMLFragment_GetAllText(TempBuffer &buffer, XMLFragmentData::Element *element)
{
	OP_STATUS status = OpStatus::OK;

	if (void **child = element->children)
		for (; *child && OpStatus::IsSuccess(status); ++child)
			if (((XMLFragmentData::Content *) *child)->type == XMLFragment::CONTENT_ELEMENT)
				status = XMLFragment_GetAllText(buffer, (XMLFragmentData::Element *) (XMLFragmentData::Content *) *child);
			else
				status = buffer.Append(((XMLFragmentData::Text *) (XMLFragmentData::Content *) *child)->data.CStr());

	return status;
}

OP_STATUS
XMLFragment::GetAllText(TempBuffer &buffer)
{
	return XMLFragment_GetAllText(buffer, data->current);
}

OP_STATUS
XMLFragment::GetAllText(TempBuffer &buffer, const XMLExpandedName &name)
{
	if (EnterElement(name))
	{
		OP_STATUS status = GetAllText(buffer);

		LeaveElement();

		return status;
	}
	else
		return OpStatus::OK;
}

void
XMLFragment::LeaveElement()
{
	if (data->depth != 0)
	{
		data->current = data->current->parent;
		++data->position[--data->depth];
	}
}

const uni_char *
XMLFragment::GetText()
{
	const uni_char *text = UNI_L("");

	if (void **children = data->current->children)
		if (XMLFragmentData::Content *child = (XMLFragmentData::Content *) children[data->position[data->depth]])
			if (child->type == XMLFragment::CONTENT_TEXT)
			{
				const uni_char *storage = ((XMLFragmentData::Text *) (XMLFragmentData::Content *) child)->data.CStr();
				if (storage)
					text = storage;
			}
	return text;
}

#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

const uni_char *
XMLFragment::GetComment()
{
	const uni_char *content = UNI_L("");

	if (void **children = data->current->children)
		if (XMLFragmentData::Content *child = (XMLFragmentData::Content *) children[data->position[data->depth]])
			if (child->type == XMLFragment::CONTENT_COMMENT)
			{
				const uni_char *storage = ((XMLFragmentData::Comment *) (XMLFragmentData::Content *) child)->data.CStr();
				if (storage)
					content = storage;
			}
	return content;
}

BOOL
XMLFragment::GetProcessingInstruction(const uni_char *&target, const uni_char *&content)
{
	if (void **children = data->current->children)
		if (XMLFragmentData::Content *child = (XMLFragmentData::Content *) children[data->position[data->depth]])
			if (child->type == XMLFragment::CONTENT_PROCESSING_INSTRUCTION)
			{
				target = ((XMLFragmentData::ProcessingInstruction *) (XMLFragmentData::Content *) child)->target.CStr();
				content = ((XMLFragmentData::ProcessingInstruction *) (XMLFragmentData::Content *) child)->data.CStr();
				if (!content)
					content = UNI_L("");
			}
	return FALSE;
}

#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT

OP_BOOLEAN
XMLFragment::GetBinaryData(const XMLExpandedName &name, char *&data, unsigned &length)
{
	if (EnterElement(name))
	{
		TempBuffer buffer;

		OP_STATUS status = GetAllText(buffer);

		LeaveElement();

		RETURN_IF_ERROR(status);

		OpString8 base64;

		/* Using UTF-8 here mostly to make sure it fails if the text
		   is not actually base64.  Just stripping the high bits in
		   each character might make it valid base64 when it's really
		   not. */
		RETURN_IF_ERROR(base64.SetUTF8FromUTF16(buffer.GetStorage()));

		char *base64ptr = base64.CStr(), *base64end = base64ptr + base64.Length();
		unsigned base64length = 0;

		while (base64ptr != base64end)
		{
			if (XMLUtils::IsSpace(*base64ptr))
				*base64ptr = 10;
			else
				++base64length;

			++base64ptr;
		}

		unsigned prelimlength = (base64length + 3) / 4 * 3;

		data = OP_NEWA(char, prelimlength);
		if (!data)
			return OpStatus::ERR_NO_MEMORY;

		unsigned long readpos;
		BOOL warning;

		length = GeneralDecodeBase64((const unsigned char *) base64.CStr(), base64.Length(), readpos, (unsigned char *) data, warning, prelimlength);

		if (warning)
		{
			OP_DELETEA(data);
			data = NULL;

			return OpStatus::ERR;
		}

		return OpBoolean::IS_TRUE;
	}
	else
		return OpBoolean::IS_FALSE;
}

OP_BOOLEAN
XMLFragment::GetBinaryData(const XMLExpandedName &name, ByteBuffer &buffer)
{
	/* Sub-optimal.  Should decode base64 content directly into the buffer. */

	char *data;
	unsigned length;

	OP_BOOLEAN result = GetBinaryData(name, data, length);
	if (result == OpBoolean::IS_TRUE)
	{
		OP_STATUS status = buffer.AppendBytes(data, length);

		OP_DELETEA(data);

		return OpStatus::IsSuccess(status) ? OpBoolean::IS_TRUE : status;
	}
	else
		return result;
}

OP_STATUS
XMLFragment::OpenElement(const XMLCompleteName &name)
{
	RETURN_IF_ERROR(Construct());

	if (data->depth == data->max_depth)
	{
		unsigned *new_position = OP_NEWA(unsigned, data->max_depth + data->max_depth + 1);

		if (!new_position)
			return OpStatus::ERR_NO_MEMORY;

		op_memcpy(new_position, data->position, sizeof new_position[0] * (data->max_depth + 1));

		OP_DELETEA(data->position);

		data->position = new_position;
		data->max_depth += data->max_depth;
	}

	XMLFragmentData::Element *element = OP_NEW(XMLFragmentData::Element, ());

	if (!element || OpStatus::IsMemoryError(element->name.Set(name)))
	{
		OP_DELETE(element);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(data->current->AddChild(element, data->position[data->depth]));

	/* Enters the new element. */
	EnterAnyElement();

	return OpStatus::OK;
}

OP_STATUS
XMLFragment::SetAttribute(const XMLCompleteName &name, const uni_char *value, XMLWhitespaceHandling wshandling)
{
	unsigned value_length = uni_strlen(value);

	if (void **attribute = data->current->attributes)
		for (; *attribute; ++attribute)
		{
			XMLFragmentData::Element::Attribute *attr = (XMLFragmentData::Element::Attribute *) *attribute;

			if (name == attr->name)
				return XMLFragment_SetAttribute(attr, value, value_length, wshandling);
		}

	XMLFragmentData::Element::Attribute *attribute = OP_NEW(XMLFragmentData::Element::Attribute, ());

	if (!attribute || OpStatus::IsMemoryError(attribute->name.Set(name)) || OpStatus::IsMemoryError(XMLFragment_SetAttribute(attribute, value, value_length, wshandling)))
	{
		OP_DELETE(attribute);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(data->current->AddAttribute(attribute));

	return OpStatus::OK;
}

OP_STATUS
XMLFragment::SetAttributeFormat(const XMLCompleteName &name, const uni_char *format, ...)
{
	va_list args;
	va_start(args, format);

	if (void **attribute = data->current->attributes)
		for (; *attribute; ++attribute)
		{
			XMLFragmentData::Element::Attribute *attr = (XMLFragmentData::Element::Attribute *) *attribute;

			if (name == attr->name)
			{
				attr->value.Empty();
				va_end(args);
				return attr->value.AppendVFormat(format, args);
			}
		}

	XMLFragmentData::Element::Attribute *attribute = OP_NEW(XMLFragmentData::Element::Attribute, ());

	if (!attribute || OpStatus::IsMemoryError(attribute->name.Set(name)) || OpStatus::IsMemoryError(attribute->value.AppendVFormat(format, args)))
	{
		OP_DELETE(attribute);
		va_end(args);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(data->current->AddAttribute(attribute));

	va_end(args);

	return OpStatus::OK;
}

void
XMLFragment::CloseElement()
{
	/* Exactly the same thing, incidently. */
	LeaveElement();
}

OP_STATUS
XMLFragment::AddText(const uni_char *value, unsigned text_length, XMLWhitespaceHandling wshandling)
{
	RETURN_IF_ERROR(Construct());

	if (XMLUtils::IsWhitespace(value) && wshandling == XMLWHITESPACEHANDLING_DEFAULT)
		return OpStatus::OK;

	XMLFragmentData::Text *text = NULL;
	BOOL append = TRUE;

	unsigned position = data->position[data->depth];

	if (void **children = data->current->children)
		if (position > 0 && ((XMLFragmentData::Content *) children[position - 1])->type == XMLFragment::CONTENT_TEXT)
			text = (XMLFragmentData::Text *) (XMLFragmentData::Content *) children[position - 1];
		else if (XMLFragmentData::Content *child = (XMLFragmentData::Content *) children[position])
			if (child->type == XMLFragment::CONTENT_TEXT)
			{
				text = (XMLFragmentData::Text *) child;
				append = FALSE;
				++data->position[data->depth];
			}

	if (!text)
	{
		text = OP_NEW(XMLFragmentData::Text, ());

		if (!text || OpStatus::IsMemoryError(data->current->AddChild(text, position)))
			return OpStatus::ERR_NO_MEMORY;

		++data->position[data->depth];
	}

	/* If one of the assertions below fails, you appear to be mixing
	   different whitespace handling for text within the same element.
	   There is no way to serialize the fragment that preserves your
	   whitespace handling. */

	if (wshandling == XMLWHITESPACEHANDLING_PRESERVE)
	{
		/* See above.  Note: this assertion is not perfect, it will
		   miss text with different whitespace handling interspersed
		   with elements. */
		OP_ASSERT(data->current->wshandling == XMLWHITESPACEHANDLING_PRESERVE || text->data.IsEmpty());

		data->current->wshandling = XMLWHITESPACEHANDLING_PRESERVE;
	}
	else
		OP_ASSERT(data->current->wshandling == XMLWHITESPACEHANDLING_DEFAULT);

	return XMLFragment_AddText(text, value, text_length == ~0u ? uni_strlen(value) : text_length, wshandling, append);
}

OP_STATUS
XMLFragment::AddText(const XMLExpandedName &name, const uni_char *text, unsigned text_length, XMLWhitespaceHandling wshandling)
{
	RETURN_IF_ERROR(OpenElement(name));
	RETURN_IF_ERROR(AddText(text, text_length, wshandling));
	CloseElement();

	return OpStatus::OK;
}

OP_STATUS
XMLFragment::AddBinaryData(const XMLExpandedName &name, const char *data, unsigned length)
{
	/* FIXME: we first encode the data into an 8 bit string, then copy
	   it into a 16 bit string, and then copy it again into the
	   final storage. */

	char *base64ptr = NULL;
	int base64length;

	MIME_Encode_Error error = MIME_Encode_SetStr(base64ptr, base64length, data, length, NULL, GEN_BASE64_ONELINE);

	if (error == MIME_FAILURE)
		return OpStatus::ERR_NO_MEMORY;

	OpString base64;

	OP_STATUS rc = base64.Set(base64ptr, base64length);
	OP_DELETEA(base64ptr);

	if ( OpStatus::IsMemoryError(rc) )
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(OpenElement(name));
	RETURN_IF_ERROR(AddText(base64.CStr()));

	CloseElement();

	return OpStatus::OK;
}

OP_STATUS
XMLFragment::AddBinaryData(const XMLExpandedName &name, const ByteBuffer &buffer)
{
	/* Sub-optimal.  Should encode base64 directly into the Text item. */

	char *data = buffer.Copy();

	if (!data)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = AddBinaryData(name, data, buffer.Length());

	OP_DELETEA(data);

	return status;
}

OP_STATUS
XMLFragment::AddFragment(XMLFragment *fragment)
{
	if (!fragment->data)
		return OpStatus::OK;

	RETURN_IF_ERROR(Construct());

	if (void **new_children = fragment->data->root->children)
		if (new_children[0])
		{
			if (data->depth + fragment->data->max_depth > data->max_depth)
			{
				unsigned new_max_depth = data->max_depth + data->max_depth;

				while (data->depth + fragment->data->max_depth > new_max_depth)
					new_max_depth += new_max_depth;

				unsigned *new_position = OP_NEWA(unsigned, new_max_depth + 1);

				if (!new_position)
					return OpStatus::ERR_NO_MEMORY;

				op_memcpy(new_position, data->position, sizeof new_position[0] * (data->max_depth + 1));

				OP_DELETEA(data->position);

				data->position = new_position;
				data->max_depth = new_max_depth;
			}

			void **children = data->current->children;
			XMLFragmentData::Content *before, *after;
			unsigned position = data->position[data->depth];

			if (children)
			{
				before = position == 0 ? NULL : (XMLFragmentData::Content *) children[position - 1];
				after = (XMLFragmentData::Content *) children[position];
			}
			else
				before = after = NULL;

			unsigned index = 0;

			if (before && before->type == XMLFragment::CONTENT_TEXT && ((XMLFragmentData::Content *) new_children[0])->type == XMLFragment::CONTENT_TEXT)
			{
				/* Merge text nodes. */

				OpString &data = ((XMLFragmentData::Text *) (XMLFragmentData::Content *) new_children[0])->data;

				RETURN_IF_ERROR(XMLFragment_AddText((XMLFragmentData::Text *) before, data.CStr(), data.Length(), XMLWHITESPACEHANDLING_PRESERVE));

				++index;
			}

			XMLFragmentData::Content *child;

			for (; (child = (XMLFragmentData::Content *) new_children[index]) != NULL; ++index)
			{
				new_children[index] = NULL;

				if (!new_children[index + 1] && after && after->type == XMLFragment::CONTENT_TEXT && child->type == XMLFragment::CONTENT_TEXT)
				{
					/* Merge text nodes. */

					OpString &data = ((XMLFragmentData::Text *) child)->data;

					RETURN_IF_ERROR(XMLFragment_AddText((XMLFragmentData::Text *) after, data.CStr(), data.Length(), XMLWHITESPACEHANDLING_PRESERVE, FALSE));

					XMLFragmentData::Content::Delete(child);

					break;
				}

				RETURN_IF_ERROR(data->current->AddChild(child, position++));
			}

			OP_DELETEA(fragment->data->root->children);
			fragment->data->root->children = NULL;

			data->position[data->depth] = position;
		}

	return OpStatus::OK;
}

OP_STATUS
XMLFragment::GetXML(TempBuffer &buffer, const GetXMLOptions &options)
{
	XMLSerializer *serializer;

	RETURN_IF_ERROR(XMLSerializer::MakeToStringSerializer(serializer, &buffer));

	OpAutoPtr<XMLSerializer> serializer_anchor(serializer);

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	if (options.canonicalize != GetXMLOptions::CANONICALIZE_NONE)
	{
		XMLSerializer::CanonicalizeVersion version;

		switch (options.canonicalize)
		{
		case GetXMLOptions::CANONICALIZE_WITHOUT_COMMENTS:
		case GetXMLOptions::CANONICALIZE_WITH_COMMENTS:
			version = XMLSerializer::CANONICALIZE_1_0;
			break;

		default:
			version = XMLSerializer::CANONICALIZE_1_0_EXCLUSIVE;
		}

		RETURN_IF_ERROR(serializer->SetCanonicalize(version, options.canonicalize == GetXMLOptions::CANONICALIZE_WITH_COMMENTS || options.canonicalize == GetXMLOptions::CANONICALIZE_WITH_COMMENTS_EXCLUSIVE));
	}
	else
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
	{
		XMLDocumentInformation docinfo;

		if (options.include_xml_declaration)
		{
			OpString encoding16;
			if (options.encoding)
				RETURN_IF_ERROR(encoding16.Set(options.encoding));
			RETURN_IF_ERROR(docinfo.SetXMLDeclaration(XMLVERSION_1_0, XMLSTANDALONE_NONE, encoding16.CStr()));
		}

		XMLSerializer::Configuration configuration;

		configuration.document_information = &docinfo;
		configuration.normalize_namespaces = TRUE;
		configuration.add_xml_space_attributes = TRUE;
		configuration.encoding = options.encoding;
		configuration.format_pretty_print = options.format_pretty_print;

		RETURN_IF_ERROR(serializer->SetConfiguration(configuration));
	}

	URL url;

	data->scope = data->current ? options.scope : XMLFragment::GetXMLOptions::SCOPE_WHOLE_FRAGMENT;
	OP_STATUS status = serializer->Serialize(NULL, url, this, NULL);
	data->scope = XMLFragment::GetXMLOptions::SCOPE_WHOLE_FRAGMENT;

	return status;
}

OP_STATUS
XMLFragment::GetXML(TempBuffer &buffer, BOOL include_xml_declaration, const char *encoding, BOOL format_pretty_print)
{
	GetXMLOptions options(include_xml_declaration);
	options.encoding = encoding;
	options.format_pretty_print = format_pretty_print;
	return GetXML(buffer, options);
}

OP_STATUS
XMLFragment::GetEncodedXML(ByteBuffer &bytebuffer, const GetXMLOptions &options)
{
	OutputConverter *converter;

	const char *encoding = options.encoding;

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	if (options.canonicalize)
		encoding = "UTF-8";
#endif // XMLUTILS_CANONICAL_XML_SUPPORT

	if (!encoding)
		return OpStatus::ERR;

	RETURN_IF_ERROR(OutputConverter::CreateCharConverter(encoding, &converter));

	if (!converter)
		return OpStatus::ERR;

	OpAutoPtr<OutputConverter> converter_anchor(converter);

	TempBuffer buffer;

	RETURN_IF_ERROR(GetXML(buffer, options));

	const uni_char *source = buffer.GetStorage();

	unsigned source_length = buffer.Length() * sizeof source[0], source_read = 0;

	while (1)
	{
		char *chunk;
		unsigned chunksize;

		RETURN_IF_ERROR(bytebuffer.GetNewChunk(&chunk, &chunksize));

		int read, written = converter->Convert((const char *) source + source_read, source_length - source_read, chunk, chunksize, &read);

		if (written < 0)
			return OpStatus::ERR_NO_MEMORY;

		if (read == 0 && written == 0)
		{
			char temporary[MIN(64, BYTEBUFFER_CHUNKSIZE)];

			written = converter->Convert((const char *) source + source_read, source_length - source_read, temporary, sizeof temporary, &read);

			if (written < 0)
				return OpStatus::ERR_NO_MEMORY;
			else if (written == 0)
			{
				OP_ASSERT(!"This ought not happen!");
				return OpStatus::ERR;
			}

			int inchunk1 = MIN(chunksize, (unsigned) written), inchunk2 = written - inchunk1;

			op_memcpy(chunk, temporary, inchunk1);
			bytebuffer.AddBytes(inchunk1);

			if (inchunk2 != 0)
			{
				RETURN_IF_ERROR(bytebuffer.GetNewChunk(&chunk, &chunksize));

				op_memcpy(chunk, temporary + inchunk1, inchunk2);
				bytebuffer.AddBytes(inchunk2);
			}
		}
		else
			bytebuffer.AddBytes(written);

		source_read += read;

		if (source_read == source_length)
			break;
	}

	return OpStatus::OK;
}

OP_STATUS
XMLFragment::GetEncodedXML(ByteBuffer &bytebuffer, BOOL include_xml_declaration, const char *encoding, BOOL format_pretty_print)
{
	GetXMLOptions options(include_xml_declaration);
	options.encoding = encoding;
	options.format_pretty_print = format_pretty_print;
	return GetEncodedXML(bytebuffer, options);
}

#ifdef XMLUTILS_XMLFRAGMENT_XMLTREEACCESSOR_SUPPORT

class XMLFragmentTreeAccessorAttributes
	: public XMLTreeAccessor::Attributes
{
private:
	XMLFragmentData::Element *element;
	BOOL ignore_nsdeclarations;

public:
	void Initialize(XMLFragmentData::Element *element0, BOOL ignore_nsdeclarations0)
	{
		element = element0;
		ignore_nsdeclarations = ignore_nsdeclarations0;
	}

	virtual unsigned GetCount();
	virtual OP_STATUS GetAttribute(unsigned index, XMLCompleteName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer);
};

class XMLFragmentTreeAccessor
	: public XMLTreeAccessor
{
private:
	XMLFragmentData *fragment;
	XMLFragmentTreeAccessorAttributes attributes;

public:
	XMLFragmentTreeAccessor(XMLFragmentData *fragment)
		: fragment(fragment)
	{
	}

	OP_STATUS Construct();

	/* From XMLTreeAccessor: */
	virtual BOOL IsCaseSensitive();
	virtual XMLTreeAccessor::NodeType GetNodeType(XMLTreeAccessor::Node *node);
	virtual XMLTreeAccessor::Node *GetRoot();
	virtual XMLTreeAccessor::Node *GetParent(XMLTreeAccessor::Node *from);
	virtual XMLTreeAccessor::Node *GetFirstChild(XMLTreeAccessor::Node *from);
	virtual XMLTreeAccessor::Node *GetLastChild(XMLTreeAccessor::Node *from);
	virtual XMLTreeAccessor::Node *GetNextSibling(XMLTreeAccessor::Node *from);
	virtual XMLTreeAccessor::Node *GetPreviousSibling(XMLTreeAccessor::Node *from);
	virtual BOOL IsEmptyText(XMLTreeAccessor::Node *node);
	virtual BOOL IsWhitespaceOnly(XMLTreeAccessor::Node *node);
	virtual void GetName(XMLCompleteName &name, XMLTreeAccessor::Node *node);
	virtual XMLCompleteName GetName(XMLTreeAccessor::Node *node);
	virtual const uni_char *GetPITarget(XMLTreeAccessor::Node *node);
	virtual void GetAttributes(XMLTreeAccessor::Attributes *&attributes, XMLTreeAccessor::Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations);
	virtual XMLTreeAccessor::Attributes *GetAttributes(XMLTreeAccessor::Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations);
	virtual OP_STATUS GetData(const uni_char *&data, XMLTreeAccessor::Node *node, TempBuffer *buffer);
	virtual URL GetDocumentURL();
	virtual const XMLDocumentInformation *GetDocumentInformation();

	static XMLFragmentData::Content *Import(XMLTreeAccessor::Node *node) { return static_cast<XMLFragmentData::Content *>(node); }
	static XMLFragmentData::Element *ImportElement(XMLTreeAccessor::Node *node) { XMLFragmentData::Content *content = Import(node); return content && content->type == XMLFragment::CONTENT_ELEMENT ? static_cast<XMLFragmentData::Element *>(content) : NULL; }
	static XMLFragmentData::Text *ImportText(XMLTreeAccessor::Node *node) { XMLFragmentData::Content *content = Import(node); return content && content->type == XMLFragment::CONTENT_TEXT ? static_cast<XMLFragmentData::Text *>(content) : NULL; }
	static XMLTreeAccessor::Node *Export(XMLFragmentData::Content *content) { return static_cast<XMLTreeAccessor::Node *>(content); }
};

OP_STATUS
XMLFragmentTreeAccessor::Construct()
{
	return ConstructFallbackImplementation();
}

/* virtual */ BOOL
XMLFragmentTreeAccessor::IsCaseSensitive()
{
	return TRUE;
}

/* virtual */ XMLTreeAccessor::NodeType
XMLFragmentTreeAccessor::GetNodeType(XMLTreeAccessor::Node *node)
{
	XMLFragmentData::Content *content = Import(node);
	if (content->type == XMLFragment::CONTENT_ELEMENT)
		return content->parent ? XMLTreeAccessor::TYPE_ELEMENT : XMLTreeAccessor::TYPE_ROOT;
	else
		return XMLTreeAccessor::TYPE_TEXT;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFragmentTreeAccessor::GetRoot()
{
	return Export(fragment->root);
}

/* virtual */ XMLTreeAccessor::Node *
XMLFragmentTreeAccessor::GetParent(XMLTreeAccessor::Node *from)
{
	XMLFragmentData::Content *parent = Import(from)->parent;
	return parent && FilterNode(parent) ? parent : NULL;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFragmentTreeAccessor::GetFirstChild(XMLTreeAccessor::Node *from)
{
	if (XMLFragmentData::Element *element = ImportElement(from))
	{
		if (void **children = element->children)
			while (XMLFragmentData::Content *child = static_cast<XMLFragmentData::Content *>(*children))
				if (FilterNode(Export(child)))
					return Export(child);
				else
					++children;
	}
	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFragmentTreeAccessor::GetLastChild(XMLTreeAccessor::Node *from)
{
	if (XMLFragmentData::Element *element = ImportElement(from))
	{
		if (void **children = element->children)
		{
			void **first = children;

			while (*children)
				++children;

			do
			{
				XMLFragmentData::Content *child = static_cast<XMLFragmentData::Content *>(*--children);
				if (FilterNode(Export(child)))
					return Export(child);
			}
			while (children != first);
		}
	}
	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFragmentTreeAccessor::GetNextSibling(XMLTreeAccessor::Node *from)
{
	XMLFragmentData::Content *content = Import(from);

	if (XMLFragmentData::Element *parent = content->parent)
	{
		void **children = parent->children;

		while (*children != content)
			++children;

		++children;

		while (XMLFragmentData::Content *child = static_cast<XMLFragmentData::Content *>(*children))
			if (FilterNode(Export(child)))
				return Export(child);
			else
				++children;
	}

	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFragmentTreeAccessor::GetPreviousSibling(XMLTreeAccessor::Node *from)
{
	XMLFragmentData::Content *content = Import(from);

	if (XMLFragmentData::Element *parent = content->parent)
	{
		void **children = parent->children;

		if (*children != content)
		{
			void **first = children;

			while (*++children != content) {}

			do
			{
				XMLFragmentData::Content *child = static_cast<XMLFragmentData::Content *>(*--children);
				if (FilterNode(Export(child)))
					return Export(child);
			}
			while (children != first);
		}
	}

	return NULL;
}

/* virtual */ BOOL
XMLFragmentTreeAccessor::IsEmptyText(XMLTreeAccessor::Node *node)
{
	if (XMLFragmentData::Text *text = ImportText(node))
		return text->data.IsEmpty();
	else
		return FALSE;
}

/* virtual */ BOOL
XMLFragmentTreeAccessor::IsWhitespaceOnly(XMLTreeAccessor::Node *node)
{
	if (XMLFragmentData::Text *text = ImportText(node))
		return XMLUtils::IsWhitespace(text->data.CStr());
	else
		return FALSE;
}

/* virtual */ void
XMLFragmentTreeAccessor::GetName(XMLCompleteName &name, XMLTreeAccessor::Node *node)
{
	if (XMLFragmentData::Element *element = ImportElement(node))
		name = element->name;
}

/* virtual */ XMLCompleteName
XMLFragmentTreeAccessor::GetName(XMLTreeAccessor::Node *node)
{
	if (XMLFragmentData::Element *element = ImportElement(node))
		return element->name;
	else
		return XMLCompleteName();
}

/* virtual */ const uni_char *
XMLFragmentTreeAccessor::GetPITarget(XMLTreeAccessor::Node *node)
{
	return UNI_L("");
}

/* virtual */ unsigned
XMLFragmentTreeAccessorAttributes::GetCount()
{
	unsigned count = 0;

	if (element && element->attributes)
	{
		void **attributes = element->attributes;

		while (XMLFragmentData::Element::Attribute *attribute = static_cast<XMLFragmentData::Element::Attribute *>(*attributes))
		{
			if (!ignore_nsdeclarations || !attribute->name.IsXMLNamespaces())
				++count;

			++attributes;
		}
	}

	return count;
}

/* virtual */ OP_STATUS
XMLFragmentTreeAccessorAttributes::GetAttribute(unsigned index, XMLCompleteName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer)
{
	if (element && element->attributes)
	{
		void **attributes = element->attributes;

		while (XMLFragmentData::Element::Attribute *attribute = static_cast<XMLFragmentData::Element::Attribute *>(*attributes))
		{
			if (!ignore_nsdeclarations || !attribute->name.IsXMLNamespaces())
				if (index-- == 0)
				{
					name = attribute->name;
					value = attribute->value.CStr();
					id = attribute->id;
					specified = FALSE;
					return OpStatus::OK;
				}

			++attributes;
		}
	}

	return OpStatus::ERR;
}

/* virtual */ void
XMLFragmentTreeAccessor::GetAttributes(XMLTreeAccessor::Attributes *&attributes0, XMLTreeAccessor::Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations)
{
	attributes.Initialize(ImportElement(node), ignore_nsdeclarations);
	attributes0 = &attributes;
}

/* virtual */ XMLTreeAccessor::Attributes *
XMLFragmentTreeAccessor::GetAttributes(XMLTreeAccessor::Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations)
{
	attributes.Initialize(ImportElement(node), ignore_nsdeclarations);
	return &attributes;
}

/* virtual */ OP_STATUS
XMLFragmentTreeAccessor::GetData(const uni_char *&data, XMLTreeAccessor::Node *node, TempBuffer *buffer)
{
	if (XMLFragmentData::Text *text = ImportText(node))
	{
		data = text->data.CStr();
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

/* virtual */ URL
XMLFragmentTreeAccessor::GetDocumentURL()
{
	return fragment->url;
}

/* virtual */ const XMLDocumentInformation *
XMLFragmentTreeAccessor::GetDocumentInformation()
{
	return &fragment->documentinfo;
}

OP_STATUS
XMLFragment::CreateXMLTreeAccessor(XMLTreeAccessor *&treeaccessor)
{
	treeaccessor = OP_NEW(XMLFragmentTreeAccessor, (data));
	if (treeaccessor && OpStatus::IsSuccess(static_cast<XMLFragmentTreeAccessor *>(treeaccessor)->Construct()))
		return OpStatus::OK;
	else
	{
		FreeXMLTreeAccessor(treeaccessor);
		return OpStatus::ERR_NO_MEMORY;
	}
}

/* static */ void
XMLFragment::FreeXMLTreeAccessor(XMLTreeAccessor *treeaccessor)
{
	XMLFragmentTreeAccessor *fragmenttreeaccessor = static_cast<XMLFragmentTreeAccessor *>(treeaccessor);
	OP_DELETE(fragmenttreeaccessor);
}

#endif // XMLUTILS_XMLFRAGMENT_XMLTREEACCESSOR_SUPPORT

#ifdef XMLUTILS_XMLFRAGMENT_XPATH_SUPPORT
#include "modules/xpath/xpath.h"

static OP_STATUS
XMLFragment_ConstructXPathNamespaces(XPathNamespaces *&namespaces, XMLNamespaceDeclaration *nsdeclarations)
{
	unsigned count = XMLNamespaceDeclaration::CountDeclaredPrefixes(nsdeclarations);

	if (count != 0)
	{
		RETURN_IF_ERROR(XPathNamespaces::Make(namespaces));

		for (unsigned index = 0; index < count; ++index)
		{
			XMLNamespaceDeclaration *nsdeclaration = XMLNamespaceDeclaration::FindDeclarationByIndex(nsdeclarations, index);

			if (OpStatus::IsMemoryError(namespaces->Add(nsdeclaration->GetPrefix(), nsdeclaration->GetUri())))
				goto failed;
		}

		return OpStatus::OK;

	failed:
		XPathNamespaces::Free(namespaces);
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		namespaces = NULL;
		return OpStatus::OK;
	}
}

static OP_STATUS
XMLFragment_EvaluateXPath(double *number, BOOL *boolean, OpString *string, const uni_char *source, XMLFragment &fragment, XMLFragmentData::Content *context_node, XPathExpression::Evaluate::Type type, XMLNamespaceDeclaration *nsdeclarations)
{
	OP_STATUS status;

	XPathNamespaces *namespaces;
	status = XMLFragment_ConstructXPathNamespaces(namespaces, nsdeclarations);

	if (OpStatus::IsSuccess(status))
	{
		XPathExpression *expression;
		XPathExpression::ExpressionData data;
		data.source = source;
		data.namespaces = namespaces;
		status = XPathExpression::Make(expression, data);

		if (OpStatus::IsSuccess(status))
		{
			XPathExpression::Evaluate *evaluate;
			status = XPathExpression::Evaluate::Make(evaluate, expression);

			if (OpStatus::IsSuccess(status))
			{
				XMLTreeAccessor *tree;
				status = fragment.CreateXMLTreeAccessor(tree);

				if (OpStatus::IsSuccess(status))
				{
					XPathNode *node;
					status = XPathNode::Make(node, tree, XMLFragmentTreeAccessor::Export(context_node));

					if (OpStatus::IsSuccess(status))
					{
						evaluate->SetContext(node);
						evaluate->SetRequestedType(type);

						const uni_char *string0 = NULL;

						OP_BOOLEAN result = OpBoolean::IS_FALSE;
						while (result == OpBoolean::IS_FALSE)
							switch (type)
							{
							case XPathExpression::Evaluate::PRIMITIVE_NUMBER:
								result = evaluate->GetNumberResult(*number);
								break;

							case XPathExpression::Evaluate::PRIMITIVE_BOOLEAN:
								result = evaluate->GetBooleanResult(*boolean);
								break;

							default:
								result = evaluate->GetStringResult(string0);
							}

						if (result == OpBoolean::IS_TRUE)
							if (type == XPathExpression::Evaluate::PRIMITIVE_STRING)
								status = string->Set(string0);
							else
								status = OpStatus::OK;
					}

					XMLFragment::FreeXMLTreeAccessor(tree);
				}

				XPathExpression::Evaluate::Free(evaluate);
			}

			XPathExpression::Free(expression);
		}

		XPathNamespaces::Free(namespaces);
	}

	return status;
}

OP_STATUS
XMLFragment::EvaluateXPathToNumber(double &result, const uni_char *expression, XMLNamespaceDeclaration *nsdeclarations)
{
	return XMLFragment_EvaluateXPath(&result, NULL, NULL, expression, *this, data->current ? data->current : data->root, XPathExpression::Evaluate::PRIMITIVE_NUMBER, nsdeclarations);
}

OP_STATUS
XMLFragment::EvaluateXPathToBoolean(BOOL &result, const uni_char *expression, XMLNamespaceDeclaration *nsdeclarations)
{
	return XMLFragment_EvaluateXPath(NULL, &result, NULL, expression, *this, data->current ? data->current : data->root, XPathExpression::Evaluate::PRIMITIVE_BOOLEAN, nsdeclarations);
}

OP_STATUS
XMLFragment::EvaluateXPathToString(OpString &result, const uni_char *expression, XMLNamespaceDeclaration *nsdeclarations)
{
	return XMLFragment_EvaluateXPath(NULL, NULL, &result, expression, *this, data->current ? data->current : data->root, XPathExpression::Evaluate::PRIMITIVE_STRING, nsdeclarations);
}

#endif // XMLUTILS_XMLFRAGMENT_XPATH_SUPPORT
#endif // XMLUTILS_XMLFRAGMENT_SUPPORT
