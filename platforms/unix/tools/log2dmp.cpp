// Inspector log to minidump converter for Linux
// to compile: g++ -Os -s -nostartfiles -fomit-frame-pointer -o log2dmp log2dmp.c

#include <stdio.h>		// printf
#include <stddef.h>	// offsetof
#include <string.h>	// memset
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// #include <winnt.h>

typedef unsigned int  DWORD;
typedef unsigned char BYTE;
#define VER_PLATFORM_WIN32_NT 2

#define CONTEXT_i386    0x00010000
#define CONTEXT_CONTROL         (CONTEXT_i386 | 1)
#define CONTEXT_INTEGER         (CONTEXT_i386 | 2)
#define CONTEXT_SEGMENTS        (CONTEXT_i386 | 4)
#define CONTEXT_FLOATING_POINT  (CONTEXT_i386 | 8)
#define CONTEXT_FULL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS)

#define SIZE_OF_80387_REGISTERS 80

typedef struct _FLOATING_SAVE_AREA {
    DWORD   ControlWord;
    DWORD   StatusWord;
    DWORD   TagWord;
    DWORD   ErrorOffset;
    DWORD   ErrorSelector;
    DWORD   DataOffset;
    DWORD   DataSelector;
    BYTE    RegisterArea[SIZE_OF_80387_REGISTERS];
    DWORD   Cr0NpxState;
} FLOATING_SAVE_AREA;

#define MAXIMUM_SUPPORTED_EXTENSION 512

typedef struct _CONTEXT {
    DWORD ContextFlags;
    DWORD   Dr0;
    DWORD   Dr1;
    DWORD   Dr2;
    DWORD   Dr3;
    DWORD   Dr6;
    DWORD   Dr7;
    FLOATING_SAVE_AREA FloatSave;
    DWORD   SegGs;
    DWORD   SegFs;
    DWORD   SegEs;
    DWORD   SegDs;
    DWORD   Edi;
    DWORD   Esi;
    DWORD   Ebx;
    DWORD   Edx;
    DWORD   Ecx;
    DWORD   Eax;
    DWORD   Ebp;
    DWORD   Eip;
    DWORD   SegCs;
    DWORD   EFlags;
    DWORD   Esp;
    DWORD   SegSs;
    BYTE    ExtendedRegisters[MAXIMUM_SUPPORTED_EXTENSION];
} CONTEXT;

// #include <winnt.h> end

// #include <DbgHelp.h>

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int ULONG32;
typedef unsigned long long ULONG64;
typedef DWORD RVA;

#define MINIDUMP_SIGNATURE ('PMDM')
#define MINIDUMP_VERSION   (42899)

typedef enum _MINIDUMP_STREAM_TYPE {
    UnusedStream                = 0,
    ReservedStream0             = 1,
    ReservedStream1             = 2,
    ThreadListStream            = 3,
    ModuleListStream            = 4,
    MemoryListStream            = 5,
    ExceptionStream             = 6,
    SystemInfoStream            = 7,
    ThreadExListStream          = 8,
    Memory64ListStream          = 9,
    CommentStreamA              = 10,
    CommentStreamW              = 11,
    HandleDataStream            = 12,
    FunctionTableStream         = 13,
    UnloadedModuleListStream    = 14,
    MiscInfoStream              = 15,
    LastReservedStream          = 0xffff
} MINIDUMP_STREAM_TYPE;

typedef struct _MINIDUMP_LOCATION_DESCRIPTOR {
    ULONG32 DataSize;
    RVA Rva;
} MINIDUMP_LOCATION_DESCRIPTOR;

typedef struct _MINIDUMP_HEADER {
    ULONG32 Signature;
    ULONG32 Version;
    ULONG32 NumberOfStreams;
    RVA StreamDirectoryRva;
    ULONG32 CheckSum;
    union {
        ULONG32 Reserved;
        ULONG32 TimeDateStamp;
    };
    ULONG64 Flags;
} MINIDUMP_HEADER;

typedef struct _MINIDUMP_DIRECTORY {
    ULONG32 StreamType;
    MINIDUMP_LOCATION_DESCRIPTOR Location;
} MINIDUMP_DIRECTORY;

typedef struct _CPU_INFORMATION {
    ULONG32 VendorId [ 3 ];
    ULONG32 VersionInformation;
    ULONG32 FeatureInformation;
    ULONG32 AMDExtendedCpuFeatures;
} CPU_INFORMATION;

typedef struct _MINIDUMP_SYSTEM_INFO {
    USHORT ProcessorArchitecture;
    USHORT ProcessorLevel;
    USHORT ProcessorRevision;

    union {
        USHORT Reserved0;
        struct {
            UCHAR NumberOfProcessors;
            UCHAR ProductType;
        };
    };

    ULONG32 MajorVersion;
    ULONG32 MinorVersion;
    ULONG32 BuildNumber;
    ULONG32 PlatformId;

    RVA CSDVersionRva;

    union {
        ULONG32 Reserved1;
        struct {
            USHORT SuiteMask;
            USHORT Reserved2;
        };
    };

    CPU_INFORMATION Cpu;
} MINIDUMP_SYSTEM_INFO;

#define EXCEPTION_MAXIMUM_PARAMETERS 15

typedef struct _MINIDUMP_EXCEPTION  {
    ULONG32 ExceptionCode;
    ULONG32 ExceptionFlags;
    ULONG64 ExceptionRecord;
    ULONG64 ExceptionAddress;
    ULONG32 NumberParameters;
    ULONG32 __unusedAlignment;
    ULONG64 ExceptionInformation [ EXCEPTION_MAXIMUM_PARAMETERS ];
} MINIDUMP_EXCEPTION;

typedef struct MINIDUMP_EXCEPTION_STREAM {
    ULONG32 ThreadId;
    ULONG32  __alignment;
    MINIDUMP_EXCEPTION ExceptionRecord;
    MINIDUMP_LOCATION_DESCRIPTOR ThreadContext;
} MINIDUMP_EXCEPTION_STREAM;

typedef struct _MINIDUMP_MEMORY_DESCRIPTOR {
    ULONG64 StartOfMemoryRange;
    MINIDUMP_LOCATION_DESCRIPTOR Memory;
} MINIDUMP_MEMORY_DESCRIPTOR;

#define NORMAL_PRIORITY_CLASS 0x00000020

typedef struct _MINIDUMP_THREAD {
    ULONG32 ThreadId;
    ULONG32 SuspendCount;
    ULONG32 PriorityClass;
    ULONG32 Priority;
    ULONG64 Teb;
    MINIDUMP_MEMORY_DESCRIPTOR Stack;
    MINIDUMP_LOCATION_DESCRIPTOR ThreadContext;
} MINIDUMP_THREAD;

typedef struct _MINIDUMP_THREAD_LIST {
    ULONG32 NumberOfThreads;
    MINIDUMP_THREAD Threads [0];
} MINIDUMP_THREAD_LIST;

#define VS_FFI_SIGNATURE    0xFEEF04BD
#define VS_FFI_STRUCVERSION 0x00010000
#define VOS_NT_WINDOWS32    0x00040004
#define VFT_APP             0x00000001
#define VFT_DLL             0x00000002

typedef struct tagVS_FIXEDFILEINFO
{
    DWORD   dwSignature;
    DWORD   dwStrucVersion;
    DWORD   dwFileVersionMS;
    DWORD   dwFileVersionLS;
    DWORD   dwProductVersionMS;
    DWORD   dwProductVersionLS;
    DWORD   dwFileFlagsMask;
    DWORD   dwFileFlags;
    DWORD   dwFileOS;
    DWORD   dwFileType;
    DWORD   dwFileSubtype;
    DWORD   dwFileDateMS;
    DWORD   dwFileDateLS;
} VS_FIXEDFILEINFO;

typedef struct _MINIDUMP_MODULE {
    ULONG64 BaseOfImage;
    ULONG32 SizeOfImage;
    ULONG32 CheckSum;
    ULONG32 TimeDateStamp;
    RVA ModuleNameRva;
    VS_FIXEDFILEINFO VersionInfo;
    MINIDUMP_LOCATION_DESCRIPTOR CvRecord;
    MINIDUMP_LOCATION_DESCRIPTOR MiscRecord;
    ULONG64 Reserved0;
    ULONG64 Reserved1;
} MINIDUMP_MODULE;

typedef struct _MINIDUMP_MODULE_LIST {
    ULONG32 NumberOfModules;
    MINIDUMP_MODULE Modules [ 0 ];
} MINIDUMP_MODULE_LIST;

typedef unsigned short WCHAR;

typedef struct _MINIDUMP_STRING {
    ULONG32 Length;
    WCHAR   Buffer [0];
} MINIDUMP_STRING;

typedef struct _MINIDUMP_MEMORY_LIST {
    ULONG32 NumberOfMemoryRanges;
    MINIDUMP_MEMORY_DESCRIPTOR MemoryRanges [0];
} MINIDUMP_MEMORY_LIST;

// #include <DbgHelp.h> end

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

extern "C" void donothing() {}

extern "C" void _start(int argc, char *argv[])
{
	asm (
	"xor %ebp, %ebp;"
	"pop %esi;"
	"mov %esp, %ecx;"
	"push %eax;"
	"push %esp;"
	"push %edx;"
	"push $donothing;"
	"push $donothing;"
	"push %ecx;"
	"push %esi;"
	"push $main;"
	"call __libc_start_main;"
	"hlt;"
	"nop;"
	"nop;"
	);
}

int main(int argc,char *argv[])
{
	if (argc < 2)
	{
		printf("log2dmp <crash log file>\n");
		return -1;
	}
	char *in_file = argv[1];

	int log_file = open(in_file, O_RDONLY);
	if (log_file == -1)
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

	int dmp_file = open(out_file, O_WRONLY | O_CREAT | O_TRUNC);

	delete[] out_file;

	if (dmp_file == -1)
	{
		printf("Couldn't create output file %s!\n", out_file);
		close(log_file);
		return -1;
	}

	int log_size = lseek(log_file, 0, SEEK_END);
	char *log_buf = (char *)mmap(0, log_size, PROT_READ, MAP_PRIVATE, log_file, 0);
	if (log_buf != (char*)-1)
	{
		char *log_pos = log_buf;
		char *mem_buffer = NULL;
		char *modules_buffer = NULL;
		do
		{
			char *log_buf_end = log_buf + log_size;

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
			static struct __attribute__((__packed__))
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

				char *cur_pos = mod_end - 1;
				while (cur_pos > mod_name)
				{
					cur_pos--;
					if (*cur_pos == '\\' || *cur_pos == '/')
					{
						mod_name = cur_pos + 1;
						break;
					}
					*cur_pos |= 0x20;
				}

				char *dll_path = new char[mod_end - mod_name + path_end - in_file];
				if (dll_path)
				{
					char *dst = dll_path, *src = in_file;
					while (src < path_end) { *dst++ = *src++; }
					src = mod_name;
					while (src < mod_end) { *dst++ = *src++; }

					int dll_file = open(dll_path, O_RDONLY);

					if (dll_file != -1)
					{
						char header_buf[0xF8];

						if (read(dll_file, header_buf, 0x40) == 0x40 &&
							*(short *)header_buf == 'ZM' &&
							lseek(dll_file, *(int *)(header_buf + 0x3C), SEEK_SET) != -1 &&
							read(dll_file, header_buf, 0xF8) == 0xF8 &&
							*(short *)header_buf == 'EP')
						{
							mod_struc->TimeDateStamp = *(int *)(header_buf + 8);
							mod_struc->SizeOfImage = *(int *)(header_buf + 0x50);
						}

						close(dll_file);
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

			write(dmp_file, &dump, sizeof(dump));
			write(dmp_file, mem_buffer, dump.thread.Stack.Memory.DataSize);
			write(dmp_file, modules_buffer, dump.directory[3].Location.DataSize);

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
							write(dmp_file, mem_buffer, mem_buf_size);
							used_buf = 0;
							mem_buf_pos = mem_buffer;
						}
					}
				}
				log_pos = search_keyword(log_pos, log_buf_end, "\n");
			}

			if (used_buf)
				write(dmp_file, mem_buffer, used_buf);

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

			write(dmp_file, ranges_buffer, dump.directory[4].Location.DataSize);
			lseek(dmp_file, offsetof(FILE_STRUCT, directory[4].Location), SEEK_SET);
			write(dmp_file, &dump.directory[4].Location, sizeof(MINIDUMP_LOCATION_DESCRIPTOR));

			free(ranges_buffer);
		} while (0);
		free(mem_buffer);
		free(modules_buffer);
		munmap(log_buf, log_size);
	}

	close(dmp_file);
	close(log_file);
	return 0;
}
