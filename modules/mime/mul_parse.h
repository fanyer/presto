/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _MUL_PARSE_H
#define _MUL_PARSE_H

#include "modules/mime/mul_par_cs.h"
#include "modules/mime/mime_enum.h"
#include "modules/mime/multipart_parser/abstract_mpp.h"
#include "modules/formats/argsplit.h"

class MultiPart_Parser : public Multipart_CacheStorage, public MultiPartParser_Listener
{
private:
	MIME_encoding encoding;
#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	BOOL use_content_location;
#endif
	BOOL binary;
	AbstractMultiPartParser* parser;
	BOOL ignoreBody;
	BOOL hasWarning;

public:

	MultiPart_Parser(URL_Rep *url, BOOL binaryMultipart);
	virtual ~MultiPart_Parser();

    virtual void ConstructL(Cache_Storage * initial_storage, ParameterList *content_type, const OpStringC8 &boundary_header, OpStringS8 &content_encoding);
	virtual void	MultipartSetFinished(BOOL force=FALSE );

protected:

	virtual void ProcessDataL();
	/** @return position where the reading of the contents finished */
	uint32 HandleContentL(const unsigned char *buffer, uint32 start_pos, uint32 buf_end, BOOL more);
	void HandleHeaderL(HeaderList &headers);
	virtual BOOL IsLoadingPart() { return ignoreBody || Multipart_CacheStorage::IsLoadingPart(); }

	virtual void OnMultiPartParserWarning(AbstractMultiPartParser::Warning reason, unsigned int offset, unsigned int part);

#undef MIME_DEBUG_MULTIPART_BINARY
#ifdef MIME_DEBUG_MULTIPART_BINARY
	FILE* f;
	void DebugStart();
	void DebugNewPart(HeaderList& headers);
	void DebugSavePart(char* data, unsigned int len);
	void DebugEnd();
#endif
};

#endif // _MUL_PARSE_H
