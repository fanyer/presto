/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_COMPILER_UNROLL_H
#define ES_COMPILER_UNROLL_H

#include "modules/ecmascript/carakan/src/compiler/es_compiler.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_expr.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_stmt.h"

class ES_LoopUnroller
{
public:
    ES_LoopUnroller(ES_Compiler &compiler)
        : compiler(compiler),
          loop_iteration(NULL)
    {
    }

    void SetLoopType(ES_Statement::Type type) { loop_type = type; }
    void SetLoopCondition(ES_Expression *condition) { loop_condition = condition; }
    void SetLoopBody(ES_Statement *body) { loop_body = body; }
    void SetLoopIteration(ES_Statement *iteration) { loop_iteration = iteration; }

    BOOL Unroll(const ES_Compiler::Register &dst);

private:
    ES_Compiler &compiler;
    ES_Statement::Type loop_type;
    ES_Expression *loop_condition;
    ES_Statement *loop_body;
    ES_Statement *loop_iteration;

    ES_Compiler::Register loop_variable, limit_variable;
    int start_value, limit_value;
};

#endif // ES_COMPILER_UNROLL_H
