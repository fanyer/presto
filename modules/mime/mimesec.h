/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/



#ifndef __MIMESEC_H__
#define __MIMESEC_H__

#include "modules/libssl/sslv3.h"
#include "modules/mime/mime_lib.h"
#include "modules/cache/url_stream.h"


class MIME_Signed_MIME_Content : public MIME_Mime_Payload, public MIME_Signed_Processor
{
public:
	MIME_Signed_MIME_Content(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	~MIME_Signed_MIME_Content();

protected:
	void PerformSpecialProcessingL(const unsigned char *src, unsigned long src_len);
};

class MIME_Signed_Text_Content : public MIME_Unprocessed_Text, public MIME_Signed_Processor
{
public:
	MIME_Signed_Text_Content(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	~MIME_Signed_Text_Content();

protected:
	void PerformSpecialProcessingL(const unsigned char *src, unsigned long src_len);
};

class MIME_KeyName : public ListElement<MIME_KeyName>
{
public:
	OpString16 keyname;
	byte KeyId[8];  /* ARRAY OK 2009-05-15 roarl */

	MIME_KeyName();
	~MIME_KeyName();
};

typedef AutoDeleteList<MIME_KeyName> MIME_KeyNameList;

class MIME_Security_Body : public MIME_MultipartBase, public MessageObject
{
protected:
	Hash_Head mime_clearsigned_md;
	DataStream *datasource;
	BOOL delay_processing;
	BOOL payload_is_mime;

	BOOL displayed_header;

	OpString8 password;

public:
	MIME_Security_Body(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT); // Headers must be set separately
	~MIME_Security_Body();

	void SetCalculatedDigestL(Hash_Head *);
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	virtual void VerifySignaturesL()=0;
	virtual void DecryptL()=0;

	void DelayProcessing(){delay_processing = TRUE;}
	void SetPayloadIsMIME(){payload_is_mime = TRUE;}
	virtual void WriteSignatureInfoL(URL &target, DecodedMIME_Storage *attach_target)=0;
	virtual void WriteEncryptionInfoL(URL &target, DecodedMIME_Storage *attach_target)=0;
protected:

	BOOL CheckSourceL();
	OP_STATUS AskPasswordL(Str::LocaleString msg_id, MIME_KeyNameList &names);

	virtual void ProcessDecodedDataL(BOOL more);

	void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *storage);
	virtual void WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links=TRUE);
};

class MIME_Multipart_Encrypted_Parameters : public MIME_Decoder
{
private:
	HeaderList	headers;
	OpString8	header_string;

public:
	MIME_Multipart_Encrypted_Parameters(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	~MIME_Multipart_Encrypted_Parameters();

	virtual MIME_Security_Body *SetUpEncryptedDecoderL(HeaderList &hdrs, URLType url_type)=0;

protected:
	virtual void ProcessDecodedDataL(BOOL no_more);
	virtual void HandleFinishedL();

	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
	virtual void WriteDisplayAttachmentsL(URL &target, DecodedMIME_Storage *attach_target, BOOL display_as_links=TRUE);
};


class MIME_Multipart_Signed_Decoder : public MIME_Multipart_Decoder
{
private:
	MIME_ContentTypeID	signature_type;
	SSL_HashAlgorithmType mic_alg;

	MIME_Signed_Processor *signed_content_processor; // The object is in the multipart list. DO NOT delete

	MIME_Security_Body *signature_body;
	
public:
	MIME_Multipart_Signed_Decoder(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	~MIME_Multipart_Signed_Decoder();

protected:
	virtual void HandleHeaderLoadedL();
	virtual MIME_Decoder *CreateNewBodyPartL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type);
	virtual void HandleFinishedL();
	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
	virtual void CreateNewBodyPartL(const unsigned char *src, unsigned long src_len); // src contains only the header. The header MUST be complete
};


class MIME_Multipart_Encrypted_Decoder : public MIME_Multipart_Decoder
{
private:
	MIME_ContentTypeID	encryption_type;

	MIME_Multipart_Encrypted_Parameters *parameters;
	MIME_Security_Body *encrypted_body;
	
public:
	MIME_Multipart_Encrypted_Decoder(HeaderList &hdrs, URLType url_type=URL_ATTACHMENT);
	~MIME_Multipart_Encrypted_Decoder();

protected:
	virtual void HandleHeaderLoadedL();
	virtual MIME_Decoder *CreateNewBodyPartL(MIME_ContentTypeID id, HeaderList &hdrs, URLType url_type);
	virtual void HandleFinishedL();
	virtual void WriteDisplayDocumentL(URL &target, DecodedMIME_Storage *attach_target);
};



#endif
