/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WBXML_DECODER_H
#define WBXML_DECODER_H

#include "modules/url/protocols/http_te.h"

class WBXML_Parser;

class WBXML_Decoder : public Data_Decoder
{
private:

	WBXML_Parser *m_parser;
	char*			m_src_buf;
	UINT32			m_src_buf_pos;
	UINT32			m_src_buf_len;

public:
	WBXML_Decoder();
	virtual ~WBXML_Decoder();

#ifdef OOM_SAFE_API
	virtual unsigned int ReadDataL(char *buf, unsigned int blen, URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more);
#else
	virtual unsigned int ReadData(char *buf, unsigned int blen, URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more);
#endif //OOM_SAFE_API

	/** Get character set associated with this Data_Decoder.
	  * @return NULL if no character set is associated
	  */
	virtual const char *GetCharacterSet();
	/** Get character set detected for this CharacterDecoder.
	  * @return Name of character set, NULL if no character set can be
	  *         determined.
	  */
	virtual const char *GetDetectedCharacterSet();
	/** Stop guessing character set for this document.
	  * Character set detection wastes cycles, and avoiding it if possible
	  * is nice.
	  */
	virtual void StopGuessingCharset();

	BOOL	IsWml();

	virtual BOOL	IsA(const uni_char *type);
};

#endif // WBXML_DECODER_H
