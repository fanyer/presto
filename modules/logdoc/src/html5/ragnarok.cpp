/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * Wrapper to invoke the parser from the command line
 */

#include "core/pch.h"

#include <string.h>
#include <stdio.h>
#include <wchar.h>

#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/html5parser.h"
#include "modules/logdoc/html5namemapper.h"
#include "modules/logdoc/src/html5/h5node.h"
#include "modules/logdoc/src/html5/html5entities.h"
#include "modules/util/opstring.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/simset.h"
#include "modules/debug/debug.h"

Opera* g_opera = NULL;
HTML5NameMapper* g_html5_name_mapper = NULL;

#define INPUT_BUFFER_SIZE 4048

static int tokenize_only = FALSE;
static Markup::Type context_elm_type = Markup::HTE_DOC_ROOT;
static char* context_elm_name = NULL;

extern OP_STATUS read_stdin(ByteBuffer &bbuf);

#include <signal.h>
extern "C"
void Debug_OpAssert(const char* expression, const char* file, int line)
{
    dbg_printf("ASSERT FAILED: OP_ASSERT(%s) %s:%d\n", expression, file, line);
#if defined(UNIX)
    raise(SIGABRT);
#else
    assert(0);
#endif
}

static void DumpNode(const uni_char *spaces, H5Node *node)
{
	switch (node->GetType())
	{
	case H5Node::TEXT:
		dbg_printf("|%S\"%S\"\n", spaces, static_cast<H5Text*>(node)->Content());
		break;

	case H5Node::DOCTYPE:
		{
			H5Doctype *doctype = static_cast<H5Doctype*>(node);
			dbg_printf("|%S<!DOCTYPE %S", spaces, doctype->GetName());
			if (doctype->GetSystemIdentifier() || doctype->GetPublicIdentifier())
			{
				if (doctype->GetPublicIdentifier())
					dbg_printf(" \"%S\"", doctype->GetPublicIdentifier());
				else
					dbg_printf(" \"\"");

				if (doctype->GetSystemIdentifier())
					dbg_printf(" \"%S\"", doctype->GetSystemIdentifier());
				else
					dbg_printf(" \"\"");
			}
			dbg_printf(">\n");
		}
		break;

	case H5Node::COMMENT:
		dbg_printf("|%S<!-- %S -->\n", spaces, static_cast<H5Comment*>(node)->GetData());
		break;

	case H5Node::ELEMENT:
		{
			H5Element *elm = static_cast<H5Element*>(node);
			const uni_char *prefix = UNI_L("");
			if (elm->GetNs() == Markup::MATH)
				prefix = UNI_L("math ");
			else if (elm->GetNs() == Markup::SVG)
				prefix = UNI_L("svg ");

			dbg_printf("|%S<%S%S>\n", spaces, prefix, elm->GetName());
			elm->SortAttributes();
			for (unsigned i = 0; i < elm->GetAttributeCount(); i++)
			{
				HTML5AttrCopy *attr = elm->GetAttributeByIndex(i);
				const uni_char *attr_prefix = UNI_L("");
				if (attr->GetNs() == Markup::XLINK)
					attr_prefix = UNI_L("xlink ");
				else if (attr->GetNs() == Markup::XML)
					attr_prefix = UNI_L("xml ");
				else if (attr->GetNs() == Markup::XMLNS)
					attr_prefix = UNI_L("xmlns ");
				
				dbg_printf("|%S  %S%S=\"%S\"\n", spaces, attr_prefix, attr->GetName(), attr->GetValue());
			}
		}
		break;
	}
}

void write_parser_results(HTML5Parser* parser, H5Node* root, const uni_char* buffer)
{
	dbg_printf("#errors\n");
	unsigned num_errors = parser->GetNumberOfErrors();
	for (unsigned i = 0; i < num_errors; i++)
	{
		unsigned line;
		unsigned pos;
		HTML5Parser::ErrorCode code = parser->GetError(i, line, pos);
		dbg_printf("%u,%u: %s\n", line, pos, HTML5Parser::GetErrorString(code));
	}

	if (context_elm_type == Markup::HTE_DOC_ROOT)
		dbg_printf("#document\n");
	else
		dbg_printf("#document-fragment\n%s\n", context_elm_name);

	uni_char *spaces = OP_NEWA(uni_char, 256);
	H5Node *iter = root->FirstChild();
	while (iter)
	{
		uni_char *current_space = spaces;
		H5Node *parent = iter->Parent();
		while (parent && current_space < spaces + 254)
		{
			*current_space++ = L' ';
			*current_space++ = L' ';
			parent = parent->Parent();
		}
		*current_space = 0;

		DumpNode(spaces, iter);

		iter = iter->Next();
	}

	OP_DELETEA(spaces);
}

void parse_command_line(int argc, char **argv)
{
	char tokenizer_flag[] = "-t";

	for (; argc; argc--, argv++)
	{
		if (op_strncmp("-c=", *argv, 3) == 0)
		{
			context_elm_name = op_strchr(*argv, '=') + 1;
			OpString elm_string;
			elm_string.Set(context_elm_name);
			context_elm_type = g_html5_name_mapper->GetTypeFromName(elm_string.CStr(), FALSE, Markup::HTML);
		}
		else if (op_strcmp(tokenizer_flag, *argv) == 0)
			tokenize_only = TRUE;
	}
}

int main(int argc, char **argv)
{
	ByteBuffer bbuffer;
	char memory[sizeof(Opera)]; /* ARRAY OK 2011-09-08 danielsp */
	op_memset(memory, 0, sizeof(Opera));
	g_opera = (Opera*)&memory;
	g_opera->InitL();

	g_html5_name_mapper = OP_NEW(HTML5NameMapper, ());
	g_html5_name_mapper->InitL();
	HTML5EntityStates::InitL();
	parse_command_line(argc, argv);

	LogicalDocument *logdoc = OP_NEW(LogicalDocument, ());
	if (!logdoc)
		return 1;

	if (tokenize_only)
		logdoc->SetTokenizeOnly();

	if (OpStatus::IsSuccess(read_stdin(bbuffer)))
	{
		uni_char *buffer = reinterpret_cast<uni_char*>(bbuffer.Copy(FALSE));
		int buffer_length = (bbuffer.Length() / sizeof(uni_char)) - 1;
		int remaining_length = buffer_length;
		int chunk_size = context_elm_type == Markup::HTE_DOC_ROOT ? 1024 : buffer_length;

		// Parsing the buffer may replace its content, so write what we received before
		// parsing it
		if (!tokenize_only)
		{
			if (buffer)
			{
				uni_char *buffer_copy = OP_NEWA(uni_char, buffer_length + 1);
				op_memcpy(buffer_copy, buffer, (buffer_length + 1) * sizeof(uni_char));
				// Replace NULs with 0xDFFF which are handled the same later, as tempbuffer doesn't handle NULs
				for (uni_char* c = buffer_copy; c < buffer_copy + buffer_length; c++)
					if (*c == 0)
						*c = 0xDFFF;

				dbg_printf("#data\n%S\n", buffer_copy);
			}
			else
				dbg_printf("#data\n\n");
		}

		OP_PARSING_STATUS pstatus = OpStatus::OK;
		do 
		{
			if (pstatus == ParsingStatus::EXECUTE_SCRIPT)
			{
#if 0
				static int i = 0;
				// dummy document.write content:
				const uni_char* script_buffer = i++ <= 1 ? UNI_L("<script>a</script>1<script>b</script>") : UNI_L("<blink>OMG!!1");
//				const uni_char* script_buffer = i++ <= 1 ? UNI_L("<script>a</script>1") : UNI_L("<blink>OMG!!1</blink>2");
				pstatus = logdoc->AddParsingData(script_buffer, uni_strlen(script_buffer));
				if (pstatus == OpStatus::OK)
#endif
					pstatus = logdoc->ContinueParsing();
			}
			else
				pstatus = logdoc->Parse(context_elm_type, buffer + (buffer_length - remaining_length), MIN(remaining_length, chunk_size), remaining_length <= chunk_size);

			if (pstatus == ParsingStatus::NEED_MORE_DATA)
				remaining_length -= chunk_size;
		}
		while (remaining_length > 0 && (pstatus == ParsingStatus::NEED_MORE_DATA || pstatus == ParsingStatus::EXECUTE_SCRIPT));

		if (!tokenize_only)
			write_parser_results(logdoc->GetParser(), logdoc->GetRoot(), buffer);
	}
	
	OP_DELETE(logdoc);

	HTML5EntityStates::Destroy();
	OP_DELETE(g_html5_name_mapper);
	g_opera->Destroy();
	
	return 0;
}
