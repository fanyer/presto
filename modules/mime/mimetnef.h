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

#ifndef _MIMETNEF_H_
#define _MIMETNEF_H_

#define TNEF_SIGNATURE 0x223E9F78

#define TNEF_MESSAGE 0x01
#define TNEF_ATTACHMENT 0x02

#define TNEF_VERSION_TYPE 0x00089006
#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"


class MS_TNEF_Decoder : public MIME_MultipartBase
{
private:
	struct ms_tnef_info{
		UINT no_processing:1;
		UINT read_header:1;
		UINT read_key:1;

		UINT attachment_part:1;
		UINT read_attach_rend:1;

		UINT record_class_read:1;
		UINT record_type_read:1;
		UINT record_read_length:1;
		UINT record_reading_record:1;
		UINT record_data_to_subelement:1;
		UINT record_read_checksum:1;

		UINT record_wait_for_all:1;
	} tnef_info;

	uint32 current_record_type;
	uint32 record_length;
	unsigned long record_read;
	unsigned short record_checksum;

	OpString	filename;
	OpString8	mime_type;
	DataStream_FIFO_Stream*	unnamed_contents;

private:
	void Init();

protected:
	virtual void ProcessDecodedDataL(BOOL no_more);
	void HandleUnnamedContentsL();
	void SaveUnnamedContentsL();

public:

	MS_TNEF_Decoder(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT); // Header elements are moved (not copied) to this element
	virtual ~MS_TNEF_Decoder();

};
#endif // _MIME_SUPPORT_
#endif // !_MIMETNEF_H_
