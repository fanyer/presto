/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined( __FreeBSD__) && defined(i386)

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
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <machine/reg.h>
#include <dlfcn.h>

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

struct libutil
{
	typedef struct kinfo_vmentry* (*kinfo_getvmmap_t) (pid_t pid, int* cntp);
	libutil() 
	  : handle(dlopen("/usr/lib/libutil.so", RTLD_LAZY))
	  , kinfo_getvmmap(handle ? (kinfo_getvmmap_t)dlsym(handle, "kinfo_getvmmap") : NULL)
	{}
	~libutil() { if (handle) dlclose(handle); }
	void* handle;
	kinfo_getvmmap_t kinfo_getvmmap;
};

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
	ptrace(PT_READ_D, pid, (caddr_t)ptmp, 0);
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

void sighandler(int sig)
{
	if( g_crash_recovery_parameters )
	{
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
	char * gpuinfoaddr = reinterpret_cast<char*>(gpu_info);
	char * gpubuf = reinterpret_cast<char*>(ret_gpudata);
	for (unsigned int offs = 0; offs < sizeof(GpuInfo); offs += sizeof(int))
	{
		int val = ptrace(PT_READ_D, pid, gpuinfoaddr + offs, 0);
		unsigned int tocopy = sizeof(GpuInfo) - offs;
		if (tocopy > sizeof(int))
			tocopy = sizeof(int);
		memcpy(gpubuf + offs, &val, tocopy);
	}
	return (ret_gpudata->initialized == GpuInfo::INITIALIZED && ret_gpudata->device_info_size <= GpuInfo::MAX_INFO_LENGTH);
}


void WriteCrashlog(pid_t pid, GpuInfo * gpu_info, char* log_filename, int log_filename_size, const char* location)
{
	int status;
	UINT i;
	const char *signame = NULL;

	ptrace(PT_ATTACH, pid, 0, 0);

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

		ptrace(PT_CONTINUE, pid, (caddr_t)1, 0);
	}

	libutil util;
	if (!util.kinfo_getvmmap)
	{
		fprintf(stderr, "opera [crash logging]: Can't load libutil.so\n");
		return;
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

#if BROWSER_BUILD_NUMBER_INT != 9999
	fputs("OPERA-CRASHLOG V1 desktop " VER_NUM_STR " " BROWSER_BUILD_NUMBER " unix FreeBSD-i386\n", log_file);
#endif

	char *exe_name = new char[PATH_MAX];
	if (!exe_name)
		return;
	exe_name[0] = '\0';

	int entry_count;
	struct kinfo_vmentry* entries = util.kinfo_getvmmap(pid, &entry_count);
	for (int i = 0; i < entry_count; i++)
	{
		if (!*exe_name)
			op_strlcpy(exe_name, entries[i].kve_path, PATH_MAX);

		unsigned int line_len = 23;
		if (*entries[i].kve_path)
			line_len += op_strlen(entries[i].kve_path)+5;

		char *line_text = new char[line_len];
		if (line_text)
		{
			sprintf(line_text, "%08X-%08X %c%c%c%s%s\n", (uint32_t)entries[i].kve_start, (uint32_t)entries[i].kve_end,
									(entries[i].kve_protection & KVME_PROT_READ) ?'r':'-',
									(entries[i].kve_protection & KVME_PROT_WRITE)?'w':'-',
									(entries[i].kve_protection & KVME_PROT_EXEC)?'x':'-',
									*entries[i].kve_path?"     ":"",
									entries[i].kve_path);

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

		if ((entries[i].kve_protection & KVME_PROT_EXEC) && !(entries[i].kve_protection & KVME_PROT_WRITE))
		{
			byte_range *range = new byte_range;
			if (range)
			{
				range->range_start = entries[i].kve_start;
				range->range_end = entries[i].kve_end;
				range->next = code_ranges.first;
				code_ranges.first = range;
			}
		}
	}
	free(entries);

	struct CONTEXT
	{
		struct fpreg fpregs;
		struct reg regs;
	} context;

	ptrace(PT_GETREGS, pid, (caddr_t)&context.regs, 0);
	ptrace(PT_GETFPREGS, pid, (caddr_t)&context.fpregs, 0);

	#define float_reg(x) offsetof(CONTEXT, fpregs.x)
	#define cpu_reg(x) offsetof(CONTEXT, regs.x)
	static unsigned char register_offsets[] =
	{
		cpu_reg(r_eax), cpu_reg(r_ebx), cpu_reg(r_ecx), cpu_reg(r_edx), cpu_reg(r_esi),
		cpu_reg(r_edi), cpu_reg(r_ebp), cpu_reg(r_esp), cpu_reg(r_eip), cpu_reg(r_eflags),
		cpu_reg(r_cs), cpu_reg(r_ds), cpu_reg(r_ss), cpu_reg(r_es), cpu_reg(r_fs), cpu_reg(r_gs),

		(float_reg(fpr_acc)+78) | 1, float_reg(fpr_acc)+74, float_reg(fpr_acc)+70,
		(float_reg(fpr_acc)+68) | 1, float_reg(fpr_acc)+64, float_reg(fpr_acc)+60,
		(float_reg(fpr_acc)+58) | 1, float_reg(fpr_acc)+54, float_reg(fpr_acc)+50,
		(float_reg(fpr_acc)+48) | 1, float_reg(fpr_acc)+44, float_reg(fpr_acc)+40,
		(float_reg(fpr_acc)+38) | 1, float_reg(fpr_acc)+34, float_reg(fpr_acc)+30,
		(float_reg(fpr_acc)+28) | 1, float_reg(fpr_acc)+24, float_reg(fpr_acc)+20,
		(float_reg(fpr_acc)+18) | 1, float_reg(fpr_acc)+14, float_reg(fpr_acc)+10,
		(float_reg(fpr_acc)+ 8) | 1, float_reg(fpr_acc)+4,  float_reg(fpr_acc),
		float_reg(fpr_env[1]) | 1, float_reg(fpr_env[0]) | 1
	};

	int args[sizeof(register_offsets) + 3];

	args[0] = exe_name ? (int)exe_name : (int)"<no name>";
	args[1] = (int)signame;
	args[2] = context.regs.r_eip;

	for (UINT i=0; i<sizeof(register_offsets); i++)
	{
		UINT offset = register_offsets[i];
		BYTE *reg = (BYTE *)&context + (offset & ~1);
		UINT reg_value = (offset & 1) ? *(unsigned short*)reg : *(unsigned int *)reg;
		args[i+3] = reg_value;
		if (i <= 8)
			add_pointer(memdump_list, reg_value, context.regs.r_esp, pid, code_ranges);
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
	delete[] exe_name;

	if (fputs(register_text, log_file) == EOF)
	{
		fprintf(stderr, "opera [crash logging]: Crash log writing failed, error writing to file %s!", log_filename);
		fclose(log_file);
		return;
	}

	UINT stack_pos = context.regs.r_esp & ~3;
	UINT frame_pointer = context.regs.r_ebp;

	char buffer[16];

	for (i=0; i<STACK_DUMP_SIZE/16; i++)
	{
		UINT b, bufsize = 0;
		for (b=0; b<16; b+=4, stack_pos+=4)
		{
			UINT value;
			errno = 0;
			*(UINT *)(buffer+b) = value = ptrace(PT_READ_D, pid, (caddr_t)stack_pos, 0);
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
				add_pointer(memdump_list, value, context.regs.r_esp, pid, code_ranges);
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
		ptrace(PT_READ_D, pid, (caddr_t)dump_start, 0);
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
				*(UINT *)(buffer+b) = value = ptrace(PT_READ_D, pid, (caddr_t)dump_start, 0);
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

#endif // defined( __FreeBSD__) && defined(i386)
