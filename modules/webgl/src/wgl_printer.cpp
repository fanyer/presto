/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009 - 2010
 *
 * WebGL GLSL compiler -- pretty printer for AST.
 *
 */

#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_printer.h"

BOOL
WGL_Printer::HasNoPrecedence()
{
    return (current_precedence == WGL_Expr::PrecLowest);
}

WGL_Expr::PrecAssoc
WGL_Printer::EnterArgListScope()
{
    const WGL_Expr::Precedence prev_prec = current_precedence;
    const WGL_Expr::Associativity prev_assoc = current_associativity;

    current_precedence = WGL_Expr::PrecAssign;
    current_associativity = WGL_Expr::AssocLeft;

    return WGL_Op::ToPrecAssoc(prev_prec, prev_assoc);
}

void
WGL_Printer::LeaveArgListScope(WGL_Expr::PrecAssoc pa)
{
    current_precedence = WGL_Op::GetPrecAssocP(pa);
    current_associativity = WGL_Op::GetPrecAssocA(pa);
}

static BOOL
NeedsParens(BOOL entering, WGL_Expr::Precedence at_prec, WGL_Expr::Associativity with_assoc, WGL_Expr::Precedence to_prec, WGL_Expr::Associativity to_assoc, unsigned binary_arg)
{
    if (entering ? (at_prec > to_prec && to_prec != WGL_Expr::PrecLowest) : (at_prec < to_prec && at_prec != WGL_Expr::PrecLowest))
        return TRUE;

    if (at_prec == to_prec && with_assoc != WGL_Expr::AssocNone && to_assoc != WGL_Expr::AssocNone)
        if (with_assoc != to_assoc)
            return TRUE;
        else if (binary_arg == 1 && with_assoc == WGL_Expr::AssocRight)
            return TRUE;
        else if (binary_arg == 2 && with_assoc == WGL_Expr::AssocLeft)
            return TRUE;

    return FALSE;
}

WGL_Expr::PrecAssoc
WGL_Printer::EnterOpScope(WGL_Expr::Operator op, unsigned binary_arg)
{
    WGL_Expr::Precedence to_prec = WGL_Op::GetPrecedence(op);
    WGL_Expr::Associativity to_assoc = WGL_Op::GetAssociativity(op);
    if (NeedsParens(TRUE, current_precedence, current_associativity, to_prec, to_assoc, binary_arg))
        OutString("(");

    WGL_Expr::Precedence prev_prec = current_precedence;
    WGL_Expr::Associativity prev_assoc = current_associativity;
    current_precedence = to_prec;
    current_associativity = to_assoc;
    return WGL_Op::ToPrecAssoc(prev_prec, prev_assoc);
}

void
WGL_Printer::LeaveOpScope(WGL_Expr::PrecAssoc pa, unsigned binary_arg)
{
    WGL_Expr::Precedence entering_prec = WGL_Op::GetPrecAssocP(pa);
    WGL_Expr::Associativity entering_assoc = WGL_Op::GetPrecAssocA(pa);
    if (NeedsParens(FALSE, current_precedence, current_associativity, entering_prec, entering_assoc, binary_arg))
        OutString(")");

    current_precedence = entering_prec;
    current_associativity = entering_assoc;
}

void
WGL_Printer::IncreaseIndent()
{
    indent += indentDelta;
}

void
WGL_Printer::DecreaseIndent()
{
    if (indent > indentDelta)
        indent -= indentDelta;
    else
        indent = 0;
}

#endif // CANVAS3D_SUPPORT
