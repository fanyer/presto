/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/hardcore/opera/opera.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/base/periodic_task.h"

#include "modules/url/url_api.h"
#include "modules/url/url_man.h"
#include "modules/debug/debug.h"
#include "modules/probetools/probepoints.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/pi/OpSystemInfo.h"

#ifdef HC_MODULE_INIT_PROFILING
# ifdef PLATFORM_HC_PROFILE_INCLUDE
#  include PLATFORM_HC_PROFILE_INCLUDE
# endif

# ifndef HC_PROFILE_INIT_START
/** The default definition of HC_PROFILE_INIT_START is to use a probe-point
 * OP_PROBE_HC_INIT_MODULE with level 1 and parameter i and set the name of the
 * probe-point to the name of the module. */
#  define HC_PROFILE_INIT_START(i, module_name)						\
	{																\
		OP_PROBE1_PARAM_L(OP_PROBE_HC_INIT_MODULE, i)
# endif
# ifndef HC_PROFILE_INIT_STOP
#  define HC_PROFILE_INIT_STOP(i, module_name)	\
	}
# endif

# ifndef HC_PROFILE_DESTROY_START
/** The default definition of HC_PROFILE_DESTROY_START is to use a probe-point
 * OP_PROBE_HC_DESTROY_MODULE with level 1 and parameter i and set the name of
 * the probe-point to the name of the module. */
#  define HC_PROFILE_DESTROY_START(i, module_name)					\
	{																\
		OP_PROBE1_PARAM_L(OP_PROBE_HC_DESTROY_MODULE, i)
# endif
# ifndef HC_PROFILE_DESTROY_STOP
# define HC_PROFILE_DESTROY_STOP(i, module_name)	\
	}
# endif

#else // !HC_MODULE_INIT_PROFILING

# undef HC_PROFILE_INIT_START
# define HC_PROFILE_INIT_START(i, module_name)
# undef HC_PROFILE_INIT_STOP
# define HC_PROFILE_INIT_STOP(i, module_name)
# undef HC_PROFILE_DESTROY_START
# define HC_PROFILE_DESTROY_START(i, module_name)
# undef HC_PROFILE_DESTROY_STOP
# define HC_PROFILE_DESTROY_STOP(i, module_name)
#endif // HC_MODULE_INIT_PROFILING

extern void StartOutOfMemoryHandlingL();

#if defined(_DEBUG) || defined(HC_MODULE_INIT_PROFILING)
#define SET_MODULE_NAME(i, x) module_names[i] = x
#else
#define SET_MODULE_NAME(i, x)
#endif
#ifdef _DEBUG
#define SET_INIT_MESSAGE(x) init_message = x
#define LOG_DETAILS(x) log_printf x
#else  // defined(_DEBUG) || defined(HC_MODULE_INIT_PROFILING)
#define SET_INIT_MESSAGE(x)
#define LOG_DETAILS(x)
#endif // defined(_DEBUG) || defined(HC_MODULE_INIT_PROFILING)

#include "modules/hardcore/opera/hardcore_opera.inc"

void
CoreComponent::InitL(const OperaInitInfo& info)
{
	/* Set this component as the current running component. */
	OpComponentActivator activator;
	LEAVE_IF_ERROR(activator.Activate(this));
	ANCHOR(OpComponentActivator, activator);

	OP_PROBE1_L(OP_PROBE_HC_INIT);
#ifdef INIT_BLACKLIST
	if (!info.use_black_list)
		op_memset(black_list, 0, sizeof(black_list));
#endif // INIT_BLACKLIST

	/* probetools is expected to be the first module to be initialised to be
	 * able to add probe-points for the module initialisation of the other
	 * modules: */
	failed_module_index = 0;
	SET_INIT_MESSAGE(module_names[0]);
	LOG_DETAILS(("LOG: Init module %s\n", module_names[0]));
	modules[0]->InitL(info);

	// initialise the other modules
	for ( unsigned int i = 1; i < OPERA_MODULE_COUNT; i++ )
	{
#ifdef INIT_BLACKLIST
		if ( !((black_list[i>>3] >> (i&7)) & 0x1) )
#endif // INIT_BLACKLIST
		{
			failed_module_index = i;
			SET_INIT_MESSAGE(module_names[i]);
			LOG_DETAILS(("LOG: Init module %s\n", module_names[i]));
			HC_PROFILE_INIT_START(i, module_names[i]);
			modules[i]->InitL(info);
			HC_PROFILE_INIT_STOP(i, module_names[i]);
		}
	}

	failed_module_index = -1;

	SET_INIT_MESSAGE("StartOutOfMemoryHandlingL");
	LOG_DETAILS(("LOG: Init OOM handling\n"));
	StartOutOfMemoryHandlingL();

	SET_INIT_MESSAGE("Opera object initialized");
	LOG_DETAILS(("LOG: Init modules done\n"));
}

class LegacyMessages : public OpTypedMessageSelection
{
public:
	LegacyMessages(const OpMessageAddress& component)
		: m_component(component) {}

	BOOL IsMember(const OpTypedMessage* message) const {
		return message->GetDst().co == m_component.co && message->GetType() == OpLegacyMessage::Type;
	}

protected:
	OpMessageAddress m_component;
};

void
CoreComponent::Destroy()
{
	const int OPERA_MODULE_COUNT_WITHOUT_PROBETOOLS_DESTROYED = 2;
	{
		/* Set this component as the current running component. */
		OpComponentActivator activator;
		if (OpStatus::IsError(activator.Activate(this)))
			return;

		OP_PROBE1_L(OP_PROBE_HC_DESTROY);
		/* Prevent new connections and name lookups from happening when URL
		   resources are being shut down. Bug CORE-14134 */

		if (g_url_api)
			g_url_api->SetPauseStartLoading(TRUE);

		m_is_alive = false;		// Don't accept any more legacy messages.
		/* Locally destined legacy messages are managed by the memory manager in
		 * hardcore and are removed from the global queue before terminating
		 * hardcore. New legacy messages will no longer be accepted. */
		LegacyMessages legacy_messages(GetAddress());
		g_component_manager->RemoveMessages(&legacy_messages);

		/* Shut down modules (only initialized). The module that failed to
		 * initialise is not destroyed. The module probetools is used to
		 * benchmark module destruction so it must be destroyed last except
		 * for the fact that probetools depends on stdlib for report formatting
		 * so we schedule probetools for penultimate destruction and stdlib is
		 * the last module to be destroyed. For this reason stdlib will always
		 * be module 0 and probetools will always be module 1, if they are loaded.
		 * This means that stdlib and probetools are destroyed after the
		 * OP_PROBE_HC_DESTROY probe-point has finished. */
		int start_module = failed_module_index == -1 ? OPERA_MODULE_COUNT : failed_module_index;
		for(int i = start_module - 1; i > OPERA_MODULE_COUNT_WITHOUT_PROBETOOLS_DESTROYED-1; --i)
		{
#ifdef INIT_BLACKLIST
			if ( !((black_list[i>>3] >> (i&7)) & 0x1) )
#endif // INIT_BLACKLIST
			{
				LOG_DETAILS(("LOG: Destroy module %s\n", module_names[i]));
				HC_PROFILE_DESTROY_START(i, module_names[i]);
				modules[i]->Destroy();
				HC_PROFILE_DESTROY_STOP(i, module_names[i]);
			}
		}
	}

	/* Destroy probetools after the OP_PROBE_HC_DESTROY probe-point has
	 * finished and also destroy its dependency the stdlib module. */
	int continue_destruction_at_module_idx = failed_module_index == -1 ? OPERA_MODULE_COUNT_WITHOUT_PROBETOOLS_DESTROYED-1 : MIN(OPERA_MODULE_COUNT_WITHOUT_PROBETOOLS_DESTROYED-1, failed_module_index-1);
	for (int i = continue_destruction_at_module_idx; i >= 0; i--)
#ifdef INIT_BLACKLIST
		if ( !((black_list[i>>3] >> (i&7)) & 0x1) )
#endif // INIT_BLACKLIST
		{
			LOG_DETAILS(("LOG: Destroy module %s\n", module_names[i]));
			modules[i]->Destroy();
		}

	LOG_DETAILS(("LOG: Destroy modules done\n"));

	OpComponent::Destroy();
}

#ifdef MESSAGELOOP_RUNSLICE_SUPPORT
/* virtual */ OP_STATUS
CoreComponent::RequestRunSlice()
{
	OP_ASSERT(g_component_manager);
	return g_component_manager->YieldPlatform();
}
#endif // MESSAGELOOP_RUNSLICE_SUPPORT

/* virtual */ OP_STATUS
CoreComponent::DispatchMessage(const OpTypedMessage* message)
{
	OP_STATUS s = OpComponent::DispatchMessage(message);

	if (OpStatus::IsRaisable(s))
		g_memory_manager->RaiseCondition(s);

	return s;
}

/* virtual */ OP_STATUS
CoreComponent::ProcessMessage(const OpTypedMessage* message)
{
	OP_STATUS status = OpStatus::OK;
	running = TRUE;

	g_periodic_task_manager->RunTasks();
	hardcore_module.message_handler->HandleErrors();

	if (message->GetType() == OpLegacyMessage::Type)
	{
		OpLegacyMessage* m = OpLegacyMessage::Cast(message);
		CollectEntropy(m);

#ifdef OPERA_PERFORMANCE
		OpProbeTimestamp start_processing;
		start_processing.timestamp_now();
		OpMessage performance_message_type = m->GetMessage();
#endif // OPERA_PERFORMANCE

		m->GetMessageHandler()->HandleMessage(m->GetMessage(), m->GetParam1(), m->GetParam2());

#ifdef OPERA_PERFORMANCE
		OpProbeTimestamp end_processing;
		end_processing.timestamp_now();
		OpProbeTimestamp ts = end_processing - start_processing;
		double processing_diff = ts.get_time();
		if (processing_diff > 100)
		{
			extern const char* GetStringFromMessage(OpMessage);
			urlManager->AddToMessageLog(GetStringFromMessage(performance_message_type), start_processing, processing_diff);
		}
#endif // OPERA_PERFORMANCE
	}
	else
		status = OpComponent::ProcessMessage(message);

	running = FALSE;
	return status;
}

BOOL
CoreComponent::FreeCachedData()
{
	BOOL result = FALSE;

	for(int i=0; i < OPERA_MODULE_COUNT; ++i)
	{
		result |= modules[i]->FreeCachedData(FALSE);
	}

	return result;
}

void
CoreComponent::CollectEntropy(OpLegacyMessage* message)
{
	//
	// The following 'entropy' construct is used for entropy collection.
	// The entropy is collected for every message a short time after
	// startup, then once every 30 messages or so.
	//
	struct Entropy
	{
		double time;					// Time
		OpLegacyMessage* msg;           // Pointer to message in memory
		struct
		{
			OpMessage msg;
			MH_PARAM_1 par1;
			MH_PARAM_2 par2;
			MessageHandler* mh;
		} msg_copy;                     // Copy of message
		char random_stack_contents[16];	// Previous/old stack contents /* ARRAY OK 2011-06-23 terjes */
	} entropy;

	m_entropy_collect_count++;

	switch (m_entropy_collect_count >> 5)
	{
		default:
			//
			// This is when we have done nothing for a while, but it
			// is time to collect some entropy again. Reset the
			// counter back to 32 to continue doing nothing.
			//
			m_entropy_collect_count = 32;	// Fall through

		case 0:
			//
			// m_entropy_collect_count == [0...31]
			// This is during startup, when we collect every message.
			// This is needed to jump-start the PRNGs to a good start
			// before they are used for anything.
			//

			//
			// Every byte obtained in this way yields about 2 bits
			// of entropy (measured by running gzip -9 on resulting
			// bytestream and looking at compression factor, which
			// was about 3.4 - so 2 bits entropy / 8 bits should
			// be an acceptable approximation).
			//
			entropy.time = g_op_time_info->GetRuntimeMS();
			entropy.msg = message;
			entropy.msg_copy.msg = message->GetMessage();
			entropy.msg_copy.par1 = message->GetParam1();
			entropy.msg_copy.par2 = message->GetParam2();
			entropy.msg_copy.mh = message->GetMessageHandler();
#ifdef VALGRIND
			// Tell valgrind not to assume the struct is uninitialized.
			// Old stack contents, padding etc. is used for randomness.
			op_valgrind_set_defined(&entropy, sizeof(entropy));
#endif // VALGRIND
			OpRandomGenerator::AddEntropyAllGenerators(&entropy, sizeof(entropy), sizeof(entropy) * 2);
			// Fall through

		case 1:
			//
			// m_entropy_collect_count == [32...63]
			// This is when we do nothing for a while, until we bump
			// into the default case. Add 'case 2', 'case 3' etc.
			// to extend the period of doing nothing.
			//
			break;
	}
}
