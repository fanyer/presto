/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _UPLOAD_BUILDER_H_
#define _UPLOAD_BUILDER_H_

#include "modules/datastream/fl_lib.h"

class OpUploadBuildStatus : public OpRecStatus
{
public:
	enum {
		NEED_PASSWORD		= REC_STATUS_ERROR +0,
		UNKNOWN_RECIPIENT	= REC_STATUS_ERROR +1,
		RECIPIENT_WRONG_KEYTYPE = REC_STATUS_ERROR +2, // S/MIME vs PGP conflict
		KEY_EXPIRED			= REC_STATUS_ERROR +3,
		KEY_UNTRUSTED		= REC_STATUS_ERROR +4,
		MULTIPLE_KEYS		= REC_STATUS_ERROR +5,
		UNKNOWN_SIGNER		= REC_STATUS_ERROR +6
	};
};

enum Multipart_Type {
	Multipart_None,
	Multipart_Normal,
	Multipart_Alternative,
	Multipart_Related
};

enum EncryptionLevel {
	Encryption_None,
	Encryption_Full
};

enum SignatureLevel {
	Signature_Unsigned,
	Signature_Signed
};

/** Upload_Builder_Base 
 *
 *	API for building a Upload element.
 *
 *	This class provides the necessary abstraction 
 *	for creating email messages without having to 
 *	take into consideration the encryption/signature 
 *	options that are available. Those decisions are 
 *	handled by the Upload_Builder_Base implementation
 *
 *	Usage:
 *
 *	1. Register all senders, if necessary request password 
 *	from the user, or specify that no signing should be done.
 *
 *	2. Register all recipients. 
 *
 *	3. Set all headers for the message. These must include all 
 *	From/To/Cc headers, as the registration in step 1 and 2 are 
 *	used to determine encryption policies.
 *
 *	4. Add each bodypart of the message (text and attachments). 
 *	Multiple Upload_Builder_Base elements can be used to create 
 *	complex structures. Only the topmost need to have step 1-3 
 *	performed on it.
 *
 *  5. Finalize the message. The completed, but unprepared, Upload_Base 
 *	is returned.
 *
 *	Steps 1-4 can be performed in any order, step 5 must always be the last step
 *
 */
class Upload_Builder_Base
{
public:
	/** Ignore missing password for signers and recipients, add them anyway */
	virtual void IgnoreSignerRecipientErrors(BOOL flag)=0;

	/** Add the email as a signer */
	virtual OP_STATUS AddSignerL(const OpStringC8 &signer, const OpStringC8 & password, unsigned char *key_id, uint32 key_id_len)=0;

	/** Remove the email as a signer */
	virtual OP_STATUS RemoveSigner(const OpStringC8 &signer)=0;

	/** Add the email as a recipient */
	virtual OP_STATUS AddRecipientL(const OpStringC8 &signer, unsigned char *key_id, uint32 key_id_len)=0;

	/** Remove the email as a receipient */
	virtual OP_STATUS RemoveRecipient(const OpStringC8 &signer)=0;

	/** Returns the signature status level of the email */
	virtual SignatureLevel GetSignatureLevel()=0;

	/** Returns the encryption level of the email */
	virtual EncryptionLevel GetEncryptionLevel()=0;

	/** Input All headers into the message */
	virtual void SetHeadersL(const OpStringC8 &hdrs, uint32 len)=0;

	/** Set the message's character set encoding*/
	virtual void SetCharsetL(const OpStringC8 &charset)=0;

	/**
	 *	Change the encryption level of the message being built. 
	 *	Only relevant if receipients have been added 
	 *
	 *	Presently, on/off are the only options
	 */
	virtual void ForceEncryptionLevel(EncryptionLevel lvl)=0;

	/** 
	 *	Change the signature level of the message being built.
	 *	Only relevant if signers have been added
	 *
	 *	Presently, on/off are the only options
	 */
	virtual void ForceSignatureLevel(SignatureLevel lvl)=0;

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
	virtual Upload_Base *SetMultipartTypeL(Multipart_Type typ, const OpStringC8 &related_start=NULL)=0;

	/**
	 *	Set up the message to use the given body as content
	 *
	 *	If a non-multipart body is already defined, the message will be 
	 *	converted to a multipart/mixed message
	 *
	 *	The returned pointer can be used for further actions, but MUST NOT 
	 *	be deleted, as the builder still owns the object.
	 */
	virtual Upload_Base *SetMessageL(const OpStringC8 &body, const OpStringC8 &content_type, const OpStringC8 &char_set)=0;

	/**
	 *	Set up the message to use the given body as content (Unicode version)
	 *
	 *	If a non-multipart body is already defined, the message will be 
	 *	converted to a multipart/mixed message
	 *
	 *	The returned pointer can be used for further actions, but MUST NOT 
	 *	be deleted, as the builder still owns the object.
	 */
	virtual Upload_Base *SetMessageL(const OpStringC &body, const OpStringC8 &content_type, const OpStringC8 &char_set)=0;

	/**
	 *	Set up the message to use the given url as content
	 *
	 *	If a non-multipart body is already defined, the message will be 
	 *	converted to a multipart/mixed message
	 *
	 *	The returned pointer can be used for further actions, but MUST NOT 
	 *	be deleted, as the builder still owns the object.
	 */
	virtual Upload_Base *AddAttachmentL(const OpStringC &url, const OpStringC &suggested_name)=0;

	/**
	 *	Set up the message to use the given upload builder as content
	 *
	 *	NOTE: This object takes ownership of the attachement builder, and will delete it
	 *	DO NOT delete the attachment after calling this function.
	 *
	 *	If a non-multipart body is already defined, the message will be 
	 *	converted to a multipart/mixed message
	 */
	virtual void AddAttachmentL(Upload_Builder_Base *attachment)=0;

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
    virtual Upload_Base *FinalizeMessageL()=0;
};

Upload_Builder_Base *CreateUploadBuilderL();

#endif // _UPLOAD_BUILDER_H_
