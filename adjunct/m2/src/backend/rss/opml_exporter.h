/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPML_EXPORTER_H
#define OPML_EXPORTER_H

class Indexer;
class Index;

class OPMLExporter
{
public:
	OPMLExporter();
	OP_STATUS Export(OpFile& file);

private:
	OP_STATUS	WriteOutline(OpFile& file, int level, const uni_char* text, const uni_char* title, const uni_char* xml_url, const uni_char* description);
	OP_STATUS	WriteOutlineForIndexAndChildIndexes(OpFile& file, int level, Index* index);
	OP_STATUS	XMLEscape(const OpStringC& input, OpString& output);
	OP_STATUS	WriteUTF8String(OpFile& file, const OpStringC& data);

	// Members.
	Indexer*	m_indexer;
	INT32		m_export_count;
};

#endif //OPML_EXPORTER_H
