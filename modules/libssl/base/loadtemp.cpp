/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/sslbase.h"

#define DECLARE_LOADANDWRITABLE_SPEC 	DataRecord_Spec LoadAndWritableList_spec(FALSE, 1, TRUE, FALSE, FALSE, 2, TRUE, TRUE);

LoadAndWritableList::LoadAndWritableList()
{
	DECLARE_LOADANDWRITABLE_SPEC;

	SetRecordSpec(LoadAndWritableList_spec);
}

#if 0
LoadAndWritableList::LoadAndWritableList(uint32 size)
{
	DECLARE_LOADANDWRITABLE_SPEC;

	SetRecordSpec(LoadAndWritableList_spec);
}
#endif

LoadAndWritableList::~LoadAndWritableList()
{
	//ClearList();
}


BOOL LoadAndWritableList::Valid(SSL_Alert *msg) const
{
	if(!SSL_Error_Status::Valid(msg))
		return FALSE;

	if(!((LoadAndWritableList *) this)->Finished())
	{
		if(msg != NULL)
			msg->Set(SSL_Internal, SSL_InternalError);
		return FALSE;
	}

	return TRUE;
}

void LoadAndWritableList::AddItem(DataStream *item)
{
	if(item)
	{
		if(item->InList())
			item->Out();

		item->Into(this); 
	}
}

void LoadAndWritableList::AddItem(LoadAndWritableList *item)
{
	if(item)
	{
		if(item->InList())
			item->Out();

		item->Into(this); 
		item->ForwardTo(this);
	}
}

uint32 LoadAndWritableList::Size() const
{
	return ((LoadAndWritableList *) this)->CalculateLength();
}

void LoadAndWritableList::SetUpMessageL(SSL_ConnectionState *pending)
{
}

SSL_KEA_ACTION LoadAndWritableList::ProcessMessage(SSL_ConnectionState *pending)
{
	return SSL_KEA_No_Action;
}

/*
template<class T>
LoadAndWritablePointer<T>::LoadAndWritablePointer(T *val) 
	: item(NULL)
{
	SetItem(val);
};

template<class T>
void LoadAndWritablePointer<T>::SetItem(T *val)
{
	if(item && item->InList())
		item->Out();
	item = val;
	AddItem(item);
}
*/

//template class LoadAndWritablePointer<class LoadAndWritableList>;

#endif // _NATIVE_SSL_SUPPORT_
