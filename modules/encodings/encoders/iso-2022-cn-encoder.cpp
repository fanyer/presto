/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/encoders/iso-2022-cn-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

#define ISO2022CN_SBCS 0x0F /* SI: Switch to ASCII mode */
#define ISO2022CN_DBCS 0x0E /* SO: Switch to DBCS mode */

UTF16toISO2022CNConverter::UTF16toISO2022CNConverter()
	: m_cur_charset(ASCII), m_so_initialized(NONE),
	  m_ss2_initialized(FALSE),
	  m_gbk_table1(NULL), m_gbk_table2(NULL),
	  m_cns11643_table1(NULL), m_cns11643_table2(NULL),
	  m_gbk_table1top(0), m_gbk_table2len(0),
	  m_cns116431_table1top(0), m_cns1143_table2len(0)
{
}

OP_STATUS UTF16toISO2022CNConverter::Construct()
{
	if (OpStatus::IsError(this->OutputConverter::Construct())) return OpStatus::ERR;

	long gbk_table1len, cns1143_table1len;

	m_gbk_table1 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("gbk-rev-table-1", gbk_table1len));
	m_gbk_table2 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("gbk-rev-table-2", m_gbk_table2len));

	m_cns11643_table1 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("cns11643-rev-table-1", cns1143_table1len));
	m_cns11643_table2 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("cns11643-rev-table-2", m_cns1143_table2len));

	// Determine which characters are supported by the tables
	m_gbk_table1top = 0x4E00 + gbk_table1len / 2;
	m_cns116431_table1top = 0x4E00 + cns1143_table1len / 2;

	return m_gbk_table1 && m_gbk_table2 && m_cns11643_table1 && m_cns11643_table2 ? OpStatus::OK : OpStatus::ERR;
}

UTF16toISO2022CNConverter::~UTF16toISO2022CNConverter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_gbk_table1);
		g_table_manager->Release(m_gbk_table2);
		g_table_manager->Release(m_cns11643_table1);
		g_table_manager->Release(m_cns11643_table2);
	}
}

int UTF16toISO2022CNConverter::switch_charset(const char *tag, size_t taglen, BOOL need_so, char *output, int spaceleft)
{
	// Don't split character and its escape, make sure we have enough space
	// left in the buffer for the switch and a character
	int lengthneeded = taglen + (need_so ? 1 : 0);
	if (lengthneeded + 2 > spaceleft)
	{
		return 0;
	}
	else
	{
		op_memcpy(output, tag, taglen);
		if (need_so)
		{
			output[taglen] = ISO2022CN_DBCS;
		}
		return lengthneeded;
	}
}

int UTF16toISO2022CNConverter::Convert(const void *src, int len, void *dest,
                                       int maxlen, int *read_p)
{
	int read = 0;
	int written = 0;
	const UINT16 *input = reinterpret_cast<const UINT16 *>(src);
	int utf16len = len / 2;

	char *output = reinterpret_cast<char *>(dest);

	while (read < utf16len && written < maxlen)
	{
		if (*input < 128)
		{
			// ASCII
			if (m_cur_charset != ASCII)
			{
				if (written + 2 > maxlen)
				{
					// Don't split escape code
					goto leave;
				}
				*(output ++) = ISO2022CN_SBCS; // SI
				written ++;
				m_cur_charset = ASCII;
			}

			switch (*input)
			{
			case 0x1B: // <ESC>
			case ISO2022CN_DBCS: // <SO>
				// Always switch back to ASCII for invalid characters
				if (m_cur_charset != ASCII)
				{
					if (written + 2 > maxlen)
					{
						// Don't split escape code
						goto leave;
					}
					*(output ++) = ISO2022CN_SBCS; // SI
					written ++;
					m_cur_charset = ASCII;
				}
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
				if (!CannotRepresent(*input, read, &output, &written, maxlen))
				{
					goto leave;
				}
#else
				*(output ++) = '?';
				CannotRepresent(*input, read);
#endif
				break;

			case 0x0A: // <LF>
			case 0x0D: // <CR>
			case 0x00: // <nul>
				// CR and LF and end-of-string reset state
				m_so_initialized = NONE;
				m_ss2_initialized = FALSE;
				/* Fall through */
			default:
				*(output ++) = static_cast<char>(*input);
			}
			written ++;
		}
		else
		{
			char gb2312_rowcell[2] = { 0, 0 };
			char cns11643_dbcsdata[2] = { 0, 0 };
			char cns11643_plane = 0;
			char cns11643_rowcell[2] = { 0, 0 };

			// Try to find the character in the currently active SO
			// charset first. If that fails, try to find it in the
			// others:
			//
			// GB2312 -> TRY_CNS_11643 -> FAIL
			// CNS_11643 -> TRY_GB2312 -> FAIL
			// (ASCII ->) TRY_GB2312 -> TRY_CNS_11643 -> FAIL
			BOOL found = FALSE;
			enum cn_charset new_charset =
				(m_cur_charset == ASCII) ? TRY_GB2312 : m_cur_charset;
			do
			{
				switch (new_charset)
				{
				case GB2312:
				case TRY_GB2312:
					if (*input >= 0x4E00 && *input < m_gbk_table1top)
					{
						gb2312_rowcell[1] = m_gbk_table1[(*input - 0x4E00) * 2];
						gb2312_rowcell[0] = m_gbk_table1[(*input - 0x4E00) * 2 + 1];
					}
					else
					{
						lookup_dbcs_table(m_gbk_table2, m_gbk_table2len, *input, gb2312_rowcell);
					}

					// Since we're using a GBK table, we have codepoints
					// outside GB2312's range. We treat those as undefined.
					if (static_cast<unsigned char>(gb2312_rowcell[0]) > 0xA0 &&
						static_cast<unsigned char>(gb2312_rowcell[0]) < 0xFE &&
						static_cast<unsigned char>(gb2312_rowcell[1]) > 0xA0)
					{
						// This codepoint is valid in ISO 2022-CN (but might
						// be undefined in GB2312, but we ignore that case)

						if (m_cur_charset != GB2312)
						{
							if (m_so_initialized != GB2312)
							{
								size_t bytes_added =
									switch_charset("\x1B$)A", 4, (m_cur_charset == ASCII), output, maxlen - written);

								if (!bytes_added)
								{
									// Don't split
									goto leave;
								}

								written += bytes_added;
								output += bytes_added;
							}
							else
							{
								if (written + 3 > maxlen)
								{
									// Don't split
									goto leave;
								}

								*(output ++) = ISO2022CN_DBCS;
								written ++;
							}

							m_so_initialized = m_cur_charset = GB2312;
						}
						else if (written + 2 > maxlen)
						{
							// Don't split character
							goto leave;
						}

						// Our table is in EUC-CN encoding, so we strip the high-bit
						*(output ++) = gb2312_rowcell[0] & 0x7F;
						*(output ++) = gb2312_rowcell[1] & 0x7F;
						written += 2;

						found = TRUE;
					}
					else
					{
						// Not found in (or valid for) GB2312; try next encoding
						new_charset = (m_cur_charset == CNS_11643_1) ? FAIL : TRY_CNS_11643;
					}
					break;

				case CNS_11643_1:
				case TRY_CNS_11643:
					if (*input >= 0x4E00 && *input < m_cns116431_table1top)
					{
						cns11643_dbcsdata[1] = m_cns11643_table1[(*input - 0x4E00) * 2];
						cns11643_dbcsdata[0] = m_cns11643_table1[(*input - 0x4E00) * 2 + 1];
					}
					else
					{
						lookup_dbcs_table(m_cns11643_table2, m_cns1143_table2len, *input, cns11643_dbcsdata);
					}

					if (cns11643_dbcsdata[0] != 0 && cns11643_dbcsdata[1] != 0)
					{
						// Convert into plane, row and cell data:
						// What we have is a little-endian number where the two
						// most significant bits are the plane (1 = plane 1, 2 =
						// plane 2, 3 = plane 14), the next seven bits is the row
						// and the seven least significant bits are the cell.
						unsigned int value =
							(static_cast<unsigned char>(cns11643_dbcsdata[0]) << 8) |
							 static_cast<unsigned char>(cns11643_dbcsdata[1]);
						cns11643_plane = static_cast<char>(value >> 14);
						cns11643_rowcell[0] = ((value >> 7) & (0x7F));
						cns11643_rowcell[1] = (value & 0x7F);
					}

					// ISO 2022-CN only supports plane 1 and 2
					if (1 == cns11643_plane)
					{
						if (m_cur_charset != CNS_11643_1)
						{
							if (m_so_initialized != CNS_11643_1)
							{
								size_t bytes_added =
									switch_charset("\x1B$)G", 4, (m_cur_charset == ASCII), output, maxlen - written);

								if (!bytes_added)
								{
									// Don't split
									goto leave;
								}

								written += bytes_added;
								output += bytes_added;
							}
							else
							{
								if (written + 3 > maxlen)
								{
									// Don't split
									goto leave;
								}

								*(output ++) = ISO2022CN_DBCS;
								written ++;
							}

							m_so_initialized = m_cur_charset = CNS_11643_1;
						}
						else if (written + 2 > maxlen)
						{
							// Don't split character
							goto leave;
						}

						*(output ++) = cns11643_rowcell[0];
						*(output ++) = cns11643_rowcell[1];
						written += 2;

						found = TRUE;
					}
					else if (2 == cns11643_plane)
					{
						if (!m_ss2_initialized)
						{
							// Need to initialize the CNS 11643 plane 2
							size_t bytes_added =
								switch_charset("\x1B$*H", 4, FALSE, output, maxlen - written);

							if (!bytes_added)
							{
								// Don't split
								goto leave;
							}

							written += bytes_added;
							output += bytes_added;

							m_ss2_initialized = TRUE;
						}

						if (written + 4 > maxlen)
						{
							// Don't split
							goto leave;
						}

						*(output ++) = 0x1B;
						*(output ++) = 0x4E;
						*(output ++) = cns11643_rowcell[0];
						*(output ++) = cns11643_rowcell[1];

						written += 4;

						found = TRUE;
					}
					else
					{
						// Character in another plane than 1 or 2, or an
						// unconvertible character
						new_charset = (m_cur_charset == CNS_11643_1) ? TRY_GB2312 : FAIL;
					}
					break;
				}
			} while (!found && FAIL != new_charset);

			if (!found)
			{
				// Always switch back to ASCII for invalid characters
				if (m_cur_charset != ASCII)
				{
					if (written + 2 > maxlen)
					{
						// Don't split
						goto leave;
					}
					*(output ++) = ISO2022CN_SBCS;
					written ++;
					m_cur_charset = ASCII;
				}
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
				if (!CannotRepresent(*input, read, &output, &written, maxlen))
				{
					goto leave;
				}
#else
				*(output ++) = '?';
				written ++;
				CannotRepresent(*input, read);
#endif
			}
		}

		read ++; // Counting UTF-16 characters, not bytes
		input ++;
	}

	// In ISO 2022-CN, we want to switch back to ASCII at end of line,
	// so if we have read all the input bytes and have space left in the
	// output buffer, we switch back.
	if (read == utf16len && written < maxlen)
	{
		written += ReturnToInitialState(output);
		// output pointer is now invalid
	}

leave:
	*read_p = read * 2; // Counting bytes, not UTF-16 characters
	m_num_converted += read;
	return written;
}	

const char *UTF16toISO2022CNConverter::GetDestinationCharacterSet()
{
	return "iso-2022-cn";
}

int UTF16toISO2022CNConverter::ReturnToInitialState(void *dest)
{
	int written = 0;
	if (ASCII != m_cur_charset)
	{
		// Switch back to ASCII mode
		if (dest)
		{
			char *output = reinterpret_cast<char *>(dest);
			*output = ISO2022CN_SBCS;
			m_cur_charset = ASCII;
		}
		++ written;
	}

	// Reset state
	if (dest)
	{
		m_so_initialized = NONE;
		m_ss2_initialized = FALSE;
	}

	return written;
}

void UTF16toISO2022CNConverter::Reset()
{
	this->OutputConverter::Reset();
	m_cur_charset = ASCII;
	m_so_initialized = NONE;
	m_ss2_initialized = FALSE;
}

int UTF16toISO2022CNConverter::LongestSelfContainedSequenceForCharacter()
{
	switch (m_cur_charset)
	{
	case ASCII:
		// Worst case: A character in the non-initialized SO charset
		// Initialize SO charset: 4 bytes
		// Output SO character:   1 byte
		// Output character:      2 bytes
		// Switch to ASCII mode:  1 byte
		return 4 + 1 + 2 + 1;

		// Non-worst case: A character in SS2 charset, with SS2 uninitialized
		// Initialize SS2 charset: 4 bytes
		// Output SS2 switch:      2 bytes
		// Output character:       2 bytes

	case CNS_11643_1:
	case GB2312:
	default:
		if (m_ss2_initialized)
		{
			// Worst case: A character in the other SO charset
			// Initialize SO charset: 4 bytes
			// Output character:      2 bytes
			// Switch to ASCII mode:  1 byte
			return 4 + 2 + 1;

			// SS2 character requires 2 + 2 bytes here
		}
		else
		{
			// Worst case: A character in the SS2 charset
			// Initialize SS2 charset: 4 bytes
			// Output SS2 switch:      2 bytes
			// Output character:       2 bytes
			// Switch to ASCII mode:   1 byte
			return 4 + 2 + 2 + 1;
		}
	}
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
