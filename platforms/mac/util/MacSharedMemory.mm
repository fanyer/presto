/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "platforms/mac/util/MacSharedMemory.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h> // For O_* constants 

////////////////////////////////////////////////////////////////////////

class MacSharedMemoryCreator : public MacSharedMemory
{
	virtual ~MacSharedMemoryCreator();
	OP_STATUS Create(int size);
};


////////////////////////////////////////////////////////////////////////

MacSharedMemory::MacSharedMemory()
{
	Reset();
}

////////////////////////////////////////////////////////////////////////

MacSharedMemory::~MacSharedMemory()
{
}

////////////////////////////////////////////////////////////////////////

OP_STATUS MacSharedMemory::Attach(int key)
{
	char shm_file[PATH_MAX];
	FileHandle fd;

	// First get the shared memory file
	sprintf(shm_file, SHARED_MEMORY_FILE, key);
	fd.m_fd = shm_open(shm_file, O_RDWR, 0);
	if (fd.m_fd < 0)
	{
#ifdef MAC_SHARED_MEMORY_DEBUG
		fprintf(stderr, "Failed to attach key:0x%x, errno:%d, pid:%d, file:%s\n", key, errno, getpid(), shm_file);
#endif // MAC_SHARED_MEMORY_DEBUG
		return OpStatus::ERR;
	}

	// Save the key
	m_key = key;

#ifdef MAC_SHARED_MEMORY_DEBUG
	fprintf(stderr, "Attached key:0x%x, pid:%d\n", m_key, getpid());
#endif // MAC_SHARED_MEMORY_DEBUG
	
	// Determine the size of the shared memory
	void *address = mmap(0, sizeof(m_size), PROT_READ | PROT_WRITE, MAP_SHARED, fd.m_fd, 0);
	if (address == MAP_FAILED)
	{
#ifdef MAC_SHARED_MEMORY_DEBUG
		fprintf(stderr, "Failed to get the size of the shared memory key:0x%x, errno:%d, pid:%d\n", m_key, errno, getpid());
#endif // MAC_SHARED_MEMORY_DEBUG
		return OpStatus::ERR;
	}
	
	// Save the size
	m_size = *(int *)address;
	
	// munmap after the size
	munmap(address, sizeof(m_size));
	
	RETURN_IF_ERROR(SetPointers(fd));

    return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////

OP_STATUS MacSharedMemory::Detach()
{
	if (!m_shm_address)
        return OpStatus::ERR;

	// Unmap the memory
	munmap(m_shm_address, m_size);

#ifdef MAC_SHARED_MEMORY_DEBUG
	fprintf(stderr, "Detached key:0x%x, pid:%d\n", m_key, getpid());
#endif // MAC_SHARED_MEMORY_DEBUG

	Reset();

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////

OP_STATUS MacSharedMemory::SetPointers(FileHandle &fd)
{
	// Map the memory
	void *address = mmap(0, m_size + sizeof(m_size), PROT_READ | PROT_WRITE, MAP_SHARED, fd.m_fd, (off_t)0);
	if (address == MAP_FAILED)
	{
#ifdef MAC_SHARED_MEMORY_DEBUG
		fprintf(stderr, "Failed to get shared memory pointer key:0x%x, errno:%d, pid:%d\n", m_key, errno, getpid());
#endif // MAC_SHARED_MEMORY_DEBUG
		return OpStatus::ERR;
	}
	
	close(fd.m_fd);
	fd.m_fd = -1;
	
	// Assign the pointer
	m_shm_address = address;
	
	// The first part of the data has the size of it so skip over it
	m_buffer = (char *)m_shm_address + sizeof(m_size);
    
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////

void MacSharedMemory::Clear()
{
	if (m_shm_address && m_buffer)
		op_memset(m_buffer, 0, m_size);
}

////////////////////////////////////////////////////////////////////////

void MacSharedMemory::Reset()
{
	m_key = -1;
    m_size = 0;
	
	// Reset the shared memory pointers
	m_buffer = NULL;
	m_shm_address = NULL;
}

