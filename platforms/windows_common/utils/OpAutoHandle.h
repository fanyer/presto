// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2012 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
//

#ifndef WINDOWS_AUTOHANDLE_H
#define WINDOWS_AUTOHANDLE_H

namespace HandleCloser
{
	inline void CloseHKEY(HKEY hkey) {RegCloseKey(hkey);}
	inline void CloseHANDLE(HANDLE handle) {::CloseHandle(handle);}
	inline void CloseHMODULE(HMODULE module) {::FreeLibrary(module);}
	inline void CloseHICON(HICON icon) {::DestroyIcon(icon);}
	inline void CloseHBITMAP(HBITMAP bitmap) {::DeleteObject(bitmap);}
	inline void ReleaseMUTEX(HANDLE mutex) {::ReleaseMutex(mutex);}
	inline void UnmapViewOfFile(LPVOID view) {::UnmapViewOfFile(view);}
	inline void UnlockGlobalMemory(HGLOBAL memory) {::GlobalUnlock(memory);}
	inline void FreeGlobalMemory(HGLOBAL memory) {::GlobalFree(memory);}
}

template<class T, void DestroyHandle(T)> class OpAutoHandle {
protected:
	T m_handle; /// Pointer to the owned handle

public:

	explicit OpAutoHandle(T handle = NULL) : m_handle(handle) {};

	/**
	 * Copy constructor for non-constant parameter, acquire the ownership
	 * of the handle currently owned by the argument OpAutoHandle.
	 */
	OpAutoHandle(OpAutoHandle& rhs) : m_handle(rhs.release()) {};

	/**
	 * Assignment without implicit conversion.
	 */
	OpAutoHandle& operator=(OpAutoHandle& rhs)
	{
		reset(rhs.release());
		return *this;
	};

	/**
	 * Destructor. Closes the handle using the appropriate method.
	 */
	~OpAutoHandle()
	{
		if (m_handle != NULL)
			DestroyHandle(m_handle);
	};


	T* operator &() const
	{
		return const_cast<T*>(&m_handle);
	};

	operator T() const
	{
		return m_handle;
	}

	/**
	 * Return the handle currently owned by this OpAutoHandle.
	 *
	 * @return handle
	 */
	T get() const
	{
		return m_handle;
	};

	/**
	 * Release ownership of the handle owned by this OpAutoHandle.
	 * @return handle.
	 */
	T release()
	{
		T handle = m_handle;
		m_handle = NULL;
		return handle;
	};

	/**
	 * Replace the handle owned by this OpAutoHandle with a new handle.
	 * Free the handle currently owned by this OpAutoHandle
	 * before taking the new ownership.
	 *
	 * @param handle 'new' handle.
	 */
	void reset(T handle = NULL)
	{
		if (m_handle != handle)
		{
			if (m_handle != NULL)
				DestroyHandle(m_handle);
			m_handle = handle;
		}
	};

};

typedef OpAutoHandle<HKEY, HandleCloser::CloseHKEY> OpAutoHKEY;
typedef OpAutoHandle<HANDLE, HandleCloser::CloseHANDLE> OpAutoHANDLE;
typedef OpAutoHandle<HMODULE, HandleCloser::CloseHMODULE> OpAutoHMODULE;
typedef OpAutoHandle<HICON, HandleCloser::CloseHICON> OpAutoHICON;
typedef OpAutoHandle<HBITMAP, HandleCloser::CloseHBITMAP> OpAutoHBITMAP;
typedef OpAutoHandle<HANDLE, HandleCloser::ReleaseMUTEX> OpAutoReleaseMUTEX;
typedef OpAutoHandle<LPVOID, HandleCloser::UnmapViewOfFile> OpAutoUnmapViewOfFile;
typedef OpAutoHandle<HGLOBAL, HandleCloser::UnlockGlobalMemory> OpAutoGlobalLock;
typedef OpAutoHandle<HGLOBAL, HandleCloser::FreeGlobalMemory> OpAutoGlobalAlloc;

#endif //WINDOWS_AUTOHANDLE_H
