/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2011
 *
 * class ES_Runtime
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/es_program_cache.h"
#include "modules/pi/OpSystemInfo.h"
#ifndef _STANDALONE
#include "modules/hardcore/mem/mem_man.h"
#endif // _STANDALONE

/* static */ unsigned
ES_Program_Cache::Weight(ES_ProgramCodeStatic *program)
{
	// Fast and simple estimate of program memory usage.
	return SCRIPT_EXPANSION_FACTOR * program->source_storage->length;
}

ES_Program_Cache::ES_Program_Cache()
	: referenced_total_weight(0), sent_timeout_message(FALSE)
{
}

/* static */ ES_Program_Cache *
ES_Program_Cache::MakeL()
{
	ES_Program_Cache *cache = OP_NEW_L(ES_Program_Cache, ());
#ifndef _STANDALONE
	if (OpStatus::IsError(g_main_message_handler->SetCallBack(cache, MSG_ES_PRUNE_PROGRAM_CACHE, 0)))
	{
		OP_DELETE(cache);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}
#endif // _STANDALONE
	return cache;
}

ES_Program_Cache::~ES_Program_Cache()
{
	OP_NEW_DBG("ES_Program_Cache::~ES_Program_Cache", "es_program_cache");
	while (ES_ProgramCodeStatic *program = referenced.First())
	{
		program->Out();
#ifdef DEBUG_ENABLE_OPASSERT
		referenced_total_weight -= Weight(program);
		OP_DBG(("Dropping program %p of weight %u. Current size %u\n", program, Weight(program), referenced_total_weight));
#endif // DEBUG_ENABLE_OPASSERT
		ES_CodeStatic::DecCacheRef(program);
	}

	OP_ASSERT(referenced_total_weight == 0);

	other.RemoveAll();
#ifndef _STANDALONE
	g_main_message_handler->RemoveCallBack(this, MSG_ES_PRUNE_PROGRAM_CACHE);
#endif // _STANDALONE
}

void
ES_Program_Cache::Clear()
{
	OP_NEW_DBG("ES_Program_Cache::Clear", "es_program_cache");
	while (ES_ProgramCodeStatic *program = referenced.First())
	{
		program->Out();
		referenced_total_weight -= Weight(program);
		OP_DBG(("Releasing program %p of weight %u. Current size %u\n", program, Weight(program), referenced_total_weight));
		if (ES_CodeStatic::DecCacheRef(program))
			program->Into(&other);
	}
	OP_ASSERT(referenced_total_weight == 0);
	UpdateTimeout();
}

size_t
ES_Program_Cache::GetMaximumSize()
{
#ifdef _STANDALONE
	return 30 * 1024 *1024;
#else
	unsigned long max_doc = g_memory_manager->MaxDocMemory();
	size_t program_cache_limit = max_doc / 10;
	const size_t min_cache_size = 2 * 1024 *1024;
	if (program_cache_limit < min_cache_size)
		program_cache_limit = min_cache_size;
	return program_cache_limit;
#endif // _STANDALONE
}

void
ES_Program_Cache::UpdateTimeout()
{
#ifndef _STANDALONE
	ES_ProgramCodeStatic *program = referenced.First();
	if (program || sent_timeout_message)
	{
		if (sent_timeout_message && program && program->program_cache_time + CACHE_TIMEOUT == next_cache_timeout)
			return;

		if (sent_timeout_message)
		{
			g_main_message_handler->RemoveDelayedMessage(MSG_ES_PRUNE_PROGRAM_CACHE, 0, 0);
			sent_timeout_message = FALSE;
		}

		if (program)
		{
			double now = g_op_time_info->GetRuntimeMS();
			unsigned delay;
			if (now > program->program_cache_time + CACHE_TIMEOUT)
				delay = 0;
			else
				delay = static_cast<unsigned>(op_ceil(program->program_cache_time + CACHE_TIMEOUT - now));

			if (g_main_message_handler->PostDelayedMessage(MSG_ES_PRUNE_PROGRAM_CACHE, 0, 0, delay))
			{
				sent_timeout_message = TRUE;
				next_cache_timeout = program->program_cache_time + CACHE_TIMEOUT;
				return;
			}
		}
	}
#endif // _STANDALONE
}

#ifndef _STANDALONE
/* virtual */ void
ES_Program_Cache::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_NEW_DBG("ES_Program_Cache::HandleCallback", "es_program_cache");
	OP_ASSERT(msg == MSG_ES_PRUNE_PROGRAM_CACHE);
	sent_timeout_message = FALSE;

	// Rather than one message per program, clean out everything that is close
	// to expiring when we're here. Close to == within 5 seconds.
	double threshold_time = g_op_time_info->GetRuntimeMS() - CACHE_TIMEOUT + 5000;
	while (ES_ProgramCodeStatic *program = referenced.First())
	{
		if (program->program_cache_time > threshold_time)
			break;
		unsigned weight = Weight(program);

		program->Out();
		if (ES_CodeStatic::DecCacheRef(program))
			program->Into(&other);

		referenced_total_weight -= weight;
		OP_DBG(("Timing out program %p of weight %u. Current size %u\n", program, weight, referenced_total_weight));
	}
	UpdateTimeout();
}
#endif // _STANDALONE

void
ES_Program_Cache::AddProgram(ES_ProgramCodeStatic *program)
{
	OP_NEW_DBG("ES_Program_Cache::AddProgram", "es_program_cache");
	double now = g_op_time_info->GetRuntimeMS();
	unsigned weight = Weight(program);

	const size_t cache_max_size = GetMaximumSize();

	// Don't cache really small (easy to recompile) or very large (ruins the rest of the cache) programs.
	if (weight > SMALL_PROGRAM_LIMIT && weight * 2 < cache_max_size)
	{
		OP_DBG(("Request to cache program %p of weight %d\n", program, weight));
		while (referenced.First() && referenced_total_weight + weight >= cache_max_size)
		{
			OP_DBG(("Cache is full (%u/%d)\n", static_cast<unsigned>(referenced_total_weight),  GetMaximumSize()));
			ES_ProgramCodeStatic *victim = referenced.First();
			unsigned victim_weight = Weight(victim);

			victim->Out();
			if (ES_CodeStatic::DecCacheRef(victim))
				victim->Into(&other);

			referenced_total_weight -= victim_weight;
			OP_DBG(("Evicting program %p of weight %u. Cache size now %u\n", program, victim_weight, referenced_total_weight));
		}

		program->Into(&referenced);
		ES_CodeStatic::IncCacheRef(program);

		referenced_total_weight += weight;
		OP_DBG(("Adding program %p of weight %u. Cache size now %u\n", program, weight, referenced_total_weight));
	}
	else
		program->Into(&other);

	program->program_cache = this;
	program->program_cache_time = now;

	UpdateTimeout();
}

void
ES_Program_Cache::RemoveProgram(ES_ProgramCodeStatic *program)
{
	OP_ASSERT(!referenced.HasLink(program) || !"Can't be deleted if it's in the list of referenced programs. Doh!");
	program->Out();
}

void
ES_Program_Cache::TouchProgram(ES_ProgramCodeStatic *program)
{
	if (referenced.HasLink(program))
	{
		program->Out();
		program->Into(&referenced);
		program->program_cache_time = g_op_time_info->GetRuntimeMS();
		UpdateTimeout();
	}
	else
	{
		program->Out();
		AddProgram(program);
	}
}

static BOOL
IsCompatible(ES_ProgramCodeStatic *program, ES_ProgramText *elements, unsigned elements_count, unsigned total_length)
{
	if (program->source_storage && program->source_storage->length == total_length)
	{
		uni_char *storage = program->source_storage->storage;

		for (unsigned index = 0; index < elements_count; ++index)
		{
			if (op_memcmp(storage, elements[index].program_text, elements[index].program_text_length * sizeof(uni_char)) != 0)
				return FALSE;

			storage += elements[index].program_text_length;
		}

		return TRUE;
	}
	else
		return FALSE;
}

ES_ProgramCodeStatic *
ES_Program_Cache::Find(ES_ProgramText *elements, unsigned elements_count)
{
	unsigned total_length = 0;

	for (unsigned index = 0; index < elements_count; ++index)
		total_length += elements[index].program_text_length;

	ES_ProgramCodeStatic *program;

	for (program = referenced.First(); program; program = program->Suc())
	{
		if (IsCompatible(program, elements, elements_count, total_length))
		{
			/* Remove and re-add the program.  This keeps the list of referenced
			   programs LRU ordered, and it also updates the 'program_cache_time'
			   of the program. */

			TouchProgram(program);
			OP_ASSERT(referenced.HasLink(program));
			return program;
		}
	}

	for (program = other.First(); program; program = program->Suc())
	{
		if (IsCompatible(program, elements, elements_count, total_length))
		{
			program->Out();
			AddProgram(program);
			return program;
		}
	}

	return NULL;
}
