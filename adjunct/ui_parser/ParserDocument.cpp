/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/ui_parser/ParserDocument.h"
#include "adjunct/ui_parser/ParserLogger.h"
#include "modules/util/opfile/opfile.h"


ParserDocument::ParserDocument()
	: m_is_inited(FALSE)
	, m_logger(0)
{
}

ParserDocument::~ParserDocument()
{
	if (m_is_inited)
		yaml_document_delete(&m_document);
}

////////// Init
OP_STATUS
ParserDocument::Init(const OpStringC& file_path, OpFileFolder folder)
{
	OpFile file;
	
	OpString8 output, path8;
	if (m_logger &&
		OpStatus::IsSuccess(path8.SetUTF8FromUTF16(file_path)) &&
		OpStatus::IsSuccess(output.AppendFormat("Loading document '%s'...", path8.CStr())))
	{
		m_logger->OutputEntry(output.CStr());
	}
	RETURN_IF_ERROR(m_logger->Evaluate(file.Construct(file_path.CStr(), folder), "ERROR constructing file"));
	RETURN_IF_ERROR(m_logger->Evaluate(file.Open(OPFILE_READ), "ERROR opening file"));

	yaml_parser_t parser;
	if (!yaml_parser_initialize(&parser))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	yaml_parser_set_input(&parser, &ReadHandler, &file);

	OP_STATUS stat = OpStatus::OK;
	if (!yaml_parser_load(&parser, &m_document))
	{
		OpString8 error_message;
		if (m_logger && OpStatus::IsSuccess(error_message.AppendFormat("  ERROR loading YAML document:\r\n"
			"    %s (index=%i, line=%i, column=%i)\r\n"
			"    %s (index=%i, line=%i, column=%i)"
			, parser.problem, parser.problem_mark.index, parser.problem_mark.line, parser.problem_mark.column
			, parser.context, parser.context_mark.index, parser.context_mark.line, parser.context_mark.column)))
		{
			m_logger->OutputEntry(error_message);
		}
		stat = OpStatus::ERR;
	}
	else if (!yaml_document_get_root_node(&m_document))
	{
		m_logger && m_logger->OutputEntry("  ERROR loading YAML root node. Are you trying to load an empty document?");
		yaml_document_delete(&m_document);
		stat = OpStatus::ERR;
	}
	else
	{
		m_logger && m_logger->OutputEntry("Document loaded successfully.");
		m_is_inited = TRUE;
	}

	yaml_parser_delete(&parser);
	return stat;
}

int ParserDocument::ReadHandler(void *data, unsigned char *buffer, size_t size, size_t *size_read)
{
	OpFile* file = static_cast<OpFile*>(data);
	if (file->Eof())
	{
		// On EOF, the handler should set the size_read to 0 and return 1
		*size_read = 0;
		return 1;
	}

	OpFileLength bytes_read;

	// If the handler failed, the returned value should be 0
	RETURN_VALUE_IF_ERROR(file->Read(buffer, size, &bytes_read), 0);
	*size_read = bytes_read;

	// On success, the handler should return 1
	return 1;
}

////////// GetRootNode
BOOL
ParserDocument::GetRootNode(ParserNodeMapping& node)
{
	yaml_node_t* yaml_node = yaml_document_get_root_node(&m_document);
	if (!yaml_node || yaml_node->type != YAML_MAPPING_NODE)
		return FALSE;

	node = ParserNodeMapping(yaml_node, this);
	return TRUE;
}


////////// GetNodeByID
BOOL ParserDocument::GetNodeByID(ParserNodeID id, ParserNode& node)
{
	return GetNodeByIDInternal<YAML_NO_NODE, ParserNode>(id, node);
}

BOOL ParserDocument::GetNodeByID(ParserNodeID id, ParserNodeScalar& node)
{
	return GetNodeByIDInternal<YAML_SCALAR_NODE, ParserNodeScalar>(id, node);
}

BOOL ParserDocument::GetNodeByID(ParserNodeID id, ParserNodeSequence& node)
{
	return GetNodeByIDInternal<YAML_SEQUENCE_NODE, ParserNodeSequence>(id, node);
}

BOOL ParserDocument::GetNodeByID(ParserNodeID id, ParserNodeMapping& node)
{
	return GetNodeByIDInternal<YAML_MAPPING_NODE, ParserNodeMapping>(id, node);
}

template<yaml_node_type_t NodeType, class T>
BOOL ParserDocument::GetNodeByIDInternal(ParserNodeID id, T& node)
{
	if (!id)
		return FALSE;

	yaml_node_t* yaml_node = yaml_document_get_node(&m_document, id);
	if (!yaml_node)
	{
		m_logger && m_logger->OutputEntry("ERROR retrieving parser node: node not found");
		return FALSE;
	}
	else if (NodeType != YAML_NO_NODE && yaml_node->type != NodeType)
	{
		OpString8 node_type_error;
		if (OpStatus::IsSuccess(node_type_error.AppendFormat("ERROR retrieving parser node: Node type should be '%s', but is '%s'", GetNodeTypeString(NodeType), GetNodeTypeString(yaml_node->type))))
		{
			m_logger && m_logger->OutputEntry(node_type_error.CStr());
		}
		return FALSE;
	}
	node = T(yaml_node, this);
	return TRUE;
}


////////// IsMergeNode
BOOL ParserDocument::IsMergeNode(ParserNodeID id)
{
	yaml_node_t* yaml_node = yaml_document_get_node(&m_document, id);
	return yaml_node && yaml_node->type == YAML_MERGE_NODE;
}


const char* ParserDocument::GetNodeTypeString(yaml_node_type_t type)
{
	switch (type)
	{
	case YAML_NO_NODE:
		return "YAML_NO_NODE";
	case YAML_SCALAR_NODE:
		return "YAML_SCALAR_NODE";
	case YAML_SEQUENCE_NODE:
		return "YAML_SEQUENCE_NODE";
	case YAML_MAPPING_NODE:
		return "YAML_MAPPING_NODE";
	case YAML_MERGE_NODE:
		return "YAML_MERGE_NODE";
	default:
		OP_ASSERT(!"Unhandled node type");
		return "";
	}
}
