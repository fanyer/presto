/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(__linux__) && defined(i386)

#include "platforms/crashlog/crashlog.h"
#include "platforms/crashlog/gpu_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#ifndef PR_SET_PTRACER
// Introduced by ubuntu 11.04 (I think)
#define PR_SET_PTRACER 0x59616d61
#endif
#include <sys/user.h>
#include <time.h>

#include "adjunct/quick/quick-version.h"

#define STACK_DUMP_SIZE 0x2000
#define DATA_DUMP_SIZE 0x400
#define CODE_DUMP_SIZE 0x10

UINT g_wait_for_debugger = 0;
char** g_crash_recovery_parameters = 0;


struct _signal
{
	int sig_num;
	struct sigaction* old_action;
	const char *signame;
} signals[] =
{
	{SIGSEGV, 0, "SIGSEGV"},
	{SIGILL,  0, "SIGILL"},
	{SIGFPE,  0, "SIGFPE"},
	{SIGABRT, 0, "SIGABRT"},
//	{SIGPIPE, 0, "SIGPIPE"},
	{SIGTRAP, 0, "SIGTRAP"}
};


struct byte_range
{
	UINT range_start;
	UINT range_end;
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
	~dll_line() { free(line); }
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

int get_exeName(pid_t pid, char *exe_name)
{
	char path[32];
	sprintf(path, "/proc/%d/exe", pid);
	*exe_name = 0;
	int len = readlink(path, exe_name, PATH_MAX-1);
	if (len != -1)
		exe_name[len] = 0;

	return len;
}

void dump_chars(FILE *log_file, char *buffer, UINT word_count)
{
	putc(' ', log_file);
	while (word_count--)
	{
		BYTE mem_byte = *buffer++;
		putc(mem_byte && (mem_byte < 9 || mem_byte > 14) ? mem_byte : '.', log_file);
	}
	putc('\n', log_file);
}

void add_pointer(byte_range_list &memdump_list, UINT pointer, UINT stack_pointer, pid_t pid, byte_range_list &code_ranges)
{
	// don't dump data that's in the stackdump anyway
	if (pointer >= stack_pointer && pointer < stack_pointer+STACK_DUMP_SIZE)
		return;

	// peekdata fails when the peeked word straddles the boundary to an invalid page,
	// even if the pointer is valid, so make sure it doesn't
	unsigned long ptmp = pointer & ~3;
	errno = 0;
	ptrace(PTRACE_PEEKDATA, pid, ptmp, 0);
	if (errno)
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

	UINT dump_start = pointer - dump_len;
	UINT dump_end = pointer + dump_len;

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


void sighandler(int sig);

void InstallCrashSignalHandler()
{
	struct sigaction action;
	action.sa_handler = sighandler;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);

	for (unsigned int i=0; i<ARRAY_SIZE(signals); i++)
	{
		if( signals[i].old_action )
			free(signals[i].old_action);
		signals[i].old_action = (struct sigaction*)malloc(sizeof(struct sigaction));
		sigaction(signals[i].sig_num, &action, signals[i].old_action );
	}
}


void RemoveCrashSignalHandler()
{
	for (unsigned int i=0; i<ARRAY_SIZE(signals); i++)
	{
		if( signals[i].old_action )
		{
			sigaction(signals[i].sig_num, signals[i].old_action, 0); 
			free(signals[i].old_action);
			signals[i].old_action = 0;
		}
		else
		{
			struct sigaction action;
			action.sa_handler = SIG_DFL;
			action.sa_flags = 0;
			sigaction(signals[i].sig_num, &action, 0 );
		}
	}
}

bool g_sighandler_crashloggerpid_installed = false;

void sighandler_crashloggerpid(int sig, siginfo_t * siginfo, void * data)
{
	pid_t crashlogger_pid = ((union sigval)siginfo->si_value).sival_int;
	prctl(PR_SET_PTRACER, crashlogger_pid, 0, 0, 0);
};


void sighandler(int sig)
{
	if( g_crash_recovery_parameters )
	{
		if (!g_sighandler_crashloggerpid_installed)
		{
			/* Install this sighandler after the crash, just in case
			 * the chosen signal is in use by something else.
			 */
			struct sigaction sa;
			sa.sa_flags = SA_SIGINFO;
			sa.sa_sigaction = sighandler_crashloggerpid;
			sigaction(SIGUSR2, &sa, 0);
			g_sighandler_crashloggerpid_installed = true;
		};
		pid_t opera_pid = getpid();
		while (g_wait_for_debugger <= 50)
		{
			if (!g_wait_for_debugger)
			{
				pid_t fork_pid = fork();
				if (fork_pid == -1)
				{
					break;
				}
				else if (!fork_pid)
				{
					// child process
					fork_pid = fork();
					if (!fork_pid)
					{
						sigval sv;
						sv.sival_int = getpid();
						sigqueue(opera_pid, SIGUSR2, sv);

						// detached grandchild process
						// give the intermediate child time to exit
						usleep(10000);
						
						// The system will by default disable new signals when in the signal handler.
						sigset_t set;
						sigemptyset(&set);
						sigaddset(&set, sig);
						sigprocmask(SIG_UNBLOCK, &set, 0);

						// now we can debug the original process						
						execv(g_crash_recovery_parameters[0], g_crash_recovery_parameters);
					}
					_exit(0);
					
				}
				else
				{
					// need to wait for the intermediate child
					// or else it becomes a zombie
					int stat_loc;
					waitpid(fork_pid, &stat_loc, 0);
				}
			}

			g_wait_for_debugger++;
			usleep(200000);
			return;
		}
	}

	// log writing failed, restore default signal handling
	RemoveCrashSignalHandler();
}

/* Fill 'ret_gpudata' with the gpu info from the crashed process.
 * Returns true if successful, false if not.  On failure, the content
 * of 'ret_gpudata' is undefined.
 */
bool GetGpuInfo(pid_t pid, GpuInfo * gpu_info, GpuInfo * ret_gpudata)
{
	UINT8 * gpuinfoaddr = reinterpret_cast<UINT8*>(gpu_info);
	UINT8 * gpubuf = reinterpret_cast<UINT8*>(ret_gpudata);
	for (unsigned int offs = 0; offs < sizeof(GpuInfo); offs += sizeof(long))
	{
		long val = ptrace(PTRACE_PEEKDATA, pid, gpuinfoaddr + offs, 0);
		unsigned int tocopy = sizeof(GpuInfo) - offs;
		if (tocopy > sizeof(long))
			tocopy = sizeof(long);
		memcpy(gpubuf + offs, &val, tocopy);
	}
	return (ret_gpudata->initialized == GpuInfo::INITIALIZED && ret_gpudata->device_info_size <= GpuInfo::MAX_INFO_LENGTH);
}

void WriteCrashlog(pid_t pid, GpuInfo * gpu_info, char* log_filename, int log_filename_size, const char* location)
{
	int status;
	UINT i;
	const char *signame = NULL;

	int wait_for_access = 100;
	while (ptrace(PT_ATTACH, pid, 0, 0) != 0 && errno == EPERM && wait_for_access > 0)
	{
		wait_for_access --;
		usleep(100000);
	};

	while (1) {
		waitpid(pid, &status, 0);

		if (!WIFSTOPPED(status))
		{
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

		ptrace(PTRACE_CONT, pid, 0, 0);
	}

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

	char *textbuf = new char[PATH_MAX];
	if (!textbuf)
		return;

#if BROWSER_BUILD_NUMBER_INT != 9999
	fputs("OPERA-CRASHLOG V1 desktop " VER_NUM_STR " " BROWSER_BUILD_NUMBER " unix Linux-i386\n", log_file);
#endif

	sprintf(textbuf, "/proc/%d/maps", pid);
	FILE *procmap = fopen(textbuf, "rt");
	if (procmap)
	{
		while (!feof(procmap)) {
			UINT libstart, libend;
			char write_prot, exec_prot;
			if (!fgets(textbuf, 512, procmap))
				break;
			if (sscanf(textbuf, "%X-%X %*c%c%c", &libstart, &libend, &write_prot, &exec_prot) == 4 &&
				exec_prot == 'x' && write_prot != 'w')
			{
				byte_range *range = new byte_range;
				if (range)
				{
					range->range_start = libstart;
					range->range_end = libend;
					range->next = code_ranges.first;
					code_ranges.first = range;
				}
			}
			char *bufpos = textbuf;
			while (*bufpos)
			{
				if (*bufpos == '/')
				{
					dll_line *new_dll = new dll_line;
					if (new_dll)
					{
						new_dll->line = strdup(textbuf);

						if (!dlls.first)
							dlls.first = new_dll;
						else
							dlls.last->next = new_dll;

						dlls.last = new_dll;
					}
					break;
				}
				bufpos++;
			}
		}
		fclose(procmap);
	}

	get_exeName(pid, textbuf);

	struct CONTEXT
	{
		struct user_fpregs_struct fpregs;
		struct user_regs_struct regs;
	} context;

	ptrace(PTRACE_GETREGS, pid, 0, &context.regs);
	ptrace(PTRACE_GETFPREGS, pid, 0, &context.fpregs);

	#define float_reg(x) offsetof(CONTEXT, fpregs.x)
	#define cpu_reg(x) offsetof(CONTEXT, regs.x)
	static unsigned char register_offsets[] =
	{
		cpu_reg(eax), cpu_reg(ebx), cpu_reg(ecx), cpu_reg(edx), cpu_reg(esi),
		cpu_reg(edi), cpu_reg(ebp), cpu_reg(esp), cpu_reg(eip), cpu_reg(eflags),
		cpu_reg(xcs), cpu_reg(xds), cpu_reg(xss), cpu_reg(xes), cpu_reg(xfs), cpu_reg(xgs),

		(float_reg(st_space)+78) | 1, float_reg(st_space)+74, float_reg(st_space)+70,
		(float_reg(st_space)+68) | 1, float_reg(st_space)+64, float_reg(st_space)+60,
		(float_reg(st_space)+58) | 1, float_reg(st_space)+54, float_reg(st_space)+50,
		(float_reg(st_space)+48) | 1, float_reg(st_space)+44, float_reg(st_space)+40,
		(float_reg(st_space)+38) | 1, float_reg(st_space)+34, float_reg(st_space)+30,
		(float_reg(st_space)+28) | 1, float_reg(st_space)+24, float_reg(st_space)+20,
		(float_reg(st_space)+18) | 1, float_reg(st_space)+14, float_reg(st_space)+10,
		(float_reg(st_space)+ 8) | 1, float_reg(st_space)+4,  float_reg(st_space),
		float_reg(swd) | 1, float_reg(cwd) | 1
	};

	int args[sizeof(register_offsets) + 3];

	args[0] = *textbuf ? (int)textbuf : (int)"<no name>";
	args[1] = (int)signame;
	args[2] = context.regs.eip;

	for (UINT i=0; i<sizeof(register_offsets); i++)
	{
		UINT offset = register_offsets[i];
		BYTE *reg = (BYTE *)&context + (offset & ~1);
		UINT reg_value = (offset & 1) ? *(unsigned short*)reg : *(unsigned int *)reg;
		args[i+3] = reg_value;
		if (i <= 8)
			add_pointer(memdump_list, reg_value, context.regs.esp, pid, code_ranges);
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
	char *register_text = new char[strlen((char*)args[0]) + strlen(signame) + ARRAY_SIZE(header_txt)-4 + 27*4];
	if (!register_text)
		return;

	vsprintf(register_text, header_txt, (va_list)args);
	if (fputs(register_text, log_file) == EOF)
	{
		fprintf(stderr, "opera [crash logging]: Crash log writing failed, error writing to file %s!", log_filename);
		fclose(log_file);
		return;
	}

	delete[] textbuf;

	UINT stack_pos = context.regs.esp & ~3;
	UINT frame_pointer = context.regs.ebp;

	char buffer[16];

	for (i=0; i<STACK_DUMP_SIZE/16; i++)
	{
		UINT b, bufsize = 0;
		for (b=0; b<16; b+=4, stack_pos+=4)
		{
			UINT value;
			errno = 0;
			*(UINT *)(buffer+b) = value = ptrace(PTRACE_PEEKDATA, pid, stack_pos, 0);
			if (errno)
			{
				if (!b)
					break;
				fputs("          ", log_file);
			}
			else
			{
				if (!b)
					fprintf(log_file, "%08X ", stack_pos);

				if (stack_pos == frame_pointer)
				{
					frame_pointer = value;
					putc('>', log_file);
				}
				else
				{
					putc(' ', log_file);
				}

				fprintf(log_file, "%08X ", value);
				add_pointer(memdump_list, value, context.regs.esp, pid, code_ranges);
				bufsize += 4;
			}
		}
		dump_chars(log_file, buffer, bufsize);
		if (bufsize != 16)
			break;
	}

	if (fputs("\nUsed Libraries:\n", log_file) == EOF)
	{
		fprintf(stderr, "opera [crash logging]: Crash log writing failed, error writing to file %s!", log_filename);
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
		fprintf(stderr, "opera [crash logging]: Crash log writing failed, error writing to file %s!", log_filename);
		fclose(log_file);
		return;
	}

	byte_range *current_mem = memdump_list.first;
	while (current_mem)
	{
		UINT dump_start = current_mem->range_start;
		UINT dump_end = current_mem->range_end;

		errno = 0;
		ptrace(PTRACE_PEEKDATA, pid, dump_start, 0);
		if (errno)
			dump_start = (dump_start | 0xFFF) + 1;

		putc('\n', log_file);

		while (dump_start < dump_end)
		{
			UINT b,c, bufsize = 0;
			for (b=0; b<16; b+=4, dump_start+=4)
			{
				UINT value;
				errno = 0;
				*(UINT *)(buffer+b) = value = ptrace(PTRACE_PEEKDATA, pid, dump_start, 0);
				if (errno)
				{
					if (!b)
						break;
					fputs("             ", log_file);
				}
				else
				{
					if (!b)
						fprintf(log_file, "%08X  ", dump_start);

					for (c=0; c<4; c++)
					{
						fprintf(log_file, "%02X ", (BYTE)value);
						value >>= 8;
					}
					putc(' ', log_file);
					bufsize += 4;
				}
			}
			dump_chars(log_file, buffer, bufsize);
			if (bufsize != 16)
				break;
		}
		current_mem = current_mem->next;
	}

	fclose(log_file);

	// The main crashlog written, now for the gpu info
	if (gpu_info)
	{
		GpuInfo gpudata;
		if (GetGpuInfo(pid, gpu_info, &gpudata))
		{
			char * log_gpuinfotxt = new char[log_dir_length + 14];
			if (log_gpuinfotxt)
			{
				memcpy(log_gpuinfotxt, log_filename, log_dir_length);
				memcpy(log_gpuinfotxt + log_dir_length, "/gpu_info.txt", 14); // Includes the terminating NUL
				FILE *gpu_file = fopen(log_gpuinfotxt, "wt");
				delete[] log_gpuinfotxt;
				if (!gpu_file)
					fprintf(stderr, "opera [crash logging]: error writing gpu info to file %s/gpu_info.txt\n!", log_filename);
				else
					fwrite(gpudata.device_info, 1, gpudata.device_info_size, gpu_file);
				fclose(gpu_file);
			}
		}
	}

	char *first_line_end = register_text;
	do { first_line_end++; } while (*first_line_end != '\n');
	*first_line_end = 0;

	fprintf(stderr, "opera [crash logging]: CRASH!!\n%s\n\nLog was created here:\r\n%s\n", register_text, log_filename);

	delete[] register_text;

	ptrace(PT_KILL, pid, 0, 0);
	waitpid(pid, &status, 0);
}

#endif // defined(__linux__) && defined(i386)
