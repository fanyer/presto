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


#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"
#include "modules/mime/multipart_parser/text_mpp.h"
#include "modules/mime/multipart_parser/binary_mpp.h"

#include "modules/hardcore/mem/mem_man.h"

MIME_MultipartBase::MIME_MultipartBase(HeaderList &hdrs, URLType url_type)
: MIME_Decoder(hdrs, url_type)
{
	current_element = NULL;
	current_display_item = NULL;
	display_attachment_list = TRUE;
}

MIME_MultipartBase::~MIME_MultipartBase()
{
}

void MIME_MultipartBase::SaveDataInSubElementL()
{
	uint32 src_len = GetLengthDecodedData();

	SaveDataInSubElementL(AccessDecodedData(), src_len);
	unprocessed_decoded.CommitSampledBytesL(src_len);
}


void MIME_MultipartBase::SaveDataInSubElementL(const unsigned char *src, unsigned long src_len)
{
	if(!current_element)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	current_element->LoadDataL(src, src_len);
}

void MIME_MultipartBase::HandleFinishedL()
{
	FinishSubElementL();
}

void MIME_MultipartBase::WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target)
{
	for (MIME_Decoder* item = sub_elements.First(); item; item = item->Suc())
		item->RetrieveDataL(target, attach_target);
}

void MIME_MultipartBase::WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links)
{
	for (MIME_Decoder* item = sub_elements.First(); item; item = item->Suc())
	{
		item->SetBigAttachmentIcons(big_icons);
		item->RetrieveAttachementL(target, attach_target, display_as_links && display_attachment_list);
	}
}

BOOL MIME_MultipartBase::HaveDecodeWarnings()
{
	if(MIME_Decoder::HaveDecodeWarnings())
		return TRUE;

	for (MIME_Decoder* item = sub_elements.First(); item; item = item->Suc())
		if(item->HaveDecodeWarnings())
			return TRUE;

	return FALSE;
}

BOOL MIME_MultipartBase::HaveAttachments()
{
	for (MIME_Decoder* item = sub_elements.First(); item; item = item->Suc())
		if(item->HaveAttachments())
			return TRUE;

	return FALSE;
}

void MIME_MultipartBase::SetUseNoStoreFlag(BOOL no_store) 
{
	MIME_Decoder::SetUseNoStoreFlag(no_store);

	if(no_store)
	{
		for (MIME_Decoder* item = sub_elements.First(); item; item = item->Suc())
			item->SetUseNoStoreFlag(no_store);
	}
}

void MIME_MultipartBase::SetForceCharset(unsigned short charset_id)
{
	MIME_Decoder::SetForceCharset(charset_id);

	for (MIME_Decoder* item = sub_elements.First(); item; item = item->Suc())
		item->SetForceCharset(charset_id);
}

BOOL MIME_MultipartBase::ReferenceAll(OpVector<SubElementId>& ids)
{
	for (MIME_Decoder* item = sub_elements.First(); item; item = item->Suc())
		if (item->ReferenceAll(ids))
			return TRUE;

	return FALSE;
}

// Only performed when no output is generated
void MIME_MultipartBase::RetrieveAttachementList(DecodedMIME_Storage *attach_target)
{
	if(!attach_target)
		return;

	for (MIME_Decoder* item = sub_elements.First(); item; item = item->Suc())
		item->RetrieveAttachementList(attach_target);
}

MIME_Multipart_Decoder::MIME_Multipart_Decoder(HeaderList &hdrs, URLType url_type, BOOL binaryMultipart)
: MIME_MultipartBase(hdrs, url_type), binary(binaryMultipart), parser(NULL)
{
}

MIME_Multipart_Decoder::~MIME_Multipart_Decoder()
{
	OP_DELETE(parser);
}

void MIME_Multipart_Decoder::HandleHeaderLoadedL()
{
	MIME_MultipartBase::HandleHeaderLoadedL();

#ifdef WBMULTIPART_MIXED_SUPPORT
	if (binary)
	{
		parser = OP_NEW_L(BinaryMultiPartParser, (this));
		return;
	}
#endif

	const char* boundary = NULL;
	if(content_type_header)
	{
		ParameterList *parameters = content_type_header->SubParameters();

		OP_ASSERT(parameters && parameters->First());  // content_type_header != NULL  => parameters != NULL

		Parameters *param = parameters->GetParameterByID(HTTP_General_Tag_Boundary, PARAMETER_ASSIGNED_ONLY);
		if (param && param->Value())
			boundary = param->Value();
		else
			RaiseDecodeWarning();
	}
	parser = OP_NEW_L(TextMultiPartParser, (boundary, 0, this));
}

void MIME_Multipart_Decoder::ProcessDecodedDataL(BOOL more)
{
	OP_ASSERT(parser);

	unsigned long unprocessed_count = GetLengthDecodedData();
	const unsigned char *unprocessed_data = AccessDecodedData();
	if (unprocessed_count)
	{
		parser->load((const char*)unprocessed_data, (unsigned int)unprocessed_count);
		CommitDecodedDataL(unprocessed_count);
	}
	if (!more)
		parser->loadingFinished();

	do
	{
		if (!current_element)
		{
			if (NumberOfParts() >= 65536)
			{
				RaiseDecodeWarning();
				break;
			}

			if (!parser->beginNextPart())
				break;

			OpStackAutoPtr<HeaderList> headers(parser->getCurrentPartHeaders());
			CreateNewBodyPartL(*headers); // Sets current_element
		}

		while (current_element && (parser->availableCurrentPartData() || parser->noMoreCurrentPartData()))
		{
			if (parser->availableCurrentPartData())
			{
				char* partData;
				unsigned int partLength = parser->getCurrentPartDataPointer(partData);
				SaveDataInSubElementL((const unsigned char*)partData, partLength);
				parser->consumeCurrentPartData(partLength);
			}
			if (parser->noMoreCurrentPartData())
			{
				parser->finishCurrentPart();
				FinishSubElementL(); // Sets current_element=NULL
			}
		}
	}
	while (!current_element);
}

void MIME_Multipart_Decoder::OnMultiPartParserWarning(AbstractMultiPartParser::Warning reason, unsigned int offset, unsigned int part)
{
	if (reason == AbstractMultiPartParser::WARNING_OUT_OF_MEMORY)
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	RaiseDecodeWarning();
}

void MIME_Multipart_Decoder::HandleFinishedL()
{
	OP_ASSERT(parser);

	ProcessDecodedDataL(FALSE);
	if (parser->getState() != AbstractMultiPartParser::STATE_FINISHED_MULTIPART)
		RaiseDecodeWarning();
	
	MIME_MultipartBase::HandleFinishedL();
}

#endif // MIME_SUPPORT
