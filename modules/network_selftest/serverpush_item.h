/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve N. Pettersen
*/

#ifndef SERVERPUSH_ITEM_H
#define SERVERPUSH_ITEM_H

#ifdef SELFTEST

class ServerPush_Filename_Item : public Link
{
private:
	int item;
	OpString	filename;

public:

	ServerPush_Filename_Item():item(0){};
	virtual ~ServerPush_Filename_Item(){if(InList()) Out();};

	OP_STATUS Construct(const OpStringC8 &a_filename);

	int GetItem() const{return item;}
	const OpStringC &GetName() const{return filename;}
	
	ServerPush_Filename_Item *Suc(){return (ServerPush_Filename_Item *) Link::Suc();}
	ServerPush_Filename_Item *Pred(){return (ServerPush_Filename_Item *) Link::Pred();}
};

class ServerPush_Filename_List : public AutoDeleteHead
{
public:
	OP_STATUS AddItem(const OpStringC8 &a_filename);

	ServerPush_Filename_Item *First(){return (ServerPush_Filename_Item *) AutoDeleteHead::First();}
};


#endif  // SELFTEST
#endif  // SERVERPUSH_ITEM_H

