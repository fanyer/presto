/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#ifdef POSIX_OK_SHARED_MEMORY
#include "modules/pi/system/OpSharedMemory.h"
#include "modules/util/OpSharedPtr.h"
#include <pthread.h>
#include <sys/shm.h>
#include <semaphore.h>

/** An implementation of OpSharedMemory.
 *
 * Consists of a SharedMemoryBlock, Attachment and the cleanup functor.
 */
class PosixSharedMemory : public OpSharedMemory
{
private:
	/** Internal: Handle to a shared memory id.
	 *
	 * Represents a shared memory block, regardless whether it is mapped to an
	 * application pointer.
	 */
	class SharedMemoryBlock
	{
	public:
		SharedMemoryBlock(int validShmid, const Identifier& identifier);

		OP_STATUS Destroy();
		int GetShmid() const;
		size_t GetSize() const;
		const Identifier& GetIdentifier() const;
		static OP_STATUS Create(OpSharedPtr<SharedMemoryBlock>& mem, size_t size);
		static OP_STATUS Open(const Identifier& identifier,
							  OpSharedPtr<SharedMemoryBlock>& mem);
	private:
		OP_STATUS Initialize();
		// Non-copyable
		SharedMemoryBlock(const SharedMemoryBlock& other);
		const SharedMemoryBlock operator=(const SharedMemoryBlock& other);

		int m_shmid;
		size_t m_size;
		Identifier m_identifier;
	};

	/** Internal: Represents a living mapping of a shared block to an application pointer.
	 */
	class Attachment
	{
	public:
		Attachment(void* mapped_memory);
		~Attachment();
		char* GetOffsetPosition(size_t offset) const;
		static OP_STATUS Create(OpSharedPtr<SharedMemoryBlock> mem,
								OpSharedPtr<Attachment>& attachment);
	private:
		// Non-copyable
		Attachment(const Attachment& other);
		const Attachment operator=(const Attachment& other);
		void* m_mapped_memory;
	};

	/** Internal: Reference counter for the shared memory block.
	 *
	 * When DecrementAndCheck() returns true, the shared block should be removed.
	 */
	class RefCounter
	{
	public:
		RefCounter();
		bool DecrementAndCheck();
		void Increment();
		static void Create(OpSharedPtr<Attachment> attachment);
	private:
		unsigned m_ref_count;
	};

	/** Internal: Locker to provide atomic operations.
	 */
	class AutoLocker
	{
	public:
		OP_STATUS Lock(const Identifier& key);
		OP_STATUS Unlink();
		/// Destructor releases, doesn't unlink
		~AutoLocker();
	private:
		OpString8 m_name;
		sem_t* m_semaphore;
	};

	struct PosixIdentifier
	{
		PosixIdentifier(key_t key = 0);
		static OP_STATUS FromIdentifier(PosixIdentifier& out, const Identifier& identifier);
		OP_STATUS ToIdentifier(Identifier& out);
		key_t key_used;
		INTPTR platformType;
	};

	/** PosixSharedMemory is non-copyable */
	PosixSharedMemory(const PosixSharedMemory& other);
	const PosixSharedMemory operator=(const PosixSharedMemory& other);

	RefCounter* GetRefCounter() const
	{
		/* RefCounter is at the beginning of the shared block, its address
		 * is effectively the address of the entire shared memory block.*/
		return reinterpret_cast<RefCounter*>(m_attachment->GetOffsetPosition(0));
	}

	OpSharedPtr<SharedMemoryBlock> m_mem_block;
	OpSharedPtr<Attachment> m_attachment;
	OpAutoPtr<CleanupFunctor> m_callback;

	static const size_t OBJ_ALIGN = sizeof(double) > sizeof(void*) ?
				sizeof(double) : sizeof(void*);
	static const size_t SHMEM_OVERHEAD = sizeof(RefCounter);
	static const size_t SHMEM_START = SHMEM_OVERHEAD +
			((OBJ_ALIGN*SHMEM_OVERHEAD - SHMEM_OVERHEAD) % OBJ_ALIGN);

	PosixSharedMemory(OpSharedPtr<SharedMemoryBlock> mem_block,
					  OpSharedPtr<Attachment> attachment);
public:
	typedef OpSharedMemory::Identifier Identifier;
	typedef OpSharedMemory::CleanupFunctor CleanupFunctor;
	~PosixSharedMemory();

	virtual size_t Size() { return m_mem_block->GetSize(); }
	virtual void* Ptr()
	{
		return m_attachment->GetOffsetPosition(SHMEM_START);
	}
	virtual const Identifier& GetIdentifier() { return m_mem_block->GetIdentifier(); }
	virtual void SetCleanupFunctor(OpAutoPtr<CleanupFunctor> cleanup_functor)
	{
		m_callback = cleanup_functor;
	}

	static INTPTR GetPlatformType()
	{
		/* For complete safety, we should include compiler type, version, flags
		 * and probably a dozen other things. But we'll settle on bitness.
		 *
		 * We're assuming all Opera components with same bitness were built with
		 * the same compilation configuration and are binary-compatible platforms.
		 */
		return sizeof(char*);
	}

	static OP_STATUS Create(size_t size, OpSharedMemory **out);
	static OP_STATUS Open(const Identifier& identifier, OpSharedMemory **out);
};


// Implementation

PosixSharedMemory::PosixIdentifier::PosixIdentifier(key_t key) :
	key_used(key), platformType(GetPlatformType())
{
}

OP_STATUS PosixSharedMemory::PosixIdentifier::FromIdentifier(
		PosixIdentifier& out, const Identifier& identifier)
{
	if(identifier.Length() != sizeof(PosixIdentifier)) return OpStatus::ERR;
	out = *reinterpret_cast<const PosixIdentifier*>(identifier.Data());
	return OpStatus::OK;
}

OP_STATUS PosixSharedMemory::PosixIdentifier::ToIdentifier(Identifier& out)
{
	return out.SetCopyData(reinterpret_cast<char*>(this), sizeof(PosixIdentifier));
}

// AutoLocker
OP_STATUS PosixSharedMemory::AutoLocker::Lock(const Identifier& key)
{
	if(m_name.HasContent()) return OpStatus::ERR;
	PosixIdentifier pid;
	RETURN_IF_ERROR(PosixIdentifier::FromIdentifier(pid, key));
	RETURN_IF_ERROR(m_name.AppendFormat("/opera-s%d", pid.key_used));
	m_semaphore = sem_open(m_name.CStr(), O_CREAT, 0600, 1);
	if(m_semaphore == SEM_FAILED) return OpStatus::ERR;
	return sem_wait(m_semaphore) ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS PosixSharedMemory::AutoLocker::Unlink()
{
	if(m_name.IsEmpty()) return OpStatus::ERR;
	return sem_unlink(m_name.CStr()) ? OpStatus::ERR : OpStatus::OK;
}

PosixSharedMemory::AutoLocker::~AutoLocker()
{
	if(!m_semaphore || m_semaphore == SEM_FAILED) return;
	int error = sem_post(m_semaphore);
	error += sem_close(m_semaphore);
	OP_ASSERT(error == 0);
}

// RefCounter
PosixSharedMemory::RefCounter::RefCounter() : m_ref_count(1)
{
}

bool PosixSharedMemory::RefCounter::DecrementAndCheck()
{
	return --m_ref_count == 0;
}

void PosixSharedMemory::RefCounter::Increment()
{
	++m_ref_count;
}

void PosixSharedMemory::RefCounter::Create(OpSharedPtr<Attachment> attachment)
{
	new (attachment->GetOffsetPosition(0)) RefCounter();
}

// SharedMemoryBlock
PosixSharedMemory::SharedMemoryBlock::SharedMemoryBlock(
		int valid_shmid,
		const Identifier &identifier) :
	m_shmid(valid_shmid)
{
	OP_ASSERT(m_shmid != -1);
	m_identifier = identifier;
}

OP_STATUS PosixSharedMemory::SharedMemoryBlock::Destroy()
{
	return shmctl(m_shmid, IPC_RMID, NULL) ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS PosixSharedMemory::SharedMemoryBlock::Initialize()
{
	// Set size
	shmid_ds info;
	if (0 != shmctl(m_shmid, IPC_STAT, &info))
	{
		switch (errno)
		{
		case EACCES: case EPERM: return OpStatus::ERR_NO_ACCESS;
		default: return OpStatus::ERR;
		}
	}
	m_size = info.shm_segsz;
	return OpStatus::OK;
}

int PosixSharedMemory::SharedMemoryBlock::GetShmid() const
{
	return m_shmid;
}

size_t PosixSharedMemory::SharedMemoryBlock::GetSize() const
{
	return m_size;
}

const PosixSharedMemory::Identifier&
PosixSharedMemory::SharedMemoryBlock::GetIdentifier() const
{
	return m_identifier;
}

OP_STATUS PosixSharedMemory::SharedMemoryBlock::Create(
		OpSharedPtr<SharedMemoryBlock>& mem,
		size_t size)
{

	int shmid = -1;
#ifdef DEBUG_ENABLE_OPASSERT
	int loop_count = 0;
#endif
	do
	{
		/* This should be random enough not to have frequent collisions with
		 * existing shared memory identifiers. If we get a collision, the loop
		 * will try again. One may try to make collisions even less frequent
		 * by including g_component_manager->GetAddress().cm in the key - this
		 * will be unique per process.*/
		key_t key = op_rand();

		if(key == IPC_PRIVATE)
			continue; // We've randomly selected a key that has special meaning.

		shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
		if(shmid == -1)
		{
			switch(errno)
			{
			case EEXIST:
				// This key already exists, try again.
				break;
			case ENOSPC: return OpStatus::ERR_NO_MEMORY;
			default: return OpStatus::ERR;
			}
		}
		else
		{
			Identifier identifier;
			RETURN_IF_ERROR(PosixIdentifier(key).ToIdentifier(identifier));
			mem = make_shared<SharedMemoryBlock>(shmid, identifier);
			return mem ? mem->Initialize() : OpStatus::ERR_NO_MEMORY;
		}

		/* If we can't find a non-colliding shared mem index for a long while,
		 * there's probably something wrong with our random key generation.*/
		OP_ASSERT((loop_count++ < 40) &&
				  "there is probably something wrong with our random key generation");
	}
	while(shmid == -1);
	return OpStatus::OK;
}

OP_STATUS PosixSharedMemory::SharedMemoryBlock::Open(
		const Identifier& identifier,
		OpSharedPtr<SharedMemoryBlock>& mem)
{
	PosixIdentifier pid;
	RETURN_IF_ERROR(PosixIdentifier::FromIdentifier(pid, identifier));
	int permissions = 0600; // user: rw, group: none, others: none
	int shmid = shmget(pid.key_used, 0, permissions);
	if(shmid < 0)
	{
		switch(errno)
		{
		case EACCES: return OpStatus::ERR_NO_ACCESS;
		case ENOMEM: return OpStatus::ERR_NO_MEMORY;
		default: return OpStatus::ERR;
		}
	}
	mem = make_shared<SharedMemoryBlock>(shmid, identifier);
	return mem ? mem->Initialize() : OpStatus::ERR_NO_MEMORY;
}

// Attachment
PosixSharedMemory::Attachment::Attachment(void* mapped_memory) :
	m_mapped_memory(mapped_memory)
{}

PosixSharedMemory::Attachment::~Attachment()
{
	// Detach.
	shmdt(m_mapped_memory);
}

char* PosixSharedMemory::Attachment::GetOffsetPosition(size_t offset) const
{
	return (char*)m_mapped_memory + offset;
}

OP_STATUS PosixSharedMemory::Attachment::Create(
		OpSharedPtr<SharedMemoryBlock> mem,
		OpSharedPtr<Attachment>& attachment)
{
	void* mapped_memory = shmat(mem->GetShmid(), NULL, 0);
	if(mapped_memory == reinterpret_cast<void*>(-1)) // -1 signifies error.
	{
		if(errno == ENOMEM)
			return OpStatus::ERR_NO_MEMORY;
		if(errno == EACCES)
			return OpStatus::ERR_NO_ACCESS;
	}
	attachment = make_shared<Attachment>(mapped_memory);
	return attachment ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

// PosixSharedMemory
OP_STATUS PosixSharedMemory::Create(size_t size,
									OpSharedMemory **out)
{
	struct SharedBlockAutoDestroyer
	{
		SharedBlockAutoDestroyer(OpSharedPtr<SharedMemoryBlock> mem) :
			m_mem(mem)
		{}
		~SharedBlockAutoDestroyer()
		{
			if(m_mem) m_mem->Destroy();
		}
		void Release()
		{
			m_mem.reset();
		}

		OpSharedPtr<SharedMemoryBlock> m_mem;
	};

	OpSharedPtr<SharedMemoryBlock> mem;
	OpSharedPtr<Attachment> attachment;
	RETURN_IF_ERROR(SharedMemoryBlock::Create(mem, size + SHMEM_START));
	SharedBlockAutoDestroyer auto_destroyer(mem);
	RETURN_IF_ERROR(Attachment::Create(mem, attachment));
	RefCounter::Create(attachment);
	PosixSharedMemory* posix_shared_memory = OP_NEW(PosixSharedMemory,
										   (mem, attachment));
	RETURN_OOM_IF_NULL(posix_shared_memory);
	*out = posix_shared_memory;
	auto_destroyer.Release();
	return OpStatus::OK;
}

OP_STATUS PosixSharedMemory::Open(const Identifier& identifier,
								  OpSharedMemory **out)
{
	PosixIdentifier pid;
	RETURN_IF_ERROR(PosixIdentifier::FromIdentifier(pid, identifier));
	if(pid.platformType != PosixSharedMemory::GetPlatformType())
		return OpStatus::ERR_NO_ACCESS;
	AutoLocker locker;
	RETURN_IF_ERROR(locker.Lock(identifier));
	OpSharedPtr<SharedMemoryBlock> mem;
	OpSharedPtr<Attachment> attachment;
	RETURN_IF_ERROR(SharedMemoryBlock::Open(identifier, mem));
	RETURN_IF_ERROR(Attachment::Create(mem, attachment));
	// Increment RefCounter
	RefCounter* rc = reinterpret_cast<RefCounter*>(attachment->GetOffsetPosition(0));
	rc->Increment();
	PosixSharedMemory* posix_shared_memory = OP_NEW(PosixSharedMemory,
										   (mem, attachment));
	RETURN_OOM_IF_NULL(posix_shared_memory);
	*out = posix_shared_memory;
	return OpStatus::OK;
}

PosixSharedMemory::PosixSharedMemory(OpSharedPtr<SharedMemoryBlock> mem_block,
									 OpSharedPtr<Attachment> attachment) :
	m_mem_block(mem_block),
	m_attachment(attachment)
{
}

PosixSharedMemory::~PosixSharedMemory()
{
	AutoLocker locker;
	OP_CHECK_STATUS(locker.Lock(GetIdentifier()));
	// Decrement ref counter and conditional cleanup.
	RefCounter* rc = GetRefCounter();
	OP_ASSERT(rc);
	if(rc->DecrementAndCheck())
	{
		// We're the last owner of this shared memory.
		// Clean up if there's a callback for that registered:
		if(m_callback.get()) m_callback->Cleanup(Ptr());
		 // Remove the block:
		OP_CHECK_STATUS(m_mem_block->Destroy());
		OP_CHECK_STATUS(locker.Unlink());
	}
}

OP_STATUS OpSharedMemory::Create(size_t size, OpSharedMemory **out)
{
	return PosixSharedMemory::Create(size, out);
}

OP_STATUS OpSharedMemory::Open(const Identifier& identifier, OpSharedMemory **out)
{
	return PosixSharedMemory::Open(identifier, out);
}

#endif // POSIX_OK_SHARED_MEMORY
