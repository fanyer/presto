/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

/** Use this if the libssl random implementation is not used. 
 *  For example if ssl is turned off, or you use external ssl.
 *  
 *   OpRandom must be implemented against the random function on the platform.
 */

#ifdef CRYPTO_API_SUPPORT

#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/libcrypto/src/CryptoStreamCipherSnow.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/hardcore/mh/mh.h"

#define g_libcrypto_m_random_generators (g_opera->libcrypto_module.m_random_generators)
#define CRYPTO_MINIMUM_ENTROPY 128


OpRandomGenerator::OpRandomGenerator(BOOL secure, BOOL feed_entropy_automatically)
	: m_random_source(NULL)
	, m_counter(0)
	, m_entropy(0)
	, m_number_of_feedings(0)	
	, m_secure(secure)
	, m_feed_entropy_automatically(feed_entropy_automatically)
	, m_stream(0)
{
}

OpRandomGenerator::~OpRandomGenerator()
{	
	if (m_feed_entropy_automatically)
		g_libcrypto_m_random_generators->RemoveByItem(this);
	
	OP_DELETE(m_random_source);
}

OpRandomGenerator *OpRandomGenerator::Create(BOOL secure, BOOL feed_entropy_automatically)
{
	OP_ASSERT(g_libcrypto_m_random_generators != NULL);
	OpRandomGenerator *new_generator = NULL;
	OP_STATUS add_status = OpStatus::ERR;
	
	if ((new_generator = OP_NEW(OpRandomGenerator, (secure, feed_entropy_automatically))) == NULL ||
			OpStatus::IsError(new_generator->Init()) ||
			(feed_entropy_automatically && OpStatus::IsError(add_status = g_libcrypto_m_random_generators->Add(new_generator)))
		)
	{
		if (OpStatus::IsError(add_status))
			new_generator->m_feed_entropy_automatically = FALSE;			 
		OP_DELETE(new_generator);
		return NULL;
	}
	return new_generator;
}

OP_STATUS OpRandomGenerator::Init()
{
	const UINT8 start_state[16] = { 0,1,2,3,4,5,6,7,8,9,0xa,0xb,0xc,0xd,0xe,0xf };	
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (
			(m_random_source = CryptoStreamCipherSnow::CreateSnow()) == NULL 
			|| OpStatus::IsError(status = m_random_source->SetKey(start_state, 16, start_state))
		)
	{
		OP_DELETE(m_random_source);
		return status;
	}
	
	m_stream = m_random_source->ClockCipher();
	return OpStatus::OK; 
}

void OpRandomGenerator::AddEntropy(const void *data, int length, int estimated_random_bits)
{
	OP_ASSERT(data && length >= 0);

	if (estimated_random_bits == -1)
		estimated_random_bits = length*8;
	
	m_entropy += estimated_random_bits;
	m_number_of_feedings++;
	
	if (m_entropy > 576) 
		m_entropy = 576;
	
	m_random_source->AddEntropy(static_cast<const UINT8*>(data), length);
}


void OpRandomGenerator::AddEntropyAllGenerators(const void *data, int length, int estimated_random_bits)
{
	OP_ASSERT(data && length >= 0);

	OpVector<OpRandomGenerator> *generators = g_libcrypto_m_random_generators;
	if (!generators)
		return;
	
	OP_ASSERT(generators->GetCount() > 0 && "FATAL! NO ACTIVE RANDOM GENERATORS");
	
	if (generators->GetCount() == 0)
		return;
	
	UINT32 i;
	UINT32 diff = 0;
	OpRandomGenerator *generator = NULL;
	for (i = 0; i < generators->GetCount(); i++)
	{
		generator = generators->Get(i); 
		generator->AddEntropy(data, length, estimated_random_bits);
		
		generator->AddEntropy(&diff, 4, 1); // make sure the different generators get different entropy 

		diff = generator->m_random_source->ClockCipher();
		generator->m_random_source->ClockCipher();
	}
	
	
// In addition, we feed the ssl generator
#if defined(_SSL_SEED_FROMMESSAGE_LOOP_) && defined(_NATIVE_SSL_SUPPORT_)
	if (generator && !(generator->m_number_of_feedings & 3))
	{
		extern void SSL_Process_Feeder();
		if(SSL_RND_feeder_len)
		{
			if(SSL_RND_feeder_pos +2 > SSL_RND_feeder_len)
				SSL_Process_Feeder();
	
			SSL_RND_feeder_data[SSL_RND_feeder_pos++] = (DWORD)(diff);
			
			SSL_RND_feeder_data[SSL_RND_feeder_pos++] = (DWORD)generator->m_random_source->ClockCipher();
		}
	}	
#endif	// _SSL_SEED_FROMMESSAGE_LOOP_ && _NATIVE_SSL_SUPPORT_
}

void OpRandomGenerator::AddEntropyFromTimeAllGenerators()
{
	double gmt_unix_time = OpDate::GetCurrentUTCTime();
	AddEntropyAllGenerators(reinterpret_cast<const void*>(&gmt_unix_time), sizeof(double), 5);	
}

void OpRandomGenerator::GetRandom(UINT8 *bytes, int length)
{
	OP_ASSERT(bytes && length >= 0);

	// Only fire the assert on the first get if not enough entropy, to avoid annoyance.
	OP_ASSERT(!m_secure || m_counter != 0 || (m_entropy > CRYPTO_MINIMUM_ENTROPY && "Not enough entropy(random data) added to random generator"));
	int i;
	for (i = 0; i < length; i++)
	{
		if ((m_counter & 3) == 0)
			m_stream = m_random_source->ClockCipher();
	
		bytes[i] = m_stream & 0xff;
		m_stream >>= 8;
		
		m_counter++;
	}
}

OP_STATUS OpRandomGenerator::GetRandomHexString(OpString8 &random_hex_string, int length)
{
	RETURN_OOM_IF_NULL(random_hex_string.Reserve(length + 1));
	GetRandom(reinterpret_cast<UINT8*>(random_hex_string.CStr()), length);
	for (int index = 0; index < length; index++)
	{
		UINT8 random_byte = random_hex_string[index] & 15;
		random_hex_string[index] = random_byte < 0x0a ? 0x30 + random_byte : 0x41 + (random_byte - 0x0a);
	}
	random_hex_string[length] = '\0';
	return OpStatus::OK;
}

OP_STATUS OpRandomGenerator::GetRandomHexString(OpString &random_hex_string, int length)
{
	OpString8 random_hex_string8;
	RETURN_IF_ERROR(GetRandomHexString(random_hex_string8, length));
	return random_hex_string.Set(random_hex_string8.CStr());
}

UINT8 OpRandomGenerator::GetUint8()
{
	// Only fire the assert on the first get if not enough entropy, to avoid annoyance.	
	OP_ASSERT(!m_secure || m_counter != 0 || (m_entropy > CRYPTO_MINIMUM_ENTROPY && "Not enough entropy(random data) added to random generator"));
	UINT8 output;
	GetRandom(&output, 1);
	return output;
}

UINT16 OpRandomGenerator::GetUint16()
{
	// Only fire the assert on the first get if not enough entropy, to avoid annoyance.	
	OP_ASSERT(!m_secure || m_counter != 0 || (m_entropy > CRYPTO_MINIMUM_ENTROPY && "Not enough entropy(random data) added to random generator"));
	UINT8 stream[2];
	GetRandom(stream, 2);
	return stream[0] | (stream[1] << 8); 
	
}

UINT32 OpRandomGenerator::GetUint32()
{
	// Only fire the assert on the first get if not enough entropy, to avoid annoyance.	
	OP_ASSERT(!m_secure || m_counter != 0 || (m_entropy > CRYPTO_MINIMUM_ENTROPY && "Not enough entropy(random data) added to random generator"));
	m_counter += 4;	
	return m_random_source->ClockCipher();
}

double OpRandomGenerator::GetDouble()
{
	// Only fire the assert on the first get if not enough entropy, to avoid annoyance.	
	OP_ASSERT(m_counter != 0 || (!m_secure || (m_entropy > CRYPTO_MINIMUM_ENTROPY && "Not enough entropy(random data) added to random generator")));
	m_counter += 8;

	UINT32 values[2];
	m_random_source->RetrieveCipherStream(values, ARRAY_SIZE(values));

	UINT32 ui1 = values[0] - 1;
	UINT32 ui2 = values[1] - 1;

	ui2 &= ~((1 << 11) - 1);

	return (double(ui1) / 4294967296.0) + (double(ui2) / 18446744073709551616.0);
}

#ifndef _NATIVE_SSL_SUPPORT_
void SSL_RND(byte *target, UINT32 len)
{
	OP_ASSERT(target);
	g_libcrypto_ssl_random_generator->GetRandom(target, len);	
}

void SSL_SEED_RND(byte *source, UINT32 len)
{
	OP_ASSERT(source);
	g_libcrypto_ssl_random_generator->AddEntropy(source, len);
}
#endif // !_NATIVE_SSL_SUPPORT_

#endif // CRYPTO_API_SUPPORT
