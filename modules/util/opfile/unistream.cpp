/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file unistream.cpp
 * This file contains code for stream classes that support Unicode.
 */

#include "core/pch.h"

#include "modules/encodings/charconverter.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/util/opfile/unistream.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opcrypt.h"

// ===== Functions =======================================================

UnicodeInputStream* createUnicodeInputStream(OpFileDescriptor *inFile, URLContentType content, OP_STATUS *status, BOOL stripbom)
{
	UnicodeFileInputStream *in = OP_NEW(UnicodeFileInputStream, ());
	if (!in)
	{
		if (status) *status = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}
	if (OpStatus::IsError(in->Construct(inFile,content,stripbom)))
	{
		if (status) *status = OpStatus::ERR_FILE_NOT_FOUND;
		OP_DELETE(in);
		return NULL;
	}
	return in;
}

// ===== UnicodeFileInputStream ==========================================

UnicodeFileInputStream::UnicodeFileInputStream() :
	m_fp(0),
	m_tmpbuf(NULL),
	m_buflen(0),
	m_bytes_in_buf(0),
	m_buf(NULL),
	m_conv(NULL),
	m_bytes_converted(0)
{
}


OP_STATUS UnicodeFileInputStream::Construct(OpFileDescriptor *inFile, URLContentType content, BOOL stripbom)
{
	m_stripbom = stripbom;

	OP_STATUS rc = SharedConstruct(inFile);
	if (OpStatus::IsSuccess(rc))
	{
		unsigned long size = (unsigned long)m_bytes_in_buf;
		const char *encoding = NULL;
		switch(content)
		{
		// FIXME: Duplicated
		case URL_CSS_CONTENT:
			encoding = CharsetDetector::GetCSSEncoding(m_tmpbuf, size);
			break;

		case URL_HTML_CONTENT:
			encoding = CharsetDetector::GetHTMLEncoding(m_tmpbuf, size);
			break;

		case URL_XML_CONTENT:
			encoding = CharsetDetector::GetXMLEncoding(m_tmpbuf, size);
			break;

		default:
			break;
		}
		if (!encoding) encoding = "iso-8859-1";

		if (OpStatus::IsError(InputConverter::CreateCharConverter(encoding, &m_conv)))
			rc = InputConverter::CreateCharConverter("iso-8859-1", &m_conv);
	}

	return rc;
}

OP_STATUS UnicodeFileInputStream::Construct(OpFileDescriptor *inFile, LocalContentType content, BOOL stripbom)
{
	m_stripbom = stripbom;

	OP_STATUS rc = SharedConstruct(inFile);
	if (OpStatus::IsSuccess(rc))
	{
		const char * OP_MEMORY_VAR encoding = NULL;
		unsigned long size = (unsigned long)m_bytes_in_buf;
		switch (content)
		{
		// FIXME: Duplicated
#ifdef PREFS_HAS_LNG
		case LANGUAGE_FILE:
			encoding = CharsetDetector::GetLanguageFileEncoding(m_tmpbuf, size);
			break;
#endif

		case TEXT_FILE:
			encoding = CharsetDetector::GetUTFEncodingFromBOM(m_tmpbuf, size);
			break;

		case UTF8_FILE:
			encoding = "utf-8";
			break;

		default:
			break;
		}

		if (!encoding)
		{
			// Assume system default
			if (g_op_system_info)
			{
				//Current implementation of GetSystemEncodingL does not leave.
				TRAP(rc, encoding = g_op_system_info->GetSystemEncodingL());
			}
			else
			{
				encoding = "iso-8859-1";
			}
		}

		if (OpStatus::IsSuccess(rc))
		{
			if (OpStatus::IsError(InputConverter::CreateCharConverter(encoding, &m_conv)))
				rc = InputConverter::CreateCharConverter("iso-8859-1", &m_conv);
		}
	}

	return rc;
}

OP_STATUS UnicodeFileInputStream::SharedConstruct(OpFileDescriptor *inFile)
{
    OP_ASSERT(inFile);
    if (inFile==NULL)
        return OpStatus::ERR_NULL_POINTER;

	m_fp = inFile;
	RETURN_IF_ERROR(m_fp->Open(OPFILE_READ));
	m_buflen = UTIL_UNICODE_INPUT_BUFLEN;
	m_tmpbuf = OP_NEWA(char, (unsigned long)m_buflen+1);
	if (m_tmpbuf == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
    m_tmpbuf[m_buflen] = 0;

	m_oom_error = FALSE;
	m_buf = OP_NEWA(uni_char, (unsigned long)m_buflen+1);
	if (m_buf == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
    m_buf[m_buflen] = 0;

	RETURN_IF_ERROR(m_fp->Read(m_tmpbuf, m_buflen, &m_bytes_in_buf));
	m_bytes_converted = 0;

	return OpStatus::OK;
}


OP_STATUS UnicodeFileInputStream::Construct(const uni_char *filename,
                                            URLContentType content,
                                            LocalContentType extended_content,
                                            BOOL stripbom)
{
	m_stripbom = stripbom;

	RETURN_IF_ERROR(m_in_file.Construct(filename));
	RETURN_IF_ERROR(m_in_file.Open(OPFILE_READ));

	m_buflen = UTIL_UNICODE_INPUT_BUFLEN;

	m_tmpbuf = OP_NEWA(char, (unsigned long)m_buflen+1);
	if (m_tmpbuf == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
    m_tmpbuf[m_buflen] = 0;

	m_oom_error = FALSE;

	m_buf = OP_NEWA(uni_char, (unsigned long)m_buflen+1);
	if (m_buf == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
    m_buf[m_buflen] = 0;

	RETURN_IF_ERROR(m_in_file.Read(m_tmpbuf, m_buflen, &m_bytes_in_buf));
	m_bytes_converted = 0;

	const char *encoding = NULL;
	unsigned long size = (unsigned long)m_bytes_in_buf;
	switch(content)
	{
	// FIXME: Duplicated
	case URL_CSS_CONTENT:
		encoding = CharsetDetector::GetCSSEncoding(m_tmpbuf, size);
		break;
	case URL_HTML_CONTENT:
		encoding = CharsetDetector::GetHTMLEncoding(m_tmpbuf, size);
		break;
	case URL_UNDETERMINED_CONTENT:
		switch (extended_content)
		{
#ifdef PREFS_HAS_LNG
		case LANGUAGE_FILE:
			encoding = CharsetDetector::GetLanguageFileEncoding(m_tmpbuf, size);
			break;
#endif

		case TEXT_FILE:
			encoding = CharsetDetector::GetUTFEncodingFromBOM(m_tmpbuf, size);
			break;

		case UTF8_FILE:
			encoding = "utf-8";
			break;
		}
		break;

		default:
			;
	}
	if (!encoding) encoding = "iso-8859-1";

	if (OpStatus::IsError(InputConverter::CreateCharConverter(encoding, &m_conv)))
		return InputConverter::CreateCharConverter("iso-8859-1", &m_conv);
	return OpStatus::OK;
}

BOOL UnicodeFileInputStream::has_more_data()
{
	return ((m_fp && m_fp->IsOpen() && !m_fp->Eof())
		|| (OpFileLength)m_bytes_converted < m_bytes_in_buf*2 && !m_oom_error);
}

BOOL UnicodeFileInputStream::no_errors()
{
	return (m_in_file.IsOpen() || m_fp) && !m_oom_error;
}

uni_char* UnicodeFileInputStream::get_block(int& length)
{
	length = 0;
	if (!no_errors())
	{
		return NULL;
	}

	if ((OpFileLength)m_bytes_converted < m_bytes_in_buf)
	{
		// Convert the data still in the current buffer
		int tmp;
		length = m_conv->Convert(m_tmpbuf + m_bytes_converted, (unsigned long)m_bytes_in_buf - m_bytes_converted,
			                     m_buf, (unsigned long)m_buflen * 2, &tmp);
		if (length == -1)
		{
			length = 0;
			m_oom_error = TRUE;
			return NULL;
		}
		m_bytes_converted += tmp;
	}
	else
	{
		// Get a new chunk of data and convert it
		OP_STATUS s;
		if (m_fp)
		{
			s = m_fp->Read(m_tmpbuf, m_buflen, &m_bytes_in_buf);
		}
		else
		{
			s = m_in_file.Read(m_tmpbuf, m_buflen, &m_bytes_in_buf);
		}

		m_oom_error = (OpStatus::IsMemoryError(s));
		if (m_oom_error)
		{
			return NULL;
		}
		m_bytes_converted = 0;
		length = m_conv->Convert(m_tmpbuf, (unsigned long)m_bytes_in_buf, m_buf, (unsigned long)m_buflen * 2, &m_bytes_converted);
		if (length == -1)
		{
			length = 0;
			m_oom_error = TRUE;
			return NULL;
		}
	}

	// Bug#94132, #130048: Strip initial byte order mark (U+FEFF), if we need to.
	if (m_stripbom && length)
	{
		m_stripbom = FALSE;
		if (m_buf[0] == 0xFEFF)
		{
			op_memmove(m_buf, m_buf + 1, length - sizeof (uni_char));
			length -= sizeof (uni_char);
		}
	}

	return m_buf;
}

UnicodeFileInputStream::~UnicodeFileInputStream()
{
	if(m_fp)
		m_fp->Close();
	else
		m_in_file.Close();

	OP_DELETEA(m_tmpbuf);
	OP_DELETEA(m_buf);
	OP_DELETE(m_conv);
}

#ifdef UTIL_STRING_STREAM

// ===== UnicodeStringOutputStream =========================================

void UnicodeStringOutputStream::WriteStringL(const uni_char* str, int len)
{
	if (!str || !m_string || len==0)
		return;

	if (len == -1)
	{
		len = uni_strlen(str);
	}

	if ((m_stringlen + len) > m_string->Capacity())
	{
		m_string->Reserve((m_string->Capacity() + len)*2);
	}

	if (m_string->Append(str, len) == OpStatus::OK)
		m_stringlen += len;
}

OP_STATUS UnicodeStringOutputStream::Construct(const OpString* string)
{
	if (!string)
		return OpStatus::ERR_NULL_POINTER;

	m_string = (OpString*)string;
	m_stringlen = m_string->Length();

	return OpStatus::OK;
}

OP_STATUS UnicodeStringOutputStream::Close()
{
	return OpStatus::OK;
}

#endif // UTIL_STRING_STREAM

// ===== UnicodeFileOutputStream =========================================

UnicodeFileOutputStream::UnicodeFileOutputStream() :
	m_tmpbuf(NULL),
	m_buflen(0),
	m_bytes_in_buf(0),
	m_buf(NULL),
	m_conv(NULL)
{
}

UnicodeFileOutputStream::~UnicodeFileOutputStream()
{
	OpStatus::Ignore(Close());

	OP_DELETEA(m_tmpbuf);
	m_tmpbuf = NULL;
	OP_DELETEA(m_buf);
	m_buf = NULL;
	OP_DELETE(m_conv);
	m_conv = NULL;
}

OP_STATUS UnicodeFileOutputStream::Close()
{
	OP_STATUS rc = OpStatus::OK;
	if (m_file.IsOpen())
	{
		rc = Flush();
		if (OpStatus::IsSuccess(rc))
			rc = m_file.Close();
		else
			return rc;
	}

	OP_DELETEA(m_tmpbuf);
	m_tmpbuf = NULL;
	OP_DELETEA(m_buf);
	m_buf = NULL;
	OP_DELETE(m_conv);
	m_conv = NULL;
	return rc;
}

OP_STATUS UnicodeFileOutputStream::Construct(const uni_char* filename, const char* encoding)
{
	RETURN_IF_ERROR(m_file.Construct(filename));
	RETURN_IF_ERROR(m_file.Open(OPFILE_WRITE));

	m_buflen = (int)(UTIL_UNICODE_OUTPUT_BUFLEN * 1.5) + 10;
	m_tmpbuf = OP_NEWA(char, m_buflen+1);
	if (!m_tmpbuf)
		return OpStatus::ERR_NO_MEMORY;

    m_tmpbuf[m_buflen] = 0;

	m_buf = OP_NEWA(uni_char, UTIL_UNICODE_OUTPUT_BUFLEN+1);
	if (!m_buf)
		return OpStatus::ERR_NO_MEMORY;

    m_buf[UTIL_UNICODE_OUTPUT_BUFLEN] = 0;

	OP_STATUS rc = OutputConverter::CreateCharConverter(encoding, &m_conv, FALSE, TRUE);

	if (!m_tmpbuf || !m_buf || OpStatus::IsError(rc))
	{
		m_file.Close();
		return OpStatus::IsError(rc) ? rc : OpStatus::ERR_NO_MEMORY;
	}

    return OpStatus::OK;
}

void UnicodeFileOutputStream::WriteStringL(const uni_char* str, int len)
{
	if (!str)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	if (len == -1)
	{
		len = uni_strlen(str);
	}

	// strategy: fill up unicode buffer first. when full, do conversion
	// and write to file.
	len *= sizeof(uni_char);
	char* srcbuf = (char*)str;
	while (len)
	{
		int bufspace = UTIL_UNICODE_OUTPUT_BUFLEN - m_bytes_in_buf;
		int copylen = len > bufspace ? bufspace : len;

		op_memcpy(((char*)m_buf)+m_bytes_in_buf, srcbuf, copylen); // don't copy \0-termination
		m_bytes_in_buf += copylen;
		len -= copylen;
		srcbuf += copylen;

		if (m_bytes_in_buf == UTIL_UNICODE_OUTPUT_BUFLEN)
		{
			LEAVE_IF_ERROR(Flush());
		}
	}
}


OP_STATUS UnicodeFileOutputStream::Flush()
{
	if (m_bytes_in_buf == 0)
	{
		return OpStatus::OK;
	}

	int read;
	int m_bytes_converted = m_conv->Convert(m_buf, m_bytes_in_buf,
		                                    m_tmpbuf, m_buflen, &read);
	if (m_bytes_converted == -1)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		OP_ASSERT(read == m_bytes_in_buf); // must consume all input

		RETURN_IF_ERROR(m_file.Write(m_tmpbuf, m_bytes_converted));
		// Please note that we flush the buffer below, even if we fail
		// to save it. This should most probably be considered a bug,
		// but until we actually tell our caller we have an error, not
		// flushing the buffer would throw us into an infinite loop.
	}
	m_bytes_in_buf = 0;

	return OpStatus::OK;
}
