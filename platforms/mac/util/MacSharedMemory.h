/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __MACSHAREDMEMORY_H__
#define __MACSHAREDMEMORY_H__

#define SHARED_MEMORY_FILE "/tmp/opera-%d"

//#define MAC_SHARED_MEMORY_DEBUG

class MacSharedMemory
{
protected:
	struct FileHandle 
	{
		FileHandle() : m_fd(-1) {}
		~FileHandle() { close(m_fd); }
		int m_fd;
	};

public:
	MacSharedMemory();
	~MacSharedMemory();
	
	OP_STATUS	Create(int size);
	OP_STATUS	Attach(int key);
	OP_STATUS	Detach();
	BOOL		IsAttached() { return (m_key >= 0); }
	
	int			GetKey() const { return m_key; }
	void		*GetBuffer() { return m_buffer; }
	int			GetSize() { return m_size; }

	void		Clear();
private:	
	void		Reset();

protected:	
	OP_STATUS	SetPointers(FileHandle &fd);

	int		m_key;
	int		m_size;				// Size of the memory block
	void	*m_buffer;			// Pointer to the actual data in the shared memory
	void	*m_shm_address;		// Pointer to the whole memory
};

#endif // __MACSHAREDMEMORY_H__
