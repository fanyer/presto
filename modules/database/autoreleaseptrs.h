/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#ifndef _MODULES_DATABASE_AUTORELEASEPTRS_H_
#define _MODULES_DATABASE_AUTORELEASEPTRS_H_

#if defined SUPPORT_DATABASE_INTERNAL || defined OPSTORAGE_SUPPORT

/**
 * Help class to automatically wrap a pointer to classes that require
 * ::Release() to be called upon, to delete the object.
 * Currently, used for OpStorage, WSD_Database and SqlTransaction
 */
template<typename Type>
class AutoReleaseTypePtr : public PS_ObjDelListener::ResourceShutdownCallback
{
public:
	explicit AutoReleaseTypePtr(Type* st=NULL) : m_ptr(st)
	{
		if (m_ptr != NULL)
			m_ptr->AddShutdownCallback(this);
	}
	~AutoReleaseTypePtr() {Release();}

	void Override(const Type* rhs)
	{
		Release(FALSE);
		Set(rhs);
	}
	void Set(const Type* rhs)
	{
		Release();
		//need to const cast because of IncRefCount
		m_ptr = const_cast<Type*>(rhs);
		if(m_ptr != NULL)
			m_ptr->AddShutdownCallback(this);
	}

	AutoReleaseTypePtr& operator=(const Type* rhs)
	{
		Set(rhs);
		return *this;
	}

	BOOL operator==(const AutoReleaseTypePtr<Type>& rhs) const
	{ return CompareTo(rhs.m_ptr); }

	BOOL operator!=(const AutoReleaseTypePtr<Type>& rhs) const
	{ return !CompareTo(rhs.m_ptr); }

	BOOL operator==(const Type *rhs) const
	{ return CompareTo(rhs); }

	BOOL operator!=(const Type *rhs) const
	{ return !CompareTo(rhs); }

	operator Type*() const { return m_ptr; }
	Type* operator->() const { return m_ptr; }
	Type& operator*() const { return *m_ptr; }

	virtual void HandleResourceShutdown() { Release(); }
private:
	Type* m_ptr;
	//don't allow these
	AutoReleaseTypePtr(const AutoReleaseTypePtr<Type>& rhs) {}
	void operator=(const AutoReleaseTypePtr<Type>&) {}

	void Release(BOOL call_release = TRUE)
	{
		if (m_ptr != NULL) {
			Type* ptr = m_ptr;
			m_ptr = NULL;
			ptr->RemoveShutdownCallback(this);
			if (call_release)
				ptr->Release();
		}
	}
	BOOL CompareTo(const Type* p) const { return p == m_ptr; }
};

#endif //defined SUPPORT_DATABASE_INTERNAL || OPSTORAGE_SUPPORT

#endif /* _MODULES_DATABASE_AUTORELEASEPTRS_H_ */
