/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(_MACINTOSH_)

#include "platforms/crashlog/crashlog.h"
#include "platforms/crashlog/gpu_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>

#include "adjunct/quick/quick-version.h"
#include "platforms/mac/Resources/buildnum.h"
#include "platforms/mac/util/systemcapabilities.h"

#define STACK_DUMP_SIZE 0x2000
#define DATA_DUMP_SIZE 0x400
#define CODE_DUMP_SIZE 0x10

const char* g_crashaction = 0;
const char* g_plugin_crashlogfolder = 0;

GpuInfo _gpu_info;
GpuInfo *g_gpu_info = &_gpu_info;

struct byte_range
{
	unsigned long range_start;
	unsigned long range_end;
	byte_range *next;
};

struct byte_range_list
{
	byte_range_list() : first(NULL) {}
	~byte_range_list()
	{
		byte_range *runner = first;
		while (runner)
		{
			byte_range *current = runner;
			runner = runner->next;
			delete current;
		}
	}
	byte_range *first;
};

struct dll_line
{
	dll_line() : next(NULL) {}
	~dll_line() { delete[] line; }
	char *line;
	dll_line *next;
};

struct dll_list
{
	dll_list() : first(NULL) {}
	~dll_list()
	{
		dll_line *runner = first;
		while (runner)
		{
			dll_line *current = runner;
			runner = runner->next;
			delete current;
		}
	}
	dll_line *first;
	dll_line *last;
};

int get_appName(char *name, uint32_t size)
{
	return _NSGetExecutablePath(name, &size);
/*
	ProcessSerialNumber psn;
	FSRef ref_app;

	if (GetCurrentProcess(&psn) != noErr)
		return -1;
		
	if (GetProcessBundleLocation(&psn, &ref_app) != noErr)
		return -1;
		
	if (FSRefMakePath(&ref_app, (UInt8*)name, size) != noErr)
		return -1;

	if (strlcat(name, "/Contents/MacOS/Opera", size) >= size)
		return -1;

	return 0;
*/
}

void dump_chars(FILE *log_file, BYTE *&buffer, UINT &read_size)
{
	putc(' ', log_file);
	for (UINT i = 0; i < 16; i++)
	{
		BYTE mem_byte = *buffer++;
		putc(mem_byte && (mem_byte < 9 || mem_byte > 14) ? mem_byte : '.', log_file);

		if (--read_size == 0)
			break;
	}
	putc('\n', log_file);
}

void add_pointer(byte_range_list &memdump_list, unsigned long pointer, unsigned long stack_pointer, task_t task, byte_range_list &code_ranges)
{
	// don't dump data that's in the stackdump anyway
	if (pointer >= stack_pointer && pointer < stack_pointer+STACK_DUMP_SIZE)
		return;

	int disposition, ref_count;

	if (vm_map_page_query(task, pointer, &disposition, &ref_count) != KERN_SUCCESS ||
		!(disposition & VM_PAGE_QUERY_PAGE_PRESENT))
		return;

	UINT dump_len = DATA_DUMP_SIZE;

	byte_range *current = code_ranges.first;
	while (current)
	{
		if (pointer >= current->range_start && pointer < current->range_end)
		{
			dump_len = CODE_DUMP_SIZE;
			break;
		}
		current = current->next;
	}

	unsigned long dump_start = pointer - dump_len;
	if (dump_start > pointer)	
		dump_start = 0;
	unsigned long dump_end = pointer + dump_len;
	if (dump_end < pointer)
		dump_end = ULONG_MAX;

	current = NULL;
	byte_range *next = memdump_list.first;
	while (next)
	{
		if (dump_start < next->range_start)
		{
			if (dump_end < next->range_start)
				break;

			next->range_start = dump_start;
			return;
		}

		current = next;
		next = current->next;

		if (dump_start <= current->range_end)
		{
			if (dump_end <= current->range_end)
				return;

			if (next && dump_end >= next->range_start)
			{
				current->range_end = next->range_end;
				current->next = next->next;
				delete next;
			}
			else
				current->range_end = dump_end;

			return;
		}
	}

	byte_range *new_range = new byte_range;
	if (new_range)
	{
		new_range->range_start = dump_start;
		new_range->range_end = dump_end;
		if (!current)
		{
			new_range->next = memdump_list.first;
			memdump_list.first = new_range;
		}
		else
		{
			new_range->next = current->next;
			current->next = new_range;
		}
	}
}

struct _signal
{
	int sig_num;
	const char *signame;
} signals[] =
{
	{SIGSEGV, "SIGSEGV"},
	{SIGBUS,  "SIGBUS"},
	{SIGILL,  "SIGILL"},
	{SIGFPE,  "SIGFPE"},
	{SIGABRT, "SIGABRT"},
//	{SIGPIPE, "SIGPIPE"},
	{SIGTRAP, "SIGTRAP"}
};

struct sigaction old_actions[ARRAY_SIZE(signals)];

void sighandler(int sig);

void InstallCrashSignalHandler()
{
	struct sigaction action;
	action.sa_handler = sighandler;
	action.sa_flags = SA_NODEFER;
	sigemptyset(&action.sa_mask);

	for (unsigned int i=0; i<ARRAY_SIZE(signals); i++)
		sigaction(signals[i].sig_num, &action, &old_actions[i]);
}

void RemoveCrashSignalHandler()
{
	for (unsigned int i=0; i<ARRAY_SIZE(signals); i++)
		sigaction(signals[i].sig_num, &old_actions[i], 0); 
}

UINT g_wait_for_debugger = 0;

void sighandler(int sig)
{
#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "opera [crashlog debug %d]: entering signal handler\n", time(NULL));
#endif // CRASHLOG_DEBUG
	char operaexe[PATH_MAX];
	char pid_string[32];
	char* args[6];
	UINT i;

	while (g_wait_for_debugger <= 50)
	{
		if (!g_wait_for_debugger)
		{
			// The crash logging process needs a send right to our Mach task port to retrieve crash information
			// It could simply get it via task_for_pid(), but that pops up a scary administrative privileges
			// password dialog.
			// Infuriatingly, there is no easy way to exchange rights between parent and child processes,
			// because ports are not inherited - except a handful of special ports.
			// Among those are the exception ports, so we just take one that by all accounts seems to be
			// unused anyway and set it to our task port, which the child can then easily get a send right to

			task_t task_self = mach_task_self();
			task_set_exception_ports(task_self, EXC_MASK_RPC_ALERT, task_self, EXCEPTION_DEFAULT, 0);

			pid_t fork_pid, pid = getpid();

			if (sprintf(pid_string, "%lu %lu", (unsigned long)pid, (unsigned long)&_gpu_info) == -1 ||
				get_appName(operaexe, PATH_MAX) == -1 || 
			 	(fork_pid = fork()) == -1)
			 	break;

			if (!fork_pid)
		 	{
#ifdef CRASHLOG_DEBUG
				fprintf(stderr, "opera [crashlog debug %d]: in child\n", time(NULL));
#endif
				// child process
		 		fork_pid = fork();
		 		if (!fork_pid)
		 		{
#ifdef CRASHLOG_DEBUG
					fprintf(stderr, "opera [crashlog debug %d]: in grandchild\n", time(NULL));
#endif
					// detached grandchild process
					// give the intermediate child time to exit
					usleep(10000);

					i=0;
					args[i++] = operaexe;
					args[i++] = (char*)"-write_crashlog"; // avoid warning for now
					args[i++] = pid_string;
					if (g_plugin_crashlogfolder)
					{
						args[i++] = (char*)"-logfolder";
						args[i++] = (char*)g_plugin_crashlogfolder;
					}
					else if (g_crashaction) {
						args[i++] = (char*)"-crashaction";
						args[i++] = (char*)g_crashaction;
					}
					args[i++] = 0;

					// now we can debug the original process
					execv(operaexe, args);
				}
				_exit(0);

			}
			else
			{
				// need to wait for the intermediate child
				// or else it becomes a zombie
				int stat_loc;
				waitpid(fork_pid, &stat_loc, 0);
#ifdef CRASHLOG_DEBUG
				fprintf(stderr, "opera [crashlog debug %d]: intermediate child exited\n", time(NULL));
#endif
			}
		}
		g_wait_for_debugger++;
		usleep(200000);
#ifdef CRASHLOG_DEBUG
		fprintf(stderr, "opera [crashlog debug %d]: waited for debugger %d\n", time(NULL), g_wait_for_debugger);
#endif
		return;
	}

#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "opera [crashlog debug %d]: giving up waiting for debugger\n", time(NULL));
#endif
	// log writing failed, restore default signal handling
	RemoveCrashSignalHandler();
#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "opera [crashlog debug %d]: leaving signal handler\n", time(NULL));
#endif
}

#if !TARGET_CPU_X86
void vsprintf_var(char *buffer, char *format, long *args)
{
	char c;
	do
	{
		char *format_start = format;
		c = *format++;
		if (c == '%')
		{
			do { c = *format++; } while (c != 'X' && c != 's');
			c = *format;
			*format = 0;
			buffer += sprintf(buffer, format_start, *args);
			*format = c;
			args++;
		}
		else
		{
			*buffer++ = c;
		}
	}
	while (c);
}
#endif

void WriteCrashlog(pid_t pid, GpuInfo* gpu_info, char* log_filename, int log_filename_size, const char* location)
{
#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "opera [crashlog debug %d]: WriteCrashlog\n", time(NULL));
#endif
	int status;
	UINT i;
	const char *signame = NULL;

	ptrace(PT_ATTACH, pid, 0, 0);

	while (1) {
		waitpid(pid, &status, 0);

		if (!WIFSTOPPED(status))
		{
#ifdef CRASHLOG_DEBUG
			fprintf(stderr, "opera [crashlog debug %d]: process not stopped, aborting!\n", time(NULL));
#endif
			// should never happen
			return;
		}
		int sig = WSTOPSIG(status);

		for (i=0; i<ARRAY_SIZE(signals); i++)
		{
			if (signals[i].sig_num == sig)
			{
				signame = signals[i].signame;
				break;
			}
		}

		if (signame)
			break;

#ifdef CRASHLOG_DEBUG
		fprintf(stderr, "opera [crashlog debug %d]: skipping sig %d\n", time(NULL), sig);
#endif
		ptrace(PT_CONTINUE, pid, (caddr_t)1, 0);
	}

#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "opera [crashlog debug %d]: logging sig %s\n", time(NULL), signame);
#endif
	dll_list dlls;
	byte_range_list code_ranges;
	byte_range_list memdump_list;

	time_t gt;

	time(&gt);
	struct tm *lt = localtime(&gt);

	int log_dir_length = snprintf(log_filename, log_filename_size, "%s/%s%04d%02d%02d%02d%02d%02d%s",
			location ? location : "/var/tmp", location ? "crash" : "opera-",
			lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec,
			location ? ".txt" : "");

	if (log_dir_length >= log_filename_size)
	{
		fprintf(stderr, "opera [crash logging]: Crash log writing failed, crash log directory path too long.\n");
		return;
	}

	FILE* log_file;
	if (!location)
	{
		// If a folder for the crashlog wasn't given, we create our own folder
		if (mkdir(log_filename, 0700) != 0)
		{
			fprintf(stderr, "opera [crash logging]: Crash log writing failed, failed to create crash log directory (errno: %s, path: %s)\n", strerror(errno), log_filename);
			return;
		}

		char * log_crashtxt = new char[log_dir_length + 11];
		if (!log_crashtxt)
			return;
		memcpy(log_crashtxt, log_filename, log_dir_length);
		memcpy(log_crashtxt + log_dir_length, "/crash.txt", 11); // Includes the terminating NUL

		log_file = fopen(log_crashtxt, "wt");
		delete[] log_crashtxt;
	}
	else
	{
		log_file = fopen(log_filename, "wt");
	}

	if (!log_file)
	{
		fprintf(stderr, "opera [crash logging]: Crash log writing failed, error writing to file %s%s\n!", log_filename, location ? "" : "/crash.txt");
		return;
	}

	task_t task_self = mach_task_self();
	task_t task;

	// get the exception port which was actually set to the task port of the crashed process by the sighandler
	exception_mask_t dummy1;
	exception_behavior_t dummy2;
	thread_state_flavor_t dummy3;
	mach_msg_type_number_t exc_count = 1;
	task_get_exception_ports(task_self, EXC_MASK_RPC_ALERT, &dummy1, &exc_count, &task, &dummy2, &dummy3);
	
	// write GPU info
	vm_offset_t dst_adr;
	mach_msg_type_number_t dst_size;
	
	if (gpu_info && vm_read(task, (vm_address_t)gpu_info, sizeof(GpuInfo), &dst_adr, &dst_size) == KERN_SUCCESS)
	{
		GpuInfo *pInfo = (GpuInfo*)dst_adr;
		if (pInfo->initialized != GpuInfo::NOT_INITIALIZED)
		{
			char gpuinfo_name[MAX_PATH];
			strlcpy(gpuinfo_name, log_filename, MAX_PATH);
			strlcat(gpuinfo_name, "/gpu_info.txt", MAX_PATH);
			FILE *gpu_file = fopen(gpuinfo_name, "wt");
			if (!gpu_file)
			{
				fprintf(stderr, "opera [crash logging]: Gpu info writing failed, error writing to file %s!\n", gpuinfo_name);
				fclose(log_file);
				return;
			}
			if (fwrite(pInfo->device_info, pInfo->device_info_size, 1, gpu_file) < 1)
			{
				fprintf(stderr, "opera [crash logging]: Gpu info writing failed, error writing to file %s!\n", gpuinfo_name);
				fclose(log_file);
				fclose(gpu_file);
				return;
			}
			fclose(gpu_file);
		}
		else
		{
			fprintf(stderr, "opera [crash logging]: Gpu info empty!\n");
		}
		vm_deallocate(task_self, dst_adr, dst_size);
	}
	else
	{
		fprintf(stderr, "opera [crash logging]: No Gpu info.\n");
	}

	char *exe_name = NULL;

	vm_address_t vmaddr = 0;
	vm_size_t vmsize;
	uint32_t nesting_depth = 0;
	struct vm_region_submap_info_64 region_info;

	while (1)
	{
		mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT_64;

		if (vm_region_recurse_64(task, &vmaddr, &vmsize, &nesting_depth,
								(vm_region_recurse_info_t)&region_info, &info_count) != KERN_SUCCESS)
		{
			break;
		}

		if (region_info.is_submap)
		{
			nesting_depth++;
			continue;
		}

/*		char buffer[PATH_MAX];
		int ret_val = proc_regionfilename(pid, vmaddr, buffer, PATH_MAX);
		buffer[ret_val] = 0;*/

		struct proc_regionwithpathinfo {
			char dummy[0xF8];			// struct proc_regioninfo + struct vnode_info;
			char vip_path[MAXPATHLEN];
		} path_info;

		char *obj_name = NULL;

		path_info.vip_path[0] = 0;
		
#define PROC_PIDREGIONPATHINFO 8
		syscall(SYS_proc_info, 2, pid, PROC_PIDREGIONPATHINFO, (uint64_t)vmaddr, &path_info, sizeof(path_info));
		
		if (path_info.vip_path[0])
			obj_name = path_info.vip_path;
		
/********************/
		if (obj_name)
		{
			if (!exe_name)
				exe_name = strdup(obj_name);

#if TARGET_CPU_X86
			unsigned int line_len = strlen(obj_name)+12+2*8;
#else
			unsigned int line_len = strlen(obj_name)+12+2*12;
#endif

			char *line_text = new char[line_len];
			if (line_text)
			{
#if TARGET_CPU_X86
				sprintf(line_text, "%08X-%08X %c%c%c     %s\n", vmaddr, vmaddr+vmsize,
#else
				sprintf(line_text, "%012lX-%012lX %c%c%c     %s\n", vmaddr, vmaddr+vmsize,
#endif
										(region_info.protection & VM_PROT_READ) ?'r':'-',
										(region_info.protection & VM_PROT_WRITE)?'w':'-',
										(region_info.protection & VM_PROT_EXECUTE)?'x':'-',
										obj_name);

				dll_line *new_dll = new dll_line;
				if (new_dll)
				{
					new_dll->line = line_text;

					if (!dlls.first)
						dlls.first = new_dll;
					else
						dlls.last->next = new_dll;

					dlls.last = new_dll;
				}
			}
		}

		if ((region_info.protection & VM_PROT_EXECUTE) && !(region_info.protection & VM_PROT_WRITE))
		{
			byte_range *range = new byte_range;
			if (range)
			{
				range->range_start = vmaddr;
				range->range_end = vmaddr+vmsize;
				range->next = code_ranges.first;
				code_ranges.first = range;
			}
		}
/********************/

		vmaddr += vmsize;
	}

	thread_act_array_t threads;
	mach_msg_type_number_t thread_count;
	
	if (task_threads(task, &threads, &thread_count) != KERN_SUCCESS)
	{
		fprintf(stderr, "opera [crash logging]: Error getting thread list!\n");
		fclose(log_file);
		return;
	}

	thread_act_t crashed_thread = 0;
	UINT crashed_thread_num = 0;

	for (i = 0; i < thread_count; i++)
	{
		thread_act_t cur_thread = threads[i];

#if TARGET_CPU_X86
		x86_exception_state32_t	exc_state;
		mach_msg_type_number_t state_count = x86_EXCEPTION_STATE32_COUNT;
		if (!crashed_thread &&
			thread_get_state(cur_thread, x86_EXCEPTION_STATE32, (thread_state_t)&exc_state, &state_count) == KERN_SUCCESS &&
#else
		x86_exception_state64_t	exc_state;
		mach_msg_type_number_t state_count = x86_EXCEPTION_STATE64_COUNT;
		if (!crashed_thread &&
			thread_get_state(cur_thread, x86_EXCEPTION_STATE64, (thread_state_t)&exc_state, &state_count) == KERN_SUCCESS &&
#endif
			exc_state.__trapno)
		{
			crashed_thread = cur_thread;
			crashed_thread_num = i;
		}
        if (i)
			mach_port_deallocate(task_self, cur_thread);
	}

	crashed_thread = threads[0];

	vm_deallocate(task_self, (vm_address_t)threads, thread_count * sizeof(thread_act_t));

	if (!crashed_thread)
	{
		fprintf(stderr, "opera [crash logging]: Error getting thread state!\n");
		fclose(log_file);
		return;
	}

#ifndef _DEBUG
#if TARGET_CPU_X86
	fprintf(log_file, "OPERA-CRASHLOG V1 desktop %s %d mac\n", VER_NUM_STR, crashed_thread_num ? 9999 : VER_BUILD_NUMBER);
#else
	fprintf(log_file, "OPERA-CRASHLOG V1 desktop %s %d mac x64\n", VER_NUM_STR, crashed_thread_num ? 9999 : VER_BUILD_NUMBER);
#endif
#endif // !_DEBUG

#if TARGET_CPU_X86
	struct CONTEXT
	{
		x86_thread_state32_t regs;
		x86_float_state32_t fpregs;
	} context;

	mach_msg_type_number_t state_count = x86_THREAD_STATE32_COUNT;
	mach_msg_type_number_t fl_state_count = x86_FLOAT_STATE32_COUNT;

	kern_return_t ret1 = thread_get_state(crashed_thread, x86_THREAD_STATE32, (thread_state_t)&context.regs, &state_count);
	kern_return_t ret2 = thread_get_state(crashed_thread, x86_FLOAT_STATE32, (thread_state_t)&context.fpregs, &fl_state_count);
#else
	struct CONTEXT
	{
		x86_thread_state64_t regs;
		x86_float_state64_t fpregs;
	} context;

	mach_msg_type_number_t state_count = x86_THREAD_STATE64_COUNT;
	mach_msg_type_number_t fl_state_count = x86_FLOAT_STATE64_COUNT;

	kern_return_t ret1 = thread_get_state(crashed_thread, x86_THREAD_STATE64, (thread_state_t)&context.regs, &state_count);
	kern_return_t ret2 = thread_get_state(crashed_thread, x86_FLOAT_STATE64, (thread_state_t)&context.fpregs, &fl_state_count);
#endif

	mach_port_deallocate(task_self, crashed_thread);

	if (ret1 != KERN_SUCCESS || ret2 != KERN_SUCCESS)
	{
		fprintf(stderr, "opera [crash logging]: Error getting thread state!\n");
		fclose(log_file);
		return;
	}

	#define float_reg(x) offsetof(CONTEXT, fpregs.x)
	#define cpu_reg(x) offsetof(CONTEXT, regs.x)
#if TARGET_CPU_X86
	static unsigned char register_offsets[] =
	{
		cpu_reg(__eax), cpu_reg(__ebx), cpu_reg(__ecx), cpu_reg(__edx), cpu_reg(__esi),
		cpu_reg(__edi), cpu_reg(__ebp), cpu_reg(__esp), cpu_reg(__eip), cpu_reg(__eflags),
		cpu_reg(__cs), cpu_reg(__ds), cpu_reg(__ss), cpu_reg(__es), cpu_reg(__fs), cpu_reg(__gs),

		(float_reg(__fpu_stmm7)+8) | 1, float_reg(__fpu_stmm7)+4, float_reg(__fpu_stmm7),
		(float_reg(__fpu_stmm6)+8) | 1, float_reg(__fpu_stmm6)+4, float_reg(__fpu_stmm6),
		(float_reg(__fpu_stmm5)+8) | 1, float_reg(__fpu_stmm5)+4, float_reg(__fpu_stmm5),
		(float_reg(__fpu_stmm4)+8) | 1, float_reg(__fpu_stmm4)+4, float_reg(__fpu_stmm4),
		(float_reg(__fpu_stmm3)+8) | 1, float_reg(__fpu_stmm3)+4, float_reg(__fpu_stmm3),
		(float_reg(__fpu_stmm2)+8) | 1, float_reg(__fpu_stmm2)+4, float_reg(__fpu_stmm2),
		(float_reg(__fpu_stmm1)+8) | 1, float_reg(__fpu_stmm1)+4, float_reg(__fpu_stmm1),
		(float_reg(__fpu_stmm0)+8) | 1, float_reg(__fpu_stmm0)+4, float_reg(__fpu_stmm0),
		float_reg(__fpu_fsw) | 1, float_reg(__fpu_fcw) | 1
	};

	int args[ARRAY_SIZE(register_offsets) + 3];

	args[0] = exe_name ? (int)exe_name : (int)"<no name>";
	args[1] = (int)signame;
	args[2] = context.regs.__eip;

	for (UINT i=0; i<ARRAY_SIZE(register_offsets); i++)
	{
		UINT offset = register_offsets[i];
		BYTE *reg = (BYTE *)&context + (offset & ~1);
		UINT reg_value = (offset & 1) ? *(unsigned short*)reg : *(unsigned int *)reg;
		args[i+3] = reg_value;
		if (i <= 8)
			add_pointer(memdump_list, reg_value, context.regs.__esp, task, code_ranges);
	}

	static char header_txt[] =
		"%s got signal %s at address %08X\n\n"
		"Registers:\n"
		"EAX=%08X   EBX=%08X   ECX=%08X   EDX=%08X   ESI=%08X\n"
		"EDI=%08X   EBP=%08X   ESP=%08X   EIP=%08X FLAGS=%08X\n"
		"CS=%04X   DS=%04X   SS=%04X   ES=%04X   FS=%04X   GS=%04X\n"
		"FPU stack:\n"
		"%04X%08X%08X %04X%08X%08X %04X%08X%08X\n"
		"%04X%08X%08X %04X%08X%08X %04X%08X%08X\n"
		"%04X%08X%08X %04X%08X%08X SW=%04X CW=%04X\n\n"
		"Stack dump:\n";
	                                                                                                   // %s   %08X
	char *register_text = new char[op_strlen((char*)args[0]) + op_strlen(signame) + ARRAY_SIZE(header_txt)-4 + 27*4];
	if (!register_text)
		return;

	vsprintf(register_text, header_txt, (va_list)args);
#else
	static unsigned short register_offsets[] =
	{
		cpu_reg(__rax), cpu_reg(__rbx), cpu_reg(__rcx), cpu_reg(__rdx), cpu_reg(__rsi),
		cpu_reg(__rdi), cpu_reg(__r8), cpu_reg(__r9), cpu_reg(__r10), cpu_reg(__r11),
		cpu_reg(__r12), cpu_reg(__r13), cpu_reg(__r14), cpu_reg(__r15), cpu_reg(__rbp),
		cpu_reg(__rsp), cpu_reg(__rip), cpu_reg(__rflags) | 2,

		(float_reg(__fpu_stmm0)+ 8) | 1, float_reg(__fpu_stmm0),
		(float_reg(__fpu_stmm1)+ 8) | 1, float_reg(__fpu_stmm1),
		(float_reg(__fpu_stmm2)+ 8) | 1, float_reg(__fpu_stmm2),
		(float_reg(__fpu_stmm3)+ 8) | 1, float_reg(__fpu_stmm3),
		(float_reg(__fpu_stmm4)+ 8) | 1, float_reg(__fpu_stmm4),
		(float_reg(__fpu_stmm5)+ 8) | 1, float_reg(__fpu_stmm5),
		(float_reg(__fpu_stmm6)+ 8) | 1, float_reg(__fpu_stmm6),
		(float_reg(__fpu_stmm7)+ 8) | 1, float_reg(__fpu_stmm7),
		float_reg(__fpu_fsw) | 1, float_reg(__fpu_fcw) | 1,

		float_reg(__fpu_xmm0)+ 8, float_reg(__fpu_xmm0),
		float_reg(__fpu_xmm1)+ 8, float_reg(__fpu_xmm1),
		float_reg(__fpu_xmm2)+ 8, float_reg(__fpu_xmm2),
		float_reg(__fpu_xmm3)+ 8, float_reg(__fpu_xmm3),
		float_reg(__fpu_xmm4)+ 8, float_reg(__fpu_xmm4),
		float_reg(__fpu_xmm5)+ 8, float_reg(__fpu_xmm5),
		float_reg(__fpu_xmm6)+ 8, float_reg(__fpu_xmm6),
		float_reg(__fpu_xmm7)+ 8, float_reg(__fpu_xmm7),
		float_reg(__fpu_xmm8)+ 8, float_reg(__fpu_xmm8),
		float_reg(__fpu_xmm9)+ 8, float_reg(__fpu_xmm9),
		float_reg(__fpu_xmm10)+ 8, float_reg(__fpu_xmm10),
		float_reg(__fpu_xmm11)+ 8, float_reg(__fpu_xmm11),
		float_reg(__fpu_xmm12)+ 8, float_reg(__fpu_xmm12),
		float_reg(__fpu_xmm13)+ 8, float_reg(__fpu_xmm13),
		float_reg(__fpu_xmm14)+ 8, float_reg(__fpu_xmm14),
		float_reg(__fpu_xmm15)+ 8, float_reg(__fpu_xmm15),
	};

	long args[ARRAY_SIZE(register_offsets) + 3];

	args[0] = exe_name ? (long)exe_name : (long)"<no name>";
	args[1] = (long)signame;
	args[2] = context.regs.__rip;

	for (UINT i=0; i<ARRAY_SIZE(register_offsets); i++)
	{
		unsigned long offset = register_offsets[i];
		BYTE *reg = (BYTE *)&context + (offset & ~3);
		unsigned long reg_value = (offset & 1) ? *(unsigned short *)reg : 
								  (offset & 2) ? *(unsigned int *)reg : *(unsigned long *)reg;
		args[i+3] = reg_value;
		if (i <= 16)
			add_pointer(memdump_list, reg_value, context.regs.__rsp, task, code_ranges);
	}

	static char header_txt[] =
		"%s got signal %s at address %012lX\n\n"
		"Registers:\n"
		"RAX=%016lX   RBX=%016lX   RCX=%016lX\n"
		"RDX=%016lX   RSI=%016lX   RDI=%016lX\n"
		" R8=%016lX    R9=%016lX   R10=%016lX\n"
		"R11=%016lX   R12=%016lX   R13=%016lX\n"
		"R14=%016lX   R15=%016lX   RBP=%016lX\n"
		"RSP=%016lX   RIP=%016lX FLAGS=%08X\n"
		"FPU stack:\n"
		"st(0)=%04X%016lX st(1)=%04X%016lX st(2)=%04X%016lX\n"
		"st(3)=%04X%016lX st(4)=%04X%016lX st(5)=%04X%016lX\n"
		"st(6)=%04X%016lX st(7)=%04X%016lX SW=%04X CW=%04X\n"
 		" XMM0=%016lX%016lX  XMM1=%016lX%016lX\n"
 		" XMM2=%016lX%016lX  XMM3=%016lX%016lX\n"
 		" XMM4=%016lX%016lX  XMM5=%016lX%016lX\n"
 		" XMM6=%016lX%016lX  XMM7=%016lX%016lX\n"
 		" XMM8=%016lX%016lX  XMM9=%016lX%016lX\n"
		"XMM10=%016lX%016lX XMM11=%016lX%016lX\n"
		"XMM12=%016lX%016lX XMM13=%016lX%016lX\n"
		"XMM14=%016lX%016lX XMM15=%016lX%016lX\n\n"
		"Stack dump:\n";
															                                         // %s %08X %012lX %016lX
	char *register_text = new char[op_strlen((char*)args[0]) + op_strlen(signame) + ARRAY_SIZE(header_txt)-4 + 4 + 6 + 57*10];
	if (!register_text)
		return;

	vsprintf_var(register_text, header_txt, args);
#endif
	free(exe_name);

	if (fputs(register_text, log_file) == EOF)
	{
		fprintf(stderr, "opera [crash logging]: Crash log writing failed, error writing to file %s!\n", log_filename);
		fclose(log_file);
		return;
	}

#if TARGET_CPU_X86
	UINT stack_pos = context.regs.__esp & ~3;
	UINT frame_pointer = context.regs.__ebp;
#else
	unsigned long stack_pos = context.regs.__rsp & ~3;
	unsigned long frame_pointer = context.regs.__rbp;
#endif

	for (UINT stack_pages = 0; stack_pages < STACK_DUMP_SIZE / 0x1000; stack_pages++)
	{
		UINT read_size = 0x1000;
		vm_offset_t dst_adr;
		mach_msg_type_number_t dst_size;

		if (vm_read(task, stack_pos, read_size, &dst_adr, &dst_size) != KERN_SUCCESS)
		{
			read_size -= (stack_pos & 0xFFF);
			if (vm_read(task, stack_pos, read_size, &dst_adr, &dst_size) != KERN_SUCCESS)
				break;
		}
		BYTE *stack_buf_pos = (BYTE *)dst_adr;
		read_size = dst_size;

		do
		{
#if TARGET_CPU_X86
			fprintf(log_file, "%08X ", stack_pos);
#else
			fprintf(log_file, "%012lX ", stack_pos);
#endif

			UINT dump_len = read_size;
			BYTE *dump_stack_pos = stack_buf_pos;

#if TARGET_CPU_X86
			for (UINT stack_words = 0; stack_words < 4; stack_words++)
			{
				if (dump_len < 4)
					fputs("          ", log_file);
#else
			for (UINT stack_words = 0; stack_words < 2; stack_words++)
			{
				if (dump_len < 8)
					fputs("                  ", log_file);
#endif
				else
				{
					unsigned long stack_word = *(unsigned long *)dump_stack_pos;
#if TARGET_CPU_X86
					add_pointer(memdump_list, stack_word, context.regs.__esp, task, code_ranges);
#else
					add_pointer(memdump_list, stack_word, context.regs.__rsp, task, code_ranges);
#endif

					if (stack_pos == frame_pointer)
					{
						frame_pointer = stack_word;
						putc('>', log_file);
					}
					else
						putc(' ', log_file);

#if TARGET_CPU_X86
					fprintf(log_file, "%08lX ", stack_word);
					dump_stack_pos += 4;
					stack_pos += 4;
					dump_len -= 4;
#else
					fprintf(log_file, "%016lX ", stack_word);
					dump_stack_pos += 8;
					stack_pos += 8;
					dump_len -= 8;
#endif
				}
			}
			dump_chars(log_file, stack_buf_pos, read_size);

		} while (read_size);

		vm_deallocate(task_self, dst_adr, dst_size);
	}

	if (fputs("\nUsed Libraries:\n", log_file) == EOF)
	{
		fprintf(stderr, "opera [crash logging]: Crash log writing failed, error writing to file %s!\n", log_filename);
		fclose(log_file);
		return;
	}

	dll_line *current_dll = dlls.first;
	while (current_dll)
	{
		fputs(current_dll->line, log_file);
		current_dll = current_dll->next;
	}

	if (fputs("\nMemory dumps:", log_file) == EOF)
	{
		fprintf(stderr, "opera [crash logging]: Crash log writing failed, error writing to file %s!\n", log_filename);
		fclose(log_file);
		return;
	}

	byte_range *current_mem = memdump_list.first;
	while (current_mem)
	{
		unsigned long dump_start = current_mem->range_start;
		unsigned long dump_end = current_mem->range_end;

		int disposition, ref_count;

		if (vm_map_page_query(task, dump_start, &disposition, &ref_count) != KERN_SUCCESS ||
			!(disposition & VM_PAGE_QUERY_PAGE_PRESENT))
			dump_start = (dump_start | 0xFFF) + 1;

		if (vm_map_page_query(task, dump_end, &disposition, &ref_count) != KERN_SUCCESS ||
			!(disposition & VM_PAGE_QUERY_PAGE_PRESENT))
			dump_end = (dump_end & ~0xFFF);

		UINT read_size = dump_end - dump_start;

		vm_offset_t dst_adr;
		mach_msg_type_number_t dst_size = 0;

		vm_read(task, dump_start, read_size, &dst_adr, &dst_size);

		BYTE *mem_buf_pos = (BYTE *)dst_adr;
		read_size = dst_size;

		putc('\n', log_file);

		while (read_size)
		{
#if TARGET_CPU_X86
			fprintf(log_file, "%08lX", dump_start);
#else
			fprintf(log_file, "%012lX", dump_start);
#endif

			UINT dump_len = read_size;
			BYTE *dump_mem_pos = mem_buf_pos;

			for (UINT mem_bytes = 0; mem_bytes < 16; mem_bytes++)
			{
				putc(' ', log_file);
				if (!(mem_bytes & 3))
					putc(' ', log_file);

				if (!dump_len)
				{
					fputs("  ", log_file);
				}
				else
				{
					fprintf(log_file, "%02X", *dump_mem_pos);
					dump_mem_pos++;
					dump_start++;
					dump_len--;
				}
			}
			dump_chars(log_file, mem_buf_pos, read_size);
		}
		vm_deallocate(task_self, dst_adr, dst_size);
		current_mem = current_mem->next;
	}

	fclose(log_file);
/*
	char *first_line_end = register_text;
	do { first_line_end++; } while (*first_line_end != '\n');
	*first_line_end = 0;

	fprintf(stderr, "opera [crash logging]: CRASH!!\n%s\n\nLog was created here:\r\n%s\n", register_text, log_filename);

	delete[] register_text;
*/
#ifdef CRASHLOG_DEBUG
	errno = 0;
#endif
	ptrace(PT_KILL, pid, 0, 0);
#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "opera [crashlog debug %d]: killing crashed instance: %d\n", time(NULL), errno);
#endif
}

#endif // defined(_MACINTOSH_)
