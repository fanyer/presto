#ifndef OGIFENC_H
#define OGIFENC_H

#include "modules/util/simset.h"

class LzwStringLink : public Link
{
public:
	LzwStringLink();
	void SetString(const UINT8* str, int len, int nr);
	int GetId() { return id_nr; }
	int GetLen() { return str_len; }
	UINT8* GetString() { return str_buf; }

private:
	unsigned char str_buf[4096];
	int str_len;
	int id_nr;
};

class LzwEncoder
{
public:
	~LzwEncoder();

	void Reset(int num_cols, int occupied_codes);
	OP_STATUS AddCode(UINT8 code);
	void Finish();
	int GetPackedLen();
	UINT8* GetPackedString();
	int GetBitEncodedLen();
	UINT8* GetBitEncodedString();

private:
	/*
	 Returns -1 if string not in table.
	 */
	int GetString(const UINT8* str, int len);

	OP_STATUS AddString(const UINT8* str, int len);

	void BitencodeString();
	int GetNrOfBits(int pos);

	UINT8 packed_string[4096];
	int packed_len;
	UINT8 bitencoded_string[4096];
	int bitencoded_len;
	UINT8 state_string[4096];
	int state_len;
	Head string_table;
	int num_cols;
	int occupied_codes;
	int clear_code;
	int end_code;
	int next_code;

};

#endif // !OGIFENC_H
