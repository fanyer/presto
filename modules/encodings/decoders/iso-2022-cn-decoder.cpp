/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && (defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_KOREAN)

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/decoders/iso-2022-cn-decoder.h"

#define ISO2022_LF   0x0A /* LF: Line break */
#define ISO2022_SBCS 0x0F /* SI: Switch to ASCII mode */
#define ISO2022_DBCS 0x0E /* SO: Switch to DBCS mode */
#define ISO2022_ESC  0x1B /* ESC: Start escape sequence */
#define ISO2022_SS2  0x4E /* SS2: G2 for next character only */

ISO2022toUTF16Converter::ISO2022toUTF16Converter(enum encoding inputencoding)
	:
#ifdef ENCODINGS_HAVE_CHINESE
	  m_gbk_table(NULL), m_cns_11643_table(NULL),
	  m_gbk_length(0),   m_cns_11643_length(0),
#endif
#ifdef ENCODINGS_HAVE_KOREAN
	  m_ksc_5601_table(NULL),
	  m_ksc_5601_length(0),
#endif
	  m_prev_byte(0),
	  m_current_so_charset(NONE),
#ifdef ENCODINGS_HAVE_CHINESE
	  m_current_ss2_charset(NONE),
#endif
	  m_state(ascii), m_last_known_state(ascii),
	  m_my_encoding(inputencoding)
{
}

OP_STATUS ISO2022toUTF16Converter::Construct()
{
	if (OpStatus::IsError(this->InputConverter::Construct())) return OpStatus::ERR;

	BOOL success = TRUE;

	switch (m_my_encoding)
	{
	default:
#ifdef ENCODINGS_HAVE_CHINESE
	case ISO2022CN:
		m_gbk_table =
			reinterpret_cast<const UINT16 *>(g_table_manager->Get("gbk-table", m_gbk_length));
		m_cns_11643_table =
			reinterpret_cast<const UINT16 *>(g_table_manager->Get("cns11643-table", m_cns_11643_length));
		m_gbk_length /= 2; // Adjust from byte count to UINT16 count
		m_cns_11643_length /= 2; // Size was in bytes, we need it in uni_chars
		success = success && m_gbk_table && m_cns_11643_table;
		break;
#endif

#ifdef ENCODINGS_HAVE_KOREAN
	case ISO2022KR:
		m_ksc_5601_table =
			reinterpret_cast<const UINT16 *>(g_table_manager->Get("ksc5601-table", m_ksc_5601_length));
		m_ksc_5601_length /= 2; // Adjust from byte count to UINT16 count
		success = success && m_ksc_5601_table;
		break;
#endif
	}

	return success ? OpStatus::OK : OpStatus::ERR;
}

ISO2022toUTF16Converter::~ISO2022toUTF16Converter()
{
	if (g_table_manager)
	{
#ifdef ENCODINGS_HAVE_CHINESE
		g_table_manager->Release(m_gbk_table);
		g_table_manager->Release(m_cns_11643_table);
#endif
#ifdef ENCODINGS_HAVE_KOREAN
		g_table_manager->Release(m_ksc_5601_table);
#endif
	}
}

BOOL ISO2022toUTF16Converter::identify_charset(char esc1, char esc2)
{
	if (esc1 == ')')
	{
		switch (esc2)
		{
#ifdef ENCODINGS_HAVE_CHINESE
		case 'A': m_current_so_charset = GB2312; break;
		case 'G': m_current_so_charset = CNS_11643_1; break;
#endif
#ifdef ENCODINGS_HAVE_KOREAN
		case 'C': m_current_so_charset = KSC_5601; break;
#endif
		default: return FALSE;
		}
		return TRUE;
	}
#ifdef ENCODINGS_HAVE_CHINESE
	else if (esc1 == '*')
	{
		if (esc2 == 'H')
		{
			m_current_ss2_charset = CNS_11643_2;
			return TRUE;
		}
	}
#endif

	return FALSE;
}

enum
{
	NO_VALID_DATA_YET	= -1,		/**< No valid output character yet */
	INVALID_INPUT		= -2,		/**< Invalid or unrecognized input */
	INVALID_INPUT_REDO	= -3		/**< Invalid input, try again with new state */
};

int ISO2022toUTF16Converter::Convert(const void *src, int len, void *dest,
                                     int maxlen, int *read_ext)
{
	int read = 0, written = 0;
	const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	uni_char *output = reinterpret_cast<uni_char *>(dest);
	maxlen &= ~1; // Make sure destination size is always even

	while (read < len && written < maxlen)
	{
		long decoded = NO_VALID_DATA_YET;

		switch (m_state)
		{
		case ascii:
			switch (*input)
			{
			case ISO2022_ESC: m_state = esc; break; // <ESC>
			case ISO2022_SBCS: m_state = ascii; break; // <SI>
			case ISO2022_DBCS: m_last_known_state = m_state = lead; break; // <SO>
			default: decoded = *input; break;
			}
			break;

		case esc:
			switch (*input)
			{
			case '$': m_state = escplus1; break; // <ESC>$
#ifdef ENCODINGS_HAVE_CHINESE
			case ISO2022_SS2:  // SS2
				if (ISO2022CN == m_my_encoding)
				{
					m_state = ss2_lead;
					break;
				}
#endif
			default: // Invalid or unrecognized input
				decoded = INVALID_INPUT_REDO;
				m_state = ascii;
				break;
			}
			break;

		case escplus1:
			switch (*input)
			{
#ifdef ENCODINGS_HAVE_CHINESE
			case '*':
#endif
			case ')':
				m_prev_byte = *input;
				m_state = escplus2;
				break;
			default: // Invalid or unrecognized input
				decoded = INVALID_INPUT_REDO;
				m_state = ascii;
				break;
			}
			break;

		case escplus2:
			if (!identify_charset(m_prev_byte, *input))
			{
				decoded = INVALID_INPUT_REDO;
			}

			// Charset switch has been recognized, now return to
			// previous state
			m_state = m_last_known_state;
			break;

		case lead:
			// Double-byte mode; lead expected
			if (*input < 0x20)
			{
				// C0 control character.
				switch (*input)
				{
				case ISO2022_ESC: m_state = esc; break; // <ESC>
				case ISO2022_SBCS: m_last_known_state = m_state = ascii; break; // <SI>
				case ISO2022_DBCS: m_state = lead; break; // <SO>
				default:
					// Anything else is an error.
					m_last_known_state = m_state = ascii;
					decoded = *input;
					HandleInvalidChar(written);
					break;
				}
			}
			else
			{
				m_prev_byte = *input;
				m_state = trail;
			}
			break;

		case trail:
			// Decode character
			decoded = INVALID_INPUT; // start off with invalid state
			m_state = lead;
			if (m_prev_byte >= 0x21 && m_prev_byte <= 0x7E &&
			    *input      >= 0x21 && *input      <= 0x7E)
			{
				int codepoint;

				switch (m_current_so_charset)
				{
#ifdef ENCODINGS_HAVE_CHINESE
				case GB2312:
					codepoint = (m_prev_byte - 1) * 191 + (*input + 0x40);
					if (codepoint < m_gbk_length)
					{
						decoded = ENCDATA16(m_gbk_table[codepoint]);
					}
					break;

				case CNS_11643_1:
					codepoint = (m_prev_byte - 0x21) * 94 + (*input - 0x21);
					if (codepoint < m_cns_11643_length)
					{
						decoded = ENCDATA16(m_cns_11643_table[codepoint]);
					}
					break;
#endif

#ifdef ENCODINGS_HAVE_KOREAN
				case KSC_5601:
					// The KSC 5601 table contains all the extra codepoints from
					// the extended EUC-KR table, so we need to index into it a
					// bit. This is partly borrowed from the EUC-KR decoder.
					if (m_prev_byte < 0x47)
					{
						// In EUC-KR, lead bytes below 0xC7 (0x47 in ISO-2022)
						// allow more trail bytes than ISO-2022.
						codepoint = (26 + 26 + 126) * (m_prev_byte - 1) + 26 + 26 +
						            *input - 1;
					}
					else
					{
						// The rest of the lead bytes only allow the standard
						// trail bytes.
						codepoint = (26 + 26 + 126) * (0xC7 - 0x81) +
						            (m_prev_byte - 0x47) * 94 + (*input - 0x21);
					}

					if (codepoint >= 0 && codepoint < m_ksc_5601_length)
					{
						decoded = ENCDATA16(m_ksc_5601_table[codepoint]);
					}
					break;
#endif
				}
			}
			else
			{
				/* Unknown character where trail byte was expected:
				 * flag a decoding error and switch to ASCII. */
				m_state = ascii;
				switch (*input)
				{
				case ISO2022_ESC:
					// Error condition: Escape sequence when expecting trail byte
					m_state = esc;
					break;

				case ISO2022_DBCS:
					// Error condition: SO when expecting trail byte
					m_state = lead;
					break;
				}
				m_last_known_state = ascii;
			}
			break;

#ifdef ENCODINGS_HAVE_CHINESE
		case ss2_lead:
			decoded = INVALID_INPUT;
			if (*input >= 0x21 && *input <= 0x7E)
			{
				m_prev_byte = *input;
				m_state = ss2_trail;
				decoded = NO_VALID_DATA_YET;
			}
			else
			{
				switch (*input)
				{
				case ISO2022_LF:
					// Error condition: Line break in stream.
					// Output it and switch back to singlebyte mode.
					m_last_known_state = m_state = ascii;
					decoded = ISO2022_LF;
					HandleInvalidChar(written);
					break;

				case ISO2022_ESC:
					// Error condition: Escape sequence when expecting trail byte
					m_state = esc;
					break;

				case ISO2022_SBCS:
					// Error condition: SI when expecting trail byte
					m_last_known_state = m_state = ascii;
					break;

				case ISO2022_DBCS:
					// Error condition: SO when expecting trail byte
					m_state = lead;
				}
			}
			break;
#endif

#ifdef ENCODINGS_HAVE_CHINESE
		case ss2_trail:
			decoded = INVALID_INPUT; // start off with invalid state
			if (m_prev_byte >= 0x21 && m_prev_byte <= 0x7E &&
			    *input      >= 0x21 && *input      <= 0x7E)
			{
				int codepoint;
				switch (m_current_ss2_charset)
				{
				case CNS_11643_2:
					codepoint = (m_prev_byte - 0x21) * 94 + (*input - 0x21) + 8742;
					if (codepoint < m_cns_11643_length)
					{
						decoded = ENCDATA16(m_cns_11643_table[codepoint]);
					}
				}
			}
			else
			{
				// Invalid character in trail byte.
				switch (*input)
				{
				case ISO2022_ESC:
					// Error condition: Escape sequence when expecting trail byte
					m_state = esc;
					break;

				case ISO2022_SBCS:
					// Error condition: SI when expecting trail byte
					m_last_known_state = m_state = ascii;
					break;

				case ISO2022_DBCS:
					// Error condition: SO when expecting trail byte
					m_state = lead;
					break;
				}
			}

			// Return to state before SS2 character
			m_state = m_last_known_state;
			break;
#endif
		}

		// Output if something was decoded
		switch (decoded)
		{
		case INVALID_INPUT:
			written += HandleInvalidChar(written, &output);
			// Consume
			read ++;
			input ++;
			break;

		case INVALID_INPUT_REDO:
			// Do not consume
			written += HandleInvalidChar(written, &output);
			break;

		default:
			// Consume and output regular character
			*(output ++) = static_cast<UINT16>(decoded);
			written += sizeof (UINT16);
			/* fall through */
		case NO_VALID_DATA_YET:
			// Consume
			read ++;
			input ++;
			break;
		}
	}

	*read_ext = read;
	m_num_converted += written / sizeof (uni_char);
	return written;
}

const char *ISO2022toUTF16Converter::GetCharacterSet()
{
	switch (m_my_encoding)
	{
	default:
#ifdef ENCODINGS_HAVE_CHINESE
	case ISO2022CN:
		return "iso-2022-cn";
#endif
#ifdef ENCODINGS_HAVE_KOREAN
	case ISO2022KR:
		return "iso-2022-kr";
#endif
	}
}

void ISO2022toUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	m_prev_byte = 0;
	m_current_so_charset = NONE;
#ifdef ENCODINGS_HAVE_CHINESE
	m_current_ss2_charset = NONE;
#endif
	m_last_known_state = m_state = ascii;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && (ENCODINGS_HAVE_CHINESE || ENCODINGS_HAVE_KOREAN)
