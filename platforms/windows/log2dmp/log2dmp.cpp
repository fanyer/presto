#include <stdio.h>		// printf
#include <stddef.h>		// offsetof
#include <windows.h>
#include <DbgHelp.h>

struct module
{
	~module() { delete[] file_name; delete[] version;}
	char *file_name;
	char *version;
	unsigned int base_addr;
	module *next;
};

struct module_list
{
	module_list() : first(NULL), count(0), total_file_name_size(0) {}
	~module_list()
	{
		module *runner = first;
		while (runner)
		{
			module *current = runner;
			runner = runner->next;
			delete current;
		}
	}
	module *first;
	unsigned int count;
	unsigned int total_file_name_size;
};

struct byte_range
{
	byte_range(unsigned int start) : next(NULL), range_length(0) { range_start = start; }
	unsigned int range_start;
	unsigned int range_length;
	byte_range *next;
};

struct byte_range_list
{
	byte_range_list() : first(NULL), count(0) {}
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
	int count;
};


char *search_keyword(char *buf, char *buf_end, char *keyword)
{
	char *search_pos = keyword;

	while (buf < buf_end)
	{
		if (*buf == 0xD)
			buf++;
		else if (*search_pos == *buf++)
		{
			search_pos++;
			if (!*search_pos)
				return buf;
		}
		else
			search_pos = keyword;
	}
	return NULL;
}


bool get_hex_after_keyword(char *&buf, char *buf_end, char *keyword, unsigned int *hex_val, unsigned int digits = 8)
{
	char *hex_pos = buf;

	if (keyword)
	{
		hex_pos = search_keyword(hex_pos, buf_end, keyword);
		if (!hex_pos)
			return false;
	}

	while (hex_pos < buf_end && *hex_pos <= ' ') hex_pos++;
	buf = hex_pos;

	unsigned int val = 0;

	while (hex_pos < buf_end && digits)
	{
		unsigned char ch = *hex_pos - '0';
		if (ch > 9)
		{
			ch = (*hex_pos & 0xDF) - 'A';
			if (ch > 5)
				break;
			ch += 10;
		}
		val = (val<<4) | ch;
		digits--;
		hex_pos++;
	}
	*hex_val = val;
	if (buf == hex_pos)
		return false;

	buf = hex_pos;
	return true;
}


void add_module(module_list &modules, char *buf, char *buf_end)
{
	char *cur_pos = buf;
	char *base_pos = search_keyword(cur_pos, buf_end, "Base:");
	if (!base_pos)
		return;

	base_pos -= sizeof("Base:")-1;

	while (cur_pos < base_pos)
	{
		// search for file extension
		if (*cur_pos == '.')
		{
			char *end_candidate = cur_pos + 4;
			if (*end_candidate == ' ')
			{
				cur_pos = buf;
				int file_name_size = end_candidate - cur_pos + 1;
				char *file_name = new char[file_name_size];
				if (!file_name)
					return;

				module tmp_module;
				tmp_module.file_name = file_name;

				while (cur_pos < end_candidate) *file_name++ = *cur_pos++;
				*file_name = 0;

				cur_pos = end_candidate;
				do { cur_pos++; } while (*cur_pos == ' ');

				if (cur_pos < base_pos)
				{
					if (!modules.first)
					{
						// main executable
						end_candidate = search_keyword(cur_pos, base_pos, "caused");
						if (!end_candidate)
							end_candidate = base_pos;
						else
							end_candidate -= sizeof("caused")-1;
					}
					else
					{
						// Used DLLs
						end_candidate = base_pos;
					}

					while (end_candidate[-1] == ' ') { end_candidate--; }

					tmp_module.version = new char[end_candidate - cur_pos + 1];
					if (!tmp_module.version)
						return;

					char *dst_pos = tmp_module.version;
					while (cur_pos < end_candidate) *dst_pos++ = *cur_pos++;
					*dst_pos = 0;

				}
				else
					tmp_module.version = NULL;

				base_pos += sizeof("Base:")-1;
				if (!get_hex_after_keyword(base_pos, buf_end, 0, &tmp_module.base_addr))
					return;

				module *new_module = new module;
				if (!new_module)
					return;

				*new_module = tmp_module;
				tmp_module.file_name = NULL;
				tmp_module.version = NULL;

				module *next_mod = modules.first;
				module *current_mod = NULL;

				while (next_mod)
				{
					if (new_module->base_addr < next_mod->base_addr)
						break;

					current_mod = next_mod;
					next_mod = current_mod->next;
				};

				if (!current_mod)
					modules.first = new_module;
				else
					current_mod->next = new_module;

				new_module->next = next_mod;

				modules.count++;
				modules.total_file_name_size += (file_name_size*2) + 4;
				return;
			}
		}
		cur_pos++;
	}
}


int main(int argc,char *argv[])
{
	if (argc < 2)
	{
		printf("log2dmp <crash log file>\n");
		return -1;
	}
	char *in_file = argv[1];

	HANDLE log_file = CreateFileA(in_file, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if (log_file == INVALID_HANDLE_VALUE)
	{
		printf("Couldn't open input file %s!\n", in_file);
		return -1;
	}

	char *file_end = in_file;
	while (*file_end) file_end++;

	char *cur_pos = file_end, *ext_pos = file_end, *path_end = in_file;

	while (cur_pos > in_file)
	{
		cur_pos--;
		if (*cur_pos == '\\' || *cur_pos == '/')
		{
			path_end = cur_pos + 1;
			break;
		}
		if (*cur_pos == '.' && ext_pos == file_end)
			ext_pos = cur_pos;
	}

	char *out_file = new char[ext_pos - in_file + 5];
	if (!out_file)
		return -1;

	char *src = in_file, *dst = out_file;
	while (src < ext_pos) *dst++ = *src++;

	*(int *)dst = 'pmd.';
	dst[4] = 0;

	HANDLE dmp_file = CreateFileA(out_file, GENERIC_WRITE, 0, 0, CREATE_ALWAYS /*CREATE_NEW*/, 0, 0);

	delete[] out_file;
	
	if (dmp_file == INVALID_HANDLE_VALUE)
	{
		printf("Couldn't create output file %s!\n", out_file);
		CloseHandle(log_file);
		return -1;
	}

	HANDLE map = CreateFileMappingA(log_file, 0, PAGE_READONLY, 0, 0, 0);
	if (map)
	{
		char *log_buf = (char *)MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
		if (log_buf)
		{
			char *log_pos = log_buf;
			char *mem_buffer = NULL;
			char *modules_buffer = NULL;
			do
			{
				char *log_buf_end = log_buf + GetFileSize(log_file, NULL);

				#define number_of_streams 5

				struct FILE_STRUCT
				{
					MINIDUMP_HEADER header;
					MINIDUMP_DIRECTORY directory[number_of_streams];
					MINIDUMP_SYSTEM_INFO sys_info;
					MINIDUMP_EXCEPTION_STREAM exc_info;
					CONTEXT context;
					ULONG32 NumberOfThreads;	// MINIDUMP_THREAD_LIST
					MINIDUMP_THREAD thread;
				} dump;

				memset(&dump, 0, sizeof(dump));

				dump.header.Signature = MINIDUMP_SIGNATURE;
				dump.header.Version = MINIDUMP_VERSION;
				dump.header.NumberOfStreams = number_of_streams;
				dump.header.StreamDirectoryRva = offsetof(FILE_STRUCT, directory);

				// SystemInfoStream

				dump.directory[0].StreamType = SystemInfoStream;
				dump.directory[0].Location.Rva = offsetof(FILE_STRUCT, sys_info);
				dump.directory[0].Location.DataSize = sizeof(dump.sys_info);

				dump.sys_info.ProcessorLevel = 6;
				dump.sys_info.NumberOfProcessors = 1;
				dump.sys_info.MajorVersion = 5;
				dump.sys_info.PlatformId = VER_PLATFORM_WIN32_NT;

				// ExceptionStream

				dump.directory[1].StreamType = ExceptionStream;		// optional
				dump.directory[1].Location.Rva = offsetof(FILE_STRUCT, exc_info);
				dump.directory[1].Location.DataSize = sizeof(dump.exc_info);

				if (!get_hex_after_keyword(log_pos, log_buf_end, "exception", (unsigned int *)&dump.exc_info.ExceptionRecord.ExceptionCode))
					break;

				if (!get_hex_after_keyword(log_pos, log_buf_end, "address", (unsigned int *)&dump.exc_info.ExceptionRecord.ExceptionAddress))
					break;

				dump.exc_info.ThreadContext.Rva = offsetof(FILE_STRUCT, context);
				dump.exc_info.ThreadContext.DataSize = sizeof(dump.context);
				dump.exc_info.ThreadId = dump.thread.ThreadId = 0xDEAD;
				dump.context.ContextFlags = CONTEXT_FULL | CONTEXT_FLOATING_POINT;

				#define float_reg(x) offsetof(CONTEXT, FloatSave.x)
				#define cpu_reg(x) offsetof(CONTEXT, x)
				static struct
				{
					unsigned char offset;
					char *keyword;
				}
				register_offsets[] =
				{
					cpu_reg(Eax), "EAX=", cpu_reg(Ebx), "EBX=", cpu_reg(Ecx), "ECX=", cpu_reg(Edx), "EDX=",
					cpu_reg(Esi), "ESI=", cpu_reg(Edi), "EDI=", cpu_reg(Ebp), "EBP=", cpu_reg(Esp), "ESP=",
					cpu_reg(Eip), "EIP=", cpu_reg(EFlags), "FLAGS=", cpu_reg(SegCs), "CS=", cpu_reg(SegDs), "DS=",
					cpu_reg(SegSs), "SS=", cpu_reg(SegEs), "ES=", cpu_reg(SegFs), "FS=", cpu_reg(SegGs),"GS=", 

					(float_reg(RegisterArea)+78) | 1, "FPU stack:\n", float_reg(RegisterArea)+74, 0, float_reg(RegisterArea)+70, 0,
					(float_reg(RegisterArea)+68) | 1, " ", float_reg(RegisterArea)+64, 0, float_reg(RegisterArea)+60, 0,
					(float_reg(RegisterArea)+58) | 1, " ", float_reg(RegisterArea)+54, 0, float_reg(RegisterArea)+50, 0,
					(float_reg(RegisterArea)+48) | 1, "\n", float_reg(RegisterArea)+44, 0, float_reg(RegisterArea)+40, 0,
					(float_reg(RegisterArea)+38) | 1, " ", float_reg(RegisterArea)+34, 0, float_reg(RegisterArea)+30, 0,
					(float_reg(RegisterArea)+28) | 1, " ", float_reg(RegisterArea)+24, 0, float_reg(RegisterArea)+20, 0,
					(float_reg(RegisterArea)+18) | 1, "\n", float_reg(RegisterArea)+14, 0, float_reg(RegisterArea)+10, 0,
					(float_reg(RegisterArea)+ 8) | 1, " ", float_reg(RegisterArea)+4,  0, float_reg(RegisterArea), 0,
					float_reg(StatusWord) | 1, "SW=", float_reg(ControlWord) | 1, "CW="
				};

				for (int i=0; i<sizeof(register_offsets)/sizeof(register_offsets[0]); i++)
				{
					unsigned int offset = register_offsets[i].offset, hex_val;
					if (!get_hex_after_keyword(log_pos, log_buf_end, register_offsets[i].keyword, &hex_val, (offset & 1) ? 4 : 8))
						break;

					char *reg = (char *)&dump.context + (offset & ~1);
					if (offset & 1)
						*(unsigned short *)reg = hex_val;
					else
						*(unsigned int *)reg = hex_val;
				}

				// ThreadListStream

				dump.directory[2].StreamType = ThreadListStream;
				dump.directory[2].Location.Rva = offsetof(FILE_STRUCT, NumberOfThreads);
				dump.directory[2].Location.DataSize = sizeof(MINIDUMP_THREAD_LIST)+sizeof(MINIDUMP_THREAD);

				byte_range_list memory_ranges;
				memory_ranges.first = new byte_range(0);
				if (!memory_ranges.first)
					break;
				memory_ranges.count++;

				unsigned int stack_start;
				int mem_buf_size = 0x2000;
				if (get_hex_after_keyword(log_pos, log_buf_end, "Stack dump:\n", &stack_start))
				{
					mem_buffer = (char *)malloc(mem_buf_size);
					if (!mem_buffer)
						break;

					int stack_count = 0;
					char *stack_pos = mem_buffer;
					
					while (1)
					{
						log_pos += 2;
						if (log_pos >= log_buf_end || *log_pos <= ' ' ||
							!get_hex_after_keyword(log_pos, log_buf_end, 0, (unsigned int *)stack_pos))
							break;

						stack_pos += 4;

						if (++stack_count == 4)
						{
							int stack_offset;
							if (!get_hex_after_keyword(log_pos, log_buf_end, "\n", (unsigned int *)&stack_offset))
								break;

							stack_offset -= stack_start;
							char *new_stack_pos = mem_buffer + stack_offset;

							// check if stack position makes sense
							if (new_stack_pos < stack_pos || new_stack_pos > stack_pos + 0x10000)
								break;

							if (stack_offset + 0x10 > mem_buf_size)
							{
								mem_buf_size = stack_offset + 0x2000;
								mem_buffer = (char *)realloc(mem_buffer, mem_buf_size);
								if (!mem_buffer)
									break;
							}

							stack_pos = new_stack_pos;
							stack_count = 0;
						}
					}

					*(unsigned int *)&dump.thread.Stack.StartOfMemoryRange = memory_ranges.first->range_start = stack_start;
					dump.thread.Stack.Memory.DataSize = memory_ranges.first->range_length = stack_pos - mem_buffer;
				}

				dump.thread.Stack.Memory.Rva = sizeof(dump);
				dump.NumberOfThreads = 1;
				dump.thread.PriorityClass = NORMAL_PRIORITY_CLASS;
				dump.thread.ThreadContext.Rva = offsetof(FILE_STRUCT, context);
				dump.thread.ThreadContext.DataSize = sizeof(dump.context);

				// ModuleListStream

				dump.directory[3].StreamType = ModuleListStream;
				dump.directory[3].Location.Rva = dump.thread.Stack.Memory.Rva + dump.thread.Stack.Memory.DataSize;

				module_list modules;
				log_pos = log_buf;

				char *next_line = search_keyword(log_pos, log_buf_end, "\n");
				if (next_line && search_keyword(log_pos, next_line, "OPERA-CRASHLOG "))
					log_pos = next_line;

				add_module(modules, log_pos, log_buf_end);

				log_pos = search_keyword(log_pos, log_buf_end, "Used DLLs:\n");
				
				while (log_pos && log_pos < log_buf_end && *log_pos > 0x20)
				{
					char *new_pos = search_keyword(log_pos, log_buf_end, "\n");
					add_module(modules, log_pos, new_pos ? new_pos : log_buf_end);
					log_pos = new_pos;
				}

				int names_offset = sizeof(MINIDUMP_MODULE_LIST) + sizeof(MINIDUMP_MODULE) * modules.count;
				dump.directory[3].Location.DataSize = names_offset + modules.total_file_name_size;
				char *modules_buffer = (char *)malloc(dump.directory[3].Location.DataSize);

				if (!modules_buffer)
					break;

				memset(modules_buffer, 0, dump.directory[3].Location.DataSize);

				MINIDUMP_MODULE_LIST *mod_list = (MINIDUMP_MODULE_LIST *)modules_buffer;
				mod_list->NumberOfModules = modules.count;
				MINIDUMP_MODULE *mod_struc = mod_list->Modules;
				MINIDUMP_STRING *name_str = (MINIDUMP_STRING *)(modules_buffer + names_offset);
				
				module *mod = modules.first;

				while (mod)
				{
					*(int *)&mod_struc->BaseOfImage = mod->base_addr;
					mod_struc->ModuleNameRva = sizeof(dump) + dump.thread.Stack.Memory.DataSize + (char *)name_str-modules_buffer;

					WCHAR *dst_pos = name_str->Buffer;
					char cur_char, *src_pos = mod->file_name;

					while (*src_pos) { *dst_pos++ = *src_pos++; }
					name_str->Length = (char *)dst_pos - (char *)name_str->Buffer;
					name_str = (MINIDUMP_STRING *)(dst_pos + 1);

					mod_struc->VersionInfo.dwSignature = VS_FFI_SIGNATURE;
					mod_struc->VersionInfo.dwStrucVersion = VS_FFI_STRUCVERSION;
					mod_struc->VersionInfo.dwFileOS = VOS_NT_WINDOWS32;
					mod_struc->VersionInfo.dwFileType = (mod == modules.first) ? VFT_APP : VFT_DLL;

					char *mod_ver = mod->version;
					if (mod_ver)
					{
						unsigned int ver_numbers[4];
						int ver_pos;
						for (ver_pos = 0; ver_pos < 4;)
						{
							unsigned int cur_num = 0;
							unsigned char cur_digit;

							while ((cur_digit = *mod_ver-'0') <= 9)
							{
								cur_num = cur_num * 10 + cur_digit;
								mod_ver++;
							}

							ver_numbers[ver_pos++] = cur_num;
							if (*mod_ver++ != '.')
								break;
						}

						DWORD ver_MS, ver_LS;

						ver_MS = ver_numbers[0] << 16;

						if (ver_pos <= 1)			// only build number
						{
							ver_LS = ver_MS;
							ver_MS = 0;
						}
						else 
						{
							ver_MS |= ver_numbers[1];
							ver_LS = ver_pos > 2 ? ver_numbers[2] : 0;
							if (ver_pos > 3)	// least significant version is two numbers
								ver_LS = ver_LS << 16 | ver_numbers[3];
						}

						mod_struc->VersionInfo.dwFileVersionMS = mod_struc->VersionInfo.dwProductVersionMS = ver_MS;
						mod_struc->VersionInfo.dwFileVersionLS = mod_struc->VersionInfo.dwProductVersionLS = ver_LS;
					}

					module *next_mod = mod->next;
					if (!next_mod)
						mod_struc->SizeOfImage = 32*1024*1024;
					else
					{
						unsigned int mod_size = next_mod->base_addr - mod->base_addr;
						mod_struc->SizeOfImage = mod_size < 32*1024*1024 ? mod_size : 32*1024*1024;
					}

					char *mod_name = mod->file_name;

					char *mod_end = mod_name;
					while (*mod_end++);

					char *cur_pos = mod_end;
					while (cur_pos > mod_name)
					{
						cur_pos--;
						if (*cur_pos == '\\' || *cur_pos == '/')
						{
							mod_name = cur_pos + 1;
							break;
						}
					}

					char *dll_path = new char[mod_end - mod_name + path_end - in_file];
					if (dll_path)
					{
						char *dst = dll_path, *src = in_file;
						while (src < path_end) { *dst++ = *src++; }
						src = mod_name;
						while (src < mod_end) { *dst++ = *src++; }

						HANDLE dll_file = CreateFileA(dll_path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

						if (dll_file != INVALID_HANDLE_VALUE)
						{
							DWORD bytes_read;
							char header_buf[0xF8];

							if (ReadFile(dll_file, header_buf, 0x40, &bytes_read, 0) &&
								bytes_read == 0x40 &&
								*(short *)header_buf == 'ZM' &&
								SetFilePointer(dll_file, *(int *)(header_buf + 0x3C), 0, FILE_BEGIN) != 0xFFFFFFFF &&
								ReadFile(dll_file, header_buf, 0xF8, &bytes_read, 0) &&
								bytes_read == 0xF8 &&
								*(short *)header_buf == 'EP')
							{
								mod_struc->TimeDateStamp = *(int *)(header_buf + 8);
								mod_struc->SizeOfImage = *(int *)(header_buf + 0x50);
							}

							CloseHandle(dll_file);
						}
						else
							printf("Couldn't find %s next to the log file, it will not be possible to\nload symbols for this module when opening the .dmp file in Visual Studio!\n", mod_name);

						delete[] dll_path;
					}

					mod_struc++;
					mod = mod->next;
				}

				// MemoryListStream

				dump.directory[4].StreamType = MemoryListStream;	// optional
				dump.directory[4].Location.Rva = dump.directory[3].Location.Rva + dump.directory[3].Location.DataSize;

				DWORD bytes_written;
				WriteFile(dmp_file, &dump, sizeof(dump), &bytes_written, 0);
				WriteFile(dmp_file, mem_buffer, dump.thread.Stack.Memory.DataSize, &bytes_written, 0);
				WriteFile(dmp_file, modules_buffer, dump.directory[3].Location.DataSize, &bytes_written, 0);

				log_pos = search_keyword(log_buf, log_buf_end, "Memory dumps:\n");

				byte_range *cur_range = memory_ranges.first;
				char *mem_buf_pos = mem_buffer;
				int used_buf = 0;

				while (log_pos)
				{
					unsigned int line_addr;
					if (get_hex_after_keyword(log_pos, log_buf_end, 0, &line_addr))
					{
						if (line_addr - (cur_range->range_start + cur_range->range_length) > 0x10)
						{
							byte_range *new_range = new byte_range(line_addr);
							if (!new_range)
								break;

							cur_range->next = new_range;
							cur_range = new_range;
							memory_ranges.count++;
						}
						for (int bytecount = 0; bytecount < 16; bytecount++)
						{
							unsigned int mem_temp;
							if (log_pos+2 >= log_buf_end || log_pos[2] <= ' ' ||
								!get_hex_after_keyword(log_pos, log_buf_end, 0, &mem_temp, 2))
								break;

							*mem_buf_pos++ = mem_temp;
							cur_range->range_length++;
							if (++used_buf == mem_buf_size)
							{
								WriteFile(dmp_file, mem_buffer, mem_buf_size, &bytes_written, 0);
								used_buf = 0;
								mem_buf_pos = mem_buffer;
							}
						}
					}
					log_pos = search_keyword(log_pos, log_buf_end, "\n");
				}

				if (used_buf)
					WriteFile(dmp_file, mem_buffer, used_buf, &bytes_written, 0);

				dump.directory[4].Location.DataSize = sizeof(MINIDUMP_MEMORY_LIST) + sizeof(MINIDUMP_MEMORY_DESCRIPTOR) * memory_ranges.count;
				char *ranges_buffer = (char *)malloc(dump.directory[4].Location.DataSize);
				if (!ranges_buffer)
					break;

				MINIDUMP_MEMORY_LIST *mem_list = (MINIDUMP_MEMORY_LIST *)ranges_buffer;
				mem_list->NumberOfMemoryRanges = memory_ranges.count;
				MINIDUMP_MEMORY_DESCRIPTOR *mem_desc = mem_list->MemoryRanges;

				cur_range = memory_ranges.first;
				do
				{
					mem_desc->StartOfMemoryRange = cur_range->range_start;
					mem_desc->Memory.DataSize = cur_range->range_length;
					if (cur_range == memory_ranges.first)
					{
						mem_desc->Memory.Rva = dump.thread.Stack.Memory.Rva;
					}
					else
					{
						mem_desc->Memory.Rva = dump.directory[4].Location.Rva;
						dump.directory[4].Location.Rva += cur_range->range_length;
					}
					mem_desc++;
					cur_range = cur_range->next;
				}
				while (cur_range);

				WriteFile(dmp_file, ranges_buffer, dump.directory[4].Location.DataSize, &bytes_written, 0);
				SetFilePointer(dmp_file, offsetof(FILE_STRUCT, directory[4].Location), NULL, FILE_BEGIN);
				WriteFile(dmp_file, &dump.directory[4].Location, sizeof(MINIDUMP_LOCATION_DESCRIPTOR), &bytes_written, 0);

				free(ranges_buffer);
			} while (0);
			free(mem_buffer);
			free(modules_buffer);
			UnmapViewOfFile(log_buf);
		}
		CloseHandle(map);
	}
	CloseHandle(dmp_file);
	CloseHandle(log_file);
	return 0;
}
