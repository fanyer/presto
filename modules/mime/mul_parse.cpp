/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_man.h"
#include "modules/url/url_pd.h"

#include "modules/mime/mul_parse.h"
#include "modules/mime/mimeutil.h"
#include "modules/mime/multipart_parser/text_mpp.h"
#include "modules/mime/multipart_parser/binary_mpp.h"

#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/base64_decode.h"
#include "modules/formats/uri_escape.h"

#include "modules/olddebug/tstdump.h"

void URL_DataStorage::CreateMultipartCacheL(ParameterList *content_type, const OpStringC8 &boundary_header)
{
	OpStackAutoPtr<MultiPart_Parser> item(NULL);
#ifdef WBMULTIPART_MIXED_SUPPORT
	if (url->GetAttribute(URL::KContentType) == URL_X_MIXED_BIN_REPLACE_CONTENT)
		item.reset(OP_NEW_L(MultiPart_Parser, (url, TRUE)));
	else
#endif
		item.reset(OP_NEW_L(MultiPart_Parser, (url, FALSE)));

	item->ConstructL(NULL, content_type, boundary_header, http_data->keepinfo.encoding);

	OP_DELETE(old_storage);
	old_storage = NULL;
	OP_DELETE(storage);
	storage = item.release();
}


MultiPart_Parser::MultiPart_Parser(URL_Rep *url, BOOL binaryMultipart)
	: Multipart_CacheStorage(url)
	, binary(binaryMultipart)
	, parser(NULL)
	, ignoreBody(FALSE)
	, hasWarning(FALSE)
{
	encoding = ENCODE_BINARY;
#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	use_content_location = FALSE;
#endif
}

MultiPart_Parser::~MultiPart_Parser()
{
	OP_DELETE(parser);
}

void MultiPart_Parser::ConstructL(Cache_Storage * initial_storage, ParameterList *content_type, const OpStringC8 &boundary_header, OpStringS8 &content_encoding)
{
	Multipart_CacheStorage::ConstructL(initial_storage, content_encoding);

#ifdef MIME_ALLOW_MULTIPART_CACHE_FILL
	Parameters *type_item = (content_type ? content_type->First() : NULL);
	use_content_location |= (type_item && type_item->GetName().Compare("multipart/mixed")==0);
#endif
#ifdef WBMULTIPART_MIXED_SUPPORT
	use_content_location |= (url->GetAttribute(URL::KContentType) == URL_X_MIXED_BIN_REPLACE_CONTENT);
#endif
#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	if (use_content_location)
	{
		ServerName *sn = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
		if(sn)
			sn->BlockByMultipart(url->GetAttribute(URL::KResolvedPort));
	}
#endif

	const char* bndry = NULL;

	Parameters *item = (content_type ? content_type->GetParameterByID(HTTP_General_Tag_Boundary, PARAMETER_ASSIGNED_ONLY) : NULL);
	if(item == NULL)
		bndry = boundary_header.CStr();
	else
		bndry = item->Value();

	encoding = ENCODE_BINARY;

#ifdef WBMULTIPART_MIXED_SUPPORT
	if (binary)
		parser = OP_NEW_L(BinaryMultiPartParser, (this));
	else
#endif
		parser = OP_NEW_L(TextMultiPartParser, (bndry, 0, this));

#ifdef MIME_DEBUG_MULTIPART_BINARY
	DebugStart();
#endif
}

void MultiPart_Parser::ProcessDataL()
{
	OP_ASSERT(parser);

	if (!desc)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	BOOL more = FALSE;
	uint32 buf_len = desc->RetrieveDataL(more);

	OP_ASSERT(more || ((URLStatus) url->GetAttribute(URL::KLoadStatus) != URL_LOADING) || GetFinished());

	const unsigned char *buffer = (const unsigned char *) desc->GetBuffer();

	if (buf_len)
	{
		parser->load((const char*)buffer, (unsigned int)buf_len);
		desc->ConsumeData(buf_len);
	}
	if (!more)
		parser->loadingFinished();

	do
	{
		if (!IsLoadingPart())
		{
			if (!parser->beginNextPart())
				break;

			OpStackAutoPtr<HeaderList> headers(parser->getCurrentPartHeaders());
			ignoreBody = FALSE;
			HandleHeaderL(*headers); // Sets IsLoadingPart()==TRUE
#ifdef MIME_DEBUG_MULTIPART_BINARY
			DebugNewPart(*headers);
#endif
		}

		while (IsLoadingPart() && (parser->availableCurrentPartData() || parser->noMoreCurrentPartData()))
		{
			char* partData;
			BOOL noMoreCurrentPartData;
			unsigned int availableCurrentPartData = parser->getCurrentPartDataPointer(partData, &noMoreCurrentPartData);
			OP_ASSERT(availableCurrentPartData || noMoreCurrentPartData);

			uint32 consumedData = HandleContentL((const unsigned char*)partData, 0, availableCurrentPartData, !noMoreCurrentPartData);
			if (consumedData == 0 && availableCurrentPartData)
				break;
#ifdef MIME_DEBUG_MULTIPART_BINARY
			DebugSavePart(partData, consumedData);
#endif

			parser->consumeCurrentPartData(consumedData);
			if (consumedData == availableCurrentPartData && noMoreCurrentPartData)
			{
				parser->finishCurrentPart();
				ignoreBody = FALSE;
				OP_ASSERT(!IsLoadingPart());
			}
		}
	}
	while (!IsLoadingPart());

	if (!more) {
		OP_ASSERT(parser->noMoreParts());
		if(GetFinished())
			MultipartSetFinished();
#ifdef MIME_DEBUG_MULTIPART_BINARY
		DebugEnd();
#endif
	}
}

uint32 MultiPart_Parser::HandleContentL(const unsigned char *buffer, uint32 start_pos, uint32 buf_end, BOOL more)
{
	if (ignoreBody)
		return buf_end;

	switch(encoding)
	{
		case ENCODE_QP:
			{
				const unsigned char *source = buffer;
				unsigned char *target = (unsigned char *) g_memory_manager->GetTempBuf2k();
				uint32 target_len = g_memory_manager->GetTempBuf2kLen();
				uint32 read_pos, written_len;

				while(start_pos < buf_end)
				{
					written_len = 0;
					read_pos = start_pos;
					while(read_pos < buf_end && written_len < target_len)
					{
						char current = source[read_pos];
						if(current == '=')
						{
							if(read_pos +1 >= buf_end)
							{
								if (!more)
									read_pos++; // Ignore a final trailing '='
								break;
							}

							uint32 scan_pos = read_pos+1;
							unsigned char next1 = source[scan_pos];

							if(op_isxdigit(next1))
							{
								if(scan_pos +1 >= buf_end)
									break; // Not enough to check for the =XX escape
							
								unsigned char next2 = source[scan_pos+1];
								if(op_isxdigit(next2))
								{
									current = UriUnescape::Decode(next1, next2);
									read_pos +=3;
								}
								else
									read_pos ++; // invalid code, pass through unchanged

								target[written_len++] = current;
								continue;
							}
							else if(op_isspace(next1))
							{
								// check for line end, if found ignore rest of line, including CRLF;

								if(next1 == '\n')
								{
									read_pos = scan_pos + 1;
									continue;
								}

								scan_pos ++;
								while(scan_pos < buf_end)
								{
									unsigned char next1 = source[scan_pos];

									if(next1 == '\n')
									{
										read_pos = scan_pos+1;
										break;
									}
									else if(!op_isspace(next1))
									{
										target[written_len++] = current;
										read_pos ++;
										break;
									}
									scan_pos ++;
								}
								if(scan_pos >= buf_end)
								{
									// Did not find EOL
									if(!more)
										read_pos = scan_pos; // No more, consider EOL found, and ignore rest of the line
									break;
								}
							}
							else
							{
								target[written_len++] = current;
								read_pos ++;
							}

						}
						else
						{
							target[written_len++] = current;
							read_pos ++;
						}
					}

					WriteDocumentDataL(target, written_len, TRUE );
					if(read_pos == start_pos)
						break;

					start_pos = read_pos;
				}

				buf_end = start_pos;
			}
			break;
		case ENCODE_BASE64:
			{
				unsigned char *target = (unsigned char *) g_memory_manager->GetTempBuf2k();
				uint32 target_len = g_memory_manager->GetTempBuf2kLen();
				BOOL warn = FALSE;
				unsigned long read_pos, written_len;

				while(start_pos < buf_end)
				{
					written_len = GeneralDecodeBase64(buffer+start_pos, buf_end - start_pos, read_pos, 
						target, warn, target_len);

					WriteDocumentDataL(target, written_len, TRUE);
					if(read_pos == 0)
						break;
					start_pos += read_pos;
				}

				buf_end = start_pos;
			}
			break;
		default:
			WriteDocumentDataL(buffer+ start_pos, buf_end - start_pos, TRUE);
			break;
	}
	if(!more)
		WriteDocumentDataL(NULL, 0, more);

	return buf_end;
}

void MultiPart_Parser::HandleHeaderL(HeaderList &headers)
{
	HeaderEntry *header;

	HeaderEntry *content_encoding = headers.GetHeaderByID(HTTP_Header_Content_Encoding);

	if((URLType) url->GetAttribute(URL::KType) != URL_HTTP && (URLType) url->GetAttribute(URL::KType) != URL_HTTPS)
	{
		ANCHORD(HeaderList, cookie_headers);
		
		cookie_headers.SetKeywordList(headers);
		headers.DuplicateHeadersL(&cookie_headers, HTTP_Header_Set_Cookie2);
		headers.DuplicateHeadersL(&cookie_headers, HTTP_Header_Set_Cookie);
		
		if(!cookie_headers.Empty())
		{
			urlManager->HandleCookiesL(url, cookie_headers
#if defined URL_SEPARATE_MIME_URL_NAMESPACE
				, url->GetContextId()
#endif
				);
		}
	}

#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	URL this_url(url, (char *) NULL);
	URL cloc;
	URL cid;

	if(use_content_location && FirstBodypartCreated())
	{
		header = headers.GetHeaderByID(HTTP_Header_Content_Location);

		if(header && header->GetValue().HasContent())
		{
			cloc = urlManager->GetURL(this_url, header->Value());

			if(cloc == this_url)
				cloc = urlManager->GetURL(this_url, header->Value(), TRUE);

			if((URLType) cloc.GetAttribute(URL::KType) != (URLType) url->GetAttribute(URL::KType) ||
				(ServerName *) cloc.GetAttribute(URL::KServerName, NULL) != (ServerName *) url->GetAttribute(URL::KServerName,NULL) ||
				cloc.GetAttribute(URL::KResolvedPort) != url->GetAttribute(URL::KResolvedPort))
				cloc = URL(); // Not the same scheme/server/port
		}

		header = headers.GetHeaderByID(HTTP_Header_Content_ID);

		if(header && header->GetValue().HasContent())
		{
			cid = ConstructContentIDURL_L(header->GetValue());

			if(cid == cloc)
				cid = URL();
		}

		if(!cid.IsEmpty() && !cloc.IsEmpty())
			cid.SetAttributeL(URL::KAliasURL, cloc);
		
		if (cid.IsEmpty() && cloc.IsEmpty())
		{
			ignoreBody = TRUE; // Otherwise, this bodypart will replace the main document
			return;
		}
	}

	URL new_target(cloc.IsEmpty() ? cid : cloc);

	if(!new_target.IsEmpty())
	{
		URLStatus status = (URLStatus) new_target.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect);
			
		if((status == URL_LOADING && !new_target.GetAttribute(URL::KWaitingForBlockingMultipart, URL::KFollowRedirect)) || status == URL_LOADED)
		{
			ignoreBody = TRUE;
			return;
		}

		new_target.SetAttribute(URL::KWaitingForBlockingMultipart, FALSE);
		if(new_target.GetRep()->GetDataStorage())
			g_main_message_handler->UnsetCallBack(new_target.GetRep()->GetDataStorage(), MSG_URL_MULTIPART_UNBLOCKED,0);
	}
#else
	URL new_target; // Empty
#endif

	HeaderEntry *content_type = headers.GetHeaderByID(HTTP_Header_Content_Type);

	ANCHORD(OpString8, c_encoding);

	c_encoding.SetL(content_encoding ? content_encoding->GetValue() : (const OpStringC8) NULL);

	CreateNextElementL(new_target, (content_type ? content_type->GetValue() : (const OpStringC8) NULL), c_encoding, !!url->GetAttribute(URL::KCachePolicy_NoStore));

	encoding = ENCODE_BINARY;
	header = headers.GetHeaderByID(HTTP_Header_Content_Transfer_Encoding);

	if(header && header->Value())
	{
		extern const KeywordIndex *MIME_Encoding_Keywords(size_t &len);
		size_t enc_len =0;

		const KeywordIndex *enc_words = MIME_Encoding_Keywords(enc_len);
		encoding = (MIME_encoding) CheckKeywordsIndex(header->Value(),enc_words, (int)enc_len);

		switch(encoding)
		{
		case ENCODE_QP:
		case ENCODE_BASE64:
		case ENCODE_BINARY:
			break;
		default:
			encoding = ENCODE_BINARY;
		}
	}
}

void MultiPart_Parser::MultipartSetFinished(BOOL force )
{
#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	if(use_content_location)
	{
		ServerName *sn = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
		if(sn)
			sn->UnblockByMultipart(url->GetAttribute(URL::KResolvedPort));
		g_main_message_handler->PostMessage(MSG_URL_MULTIPART_UNBLOCKED, 0, 0);
	}
#endif
	Multipart_CacheStorage::MultipartSetFinished(force);
}

void MultiPart_Parser::OnMultiPartParserWarning(AbstractMultiPartParser::Warning reason, unsigned int offset, unsigned int part)
{
	hasWarning = TRUE;
}

#ifdef MIME_DEBUG_MULTIPART_BINARY
void MultiPart_Parser::DebugStart()
{
	f = fopen("DebugMultipartBinary.mht","w");
	fwrite("Content-Type: multipart/related; boundary=----Boundary\n",55,1,f);
	fwrite("MIME-Version: 1.0\n",18,1,f);
}
void MultiPart_Parser::DebugNewPart(HeaderList& headers)
{
	fwrite("\n------Boundary\n",16,1,f);
	for (HeaderEntry *he = headers.First(); he; he=he->Suc()) {
		fwrite(he->Name(),op_strlen(he->Name()),1,f);
		fwrite(": ",2,1,f);
		fwrite(he->Value(),op_strlen(he->Value()),1,f);
		fwrite("\n",1,1,f);
	}
	fwrite("\n",1,1,f);
}
void MultiPart_Parser::DebugSavePart(char* data, unsigned int len)
{
	fwrite(data,len,1,f);
}
void MultiPart_Parser::DebugEnd()
{
	fwrite("\n------Boundary--\n",18,1,f);
	fclose(f);
}
#endif
