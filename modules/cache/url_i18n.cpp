/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
** Peter Krefting
**
*/

#include "core/pch.h"
#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/cache/url_i18n.h"

#include "modules/hardcore/mh/messages.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/url/url2.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"

#include "modules/encodings/charconverter.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/utility/createinputconverter.h"
#include "modules/encodings/utility/charsetnames.h"

#ifdef DATADESC_BUFFER_SIZE
# if DATADESC_BUFFER_SIZE % 2
#  error "DATADESC_BUFFER_SIZE must be even, or UNICODE_DOWNSIZE shall wreak havoc"
# endif
#else
#  define DATADESC_BUFFER_SIZE (8192-128)
#endif // DATADESC_BUFFER_SIZE

// ========================================================================================

CharacterDecoder::CharacterDecoder(unsigned short parent_charset, Window *window)
	: m_converter(NULL),
	m_detector(NULL),
	m_buffer_used(0),
	m_parent_charset(parent_charset),
	m_first_run(true),
	m_window(window)
{
	g_charsetManager->IncrementCharsetIDReference(m_parent_charset);
	m_detect_charset = true;
	m_buffer = NULL;
}

OP_STATUS CharacterDecoder::Construct()
{
	m_buffer = OP_NEWA(char, DATADESC_BUFFER_SIZE);
	return m_buffer != NULL ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

CharacterDecoder::~CharacterDecoder()
{
	g_charsetManager->DecrementCharsetIDReference(m_parent_charset);
	OP_DELETE(m_converter);
	OP_DELETE(m_detector);
	OP_DELETEA(m_buffer);
}

unsigned int CharacterDecoder::ReadData(char *buf, unsigned int blen, HTTP_1_1 *conn, BOOL &read_from_connection)
{
	return 0; // don't do conversion when moving to cache
}

// buf:          destination buffer
// blen:         max number of bytes to write to destination
// read_storage: used by Data_Decoder::ReadData to determine where to read data from
// more:         whether there is more data (we leave it to Data_Decoder::ReadData to find out)
// return value: number of bytes written into destination buffer

unsigned int CharacterDecoder::ReadData(char *buf, unsigned int blen, URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more)
{
	if(m_buffer == NULL)
		return 0;

	// Sometimes we have old data in the buffer, but we still peek at the whole
	// buffer to determine character set. Unnecessary, and when using OPTIMIZE_CHARSETDETECTION
	// we will stop much earlier than necessary, which may cause some autodetection to fail.
	int old_m_buffer_used = m_buffer_used;

	if (m_buffer_used < DATADESC_BUFFER_SIZE)
		m_buffer_used += Data_Decoder::ReadData(m_buffer + m_buffer_used, DATADESC_BUFFER_SIZE - m_buffer_used,
											  desc, read_storage, more);
    else
        // Yngve, dubious fix here

        more = !source_is_finished;

	if (m_converter == NULL)
	{
		if(m_buffer_used == 0 && !source_is_finished) // Some content types need to know a bit of the content if they are to detect charset properly, so return if more content is expected.
			return 0;
		m_converter = createInputConverter(desc, m_buffer, m_buffer_used, m_window, m_parent_charset);
		if (NULL == m_converter)
			return 0;
	}
	if (m_detect_charset && m_detector == NULL)
	{
		// Get URL object
		URL url = desc->GetURL();

		// Check if user has overriden the character set detection
		Window *window = m_window ? m_window : desc->GetDocumentWindow();
		const char *override = NULL;
		if (window)
		{
			override = window->GetForceEncoding();
		}
		else
		{
			override = g_pcdisplay->GetForceEncoding();
		}
		if (override && strni_eq(override, "AUTODETECT-", 11))
			override = NULL;

		// Check for HTTP specified character set
		if (url.GetAttribute(URL::KMIME_CharSet).HasContent() || (override && *override))
		{
			// No need to waste cycles on detection
			m_detect_charset = false;
		}
		else
		{
			// Setup charset detector object
			//FIXME:OOM (should report)
			m_detector = OP_NEW(CharsetDetector, (url.GetAttribute(URL::KHostName).CStr(),
			                                 window, NULL,
											 url.GetAttribute(URL::KHTTPContentLanguage).CStr()
											 ));
			if (NULL == m_detector)
			{
				m_detect_charset = false;
			}
		}
	}

	int bytes_converted = 0;
	int written = m_converter -> Convert(m_buffer, m_buffer_used, buf, blen, &bytes_converted);
	// written = -1 means OOM

	// We remove UTF16 BOMs if they are present, that needs to have at least two bytes present.
	if(m_first_run && written <2 && !source_is_finished)
		return 0;

	// Bug#94132, #130048: Strip initial byte order mark (U+FEFF)
	if (m_first_run && written > 1 &&
#ifdef OPERA_BIG_ENDIAN
		buf[0] == '\xFE' && buf[1] == '\xFF'
#else
		buf[1] == '\xFE' && buf[0] == '\xFF'
#endif
	                                                )
	{
		op_memmove(buf, buf + 2, written - 2);
		written -= 2;
	}
	m_first_run = false;

	if (m_detect_charset && m_detector)
	{
		m_detector->PeekAtBuffer(m_buffer + old_m_buffer_used, m_buffer_used-old_m_buffer_used);
	}

	if (m_buffer_used > bytes_converted)
	{
		// moving unconverted part of m_buffer to beginning of m_buffer
		// may overlap, so use memmove
		op_memmove(m_buffer, m_buffer + bytes_converted, m_buffer_used - bytes_converted);
		if(!more && desc && !desc->PostedMessage())
			desc->PostMessage(MSG_URL_DATA_LOADED, desc->GetURL().Id(), 0);
		more = TRUE;
	}

	// Some decoders will not consume data if they cannot produce output,
	// so if the source is finished, nothing got converted and there is
	// enough space in the buffer, we force termination.
	if (source_is_finished && !bytes_converted && blen > 10)
	{
		more = FALSE;
	}

	if(source_is_finished && !more)
		finished = TRUE;

	if(written < 0)
	{
		finished = FALSE;
		error = TRUE;
		more = FALSE;
		written = 0;
	}

	m_buffer_used -= bytes_converted;

	return written;
}


const char *CharacterDecoder::GetCharacterSet()
{
	if (m_converter)
		return m_converter->GetCharacterSet();
	else
		return NULL;
}

const char *CharacterDecoder::GetDetectedCharacterSet()
{
	if (m_detector)
		return m_detector->GetDetectedCharset();
	else
		return NULL;
}

void CharacterDecoder::StopGuessingCharset()
{
	OP_DELETE(m_detector);
	m_detector = NULL;
	m_detect_charset = false;
}

int CharacterDecoder::GetFirstInvalidCharacterOffset()
{
	return m_converter ? m_converter->GetFirstInvalidOffset() : -1;
}
