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

#ifndef __SSLLOADTEMP_H
#define __SSLLOADTEMP_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/datastream/fl_rec.h"
#include "modules/libssl/base/sslerr.h"


class LoadAndWritableList : public DataStream_BaseRecord, public SSL_Error_Status
{
protected:
    LoadAndWritableList(const LoadAndWritableList &); // Nonexistent
public:
    LoadAndWritableList();
    LoadAndWritableList(uint32 size);
	virtual ~LoadAndWritableList();

	uint32 Size() const;
    virtual BOOL Valid(SSL_Alert *msg = NULL) const; 

	void AddItem(DataStream *item); /** Does not set ForwardTo */
	void AddItem(DataStream &item){AddItem(&item);}; /** Does not set ForwardTo */
	void AddItem(LoadAndWritableList *item); /** Also sets ForwardTo */
	void AddItem(LoadAndWritableList &item){AddItem(&item);} /** Also sets ForwardTo */

	virtual void SetUpMessageL(SSL_ConnectionState *pending);
	virtual SSL_KEA_ACTION ProcessMessage(SSL_ConnectionState *pending);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "LoadAndWritableList";}
#endif
};


/** T MUST support the functions Load, Write, FinishedLoading. Size and 
 *  Valid in the same manner as class LoadAndWritable (preferably as a subclass)
 */
template<class T> class LoadAndWritablePointer : public LoadAndWritableList
{
private:
	T	*item;

private:
	void SetItem(T *val){OP_ASSERT(!item || item->InList()); if(item){OP_ASSERT(Head::HasLink(item)); item->Out();} OP_ASSERT(Head::Cardinal()==2); item = val; LoadAndWritableList::AddItem(item);}

public:
	LoadAndWritablePointer(T *val=NULL):item(val){LoadAndWritableList::AddItem(item);};

	operator T *(){return item;};
	operator const T *() const{return item;};
	T *operator ->(){return item;}

	LoadAndWritablePointer &operator =(T *val){SetItem(val); return *this;}
	int operator !=(const T *val) const{return item != val;}
	int operator ==(const T *val) const{return item == val;}

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "LoadAndWritablePointer";}
#endif
};

#endif
#endif
