/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"
#include "modules/search_engine/log/Log.h"

#ifdef SEARCH_ENGINE

#include "modules/pi/OpSystemInfo.h"

#include "modules/search_engine/log/FileLogger.h"

#ifdef QUICK
// used for MoveLogFile
#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#endif

OutputLogDevice *SearchEngineLog::CreateLog(int device, ...)
{
	va_list args;
	const uni_char *fname;
	OpFileFolder folder;
	FileLogger *flogger;

	switch (device & 255)
	{
	case SearchEngineLog::File:
	case SearchEngineLog::FolderFile:
		{
			if ((flogger = OP_NEW(FileLogger, ((device & SearchEngineLog::LogOnce) != 0))) == NULL)
				return NULL;

			va_start(args, device);
			fname = va_arg(args, const uni_char *);
			if ((device & 255) == SearchEngineLog::FolderFile)
				folder = (OpFileFolder)va_arg(args, int);
			else
				folder = OPFILE_ABSOLUTE_FOLDER;
				
			va_end(args);

			if (OpStatus::IsError(flogger->Open((device & SearchEngineLog::FOverwrite) != 0, fname, folder)))
			{
				OP_DELETE(flogger);
				return NULL;
			}

			return flogger;
		}
	}

	return NULL;
}

#ifdef QUICK
#define MoveLogFile DesktopOpFileUtils::Move
#else
OP_STATUS MoveLogFile(OpFile *old_location, OpFile *new_location)
{
	OP_STATUS err;

	err = new_location->CopyContents(old_location, TRUE);

	if (OpStatus::IsError(err))
		return err;

	old_location->Delete();

	return OpStatus::OK;
}
#endif

static const uni_char *MkFName(OpString &fname, const uni_char *base, int num)
{
	if (OpStatus::IsError(fname.Set(base)))
		return NULL;

	if (num <= 0)
		return fname.CStr();

	if (OpStatus::IsError(fname.AppendFormat(UNI_L(".%i"), num)))
		return NULL;
	return fname.CStr();
}

OP_STATUS SearchEngineLog::ShiftLogFile(int max_files, const uni_char *filename, OpFileFolder folder)
{
	OpString logfname;
	OpFile f, mf;
	int i;
	BOOL exists;

	if (logfname.Reserve((int)uni_strlen(filename) + 5) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	i = max_files;

	RETURN_IF_ERROR(f.Construct(MkFName(logfname, filename, i), folder));
	if (OpStatus::IsError(f.Exists(exists)))
		exists = TRUE;  // try to delete it anyway

	while (exists && i <= 100)
	{
		RETURN_IF_ERROR(f.Delete());

		++i;

		RETURN_IF_ERROR(f.Construct(MkFName(logfname, filename, i), folder));
		if (OpStatus::IsError(f.Exists(exists)))
			exists = FALSE;  // to avoid infinite loop
	}

	i = max_files - 1;

	while (i >= 0)
	{
		RETURN_IF_ERROR(f.Construct(MkFName(logfname, filename, i), folder));
		RETURN_IF_ERROR(f.Exists(exists));

		if (exists)
		{
			RETURN_IF_ERROR(mf.Construct(MkFName(logfname, filename, i + 1), folder));
			RETURN_IF_ERROR(MoveLogFile(&f, &mf));
		}

		--i;
	}

	return OpStatus::OK;
}


InputLogDevice *SearchEngineLog::OpenLog(const uni_char *filename, OpFileFolder folder)
{
	InputLogDevice *ld;

	if ((ld = OP_NEW(InputLogDevice, ())) == NULL)
		return NULL;

	if (OpStatus::IsError(ld->Construct(filename, folder)))
	{
		OP_DELETE(ld);
		return NULL;
	}

	return ld;
}

int OutputLogDevice::Hash(const char *data, int size)
{
	const char *endp = data + size;
	int hash = 0;

	while (data < endp)
	{
		hash = ((hash << 5) + hash) + *data++; // hash*33 + c
	}

	return hash;
}

const char *SearchEngineLog::SeverityStr(int severity)
{
	int i = 0;

	if (!severity)
		return "";

	while (severity != 1)
	{
		severity = ((unsigned)severity) >> 1;
		++i;
	}

	switch (i)
	{
	case 0:
		return "D";
	case 1:
		return "I";
	case 2:
		return "N";
	case 3:
		return "W";
	case 4:
		return "C";
	case 5:
		return "A";
	case 6:
		return "E";
	}

	return "";
}

OP_BOOLEAN OutputLogDevice::MakeLine(OpString8 &dst, int severity, const char *id, const char *format, va_list args)
{
	int hash;
	time_t current_time;
	struct tm *loc_time;
	double msec;
	OpString8 fmt;

	msec = g_op_time_info->GetWallClockMS();
	op_time(&current_time);
	msec -= (unsigned long)(msec / 1000.0) * 1000.0;

	loc_time = op_localtime(&current_time);

	RETURN_IF_ERROR(fmt.AppendFormat("%.04i-%.02i-%.02iT%.02i:%.02i:%.02i.%.03i %s [%s] %s\r\n",
		loc_time->tm_year + 1900, loc_time->tm_mon + 1, loc_time->tm_mday,
		loc_time->tm_hour, loc_time->tm_min, loc_time->tm_sec, (int)(msec + 0.5),
		SearchEngineLog::SeverityStr(severity),
		id == NULL ? "" : id,
		format));

	RETURN_IF_ERROR(dst.AppendVFormat(fmt.CStr(), args));

	if (!m_once)
		return OpBoolean::IS_TRUE;

	hash = Hash(dst.CStr() + 24, dst.Length() - 26);

	if (hash == m_hash)
		return OpBoolean::IS_FALSE;

	m_hash = hash;
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN OutputLogDevice::MakeLine(OpString8 &dst, int severity, const char *id, const uni_char *format, va_list args)
{
	int hash;
	time_t current_time;
	struct tm *loc_time;
	double msec;
	OpString sev_str, id_str, udst, fmt;

	msec = g_op_time_info->GetWallClockMS();
	op_time(&current_time);
	msec -= (unsigned long)(msec / 1000.0) * 1000.0;

	loc_time = op_localtime(&current_time);

	RETURN_IF_ERROR(sev_str.Set(SearchEngineLog::SeverityStr(severity)));
	RETURN_IF_ERROR(id_str.Set(id));

	RETURN_IF_ERROR(fmt.AppendFormat(UNI_L("%.04i-%.02i-%.02iT%.02i:%.02i:%.02i.%.03i %s [%s] %s\r\n"),
		loc_time->tm_year + 1900, loc_time->tm_mon + 1, loc_time->tm_mday,
		loc_time->tm_hour, loc_time->tm_min, loc_time->tm_sec, (int)(msec + 0.5),
		sev_str.CStr(),
		id_str.CStr() == NULL ? UNI_L("") : id_str.CStr(),
		format));

	RETURN_IF_ERROR(udst.AppendVFormat(fmt.CStr(), args));

	RETURN_IF_ERROR(dst.SetUTF8FromUTF16(udst.CStr()));

	if (!m_once)
		return OpBoolean::IS_TRUE;

	hash = Hash(dst.CStr() + 24, dst.Length() - 26);

	if (hash == m_hash)
		return OpBoolean::IS_FALSE;

	m_hash = hash;
	return OpBoolean::IS_TRUE;
}


OP_STATUS OutputLogDevice::MakeBLine(OpString8 &dst, int severity, const char *id)
{
	time_t current_time;
	struct tm *loc_time;
	double msec;
	OpString8 str_size;

	msec = g_op_time_info->GetWallClockMS();
	op_time(&current_time);
	msec -= (unsigned long)(msec / 1000.0) * 1000.0;

	loc_time = op_localtime(&current_time);

	return dst.AppendFormat("%.04i-%.02i-%.02iT%.02i:%.02i:%.02i.%.03i %s [%s]\\\r\n",
		loc_time->tm_year + 1900, loc_time->tm_mon + 1, loc_time->tm_mday,
		loc_time->tm_hour, loc_time->tm_min, loc_time->tm_sec, (int)(msec + 0.5),
		SearchEngineLog::SeverityStr(severity),
		id == NULL ? "" : id);
}

void OutputLogDevice::Write(int severity, const char *id, const char *format, ...)
{
	va_list args;

	if ((severity & m_mask) == 0)
		return;

	va_start(args, format);
	VWrite(severity, id, format, args);
	va_end(args);
}

void OutputLogDevice::Write(int severity, const char *id, const uni_char *format, ...)
{
	va_list args;

	if ((severity & m_mask) == 0)
		return;

	va_start(args, format);
	VWrite(severity, id, format, args);
	va_end(args);
}


#include "modules/zlib/zlib.h"

InputLogDevice::InputLogDevice(void)
{
	m_time = 0;
	m_severity = (SearchEngineLog::Severity)0;
	m_binary = FALSE;
	m_size = 0;
	m_compressed_size = 0;
	m_data = NULL;
}

InputLogDevice::~InputLogDevice(void)
{
	m_file.Close();

	OP_DELETEA(m_data);
}

OP_STATUS InputLogDevice::Construct(const uni_char *filename, OpFileFolder folder)
{
	RETURN_IF_ERROR(m_file.Construct(filename, folder));

	return m_file.Open(OPFILE_READ);
}

BOOL InputLogDevice::End(void)
{
	OpFileLength pos, size;

	if (!m_file.IsOpen())
		return TRUE;

	if (!m_binary || m_compressed_size == 0)
	{
		if (m_file.Eof())
			return TRUE;

		if (OpStatus::IsError(m_file.GetFilePos(pos)))
			return TRUE;
		if (OpStatus::IsError(m_file.GetFileLength(size)))
			return TRUE;

		return pos >= size;
	}

	if (OpStatus::IsError(m_file.GetFilePos(pos)))
		return TRUE;
	if (OpStatus::IsError(m_file.GetFileLength(size)))
		return TRUE;

	return pos + m_compressed_size + 2 >= size;
}

OP_BOOLEAN InputLogDevice::Read(void)
{
	char *s, *e;
	struct tm iso_time;
	time_t sec_time;
	int i;
	char chr_length[8];  /* ARRAY OK 2010-09-24 roarl */
	OpFileLength pos;

	if (m_data != NULL)
	{
		OP_DELETEA(m_data);
		m_data = NULL;
	}
	else if (m_binary)
	{
		RETURN_IF_ERROR(m_file.GetFilePos(pos));
		RETURN_IF_ERROR(m_file.SetFilePos(pos + m_compressed_size + 2));
	}

	m_size = 0;
	m_compressed_size = 0;
	m_umessage.Empty();

	if (!m_file.IsOpen() || m_file.Eof())
		return OpBoolean::IS_FALSE;


	RETURN_IF_ERROR(m_file.ReadLine(m_message));

	if (m_file.Eof())
		return OpBoolean::IS_FALSE;

	s = m_message.DataPtr();

	// this would be unnecessary if OpFile/OpLowLevelFile wasn't buggy
	e = s + op_strlen(s) - 1;
	while ((*e == '\r' || *e == '\n') && e >= s)
	{
	    *e = 0;
	    --e;
	}
	
	op_memset(&iso_time, 0, sizeof(iso_time));

	e = s + 4;
	if (*e != '-')
		return OpStatus::ERR_PARSING_FAILED;

	iso_time.tm_year = op_atoi(s) - 1900;

	s = e + 1;
	e = s + 2;
	if (*e != '-')
		return OpStatus::ERR_PARSING_FAILED;

	iso_time.tm_mon = op_atoi(s) - 1;

	s = e + 1;
	e = s + 2;
	if (*e != 'T')
		return OpStatus::ERR_PARSING_FAILED;

	iso_time.tm_mday = op_atoi(s);

	s = e + 1;
	e = s + 2;
	if (*e != ':')
		return OpStatus::ERR_PARSING_FAILED;

	iso_time.tm_hour = op_atoi(s);

	s = e + 1;
	e = s + 2;
	if (*e != ':')
		return OpStatus::ERR_PARSING_FAILED;

	iso_time.tm_min = op_atoi(s);

	s = e + 1;
	e = s + 2;
	if (*e != '.')
		return OpStatus::ERR_PARSING_FAILED;

	iso_time.tm_sec = op_atoi(s);

	s = e + 1;
	e = s + 3;
	if (*e != ' ')
		return OpStatus::ERR_PARSING_FAILED;

	if ((sec_time = op_mktime(&iso_time)) == (time_t)-1)
		return OpStatus::ERR_PARSING_FAILED;

	m_time = (double)sec_time;
	m_time *= 1000.0;
	m_time += op_atoi(s);

	i = 0;
	s = e + 1;

	do
	{
		++i;

		e = (char *)SearchEngineLog::SeverityStr(i);
		if (*e == 0)
			return OpStatus::ERR_PARSING_FAILED;

	} while (op_strncmp(s, e, op_strlen(e)) != 0);

	m_severity = (SearchEngineLog::Severity)i;

	s += op_strlen(e) + 1;

	if (*s != '[')
		return OpStatus::ERR_PARSING_FAILED;

	++s;
	e = s;
	while (*e != ']' && *e != 0)
		++e;
	if (*e != ']')
		return OpStatus::ERR_PARSING_FAILED;

	if (e == s)
		m_id.Empty();
	else
		RETURN_IF_ERROR(m_id.Set(s, (int)(e - s)));

	s = e + 1;

	switch (*s)
	{
	case ' ':
		m_binary = FALSE;
		m_message.Delete(0, (int)(s - m_message.CStr() + 1));
		break;
	case '\\':
		m_binary = TRUE;
		m_message.Empty();

		RETURN_IF_ERROR(m_file.Read(chr_length, 8, &pos));
		if (pos != 8)
			return OpStatus::ERR;

		m_size = 0;
		for (i = sizeof(m_size) - 1; i >= 0; --i)
		{
			m_size <<= 8;
			m_size |= (unsigned char)chr_length[i];
		}

		m_compressed_size = 0;

		RETURN_IF_ERROR(m_file.Read(chr_length, 8, &pos));
		if (pos != 8)
			return OpStatus::ERR;

		for (i = sizeof(m_compressed_size) - 1; i >= 0; --i)
		{
			m_compressed_size <<= 8;
			m_compressed_size |= (unsigned char)chr_length[i];
		}

		break;
	default:
		return OpStatus::ERR_PARSING_FAILED;
	}

	return OpBoolean::IS_TRUE;
}

const uni_char *InputLogDevice::UMessage(void)
{
	if (OpStatus::IsError(m_umessage.SetFromUTF8(m_message.CStr())))
		return NULL;
	return m_umessage.CStr();
}

const void *InputLogDevice::Data(void)
{
	OpString buf;
	z_stream decompressor;
	OpFileLength bytes_read;

	if (m_data != NULL)
		return m_data;

	if (buf.Reserve((unsigned)m_compressed_size + 2) == NULL)
		return NULL;

	if ((m_data = OP_NEWA(char, (unsigned)m_size)) == NULL)
		return NULL;

	if (OpStatus::IsError(m_file.Read(buf.DataPtr(), m_compressed_size + 2, &bytes_read)) || bytes_read != m_compressed_size + 2)
	{
		OP_DELETEA(m_data);
		m_data = NULL;
		return NULL;
	}

	decompressor.zalloc = (alloc_func)0;
	decompressor.zfree = (free_func)0;

	decompressor.next_in = (unsigned char *)buf.DataPtr();
	decompressor.avail_in = (unsigned)m_compressed_size;

	decompressor.next_out = (unsigned char *)m_data;
	decompressor.avail_out = (unsigned)m_size;

	if (inflateInit(&decompressor) != 0)
		return NULL;

	inflate(&decompressor, Z_FINISH);

	inflateEnd(&decompressor);

	return m_data;
}

OP_STATUS InputLogDevice::SaveData(const uni_char *file_name, OpFileFolder folder)
{
	OpString8 r_buf, w_buf;  // using OpString as a kind of auto pointer
	z_stream decompressor;
	OpFileLength c_size, bytes_read;
	OpFile f;
	int r_size;

	if (m_compressed_size == 0)
		return OpStatus::ERR;

	if (r_buf.Reserve(4096) == NULL)
		return OpStatus::ERR_NO_MEMORY;
	if (w_buf.Reserve(4096) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	decompressor.zalloc = Z_NULL;
	decompressor.zfree = Z_NULL;
	decompressor.opaque = Z_NULL;
	decompressor.avail_in = 0;
	decompressor.next_in = Z_NULL;
    if (inflateInit(&decompressor) != Z_OK)
        return OpStatus::ERR;
	
	RETURN_IF_ERROR(f.Construct(file_name, folder));
	RETURN_IF_ERROR(f.Open(OPFILE_WRITE));

	c_size = 0;
	while (c_size < m_compressed_size)
	{
		r_size = (int)(c_size + r_buf.Capacity() < m_compressed_size ? r_buf.Capacity() : m_compressed_size - c_size);
		RETURN_IF_ERROR(m_file.Read(r_buf.DataPtr(), r_size, &bytes_read));
		if (bytes_read != static_cast<OpFileLength>(r_size))
			return OpStatus::ERR;
		c_size += r_size;

		decompressor.avail_in = r_size;
		decompressor.next_in = (unsigned char *)r_buf.DataPtr();

		do {
			decompressor.avail_out = w_buf.Capacity();
			decompressor.next_out = (unsigned char *)w_buf.DataPtr();

			inflate(&decompressor, Z_NO_FLUSH);

			if (OpStatus::IsError(f.Write(w_buf.DataPtr(), w_buf.Capacity() - decompressor.avail_out)))
			{
				inflateEnd(&decompressor);
				return OpStatus::ERR_NO_DISK;
			}
		} while (decompressor.avail_out == 0);
	}

	m_file.Read(&r_size, 2, &bytes_read);  // skip \r\n

	inflateEnd(&decompressor);

	f.Close();

	m_binary = FALSE;

	return OpStatus::OK;
}


//#else  // SEARCH_ENGINE

/*****************************************************************************/
/* this is used when logging is disabled                                     */
/*****************************************************************************/


/*int OutputLogDevice::Hash(const char *data, int size)
{
	return 0;
}

const char *OutputLogDevice::SeverityStr(int severity)
{
	return NULL;
}

void OutputLogDevice::Write(int severity, const char *id, const char *format, ...)
{
}

void OutputLogDevice::Write(int severity, const char *id, const uni_char *format, ...)
{
}

class EmptyLogger : public OutputLogDevice
{
	virtual ~EmptyLogger(void) {}

	virtual void Write(SearchEngineLog::Severity severity, const char *id, const char *format, ...) {}
	virtual void Write(SearchEngineLog::Severity severity, const char *id, const uni_char *format, ...) {}
	virtual void Write(SearchEngineLog::Severity severity, const char *id, const void *data, int size) {}
};

OutputLogDevice *SearchEngineLog::CreateLog(Device device, ...)
{
	return OP_NEW(EmptyLogger, ());
}

InputLogDevice *SearchEngineLog::OpenLog(const uni_char *filename, OpFileFolder)
{
	return NULL;
}*/

#endif  // SEARCH_ENGINE
