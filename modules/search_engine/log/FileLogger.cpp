/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#include "core/pch.h"

#ifdef SEARCH_ENGINE

#include "modules/search_engine/log/FileLogger.h"
#include "modules/zlib/zlib.h"

FileLogger::~FileLogger(void)
{
	m_file.Close();
}

OP_STATUS FileLogger::Open(BOOL truncate, const uni_char *filename, OpFileFolder folder)
{
	BOOL fexists;

	RETURN_IF_ERROR(m_file.Construct(filename, folder));

	if (truncate)
		return m_file.Open(OPFILE_OVERWRITE | OPFILE_COMMIT);

	RETURN_IF_ERROR(m_file.Exists(fexists));

	if (!fexists)
		return m_file.Open(OPFILE_WRITE | OPFILE_COMMIT);

	RETURN_IF_ERROR(m_file.Open(OPFILE_UPDATE | OPFILE_COMMIT));
	return m_file.SetFilePos(0, SEEK_FROM_END);
}

OP_STATUS FileLogger::Clear(void)
{
	if (!m_file.IsOpen())
		return OpStatus::ERR_BAD_FILE_NUMBER;

	m_file.Flush();
	return m_file.SetFileLength(0);
}

void FileLogger::VWrite(int severity, const char *id, const char *format, va_list args)
{
	OpString8 ln;
	OP_BOOLEAN differs;

	if ((severity & m_mask) == 0)
		return;

	differs = MakeLine(ln, severity, id, format, args);

	if (differs != OpBoolean::IS_TRUE)
		return;

	m_file.Write(ln.CStr(), ln.Length());
}

void FileLogger::VWrite(int severity, const char *id, const uni_char *format, va_list args)
{
	OpString8 ln;
	OP_BOOLEAN differs;

	if ((severity & m_mask) == 0)
		return;

	differs = MakeLine(ln, severity, id, format, args);

	if (differs != OpBoolean::IS_TRUE)
		return;

	m_file.Write(ln.CStr(), ln.Length());
}

void FileLogger::Write(int severity, const char *id, int size, const void *data)
{
	OpString8 ln;
	OpString8 w_buf;  // using OpString as a kind of auto pointer
	z_stream compressor;
	int wr_length;
	size_t i;
	OpFileLength spos;
	unsigned char chr_length[8], chr_size[8];  /* ARRAY OK 2010-09-24 roarl */

	if ((severity & m_mask) == 0)
		return;

	if (w_buf.Reserve(size + (size + 9999) / 1000 + 12) == NULL)
		return;

	RETURN_VOID_IF_ERROR(MakeBLine(ln, severity, id));

	compressor.avail_in = (unsigned)size;
	compressor.next_in = (unsigned char *)data;

	compressor.avail_out = w_buf.Capacity();
	compressor.next_out = (unsigned char *)w_buf.DataPtr();

    compressor.zalloc = (alloc_func)0;
    compressor.zfree = (free_func)0;
    compressor.opaque = (voidpf)0;

	if (deflateInit(&compressor, 9) != 0)  // Z_OK
		return;

	deflate(&compressor, Z_FINISH);

	deflateEnd(&compressor);

	wr_length = size;

	op_memset(chr_size, 0, 8);

	for (i = 0; i < sizeof(wr_length); ++i)  // to avoid problems with endians
	{
		chr_size[i] = (unsigned char)(wr_length & 0xFF);
		wr_length >>= 8;
	}

	wr_length = compressor.total_out;

	op_memset(chr_length, 0, 8);

	for (i = 0; i < sizeof(wr_length); ++i)  // to avoid problems with endians
	{
		chr_length[i] = (unsigned char)(wr_length & 0xFF);
		wr_length >>= 8;
	}

	RETURN_VOID_IF_ERROR(m_file.Write(ln.CStr(), ln.Length()));

	RETURN_VOID_IF_ERROR(m_file.Write(chr_size, 8));
	RETURN_VOID_IF_ERROR(m_file.Write(chr_length, 8));

	RETURN_VOID_IF_ERROR(m_file.GetFilePos(spos));

	if (OpStatus::IsError(m_file.Write(w_buf.DataPtr(), compressor.total_out)))
	{
		op_memset(chr_length, 0, 8);

		m_file.SetFileLength(spos);
		m_file.SetFilePos(spos - 8);
		m_file.Write(chr_length, 8);
		m_file.SetFilePos(0, SEEK_FROM_END);
	}

	m_file.Write("\r\n", 2);
}

void FileLogger::WriteFile(int severity, const char *id, const uni_char *file_name, OpFileFolder folder)
{
	OpString8 ln;
	OpFile f;
	OpFileLength spos, epos, bytes_read;
	size_t i;
	OpString8 r_buf, w_buf;  // using OpString as a kind of auto pointer
	z_stream compressor;
	unsigned char chr_length[8], chr_size[8];  /* ARRAY OK 2010-09-24 roarl */
	int compressed_len;

	if ((severity & m_mask) == 0)
		return;

	if (r_buf.Reserve(4096) == NULL)
		return;
	if (w_buf.Reserve(5013) == NULL)
		return;

	RETURN_VOID_IF_ERROR(f.Construct(file_name, folder));
	RETURN_VOID_IF_ERROR(f.Open(OPFILE_READ));

	RETURN_VOID_IF_ERROR(f.GetFileLength(bytes_read));

	op_memset(chr_size, 0, 8);

	for (i = 0; i < sizeof(bytes_read); ++i)  // to avoid problems with endians
	{
		chr_size[i] = (unsigned char)(bytes_read & 0xFF);
		bytes_read >>= 8;
	}

	compressor.zalloc = Z_NULL;
	compressor.zfree = Z_NULL;
	compressor.opaque = Z_NULL;
	
	if (deflateInit(&compressor, 9) != 0)  // Z_OK
		return;

	RETURN_VOID_IF_ERROR(MakeBLine(ln, severity, id));

	RETURN_VOID_IF_ERROR(m_file.Write(ln.CStr(), ln.Length()));

	op_memset(chr_length, 0, 8);

	RETURN_VOID_IF_ERROR(m_file.Write(chr_size, 8));
	RETURN_VOID_IF_ERROR(m_file.Write(chr_length, 8));

	RETURN_VOID_IF_ERROR(m_file.GetFilePos(spos));

	bytes_read = 1;
	while (bytes_read != 0 && !f.Eof())
	{
		if (OpStatus::IsError(f.Read(r_buf.DataPtr(), r_buf.Capacity(), &bytes_read)))
		{
			m_file.SetFilePos(spos);
			m_file.SetFileLength(spos);
			m_file.SetFilePos(spos);
			break;
		}

		compressor.avail_in = (unsigned)bytes_read;

		compressor.next_in = (unsigned char *)r_buf.DataPtr();

		do {
			compressor.avail_out = w_buf.Capacity();
			compressor.next_out = (unsigned char *)w_buf.DataPtr();

			deflate(&compressor, (f.Eof() || bytes_read == 0) ? Z_FINISH : Z_NO_FLUSH);

			compressed_len = w_buf.Capacity() - compressor.avail_out;

			if (OpStatus::IsError(m_file.Write(w_buf.DataPtr(), compressed_len)))
			{
				m_file.SetFilePos(spos);
				m_file.SetFileLength(spos);
				m_file.SetFilePos(spos);
				break;
			}
		} while (compressor.avail_out == 0 && compressed_len > 0);
	}

	f.Close();
	deflateEnd(&compressor);

	if (OpStatus::IsError(m_file.GetFilePos(epos)))
		if (OpStatus::IsError(m_file.GetFileLength(epos)))
		{
			m_file.SetFileLength(spos);
			epos = spos;
		}

	epos -= spos;
	for (i = 0; i < 8U; ++i)  // to avoid problems with endians
	{
		chr_length[i] = (unsigned char)(epos & 0xFF);
		epos >>= 8;
	}

	m_file.SetFilePos(spos - 8);
	m_file.Write(chr_length, 8);
	m_file.SetFilePos(0, SEEK_FROM_END);

	m_file.Write("\r\n", 2);
}

#endif // SEARCH_ENGINE
