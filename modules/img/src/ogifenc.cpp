#include "core/pch.h"

#ifdef SELFTEST

#include "modules/img/src/ogifenc.h"

////////////////////////////////////////////////
// LzwStringLink                              //
////////////////////////////////////////////////

LzwStringLink::LzwStringLink()
{
	str_len = 0;
	id_nr = 0;
}

void LzwStringLink::SetString(const UINT8* str, int len, int nr)
{
	op_memcpy(str_buf, str, len);
	str_len = len;
	id_nr = nr;
}

////////////////////////////////////////////////
// LzwEncoder                                 //
////////////////////////////////////////////////

LzwEncoder::~LzwEncoder()
{
	string_table.Clear();
}

void LzwEncoder::Reset(int num_cols, int occupied_codes)
{
	this->num_cols = 1 << (num_cols + 1);
	this->occupied_codes = 1 << occupied_codes;
	this->clear_code = this->occupied_codes;
	this->end_code = clear_code + 1;
	next_code = clear_code + 2;
	state_len = 0;
	packed_len = 0;
	bitencoded_len = 0;
	string_table.Clear();
}

OP_STATUS LzwEncoder::AddCode(UINT8 code)
{
	UINT8 temp_string[4096];
	op_memcpy(temp_string, state_string, state_len);
	temp_string[state_len] = code;
	int table_code = GetString(temp_string, state_len + 1);
	if (table_code >= 0)
	{
		state_string[state_len] = code;
		state_len++;
	}
	else
	{
		RETURN_IF_ERROR(AddString(temp_string, state_len + 1));
		int tab_code = GetString(state_string, state_len);
		OP_ASSERT(tab_code >= 0);
		packed_string[packed_len] = tab_code;
		packed_len++;
		state_string[0] = code;
		state_len = 1;
	}

	return OpStatus::OK;
}

void LzwEncoder::Finish()
{
	int table_code = GetString(state_string, state_len);
	OP_ASSERT(table_code >= 0);
	packed_string[packed_len] = table_code;
	packed_len++;
	BitencodeString();
}

int LzwEncoder::GetPackedLen()
{
	return packed_len;
}

UINT8* LzwEncoder::GetPackedString()
{
	return packed_string;
}

int LzwEncoder::GetBitEncodedLen()
{
	return bitencoded_len;
}

UINT8* LzwEncoder::GetBitEncodedString()
{
	return bitencoded_string;
}

int LzwEncoder::GetString(const UINT8* str, int len)
{
	if (len == 1)
	{
		UINT8 num = str[0];
		return num;
	}
	for (LzwStringLink* link = (LzwStringLink*)string_table.First(); link != NULL; link = (LzwStringLink*)link->Suc())
	{
		if (len == link->GetLen() && op_memcmp(str, link->GetString(), len) == 0)
		{
			return link->GetId();
		}
	}
	return -1;
}

OP_STATUS LzwEncoder::AddString(const UINT8* str, int len)
{
	LzwStringLink* link = OP_NEW(LzwStringLink, ());
	if (link)
	{
		link->SetString(str, len, next_code);
		next_code++;
		link->Into(&string_table);
	}
	else
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

int LzwEncoder::GetNrOfBits(int pos)
{
	int tot = pos + occupied_codes + 2;
	if (tot <= 2)
	{
		return 1;
	}
	else if (tot <= 4)
	{
		return 2;
	}
	else if (tot <= 8)
	{
		return 3;
	}
	else if (tot <= 16)
	{
		return 4;
	}
	else if (tot <= 32)
	{
		return 5;
	}
	else if (tot <= 64)
	{
		return 6;
	}
	else if (tot <= 128)
	{
		return 7;
	}
	else if (tot <= 256)
	{
		return 8;
	}
	else
	{
		OP_ASSERT(FALSE);
	}
	return 0;
}

void LzwEncoder::BitencodeString()
{
	bitencoded_len = 0;
	int bit_buf = 0;
	int bit_buf_used = 0;
	int i = 0;
	for (i = 0; i < packed_len; i++)
	{
		int nr_of_bits = GetNrOfBits(i);
		bit_buf |= packed_string[i] << bit_buf_used;
		bit_buf_used += nr_of_bits;
		if (bit_buf_used >= 8)
		{
			int data = bit_buf & 0xff;
			bitencoded_string[bitencoded_len] = data;
			bitencoded_len++;
			bit_buf_used -= 8;
			bit_buf >>= 8;
		}
	}
	// Add the last one here, if any rest, followed by the end code.
	int nr_of_bits = GetNrOfBits(i);
	bit_buf |= end_code << bit_buf_used;
	bit_buf_used += nr_of_bits;
	if (bit_buf_used >= 8)
	{
		int data = bit_buf & 0xff;
		bitencoded_string[bitencoded_len] = data;
		bitencoded_len++;
		bit_buf_used -= 8;
		bit_buf >>= 8;
	}
	if (bit_buf_used >= 1)
	{
		int data = bit_buf & 0xff;
		bitencoded_string[bitencoded_len] = data;
		bitencoded_len++;
	}
}

#endif // SELFTEST
