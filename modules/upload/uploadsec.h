/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "modules/upload/upload.h"
#include "modules/mime/mime_lib.h"

class Upload_Secure_Base : public Upload_EncapsulatedElement
{
public:

	Upload_Secure_Base();
	~Upload_Secure_Base();

	void InitL(Upload_Base *elm, const OpStringC8 &content_type = NULL, MIME_Encoding enc=ENCODING_AUTODETECT, const char **header_names=NULL);

	virtual const char *GetSignatureDigestAlg()=0;

	virtual void GenerateSignatureL(Hash_Head &)=0;
};

class Upload_Signature_Generator : public Upload_EncapsulatedElement, public MIME_Signed_Processor
{
private:
	BOOL prepared;

public:
	Upload_Signature_Generator();
	~Upload_Signature_Generator();

	void InitL(Upload_Base *elm);

	virtual uint32 PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions);

protected:
	void PerformPayloadActionL(unsigned char *src, uint32 src_len);
};


class Upload_Multipart_Signed : public Upload_Multipart
{
protected:
	Upload_Signature_Generator *signed_content;
	Upload_Secure_Base *signature;
	BOOL prepared;

public:

	Upload_Multipart_Signed();
	~Upload_Multipart_Signed();

	void InitL(const OpStringC8 &content_type = NULL, const char **header_names=NULL);

	void AddContentL(Upload_Base *signd_content);
	void AddSignatureL(Upload_Secure_Base *sig);

	/** Prepares content, returns minimum bufferlength required to store the headers */
	virtual uint32 PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions);
};

class Upload_Multipart_Encrypted : public Upload_Multipart
{
	Upload_Secure_Base *encrypted_content;
	BOOL prepared;

public:

	Upload_Multipart_Encrypted();
	~Upload_Multipart_Encrypted();

	void InitL(Upload_Base *encrypted_parameters, Upload_Secure_Base *encrypted_content, const OpStringC8 &content_type = NULL, const char **header_names=NULL);

	/** Prepares content, returns minimum bufferlength required to store the headers */
	virtual uint32 PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions);
};
