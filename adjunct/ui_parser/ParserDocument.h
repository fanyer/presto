/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef PARSER_DOCUMENT_H
#define PARSER_DOCUMENT_H

#include "adjunct/ui_parser/ParserNode.h"
#include "adjunct/ui_parser/yaml/yaml.h"

class ParserLogger;

/*
 * @brief A wrapper for the YAML document
 *
 */
class ParserDocument
{
public:
	ParserDocument();
	~ParserDocument();

	/**
	 * Loads the file from the path specified and retrieves/stores the YAML document.
	 */
	OP_STATUS		Init(const OpStringC& file_path, OpFileFolder folder);

	/** Initializes from an existing yaml document
	  * @param document Document to initialize from. ParserDocument is responsible for freeing resources
	  *        associated with this document.
	  */
	OP_STATUS		Init(const yaml_document_t& document) { m_document = document; m_is_inited = TRUE; return OpStatus::OK; }

	/** Set a logger to use for logging parser errors, or NULL for no logging */
	void			SetLogger(ParserLogger* logger) { m_logger = logger; }

	/** Retrieves the root node from the YAML document
	  * @param node Root node if successful
	  * @return Whether the root node was found
	  */
	BOOL			GetRootNode(ParserNodeMapping& node);

	/** Retrieves a node by ID from the YAML document.
	  * @param id ID of node to find
	  * @param node Node if found
	  * @return Whether the node was found
	  */
	BOOL			GetNodeByID(ParserNodeID id, ParserNode& node);
	BOOL			GetNodeByID(ParserNodeID id, ParserNodeScalar& node);
	BOOL			GetNodeByID(ParserNodeID id, ParserNodeSequence& node);
	BOOL			GetNodeByID(ParserNodeID id, ParserNodeMapping& node);

	/** @return whether the node at ID is a merge node (indicates value for this key
	  *         should be merged in a mapping)
	  */
	BOOL			IsMergeNode(ParserNodeID id);

private:
	template<yaml_node_type_t NodeType, class T>
		BOOL GetNodeByIDInternal(ParserNodeID id, T& node);
	static int ReadHandler(void *data, unsigned char *buffer, size_t size, size_t *size_read);
	static const char* GetNodeTypeString(yaml_node_type_t type);

	yaml_document_t	m_document;	
	BOOL			m_is_inited;
	ParserLogger*	m_logger;
};


#endif // PARSER_DOCUMENT_H
