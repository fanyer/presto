
#ifndef __TIMING_H__
#define __TIMING_H__


#define TIMING_COMM_LOAD			0
#define TIMING_CACHE_RETRIEVAL		1
#define TIMING_FILE_OPERATION		2
#define TIMING_FORMATTING			3
#define TIMING_DOC_LOAD				4
#define TIMING_IMG_LOAD				5
#define TIMING_DOC_DISPLAY			6
#define TIMING_COMM_LOAD_DS			0
#define TIMING_COMM_LOAD_HT			0
#define TIMING_COMM_LOAD_DS1		0
#define TIMING_COMM_LOAD_CB			0
#define TIMING_COMM_LOAD_CB1		0

#define TIMING_NUM_SECTIONS			7


#if defined(YNP_WORK) && !defined(NO_LONG_LONG)
//#define _ACTIVATE_TIMING_
//#define HTTP_BENCHMARK
#endif

#ifdef _ACTIVATE_TIMING_
void InitTiming();
void StartTiming(int section);
void AddTimingProcessed(int section, unsigned long count);
void StopTiming(int section);
void FlushTiming();

#define __TIMING_CODE(text) text
#define __DO_INIT_TIMING() InitTiming()
#define __DO_START_TIMING(sec) StartTiming(sec)
#define __DO_STOP_TIMING(sec) StopTiming(sec)
#define __DO_ADD_TIMING_PROCESSED(sec, cnt) AddTimingProcessed(sec, (cnt))
#define __DO_FLUSH_TIMING() FlushTiming()
#else
#define __TIMING_CODE(text)
#define __DO_INIT_TIMING() 
#define __DO_START_TIMING(sec) 
#define __DO_ADD_TIMING_PROCESSED(sec, cnt)
#define __DO_STOP_TIMING(sec) 
#define __DO_FLUSH_TIMING() 
#endif

#ifdef HTTP_BENCHMARK
void InitBenchmark();
void StartBenchmark();
void EndBenchmark();
void BenchMarkLoading();
void BenchMarkLastActive();
void BenchMarkNewConn();
void BenchMarkGetTime(LONGLONG &last_active);
void BenchMarkAddConnIdleTime(LONGLONG last_active);
void BenchMarkAddConnWaitTime(LONGLONG last_active);



extern BOOL bench_active;
extern BOOL benchmark_persistent;
extern BOOL benchmark_pipelined;
extern int benchmark_new_policy;
extern int  benchmark_conn_count;
extern int	 benchmark_new_conn;
extern int  benchmark_new_policy_after;

#endif


#endif //__TIMING_H__

