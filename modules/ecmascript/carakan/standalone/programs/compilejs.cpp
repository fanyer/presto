/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2005-2006
 *
 * Driver for standalone ECMAScript compiler
 * Lars T Hansen
 *
 * Usage: see error message in code below.
 */

#include "core/pch.h"

#include "modules/util/tempbuf.h"
#include "modules/util/adt/bytebuffer.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "programs/ecmascript_standalone_version.h"
#define ecmascript_engine_version "futhark_1"  // FIXME

Opera* g_opera;
OpSystemInfo* g_op_system_info;
OpTimeInfo* g_op_time_info;

int main( int argc, char **argv )
{
	char memory[sizeof(Opera)];
	op_memset(memory, 0, sizeof(Opera));
	g_opera = (Opera*)&memory;
	g_opera->InitL();
	g_op_system_info = new OpSystemInfo;
	g_op_time_info = new OpTimeInfo;

    ES_Init();

#ifdef OPERETTE_JAVASCRIPT
	BOOL opt_operette = FALSE;
	BOOL opt_operettelib = FALSE;
	int  operettelib_label = 0;
#endif
	BOOL opt_use_expr = FALSE;
	BOOL opt_prune = FALSE;
	BOOL opt_quiet = FALSE;
	BOOL opt_disasm = FALSE;
	BOOL opt_cpp = FALSE;
	BOOL opt_protect_header = FALSE;
	int  opt_optimize = 1;
	BOOL opt_function = FALSE;
	const char *input_file = NULL;
	const char *prefix = NULL;
	ByteBuffer condition;

	/* Options processing */
	int arg=1;
	while ( arg < argc )
	{
		if (op_strcmp( argv[arg], "--expr" ) == 0) 
		{
			opt_use_expr = TRUE;
			opt_disasm = TRUE;
			++arg;
			continue;
		}

		if (op_strcmp( argv[arg], "--quiet" ) == 0) 
		{
			opt_quiet = TRUE;
			++arg;
			continue;
		}

		if (op_strcmp( argv[arg], "--disasm" ) == 0)
		{
			opt_disasm = TRUE;
			++arg;
			continue;
		}

		if (op_strcmp( argv[arg], "--c++" ) == 0)
		{
			opt_disasm = TRUE;
			opt_cpp = TRUE;
			++arg;
			continue;
		}

		if (op_strcmp( argv[arg], "--prune" ) == 0)
		{
			opt_prune = TRUE;
			++arg;
			continue;
		}

		if (op_strncmp( argv[arg], "--optimize=", 11 ) == 0)
		{
			if (op_strcmp( argv[arg]+11, "0" ) == 0)
				opt_optimize = 0;
			else if (op_strcmp( argv[arg]+11, "1" ) == 0)
				opt_optimize = 1;
			else if (op_strcmp( argv[arg]+11, "2" ) == 0)
				opt_optimize = 2;
			else
			{
				input_file = NULL;
				break;
			}
			++arg;
			continue;
		}

		if (op_strcmp( argv[arg], "--optimize" ) == 0)
		{
			opt_optimize = 1;
			++arg;
			continue;
		}

		if (op_strcmp( argv[arg], "--function" ) == 0)
		{
			opt_function = TRUE;
			++arg;
			continue;
		}

		if (op_strcmp( argv[arg], "--protect_header" ) == 0)
		{
			opt_protect_header = TRUE;
			++arg;
			continue;
		}

		if (op_strncmp( argv[arg], "--ifdef=", 8 ) == 0)
		{
			if (condition.Length() > 0)
				condition.AppendString(" && ");
			condition.AppendString("defined ");
			condition.AppendString(argv[arg]+8);
			++arg;
			continue;
		}

		if (op_strncmp( argv[arg], "--prefix=", 9 ) == 0)
		{
			prefix = argv[arg]+9;
			++arg;
			continue;
		}

#ifdef OPERETTE_JAVASCRIPT

		if (strcmp( argv[arg], "--operette" ) == 0)
		{
			opt_operette = TRUE;
			++arg;
			continue;
		}
		
		if (strcmp( argv[arg], "--operettelib" ) == 0)
		{
			opt_operettelib = TRUE;
			++arg;
			if (arg == argc || sscanf(argv[arg], "%d", &operettelib_label) != 1)
			{
				input_file = NULL;
				break;
			}
			++arg;
			continue;
		}

#endif

		if (input_file != NULL)
		{
			input_file = NULL;	// triggers error below
			break;
		}
		input_file = argv[arg];
		++arg;
	}

	if (!opt_quiet)
	{
		fprintf( stderr, "\nOpera JavaScript compiler\n" );
		fprintf( stderr, "Copyright (C) Opera Software ASA  1995-2006\n\n" );

		fprintf( stderr, "FOR OPERA INTERNAL USE ONLY -- DO NOT DISTRIBUTE EXTERNALLY!\n\n" );

		fprintf( stderr, "Driver:   %s\n", ecmascript_standalone_version );
		fprintf( stderr, "Compiler: %s\n", ecmascript_engine_version );
		fprintf( stderr, "Bytecode: %d.%d\n", BYTECODE_MAJOR_VERSION, BYTECODE_MINOR_VERSION );
		fprintf( stderr, "OMF:      %d.%d\n\n", OMF_MAJOR_VERSION, OMF_MINOR_VERSION );
		fflush( stderr );
	}

	if (input_file == NULL)
	{
		fprintf( stderr, "Usage: compilejs [options] filename\n" );
		fprintf( stderr, "       compilejs [options] \"expression\"\n\n" );
		fprintf( stderr, "Without options, compile JavaScript source ('.js') to Opera bytecodes\n" );
		fprintf( stderr, "on OPERA-OMF format and store the output in a file of type '.jsobj'.\n\n" );
		fprintf( stderr, "Options:\n" );
		fprintf( stderr, "\n   --expr            Use expression instead of file.\n" );
		fprintf( stderr, "\n   --quiet           Suppress banners etc.\n" );
		fprintf( stderr, "\n   --disasm          Produce symbolic disassembly in '.jss' file\n" );
		fprintf( stderr, "\n   --c++             Produce C++ format disassembly in '.cpp' file\n" );
		fprintf( stderr, "\n   --ifdef=<name>    #ifdef generated C++ code on <name>\n" );
		fprintf( stderr, "\n   --optimize[=<n>]  Set optimization level at <n>: 0, 1 (default), or 2\n" );
		fprintf( stderr, "\n   --function        Compile as function body, ie, allow top-level \"return\"" );
		fprintf( stderr, "\n   --prefix=<name>   Use <name> as public prefix rather than \"es_library\"\n" );
		fprintf( stderr, "\n   --protect_header  Include the ecmascript header inside the #ifdefs\n" );
		fprintf( stderr, "\n   --prune           Remove trailing NOPs from disassembled output\n" );
#ifdef OPERETTE_JAVASCRIPT
		fprintf( stderr, "\n   --operette        Translate instructions to the Operette instruction set\n" );
		fprintf( stderr, "                     and produce OPERA-JES object module format.\n");
		fprintf( stderr, "\n   --operettelib k   Translate instructions to the Operette instruction set\n" );
		fprintf( stderr, "                     and produce a Java source fragment, with case labels\n" );
		fprintf( stderr, "                     for nested functions starting with the value 'k'.\n" );
#endif
		return 1;
	}

	char output_file[FILENAME_MAX+1];

	if (!opt_use_expr)
	{
		if (op_strlen(input_file)+1 >= sizeof(output_file))
		{
			fprintf( stderr, "ERROR: Too long filename %s\n", input_file );
			return 1;
		}

		op_strcpy( output_file, input_file );
		char *ext  = op_strrchr(output_file, '.');
		char *psc  = op_strrchr(output_file, '/');
		char *psc2 = op_strrchr(output_file, '\\');
		char *psc3 = op_strrchr(output_file, ':');

		/* Merge psc2 and psc3 into psc */
		if (psc == NULL) psc = psc2;
		if (psc == NULL) psc = psc3;

		if (psc != NULL && psc2 != NULL && psc < psc2) psc = psc2;	// Prefer later of \ and /
		if (psc != NULL && psc3 != NULL && psc < psc3) psc = psc3;	// Prefer later of \, /, and :

		/* Replace or append extension, as the case may be */
		if (ext != NULL && (psc == NULL || ext > psc))
			*ext = 0;

		if (op_strlen(output_file) + 7 >= sizeof(output_file))
		{
			fprintf( stderr, "ERROR: Output filename would be too long\n");
			return 1;
		}

		if (opt_disasm)
		{
			if (opt_cpp)
				op_strcat( output_file, ".cpp" );
			else
				op_strcat( output_file, ".jss" );
		}
		else
			op_strcat( output_file, ".jsobj" );
	}

	/* Process input */
	TempBuffer text;

	if (opt_use_expr)
	{
		text.Append(input_file);
	} 
	else
	{
		if (!read_file(input_file, &text))
		{
			fprintf( stderr, "ERROR: Can't  read file\n" );
			return 1;
		}
	}

	ByteBuffer external_representation;

	OP_STATUS r = OpStatus::OK;

	ES_CompilerOptions opt;
	opt.disassemble = opt_disasm;
	opt.function_body = opt_function;
	opt.optimize_level = opt_optimize;
	opt.cplusplus = opt_cpp;
	opt.protect_header = opt_protect_header;
	opt.prune = opt_prune;
	if (condition.Length() > 0)
		opt.condition = condition.Copy(TRUE);
	opt.prefix = prefix;

#ifdef OPERETTE_JAVASCRIPT
	if (opt_operette || opt_operettelib)
	{
		r = ES_CompileToJavaRepresentation( text.GetStorage(), text.Length(), &external_representation, opt_operettelib, operettelib_label, &opt );
	}
	else
#endif
	{
		r = ES_CompileToExternalRepresentation( text.GetStorage(), text.Length(), &external_representation, &opt );
	}

	if (OpStatus::IsError(r))
	{
		fprintf( stderr, "ERROR: Compilation failed.\n" );
		return 1;
	}

	/* Produce output */
	if (opt_use_expr)
	{
		for ( int nchunks=external_representation.GetChunkCount(), i=0 ; i < nchunks ; i++ )
		{
			unsigned int nbytes;
			char *chunk = external_representation.GetChunk(i, &nbytes);
			fwrite( chunk, 1, nbytes, stderr );
		}

		if (!opt_quiet)
			fprintf( stderr, "Done.\n" );
	} 
	else
	{
		FILE *output;

#ifdef UNIX
		output = fopen(output_file, "wb");
#else
		fopen_s(&output, output_file, "wb");
#endif
		if (output == NULL)
		{
			fprintf( stderr, "ERROR: Can't create %s\n", output_file );
			return 1;
		}

		for ( int nchunks=external_representation.GetChunkCount(), i=0 ; i < nchunks ; i++ )
		{
			unsigned int nbytes;
			char *chunk = external_representation.GetChunk(i, &nbytes);
			fwrite( chunk, 1, nbytes, output );
		}

		fclose( output );

		if (!opt_quiet)
			fprintf( stderr, "Generated %s\nDone.\n", output_file );
	}

	return 0;
}

