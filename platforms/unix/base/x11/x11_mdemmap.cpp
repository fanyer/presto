/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/

#include "core/pch.h"

#if defined(LIBGOGI_CAP_MMAP_INTERFACE) && defined(MDE_USE_MMAP)

#include "modules/libgogi/mde_mmap.h"
#include "platforms/posix/posix_native_util.h"

# include <fcntl.h>
# include <sys/mman.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <unistd.h>

class X11_MDE_MMapping : public MDE_MMapping
{
public:
	X11_MDE_MMapping() : address(NULL), size(0)
	{}
	virtual ~X11_MDE_MMapping()
	{
		munmap(address, size);
	}

	OP_STATUS MapFile(const uni_char* file)
	{
		int fd;
		struct stat st;
		// This saves rather large amounts of memory since the font is
		// automatically read from the on-disk file on demand, and removed
		// from ram-cache when the RAM is getting full.

		OpString8 filename;
		RETURN_IF_ERROR(PosixNativeUtil::ToNative(file, &filename));
		fd = open( filename.CStr(), O_RDONLY, 0 );
		if( fd < 0 )
		{
			perror("open");
			return OpStatus::ERR;
		}
		fstat( fd, &st );

		size = st.st_size;
		address = mmap( 0, st.st_size, PROT_READ, MAP_SHARED, fd, 0 );
		close(fd);

		if( address == (void *)-1 )
		{
			return OpStatus::ERR;
		}
		return OpStatus::OK;
	}

	virtual void* GetAddress(){return address;}
	virtual int GetSize(){return size;}
private:
	void* address;
	int size;
};
OP_STATUS MDE_MMapping::Create(MDE_MMapping** m, const uni_char* file)
{
	X11_MDE_MMapping* pm = new X11_MDE_MMapping();
	if (!pm)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err = pm->MapFile(file);
	if (OpStatus::IsError(err))
	{
		delete pm;
		pm = NULL;
	}
	*m = pm;
	return err;
}
#endif // LIBGOGI_CAP_MMAP_INTERFACE && MDE_USE_MMAP

