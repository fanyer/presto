/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(MSWIN) && (defined(DESKTOP_STARTER) || defined(PLUGIN_WRAPPER))

#include "platforms/crashlog/crashlog.h"
#include "platforms/windows_common/utils/OpAutoHandle.h"

extern "C"
{
#include "adjunct/quick/quick-version.h"
#include "platforms/crashlog/gpu_info.h"
#include "platforms/windows/windows_ui/res/#BuildNr.rci"
#pragma warning(disable:4740)	// flow in or out of inline asm code suppresses global optimization

void CloseMemGuardLog();
void WriteShortStacks();
BOOL CheckMemGuardException(struct _EXCEPTION_POINTERS *);

char * __stdcall str_begins_with(char *str1, char *str2)
{
	while (*str2)
	{
		if (*str1 != *str2)
			return NULL;

		str1++;
		str2++;
	}
	return str1;
}

HANDLE CreateCrashlogEvent(DWORD process_id, bool must_exist)
{
	char name[30];
	wsprintfA(name, "OperaCrashlogEvent-%u", process_id);
	HANDLE h = CreateEventA(NULL, TRUE, FALSE, name);
	if (h && must_exist && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		CloseHandle(h);
		h = NULL;
	}
	return h;
}


LPSTR g_CmdLine = 0;
LPSTR g_crashlog_folder = 0;
GpuInfo g_gpu_info;
volatile LONG g_crash_thread_id = 0; // id of first crashing thread

LONG WINAPI ExceptionFilter(struct _EXCEPTION_POINTERS *exc)
{
	char operaexe[MAX_PATH], command_line[1024];

	if (!g_crash_thread_id && CheckMemGuardException(exc))
		return EXCEPTION_CONTINUE_EXECUTION;

	LONG thread_id = GetCurrentThreadId();

	if (InterlockedCompareExchange(&g_crash_thread_id, thread_id, 0) == 0)
	{
		// first crash - start crashlog process

		CloseMemGuardLog();
		WriteShortStacks();

		LPSTR orig_cmdline = g_CmdLine;
		char no_restart = ' ';
		char *skip_crashlog = str_begins_with(g_CmdLine, "-crashlog \"");
		if (skip_crashlog)
		{
			// this is already a restarted instance, cut away the old crashlog argument
			char *crashlog_path = command_line;
			
			while (*skip_crashlog)
			{
				char ch = *skip_crashlog++;
				if (ch == '"')
					break;
				*crashlog_path++ = ch;
			}
			*crashlog_path = 0;

			if (*skip_crashlog == ' ')
				skip_crashlog++;

			orig_cmdline = skip_crashlog;

			if (GetFileAttributesA(command_line) != 0xFFFFFFFF)
			{
				// the crash log still exists, so the restarted instance must have crashed
				// before it could even show the crash dialogue
				no_restart = '!';
			}
		}

		DWORD process_id = GetCurrentProcessId();

		// named event which will be signalled by crash logging subprocess after it is attached as a debugger
		// must be created before the subprocess
		OpAutoHANDLE wait_event(CreateCrashlogEvent(process_id, false));
		if (!wait_event.get())
			return EXCEPTION_CONTINUE_SEARCH;

		char *dest = command_line;

		// If this is a normal opera process crash, we add the commandline parameters that look like this:
		//
		// -write_crashlog 4A04 488A672
		//
		// where:
		// 4A04 is the <pid> in hex
		// 488A672 is the <address to the gpu_info>
		//	
		UINT param_len = wsprintfA(dest, "-write_crashlog%c%x %p %s", no_restart, process_id, &g_gpu_info, g_crashlog_folder?g_crashlog_folder:"");

		if (!param_len)
			return EXCEPTION_CONTINUE_SEARCH;

		dest += param_len;

		if (*orig_cmdline && !g_crashlog_folder)
		{
			*dest++ = ' ';
			do
			{
				*dest++ = *orig_cmdline++;
				param_len++;
			}
			while (*orig_cmdline && param_len < ARRAY_SIZE(command_line)-2);
		}
	
		*dest = 0;

		if (!GetModuleFileNameA(NULL, operaexe, MAX_PATH) ||
			ShellExecuteA(0, NULL, operaexe, command_line, NULL, SW_NORMAL) <= (HINSTANCE)32)
			return EXCEPTION_CONTINUE_SEARCH;

		// crash logging process has 10 seconds to attach itself as a debugger
		if (WaitForSingleObject(wait_event.get(), 10000) != WAIT_OBJECT_0)
			return EXCEPTION_CONTINUE_SEARCH;

		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if (g_crash_thread_id != thread_id)
	{
		// another thread crashed - just suspend it, crashlog will be created only for the first thread
		SuspendThread(GetCurrentThread());
	}

	// log writing failed, pass exception to standard handler/debugger
	return EXCEPTION_CONTINUE_SEARCH;
}


#define STACK_DUMP_SIZE 0x2000
#define DATA_DUMP_SIZE 0x400
#define CODE_DUMP_SIZE 0x10

struct thread_map
{
	~thread_map() { delete next; }
	DWORD thread_id;
	HANDLE thread_handle;
	thread_map *next;
};

struct byte_range
{
	ULONG_PTR range_start;
	ULONG_PTR range_end;
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

class termination_info
{
	public:
	termination_info(char *log_fn) : register_text(NULL), mem_buffer(NULL), dump_buffer(NULL),
									 log_file(NULL), log_filename(log_fn), error_msg(NULL), is_fileerr(FALSE) {}
	~termination_info()
	{
		delete[] register_text;
		delete[] mem_buffer;
		delete[] dump_buffer;

		if (error_msg)
		{
			if (is_fileerr)
			{
				char *err_msg = new char[strlen(error_msg)+strlen(log_filename)+2];
				if (err_msg)
				{
					wsprintfA(err_msg, "%s %s", error_msg, log_filename);
					error_msg = err_msg;
				}
				else
					is_fileerr = FALSE;
			}

			LPSTR lpMsgBuf;
			DWORD messSize = FormatMessageA( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPSTR) &lpMsgBuf,
				0,
				NULL 
			);

			static char msg_format[] = "Crash log writing failed, %s!\nError description from system: %s";
			char *msg_buf = new char[strlen(error_msg) + messSize + ARRAY_SIZE(msg_format)-4];
			if (msg_buf)
			{
				wsprintfA(msg_buf, msg_format, error_msg, lpMsgBuf);
				MessageBoxA(0, msg_buf, "Opera Crash Logging", 0);
				delete[] msg_buf;
			}

			if (is_fileerr)
				delete[] error_msg;

			LocalFree(lpMsgBuf);
		}
		if (log_file)
			CloseHandle(log_file);
	}

	BOOL write_log(void *buffer, UINT length)
	{
		DWORD bytes_written;
		if (!WriteFile(log_file, buffer, length, &bytes_written, 0))
		{
			is_fileerr = TRUE;
			error_msg = "error writing to file";
			return FALSE;
		}
		return TRUE;
	}

	char *register_text;
	BYTE *mem_buffer;
	char *dump_buffer;

	HANDLE log_file;
	char *log_filename;
	char *error_msg;
	BOOL is_fileerr;
};

BYTE * __stdcall convert_rva(BYTE *file_view, BYTE *pe_header, UINT rva)
{
	UINT section_count = *(unsigned short *)(pe_header+6);
	UINT header_size = *(unsigned short *)(pe_header+0x14);
	pe_header += header_size + 0x18 + 0xc;
	while (section_count--)
	{
		UINT section_start = *(UINT *)pe_header;
		if (rva >= section_start && rva < section_start + *(UINT *)(pe_header+4))
			return file_view + rva - section_start + *(UINT *)(pe_header+8);

		pe_header += 0x28;
	}
	return NULL;
}


BYTE * __stdcall find_version_resource(BYTE *resources)
{
	UINT resource_level = 0;
	BYTE *res_ptr = resources;
	while (1)
	{
		UINT ID_entry_count = *(unsigned short*)(res_ptr+0xE);
		if (!ID_entry_count)
			break;

		UINT name_entry_count = *(unsigned short*)(res_ptr+0xC);
		res_ptr += + name_entry_count*8 + 0x10;
		while (1)
		{
			UINT entry_rva_and_flag = *(UINT *)(res_ptr+4);
			UINT entry_rva = entry_rva_and_flag & 0x7FFFFFFF;

			if (resource_level >= 2)
			{
				if (entry_rva_and_flag == entry_rva)
					return resources + entry_rva;
			}
			else if ((resource_level == 0 && *(UINT *)res_ptr == (UINT)RT_VERSION) ||
					 (resource_level == 1 && *(UINT *)res_ptr == 1))
			{
				if (entry_rva_and_flag == entry_rva)
					return resources + entry_rva;
				else
				{
					res_ptr = resources + entry_rva;
					resource_level++;
					break;
				}
			}
			res_ptr +=8;
			ID_entry_count--;
			if (!ID_entry_count)
				return 0;
		}
	}
	return 0;
}


UINT __stdcall ver_get_value(BYTE *version_info, UINT version_info_size, char *ver_key, char *&dest, UINT &dest_len, INT version_align)
{
	while (1)
	{
		char *compare_pos = ver_key;
		while (1)
		{
			if (!version_info_size)
				return version_align;

			char current = *version_info++ & 0xDF;
			if (--version_info_size)
			{
				if (!*version_info)	// unicode
				{
					version_info++;
					version_info_size--;
				}
			}
			if (*compare_pos != current)
				break;

			if (!current)
			{
				while (version_info_size && !*version_info)
				{
					version_info++;
					version_info_size--;
				}
				while (version_info_size && *version_info && dest_len)
				{
					*dest++ = *version_info++;
					version_align--;
					if (--version_info_size)
					{
						if (!*version_info)	// unicode
						{
							version_info++;
							version_info_size--;
						}
					}
					dest_len--;
				}
				return version_align;
			}
			compare_pos++;
		}
	}
}


char * __stdcall get_range_and_name(void *image_name, WORD image_name_is_unicode, HANDLE file_handle, ULONG_PTR image_base, INT ver_align, byte_range_list &code_ranges)
{
	BOOL have_image_name;
	char line_buf[MAX_PATH], *line_pos = line_buf;
	UINT line_len = MAX_PATH;
	INT version_align = ver_align;

	if (!IsBadReadPtr(image_name, sizeof(LPVOID)) && !IsBadReadPtr(*(void **)image_name, sizeof(LPVOID)))
	{
		have_image_name = TRUE;
		if (image_name_is_unicode)
		{
			uni_char *fullname = *(uni_char **)image_name;
			while (*fullname && line_len) { *line_pos++ = (char)*fullname++; version_align--; line_len--; }
		}
		else
		{
			char *fullname = *(char **)image_name;
			while (*fullname && line_len) { *line_pos++ = *fullname++; version_align--; line_len--; }
		}
	}
	else
		have_image_name = FALSE;

	HANDLE mapping = CreateFileMappingA(file_handle, 0, PAGE_READONLY, 0, 0, 0);
	if (mapping)
	{
		BYTE *file_view = (BYTE *)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
		if (file_view)
		{
			BYTE *file_pos = file_view;
			if (*(short *)file_pos == 'ZM')
			{
				file_pos += *(UINT *)(file_pos+0x3c);
				if (*(short *)file_pos == 'EP')
				{
					BYTE *pe_header = file_pos;
					byte_range *range = new byte_range;
					if (range)
					{
						range->range_start = image_base;
						range->range_end = image_base + *(UINT *)(pe_header+0x2c) + *(UINT *)(pe_header+0x1c);
						range->next = code_ranges.first;
						code_ranges.first = range;
					}
#ifdef _WIN64
					UINT resource_rva = *(UINT *)(pe_header+0x98);
#else
					UINT resource_rva = *(UINT *)(pe_header+0x88);
#endif
					BYTE *version_info = NULL;
					UINT version_info_size = 0;

					if (resource_rva)
					{
						BYTE *resource = convert_rva(file_view, pe_header, resource_rva);
						if (resource)
						{
							resource = find_version_resource(resource);
							if (resource)
							{
								version_info_size = *(UINT *)(resource+4);
								version_info = convert_rva(file_view, pe_header, *(UINT *)resource);
							}
						}
					}
					if (!have_image_name)
					{
						if (version_align)
							version_align = 18;
						do
						{
#ifdef _WIN64
							UINT exports_rva = *(UINT *)(pe_header+0x88);
#else
							UINT exports_rva = *(UINT *)(pe_header+0x78);
#endif
							if (exports_rva)
							{
								BYTE *exports = convert_rva(file_view, pe_header, exports_rva);
								if (exports)
								{
									UINT dll_name_rva = *(UINT *)(exports+0xC);
									BYTE *dll_name = convert_rva(file_view, pe_header, dll_name_rva);
									if (dll_name)
									{
										while (*dll_name && line_len) { *line_pos++ = *dll_name++; version_align--; line_len--; }
										break;
									}
								}
							}

							if (version_info_size)
								version_align = ver_get_value(version_info, version_info_size, "ORIGINALFILENAME", line_pos, line_len, version_align);

						} while (0);
					}
					if (version_info_size && line_len)
					{
						do { *line_pos++ = ' '; line_len--; } while (--version_align > 0 && line_len);
						ver_get_value(version_info, version_info_size, "FILEVERSION", line_pos, line_len, 0);
					}
				}
			}
			UnmapViewOfFile(file_view);
		}
		CloseHandle(mapping);
	}

	line_len = MAX_PATH - line_len;
	if (!line_len)
		return NULL;

	char *ret_str = new char[ver_align ? (line_len > 63 ? line_len + 18 : 81) : (line_len + 1)];

	if (ret_str)
	{
		if (ver_align)
		{
			*line_pos = 0;
#ifdef _WIN64
			sprintf(ret_str, "%-63s Base: %12I64X\r\n", line_buf, image_base);
#else
			wsprintfA(ret_str, "%-63s Base: %8X\r\n", line_buf, image_base);
#endif
		}
		else
		{
			line_pos = line_buf;
			char *str_pos = ret_str;
			while (line_len) { *str_pos++ = *line_pos++; line_len--; }
			*str_pos = 0;
		}
	}
	return ret_str;
}

char * __stdcall dump_chars(char *dump_pos, BYTE *&mem_buf_pos, UINT &read_size)
{
	*(short *)dump_pos = '  ';
	dump_pos += 2;

	for (UINT i = 0; i < 16; i++)
	{
		BYTE mem_byte = *mem_buf_pos++;

		*dump_pos++ = mem_byte && (mem_byte < 9 || mem_byte > 14) ? mem_byte : '.';

		if (--read_size == 0)
			break;
	}

	*(short *)dump_pos = 0x0A0D;
	dump_pos += 2;

	return dump_pos;
}

void __stdcall add_pointer(byte_range_list &memdump_list, ULONG_PTR pointer, ULONG_PTR stack_pointer, HANDLE process, byte_range_list &code_ranges)
{
	// don't dump reserved memory areas or data that's in the stackdump anyway
	if (pointer < 0x100000 || (pointer >= stack_pointer && pointer < stack_pointer+STACK_DUMP_SIZE))
		return;

	char dummy;
	if (!ReadProcessMemory(process, (LPVOID)pointer, &dummy, 1, 0))
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

	ULONG_PTR dump_start = pointer - dump_len;
	ULONG_PTR dump_end = pointer + dump_len;

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

bool GetGpuInfo(HANDLE process, GpuInfo *gpu_info_src, GpuInfo *gpu_info_dst)
{
	SIZE_T bytes_read = 0;
	
	if (!ReadProcessMemory(process, gpu_info_src, gpu_info_dst, sizeof(GpuInfo), &bytes_read))
		return false;

	return (gpu_info_dst->initialized == GpuInfo::INITIALIZED && gpu_info_dst->device_info_size <= GpuInfo::MAX_INFO_LENGTH);
}

HANDLE __stdcall CreateLogFile(char *log_folder_path, char *basename, UINT *log_path_len)
{
	if (!log_folder_path)
		return INVALID_HANDLE_VALUE;

	char file_path[MAX_PATH] = {0};
	UINT path_len;
	
	if (!*log_folder_path)
	{
		SYSTEMTIME sys_time;
		GetLocalTime(&sys_time);
		
		char date_string[15];
		wsprintfA(date_string, "%04d%02d%02d%02d%02d%02d", sys_time.wYear, sys_time.wMonth, sys_time.wDay,
                                                           sys_time.wHour, sys_time.wMinute, sys_time.wSecond);

		UINT max_len = MAX_PATH - strlen(basename) - 21;
		path_len = GetTempPathA(max_len, log_folder_path);
        
		if (!path_len || path_len > max_len)
			return INVALID_HANDLE_VALUE;

		char *path_end = log_folder_path + path_len;

		wsprintfA(path_end, "opera-%s\\", date_string);
		
		if (!CreateDirectoryA(log_folder_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
			return INVALID_HANDLE_VALUE;
	}

	path_len = strlen(log_folder_path);
	size_t file_name_len = strlen(basename);

	if (path_len + file_name_len >= MAX_PATH)
		return INVALID_HANDLE_VALUE;

	memcpy(file_path, log_folder_path, path_len);
	memcpy(file_path + path_len, basename, file_name_len + 1);

	if (log_path_len)
		*log_path_len = path_len;

	return CreateFileA(file_path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
}

HANDLE __stdcall CreatePluginLogFile(char *log_filename, char *basename, char* log_folder, UINT *log_path_len)
{
	if (!log_folder)
		return INVALID_HANDLE_VALUE;

	SYSTEMTIME sys_time;
	GetLocalTime(&sys_time);

	char date_string[15];
	wsprintfA(date_string, "%04d%02d%02d%02d%02d%02d", sys_time.wYear, sys_time.wMonth, sys_time.wDay,
                                                       sys_time.wHour, sys_time.wMinute, sys_time.wSecond);

	UINT max_len = MAX_PATH - strlen(basename) - 13;		// basename contains "%s"
	UINT path_len;
	if (!(*log_folder))
	{
		path_len = GetTempPathA(max_len, log_filename);
		if (!path_len || path_len > max_len)
			return INVALID_HANDLE_VALUE;
	}
	else
	{
		path_len = strlen(log_folder);
		if (path_len > max_len)
			return INVALID_HANDLE_VALUE;

		//The parent folder of the log folder is the profile folder. We can safely assume that it already exists
		if(!CreateDirectoryA(log_folder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
			return INVALID_HANDLE_VALUE;

		strcpy(log_filename, log_folder);
	}

	char *path_end = log_filename + path_len;
	if (path_end[-1] != '\\')
		*path_end++ = '\\';

	path_len = path_end - log_filename;
	path_len += wsprintfA(path_end, basename, date_string);
	if (log_path_len)
		*log_path_len = path_len;

	return CreateFileA(log_filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
}

#define cpu_reg(x) offsetof(CONTEXT, x)
#ifdef _WIN64
#define float_reg(x) offsetof(CONTEXT, FltSave.x)
#define INT_REGISTER_COUNT 17
static unsigned short register_offsets[] =
{
	cpu_reg(Rax), cpu_reg(Rbx), cpu_reg(Rcx), cpu_reg(Rdx), cpu_reg(Rsi),
	cpu_reg(Rdi), cpu_reg(R8), cpu_reg(R9), cpu_reg(R10), cpu_reg(R11),
	cpu_reg(R12), cpu_reg(R13), cpu_reg(R14), cpu_reg(R15), cpu_reg(Rbp),
	cpu_reg(Rsp), cpu_reg(Rip), cpu_reg(EFlags) | 2,

	(float_reg(FloatRegisters)+   8) | 1, float_reg(FloatRegisters),
	(float_reg(FloatRegisters)+0x18) | 1, float_reg(FloatRegisters)+0x10,
	(float_reg(FloatRegisters)+0x28) | 1, float_reg(FloatRegisters)+0x20,
	(float_reg(FloatRegisters)+0x38) | 1, float_reg(FloatRegisters)+0x30,
	(float_reg(FloatRegisters)+0x48) | 1, float_reg(FloatRegisters)+0x40,
	(float_reg(FloatRegisters)+0x58) | 1, float_reg(FloatRegisters)+0x50,
	(float_reg(FloatRegisters)+0x68) | 1, float_reg(FloatRegisters)+0x60,
	(float_reg(FloatRegisters)+0x78) | 1, float_reg(FloatRegisters)+0x70,
	float_reg(StatusWord) | 1, float_reg(ControlWord) | 1,

	float_reg(XmmRegisters)+   8, float_reg(XmmRegisters),
	float_reg(XmmRegisters)+0x18, float_reg(XmmRegisters)+0x10,
	float_reg(XmmRegisters)+0x28, float_reg(XmmRegisters)+0x20,
	float_reg(XmmRegisters)+0x38, float_reg(XmmRegisters)+0x30,
	float_reg(XmmRegisters)+0x48, float_reg(XmmRegisters)+0x40,
	float_reg(XmmRegisters)+0x58, float_reg(XmmRegisters)+0x50,
	float_reg(XmmRegisters)+0x68, float_reg(XmmRegisters)+0x60,
	float_reg(XmmRegisters)+0x78, float_reg(XmmRegisters)+0x70,
	float_reg(XmmRegisters)+0x88, float_reg(XmmRegisters)+0x80,
	float_reg(XmmRegisters)+0x98, float_reg(XmmRegisters)+0x90,
	float_reg(XmmRegisters)+0xA8, float_reg(XmmRegisters)+0xA0,
	float_reg(XmmRegisters)+0xB8, float_reg(XmmRegisters)+0xB0,
	float_reg(XmmRegisters)+0xC8, float_reg(XmmRegisters)+0xC0,
	float_reg(XmmRegisters)+0xD8, float_reg(XmmRegisters)+0xD0,
	float_reg(XmmRegisters)+0xE8, float_reg(XmmRegisters)+0xE0,
	float_reg(XmmRegisters)+0xF8, float_reg(XmmRegisters)+0xF0
};
#else
#define float_reg(x) offsetof(CONTEXT, FloatSave.x)
#define INT_REGISTER_COUNT 9
static unsigned char register_offsets[] =
{
	cpu_reg(Eax), cpu_reg(Ebx), cpu_reg(Ecx), cpu_reg(Edx), cpu_reg(Esi),
	cpu_reg(Edi), cpu_reg(Ebp), cpu_reg(Esp), cpu_reg(Eip), cpu_reg(EFlags),
	cpu_reg(SegCs), cpu_reg(SegDs), cpu_reg(SegSs), cpu_reg(SegEs), cpu_reg(SegFs), cpu_reg(SegGs),

	(float_reg(RegisterArea)+78) | 1, float_reg(RegisterArea)+74, float_reg(RegisterArea)+70,
	(float_reg(RegisterArea)+68) | 1, float_reg(RegisterArea)+64, float_reg(RegisterArea)+60,
	(float_reg(RegisterArea)+58) | 1, float_reg(RegisterArea)+54, float_reg(RegisterArea)+50,
	(float_reg(RegisterArea)+48) | 1, float_reg(RegisterArea)+44, float_reg(RegisterArea)+40,
	(float_reg(RegisterArea)+38) | 1, float_reg(RegisterArea)+34, float_reg(RegisterArea)+30,
	(float_reg(RegisterArea)+28) | 1, float_reg(RegisterArea)+24, float_reg(RegisterArea)+20,
	(float_reg(RegisterArea)+18) | 1, float_reg(RegisterArea)+14, float_reg(RegisterArea)+10,
	(float_reg(RegisterArea)+ 8) | 1, float_reg(RegisterArea)+4,  float_reg(RegisterArea),
	float_reg(StatusWord) | 1, float_reg(ControlWord) | 1
};
#endif


UINT __stdcall WriteCrashlog(DWORD process_id, GpuInfo* gpu_info, char* log_folder, char* log_filename, const char* location)
{
	termination_info ti(log_filename);

	{
		OpAutoHANDLE crashlog_event(CreateCrashlogEvent(process_id, true));
		if (!crashlog_event.get())
		{
			ti.error_msg = "error creating crashlog event";
			return 0;
		}

		if (!DebugActiveProcess(process_id))
		{
			ti.error_msg = "couldn't debug process";
			return 0;
		}

		// signal to process that crashed that crash logger is attached
		if (!SetEvent(crashlog_event.get()))
		{
			ti.error_msg = "error signalling crashlog event";
			return 0;
		}
	}

	DEBUG_EVENT dev;
	HANDLE process = 0;
	ULONG_PTR image_base = 0;
	BOOL is_crash_event = FALSE;
	thread_map threads;
	threads.next = NULL;
	byte_range_list code_ranges;
	dll_list dlls;
	byte_range_list memdump_list;
	char *executable_name = NULL;

	while(WaitForDebugEvent(&dev, 10000))
	{
		switch (dev.dwDebugEventCode)
		{
		case CREATE_PROCESS_DEBUG_EVENT:
		{
			process = dev.u.CreateProcessInfo.hProcess;
			threads.thread_id = dev.dwThreadId;						// main thread
			threads.thread_handle = dev.u.CreateProcessInfo.hThread;
			image_base = (ULONG_PTR)dev.u.CreateProcessInfo.lpBaseOfImage;
			executable_name = get_range_and_name(dev.u.CreateProcessInfo.lpImageName, dev.u.CreateProcessInfo.fUnicode, dev.u.CreateProcessInfo.hFile, image_base, 0, code_ranges);
			break;
		}
		case CREATE_THREAD_DEBUG_EVENT:
		{
			thread_map *new_thread = new thread_map;
			if (new_thread)
			{
				new_thread->thread_id = dev.dwThreadId;
				new_thread->thread_handle = dev.u.CreateThread.hThread;
				new_thread->next = threads.next;
				threads.next = new_thread;
			}
			break;
		}
		case LOAD_DLL_DEBUG_EVENT:
		{
			dll_line *new_dll = new dll_line;
			if (new_dll)
			{
				new_dll->line = get_range_and_name(dev.u.LoadDll.lpImageName, dev.u.LoadDll.fUnicode, dev.u.LoadDll.hFile, (ULONG_PTR)dev.u.LoadDll.lpBaseOfDll, 39, code_ranges);

				if (!new_dll->line)
					new_dll->line = "<no name>";

				if (!dlls.first)
					dlls.first = new_dll;
				else
					dlls.last->next = new_dll;

				dlls.last = new_dll;
			}
			break;
		}
		case EXCEPTION_DEBUG_EVENT:
		{
			if (!is_crash_event && dev.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
			{
				// ignore the breakpoint triggered by DebugActiveProcess.
				// We are only interested in the actual exception.
				is_crash_event = TRUE;
				break;
			}

			UINT log_path_len = 0;
			HANDLE log_file;

			// Is it a plug-in crash
			if (log_folder)
			{
				log_file = CreatePluginLogFile(log_filename, "crash%s.txt", log_folder, &log_path_len);
			}
			else
			{
				GpuInfo info;
				if (GetGpuInfo(process, gpu_info, &info))
				{
					HANDLE gpu_file = CreateLogFile(log_filename, "gpu_info.txt", &log_path_len);

					if (gpu_file != INVALID_HANDLE_VALUE)
					{
						DWORD written = 0;
						WriteFile(gpu_file, info.device_info, info.device_info_size, &written, NULL);
						CloseHandle(gpu_file);
					}
				}

				log_file = CreateLogFile(log_filename, "crash.txt", &log_path_len);
			}

			if (log_file == INVALID_HANDLE_VALUE)
			{
				delete[] executable_name;

				if (!log_path_len)
					ti.error_msg = "error getting temp directory";
				else
				{
					ti.is_fileerr = TRUE;
					ti.error_msg = "error creating file";
				}
				return 0;
			}

			ti.log_file = log_file;

			HANDLE current_thread = 0;
			thread_map *check_thread = &threads;
			do
			{
				if (check_thread->thread_id == dev.dwThreadId)
				{
					current_thread = check_thread->thread_handle;
					break;
				}
				check_thread = check_thread->next;
			} while (check_thread);

			CONTEXT context;
			context.ContextFlags = CONTEXT_FULL | CONTEXT_FLOATING_POINT;
			GetThreadContext(current_thread, &context);

			LONG_PTR args[sizeof(register_offsets) + 4];

			args[0] = executable_name ? (LONG_PTR)executable_name : (LONG_PTR)"<no name>";
			args[1] = dev.u.Exception.ExceptionRecord.ExceptionCode;
			args[2] = (LONG_PTR)dev.u.Exception.ExceptionRecord.ExceptionAddress;
			args[3] = image_base;

			for (UINT i=0; i<ARRAY_SIZE(register_offsets); i++)
			{
				ULONG_PTR offset = register_offsets[i];
				BYTE *reg = (BYTE *)&context + (offset & ~3);
				ULONG_PTR reg_value = (offset & 1) ? *(unsigned short *)reg :
#ifdef _WIN64
									  (offset & 2) ? *(unsigned int *)reg : 
#endif
									  *(ULONG_PTR *)reg;

				args[i+4] = reg_value;
#ifdef _WIN64
				if (i <= 16)
					add_pointer(memdump_list, reg_value, context.Rsp, process, code_ranges);
#else
				if (i <= 8)
					add_pointer(memdump_list, reg_value, context.Esp, process, code_ranges);
#endif
			}

			static char header_txt[] = 
#ifdef _WIN64
#ifndef _DEBUG
				"OPERA-CRASHLOG V1 desktop " VER_NUM_STR " " VER_BUILD_NUMBER_STR " windows x64\r\n"
#endif //!_DEBUG
				"%s caused exception %X at address %012I64X (Base: %I64X)\r\n\r\n"
				"Registers:\r\n"
				"RAX=%016I64X   RBX=%016I64X   RCX=%016I64X\r\n"
				"RDX=%016I64X   RSI=%016I64X   RDI=%016I64X\r\n"
				" R8=%016I64X    R9=%016I64X   R10=%016I64X\r\n"
				"R11=%016I64X   R12=%016I64X   R13=%016I64X\r\n"
				"R14=%016I64X   R15=%016I64X   RBP=%016I64X\r\n"
				"RSP=%016I64X   RIP=%016I64X FLAGS=%08X\r\n"
				"FPU stack:\r\n"
				"st(0)=%04X%016I64X st(1)=%04X%016I64X st(2)=%04X%016I64X\r\n"
				"st(3)=%04X%016I64X st(4)=%04X%016I64X st(5)=%04X%016I64X\r\n"
				"st(6)=%04X%016I64X st(7)=%04X%016I64X SW=%04X CW=%04X\r\n"
 				" XMM0=%016I64X%016I64X  XMM1=%016I64X%016I64X\r\n"
 				" XMM2=%016I64X%016I64X  XMM3=%016I64X%016I64X\r\n"
 				" XMM4=%016I64X%016I64X  XMM5=%016I64X%016I64X\r\n"
 				" XMM6=%016I64X%016I64X  XMM7=%016I64X%016I64X\r\n"
 				" XMM8=%016I64X%016I64X  XMM9=%016I64X%016I64X\r\n"
				"XMM10=%016I64X%016I64X XMM11=%016I64X%016I64X\r\n"
				"XMM12=%016I64X%016I64X XMM13=%016I64X%016I64X\r\n"
				"XMM14=%016I64X%016I64X XMM15=%016I64X%016I64X\r\n\r\n"
				"Stack dump:\r\n";
			                                                                        // %s  %X %08X %012I64X %I64X %016I64X
			ti.register_text = new char[strlen((char*)args[0]) + ARRAY_SIZE(header_txt)-2 + 6 + 4 + 4 + 7 + 57*8];

#else
#ifndef _DEBUG
				"OPERA-CRASHLOG V1 desktop " VER_NUM_STR " " VER_BUILD_NUMBER_STR " windows\r\n"
#endif //!_DEBUG
				"%s caused exception %X at address %08X (Base: %X)\r\n\r\n"
				"Registers:\r\n"
				"EAX=%08X   EBX=%08X   ECX=%08X   EDX=%08X   ESI=%08X\r\n"
				"EDI=%08X   EBP=%08X   ESP=%08X   EIP=%08X FLAGS=%08X\r\n"
				"CS=%04X   DS=%04X   SS=%04X   ES=%04X   FS=%04X   GS=%04X\r\n"
				"FPU stack:\r\n"
				"%04X%08X%08X %04X%08X%08X %04X%08X%08X\r\n"
				"%04X%08X%08X %04X%08X%08X %04X%08X%08X\r\n"
				"%04X%08X%08X %04X%08X%08X SW=%04X CW=%04X\r\n\r\n"
				"Stack dump:\r\n";
			                                                                            // %s   %X    %08X
			ti.register_text = new char[strlen((char*)args[0]) + ARRAY_SIZE(header_txt)-2 + 2*6 + 27*4];
#endif
			if (!ti.register_text)
			{
				delete[] executable_name;
				ti.error_msg = "out of memory";
				return 0;
			}

#ifdef _WIN64
			int write_len = vsprintf(ti.register_text, header_txt, (va_list)args);
#else
			DWORD write_len = wvsprintfA(ti.register_text, header_txt, (va_list)args);
#endif

			delete[] executable_name;

			if (!ti.write_log(ti.register_text, write_len))
				return 0;

#ifdef _WIN64
			ULONG_PTR stack_pos = context.Rsp;
			ULONG_PTR frame_pointer = context.Rbp;
#else
			ULONG_PTR stack_pos = context.Esp;
			ULONG_PTR frame_pointer = context.Ebp;
#endif

			ti.mem_buffer = new BYTE[0x1000];
			if (!ti.mem_buffer)
			{
				ti.error_msg = "out of memory";
				return 0;
			}
#ifdef _WIN64
			ti.dump_buffer = new char[STACK_DUMP_SIZE/0x10 * 68];
#else
			ti.dump_buffer = new char[STACK_DUMP_SIZE/0x10 * 80 + 2];
#endif
			if (!ti.dump_buffer)
			{
				ti.error_msg = "out of memory";
				return 0;
			}

			for (UINT stack_pages = 0; stack_pages < STACK_DUMP_SIZE / 0x1000; stack_pages++)
			{
				UINT read_size = 0x1000;
				if (!ReadProcessMemory(process, (LPVOID)stack_pos, ti.mem_buffer, read_size, 0))
				{
					read_size -= (stack_pos & 0xFFF);
					if (!ReadProcessMemory(process, (LPVOID)stack_pos, ti.mem_buffer, read_size, 0))
						break;
				}
				char *dump_pos = ti.dump_buffer;
				BYTE *stack_buf_pos = ti.mem_buffer;

				do
				{
#ifdef _WIN64
					dump_pos += sprintf(dump_pos, "%012I64X", stack_pos);
#else
					dump_pos += wsprintfA(dump_pos, "%08X", stack_pos);
#endif

					UINT dump_len = read_size;
					BYTE *dump_stack_pos = stack_buf_pos;

#ifdef _WIN64
					for (UINT stack_words = 0; stack_words < 2; stack_words++)
#else
					for (UINT stack_words = 0; stack_words < 4; stack_words++)
#endif
					{
						*(short *)dump_pos = '  ';
						dump_pos += 2;

#ifdef _WIN64
						if (dump_len < 8)
						{
							*(LONG_PTR *)dump_pos = (LONG_PTR)'    '<<32 | '    ';
							dump_pos += 8;
							*(LONG_PTR *)dump_pos = (LONG_PTR)'    '<<32 | '    ';
							dump_pos += 8;
						}
#else
						if (dump_len < 4)
						{
							*(int *)dump_pos = '    ';
							dump_pos += 4;
							*(int *)dump_pos = '    ';
							dump_pos += 4;
						}
#endif
						else
						{
							ULONG_PTR stack_word = *(ULONG_PTR *)dump_stack_pos;
#ifdef _WIN64
							add_pointer(memdump_list, stack_word, context.Rsp, process, code_ranges);
#else
							add_pointer(memdump_list, stack_word, context.Esp, process, code_ranges);
#endif

							if (frame_pointer == stack_pos)
							{
								frame_pointer = stack_word;
								dump_pos[-1] = '>';
							}
#ifdef _WIN64
							dump_pos += sprintf(dump_pos, "%016I64X", stack_word);
							dump_stack_pos += 8;
							stack_pos += 8;
							dump_len -= 8;
#else
							dump_pos += wsprintfA(dump_pos, "%08X", stack_word);
							dump_stack_pos += 4;
							stack_pos += 4;
							dump_len -= 4;
#endif
						}
					}
					dump_pos = dump_chars(dump_pos, stack_buf_pos, read_size);

				} while (read_size);

				if (!ti.write_log(ti.dump_buffer, dump_pos - ti.dump_buffer))
					return 0;
			}

			static char used_dlls_txt[] = "\r\nUsed DLLs:\r\n";
			if (!ti.write_log(used_dlls_txt, ARRAY_SIZE(used_dlls_txt)-1))
				return 0;

			dll_line *current_dll = dlls.first;
			while (current_dll)
			{
				if (!ti.write_log(current_dll->line, (UINT)strlen(current_dll->line)))
					return 0;
				current_dll = current_dll->next;
			}

			static char mem_dumps_txt[] = "\r\nMemory dumps:\r\n";
			if (!ti.write_log(mem_dumps_txt, ARRAY_SIZE(mem_dumps_txt)-1))
				return 0;

			byte_range *current_mem = memdump_list.first;
			while (current_mem)
			{
				char dummy;

				ULONG_PTR dump_start = current_mem->range_start;
				ULONG_PTR dump_end = current_mem->range_end;

				if (!ReadProcessMemory(process, (LPVOID)dump_start, &dummy, 1, 0))
					dump_start = (dump_start | 0xFFF) + 1;

				if (!ReadProcessMemory(process, (LPVOID)dump_end, &dummy, 1, 0))
					dump_end = (dump_end & ~0xFFF);

				while (dump_start < dump_end)
				{
					UINT read_size = dump_end - dump_start;
					if (read_size > 0x1000)
						read_size = 0x1000;

					ReadProcessMemory(process, (LPVOID)dump_start, ti.mem_buffer, read_size, 0);

					char *dump_pos = ti.dump_buffer;
					BYTE *mem_buf_pos = ti.mem_buffer;

					while (read_size)
					{
#ifdef _WIN64
						dump_pos += sprintf(dump_pos, "%012I64X", dump_start);
#else
						dump_pos += wsprintfA(dump_pos, "%08X", dump_start);
#endif

						UINT dump_len = read_size;
						BYTE *dump_mem_pos = mem_buf_pos;

						for (UINT mem_bytes = 0; mem_bytes < 16; mem_bytes++)
						{
							*dump_pos++ = ' ';
							if (!(mem_bytes & 3))
								*dump_pos++ = ' ';

							if (!dump_len)
							{
								*(short *)dump_pos = '  ';
								dump_pos += 2;
							}
							else
							{
#ifdef _WIN64
								dump_pos += sprintf(dump_pos, "%02X", *dump_mem_pos);
#else
								dump_pos += wsprintfA(dump_pos, "%02X", *dump_mem_pos);
#endif
								dump_mem_pos++;
								dump_start++;
								dump_len--;
							}
						}
						dump_pos = dump_chars(dump_pos, mem_buf_pos, read_size);
					}

					if (dump_start >= dump_end)
					{
						*(short *)dump_pos = 0x0A0D;
						dump_pos += 2;
					}

					if (!ti.write_log(ti.dump_buffer, dump_pos - ti.dump_buffer))
						return 0;
				}
				current_mem = current_mem->next;
			}
			return log_path_len;
		}
		}
		ContinueDebugEvent(dev.dwProcessId, dev.dwThreadId, DBG_CONTINUE);
	}
	return 0;
}

void *internal_malloc(size_t size, BYTE **stack_frame);
void *MemGuard_malloc(size_t size);
void *common_malloc();
void MemGuard_free(void *mem);
void *MemGuard_realloc(void *mem, size_t new_size);
void MemGuard_doexit();
void MemGuard_breakpoint();

struct _mg_config
{
	UINT stacksize, max_alloccount;

	// can't use array, inline assembler can't calculate array offsets
	struct
	{
		BYTE *malloc_offset, *func1;
		BYTE *free_offset, *func2;
		BYTE *realloc_offset, *func3;
		BYTE *doexit_offset, *func4;
		BYTE *layout_pool1, *func5;
		BYTE *layout_pool2, *func6;
		BYTE *layout_pool3, *func7;
	} hook_functions;

	CRITICAL_SECTION crit_sect;

	// lower region is for allocations <4 KiB (one page), upper region for the rest,
	// to avoid excessive fragmentation
	struct mem_region
	{
		UINT region_start;
		UINT region_end;
		UINT next_alloc;		// page index that follows the last successful allocation
		UINT realmem;		// real size of allocated memory in region
		UINT logmem;			// size of guarded allocated memory
		UINT pages_free;		// pages avaiable for allocations and guard pages
	} mem_regions[2];

	UINT virt_pages;			// total number of reserved virtual memory pages
	int *memlist;			// address of memory list
	void *membase;			// address of reserved memory for allocations
	BYTE *stackdumps;		// address of memory for stackdumps
	DWORD_PTR *counts;		// address of memory for allocation counts
	DWORD_PTR image_base;
	HANDLE loghandle;

	BYTE **logoffs, **excloffs, **breakpoints;
	UINT *bpsizes;
	BYTE **membreaks;
	UINT *mbbases, *mbsizes;
	BYTE **stackguards;
	
	BYTE **logoffpos, **excloffpos, **breakpointpos;
	UINT *bpsizepos;
	BYTE **membreakpos;
	UINT *mbbasepos, **mbsizepos;
	BYTE **stackguardpos;

	BYTE *logbuf, *logbufpos, *logbufend;
} mg_config =
{
	0x800,
#ifdef _WIN64
	UINT_MAX,
	(BYTE *)-16,		(BYTE *)MemGuard_malloc,
	(BYTE *)-3,		(BYTE *)MemGuard_free,
	(BYTE *)-0x30,	(BYTE *)MemGuard_realloc,
	(BYTE *)14,		(BYTE *)MemGuard_doexit,
	(BYTE *)-7,		(BYTE *)-1
#else
	0x200,
	(BYTE *)-8,		(BYTE *)MemGuard_malloc -5,
	(BYTE *)-10,		(BYTE *)MemGuard_free -5,
	(BYTE *)-0x5c,	(BYTE *)MemGuard_realloc -5,
	(BYTE *)3,		(BYTE *)MemGuard_doexit -5,
	(BYTE *)0,		(BYTE *)-6,
	(BYTE *)0,		(BYTE *)-7,
	(BYTE *)-15,		(BYTE *)-1
#endif
};

struct _mg_config::mem_region* mem_regions;

#define SMALL_STACKDUMP_SIZE		0x20 * sizeof(DWORD_PTR)
#define FLAG_BLOCK_USED			0x80000000
#define FLAG_BLOCK_READONLY		0x40000000
#define FLAG_BLOCK_ECMASCRIPT	0x20000000
#define SIZE_MASK				(FLAG_BLOCK_ECMASCRIPT-1)

void * __stdcall InitializeMemGuard(char *mg_ini)
{
	// set mem_regions here dynamically so that the compiler can't know its value,
	// and always go through it instead of accessing mg_config.mem_regions directly, 
	// to prevent the compiler from making some silly "optimisations"
	mem_regions = mg_config.mem_regions;

	/*
	MemGuard.ini contains settings, one per line, usually with parameters which are hex numbers.
	If a parameter specifies a code address, the number is relative to the base address of Opera.dll.

	No modifier: A single number is interpreted as the return address of a
				 specific allocation/deletion for a long_stack_dump
	Modifier x: Return address of an allocation to be excluded from
				guarding to avoid excessive slowdown and/or memory usage
	Modifier p: Breakpoint for a long_stack_dump at any interesting location.
				Parameters are the address and number of instruction bytes to
				replace (at least 5), e.g. p1EF389 6
				Replaced instructions must not contain conditional/short jumps!
				First 32 bytes of stackdump are the registers
				eax, ebx, ecx, edx, esi, edi, ebp, esp
	Modifier m: Breakpoint for a long_stack_dump on memory write
				Parameters are the return address of the allocation in which
				the breakpoint is to be set, the offset (must not be bigger than
				0x1000) and the size of the trigger area within that allocation,
				e.g. m1EF389 DC 4
	Modifier g: Start address of an ebp-frame function for stack guarding
	Modifier s: Number to override the default long_stack_dump size (0x800)
	Modifier l: Number to override the default allocation count limit
	Modifier j: Also guard Ecmascript allocations
*/

	static unsigned char modifiers[] = {'x',1,'p',2,'m',4,'g',7,'s',60,'l',61,0};

	UINT state = 0, number = 0;
	unsigned char c;
	while ((c = *mg_ini++) != 0)
	{
		c |= 0x20;

		if (!state)
		{
			unsigned char *mod = modifiers;
			do
			{
				if (c == *mod)
				{
					state = mod[1];
					goto next_char;	// continue outer while
				}
			} while (*(mod += 2));
		}
		if ((c -= '0') <= 9 || ((c -= 0x27) >= 10 && c <= 15))
		{
			number = number << 4 | c;
			if (*mg_ini)
				continue;
		}

		if (!state && !number)
			continue;

		if (state == 60)
		{
			mg_config.stacksize = number;
			state = 0;
		}
		else if (state == 61)
		{
			mg_config.max_alloccount = number;
			state = 0;
		}
		else
		{
			BYTE ***buf = &mg_config.logoffs + state;
			BYTE ***pos = buf + (&mg_config.logoffpos - &mg_config.logoffs);
			int new_bytesize = (char *)(*pos+1)-(char *)*buf;
			*buf = (BYTE **)realloc(*buf, new_bytesize);
			*pos = (BYTE **)((char *)*buf + new_bytesize);
			*(*pos-1) = (BYTE *)number;

			if (state == 2 || state == 4 || state == 5)
				state++;
			else
				state = 0;
		}
		number = 0;
		next_char:;
	}

	if (mg_config.membase)
	{
		// this was an additional ini, skip first-time initialization
		return mg_config.membase;
	}

	InitializeCriticalSection(&mg_config.crit_sect);

#ifdef _WIN64
	DWORD_PTR virt_size = 0x1000000000;
#else
	DWORD_PTR virt_size = 0x70000000;
#endif
	void *membase;
	while ((membase = VirtualAlloc(0, virt_size, MEM_RESERVE, PAGE_READWRITE)) == NULL && virt_size > 0x10000000)
		virt_size -= 0x10000000;

	mg_config.membase = membase;
	mg_config.virt_pages = virt_size / 0x1000;

	return membase;
}


extern "C++" void WindowsUtils::ShowError(char *message);

BOOL __stdcall InstallMemGuardHooks(HINSTANCE opera_dll)
{
	if (!mg_config.membase)
		return TRUE;

	UINT logbufsize = (0x10000/mg_config.stacksize)*mg_config.stacksize;
	DWORD_PTR pages = mg_config.virt_pages;
	int *memlist;

	if ((mg_config.logbufend = mg_config.logbuf = mg_config.logbufpos = (BYTE *)
			VirtualAlloc(0, logbufsize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) == NULL ||
		(mg_config.stackdumps = (BYTE *)
			VirtualAlloc(0, pages * SMALL_STACKDUMP_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) == NULL ||
		(mg_config.counts = (DWORD_PTR *)
			VirtualAlloc(0, 0x2000 * sizeof(DWORD_PTR)*2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) == NULL ||
		(memlist = mg_config.memlist = (int *)
			VirtualAlloc(0, pages * sizeof(int), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) == NULL)
	{
		WindowsUtils::ShowError("Couldn't initialize MemGuard, %s");
		return FALSE;
	}

	// reserve upper 12% for big allocations (>=4 KiB, one page) to reduce fragmentation loss
	UINT eighth = pages/8;
	memlist[0] = mem_regions[0].pages_free = mem_regions[0].region_end =
		         mem_regions[1].region_start = mem_regions[1].next_alloc = eighth*7;
	mem_regions[1].pages_free = memlist[eighth*7] = eighth;
	mem_regions[1].region_end = pages;

	mg_config.logbufend += logbufsize;

	mg_config.image_base = (DWORD_PTR)opera_dll;
	BYTE *image_base = (BYTE *)opera_dll;
	BYTE *pe_header = image_base + *(UINT *)(image_base+0x3c);

	if (*(short *)image_base != 'ZM' || *(short *)pe_header != 'EP')
	{
		MessageBoxA(0, "Couldn't initialize MemGuard, error patching Opera.dll!", "Opera Error", MB_OK);
		return FALSE;
	}

	BYTE *code_start = image_base + *(UINT *)(pe_header+0x2c);
	BYTE *code_end = code_start + *(UINT *)(pe_header+0x1c);

	static BYTE hooksigs[] = {
#ifdef _WIN64
		0x8B,0xD9,0x48,0x83,0xF9,0xE0,0x77,0x7C,		// malloc
		0x74,0x37,0x53,0x48,0x83,0xEC,0x20,0x4C,		// free
		0x48,0x83,0xFA,0xE0,0x77,0x43,0x48,0x8B,		// realloc
		0x01,0x00,0x00,0x00,0xB9,0x08,0x00,0x00,		// doexit
		0x63,0x51,0x0C,0x48,0x8B,0xD9,0x48,0xC7		// LayoutPool::ConstructL
	};

	int num_sigs = sizeof(hooksigs)/8;
#else
		0x08,0x83,0xFB,0xE0,0x77,0x6F,0x56,0x57,		// malloc
		0x2D,0xFF,0x75,0x08,0x6A,0x00,0xFF,0x35,		// free
		0x74,0x1D,0x83,0xFE,0xE0,0x76,0xCB,0x56,		// realloc
		0xFF,0xFF,0x59,0xFF,0x75,0x08,0xFF,0x15,		// doexit
		0x8B,0x46,0x08,0x6A,0x04,0x33,0xC9,0x5A,		// LayoutPool::ConstructL (size/PGO)
		0x8B,0x47,0x08,0x33,0xC9,0xBA,0x04,0x00,		// LayoutPool::ConstructL (speed)
		0x8B,0x31,0x89,0x14,0x86,0xFF,0x41,0x04		// LayoutPool::Delete (without LTCG)
	};

	int num_sigs = sizeof(hooksigs)/8 - 2;			// only one of the LayoutPool sigs will be found
#endif
	BYTE *code_pos;
	for (code_pos = code_start; code_pos < code_end && num_sigs; code_pos++)
	{
		for (int sig_id = 0; sig_id < sizeof(hooksigs)/8; sig_id++)
		{
			if (!memcmp(code_pos, hooksigs + sig_id*8, 8))
			{
				BYTE **hook_data = (BYTE **)&mg_config.hook_functions + sig_id * 2;
				BYTE *patch_adr = (hook_data[0] += (ULONG_PTR)code_pos);
				BYTE *func_adr = hook_data[1];
				if (func_adr)
				{
					DWORD old_protect;
					VirtualProtect(patch_adr, 0x100, PAGE_READWRITE, &old_protect);
					if ((INT_PTR)func_adr > 0)
#ifdef _WIN64
					{
						patch_adr[0] = 0x48;
						patch_adr[1] = 0xB8;						// mov rax, ...
						*(BYTE **)(patch_adr+2) = func_adr;
						patch_adr[10] = 0xFF;
						patch_adr[11] = 0xE0;					// jmp rax
					}
					else {
						// Disable all 14 LayoutPools, see modules/layout/layoutpool.cpp
						*(ULONG_PTR *)patch_adr = 0xC30C4189C03148; // xor rax,rax; mov [rcx+LayoutPool.m_size],eax; ret
					}
#else
					{
						*patch_adr = 0xE9;						// jmp
						*(ULONG_PTR *)(patch_adr+1) = func_adr - patch_adr;
					}
					else {
						// Disable all 14 LayoutPools, see modules/layout/layoutpool.cpp
						if (++func_adr != 0)
						{
							patch_adr[0] = 0x33;
							patch_adr[1] = 0xC0;						// xor eax, eax
							patch_adr[2] = 0x89;
							patch_adr[3] = 0x41 - (BYTE)func_adr;
							patch_adr[4] = 0x08;						// mov [<reg>+LayoutPool.m_size],eax
							patch_adr[5] = 0xC3;						// ret
						}
						else 
						{
							patch_adr[0] = 0x90;						// nop
							patch_adr[1] = 0x90;
						}
					}
#endif
					VirtualProtect(patch_adr, 0x100, old_protect, &old_protect);
				}
				num_sigs--;
				break;
			}
		}
	}
	if (code_pos == code_end)
	{
		MessageBoxA(0, "Couldn't initialize MemGuard, function signature not found in Opera.dll!", "Opera Error", MB_OK);
		return FALSE;
	}
	int code_size = ((ULONG_PTR)mg_config.breakpointpos - (ULONG_PTR)mg_config.breakpoints) * (64 / sizeof(BYTE **));
	if (code_size)
	{
		BYTE *code_pages = (BYTE *)VirtualAlloc(0, code_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		code_pos = code_pages;
#ifdef _WIN64
		*(void **)code_pos = MemGuard_breakpoint;
		code_pos += sizeof(void *);
#endif
		for (BYTE **brk = mg_config.breakpoints; brk < mg_config.breakpointpos; brk++)
		{
			int brk_size = *(int *)((ULONG_PTR)mg_config.bpsizes + (ULONG_PTR)brk - (ULONG_PTR)mg_config.breakpoints);
#ifdef _WIN64
			if (brk_size < 13 || brk_size > 25)
#else
			if (brk_size < 5 || brk_size > 40)
#endif
				MessageBoxA(0, "Warning: Bad breakpoint size in MemGuard.ini", "Opera Error", MB_OK);
			else
			{
				*brk += (DWORD_PTR)image_base;
				DWORD old_protect;
				VirtualProtect(*brk, brk_size, PAGE_READWRITE, &old_protect);

				BYTE *start_pos = code_pos;
#ifdef _WIN64
				code_pos[8]  = 0x58;						// pop rax
				code_pos[9]  = 0x48;
				code_pos[10] = 0x87;
				code_pos[11] = 0x04;
				code_pos[12] = 0x24;						// xchg rax,[rsp]

				code_pos[13] = 0x9C;						// pushf

				code_pos[14] = 0xFF;
				code_pos[15] = 0x15;						// call [rip+xx]
				*(DWORD *)(code_pos+16) = code_pages - (code_pos + 20);

				code_pos[20] = 0x48;
				code_pos[21] = 0x83;
				code_pos[22] = 0x44;
				code_pos[23] = 0x24;						// add [rsp+8],xx: modify the return address to skip the replaced code
				code_pos[24] = 0x08;						// (MemGuard_breakpoint adjusts it to point at the replaced code)
				code_pos[25] = brk_size;

				code_pos[26] = 0x9D;						// popf

				code_pos[27] = 0x8F;
				code_pos[28] = 0x05;						// pop [rip+xx]
				*(DWORD *)(code_pos+29) = start_pos - (code_pos + 33);
				code_pos += 33;

				int size = brk_size;
				BYTE *original_code = *brk;
				while (size--) { *code_pos++ = *original_code++; }

				code_pos[0] = 0xFF;
				code_pos[1] = 0x25;						// jmp [rip+xx]
				*(DWORD *)(code_pos+2) = start_pos - (code_pos + 6);

				code_pos = *brk;
				*code_pos++ = 0x50;						// push rax
				*code_pos++ = 0x48;
				*code_pos++ = 0xB8;						// mov rax, ...
				*(void **)code_pos = start_pos + sizeof(void *);
				code_pos += sizeof(void *);

				*code_pos++ = 0xFF;
				*code_pos++ = 0xD0;						// call rax
#else
				code_pos += 4;
				*code_pos++ = 0x9C;						// pushf
				*code_pos = 0xE8;						// call
				code_pos += 5;
				*(DWORD *)(code_pos-4) = (BYTE *)MemGuard_breakpoint - code_pos;

				char not_jump = **brk - 0xE9;
				if (not_jump)	
				{
					*(DWORD *)code_pos = 0x04244483;		// add [esp+4],xx: modify the return address to skip the replaced code
					code_pos += 4;						// (MemGuard_breakpoint adjusts it to point at the replaced code)
					*code_pos++ = brk_size;
				}

				*code_pos++ = 0x9D;						// popf

				if (!not_jump)							// replaced code is a jump
				{
					*(DWORD *)code_pos = 0x0424648D;		// lea esp,[esp+4] (add esp,4 would modify flags!)
					code_pos += 4;
				}

				if (!not_jump || !++not_jump)			// replaced code is a jump or call
				{
					*code_pos = 0xE9;					// jmp
					*(DWORD *)(code_pos+1) = *(DWORD *)(*brk+1) + *brk - code_pos;
				}
				else
				{
					*code_pos++ = 0x8F;
					*code_pos++ = 0x05;					// pop [xx]
					*(BYTE **)code_pos = start_pos;
					code_pos += 4;

					int size = brk_size;
					BYTE *original_code = *brk;
					while (size--) { *code_pos++ = *original_code++; }

					*code_pos++ = 0xFF;
					*code_pos++ = 0x25;					// jmp [xx]
					*(BYTE **)code_pos = start_pos;
				}
				**brk = 0xE8;							// call
				*(DWORD *)(*brk+1) = start_pos+4 - (*brk+5);
#endif
				VirtualProtect(*brk, brk_size, old_protect, &old_protect);

				code_pos = start_pos + 64;
			}
		}
	}
	return TRUE;
}


void DestroyMemGuard()
{
	if (!mem_regions)
		return;

	DeleteCriticalSection(&mg_config.crit_sect);

	for (BYTE ***buf = &mg_config.logoffs; buf < &mg_config.logoffpos; buf++)
	{
		free(*buf);
		*buf = NULL;
	}

	CloseMemGuardLog();

	if (mg_config.memlist)
	{
		UINT current_id = 0;
		HANDLE leak_log = 0;

		while (1)
		{
			UINT block_size = mg_config.memlist[current_id];
			if ((int)block_size < 0)
			{
				if (!leak_log)
				{
					char filename[MAX_PATH];
					leak_log = CreateLogFile(filename, "MemLeak.bin", 0);
					if (leak_log == INVALID_HANDLE_VALUE)
						break;
				}

				DWORD dummy;
				WriteFile(leak_log, mg_config.stackdumps + (DWORD_PTR)(current_id+1) * SMALL_STACKDUMP_SIZE, SMALL_STACKDUMP_SIZE, &dummy, 0);
				block_size &= SIZE_MASK;
			}
			current_id += block_size;
			if (current_id >= mg_config.virt_pages)
			{
				if (leak_log)
					CloseHandle(leak_log);
				break;
			}
		}
	}

	VirtualFree(mg_config.counts, 0, MEM_RELEASE);
	VirtualFree(mg_config.logbuf, 0, MEM_RELEASE);
	VirtualFree(mg_config.memlist, 0, MEM_RELEASE);
	VirtualFree(mg_config.stackdumps, 0, MEM_RELEASE);

	VirtualFree(mg_config.membase, (DWORD_PTR)mg_config.virt_pages * 0x1000, MEM_DECOMMIT);
	VirtualFree(mg_config.membase, 0, MEM_RELEASE);
}


__declspec(noinline) void EnterCrit()
{
	EnterCriticalSection(&mg_config.crit_sect);
}


__declspec(noinline) void LeaveCrit()
{
	LeaveCriticalSection(&mg_config.crit_sect);
}


BYTE * __stdcall CheckLogging(BYTE *ret_addr)
{
	for (BYTE **log = mg_config.logoffs; log < mg_config.logoffpos; log++)
		if (ret_addr == *log + mg_config.image_base)
			return ret_addr;

	return 0;
}


void __stdcall CopyStack(BYTE **stack_frame, UINT size, DWORD_PTR leading_number, BYTE *buffer)
{
	*(DWORD_PTR *)buffer = leading_number;
	size -= sizeof(DWORD_PTR);
	buffer += sizeof(DWORD_PTR);
	BYTE *last_byte = (BYTE *)stack_frame + size - 1;

	while (IsBadReadPtr(last_byte, 1))
		last_byte = (BYTE *)((DWORD_PTR)last_byte & ~0xFFF) - 1;

	size_t copy_size = last_byte+1 - (BYTE *)stack_frame;
	memcpy(buffer, stack_frame, copy_size);
	size -= copy_size;
	if (size)
		memset(buffer+copy_size, 0, size);
}


void WriteShortStacks()
{
	char filename[MAX_PATH] = {};
	HANDLE handle = CreateLogFile(filename, "MemCrash.bin", 0);
	if (handle == INVALID_HANDLE_VALUE)
		return;

	DWORD dummy;
#ifdef _WIN64
	// only save one 16th of the logs for now, otherwise the file size would be 4 GB
	WriteFile(handle, mg_config.stackdumps, (DWORD_PTR)mg_config.virt_pages * SMALL_STACKDUMP_SIZE/16, &dummy, 0);
#else
	WriteFile(handle, mg_config.stackdumps, (DWORD_PTR)mg_config.virt_pages * SMALL_STACKDUMP_SIZE, &dummy, 0);
#endif
	CloseHandle(handle);
}


void FlushLongStacks()
{
	UINT size = mg_config.logbufpos - mg_config.logbuf;
	if (!size)
		return;

	mg_config.logbufpos = mg_config.logbuf;

	HANDLE handle = mg_config.loghandle;
	if (!handle)
	{
		char filename[MAX_PATH] = {};
		handle = CreateLogFile(filename, "MemGuard.bin", 0);
		if (handle == INVALID_HANDLE_VALUE)
			return;

		mg_config.loghandle = handle;
	}

	DWORD dummy;
	WriteFile(handle, mg_config.logbuf, size, &dummy, 0);
}


void CloseMemGuardLog()
{
	FlushLongStacks();

	if (mg_config.loghandle)
		CloseHandle(mg_config.loghandle);
}


void __stdcall WriteLongStack(BYTE **frame, DWORD_PTR leading_number, DWORD_PTR *registers, UINT reg_size)
{
	if (!registers && !CheckLogging(*frame))
		return;

	DWORD_PTR *buffer = (DWORD_PTR *)mg_config.logbufpos;
	UINT size = mg_config.stacksize;
	mg_config.logbufpos += size;

	if (reg_size)
	{
		do { *buffer++ = *registers++; size -= sizeof(DWORD_PTR); } while (--reg_size);
		leading_number = *registers;
	}
	CopyStack(frame, size, leading_number, (BYTE *)buffer);

	if (mg_config.logbufpos == mg_config.logbufend)
		FlushLongStacks();
}


BOOL CheckMemGuardException(struct _EXCEPTION_POINTERS *exc)
{
	static void *breakpoint_addr;

	if (exc->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
	{
		DWORD_PTR addr = exc->ExceptionRecord->ExceptionInformation[1];
		UINT alloc_page = (addr - (DWORD_PTR)mg_config.membase) >> 12;
		if (alloc_page >= mg_config.virt_pages || !(mg_config.memlist[alloc_page] & FLAG_BLOCK_READONLY))
			return FALSE;

		// only one breakpoint handled at a time
		EnterCrit();

		breakpoint_addr = (void *)addr;
		DWORD dummy;
		VirtualProtect((void *)addr, 1, PAGE_READWRITE, &dummy);

		PCONTEXT ctx = exc->ContextRecord;

		BYTE *stackdump = mg_config.stackdumps + (DWORD_PTR)alloc_page * SMALL_STACKDUMP_SIZE;
		addr -= *(DWORD_PTR *)(stackdump+SMALL_STACKDUMP_SIZE);

		DWORD_PTR membreak_id = *((DWORD_PTR *)stackdump+1);
		addr -= mg_config.mbbases[membreak_id];
		
		if (addr < mg_config.mbsizes[membreak_id])
		{
			DWORD_PTR registers[INT_REGISTER_COUNT];

			for (UINT i=0; i<INT_REGISTER_COUNT; i++)
				registers[i] = *(ULONG_PTR *)((BYTE *)ctx + register_offsets[i]);

#ifdef _WIN64
			WriteLongStack((BYTE **)ctx->Rsp, 0, registers, INT_REGISTER_COUNT-1);
#else
			WriteLongStack((BYTE **)ctx->Esp, 0, registers, INT_REGISTER_COUNT-1);
#endif
		}

		ctx->EFlags |= 0x100;		// set TRAP flag

		return TRUE;
	}
	else if (exc->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP)
	{
		DWORD dummy;
		VirtualProtect(breakpoint_addr, 1, PAGE_READONLY, &dummy);
		LeaveCrit();
		return TRUE;
	}
	return FALSE;
}


BYTE ** __stdcall GetAllocFrame(BYTE **stack_pointer)
{
	BYTE *ret_addr = *stack_pointer;

#ifdef _WIN64
	// op_meminternal_malloc_leave with PGO (always inlined without PGO)
	if ((*(int *)(ret_addr + 4) == 0x840fc085 && *(int *)(ret_addr - 9) == 0x20EC8348) ||
	// _malloc_crt
		 *(int *)(ret_addr + 0x1B) == 0x03E89D8D)
		return stack_pointer + 6;
#else
	// op_meminternal_malloc_leave with PGO
	if (*(int *)(ret_addr + 2) == 0x840fc085 && *(int *)(ret_addr - 9) == 0x042474ff)
		return stack_pointer + 2;

	// op_meminternal_malloc_leave without PGO
	if (*(int *)(ret_addr + 6) == 0xe8fe6a08 && *(int *)(ret_addr - 9) == 0x082474ff)
		return stack_pointer + 3;

	// _malloc_crt
	if (*(int *)(ret_addr + 4) == 0x392775ff)
		return stack_pointer + 5;
#endif

	return stack_pointer;
}


DWORD_PTR * __stdcall CheckExclusionAndCounts(BYTE *ret_addr)
{
	for (BYTE **exclusion = mg_config.excloffs; exclusion < mg_config.excloffpos; exclusion++)
		if (ret_addr == *exclusion + mg_config.image_base)
			return 0;

	int max_counts = 0x2000;
	DWORD_PTR *cur_cnt = mg_config.counts;
	while (*cur_cnt != (DWORD_PTR)ret_addr)
	{
		if (!*cur_cnt)
		{
			*cur_cnt = (DWORD_PTR)ret_addr;
			break;
		}

		if (!--max_counts)
			return 0;

		cur_cnt += 2;
	}

	cur_cnt++;
	// Limit count of automatically guarded allocations from one place
	if (!CheckLogging(ret_addr) && *cur_cnt == mg_config.max_alloccount)
		return 0;

	(*cur_cnt)++;
	return cur_cnt;
}


BOOL __stdcall DoMalloc(size_t size, BYTE **stack_frame, void *&mem)
{
	EnterCrit();

	DWORD_PTR *count = CheckExclusionAndCounts(*stack_frame);
	if (!count)
	{
		// number of allocations per calling location exceeded, or calling location configured to be excluded
		LeaveCrit();
		return FALSE;
	}

	if (!size)
	{
		// allocation of 0 bytes is silly, but let's work around it for now
		size++;
	}

	UINT size_pages = (size + 0x1FFF) >> 12;
	struct _mg_config::mem_region* current_region = mem_regions;

	BOOL retry_other_region = FALSE;

	if (size_pages > current_region[0].pages_free)
	{
		if (size_pages > current_region[1].pages_free)
		{
			LeaveCrit();
			return FALSE;	// OOM
		}

		// won't fit in lower region, but might fit in upper region
		current_region++;
	}
	else if (size_pages <= current_region[1].pages_free)
	{
		// might fit in either region
		retry_other_region = TRUE;

		if (size_pages > 2)
			current_region++;
	}

	int *memlist = mg_config.memlist;

	while (1)
	{
		UINT current_id = current_region->next_alloc;
		UINT page_count = current_region->region_end - current_region->region_start;

		while (1)
		{
			if (current_id == current_region->region_end)
				current_id = current_region->region_start;

			UINT block_size = memlist[current_id];
			if ((int)block_size < 0)
			{
				block_size &= SIZE_MASK;
			}
			else
			{
				// free block found
				while(1)
				{
					UINT next_id = current_id + block_size;
					if (block_size >= size_pages)
					{
						size_t alloc_size = (size_pages-1) << 12;	// one guard page

						void *alloc_addr = VirtualAlloc((char *)mg_config.membase + ((DWORD_PTR)current_id << 12), alloc_size, MEM_COMMIT, PAGE_READWRITE);
						if (!alloc_addr)
						{
							LeaveCrit();
							return FALSE;
						}

						// fill with 0xCC to catch uninitialised variables
						memset(alloc_addr, 0xCC, alloc_size);

						current_region->logmem += alloc_size;
						current_region->realmem += size;

						if (block_size > size_pages)
						{
							// split bigger block
							block_size -= size_pages;
							next_id = current_id + size_pages;
							memlist[next_id] = block_size;
						}
						current_region->next_alloc = next_id;
						current_region->pages_free -= size_pages;

						memlist += current_id;
						*memlist = size_pages | FLAG_BLOCK_USED;

						alloc_addr = (char*)alloc_addr + (-(int)size & 0xFFF);
						mem = alloc_addr;
						BYTE *stackdump = mg_config.stackdumps + (DWORD_PTR)current_id * SMALL_STACKDUMP_SIZE;

						// store pointer to allocation count so that it can be decremented again when memory is freed
						*(DWORD_PTR **)stackdump = count;

						for (BYTE **mb = mg_config.membreaks; mb < mg_config.membreakpos; mb++)
							if (*stack_frame == *mb + mg_config.image_base)
							{
								*memlist |= FLAG_BLOCK_READONLY;

								// Save membreak index, to be used by the exception handler
								*((DWORD_PTR *)stackdump+1) = mb - mg_config.membreaks;

								DWORD dummy;
								VirtualProtect(alloc_addr, size, PAGE_READONLY, &dummy);
							}

						stackdump += SMALL_STACKDUMP_SIZE;
						CopyStack(stack_frame, SMALL_STACKDUMP_SIZE, (DWORD_PTR)alloc_addr, stackdump);
						WriteLongStack(stack_frame, (DWORD_PTR)alloc_addr, 0, 0);
						LeaveCrit();
						return TRUE;
					}

					// no adjacent free blocks possible if last block in region
					if (next_id == current_region->region_end)
						break;

					UINT next_block_size = memlist[next_id];
					if ((int)next_block_size < 0)
						break;

					// consolidate adjacent free blocks and try again
					block_size += next_block_size;
					memlist[current_id] = block_size;
				}
			}

			// block used or too small

			if (page_count <= block_size)
				// no matching block found in region
				break;

			current_id += block_size;
			page_count -= block_size;
		}
		if (!retry_other_region)
		{
			LeaveCrit();
			return FALSE;	// OOM
		}

		retry_other_region = FALSE;

		if (current_region->region_start == 0)
			current_region++;
		else
			current_region--;
	}
}


BOOL __stdcall DoFree(void *mem, BYTE **stack_frame)
{
	EnterCrit();

	UINT alloc_page = ((DWORD_PTR)mem - (DWORD_PTR)mg_config.membase) >> 12;
	BOOL was_guarded = alloc_page < mg_config.virt_pages;
	if (was_guarded)
	{
		int *memlist_entry = mg_config.memlist + alloc_page;
		BYTE *stackdump = mg_config.stackdumps + (DWORD_PTR)(alloc_page + 1) * SMALL_STACKDUMP_SIZE;

		if (*memlist_entry > 0 || mem != *(void **)stackdump)
		{
			// invalid pointer or double delete
			RaiseException(0xBADF00D, EXCEPTION_NONCONTINUABLE, 1, (ULONG_PTR *)&stack_frame);
			return FALSE;
		}

		struct _mg_config::mem_region* region = mem_regions;
		if (alloc_page >= region->region_end)
			region++;

		*memlist_entry &= SIZE_MASK;
		UINT block_size = *memlist_entry;

		size_t alloc_size = (block_size-1) << 12;
		region->pages_free += block_size;
		region->logmem -= alloc_size;
		region->realmem -= *(UINT *)(stackdump+8);

		VirtualFree((void *)((DWORD_PTR)mem & ~0xFFF), alloc_size, MEM_DECOMMIT);

		stackdump -= SMALL_STACKDUMP_SIZE;
		// decrease allocation counter
		(**(DWORD_PTR **)stackdump)--;

		CopyStack(stack_frame, SMALL_STACKDUMP_SIZE, (DWORD_PTR)mem, stackdump);
	}
	WriteLongStack(stack_frame, (DWORD_PTR)mem, 0, 0);

	LeaveCrit();	
	return was_guarded;
}


BOOL __stdcall DoRealloc(void *mem, size_t new_size, BYTE **stack_frame, void *&new_mem)
{
	UINT alloc_page = 0;

	if (mem)
	{
		alloc_page = ((DWORD_PTR)mem - (DWORD_PTR)mg_config.membase) >> 12;
		if (alloc_page >= mg_config.virt_pages)
			return FALSE;
	}

	new_mem = new_size ? internal_malloc(new_size, stack_frame) : 0;

	if (alloc_page)
	{
		if (new_mem)
		{
			UINT copy_size = ((mg_config.memlist[alloc_page] - 1) << 12) - 	((int)mem & 0xFFF);
			if (copy_size > new_size)
				copy_size = new_size;	
			memcpy(new_mem, mem, copy_size);
		}
		DoFree(mem, stack_frame);
	}

	return TRUE;
}


#ifndef _WIN64
__declspec(naked) void *internal_malloc(size_t size, BYTE **stack_frame)
{
	_asm mov eax, [esp+8]		// stack_frame
	_asm jmp common_malloc
}
__declspec(naked) void *MemGuard_malloc(size_t size)
{
	_asm push esp
	_asm call GetAllocFrame
}	// fall through
__declspec(naked) void *common_malloc()
{
	_asm push ecx		// make room for local variable "mem"
	_asm push esp		// &mem
	_asm push eax		// stack_frame
	_asm push [esp+16]	// size
	_asm call DoMalloc

	_asm test eax, eax
	_asm pop eax			// return value = mem
	_asm jne retrn

	_asm mov eax, mg_config.hook_functions.malloc_offset
	_asm add eax, 5
	_asm push ebp
	_asm mov ebp, esp
	_asm jmp eax			// jump to original malloc

retrn:
	_asm ret
}


__declspec(naked) void MemGuard_free(void *mem)
{
	_asm mov eax, [esp+4]
	_asm test eax, eax	// null delete?
	_asm je retrn

	_asm push esp		// stack_frame
	_asm push eax		// mem
	_asm call DoFree

	_asm test eax,eax
	_asm jne retrn

	_asm mov eax, mg_config.hook_functions.free_offset
	_asm add eax, 5
	_asm push ebp
	_asm mov ebp, esp
	_asm jmp eax			// jump to original free

retrn:
	_asm ret
}


__declspec(naked) void *MemGuard_realloc(void *mem, size_t new_size)
{
	_asm mov eax, esp

	_asm push ecx		// make room for local variable "new_mem"
	_asm push esp		// 4th argument = &new_mem
	_asm push eax
	_asm push [eax+8]	// new_size
	_asm push [eax+4]	// mem
	_asm call DoRealloc

	_asm test eax,eax
	_asm pop eax			// return value = new_mem
	_asm jne retrn

	_asm mov eax, mg_config.hook_functions.realloc_offset
	_asm add eax, 5
	_asm push ebp
	_asm mov ebp, esp
	_asm jmp eax			// jump to original realloc

retrn:
	_asm ret	
}


__declspec(naked) void MemGuard_breakpoint()
{
	_asm push esp
	_asm push ebp
	_asm push edi
	_asm push esi
	_asm push edx
	_asm push ecx
	_asm push ebx
	_asm push eax
	_asm mov esi,esp
	_asm add dword ptr [esi+1ch],8	// modify esp content to match dump
	_asm lea edx,[esi+28h]
	_asm sub dword ptr [edx],5		// adjust return address to point at the breakpoint instruction
	_asm push 7
	_asm push esi
	_asm push eax
	_asm push edx
	_asm call WriteLongStack
	_asm pop eax
	_asm pop ebx
	_asm pop ecx
	_asm pop edx
	_asm pop esi
	_asm add esp,12					// other registers haven't been modified
	_asm ret
}
#endif

void MemGuard_doexit()
{
	// runtime error - provoke CRASH!
	RaiseException(0xBADF00D, EXCEPTION_NONCONTINUABLE, 0, 0);
}
} // extern "C"

#endif // MSWIN && (DESKTOP_STARTER || PLUGIN_WRAPPER)
