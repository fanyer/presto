/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ST_YAML_DOCUMENT_H
#define ST_YAML_DOCUMENT_H

#ifdef SELFTEST

#include "adjunct/ui_parser/yaml/yaml.h"

/** @brief Helper for selftests to parse a Yaml document from a string */
struct ST_YamlDocument
{
	yaml_document_t document;

	ST_YamlDocument(const yaml_document_t& _document) : document(_document) {}
	~ST_YamlDocument() { yaml_document_delete(&document); }
	operator yaml_document_t*() { return &document; }

	static yaml_document_t FromString(const char* string)
	{
		yaml_document_t document;
		yaml_parser_t parser;
		if (!yaml_parser_initialize(&parser))
		{
			static yaml_document_t blank;
			return blank;
		}
		yaml_parser_set_input_string(&parser, (const unsigned char*)string, op_strlen(string));
		yaml_parser_load(&parser, &document);
		yaml_parser_delete(&parser);
		return document;
	}
};

#endif // SELFTEST

#endif // ST_YAML_DOCUMENT_H