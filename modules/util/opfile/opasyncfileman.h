/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Robert Kacirek
*/

#ifndef MODULES_UTIL_OPFILE_OP_ASYNC_FILEMAN_H
#define MODULES_UTIL_OPFILE_OP_ASYNC_FILEMAN_H

#ifdef UTIL_HAVE_ASYNC_FILEMAN

#include "modules/util/simset.h"

class MOpAsyncFileManObserver
{
public:
    virtual void OnActionDone(OP_STATUS status, int id)=0;
};

class OpAsyncManObserverLinkItem: public Link
{
public:
    MOpAsyncFileManObserver* m_observer;
};

class OpAsyncFileManBase
{
public:
    inline virtual ~OpAsyncFileManBase();

    inline OP_STATUS AddObserver(MOpAsyncFileManObserver* observer);
    inline OP_STATUS RemoveObserver(MOpAsyncFileManObserver* observer);

protected:
    void SignalAllObservers(OP_STATUS status, int id);

    Head m_Observers;
};


class OpAsyncFileMan: public OpAsyncFileManBase
{
public:
    virtual ~OpAsyncFileMan(){};
	virtual OP_STATUS DeleteAsync(const OpStringC& fileName, int& requestId)=0;
};

inline OP_STATUS OpAsyncFileManBase::AddObserver(MOpAsyncFileManObserver* observer)
{
    OpAsyncManObserverLinkItem* item = new OpAsyncManObserverLinkItem();
    if (!item) 
    {   
        return OpStatus::ERR_NO_MEMORY;
    }
    item->m_observer = observer;
    item->Into(&m_Observers);
    return OpStatus::OK;
}

inline OP_STATUS OpAsyncFileManBase::RemoveObserver(MOpAsyncFileManObserver* observer)
{
    OpAsyncManObserverLinkItem* item = static_cast<OpAsyncManObserverLinkItem*>(m_Observers.First());
    while ( item )
    {
        OpAsyncManObserverLinkItem* tmp = static_cast<OpAsyncManObserverLinkItem*>(item);
        item = static_cast<OpAsyncManObserverLinkItem*>(item->Suc());
        if (tmp->m_observer == observer)
        {
            tmp->Out();
            delete tmp;
        }
    }
    return OpStatus::OK;
}

inline void OpAsyncFileManBase::SignalAllObservers(OP_STATUS status, int id)
{
    OpAsyncManObserverLinkItem* item = static_cast<OpAsyncManObserverLinkItem*>(m_Observers.First());
    while ( item )
    {
        item->m_observer->OnActionDone(status, id);
        item = static_cast<OpAsyncManObserverLinkItem*>(item->Suc());
    }
}

inline OpAsyncFileManBase::~OpAsyncFileManBase()
{
    OpAsyncManObserverLinkItem* item = static_cast<OpAsyncManObserverLinkItem*>(m_Observers.First());
    while ( item )
    {
        OpAsyncManObserverLinkItem* tmp = static_cast<OpAsyncManObserverLinkItem*>(item);
        item = static_cast<OpAsyncManObserverLinkItem*>(item->Suc());
        tmp->Out();
        delete tmp;
    }
}

#endif // UTIL_HAVE_ASYNC_FILEMAN

#endif // !MODULES_UTIL_OPFILE_OP_ASYNC_FILEMAN_H
