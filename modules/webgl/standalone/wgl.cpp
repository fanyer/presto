/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- standalone shell.
 *
 */
#include "core/pch.h"
#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_context.h"
#include "modules/webgl/src/wgl_lexer.h"
#include "modules/webgl/src/wgl_parser.h"
#include "modules/webgl/src/wgl_printer_stdio.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_builder.h"
#include "modules/webgl/src/wgl_cgbuilder.h"
#include "modules/webgl/src/wgl_validate.h"
#include "modules/webgl/src/wgl_validator.h"

#include "modules/util/simset.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>

#ifndef WIN32
# include <sys/time.h>
# include <sys/resource.h>
# include <sys/errno.h>
#endif // !WIN32

Opera* g_opera;
OpSystemInfo* g_op_system_info;
OpTimeInfo *g_op_time_info;

extern "C"
void Debug_OpAssert(const char* expression, const char* file, int line)
{
    printf("ASSERT FAILED: OP_ASSERT(%s) %s:%d\n",expression, file, line);
#ifdef UNIX
    raise(SIGABRT);
#else
    assert(0);
#endif // UNIX
}

static OP_STATUS
read_file(uni_char *&buf, unsigned int &size, char *file_name)
{
    struct stat st;
    if (stat(file_name, &st) < 0)
        return OpStatus::ERR;

    int len = st.st_size;
    buf = OP_NEWA(uni_char, (st.st_size + 1));
    if (!buf)
        return OpStatus::ERR_NO_MEMORY;

    FILE *f = fopen(file_name, "rb");
    if (!f)
    {
        OP_DELETEA(buf);
        return OpStatus::ERR;
    }
    while (len > 0)
    {
        int l;
        if ((l = fread(buf+(st.st_size - len), sizeof(unsigned char), len, f)) <= 0)
        {
            OP_DELETEA(buf);
            fclose(f);
            return OpStatus::ERR;
        }
        len -= l;
    }
    fclose(f);

    make_doublebyte_in_place(buf, st.st_size-1);
    size = st.st_size;
    return OpStatus::OK;
}

/** Ad-hoc class holding the options supported by the standalone test
 *  harness. Correspondence to command-line options (and meaning)
 *  should be easily derivable from the names.
 */
class WGL_Standalone_Options
{
public:
    WGL_Standalone_Options()
        : quiet(FALSE)
        , debug(FALSE)
        , codegen(FALSE)
        , validate(TRUE)
        , show_vars(FALSE)
        , have_vs(FALSE)
        , have_fs(FALSE)
        , vs_file("")
        , fs_file("")
        , out_file("")
        , for_directx_version(10)
        , for_gl_es(FALSE)
        , for_highp(TRUE)
        , support_oes(FALSE)
        , for_version(100)
    {
    }

    BOOL quiet;
    BOOL debug;
    BOOL codegen;
    BOOL validate;
    BOOL show_vars;

    BOOL have_vs;
    BOOL have_fs;

    const char *vs_file;
    const char *fs_file;
    const char *out_file;

    unsigned for_directx_version;

    BOOL for_gl_es;
    BOOL for_highp;
    BOOL support_oes;
    unsigned for_version;
};

class OpAutoContext
{
public:
    OpAutoContext(WGL_Context *context)
        : context(context)
    {
    }

    ~OpAutoContext()
    {
        if (context)
            context->Release();
    }

private:
    WGL_Context *context;
};

// Returns true if the program parses and validates, otherwise false.
BOOL
ProcessSourceL(WGL_Context *context, WGL_PrettyPrinter *printer, WGL_DeclList *&shader_decls, WGL_ShaderVariables *vars, const uni_char *source, unsigned length, BOOL for_vertex, WGL_Standalone_Options &opts)
{
    WGL_ProgramText vs_input[1];
    vs_input[0].program_text = const_cast<uni_char *>(source);
    vs_input[0].program_text_length = length;

    WGL_Lexer *lexer;
    WGL_Lexer::MakeL(lexer, context);
    lexer->ConstructL(vs_input, ARRAY_SIZE(vs_input), for_vertex);

    WGL_Error::ReportErrorsL(printer->printer, lexer->GetFirstError());
    if (lexer->HaveErrors())
        return FALSE;

    WGL_Parser *parser;
    WGL_Parser::MakeL(parser, context, lexer, for_vertex, opts.codegen);

    shader_decls = parser->TopDecls();

    WGL_Error::ReportErrorsL(printer->printer, parser->GetFirstError());
    if (parser->HaveErrors())
        return FALSE;

    WGL_ValidateShader *validator;
    WGL_Validator::Configuration config;
    config.for_vertex = for_vertex;
    config.output_format = opts.codegen ? (opts.for_directx_version == 10 ?
                                               VEGA3D_SHADER_LANGUAGE(SHADER_HLSL_10) :
                                               VEGA3D_SHADER_LANGUAGE(SHADER_HLSL_9))
                                          :
                                          (opts.for_gl_es ?
                                               VEGA3D_SHADER_LANGUAGE(SHADER_GLSL_ES) :
                                               VEGA3D_SHADER_LANGUAGE(SHADER_GLSL));
    config.output_version = opts.for_version;
    WGL_ValidateShader::MakeL(validator, context, printer->printer);
    if (!validator->ValidateL(shader_decls, config))
    {
        WGL_Error::ReportErrorsL(printer->printer, validator->GetFirstError());
        return FALSE;
    }

    if (vars)
    {
        LEAVE_IF_ERROR(context->RecordResult(for_vertex, vars));
        if (opts.show_vars)
            LEAVE_IF_ERROR(vars->Show(printer));
    }

    return TRUE;
}

int
main(int argc, char **argv)
{
    Opera opera;
    op_memset(&opera, 0, sizeof(Opera));
    g_opera = &opera;
    g_opera->InitL();

    WGL_Standalone_Options opts;

    int arg = 1;
    unsigned files_seen = 0;
    while (arg < argc)
    {
        if (op_strcmp(argv[arg], "-v") == 0)
            opts.quiet = FALSE;
        else if (op_strcmp(argv[arg], "-va") == 0 || op_strcmp(argv[arg], "--validate") == 0)
            opts.validate = TRUE;
        else if (op_strcmp(argv[arg], "-q") == 0)
            opts.quiet = TRUE;
        else if (op_strcmp(argv[arg], "-d") == 0)
            opts.debug = TRUE;
        else if (op_strcmp(argv[arg], "-fs") == 0 || op_strcmp(argv[arg], "--fragment-shader") == 0)
            opts.have_fs = TRUE;
        else if (op_strcmp(argv[arg], "-g") == 0 ||  op_strcmp(argv[arg], "--generate-hlsl") == 0)
            opts.codegen = TRUE;
        else if (op_strcmp(argv[arg], "--oes") == 0)
            opts.support_oes = TRUE;
        else if (op_strncmp(argv[arg], "--version-output=", 17) == 0)
        {
            unsigned i = op_atoi(argv[arg] + 17);
            opts.for_version = i;
        }
        else if (op_strcmp(argv[arg], "--dx9") == 0)
            opts.for_directx_version = 9;
        else if (op_strcmp(argv[arg], "--dx10") == 0)
            opts.for_directx_version = 10;
        else if (op_strcmp(argv[arg], "--gles") == 0)
            opts.for_gl_es = TRUE;
        else if (op_strcmp(argv[arg], "--nohighp") == 0)
            opts.for_highp = FALSE;
        else if (op_strcmp(argv[arg], "-s") == 0 ||  op_strcmp(argv[arg], "--show-variables") == 0)
            opts.show_vars = TRUE;
        else if (op_strcmp(argv[arg], "-o") == 0 && (arg + 1) < argc)
        {
            ++arg;
            opts.out_file = argv[arg];
        }
        else if (op_strcmp(argv[arg], "-vs") == 0 || op_strcmp(argv[arg], "--vertex-shader") == 0)
            opts.have_vs = TRUE;
        else if (files_seen < 2)
        {
            if (!opts.have_vs && !(files_seen == 0 && opts.have_fs))
            {
                opts.vs_file = argv[arg];
                opts.have_vs = TRUE;
            }
            else if (files_seen < 2)
            {
                opts.fs_file = argv[arg];
                opts.have_fs = TRUE;
            }
            files_seen++;
        }
        else
        {
            fprintf(stderr, "Unrecognised option: %s\n", argv[arg]);
            fflush(stderr);
        }

        arg++;
    }

    if (files_seen < 1)
    {
        printf("Usage: %s [options] vshader-file [pshader-file]\n", argv[0]);
        return 1;
    }

    FILE *out_file;
    if (*opts.out_file)
    {
        out_file = fopen(opts.out_file, "w");
        if (!out_file)
        {
            fprintf(stderr, "Unable to open '%s'; exiting.\n", opts.out_file);
            return 1;
        }
    }
    else
        out_file = stdout;

    OpMemGroup region;
    WGL_StdioPrinter *printer = OP_NEW(WGL_StdioPrinter, (out_file, opts.for_gl_es, opts.for_highp, *opts.out_file));
    if (!printer)
    {
        if (*opts.out_file)
            fclose(out_file);
        return 1;
    }
    OpAutoPtr<WGL_StdioPrinter> anchor_printer(printer);

    WGL_PrettyPrinter pretty(printer);
    WGL_Context *context;
    if (OpStatus::IsError(WGL_Context::Make(context, &region, opts.support_oes)))
        return 1;

    OpAutoContext anchor_context(context);

    WGL_ASTBuilder builder(context);
    context->SetASTBuilder(&builder);

    BOOL vs_validates;
    BOOL fs_validates;

    WGL_DeclList *vertex_shader = NULL;
    WGL_ShaderVariables vertex_vars;
    if (opts.have_vs)
    {
        uni_char *vs_buf;
        unsigned size;
        if (OpStatus::IsError(read_file(vs_buf, size, const_cast<char *>(opts.vs_file))))
        {
            fprintf(stderr, "Error while reading '%s'; exiting.\n", opts.vs_file);
            return 1;
        }

        TRAPD(status,
              context->SetSourceContextL(opts.vs_file);
              vs_validates = ProcessSourceL(context, &pretty, vertex_shader, &vertex_vars, vs_buf, size, TRUE, opts));

        OP_DELETEA(vs_buf);
        if (OpStatus::IsError(status))
            return 1;
    }

    WGL_DeclList *fragment_shader = NULL;
    WGL_ShaderVariables fragment_vars;
    if (opts.have_fs)
    {
        uni_char *ps_buf;
        unsigned size;
        if (OpStatus::IsError(read_file(ps_buf, size, const_cast<char *>(opts.fs_file))))
        {
            fprintf(stderr, "Error while reading '%s'; exiting.\n", opts.fs_file);
            return 1;
        }

        TRAPD(status,
              context->SetSourceContextL(opts.fs_file);
              fs_validates = ProcessSourceL(context, &pretty, fragment_shader, &fragment_vars, ps_buf, size, FALSE, opts));

        OP_DELETEA(ps_buf);
        if (OpStatus::IsError(status))
            return 1;
    }

    if (opts.have_vs && vs_validates &&
        opts.have_fs && fs_validates)
    {
        WGL_Validator::Configuration config;
        TRAPD(status, WGL_Validator::ValidateLinkageL(config, &vertex_vars, &fragment_vars));
        if (OpStatus::IsError(status))
            return 1;
    }

    if (opts.debug && opts.have_vs)
    {
        if (!vs_validates)
        {
            printf("(No -d output for vertex shader (%s) - did not validate.)\n", opts.vs_file);
        }
        else
        {
            printer->OutString("// Vertex shader: ");
            printer->OutString(opts.vs_file);
            printer->OutNewline();
            if (context->GetUsedClamp())
            {
                printer->OutString("int webgl_op_clamp(int x, int ma) { return (x < 0 ? 0 : (x > ma ? ma : x)); }");
                printer->OutNewline();
            }
            vertex_shader->VisitDeclList(&pretty);
        }
    }

    if (opts.debug && opts.have_fs)
    {
        if (!fs_validates)
        {
            printf("(No -d output for fragment shader (%s) - did not validate.)\n", opts.fs_file);
        }
        else
        {
            printer->OutString("// Fragment shader: ");
            printer->OutString(opts.fs_file);
            printer->OutNewline();
            WGL_Validator::PrintVersionAndExtensionPrelude(printer, opts.for_version, context);
            fragment_shader->VisitDeclList(&pretty);
        }
    }

    if (opts.codegen)
    {
        if (opts.have_vs && !vs_validates)
        {
            printf("(No HLSL code generation, because the vertex shader (%s) did not validate.)\n", opts.vs_file);
            goto after_codegen;
        }

        if (opts.have_fs && !fs_validates)
        {
            printf("(No HLSL code generation, because the fragment shader (%s) did not validate.)\n", opts.fs_file);
            goto after_codegen;
        }

        WGL_ASTBuilder fragment_builder(context);
        WGL_HLSLBuiltins::SemanticInfo semantic_info[] =
            { {UNI_L("g_Normal"), UNI_L("NORMAL"), WGL_HLSLBuiltins::NORMAL},
              {UNI_L("g_Diffuse"), UNI_L("COLOR"), WGL_HLSLBuiltins::COLOR}
            };

        WGL_CgBuilder::Configuration config(opts.for_directx_version);
        config.vertex_semantic_info = semantic_info;
        config.vertex_semantic_info_elements = ARRAY_SIZE(semantic_info);
        config.pixel_semantic_info = semantic_info;
        config.pixel_semantic_info_elements = ARRAY_SIZE(semantic_info);
        WGL_CgBuilder cg(&config, context, printer, &fragment_builder);

        TRAPD(status,
              vertex_vars.MergeWithL(&fragment_vars);
              cg.GenerateCode(&vertex_vars, vertex_shader, fragment_shader));

        if (OpStatus::IsError(status))
            return 1;
    }
after_codegen:

    return 0;
}
