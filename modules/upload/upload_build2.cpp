/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _MIME_SEC_ENABLED_


#include "modules/util/str.h"
#include "modules/upload/upload_build2.h"

#ifdef _SUPPORT_OPENPGP_
#include "modules/libopenpgp/pgp_build_api.h"
#endif

static const KeywordIndex Upload_untrusted_headers_ContentType[] = {
	{NULL, FALSE},
	{"Content-Type",TRUE}
};

static const char *UploadHeaders[] = {
	"Date",
	"To",
	"Subject",
	"Reply-To",
	"From",
	"Organization",
	"Cc",
	"Content-Type",
	"Message-ID",
	"User-Agent",
	"MIME-Version",
	NULL
};

Upload_Builder_Base *CreateUploadBuilderL()
{
	return OP_NEW_L(Upload_Builder, ());
}


Signer_Item::Signer_Item(Message_Type typ)
: keytype(typ)
{
}

Signer_Item::~Signer_Item()
{
	password.Wipe();
	if(InList())
		Out();
}

void Signer_Item::ConstructL(const OpStringC8 &signr, const OpStringC8 & passwrd, unsigned char *key_id, uint32 key_id_len)
{
	signer.SetL(signr);
	password.SetL(passwrd);
	keyid.Set(key_id, key_id_len);
	SSL_Alert err;
	if(keyid.Error(&err))
		LEAVE(err.GetOPStatus());
}

Signer_List::~Signer_List()
{
	Clear();
}

void Signer_List::AddItemL(Message_Type typ, const OpStringC8 &signer, const OpStringC8 & password, unsigned char *key_id, uint32 key_id_len)
{
	OpStackAutoPtr<Signer_Item> item(OP_NEW_L(Signer_Item, (typ))); 

	item->ConstructL(signer, password, key_id, key_id_len);
	
	item->Into(this);

	item.release();
}

void Signer_List::RemoveItem(const OpStringC8 &signer)
{
	Signer_Item *item, *next_item;

	next_item = First();
	while(next_item)
	{
		item = next_item;
		next_item = next_item->Suc();

		if(item->signer.CompareI(signer) == 0)
			OP_DELETE(item); // automatically removed from the list
	}
}

Recipient_Item::Recipient_Item(Message_Type typ)
: keytype(typ)
{
}

Recipient_Item::~Recipient_Item()
{
}

void Recipient_Item::ConstructL(const OpStringC8 &recipnt, unsigned char *key_id, uint32 key_id_len)
{
	recipient.SetL(recipnt);
	keyid.Set(key_id, key_id_len);
	SSL_Alert err;
	if(keyid.Error(&err))
		LEAVE(err.GetOPStatus());
}

Recipient_List::~Recipient_List()
{
	Clear();
}

void Recipient_List::AddItemL(Message_Type typ, const OpStringC8 &recipient, unsigned char *key_id, uint32 key_id_len)
{
	OpStackAutoPtr<Recipient_Item> item(OP_NEW_L(Recipient_Item, (typ))); 

	item->ConstructL(recipient, key_id, key_id_len);
	
	item->Into(this);

	item.release();
}


void Recipient_List::RemoveItem(const OpStringC8 &recipent)
{
	Recipient_Item *item, *next_item;

	next_item = First();
	while(next_item)
	{
		item = next_item;
		next_item = next_item->Suc();

		if(item->recipient.CompareI(recipent) == 0)
			OP_DELETE(item); // automatically removed from the list
	}
}


Upload_Builder::Upload_Builder()
:	multipart_status(Multipart_None), 
	multipart_message(NULL),
	message(NULL), 
	ignore_signrecip_error(FALSE), 
	message_type(Message_Plain),
	signature_level(Signature_Unsigned),
	encryption_level(Encryption_None)
{
}

Upload_Builder::~Upload_Builder()
{
	OP_DELETE(message);
	message = NULL;
}

void Upload_Builder::IgnoreSignerRecipientErrors(BOOL flag)
{
	ignore_signrecip_error = flag;
}

OP_STATUS Upload_Builder::AddSignerL(const OpStringC8 &signer, const OpStringC8 & password, unsigned char *key_id, uint32 key_id_len)
{
#ifdef _SUPPORT_OPENPGP_
	extern BOOL PGP_SignerExists(const OpStringC8 &signer, unsigned char *key_id, uint32 key_id_len);
	BOOL found = FALSE;

	if(!(found = PGP_SignerExists(signer, key_id, key_id_len)) && !ignore_signrecip_error)
			return OpUploadBuildStatus::UNKNOWN_SIGNER;

	

	signers.AddItemL((found ? Message_PGP : Message_Plain), signer, password, key_id, key_id_len);

	if(found)
	{
		message_type = Message_PGP;
		signature_level = Signature_Signed;
	}
#endif

	return OpUploadBuildStatus::OK;
}

OP_STATUS Upload_Builder::RemoveSigner(const OpStringC8 &signer)
{
	signers.RemoveItem(signer);

	signature_level = Signature_Unsigned;

	Signer_Item *item = signers.First();
	while(item)
	{
		if(item->keytype != Message_Plain)
			signature_level = Signature_Signed;

		item = item->Suc();
	}

	return OpUploadBuildStatus::OK;
}

OP_STATUS Upload_Builder::AddRecipientL(const OpStringC8 &recp, unsigned char *key_id, uint32 key_id_len)
{
	BOOL found = FALSE;
#ifdef _SUPPORT_OPENPGP_
	extern BOOL PGP_RecipientExists(const OpStringC8 &recp, unsigned char *key_id, uint32 key_id_len);

	if(!(found = PGP_RecipientExists(recp, key_id, key_id_len)) && !ignore_signrecip_error)
		return OpUploadBuildStatus::UNKNOWN_SIGNER;
	
	recipients.AddItemL((found ? Message_PGP : Message_Plain), recp, key_id, key_id_len);

	if(found)
	{
		message_type = Message_PGP;
		encryption_level = Encryption_Full;
	}
#endif

	return (found ? OpUploadBuildStatus::OK : OpUploadBuildStatus::UNKNOWN_SIGNER);
}

OP_STATUS Upload_Builder::RemoveRecipient(const OpStringC8 &recp)
{
	recipients.RemoveItem(recp);

	encryption_level = Encryption_None;

	Recipient_Item *item= recipients.First();
	while(item)
	{
		if(item->keytype != Message_Plain)
			encryption_level = Encryption_Full;
		item = item->Suc();
	}

	return OpUploadBuildStatus::OK;
}

SignatureLevel Upload_Builder::GetSignatureLevel()
{
	return signature_level;
}

EncryptionLevel Upload_Builder::GetEncryptionLevel()
{
	return encryption_level;
}

void Upload_Builder::SetHeadersL(const OpStringC8 &hdrs, uint32 len)
{
	header_list.SetL(hdrs, len);
}

void Upload_Builder::SetCharsetL(const OpStringC8 &charset)
{
	character_set.SetL(charset);
}

void Upload_Builder::ForceEncryptionLevel(EncryptionLevel lvl)
{
	encryption_level = lvl;
}

void Upload_Builder::ForceSignatureLevel(SignatureLevel lvl)
{
	signature_level = lvl;
}

Upload_Base *Upload_Builder::SetMultipartTypeL(Multipart_Type typ, const OpStringC8 &related_start)
{
	if(multipart_message)
		return message;

	multipart_status = typ;

	OpStackAutoPtr<Upload_Multipart> mult(OP_NEW_L(Upload_Multipart, ()));

	switch(multipart_status)
	{
	case Multipart_Normal:
		mult->InitL("multipart/mixed", UploadHeaders);
		break;
	case Multipart_Alternative:
		mult->InitL("multipart/alternative", UploadHeaders);
		break;
	case Multipart_Related:
		mult->InitL("multipart/related", UploadHeaders);
		if(related_start.HasContent())
			mult->AccessHeaders().AddParameterL("Content-Type", "start", related_start);
		break;
	}

	multipart_message = mult.release();

	if(message != NULL)
		multipart_message->AddElement(message);

	message = multipart_message;

	return message;
}

Upload_Base *Upload_Builder::SetMessageL(const OpStringC8 &body, const OpStringC8 &content_type, const OpStringC8 &char_set)
{
	OpStackAutoPtr<Upload_OpString8> elm(OP_NEW_L(Upload_OpString8, ()));

	elm->InitL(body, content_type, char_set, ENCODING_AUTODETECT, UploadHeaders);

	return AddElementL(elm.release());
}

Upload_Base *Upload_Builder::SetMessageL(const OpStringC &body, const OpStringC8 &content_type, const OpStringC8 &char_set)
{
#ifdef UPLOAD2_OPSTRING16_SUPPORT
	OpStackAutoPtr<Upload_OpString16> elm(OP_NEW_L(Upload_OpString16, ()));

	elm->InitL(body, content_type, char_set, ENCODING_AUTODETECT, UploadHeaders);

	return AddElementL(elm.release);
#else
	LEAVE(OpRecStatus::UNSUPPORTED_FORMAT);
	return NULL;
#endif
}

Upload_Base *Upload_Builder::AddAttachmentL(const OpStringC &url, const OpStringC &suggested_name)
{
	OpStackAutoPtr<Upload_URL> elm(OP_NEW_L(Upload_URL, ()));

	elm->InitL(url, suggested_name, NULL, ENCODING_AUTODETECT, UploadHeaders);

	return AddElementL(elm.release());
}

void Upload_Builder::AddAttachmentL(Upload_Builder_Base *att)
{
	if(att == NULL)
		return;

	OpStackAutoPtr<Upload_Builder_Base> attachment(att);

	OpStackAutoPtr<Upload_Base> elm(attachment->FinalizeMessageL());
	
	attachment.reset();

	if(!elm.get())
		return;

	AddElementL(elm.release());
}


Upload_Base *Upload_Builder::FinalizeMessageL()
{
	if(message)
		message->SetCharsetL(character_set);

#ifdef _SUPPORT_OPENPGP_
	if(message && message_type == Message_PGP)
	{
		Upload_Base *mess_temp = message;
		message = NULL;
		multipart_message = NULL;

		message = PGP_BuildUploadL(mess_temp, (signature_level != Signature_Unsigned ? &signers : NULL), (encryption_level != Encryption_None ? &recipients : NULL));
	}
#endif

	if(message)
	{
		message->ExtractHeadersL((unsigned char *) header_list.CStr(), header_list.Length() , TRUE, HEADER_REMOVE_BCC, Upload_untrusted_headers_ContentType, ARRAY_SIZE(Upload_untrusted_headers_ContentType));
	}

	Upload_Base *temp = message;

	message = NULL;

	return temp;
}


Upload_Base *Upload_Builder::AddElementL(Upload_Base *elm)
{
	if(elm == NULL)
		return NULL;

	OpStackAutoPtr<Upload_Base> element(elm);

	if(message != NULL)
	{
		SetMultipartTypeL(Multipart_Normal);

		multipart_message->AddElement(element.release());
	}
	else
		message = element.release();

	return elm;
}

#endif // _MIME_SEC_ENABLED_
