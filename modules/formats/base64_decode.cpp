/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/formats/base64_decode.h"

// Reverse table versions Base64_Encoding_char.
// 0-63 normal encoded values
// 64	legal termination character
// 65	ignore
// 66	illegal,ignore
const unsigned char Base64_decoding_table[128] = {
//                                            \n           \r
	66, 66, 66, 66,  66, 66, 66, 66,  66, 66, 65, 66,  66, 65, 66, 66,
//
	66, 66, 66, 66,  66, 66, 66, 66,  66, 66, 66, 66,  66, 66, 66, 66,
//  ' '  !   "   #    $   %   &   '    (   )   *   +    ,   -   .   /
	66, 66, 66, 66,  66, 66, 66, 66,  66, 66, 66, 62,  66, 66, 66, 63, 
//   0   1   2   3    4   5   6   7    8   9   :   ;    <   =   >   ?
	52, 53, 54, 55,  56, 57, 58, 59,  60, 61, 66, 66,  66, 64, 66, 66, 
//   @   A   B   C    D   E   F   G    H   I   J   K    L   M   N   O
	66,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14, 
//   P   Q   R   S    T   U   V   W    X   Y   Z   [    \   ]   ^   _
	15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25, 66,  66, 66, 66, 66,
//   `   a   b   c    d   e   f   g    h   i   j   k    l   m   n   o
	66, 26, 27, 28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40,
//   p   q   r   s    t   u   v   w    x   y   z   {    |   }   ~
	41, 42, 43, 44,  45, 46, 47, 48,  49, 50, 51, 66,  66, 66, 66, 66
};

unsigned long GeneralDecodeBase64(const unsigned char *source, unsigned long len, unsigned long &read_pos, unsigned char *target, BOOL &warning, 
								  unsigned long max_write_len, unsigned char *decode_buf, unsigned int *decode_len)
{
	unsigned long pos =0,pos1;
	unsigned long target_pos = 0;
	register int i=0;
	register unsigned long temp;
	register unsigned char code;


	if(decode_len)
		*decode_len = 0;

	if(max_write_len == 0)
	{
		decode_buf = NULL;
		decode_len = NULL;
	}
	else
	{
		if(decode_buf == NULL && decode_len != NULL)
			decode_len = NULL;
		else if(decode_buf != NULL && decode_len == NULL)
			decode_buf = NULL;
	}

	warning = FALSE;
	read_pos = 0;

	if(source == NULL || len == 0)
		return 0;

	// If target is NULL we will just "dump it to /dev/null"

	pos1=pos;
	temp = 0;
	while(pos < len && (!max_write_len || target_pos < max_write_len))
	{
		
		code = source[pos++];
		if(code < 0x80)
		{
			code = Base64_decoding_table[code];
			if(code <64)
			{
				temp = (temp << 6) | code;
				
				if((++i) == 4)
				{
					if(max_write_len == 0 || target_pos+3 <= max_write_len)
					{
						if(target != NULL)
						{
							target[target_pos++] = (unsigned char ) (temp >> 16);
							target[target_pos++] = (unsigned char ) (temp >> 8);
							target[target_pos++] = (unsigned char ) (temp);
						}
						else
							target_pos += 3;
					}
					else
					{
						if(decode_buf == NULL)
							break;

						if(target != NULL)
							target[target_pos++] = (unsigned char ) (temp >> 16);
						else
							target_pos ++;

						if(max_write_len== 0 || target_pos < max_write_len)
						{
							if(target != NULL)
								target[target_pos++] = (unsigned char ) (temp >> 8);
							else
								target_pos++;
						}
						else if(decode_buf)
						{
							decode_buf[(*decode_len)++] = (unsigned char ) (temp >> 8);
						}

						// Always: target_pos >= max_write_len
						if(decode_buf)
						{
							decode_buf[(*decode_len)++] = (unsigned char ) (temp);
						}

					}
					i = 0;
					pos1 = pos;
				}
			}
			else if(code == 64) // "=" ignore charachter
			{
				if(i <2)
				{
					if (i == 1)
						warning = TRUE;
					// else, this '=' could be the second one in a split decoding
				}
				else
				{
					if(i == 2)
					{
						if(target != NULL)
							target[target_pos++] = (unsigned char ) (temp >> (12-8));
						else
							target_pos++;
					}
					else
					{
						if(decode_buf == NULL && max_write_len != 0 && target_pos + 2 > max_write_len)
							break;

						if(target != NULL)
							target[target_pos++] = (unsigned char ) (temp >> (18-8));
						else
							target_pos ++;

						if(!max_write_len || target_pos < max_write_len)
						{
							if(target != NULL)
								target[target_pos++] = (unsigned char ) (temp >> (18-16));
							else
								target_pos++;
						}
						else if(decode_buf)
						{
							decode_buf[(*decode_len)++] = (unsigned char ) (temp >> (18-16));
						}
					}
				}
				i++;

				// skip the next 4-i legal characters
				while(i< 4 && pos < len)
				{
					code = source[pos++];
					if(code < 0x80)
					{
						code = Base64_decoding_table[code];
						if(code < 64)
						{
							i++;
							warning = TRUE;
						}
						else if(code == 64)
							i++;
						else if(code > 65)
							warning = TRUE;
					}
					else
						warning = TRUE;
				}
				i = 0;
				pos1 = pos;
			}
			else if(code > 65)
				warning = TRUE;
			else if(pos == pos1+1)
				pos1 = pos;
		}
		else
			warning = TRUE;
	}

	// Skipping past whitespace until the first base64 character
	while(pos1 < len)
	{
		code = source[pos1];
		if(code < 0x80)
		{
			code = Base64_decoding_table[code];
			if(code <= 64)
				break;
			else if(code >65)
				warning = TRUE;
			pos1++;
		}
		else
		{
			warning = TRUE;
			pos1++;
		}
	}
	read_pos = pos1;
	return target_pos;
}
