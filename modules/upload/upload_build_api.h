/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _UPLOAD_BUILDER_API_H_
#define _UPLOAD_BUILDER_API_H_

#include "modules/libssl/sslv3.h"

enum Message_Type {
	Message_Plain,
	Message_PGP,
	Message_SMIME
};

class Signer_Item : public Link
{
public:
	Message_Type keytype;
	OpString8 signer;
	OpString8 password;

	SSL_varvector32 keyid;

public:

	Signer_Item(Message_Type typ);
	~Signer_Item();

	void ConstructL(const OpStringC8 &signer, const OpStringC8 & password, unsigned char *key_id, uint32 key_id_len);

	Signer_Item *Suc(){return (Signer_Item *) Link::Suc();}
	Signer_Item *Pred(){return (Signer_Item *) Link::Pred();}
};

class Signer_List : public Head
{
public:
	~Signer_List();

	void AddItemL(Message_Type typ, const OpStringC8 &signer, const OpStringC8 & password, unsigned char *key_id, uint32 key_id_len);
	void RemoveItem(const OpStringC8 &signer);

	Signer_Item *First(){return (Signer_Item *) Head::First();}
	Signer_Item *Last(){return (Signer_Item *) Head::Last();}
};

class Recipient_Item : public Link
{
public:
	Message_Type keytype;
	OpString8 recipient;

	SSL_varvector32 keyid;

public:

	Recipient_Item(Message_Type typ);
	~Recipient_Item();

	void ConstructL(const OpStringC8 &recipient, unsigned char *key_id, uint32 key_id_len);

	Recipient_Item *Suc(){return (Recipient_Item *) Link::Suc();}
	Recipient_Item *Pred(){return (Recipient_Item *) Link::Pred();}
};

class Recipient_List : public Head
{
public:
	~Recipient_List();

	void AddItemL(Message_Type typ, const OpStringC8 &recipient, unsigned char *key_id, uint32 key_id_len);
	void RemoveItem(const OpStringC8 &recp);

	Recipient_Item *First(){return (Recipient_Item *) Head::First();}
	Recipient_Item *Last(){return (Recipient_Item *) Head::Last();}
};

#endif // _UPLOAD_BUILDER_API_H_
