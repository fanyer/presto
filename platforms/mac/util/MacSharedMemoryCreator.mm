/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "platforms/mac/util/MacSharedMemoryCreator.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h> // For O_* constants 

int MacSharedMemoryCreator::s_current_key = 0;

////////////////////////////////////////////////////////////////////////

MacSharedMemoryCreator::MacSharedMemoryCreator() : MacSharedMemory()
{
	// Set the start integer randomly for this process
	if (!s_current_key)
	{
		srand(time(NULL));
		s_current_key = rand();
	}
}

////////////////////////////////////////////////////////////////////////

MacSharedMemoryCreator::~MacSharedMemoryCreator()
{
	Destroy();
}

////////////////////////////////////////////////////////////////////////

OP_STATUS MacSharedMemoryCreator::Create(int size)
{
	char shm_file[PATH_MAX];
	int  key = s_current_key;
	FileHandle fd;

	do {
		// Increment the key
		key++;
			
		// Catch rollovers
		if (key == INT_MAX)
		{
			srand(time(NULL));
			key = rand();
		}

		// Make a random file and open it
		sprintf(shm_file, SHARED_MEMORY_FILE, key);
		fd.m_fd = shm_open(shm_file, O_CREAT | O_RDWR | O_EXCL, 0600);
	} while (fd.m_fd < 0 && errno == EEXIST);

	if (fd.m_fd < 0)
		return OpStatus::ERR;

	// Set the size
	if (ftruncate(fd.m_fd, size + sizeof(size)) < 0)
		return OpStatus::ERR;
	
	// Save the key and size
	m_key = key;
	m_size = size;
	
	RETURN_IF_ERROR(SetPointers(fd));
	
	// Set the size in the shared memory
	op_memcpy(m_shm_address, (void *)&m_size, sizeof(m_size));

#ifdef MAC_SHARED_MEMORY_DEBUG
	fprintf(stderr, "Created key:0x%x, size:%d, pid:%d, file:%s\n", key, m_size, getpid(), shm_file);
#endif // MAC_SHARED_MEMORY_DEBUG
	
	// Update the static so the next time it's incremented
	s_current_key = m_key;
		
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////

void MacSharedMemoryCreator::Destroy()
{
	if (m_key > -1)
	{
		char shm_file[PATH_MAX];
		int key = m_key;
		
		// First detach the memory
		Detach();
		
		// Get the name to the shared memory file
		sprintf(shm_file, SHARED_MEMORY_FILE, key);
		
		// Unlink the memory now so it will be cleaned up
		shm_unlink(shm_file);
	}
}
