/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#ifdef CORE_CODE_IN_DOC_MODULE
#include "modules/doc/win_man.h"
#else
#include "core/win_man.h"
#endif // CORE_CODE_IN_DOC_MODULE

#ifdef QUICK
# include "adjunct/quick/windows/BrowserDesktopWindow.h"
# include "adjunct/quick/Application.h"
#endif

#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/url/url_man.h"
#include "timing.h"
#include "tstdump.h"
#include <limits.h>

#ifdef _ACTIVATE_TIMING_

static FILE *timingfile=NULL;
static LONGLONG frequency;
static struct{
	LONGLONG time_used;
	LONGLONG time_used_min;
	LONGLONG time_used_max;
	LONGLONG last_started;
	unsigned call_count;
	unsigned long data_processed;
	unsigned long data_processed_max;
	unsigned long data_processed_min;
} timing_sections[TIMING_NUM_SECTIONS];
static DWORD last_saved;
static DWORD diff;

void ResetTimers()
{
	int i;
	for(i=0; i< TIMING_NUM_SECTIONS; i++)
	{
		timing_sections[i].time_used = 0 ;
		timing_sections[i].time_used_min = _UI64_MAX ;
		timing_sections[i].time_used_max = 0 ;
		timing_sections[i].last_started = 0;
		timing_sections[i].call_count = 0;
		timing_sections[i].data_processed = 0;
		timing_sections[i].data_processed_max = 0;
		timing_sections[i].data_processed_min = ULONG_MAX;
	}
}

void WriteTiming()
{
	if(timingfile == NULL)
		return;
	if(!diff)
		diff = (1000 - last_saved%1000);
	fprintf(timingfile, "% .12lu, % .12lu, % .12I64u",
		last_saved/1000UL, diff, frequency);

	int i;
	for(i=0; i< TIMING_NUM_SECTIONS; i++)
	{
		if(timing_sections[i].data_processed_min == ULONG_MAX)
			timing_sections[i].data_processed_min = timing_sections[i].data_processed_max;
		if(timing_sections[i].time_used_min == _UI64_MAX)
			timing_sections[i].time_used_min = timing_sections[i].time_used_max;

		fprintf(timingfile, ", % .12u, % .12I64u, % .12I64u, % .12I64u, % .12lu, % .12lu, % .12lu",
		timing_sections[i].call_count, timing_sections[i].time_used,
		timing_sections[i].time_used_min,timing_sections[i].time_used_max,
		timing_sections[i].data_processed, 
		timing_sections[i].data_processed_min, 
		timing_sections[i].data_processed_max
		);
	}
	fprintf(timingfile, "\n");
	ResetTimers();
	last_saved = GetTickCount();
}

void InitTiming()
{
	last_saved = GetTickCount();
	diff = 0;
	LARGE_INTEGER freq;
	if(!QueryPerformanceFrequency(&freq))
	{
		OP_ASSERT(FALSE);
	}
	frequency = freq.QuadPart;
	ResetTimers();
	timingfile = fopen("c:\\klient\\timing.txt","w");
}

void StartTiming(int section)
{
	if(section < 0 || section >= TIMING_NUM_SECTIONS)
		return;

	if(timing_sections[section].last_started != 0)
		return;

	DWORD ct = GetTickCount();
	ct -= ct%1000;
	diff = 0;
	if(ct > last_saved)
		WriteTiming();

	LARGE_INTEGER qt;
	QueryPerformanceCounter(&qt);
	timing_sections[section].last_started = qt.QuadPart;
	timing_sections[section].call_count ++;
}

void AddTimingProcessed(int section, unsigned long count)
{
	if(section < 0 || section >= TIMING_NUM_SECTIONS)
		return;

	if(timing_sections[section].last_started == 0)
		return;

	timing_sections[section].data_processed += count;
	if(count > timing_sections[section].data_processed_max)
		timing_sections[section].data_processed_max = count;
	if(count < timing_sections[section].data_processed_min)
		timing_sections[section].data_processed_min = count;
}


void StopTiming(int section)
{
	if(section < 0 || section >= TIMING_NUM_SECTIONS)
		return;

	if(timing_sections[section].last_started == 0)
		return;

	LARGE_INTEGER qt;
	QueryPerformanceCounter(&qt);

	LONGLONG diff = qt.QuadPart - timing_sections[section].last_started;

	timing_sections[section].time_used += diff;
	if(diff < timing_sections[section].time_used_min)
		timing_sections[section].time_used_min = diff;
	if(diff > timing_sections[section].time_used_max)
		timing_sections[section].time_used_max = diff;

	timing_sections[section].last_started = 0;

	DWORD ct = GetTickCount();
	diff = ct -last_saved;
	ct -= ct%1000;
	if(ct > last_saved)
		WriteTiming();
	diff = 0;
}

void FlushTiming()
{
	diff = 0;
	WriteTiming();
	if(timingfile)
	{
		fprintf(timingfile, "% .12lu, ", 0UL);
		fclose(timingfile);
	}
	timingfile = NULL;
}

#endif	//_ACTIVATE_TIMING_



#ifdef HTTP_BENCHMARK

#define BENCH_1 "http://certo.opera.com/manyimgs/pipeline.html"
#define BENCH_2 "http://developer.skolelinux.no/~herman/tc/pipelining/"
#define BENCH_3 "http://www.blooberry.com/24imgs/pipeline.html"
#define BENCH_4 "http://certo.opera.com/100img/"
#define BENCH_5 "http://developer.skolelinux.no/~herman/tc/100img/"

#define NOT_PERSISTENT FALSE
#define PERSISTENT TRUE

#define NOT_PIPELINED FALSE
#define PIPELINED TRUE

#define NOT_NEW_POLICY FALSE
#define NEW_POLICY 1
#define NEW_POLICY_2 2

#define BENCH_REPEAT_COUNT 5
#define BENCH_SEQUENCES 5

static const char *bench_url[] = {
	//BENCH_3
	BENCH_1, BENCH_2, 
	BENCH_4, BENCH_5,
};

static const int bench_max_conn[]  = { 1, 2, 3, 4, 5, 6,7, 8 ,9 , 10,11, 12,13, 14,15, 16};
//static const int bench_max_conn[]  = { 2, 4, 8, 16};


static const struct bench_config {
	BOOL persistent;
	BOOL pipeline;
	int new_policy;
	int  new_conn_after;
	int  min_conn_new_policy;
} benchmark_list_conn[] = {
	{PERSISTENT,		PIPELINED,		NOT_NEW_POLICY,  1, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 1, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 2, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 3, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 4, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 5, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 6, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 7, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 8, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 9, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		10, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		11, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		12, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		13, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		14, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		15, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		16, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 1, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 2, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 3, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 4, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 5, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 6, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 7, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 8, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		 9, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		10, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		11, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		12, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		13, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		14, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		15, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY,		16, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 1, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 2, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 3, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 4, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 5, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 6, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 7, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 8, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 9, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	10, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	11, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	12, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	13, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	14, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	15, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	16, 2 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 1, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 2, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 3, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 4, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 5, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 6, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 7, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 8, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	 9, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	10, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	11, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	12, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	13, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	14, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	15, 1 },
	{PERSISTENT,		PIPELINED,		NEW_POLICY_2,	16, 1 },
	{NOT_PERSISTENT,	NOT_PIPELINED,	NOT_NEW_POLICY,  1, 1 },
	{PERSISTENT,		NOT_PIPELINED,	NOT_NEW_POLICY,  1, 1 },
};


static int bench_sequence = 0;
static int bench_url_pos = 0;
static int bench_conn_pos = 0;
static int bench_maxconn_pos = 0;

static LONGLONG b_frequency=0;
static LONGLONG b_started=0;
static LONGLONG b_lastactive=0;

static double benchmarks[ARRAY_SIZE(bench_url)][BENCH_REPEAT_COUNT];
static int benchmarks_conncount[ARRAY_SIZE(bench_url)][BENCH_REPEAT_COUNT];
static double benchmarks_idleconn[ARRAY_SIZE(bench_url)][BENCH_REPEAT_COUNT];
static double benchmarks_waitconn[ARRAY_SIZE(bench_url)][BENCH_REPEAT_COUNT];
static int bench_count = 0;

BOOL bench_active = FALSE;
BOOL benchmark_persistent = TRUE;
BOOL benchmark_pipelined = TRUE;
int benchmark_new_policy = TRUE;
int  benchmark_conn_count = 8;
int	 benchmark_new_conn = 4;
int  benchmark_new_policy_after = 2;
unsigned long benchmark_count = 0;

void PrepareNextBenchMark_Step1(unsigned long c)
{
	if(c && c != benchmark_count)
		return;

	BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();
	if (browser_window)
	{
		browser_window->GetWorkspace()->CloseAll();
	}
	urlManager->CloseAllConnections();
	g_main_message_handler->PostDelayedMessage(33335, 0,0,500);
}

void PrepareNextBenchMark()
{
	urlManager->EmptyDCache();
	urlManager->ClearCookiesCommit();
}

void StartBenchmark()
{
	if(!bench_active)
		return;

	PrepareNextBenchMark();
	benchmark_count ++;

	benchmark_persistent = benchmark_list_conn[bench_conn_pos].persistent;
	benchmark_pipelined = benchmark_list_conn[bench_conn_pos].pipeline;
	benchmark_new_policy = benchmark_list_conn[bench_conn_pos].new_policy;
	benchmark_conn_count = bench_max_conn[bench_maxconn_pos];
	benchmark_new_conn = benchmark_list_conn[bench_conn_pos].new_conn_after;
	benchmark_new_policy_after = benchmark_list_conn[bench_conn_pos].min_conn_new_policy;
	benchmarks_conncount[bench_url_pos][bench_count] = 0;
	benchmarks_idleconn[bench_url_pos][bench_count] = 0;
	benchmarks_waitconn[bench_url_pos][bench_count] = 0;
	b_started = 0;
	b_lastactive = 0;

	TRAPD(op_err, prefsManager->WriteMaxConnectionsServerL(benchmark_conn_count));//FIXME:OOM

	//PrintfTofile("loading.txt","Starting Benchmark %lu\n", g_op_time_info->GetWallClockMS());
	windowManager->OpenURLNewWindow(bench_url[bench_url_pos]);
	g_main_message_handler->PostDelayedMessage(33334, 0,benchmark_count,180000);
}

void EndBenchmark()
{
	if(!bench_active || !b_started)
		return;

	//LARGE_INTEGER qt;
	//QueryPerformanceCounter(&qt);
	g_main_message_handler->RemoveDelayedMessage(33334, 0,benchmark_count);

	LONGLONG diff = b_lastactive - b_started;

	double time_used = (double) diff;

	benchmarks[bench_url_pos++][bench_count] = time_used/ ((double) b_frequency);

	if(bench_url_pos >= ARRAY_SIZE(bench_url))
	{
		bench_url_pos =0;
		bench_count++;
	}

	if(bench_count >= BENCH_REPEAT_COUNT)
	{
		int j;
		int k = bench_sequence * ARRAY_SIZE(bench_url);
		char *filename = (char *) g_memory_manager->GetTempBuf2k();
		OpString8 cur_time;
		time_t curt = prefsManager->CurrentTime();

		cur_time.Set(ctime(&curt));
		j = cur_time.FindFirstOf("\r\n");
		if(j)
			cur_time.Delete(j);
		
		for(j= 0; j< ARRAY_SIZE(bench_url); j++,k++)
		{
			int i;
			double b_sum= 0;
			double b_max, b_min;
			double item;
			double b_idle= 0;
			double b_idle_max, b_idle_min;
			double idle;
			double b_wait= 0;
			double b_wait_max, b_wait_min;
			double wait;
			int b_conn= 0;
			int b_conn_max, b_conn_min;
			int conn;

			
			sprintf(filename, "benchmarks%04d.txt", k);
			
			PrintfTofile(filename, "%s ,\t%d,\t%d,\t%d,\t%d,\t%d,\t%d", bench_url[j], 
				benchmark_persistent,benchmark_pipelined,benchmark_new_policy,benchmark_conn_count,benchmark_new_conn,benchmark_new_policy_after);

			b_sum= 0;
			b_idle= 0;
			b_wait= 0;
			b_conn= 0;
			
			b_max = b_min = benchmarks[j][0];
			b_conn_max = b_conn_min = benchmarks_conncount[j][0];
			b_idle_max = b_idle_min = benchmarks_idleconn[j][0];
			b_wait_max = b_wait_min = benchmarks_waitconn[j][0];
			
			for(i= 0; i< BENCH_REPEAT_COUNT; i++)
			{
				item = benchmarks[j][i];
				conn = benchmarks_conncount[j][i];
				idle = benchmarks_idleconn[j][i];
				wait = benchmarks_waitconn[j][i];
				
				b_sum += item;
				b_conn += conn;
				b_idle += idle;
				b_wait += wait;
				
				if(item< b_min)
					b_min = item;
				
				if(item> b_max)
					b_max = item;
				
				if(conn< b_conn_min)
					b_conn_min = conn;
				
				if(conn> b_conn_max)
					b_conn_max = conn;
				
				if(idle< b_idle_min)
					b_idle_min = idle;
				
				if(idle> b_idle_max)
					b_idle_max = idle;
				
				if(wait< b_wait_min)
					b_wait_min = wait;
				
				if(wait> b_wait_max)
					b_wait_max = wait;
				
				PrintfTofile(filename, ",\t%.8g,\t%.4d,\t%.8g,\t%.8g", item, conn, idle, wait);
			}
			
			PrintfTofile(filename, ", %s, %d\n", cur_time.CStr(),curt);
			sprintf(filename, "summary%04d.txt", k);
			b_sum /= i;
			b_conn /= i;
			b_idle /= i;
			b_wait /= i;
			PrintfTofile(filename, "%s ,\t%.8g,\t%.4d,\t%.8g,\t%.8g,\t%.8g,\t%.4d,\t%.8g,\t%.8g,\t%.8g,\t%.4d,\t%.8g,\t%.8g,\t%d,\t%d,\t%d,\t%d,\t%d,\t%d, %s, %d\n",
				bench_url[j], b_sum, b_conn, b_idle, b_wait, b_min, b_conn_min, b_idle_min, b_wait_min, b_max, b_conn_max, b_idle_max, b_wait_max,
				benchmark_persistent,benchmark_pipelined,benchmark_new_policy,benchmark_conn_count,benchmark_new_conn,benchmark_new_policy_after, cur_time.CStr(), curt);
		}
		bench_count = 0;

		bench_maxconn_pos ++;
		if(bench_maxconn_pos >= ARRAY_SIZE(bench_max_conn))
		{
			bench_maxconn_pos = 0;
			bench_conn_pos ++;
			if(bench_conn_pos >= ARRAY_SIZE(benchmark_list_conn))
			{
				bench_conn_pos = 0;
				bench_sequence++;

				if(bench_sequence >= BENCH_SEQUENCES)
				{
					for(j= 0; j< BENCH_SEQUENCES*ARRAY_SIZE(bench_url); j++)
					{
						sprintf(filename, "benchmarks%04d.txt", j);
						CloseDebugFile(filename);
						sprintf(filename, "summary%04d.txt", j);
						CloseDebugFile(filename);
					}
					bench_active = FALSE;
					return; // benchmark finished
				}
			}
		}
		g_main_message_handler->PostDelayedMessage(33334, 0,0,5000);
		return;
	}

	//StartBenchmark();
	g_main_message_handler->PostDelayedMessage(33334, 0,0,250);
}

void BenchMarkLoading()
{
	if(bench_active && !b_started)
	{
		LARGE_INTEGER qt;
		QueryPerformanceCounter(&qt);

		b_started = qt.QuadPart;
		b_lastactive = b_started;
	}
}

void BenchMarkLastActive()
{
	if(bench_active && b_started)
	{
		LARGE_INTEGER qt;
		QueryPerformanceCounter(&qt);

		b_lastactive = qt.QuadPart;
	}
}

void BenchMarkNewConn()
{
	if(bench_active && b_started)
	{
		benchmarks_conncount[bench_url_pos][bench_count]++;
	}
}

void BenchMarkAddConnWaitTime(LONGLONG last_active)
{
	if(bench_active && b_started && last_active != 0)
	{
		LARGE_INTEGER qt;
		QueryPerformanceCounter(&qt);

		LONGLONG diff = qt.QuadPart-last_active;

		double time_used = (double) diff;

		benchmarks_waitconn[bench_url_pos][bench_count]+= time_used/ ((double) b_frequency);
	}
}

void BenchMarkAddConnIdleTime(LONGLONG last_active)
{
	if(bench_active && b_started && last_active != 0)
	{
		LARGE_INTEGER qt;
		QueryPerformanceCounter(&qt);

		LONGLONG diff = qt.QuadPart-last_active;

		double time_used = (double) diff;

		benchmarks_idleconn[bench_url_pos][bench_count]+= time_used/ ((double) b_frequency);
	}
}

void BenchMarkGetTime(LONGLONG &last_active)
{
	if(bench_active && b_started)
	{
		LARGE_INTEGER qt;
		QueryPerformanceCounter(&qt);

		last_active = qt.QuadPart;
	}
	else
		last_active = 0;
}

void InitBenchmark()
{
	bench_active = TRUE;
	LARGE_INTEGER freq;
	if(!QueryPerformanceFrequency(&freq))
	{
		int test = 1;
		OP_ASSERT(FALSE);
	}
	b_frequency = freq.QuadPart;
	bench_count = 0;
	bench_url_pos = 0;
	bench_sequence = 0;
	bench_conn_pos = 0;
	bench_maxconn_pos = 0;

	StartBenchmark();
}


#endif
