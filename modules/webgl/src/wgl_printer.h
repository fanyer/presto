/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- dumping output.
 *
 */
#ifndef WGL_PRINTER_H
#define WGL_PRINTER_H

#include "modules/webgl/src/wgl_ast.h"

/** The interface for emitting outputs from
    handling shader language validation and translation.

    For convenience, keeps track of state needed when
    pretty printing expressions, but interface instances
    provide the actual output methods. */
class WGL_Printer
{
public:
    WGL_Printer(BOOL for_es, BOOL for_highp)
        : current_precedence(WGL_Expr::PrecLowest)
        , current_associativity(WGL_Expr::AssocNone)
        , binary_argument(0)
        , indent(0)
        , indentDelta(4)
        , for_gles_target(for_es)
        , for_highp_target(for_highp)
    {
    }

    virtual ~WGL_Printer()
    {
    }

    /* NOTE: Output methods must report errors by raising
       an exception; toplevel printing routines are responsible
       for handling these, either by trapping or propagating
       the exception upwards. */

    virtual void OutInt(int i) = 0;
    virtual void OutDouble(double d) = 0;
    virtual void OutBool(BOOL b) = 0;
    virtual void OutString(const char *s) = 0;
    virtual void OutString(uni_char *s) = 0;
    virtual void OutString(const uni_char *s) = 0;
    virtual void OutStringId(const uni_char *s) = 0;
    virtual void OutString(WGL_VarName *i) = 0;

    virtual void OutNewline() = 0;

    virtual void Flush(BOOL as_error) = 0;

    BOOL HasNoPrecedence();

    WGL_Expr::PrecAssoc EnterArgListScope();
    /**< Sets the current precedence and associativity to values suitable for
         printing an argument list of a function or constructor call. Arguments
         will be printed with surrounding parentheses if they use the comma
         operator at the top level.

         Returns the previous precedence and associativity. */

    void LeaveArgListScope(WGL_Expr::PrecAssoc pa);
    /**< Restores the current precedence and associativity after we have
         finished printing an argument list. */

    WGL_Expr::PrecAssoc EnterOpScope(WGL_Expr::Operator op, unsigned binary_arg = 0);
    /**< Marks the beginning of processing the operator 'op'. An opening
         parenthesis will be output if needed (e.g., if 'op' has lower
         precedence than the parent operator).

         Returns the previous precedence and associativity. */

    void LeaveOpScope(WGL_Expr::PrecAssoc pa, unsigned binary_arg = 0);
    /**< Marks the end of processing the operator 'op'. The current precedence
         and associativity is restored to 'pa'. A closing parenthesis matching
         the opening parenthesis output by EnterOpScope() will be output if
         needed. */

    unsigned AsBinaryArg(unsigned a)
    {
        unsigned old = binary_argument;
        binary_argument = a; return old;
    }

    unsigned GetBinaryArg() { return binary_argument; }

    void IncreaseIndent();
    void DecreaseIndent();

    BOOL ForGLESTarget() { return for_gles_target; }
    BOOL ForHighpTarget() { return for_highp_target; }

protected:
    WGL_Expr::Precedence current_precedence;
    WGL_Expr::Associativity current_associativity;
    unsigned binary_argument;

    unsigned indent;
    unsigned indentDelta;

    BOOL for_gles_target;
    BOOL for_highp_target;
};

#endif // WGL_PRINTER_H
