/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _HTTP_TE_H_
#define _HTTP_TE_H_

#include "modules/url/protocols/http1.h"

class HTTP_1_1;
class URL_DataDescriptor;

#include "modules/util/simset.h"

class Data_Decoder : public Link
{
protected:
	BOOL source_is_finished;
	BOOL finished;
	BOOL error;
	BOOL can_error_be_hidden;

public:
	Data_Decoder();
	virtual ~Data_Decoder();

	BOOL Finished(){return finished;}
	BOOL Error();
	BOOL CanErrorBeHidden();

	virtual unsigned int ReadData(char *buf, unsigned int blen, HTTP_1_1 *conn, BOOL &read_from_connection);

	virtual unsigned int ReadData(char *buf, unsigned int blen, URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more);

	/** Get character set associated with this Data_Decoder.
	  * @return NULL if no character set is associated
	  */
	virtual const char *GetCharacterSet() { return NULL; }
	/** Get character set detected for this CharacterDecoder.
	  * @return Name of character set, NULL if no character set can be
	  *         determined.
	  */
	virtual const char *GetDetectedCharacterSet() { return NULL; };
	/** Stop guessing character set for this document.
	  * Character set detection wastes cycles, and avoiding it if possible
	  * is nice.
	  */
	virtual void StopGuessingCharset() {};

	virtual BOOL	IsA(const uni_char *type) { return uni_stricmp(type, UNI_L("Data_Decoder")) == 0; }
};

#ifdef _HTTP_COMPRESS
# include "modules/zlib/zlib.h"

class HTTP_Transfer_Decoding : public Data_Decoder
{
private:  
	HTTP_Transfer_Decoding();
	
	HTTP_compression method;

#ifndef URL_USE_ZLIB_FOR_GZIP
	int (*decompress_step)(z_streamp strm, int flush);
	int (*decompress_final)(z_streamp strm);
#endif

	unsigned char *buffer;
	unsigned int buffer_len;
	unsigned int buffer_used;
	
	z_stream_s	state;

#ifndef URL_USE_ZLIB_FOR_GZIP
	struct{
		BYTE read_header:1;
		BYTE transparent:1;
		BYTE read_magic:1;
		BYTE read_method:1;
		BYTE read_flag:1;
		BYTE read_discarded:1;
		BYTE read_extra_len_lo:1;
		BYTE read_extra_len_hi:1;
		BYTE read_extra:1;
		BYTE read_name:1;
		BYTE read_comment:1;
		BYTE read_crc_1:1;
		BYTE read_crc_2:1;
		BYTE reading_final_crc:1;

		int remaining_bytes;
		unsigned long crc;
		unsigned long file_crc;
	} gzip_state;
#endif
	BOOL deflate_first_byte;
	BOOL first_read;

public:
	OP_STATUS Construct(HTTP_compression mth);
	static HTTP_Transfer_Decoding* Create(HTTP_compression mth);

	virtual ~HTTP_Transfer_Decoding();

	virtual unsigned int ReadData(char *buf, unsigned int blen, HTTP_1_1 *conn, BOOL &read_from_connection);
	virtual unsigned int ReadData(char *buf, unsigned int blen, URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more);

	unsigned int DoDecodingStep(char *buf, unsigned int blen);

#ifndef URL_USE_ZLIB_FOR_GZIP
	void ResetGzip();
	unsigned int DoGzipFirstStep(unsigned char *buf, unsigned int blen);
	unsigned int DoGzipFinalStep(unsigned char *buf, unsigned int blen);
#endif
};
#endif // _HTTP_COMPRESS

#endif // _HTTP_TE_H_
