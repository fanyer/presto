/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/idna/idna.h"
#include "modules/util/str.h"
#include "modules/formats/uri_escape.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/unicode/unicode.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/unicode/unicode_stringiterator.h"


// Full stop ("."), Ideographic full stop, fullwidth full stop, half width full stop, as per IDNA
#define IDNA_FULLSTOPS UNI_L("\x002E\x3002\xFF0E\xFF61")

ServerName* IDNA::GetServerName(ServerName_Store &store, OP_STATUS &op_err, const uni_char *name, unsigned short &port,BOOL create, BOOL Normalize)
{
	// %xx is assumed to be UTF-8
	op_err = OpStatus::OK;

	if(name == NULL)
		return NULL;

	OpString temp_name;

	unsigned int len = uni_strlen(name);
	if(len > IDNA_TempBufSize)
		return NULL;

	uni_strcpy(g_IDNA_tembuf5, name);
	UriUnescape::ReplaceChars(g_IDNA_tembuf5, UriUnescape::MailUtf8);

	uni_char *port_start = NULL;
	if(g_IDNA_tembuf5[0] == '[')
	{
		uni_char *ipend = uni_strchr(g_IDNA_tembuf5+1, ']');
		if(ipend && ipend[1] == ':')
			port_start = ipend+1;
	}
	else
		port_start = uni_strchr(g_IDNA_tembuf5, ':');

	port = 0;
	if ((port_start) && *(port_start + 1) == '\0')
	{
		*port_start = '\0';
	}
	else if(port_start)
	{
		*(port_start++) = '\0';
		int t_port = uni_atoi(port_start);
		if (t_port < 1 || t_port > 65535)
		{
			op_err = OpStatus::ERR_OUT_OF_RANGE;
			return NULL;
		}
		port = (unsigned short) t_port;
	}

	unsigned int len_name = uni_strlen(g_IDNA_tembuf5);
	unsigned int i = len_name;
	const uni_char *test = g_IDNA_tembuf5;

	while(i>0)
	{
		if(*(test++) > 127)
			break;
		i--;
	}

	if(i==0  && (!Normalize || uni_strspn(g_IDNA_tembuf5, UNI_L("0123456789ABCDEFabcdefXx.")) != uni_strlen(g_IDNA_tembuf5)))
	{
		extern void us_ascii_str_lower(char *str);
		make_singlebyte_in_buffer(g_IDNA_tembuf5, len_name,  g_IDNA_tembuf4, IDNA_TempBufSize);
		us_ascii_str_lower(g_IDNA_tembuf4);

		ServerName *name_cand = store.GetServerName(g_IDNA_tembuf4);
		if(name_cand)
			return name_cand;
	}

	BOOL is_mailaddress = FALSE;
	TRAP(op_err, ConvertToIDNA_L(g_IDNA_tembuf5, g_IDNA_tembuf6, IDNA_TempBufSize, is_mailaddress));
	if(OpStatus::IsError(op_err))
	{
		return NULL;
		//uni_strcpy(uni_temp_buffer3, uni_temp_buffer2);
	}

	BOOL is_ipaddress = FALSE;
	if(Normalize)
	{
		size_t name_len = uni_strlen(g_IDNA_tembuf6);
		do{
			if (name_len == 0)
				break;
			if(uni_strspn(g_IDNA_tembuf6, UNI_L("0123456789.")) == name_len)
			{
				is_ipaddress = TRUE;
				break;
			}
			if(uni_strspn(g_IDNA_tembuf6, UNI_L("0123456789abcdefx.")) != name_len)
				break; // Not possibly an IP address

			uni_char *label = g_IDNA_tembuf6;
			size_t span;

			while(*label)
			{
				if(label[0] == '0' && label[1] == 'x') // Hex ?
				{
					span = uni_strspn(g_IDNA_tembuf6, UNI_L("0123456789abcdefx"));
				}
				else
					span = uni_strspn(g_IDNA_tembuf6, UNI_L("0123456789"));

				if(label[span] && label[span] != '.')
					break; // Not the full label

				label += span;
				if(*label) // if not end of string
					label ++;
			}

			if(label[0] == '\0')
				is_ipaddress = TRUE;
		}while(0);
	}

	if(is_ipaddress)
	{
		// Normalize IPv4 addresses to quad-dot
		do{
			OpSocketAddress* socketAddress=NULL;
			if(OpStatus::IsError(OpSocketAddress::Create(&socketAddress)))
				break;

			OpAutoPtr<OpSocketAddress> socket_addr(socketAddress);
			if(OpStatus::IsError(socket_addr->FromString(g_IDNA_tembuf6)))
				break;

			OpString tempaddress;

			if(OpStatus::IsError(socket_addr->ToString(&tempaddress)))
				break;

			if((unsigned int) tempaddress.Length() > IDNA_TempBufSize)
				break;

			uni_strlcpy(g_IDNA_tembuf6, tempaddress.CStr(), IDNA_TempBufSize);
		}while(0);
	}



	{
		uni_char *src = g_IDNA_tembuf6;
		char *trg = (char*)g_IDNA_tembuf5;
		uni_char temp;

		while((temp = *(src++)) != 0)
		{
			if(temp > 0x80 || UriEscape::NeedEscape(temp, UriEscape::StandardUnsafe))
				return NULL; // escapes and non-ASCII are not allowed

			*(trg++) = (char) temp;
		}
		*(trg++) = '\0';
	}

	return store.GetServerName((char*)g_IDNA_tembuf5, create);
}

#define PUNYCODE_BASE			36
#define PUNYCODE_TMIN			1
#define PUNYCODE_TMAX			26
#define PUNYCODE_SKEW			38
#define PUNYCODE_DAMP			700
#define PUNYCODE_INITIAL_BIAS	72
#define PUNYCODE_INITIAL_N		128

#if defined DEBUG && defined YNP_WORK
#ifdef SELFTEST
#define	IDNA_SELFTEST_CHECK g_selftest.running
#else
#define IDNA_SELFTEST_CHECK FALSE
#endif // SELFTEST
#define IDNA_LEAVE(e) { \
	OP_ASSERT(IDNA_SELFTEST_CHECK); \
	LEAVE(e);\
}
#else
#define IDNA_LEAVE(e) LEAVE(e)
#endif

#include "modules/encodings/encoders/encoder-utility.h"

static const char IDNA_punycode_encode_chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";

unsigned long IDNA::Punycode_adapt(unsigned long delta, unsigned long numpoints, BOOL firsttime)
{
	unsigned long k;

	if(firsttime)
		delta /= PUNYCODE_DAMP;
	else
		delta /= 2;

	delta += delta/numpoints;

	k = 0;
	while(delta > ((PUNYCODE_BASE - PUNYCODE_TMIN) * PUNYCODE_TMAX)/2)
	{
		delta /= (PUNYCODE_BASE - PUNYCODE_TMIN);
		k += PUNYCODE_BASE;
	}

	return k + (((PUNYCODE_BASE - PUNYCODE_TMIN + 1) * delta) / (delta + PUNYCODE_SKEW));
}

void IDNA::MapFromUserInput_L(uni_char *string, uni_char *temp_buffer, size_t max_len)
{
	OP_ASSERT(string || !"Invalid call to IDNA::MapFromUserInput_L()");
	OP_ASSERT(temp_buffer || !"Invalid call to IDNA::MapFromUserInput_L()");

	Unicode::Lower(string, TRUE);
#ifdef SUPPORT_UNICODE_NORMALIZE
	unsigned src_len = uni_strlen(string);
	if (src_len>max_len)
		IDNA_LEAVE(OpStatus::ERR);

	Unicode::DecomposeNarrowWide(string, uni_strlen(string), temp_buffer);
	uni_char *temp_buf = Unicode::Normalize(temp_buffer, uni_strlen(temp_buffer), TRUE, FALSE);
	if (!temp_buf)
		IDNA_LEAVE(OpStatus::ERR);

	if(uni_strlen(temp_buf) >= max_len) // need an extra character for null termination
		IDNA_LEAVE(OpStatus::ERR);
	uni_strcpy(string, temp_buf);
	OP_DELETEA(temp_buf);
#endif // SUPPORT_UNICODE_NORMALIZE
}

uni_char *IDNA::ConvertULabelToALabel_L(const uni_char *source, size_t source_size, uni_char *target, size_t targetbufsize, BOOL is_mailaddress, BOOL strict_mode)
{
	OP_ASSERT(source || !"Invalid call to IDNA::ConvertULabelToALabel_L()");
	OP_ASSERT(target || !"Invalid call to IDNA::ConvertULabelToALabel_L()");

	const uni_char* prefix = is_mailaddress ? IMAA_PREFIX_L : IDNA_PREFIX_L;
	size_t prefix_len = uni_strlen(prefix);
	if(uni_strnicmp(source, prefix, prefix_len) == 0)
	{
		// This function must not accept a punycode string
		IDNA_LEAVE(OpStatus::ERR);
	}

	uni_char *start_target = target;
	UnicodeStringIterator it(source, 0, source_size);
	for (; !it.IsAtEnd(); it.Next())
		if(it.At() >= 0x80)
			break;

	if(it.IsAtEnd())
	{
		if(source_size >= targetbufsize)
			IDNA_LEAVE(OpStatus::ERR);

		// only ASCII, or starts with the IDNA/IMAA prefix;
		uni_strncpy(target, source, source_size);
		target += source_size;
		return target;
	}

	if(targetbufsize < prefix_len)
		IDNA_LEAVE(OpStatus::ERR);
	uni_strcpy(target, prefix);
	target += prefix_len;
	targetbufsize -= prefix_len;

	{
		unsigned long n = PUNYCODE_INITIAL_N;
		unsigned long delta = 0;
		unsigned long h, b, m, q, k, t;
		unsigned long len = 0;
		unsigned long bias = PUNYCODE_INITIAL_BIAS;

		for (it = UnicodeStringIterator(source, 0, source_size); !it.IsAtEnd(); it.Next())
			len++;

		b = 0;

		for (it = UnicodeStringIterator(source, 0, source_size); !it.IsAtEnd(); it.Next())
		{
			if(it.At() < PUNYCODE_INITIAL_N)
			{
				*(target++) = (is_mailaddress ? (uni_char) it.At() : uni_tolower((uni_char) it.At()));
				b++;
				targetbufsize--;
				if(targetbufsize == 0)
					IDNA_LEAVE(OpStatus::ERR);
			}
		}
		h = b;
		if(b>0)
		{
			*(target++) = '-';
			targetbufsize--;
			if(targetbufsize == 0)
				IDNA_LEAVE(OpStatus::ERR);
		}

		while(h< len)
		{

			m = ULONG_MAX;
			for (it = UnicodeStringIterator(source, 0, source_size); !it.IsAtEnd(); it.Next())
				if(it.At() >= n && it.At() < m )
					m = it.At();

			// there is always an character > n due to the loop condition -> m >= n;
			delta += (m-n)*(h+1);
			n=m;
			for (it = UnicodeStringIterator(source, 0, source_size); !it.IsAtEnd(); it.Next())
			{
				if(it.At() < n)
				{
					delta++;
					if(delta >= (1 << 26))
						IDNA_LEAVE(OpStatus::ERR); // overflow
				}
				else if(it.At() == n)
				{
					q = delta;
					for(k = PUNYCODE_BASE;;k+=PUNYCODE_BASE)
					{
						if(k<=bias)
							t = PUNYCODE_TMIN;
						else if(k>=bias + PUNYCODE_TMAX)
							t = PUNYCODE_TMAX;
						else
							t = k - bias;

						if(q<t)
							break;

						*(target++) = IDNA_punycode_encode_chars[t+((q-t) % (PUNYCODE_BASE -t))];
						targetbufsize--;
						if(targetbufsize == 0)
							IDNA_LEAVE(OpStatus::ERR);

						q = (q-t)/(PUNYCODE_BASE - t);
					}
					*(target++) = IDNA_punycode_encode_chars[q];
					targetbufsize--;
					if(targetbufsize == 0)
						IDNA_LEAVE(OpStatus::ERR);

					bias = Punycode_adapt(delta, h+1, (h==b));

					delta = 0;
					h++;
				}
			}
			delta++;
			n++;
		}
		*target = '\0';

		b = uni_strlen(start_target);
		if(b < 1 /*|| b>= 64*/)
			IDNA_LEAVE(OpStatus::ERR);
	}

	return target;
}

class ComponentProcessor
{
public:
	virtual uni_char* ProcessLabel_L(const uni_char *source, size_t source_size, uni_char *target, size_t target_size, BOOL is_local_part) = 0;
};

class ToULabelConverter: public ComponentProcessor
{
public:
	uni_char* ProcessLabel_L(const uni_char *source, size_t source_size, uni_char *target, size_t target_size, BOOL is_local_part)
	{
		if (uni_strnicmp(source, IDNA_PREFIX, STRINGLENGTH(IDNA_PREFIX)) == 0)
		{
			char *source_temp = g_IDNA_tembuf4;

			for (const uni_char *src = source; src < source + source_size; ++src)
				if(*(src++) >= 0x80)
					IDNA_LEAVE(OpStatus::ERR);

			make_singlebyte_in_buffer(source, source_size, source_temp, IDNA_TempBufSize);
			return IDNA::ConvertALabelToULabel_L(source_temp, target, target_size, is_local_part);
		}
		else
		{
			if (source_size > target_size)
				IDNA_LEAVE(OpStatus::ERR);
			uni_strncpy(target, source, source_size);
			return target + source_size;
		}
	}
};

class ToALabelConverter: public ComponentProcessor
{
public:
	uni_char* ProcessLabel_L(const uni_char *source, size_t source_size, uni_char *target, size_t target_size, BOOL is_local_part)
	{
		if (source_size > target_size)
			IDNA_LEAVE(OpStatus::ERR);
		const uni_char *temp_source = source;
		uni_char *temp_target = target;
		while (temp_source < source + source_size)
		{
			if (*temp_source >= '0' && *temp_source <= '9' || *temp_source >= 'a' && *temp_source <= 'z' || *temp_source == '-')
				*(target++) = *(temp_source++);
			else
				return IDNA::ConvertULabelToALabel_L(source, source_size, temp_target, target_size, is_local_part, !is_local_part);
		}
		return target;
	}
};

BOOL IDNA::IsIDNAFullStop(uni_char ch)
{
	return uni_strchr(IDNA_FULLSTOPS, ch) != NULL;
}

void IDNA::ProcessDomainComponents_L(const uni_char *source, uni_char *target, size_t targetbufsize, BOOL is_mailaddress, ComponentProcessor &processor)
{
	const uni_char *comp_end = source;
	BOOL mailaddress_without_domain = (is_mailaddress && uni_strpbrk(source, UNI_L("\x0040\xFF20"))==NULL); // Commercial at ("@") and fullwidth commercial at

	do{
		comp_end = uni_strpbrk(source, IDNA_FULLSTOPS);
		if (is_mailaddress)
		{
			const uni_char *local_part_end = uni_strpbrk(source, UNI_L("\x0040\xFF20")); // Commercial at ("@") and fullwidth commercial at
			if (!local_part_end)
			{
				if (!mailaddress_without_domain) //Do we go from username to domain part of an email address?
					is_mailaddress = FALSE;
			}
			else if (local_part_end < comp_end)
			{
				comp_end = local_part_end;
			}
		}

		size_t source_len = comp_end ? (comp_end - source) : uni_strlen(source);
		uni_char *new_target_pos = processor.ProcessLabel_L(source, source_len, target, targetbufsize, is_mailaddress);

		if (comp_end)
		{
			if (new_target_pos >= target + targetbufsize)
			{
				IDNA_LEAVE(OpStatus::ERR);
			}
			new_target_pos[0] = is_mailaddress ? 0x40 /* @ */ : 0x2E /* dot */; 
			source = comp_end + 1;
			targetbufsize -= new_target_pos + 1 - target;
			target = new_target_pos + 1;
		}
		else
			new_target_pos[0] = 0;
	} while(comp_end);
}

// Leaves if an encoding error is encountered
void IDNA::ConvertToIDNA_L(const uni_char *source, uni_char *target, size_t targetbufsize, BOOL is_mailaddress)
{
	if(source == NULL || target == NULL)
		return;

#ifdef SUPPORT_LITERAL_IPV6_URL
	// if server name meets the condition below - it's definitely not a valid domain name, but might be a literal IPv6 address
	// further processing it in this method doesn't make sense
	size_t len = uni_strlen(source);
	if (source[0] == '[' && source[len - 1] == ']' && uni_strspn(source, UNI_L("[]:.0123456789ABCDEFabcdef")) == len)
	{
		uni_strncpy(target, source, targetbufsize);
		return;
	}
#endif // SUPPORT_LITERAL_IPV6_URL

	ToULabelConverter tu_converter;
	uni_char *unicode_source = g_IDNA_tembuf3;
	ProcessDomainComponents_L(source, unicode_source, IDNA_TempBufSize, is_mailaddress, tu_converter);
	MapFromUserInput_L(unicode_source, g_IDNA_tembuf2, IDNA_TempBufSize);
	
	const uni_char *tmp, *unicode_source_end = unicode_source + uni_strlen(unicode_source);
	for (tmp = unicode_source; tmp < unicode_source_end; ++tmp)
		if (*tmp >= 0x80)
			break;
	BOOL asci_only = tmp == unicode_source_end;

	const uni_char *at_pos = uni_strchr(unicode_source, '@');
	const uni_char *domain_part = (is_mailaddress && at_pos) ? at_pos+1 : unicode_source;
	if (((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BlockIDNAInvalidDomains)) || asci_only) && domain_part && !IDNALabelValidator::IsDomainNameIDNAValid_L(domain_part))
		IDNA_LEAVE(OpStatus::ERR);
	
	ToALabelConverter ta_converter;
	ProcessDomainComponents_L(unicode_source, target, targetbufsize, is_mailaddress, ta_converter);
}

static const unsigned char IDNA_punycode_Decode_Table[] = { 
	// Indexed from 48 ('0') to 127 case-insensitive decoding
	// Values 0 - 35 are legal, 36 + are illegal
	26, 27, 28, 29,  30, 31, 32 ,33,  34, 35, 36, 36,  36, 36, 36, 36,
	36,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14,
	15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25, 36,  36, 36, 36, 36,
	36,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14,
	15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25, 36,  36, 36, 36, 36
};

uni_char *IDNA::ConvertALabelToULabel_L(const char *source1, uni_char *target1, size_t targetbufsize, BOOL& is_mailaddress)
{
	BOOL orig_is_mailaddress = is_mailaddress;

	uni_char *temp_source = g_IDNA_tembuf1;
	uni_char *temp_buf = g_IDNA_tembuf2;

	BOOL only_7bit = TRUE;
	const char *src = source1;
	unsigned char tmp;

	while((tmp = *(src++)) != '\0')
	{
		if(tmp >= 0x80)
		{
			only_7bit = FALSE;
			break;
		}
	}

	if(!only_7bit)
	{
		unsigned long len = IDNA_TempBufSize;

		unsigned long lens = op_strlen(source1);
		if(lens >=len)
			IDNA_LEAVE(OpStatus::ERR);

		make_doublebyte_in_buffer(source1, lens, temp_source, len);

		MapFromUserInput_L(temp_source, temp_buf, len);
		if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::BlockIDNAInvalidDomains) && !IDNALabelValidator::IsLabelIDNAValid_L(temp_source))
			IDNA_LEAVE(OpStatus::ERR);

		make_singlebyte_in_place((char *)temp_source);

		source1 = (const char *) temp_source; // converted back to 8 bit
	}

	is_mailaddress = FALSE;

	BOOL has_idna_prefix = op_strnicmp(source1, IDNA_PREFIX, STRINGLENGTH(IDNA_PREFIX))==0;
	BOOL has_imaa_prefix = !has_idna_prefix && op_strnicmp(source1, IMAA_PREFIX, STRINGLENGTH(IMAA_PREFIX))==0;

	if (!has_idna_prefix && (!has_imaa_prefix || !orig_is_mailaddress))
	{
		while((*source1) != '\0' && targetbufsize)
		{
			*(target1++) = *(source1++);
			targetbufsize--;
			if(targetbufsize == 0)
				IDNA_LEAVE(OpStatus::ERR);
		}
		*target1 = '\0';
		return target1;
	}

	// always begins with IDNA_PREFIX or IMAA_PREFIX
	const char *source;
	if (has_idna_prefix)
	{
		source = source1 + STRINGLENGTH(IDNA_PREFIX);
	}
	else// if(has_imaa_prefix)
	{
		is_mailaddress = TRUE;
		source = source1 + STRINGLENGTH(IMAA_PREFIX);
	}

	const char *current;
	unsigned char cur_char;
	//const char *source2 = source;
	uni_char *target = target1;


	unsigned long n = PUNYCODE_INITIAL_N;
	unsigned long i = 0;
	unsigned long bias = PUNYCODE_INITIAL_BIAS;
	unsigned long j, old_i, k, t, w, digit;
	unsigned long out_len = 0; // Length in UTF-16 code units (uni_chars)

	current = op_strrchr(source,'-');
	if(current)
	{
		out_len = current - source;
		if(targetbufsize <= out_len)
			IDNA_LEAVE(OpStatus::ERR);
		for(i = 0;i< out_len; i++)
		{
			if((target1[i] = (orig_is_mailaddress ? source[i] : op_tolower(source[i]))) >= PUNYCODE_INITIAL_N)
				IDNA_LEAVE(OpStatus::ERR);
		}
		OP_ASSERT(source[out_len] == '-');
	}

	unsigned long numchars = out_len; // Length in Unicode characters

	if(current)
		current ++;
	else
		current = source;
	i=0;
	while((*current) != 0)
	{
		old_i = i;
		w = 1;
		for(k = PUNYCODE_BASE;;k+=PUNYCODE_BASE)
		{
			cur_char = *(current++);
			if(cur_char < 0x30 || cur_char >= 0x80)
				IDNA_LEAVE(OpStatus::ERR);

			digit = IDNA_punycode_Decode_Table[cur_char - 0x30];
			if(digit > 35)
				IDNA_LEAVE(OpStatus::ERR);

			i += digit * w;

			if(k<=bias)
				t = PUNYCODE_TMIN;
			else if(k>=bias + PUNYCODE_TMAX)
				t = PUNYCODE_TMAX;
			else
				t = k - bias;

			if(digit < t)
				break;

			w *= (PUNYCODE_BASE - t);
		}

		bias = Punycode_adapt(i-old_i, numchars +1, (old_i == 0));
		n += i/(numchars + 1);
		i %= numchars+1;

		if(n < 0x10000UL)
		{
			// One UTF-16 code unit.
			if (Unicode::IsSurrogate((uni_char) n))
				IDNA_LEAVE(OpStatus::ERR);

			for(j = out_len; j>i; j--)
				target[j] = target[j-1];

			target[i] = (uni_char) n;
			out_len ++;
			i++;
		}
		else if(n < 0x110000UL && targetbufsize > out_len+2)
		{
			// Need 2 code units. We need to take care here since
			// the index (i) is in characters, while target
			// is indexed using UTF-16 code units.
			unsigned long newarrayindex = i;
			for(unsigned long k = 0; k < newarrayindex; k ++)
				if (Unicode::IsHighSurrogate(target[k]))
					newarrayindex ++;

			for(j = out_len+1; j>newarrayindex; j--)
				target[j] = target[j-2];

			Unicode::MakeSurrogate(n, target[newarrayindex], target[newarrayindex+1]);
			out_len +=2;
			i++;
		}
		else
			IDNA_LEAVE(OpStatus::ERR);

		if(targetbufsize <= out_len)
			IDNA_LEAVE(OpStatus::ERR);

		++ numchars;
	}
	target += out_len;
	*target = '\0';

	return target;
}

void IDNA::ConvertFromIDNA_L(const char *src, uni_char *trg, size_t trg_bufsize, BOOL& is_mailaddress)
{
	BOOL orig_is_mailaddress = is_mailaddress;

	is_mailaddress = FALSE;
	const char * OP_MEMORY_VAR source = src;
	uni_char * OP_MEMORY_VAR target = trg;
	OP_MEMORY_VAR size_t targetbufsize= trg_bufsize;

	while(source)
	{
		char * OP_MEMORY_VAR sep = const_cast<char*>(source + op_strcspn(source, ".@"));
		if (*sep==0)
			sep = NULL;

		OP_MEMORY_VAR char sep_char = 0;
		if(sep)
		{
			sep_char = *sep;
			*sep = '\0';
		}

		BOOL has_idna_prefix = op_strnicmp(source, IDNA_PREFIX, STRINGLENGTH(IDNA_PREFIX))==0;
		BOOL has_imaa_prefix = !has_idna_prefix && op_strnicmp(source, IMAA_PREFIX, STRINGLENGTH(IMAA_PREFIX))==0;

		if (has_idna_prefix || (has_imaa_prefix && orig_is_mailaddress))
		{
			uni_char *old_target = target;
			BOOL is_mailaddress_tmp = orig_is_mailaddress;
			TRAPD(op_err, target = ConvertALabelToULabel_L(source, target, targetbufsize - 1, is_mailaddress_tmp));
			is_mailaddress |= is_mailaddress_tmp;

			if(OpStatus::IsSuccess(op_err))
			{
				uni_char *temp_convert = g_IDNA_tembuf3;

				TRAP(op_err, ConvertULabelToALabel_L(old_target, uni_strlen(old_target), temp_convert, targetbufsize, is_mailaddress_tmp, !is_mailaddress_tmp));

				if(OpStatus::IsSuccess(op_err))
				{
					OpStringC temp_convert1(temp_convert);
					if(temp_convert1.CompareI(source)!=0)
						op_err = OpStatus::ERR;
				}
			}

			if(OpStatus::IsError(op_err))
			{
				if(sep)
					*(sep) = sep_char;
				IDNA_LEAVE(op_err);
			}

			if(targetbufsize <= (size_t) (target- old_target))
				IDNA_LEAVE(OpStatus::ERR);

			targetbufsize -= (old_target -target);
		}
		else
		{
			UTF8toUTF16Converter converter;
			ANCHOR(UTF8toUTF16Converter, converter);
			int read=0;

			LEAVE_IF_ERROR(converter.Construct());

			int len = op_strlen(source);
			len = converter.Convert(source, len, (char *) target, targetbufsize - 1, &read);
			if(len == -1)
			{
				if(sep)
					*(sep) = sep_char;
				IDNA_LEAVE(OpStatus::ERR);
			}

			len = UNICODE_DOWNSIZE(len);
			target += len;
			targetbufsize -= len;
		}
		if(sep)
		{
			*(target++) = sep_char;
			*(sep) = sep_char;
		}
		source = sep ? sep + 1 : NULL;
	}
	*target = 0;
}

enum URL_IDN_SecurityLevel IDNA::ComputeSecurityLevel_L(const uni_char *domain)
{
	if (!IDNALabelValidator::IsDomainNameIDNAValid_L(domain, TRUE))
		return IDN_Invalid;
	return ComputeSecurityLevelRecursive(domain, uni_strlen(domain), TRUE);
}

enum URL_IDN_SecurityLevel IDNA::ComputeSecurityLevelRecursive(const uni_char *domain, int len, BOOL split)
{
	if (!domain || !*domain)
		return IDN_No_IDN;

	// Split at dots
	const uni_char *p;
	if (split && (p = uni_strchr(domain, '.')))
	{
		// Recursively test both parts
		URL_IDN_SecurityLevel left = ComputeSecurityLevelRecursive(domain, p - domain, FALSE);
		URL_IDN_SecurityLevel right= ComputeSecurityLevelRecursive(p + 1, len - (p - domain) - 1, TRUE);

		// Return the highest level found
		if (left > right)
			return left;
		else
			return right;
	}

	// Calculate security level for this segment
	int scriptsused[SC_NumberOfScripts];
	int number_of_scripts = 0;
	op_memset(scriptsused, 0, sizeof scriptsused);

	p = domain;
	for (int n = len; n>0; --n)
	{
		unsigned int ch;
		if (Unicode::IsHighSurrogate(*p))
		{
			ch = Unicode::DecodeSurrogate(*p, *(p + 1));
			OP_ASSERT(ch < 0x110000UL);
			p++;
			n--;
			if (!*p)
			{
				return IDN_Invalid;
			}
		}
		else
		{
			ch = *p;
			OP_ASSERT(ch < 0x110000UL);
		}

		ScriptType st = Unicode::GetScriptType(ch);
		if (st != SC_Unknown && !scriptsused[st])
		{
			++number_of_scripts;
			scriptsused[st] = 1;
		}

		++p;
	}

	// Compute the level
	URL_IDN_SecurityLevel level = IDN_Unrestricted;

	// Minimal level ("Highly Restrictive"): -----
	// FIXME: Check for identifier profile

	// * Any single script
	if (number_of_scripts <= 1)
	{
		level = IDN_Minimal;
	}

	// * Han + Hiragana + Katakana
	else if (number_of_scripts == scriptsused[SC_Han] + scriptsused[SC_Hiragana] + scriptsused[SC_Katakana])
	{
		level = IDN_Minimal;
	}

	// * Han + Bopomofo
	else if (number_of_scripts == scriptsused[SC_Han] + scriptsused[SC_Bopomofo])
	{
		level = IDN_Minimal;
	}

	// * Han + Hangul
	else if (number_of_scripts == scriptsused[SC_Han] + scriptsused[SC_Hangul])
	{
		level = IDN_Minimal;
	}

	// Moderate level ("Moderately Restrictive"): -----
	// * Allow latin with other scripts, except Cyrillic, Greek, Cherokee
	else if (level == IDN_Unrestricted &&
		scriptsused[SC_Latin] == 1 &&
		scriptsused[SC_Cyrillic] == 0 &&
		scriptsused[SC_Greek] == 0 &&
		scriptsused[SC_Cherokee] == 0)
	{
		level = IDN_Moderate;
	}

	return level;
}

#undef IDNA_LEAVE
