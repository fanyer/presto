/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_MDE_MMAP

#include "modules/libgogi/mde_mmap.h"
#include "modules/pi/OpLocale.h"
#include "modules/util/opautoptr.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

class Posix_MDE_MMapping: public MDE_MMapping
{
	void* m_addr;
	size_t m_size;

public:
	Posix_MDE_MMapping() : m_addr(NULL), m_size(0) {}

	OP_STATUS Init(const uni_char* file)
	{
		OpAutoPtr<OpLocale> _locale;
		OpLocale* locale = g_oplocale;
		if (!locale)
		{
			RETURN_IF_ERROR(OpLocale::Create(&locale));
			_locale.reset(locale);
		}

		OpString8 locale_file;
		RETURN_IF_ERROR(locale->ConvertToLocaleEncoding(&locale_file, file));
		int fd = open(locale_file.CStr(), O_RDONLY);
		if (fd < 0)
		{
			return ErrnoToStatus(errno);
		}

		struct stat st;
		int res = fstat(fd, &st);
		if (res < 0)
		{
			int err = errno;
			close(fd);
			return ErrnoToStatus(err);
		}

		size_t size = (size_t)st.st_size;
		OP_ASSERT((size_t)(int)size == size);
		void* addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		int mmap_err = errno;
		close(fd);

		if (addr == MAP_FAILED)
		{
			return ErrnoToStatus(mmap_err);
		}

		m_size = size;
		m_addr = addr;
		return OpStatus::OK;
	}

	virtual ~Posix_MDE_MMapping()
	{
		if (m_addr)
			munmap(m_addr, m_size);
	}

	static OP_STATUS ErrnoToStatus(int err)
	{
		switch (err)
		{
		case ENOMEM: case ENOBUFS: case ENFILE:
			return OpStatus::ERR_NO_MEMORY;
		case ENOENT: case ENOTDIR:
			return OpStatus::ERR_FILE_NOT_FOUND;
		case EACCES:
			return OpStatus::ERR_NO_ACCESS;
		default:
			return OpStatus::ERR;
		}
	}

	virtual void* GetAddress() { return m_addr; }
	virtual int GetSize() { return m_size; }
};

/* static */
OP_STATUS MDE_MMapping::Create(MDE_MMapping** m, const uni_char* file)
{
	*m = NULL;

	Posix_MDE_MMapping* ret = OP_NEW(Posix_MDE_MMapping, ());
	if (!ret)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS stat = ret->Init(file);
	if (OpStatus::IsSuccess(stat))
	{
		*m = ret;
	}
	else
	{
		OP_DELETE(ret);
	}

	return stat;
}
#endif // POSIX_OK_MDE_MMAP
