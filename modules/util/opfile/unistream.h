/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file unistream.h
 * Stream classes converting to/from legacy encodings. This file defines
 * a couple of classes that wrap around OpFile (or OpString), converting
 * to or from legacy encodings.
 *
 * FIXME: These classes should be merged into the OpFile hierarchy, and
 * the UnicodeStringOutputStream replaced with a version that produces
 * an UniString instead.
 */

#ifndef MODULES_UTIL_OPFILE_UNISTREAM_H
#define MODULES_UTIL_OPFILE_UNISTREAM_H

#include "modules/url/url_enum.h"
#include "modules/util/opfile/opfile.h"

class InputConverter;
class OutputConverter;

/**
 * Creates a stream object that can return blocks of data read from a
 * file and correctly converted to Unicode.
 */
class UnicodeInputStream* createUnicodeInputStream(OpFileDescriptor *inFile, URLContentType content, OP_STATUS *retstatus = NULL, BOOL stripbom = FALSE);

// ===== INPUT STREAMS ===================================================

/**
 * Abstract class describing input streams of data in
 * Unicode UTF-16.
 */
class UnicodeInputStream
{
public:
	/** Standard destructor */
	virtual ~UnicodeInputStream() {}

	/** Check if there is more data pending */
	virtual BOOL has_more_data() = 0;

	/** Check that no errors occured while loading */
	virtual BOOL no_errors() = 0;

	/**
	 * Retrieve a chunk of data from the file.
	 * Return a NULL pointer and a length of 0 if an OOM condition
	 * (or possibly other error) occur.
	 * @param length Output: Length in number of bytes retrieved.
	 * @return A pointer to the data read.
	 */
	virtual uni_char* get_block(int& length) = 0;
};

/**
 * Class used to read a file from local storage while converting it
 * to Unicode.
 */
class UnicodeFileInputStream : public UnicodeInputStream
{
public:
	/** Standard first-phase constructor */
	UnicodeFileInputStream();

	/** Standard destructor */
	virtual ~UnicodeFileInputStream();

	/**
	 * Special content types for files that do not have URLContentType
	 * enumerators. The method use to detect the file character encoding
	 * can be changed by using a different local content type.
	 *
	 * - LANGUAGE_FILE: Looks for the "Charset" header in the early part
	 *   of the language file to detect the encoding
	 * - TEXT_FILE: Looks for a Unicode byte order mark, and falls back
	 *   to the system character encoding if none was found.
	 * - UTF8_FILE: Forces the file to be interpreted as UTF-8 data.
	 */
	enum LocalContentType
	{
		LANGUAGE_FILE,	///< Opera LNG file
		TEXT_FILE,		///< Standard text file
		UTF8_FILE,		///< UTF-8 encoded file
		UNKNOWN_CONTENT
	};

	/**
	 * Construct the stream class. Detects the character encoding of the
	 * file based on the content parameter.
	 *
	 * @param inFile File to load from local storage.
	 * @param content URL type of this stream. Only URL_CSS_CONTENT,
	 * URL_HTML_CONTENT and URL_XML_CONTENT are supported.
	 * @param stripbom Strip byte order mark from output.
	 * @return Status for the operation.
	 */
	OP_STATUS Construct(OpFileDescriptor *inFile, URLContentType content,
	                    BOOL stripbom = FALSE);

	/**
	 * Construct the stream class. Detects the character encoding of the
	 * file based on the content parameter.
	 *
	 * @param inFile File to load from local storage.
	 * @param content Local file content which does not have a URL type.
	 * @param stripbom Strip byte order mark from output.
	 * @return Status for the operation.
	 */
	OP_STATUS Construct(OpFileDescriptor *inFile, LocalContentType content,
	                    BOOL stripbom = FALSE);

	/**
	 * Construct the stream class from a file name. Detects the character
	 * encoding of the file based on the content parameter, or the
	 * extended_content parameter if content if content is set to
	 * URL_UNDETERMINED_CONTENT.
	 *
	 * @param filename File to load from local storage.
	 * @param content URL type of this stream.
	 * @param extended_content
	 *  Local file content which does not have a URL type.
	 * @param stripbom Strip byte order mark from output.
	 * @return Status for the operation.
	 */
	OP_STATUS Construct(const uni_char* filename, URLContentType content,
	                    LocalContentType extended_content = UNKNOWN_CONTENT,
	                    BOOL stripbom = FALSE);

	/** Check if there is more data pending */
	virtual BOOL has_more_data();

	/** Check that no errors occured while loading */
	virtual BOOL no_errors();

	/**
	 * Retrieve a chunk of data from the file.
	 * Return a NULL pointer and a length of 0 if an OOM condition
	 * (or possibly other error) occur.
	 * @param length Output: Length in number of bytes retrieved.
	 * @return A pointer to the data read.
	 */
	virtual uni_char* get_block(int& length);

private:
	OP_STATUS SharedConstruct(OpFileDescriptor *inFile);

	OpFileDescriptor* m_fp;
	OpFile m_in_file;
	char *m_tmpbuf;
	OpFileLength m_buflen;
	OpFileLength m_bytes_in_buf;

	uni_char *m_buf;
	InputConverter *m_conv;

	int m_bytes_converted;
	BOOL m_oom_error;
	BOOL m_stripbom;
};


// ===== OUTPUT STREAMS ==================================================

/**
 * Abstract class for Unicode output streams.
 */
class UnicodeOutputStream
{
public:
	/**
	 * Destructor. Implicitly flushes and closes the stream.
	 */
	virtual ~UnicodeOutputStream() {}

	/**
	 * Write the first len code units (not characters) of the string.
	 */
	virtual void WriteStringL(const uni_char* str, int len=-1) = 0;

	/**
	 * Close the stream.
	 */
	virtual OP_STATUS Close() = 0;
};

#ifdef UTIL_STRING_STREAM

/**
 * This stream is used to stream Unicode data to OpString.
 */
class UnicodeStringOutputStream : public UnicodeOutputStream
{
public:
	UnicodeStringOutputStream() : m_string(NULL), m_stringlen(0L) {}

	OP_STATUS Construct(const OpString* string);

	void WriteStringL(const uni_char* str, int len=-1);

	virtual OP_STATUS Close();

private:
	OpString* m_string;
	long	  m_stringlen;
};

#endif // UTIL_STRING_STREAM

/**
 * This stream is used to write Unicode data to files in
 * some encoding.
 */
class UnicodeFileOutputStream : public UnicodeOutputStream
{
public:
	UnicodeFileOutputStream();
	virtual ~UnicodeFileOutputStream();

	/**
	 * Construct a stream that will write data out in the requested
	 * encoding. This method is not idempotent; that is, don't call
	 * it more than once on the same object.
	 * @param filename File to write to.
	 * @param encoding The character encoding to write data to file in.
	 * @return Status for the operation.
	 */
	OP_STATUS Construct(const uni_char* filename, const char* encoding);

	/**
	 * Write the first len code units (not characters) of the string
	 * to the file.
	 */
	virtual void WriteStringL(const uni_char* str, int len=-1);

	/**
	 * Close the file connected to this Unicode stream.
	 * @return Status for the operation.
	 */
	virtual OP_STATUS Close();

private:
	/**
	 * Convert storage buffer to output encoding and write it out.
	 */
	OP_STATUS Flush();

	OpFile m_file;
	char *m_tmpbuf;
	int m_buflen;
	int m_bytes_in_buf;
	uni_char *m_buf;
	OutputConverter *m_conv;
};

#endif // !MODULES_UTIL_OPFILE_UNISTREAM_H
