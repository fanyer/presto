/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

// ----------------------------------------------------

#ifndef M2MIME_H
#define M2MIME_H

// ----------------------------------------------------

//#include "core/pch.h"

#include "adjunct/m2/src/include/defs.h"

#include "modules/upload/upload.h" // Upload_Element_Type
#include "modules/url/url_man.h"
#include "modules/encodings/decoders/utf7-decoder.h"
#include "modules/encodings/encoders/utf7-encoder.h"

// ----------------------------------------------------

class CharConverter;
class UTF16toUTF8Converter;
class UTF8toUTF16Converter;
//class UTF16toUTF7Converter;
//class UTF7toUTF16Converter;

class Upload_Base;
enum Upload_Element_Type;
class Upload_Builder_Base;
// ----------------------------------------------------

// handles signature verifying
// mime decodeing
// decryption

class Decoder
{
public:
	// TODO: We have to make sure this url is really deleted 
	virtual ~Decoder() { m_decoder_url->Unload(); OP_DELETE(m_decoder_url); }

	class MessageListener
	{
	public:
        virtual ~MessageListener() {}
        virtual OP_STATUS OnDecodedMultipart(URL* url, const OpStringC8& charset, const OpStringC& filename,
                                             int size, BYTE* data, BOOL is_mailbody, BOOL is_visible) = 0;

        virtual void      OnFinishedDecodingMultiparts(OP_STATUS status) = 0;

		virtual void	  OnRestartDecoding() = 0;
    };

private:
    BOOL			m_ignore_content;
    BOOL			m_prefer_plaintext;
	URL *			m_decoder_url;

public:
    
	Decoder() 
	: m_ignore_content(FALSE)
	, m_prefer_plaintext(TRUE)
	, m_decoder_url(NULL)
	{
	}

    virtual void      DecodingStopped();
    virtual void      SignalReDecodeMessage();
	virtual OP_STATUS DecodeMessage(const char* headers, const char* body, UINT32 message_size, BOOL ignore_content, BOOL body_only, BOOL prefer_plaintext, BOOL ignore_warnings, UINT32 id, MessageListener* listener);
	virtual OP_STATUS DecodeSuburl(URL& suburl, BOOL& found_body, BOOL ignore_content, BOOL body_only, MessageListener* listener, BOOL is_visible);
};

class MimeUtils
{
public:
    virtual ~MimeUtils() {}

	// called from OString

	virtual OP_STATUS RemoveHeaderEscapes(const char* charset,
		                                  const char *&source,
									      OpString16& target) = 0;

	// UTF8toUTF16Converter
#ifdef _DEBUG
	virtual UTF8toUTF16Converter* CreateUTF8toUTF16ConverterDbg(const OpStringC8& source_file, int source_line) = 0;
#else
	virtual UTF8toUTF16Converter* CreateUTF8toUTF16Converter() = 0;
#endif
	virtual void DeleteUTF8toUTF16Converter(UTF8toUTF16Converter*) = 0;

	// UTF16toUTF8Converter
#ifdef _DEBUG
	virtual UTF16toUTF8Converter* CreateUTF16toUTF8ConverterDbg(const OpStringC8& source_file, int source_line) = 0;
#else
	virtual UTF16toUTF8Converter* CreateUTF16toUTF8Converter() = 0;
#endif
	virtual void DeleteUTF16toUTF8Converter(UTF16toUTF8Converter*) = 0;

	// UTF7toUTF16Converter
#ifdef _DEBUG
	virtual UTF7toUTF16Converter* CreateUTF7toUTF16ConverterDbg(UTF7toUTF16Converter::utf7_variant variant, const OpStringC8& source_file, int source_line) = 0;
#else
	virtual UTF7toUTF16Converter* CreateUTF7toUTF16Converter(UTF7toUTF16Converter::utf7_variant variant) = 0;
#endif
	virtual void DeleteUTF7toUTF16Converter(UTF7toUTF16Converter*) = 0;

	// UTF16toUTF7Converter
#ifdef _DEBUG
	virtual UTF16toUTF7Converter* CreateUTF16toUTF7ConverterDbg(UTF16toUTF7Converter::utf7_variant variant, const OpStringC8& source_file, int source_line) = 0;
#else
	virtual UTF16toUTF7Converter* CreateUTF16toUTF7Converter(UTF16toUTF7Converter::utf7_variant variant) = 0;
#endif
	virtual void DeleteUTF16toUTF7Converter(UTF16toUTF7Converter*) = 0;


	// Upload_Element is the "Encoder"; handles signing, mime encoding and encryption


#if !defined __SUPPORT_OPENPGP__
	virtual Upload_Base*	CreateUploadElement(Upload_Element_Type typ) = 0;
#else
	virtual Upload_Builder_Base*	CreateUploadBuilder() = 0;
	virtual void            DeleteUploadBuilder(Upload_Builder_Base* builder) = 0;
#endif
	virtual void            DeleteUploadElement(Upload_Base* element) = 0;

	// "Decoder"
	virtual Decoder* CreateDecoder() = 0;
	virtual void     DeleteDecoder(Decoder* decoder) = 0;

};

// ----------------------------------------------------

#endif // M2MIME_H

// ----------------------------------------------------
