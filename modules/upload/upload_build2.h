/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _UPLOAD_BUILDER_2_H_
#define _UPLOAD_BUILDER_2_H_

#include "modules/upload/upload.h"
#include "modules/upload/upload_build.h"
#include "modules/upload/upload_build_api.h"

class Upload_Builder : public Upload_Builder_Base
{
private:
	Multipart_Type	multipart_status;
	Upload_Multipart *multipart_message; // always == message if non-NULL
	Upload_Base		*message;

	BOOL			ignore_signrecip_error;

	Message_Type	message_type;

	Signer_List		signers;
	Recipient_List	recipients;

	SignatureLevel	signature_level;
	EncryptionLevel	encryption_level;

	OpString8		header_list;
	OpString8		character_set;

public:

	Upload_Builder();
	~Upload_Builder();

protected:
	Upload_Base *AddElementL(Upload_Base *elm);


public:
	/** Ignore missing password for signers and recipients, add them anyway */
	virtual void IgnoreSignerRecipientErrors(BOOL flag);

	/** Add the email as a signer */
	virtual OP_STATUS AddSignerL(const OpStringC8 &signer, const OpStringC8 & password, unsigned char *key_id, uint32 key_id_len);

	/** Remove the email as a signer */
	virtual OP_STATUS RemoveSigner(const OpStringC8 &signer);

	/** Add the email as a recipient */
	virtual OP_STATUS AddRecipientL(const OpStringC8 &signer, unsigned char *key_id, uint32 key_id_len);

	/** Remove the email as a receipient */
	virtual OP_STATUS RemoveRecipient(const OpStringC8 &signer);

	/** Returns the signature status level of the email */
	virtual SignatureLevel GetSignatureLevel();

	/** Returns the encryption level of the email */
	virtual EncryptionLevel GetEncryptionLevel();

	/** Input All headers into the message */
	virtual void SetHeadersL(const OpStringC8 &hdrs, uint32 len);

	/** Set the message's character set encoding*/
	virtual void SetCharsetL(const OpStringC8 &charset);

	/**
	 *	Change the encryption level of the message being built. 
	 *	Only relevant if receipients have been added 
	 */
	virtual void ForceEncryptionLevel(EncryptionLevel lvl);

	/** 
	 *	Change the signature level of the message being built.
	 *	Only relevant if signers have been added
	 */
	virtual void ForceSignatureLevel(SignatureLevel lvl);

	/**	configure this builder so that it builds a Multipart 
	 *	message of the given type, multipart/related should 
	 *	specify the related start content-id.
	 *
	 *	If not called, a message + attachement call sequence will
	 *	result in a multipart/mixed message
	 *
	 *	The returned pointer can be used for further actions, but MUST NOT 
	 *	be deleted, as the builder still owns the object.
	 */
	virtual Upload_Base *SetMultipartTypeL(Multipart_Type typ, const OpStringC8 &related_start=NULL);

	/**
	 *	Set up the message to use the given body as content
	 *
	 *	If a non-multipart body is already defined, the message will be 
	 *	converted to a multipart/mixed message
	 *
	 *	The returned pointer can be used for further actions, but MUST NOT 
	 *	be deleted, as the builder still owns the object.
	 */
	virtual Upload_Base *SetMessageL(const OpStringC8 &body, const OpStringC8 &content_type, const OpStringC8 &char_set);

	/**
	 *	Set up the message to use the given body as content (Unicode version)
	 *
	 *	If a non-multipart body is already defined, the message will be 
	 *	converted to a multipart/mixed message
	 *
	 *	The returned pointer can be used for further actions, but MUST NOT 
	 *	be deleted, as the builder still owns the object.
	 */
	virtual Upload_Base *SetMessageL(const OpStringC &body, const OpStringC8 &content_type, const OpStringC8 &char_set);

	/**
	 *	Set up the message to use the given url as content
	 *
	 *	If a non-multipart body is already defined, the message will be 
	 *	converted to a multipart/mixed message
	 *
	 *	The returned pointer can be used for further actions, but MUST NOT 
	 *	be deleted, as the builder still owns the object.
	 */
	virtual Upload_Base *AddAttachmentL(const OpStringC &url, const OpStringC &suggested_name);

	/**
	 *	Set up the message to use the given upload builder as content
	 *
	 *	NOTE: This object takes ownership of the attachement builder, and will delete it
	 *	DO NOT delete the attachment after calling this function.
	 *
	 *	If a non-multipart body is already defined, the message will be 
	 *	converted to a multipart/mixed message
	 */
	virtual void AddAttachmentL(Upload_Builder_Base *attachment);

	/**
	 *	Finalize the message, and return the final Upload_Base element to the caller.
	 *
	 *	If necessary this call will create the necessary encryption/signature elements.
	 *
	 *	NOTE: The returned object IS NOT prepared. Prepare MUST be performed separately.
	 *
	 *	NOTE: The caller takes ownership of the returned object, and MUST DELETE it.
	 *	The builder WILL NOT delete the returned object after Finalize has returned.
	 */
    virtual Upload_Base *FinalizeMessageL();
};

#endif // _UPLOAD_BUILDER_2_H_
