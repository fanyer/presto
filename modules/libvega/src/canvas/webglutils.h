/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WEBGLUTILS_H
#define WEBGLUTILS_H

#ifdef CANVAS3D_SUPPORT

class ES_Object;

/** A base for handling the common reference counting and connection to
 *  the DOM object counterpart. */
class WebGLRefCounted
{
public:
	WebGLRefCounted() : m_esObject(NULL), m_destroyed(false), m_pendingDestroy(false), m_refCount(0) { }
	virtual ~WebGLRefCounted() { }

	bool isDestroyed() const         { return m_destroyed; }
	bool isPendingDestroy() const    { return m_pendingDestroy; }

	ES_Object* getESObject()         { return m_esObject; }
	void setESObject(ES_Object* obj) { m_esObject = obj; }

	virtual void releaseHandles()    { }

	// Called by the DOM object counterpart when it is destroyed.
	void esObjectDestroyed()
	{
		if (m_refCount)
		{
			m_pendingDestroy = true;
			m_esObject = NULL;
		}
		else
			OP_DELETE(this);
	}

	// User initiated destroy which may or may not destroy immediately.
	void destroy()
	{
		if (!m_refCount)
		{
			destroyInternal();
			m_destroyed = true;
			if (!m_esObject)
				OP_DELETE(this);
		}
		else
		{
			m_pendingDestroy = true;
			releaseHandles();
		}
	}

	void addRef()
	{
		++m_refCount;
	}

	void removeRef()
	{
		OP_ASSERT(m_refCount != 0);
		--m_refCount;
		if(!m_refCount && m_pendingDestroy)
			destroy();
	}

private:
	// Should be implemented to destroy any internal resources.
	virtual void destroyInternal() = 0;

	ES_Object* m_esObject;
	bool m_destroyed;
	bool m_pendingDestroy;
	unsigned int m_refCount;
};

/** A light smart pointer class to be used with the WebGLRefCounted. */
template <typename T>
class WebGLSmartPointer
{
public:
	WebGLSmartPointer(T *p = NULL) : ptr(p)  { if (ptr) ptr->addRef(); }
	WebGLSmartPointer(const WebGLSmartPointer<T> &s) : ptr(s.ptr)  { if (ptr) ptr->addRef(); }
	~WebGLSmartPointer()  { if (ptr) ptr->removeRef(); }

	void operator =(const WebGLSmartPointer<T> &s)  { SetPointer(s.ptr); }
	void operator =(T *p)  { SetPointer(p); }
	T *operator ->() const  { OP_ASSERT(ptr); return ptr; }
	T *GetPointer() const  { return ptr; }
	operator bool() const  { return ptr != NULL; }
	bool operator != (const T *p) const { return ptr != p; }
	bool operator == (const T *p) const { return ptr == p; }

private:
	void SetPointer(T *p)
	{
		if (p == ptr)
			return;
		if (ptr)
			ptr->removeRef();
		ptr = p;
		if (ptr)
			ptr->addRef();
	}

	T *ptr;
};

struct CanvasWebGLParameter
{
	enum Type{
		PARAM_BOOL, PARAM_BOOL2, PARAM_BOOL3, PARAM_BOOL4,
		PARAM_UBYTE4,
		PARAM_INT, PARAM_INT2, PARAM_INT3, PARAM_INT4,
		PARAM_UINT,
		PARAM_FLOAT, PARAM_FLOAT2, PARAM_FLOAT3, PARAM_FLOAT4,
		PARAM_OBJECT,
		PARAM_STRING,
		PARAM_MAT2, PARAM_MAT3, PARAM_MAT4,
		PARAM_UNKNOWN
	} type;
	union {
		bool bool_param;
		unsigned char ubyte_param[4]; /* ARRAY OK 2011-07-01 wonko */
		int int_param[4];             /* ARRAY OK 2011-07-01 wonko */
		unsigned int uint_param[1];   /* ARRAY OK 2011-07-01 wonko */
		float float_param[16];        /* ARRAY OK 2011-07-01 wonko */
		ES_Object* object_param;
		const uni_char* string_param;
	} value;
};

// If set the uniform is a samplerCube.
const UINT32 IS_SAMPLER_CUBE_BIT = 1 << 29;

#endif //CANVAS3D_SUPPORT
#endif //WEBGLUTILS_H
