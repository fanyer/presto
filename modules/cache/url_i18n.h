/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef URL_I18N_H_
#define URL_I18N_H_

#include "modules/url/protocols/http_te.h"
class CharsetDetector;
class CharConverter;
class InputConverter;

/**
 * A Data_Decoder that converts the bytes read from the network
 * to the character encoding used internally by Opera. This decoder
 * is always the last in the HTTP content-encoding decoder stack.
 */

class CharacterDecoder : public Data_Decoder
{
private:
	InputConverter*		m_converter;
	CharsetDetector*	m_detector;
	char*				m_buffer;
	int                 m_buffer_used;
	unsigned short 		m_parent_charset;
	bool				m_detect_charset;
	bool				m_first_run;
	class Window*		m_window;

public:
	/**
	 * Construct the CharacterDecoder object.
	 *
	 * @param parent_charset_id Character encoding (charset) passed from the
	 *   parent resource. Should either be the actual encoding of the
	 *   enclosing resource, or an encoding recommended by it, for instance
	 *   as set by a CHARSET attribute in a SCRIPT tag. Pass 0 if no such
	 *   contextual information is available.
	 * @param window The window in which the document is loading, or to which
	 *   it is associated.
	 */
	CharacterDecoder(unsigned short parent_charset, class Window *window = NULL);
	virtual ~CharacterDecoder();

	OP_STATUS Construct();
    virtual unsigned int ReadData(char *buf, unsigned int blen, HTTP_1_1 *conn, BOOL &read_from_connection);
	virtual unsigned int ReadData(char *buf, unsigned int blen, URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more);
	/** Get character set associated with this CharacterDecoder.
	  * @return Name of character set, NULL if no character decoder has
	  *         been instantiated yet.
	  */
	virtual const char *GetCharacterSet();
	/** Get character set detected for this CharacterDecoder.
	  * @return Name of character set, NULL if no character set can be
	  *         determined.
	  */
	virtual const char *GetDetectedCharacterSet();
	/** Stop guessing character set for this document.
	  * Character set detection wastes cycles, and avoiding it if possible
	  * is nice.
	  */
	virtual void StopGuessingCharset();

	virtual BOOL	IsA(const uni_char *type) { return uni_stricmp(type, UNI_L("CharacterDecoder")) == 0 || Data_Decoder::IsA(type); }

	int GetFirstInvalidCharacterOffset();
};

#endif // !URL_I18N_H_
