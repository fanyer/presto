/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef PI_SHARED_MEMORY

#include "modules/pi/system/OpSharedMemory.h"
#include "platforms/windows_common/utils/OpAutoHandle.h"
#include <climits> // for CHAR_BIT

/** An implementation of OpSharedMemory.
 */
class WindowsOpSharedMemory : public OpSharedMemory
{
private:
	/** Internal: Reference counter for the shared memory block.
	 */
	class RefCounter
	{
	public:
		/// After construction, counter value is 1.
		RefCounter();

		/// @return true if count equals zero after decrementing, false otherwise.
		bool DecrementAndCheck();
		void Increment();
	private:
		LONG m_ref_count;
	};

	/** Internal: Header to facilitate offset computation. */
	struct SharedMemoryHeader
	{
		SharedMemoryHeader(size_t size) :
			size(size)
		{}

		RefCounter ref_counter;
		size_t size;
	};

	/** Internal: Locker to provide atomic operations. */
	class AutoLocker
	{
	public:
		AutoLocker();
		OP_STATUS Lock(const Identifier& key);
		/// Destructor releases
		~AutoLocker();
	private:
		HANDLE m_semaphore;
	};

	/** @return Number of bits required to store a pointer on this platform.
	 */
	static INTPTR GetPlatformBitness();
	static bool PlatformBitnessMatches(const Identifier& identifier);

	static OP_STATUS CreateSharedMemory(HANDLE& mem, Identifier &identifier_out, size_t size);
	static OP_STATUS OpenSharedMemory(HANDLE& mem, const Identifier& identifier);
	static OP_STATUS CreateMapping(void*& mapped_pointer_out, HANDLE mem);

	/// Creates a SharedMemoryHeader at the begin of mapped_pointer
	static void CreateSharedMemoryHeader(void* mapped_pointer, size_t size);

	static SharedMemoryHeader& GetHeader(void* mapped_pointer)
	{
		return *reinterpret_cast<SharedMemoryHeader*>(mapped_pointer);
	}

	/** Constructor.
	 *
	 * Private, use Create() or Open() to get instance.
	 *
	 * @param memory Handle to valid, open shared memory obtained through
	 * CreateFileMapping.
	 * @param mapped_pointer Pointer mapped to the shared memory, obtained
	 * through MapViewOfFile.
	 * @param id Identifier of the memory block. id.key is the number used to
	 * name the shared memory block during a call to CreateFileMapping. If the
	 * name was "Local\Opera_sm_1234" it should be equal to 1234. id.platformType
	 * is the value returned by GetPlatformType().
	 */
	WindowsOpSharedMemory(HANDLE memory, void* mapped_pointer, Identifier id);

	OpAutoHANDLE m_memory_handle;
	OpAutoUnmapViewOfFile m_mapped_pointer;
	Identifier m_identifier;

	OpAutoPtr<CleanupFunctor> m_cleanup_callback;
	static LONG m_counter;

	static const size_t SHMEM_OBJ_ALIGN = sizeof(double) > sizeof(void*) ?
				sizeof(double) : sizeof(void*);
	static const size_t SHMEM_OVERHEAD = sizeof(SharedMemoryHeader);
	static const size_t SHMEM_START = SHMEM_OVERHEAD +
			((SHMEM_OBJ_ALIGN*SHMEM_OVERHEAD - SHMEM_OVERHEAD) % SHMEM_OBJ_ALIGN);
public:
	typedef OpSharedMemory::Identifier Identifier;
	typedef OpSharedMemory::CleanupFunctor CleanupFunctor;
	~WindowsOpSharedMemory();

	virtual size_t Size()
	{
		return GetHeader(m_mapped_pointer).size;
	}

	// Usable memory is behind the Header.
	virtual void* Ptr()
	{
		return reinterpret_cast<char*>(m_mapped_pointer.get()) + SHMEM_START;
	}

	virtual const Identifier& GetIdentifier()
	{
		return m_identifier;
	}

	virtual void SetCleanupFunctor(OpAutoPtr<CleanupFunctor> cleanupFunctor)
	{
		m_cleanup_callback = cleanupFunctor;
	}

	static OP_STATUS Create(size_t size, OpSharedMemory **out);
	static OP_STATUS Open(const Identifier& identifier, OpSharedMemory **out);
};

LONG WindowsOpSharedMemory::m_counter = 0;

// Implementation
// AutoLocker
WindowsOpSharedMemory::AutoLocker::AutoLocker() :
	m_semaphore(NULL)
{
}

OP_STATUS WindowsOpSharedMemory::AutoLocker::Lock(const Identifier &key)
{
	const size_t name_len = 64 + 10 /* "_semaphore" */;
	char name[name_len] = { 0 }; // ARRAY OK 2012-08-24 mpawlowski
	int result = _snprintf_s(name,
							 name_len,
							 "%s_semaphore",
							 key.Data());
	if(result <= 0)
		return OpStatus::ERR;

	m_semaphore = CreateSemaphoreA(
				NULL,
				1,
				1,
				name);
	if(!m_semaphore)
		return OpStatus::ERR;
	DWORD wait_result = WaitForSingleObject(
		m_semaphore,
		INFINITE);
	return wait_result == WAIT_OBJECT_0 ? OpStatus::OK : OpStatus::ERR;
}

WindowsOpSharedMemory::AutoLocker::~AutoLocker()
{
	if(m_semaphore)
	{
		ReleaseSemaphore(m_semaphore, 1, NULL);
		CloseHandle(m_semaphore);
	}
}

// RefCounter
WindowsOpSharedMemory::RefCounter::RefCounter() : m_ref_count(1)
{
}

bool WindowsOpSharedMemory::RefCounter::DecrementAndCheck()
{
	OP_ASSERT(m_ref_count > 0);
	return --m_ref_count == 0;
}

void WindowsOpSharedMemory::RefCounter::Increment()
{
	++m_ref_count;
}

// SharedMemoryHeader
void WindowsOpSharedMemory::CreateSharedMemoryHeader(
		void* mapped_pointer, size_t size)
{
	OP_ASSERT(size >= sizeof(SharedMemoryHeader));
	new (&(GetHeader(mapped_pointer))) SharedMemoryHeader(size);
}

// WindowsOpSharedMemory
OP_STATUS WindowsOpSharedMemory::OpenSharedMemory(HANDLE& mem, const Identifier &identifier)
{
	mem = OpenFileMappingA(
		FILE_MAP_ALL_ACCESS, // access rights
		false, // child process can inherit handle
		identifier.Data()); // name of the file mapping
	return mem ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS WindowsOpSharedMemory::CreateSharedMemory(
		HANDLE &mem, Identifier& identifier_out, size_t size)
{
	const size_t key_len = 64;
	char key[key_len] = { 0 }; // ARRAY OK 2012-08-22 mpawlowski
	int result = _snprintf_s(key,
							 key_len,
							 "Local\\Opera%d_sm_%X_%X",
							 GetPlatformBitness(),
							 GetCurrentProcessId(),
							 InterlockedIncrement(&m_counter));
	// An example name: Local\Opera32_sm_1DC_5
	if(result <= 0)
		return OpStatus::ERR;

	HANDLE handle = CreateFileMappingA(
		INVALID_HANDLE_VALUE,	// use paging file
		NULL,					// default security
		PAGE_READWRITE,			// read/write access
		0,		// maximum object size (high-order DWORD)
		size,	// maximum object size (low-order DWORD)
		key);	// name of mapping object

	if (!handle)
	{
		switch(GetLastError())
		{
		case ERROR_DISK_FULL: return OpStatus::ERR_NO_MEMORY;
		default: return OpStatus::ERR;
		}
	}

	RETURN_IF_ERROR(identifier_out.SetCopyData(key, key_len));
	mem = handle;
	return OpStatus::OK;
}

OP_STATUS WindowsOpSharedMemory::CreateMapping(void*& mapped_pointer_out, HANDLE mem)
{
	mapped_pointer_out = MapViewOfFile(
		mem,	// handle to map object
		FILE_MAP_ALL_ACCESS,	// read/write permission
		0,	// offset (high-order DWORD)
		0,	// offset (low-order DWORD)
		0	// number of bytes to map, 0 means all
		);
	return mapped_pointer_out ? OpStatus::OK : OpStatus::ERR;
}

INTPTR WindowsOpSharedMemory::GetPlatformBitness()
{
	/* We're assuming all Opera components with same bitness were built with
	 * the same compilation configuration and are binary-compatible platforms.
	 */
	return sizeof(char*) * CHAR_BIT;
}

bool WindowsOpSharedMemory::PlatformBitnessMatches(const Identifier& identifier)
{
	char bitness_string[16] = { 0 }; // ARRAY OK 24-08-2012 mpawlowski
	int result = _snprintf(bitness_string, 16, "Opera%d", GetPlatformBitness());
	if(result < 0)
		return false;
	/* The identifier looks like Local\Opera32_sm_1DC_5, where Opera32 or Opera64
	 * signifies platform bitness. We search for that substring.*/
	return identifier.FindFirst(bitness_string) != OpDataNotFound;
}

OP_STATUS WindowsOpSharedMemory::Create(size_t size,
										OpSharedMemory **out)
{
	HANDLE mem = 0;
	void* mapped_pointer = NULL;
	size_t totalSize = size + SHMEM_START;
	Identifier id;
	RETURN_IF_ERROR(CreateSharedMemory(mem, id, totalSize));
	OpAutoHANDLE auto_mem(mem);

	RETURN_IF_ERROR(CreateMapping(mapped_pointer, mem));
	OpAutoUnmapViewOfFile auto_mapped_pointer(mapped_pointer);

	CreateSharedMemoryHeader(mapped_pointer, size);
	WindowsOpSharedMemory* windows_shmem = OP_NEW(WindowsOpSharedMemory,
											(mem, mapped_pointer, id));
	RETURN_OOM_IF_NULL(windows_shmem);
	*out = windows_shmem;
	auto_mem.release();
	auto_mapped_pointer.release();
	return OpStatus::OK;
}

OP_STATUS WindowsOpSharedMemory::Open(const Identifier& identifier,
									  OpSharedMemory **out)
{
	if(!PlatformBitnessMatches(identifier))
		return OpStatus::ERR_NO_ACCESS;
	AutoLocker locker;
	RETURN_IF_ERROR(locker.Lock(identifier));
	HANDLE mem = 0;
	void* mapped_pointer = NULL;
	RETURN_IF_ERROR(OpenSharedMemory(mem, identifier));
	OpAutoHANDLE auto_mem(mem);

	RETURN_IF_ERROR(CreateMapping(mapped_pointer, mem));
	OpAutoUnmapViewOfFile auto_mapped_pointer(mapped_pointer);

	// Increment RefCounter
	GetHeader(mapped_pointer).ref_counter.Increment();
	WindowsOpSharedMemory* windows_shmem = OP_NEW(WindowsOpSharedMemory,
											(mem, mapped_pointer, identifier));
	RETURN_OOM_IF_NULL(windows_shmem);
	*out = windows_shmem;
	auto_mem.release();
	auto_mapped_pointer.release();
	return OpStatus::OK;
}

WindowsOpSharedMemory::WindowsOpSharedMemory(
		HANDLE memory, void *mapped_pointer, Identifier id) :
	m_memory_handle(memory),
	m_mapped_pointer(mapped_pointer),
	m_identifier(id)
{
}

WindowsOpSharedMemory::~WindowsOpSharedMemory()
{
	AutoLocker locker;
	OP_CHECK_STATUS(locker.Lock(GetIdentifier()));

	// Decrement ref counter and conditional cleanup.
	RefCounter& rc = GetHeader(m_mapped_pointer).ref_counter;
	if(rc.DecrementAndCheck())
	{
		// We're the last owner of this shared memory.
		// Clean up if there's a callback for that registered:
		if(m_cleanup_callback.get())
			m_cleanup_callback->Cleanup(Ptr());
	}
}

OP_STATUS OpSharedMemory::Create(size_t size, OpSharedMemory **out)
{
	return WindowsOpSharedMemory::Create(size, out);
}

OP_STATUS OpSharedMemory::Open(const Identifier& identifier, OpSharedMemory **out)
{
	return WindowsOpSharedMemory::Open(identifier, out);
}

#endif // PI_SHARED_MEMORY

