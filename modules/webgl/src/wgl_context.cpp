/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2010-2011
 *
 * WebGL compiler -- compilation context.
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_context.h"
#include "modules/webgl/src/wgl_symbol.h"
#include "modules/webgl/src/wgl_string.h"
#include "modules/webgl/src/wgl_intrinsics.h"
#include "modules/webgl/src/wgl_lexer.h"
#ifndef WGL_STANDALONE
#include "modules/libcrypto/include/CryptoHash.h"
#endif // !WGL_STANDALONE

/* static */ OP_STATUS
WGL_Context::Make(WGL_Context *&new_context, OpMemGroup *region, BOOL support_oes_extension)
{
    new_context = OP_NEWGRO(WGL_Context, (region), region);
    if (!new_context)
        return OpStatus::ERR_NO_MEMORY;

    new_context->strings_table = WGL_Identifier_List::Make(new_context, 4);
    if (!new_context->strings_table)
        return OpStatus::ERR_NO_MEMORY;

    new_context->unique_table = WGL_Identifier_List::Make(new_context, 4);
    if (!new_context->unique_table)
        return OpStatus::ERR_NO_MEMORY;

    new_context->extensions_supported = support_oes_extension ? WGL_GLSLBuiltins::Extension_OES_standard_derivatives : 0;

    unsigned count = 0;
    unsigned s = new_context->extensions_supported;
    while (s)
    {
        if ((s & 0x1) != 0)
            count++;
        s = s >> 1;
    }
    new_context->extensions_supported_count = count;

    return OpStatus::OK;
}

void
WGL_Context::SetSourceContextL(const char *s)
{
    if (s)
    {
        int val_len = static_cast<int>(op_strlen(s));
        int str_len = val_len + 1;

        file_name = OP_NEWGROA_L(uni_char, str_len, Arena());
        uni_char *str = const_cast<uni_char *>(file_name);
        make_doublebyte_in_buffer(s, val_len, str, str_len);
    }
    else
        file_name = NULL;
}

BOOL
WGL_Context::IsTypeName(WGL_String *s)
{
    unsigned position;
    return types_table->IndexOf(s, position);
}

WGL_Type *
WGL_ValidationState::LookupTypeName(WGL_VarName *name)
{
    List<WGL_Type> *existing_type;
    if (OpStatus::IsSuccess(types_map.GetData(name, &existing_type)) && existing_type && existing_type->Cardinal() > 0)
        return existing_type->First();
    else
        return NULL;
}

WGL_Type *
WGL_ValidationState::LookupVarType(WGL_String *i, WGL_VarName **alias_var, BOOL current_scope_only, BOOL deref_name)
{
    for (WGL_BindingScope<WGL_VarAlias> *scope = alias_scope.First(); scope; scope = scope->Suc())
    {
        if (scope->list)
            for (WGL_VarAlias *v = scope->list->First(); v; v = v->Suc())
                if (i->Equals(v->var1))
                {
                    if (alias_var)
                        *alias_var = v->var2;
                    i = v->var2;
                    break;
                }
        if (current_scope_only)
            break;
    }

    WGL_Type *var_type = NULL;

    for (WGL_BindingScope<WGL_VarBinding> *scope = variable_environment.First(); scope; scope = scope->Suc())
    {
        for (WGL_VarBinding *v = scope->list->First(); v; v = v->Suc())
            if (i->Equals(v->var))
            {
                var_type = v->type;
                break;
            }
        if (current_scope_only)
            return var_type;
    }

    if (!var_type)
    {
        for (WGL_VarBinding *v = global_scope.First(); v; v = v->Suc())
            if (i->Equals(v->var))
            {
                var_type = v->type;
                break;
            }
    }

    if (!var_type)
    {
        List<WGL_Builtin> *list = LookupBuiltin(i);
        if (list && list->First() && list->First()->IsBuiltinVar())
            return FromBuiltinType(&static_cast<WGL_BuiltinVar *>(list->First())->type);
    }
    else if (deref_name && var_type->GetType() == WGL_Type::Name)
    {
        /* The identifier is a struct name, which we resolve to the struct type */
        return LookupTypeName(static_cast<WGL_NameType *>(var_type)->name);
    }

    return var_type;
}

WGL_VarBinding *
WGL_ValidationState::LookupVarBinding(WGL_String *i)
{
    for (WGL_BindingScope<WGL_VarBinding> *scope = variable_environment.First(); scope; scope = scope->Suc())
    {
        for (WGL_VarBinding *v = scope->list->First(); v; v = v->Suc())
            if (i->Equals(v->var))
                return v;
    }

    for (WGL_BindingScope<WGL_VarAlias> *scope = alias_scope.First(); scope; scope = scope->Suc())
    {
        for (WGL_VarAlias *v = scope->list ? scope->list->First() : NULL; v; v = v->Suc())
            if (i->Equals(v->var1))
                return LookupVarBinding(v->var2);
    }

    for (WGL_VarBinding *v = global_scope.First(); v; v = v->Suc())
        if (i->Equals(v->var))
            return v;

    return NULL;
}

WGL_Type *
WGL_ValidationState::LookupLocalVar(WGL_String *i, BOOL current_scope_only)
{
    for (WGL_BindingScope<WGL_VarBinding> *scope = variable_environment.First(); scope; scope = scope->Suc())
    {
        for (WGL_VarBinding *v = scope->list->First(); v; v = v->Suc())
            if (i->Equals(v->var))
                return v->type;
        if (current_scope_only)
            break;
    }

    for (WGL_BindingScope<WGL_VarAlias> *scope = alias_scope.First(); scope; scope = scope->Suc())
    {
        for (WGL_VarAlias *v = scope->list ? scope->list->First() : NULL; v; v = v->Suc())
            if (i->Equals(v->var1))
                return LookupLocalVar(v->var2, current_scope_only);
        if (current_scope_only)
            break;
    }

    return NULL;
}

WGL_Type *
WGL_ValidationState::LookupGlobal(WGL_String *i)
{
    for (WGL_VarBinding *v = global_scope.First(); v; v = v->Suc())
        if (i->Equals(v->var))
            return v->type;

    return NULL;
}

WGL_VaryingOrUniform *
WGL_ValidationState::LookupUniform(WGL_String *i, BOOL record_use)
{
    for (WGL_VaryingOrUniform *u = uniforms.First(); u; u = u->Suc())
        if (i->Equals(u->var))
        {
            if (record_use)
                u->usages++;
            return u;
        }

    return NULL;
}

WGL_VaryingOrUniform *
WGL_ValidationState::LookupVarying(WGL_String *i, BOOL record_use)
{
    for (WGL_VaryingOrUniform *u = varyings.First(); u; u = u->Suc())
        if (i->Equals(u->var))
        {
            if (record_use)
                u->usages++;
            return u;
        }

    return NULL;
}

WGL_Attribute *
WGL_ValidationState::LookupAttribute(WGL_String *i, BOOL record_use)
{
    for (WGL_Attribute *attr = attributes.First(); attr; attr = attr->Suc())
        if (i->Equals(attr->var))
        {
            if (record_use)
                attr->usages++;
            return attr;
        }

    return NULL;
}

WGL_FunctionData *
WGL_ValidationState::LookupFunction(WGL_String *i)
{
    for (WGL_FunctionData *f = function_scope.First(); f; f = f->Suc())
        if (i->Equals(f->name))
            return f;

    return NULL;
}

List<WGL_Builtin> *
WGL_ValidationState::LookupBuiltin(WGL_String *i)
{
    List<WGL_Builtin> *list;
    if (OpStatus::IsSuccess(builtins_map.GetData(i, &list)))
        return list;
    else
        return NULL;
}

WGL_Field *
WGL_ValidationState::LookupField(WGL_FieldList *field_list, WGL_String *i)
{
    for (WGL_Field *f = field_list ? field_list->list.First() : NULL; f; f = f->Suc())
        if (i->Equals(f->name))
            return f;

    return NULL;
}

BOOL
WGL_VariableUse::IsIn(List<WGL_VariableUse> &list, WGL_VarName *g)
{
    for (WGL_VariableUse *f = list.First(); f; f = f->Suc())
        if (f->var->Equals(g))
            return TRUE;

    return FALSE;
}

void
WGL_ValidationState::AddFunctionCall(WGL_VarName *f)
{
    if (current_function)
    {
        if (!current_callers)
            current_callers = OP_NEWGRO_L(List<WGL_VariableUse>, (), context->Arena());

        if (!WGL_VariableUse::IsIn(*current_callers, f))
        {
            WGL_VariableUse *f_use = OP_NEWGRO_L(WGL_VariableUse, (f), context->Arena());
            f_use->Into(current_callers);
        }
    }
}

BOOL
WGL_Context::IsUnique(WGL_String *s)
{
    unsigned index;
    return unique_table->IndexOf(s, index);
}

WGL_Context::~WGL_Context()
{
}

void
WGL_Context::Release()
{
    file_name = NULL;
    validation_state.Release();
}

void
WGL_Context::ResetLocalsL()
{
    GetValidationState().precision_environment.RemoveAll();
    GetValidationState().variable_environment.RemoveAll();
    GetValidationState().alias_scope.RemoveAll();
}

void
WGL_Context::ResetL(BOOL for_vertex)
{
    errors.RemoveAll();
    ResetLocalsL();
    ResetGlobalsL();
    line_number = 1;

    if (for_vertex != !processing_fragment)
    {
        processing_fragment = !for_vertex;
        extensions_enabled = 0;
    }
}

void
WGL_Context::ResetGlobalsL()
{
    /* No need to delete; assumed to be an arena allocated list.
       The string & unique tables are separately allocated and freed. */
    /* Create, and leave be across resets. */
    if (!strings_table)
        LEAVE_IF_NULL(strings_table = WGL_Identifier_List::Make(this, 4));

    LEAVE_IF_NULL(types_table = WGL_Identifier_List::Make(this, 4));

    GetValidationState().ResetL();
}

void
WGL_Context::EnterLocalScope(WGL_Type *return_type)
{
    GetValidationState().EnterScope(return_type);
}

void
WGL_Context::LeaveLocalScope()
{
    GetValidationState().LeaveScope();
}

BOOL
WGL_Context::IsVectorType(unsigned i)
{
    return (i <= WGL_Lexer::TYPE_UVEC4);
}

/* static */ WGL_VectorType::Type
WGL_Context::ToVectorType(unsigned i)
{
    return static_cast<WGL_VectorType::Type>(1 + (i - WGL_Lexer::TYPE_VEC2));
}

/* static */ BOOL
WGL_Context::IsMatrixType(unsigned i)
{
    return (i >= WGL_Lexer::TYPE_MAT2 && i <= WGL_Lexer::TYPE_MAT4X4);
}

/* static */ WGL_MatrixType::Type
WGL_Context::ToMatrixType(unsigned i)
{
    return static_cast<WGL_MatrixType::Type>(1 + (i - WGL_Lexer::TYPE_MAT2));
}

/* static */ BOOL
WGL_Context::IsSamplerType(unsigned i)
{
    return (i >= WGL_Lexer::TYPE_SAMPLER1D && i <= WGL_Lexer::TYPE_USAMPLER2DMSARRAY);
}

/* static */ WGL_SamplerType::Type
WGL_Context::ToSamplerType(unsigned i)
{
    return static_cast<WGL_SamplerType::Type>(1 + (i - WGL_Lexer::TYPE_SAMPLER1D));
}

BOOL
WGL_Context::IsSupportedExtension(const uni_char *extension, unsigned extension_length)
{
    for (unsigned i = 0; i < WGL_GLSLBuiltins::extension_info_elements; i++)
        if ((WGL_GLSLBuiltins::extension_info[i].id & extensions_supported) != 0)
            if (processing_fragment && WGL_GLSLBuiltins::extension_info[i].fragment_name && uni_strncmp(WGL_GLSLBuiltins::extension_info[i].fragment_name, extension, extension_length) == 0 ||
                !processing_fragment && WGL_GLSLBuiltins::extension_info[i].vertex_name && uni_strncmp(WGL_GLSLBuiltins::extension_info[i].vertex_name, extension, extension_length) == 0)
                return TRUE;

    return FALSE;
}

BOOL
WGL_Context::IsEnabledExtension(unsigned idx, WGL_GLSLBuiltins::FunctionInfo **functions, unsigned *functions_count)
{
    OP_ASSERT(idx < 8 * sizeof(unsigned));
    unsigned mask = 0x1 << idx;
    if ((extensions_enabled & mask) != 0)
    {
        if (functions)
        {
            *functions = processing_fragment ? WGL_GLSLBuiltins::extension_info[idx].fragment_functions : WGL_GLSLBuiltins::extension_info[idx].vertex_functions;
            *functions_count = processing_fragment ? WGL_GLSLBuiltins::extension_info[idx].fragment_functions_count : WGL_GLSLBuiltins::extension_info[idx].vertex_functions_count;
        }
        return TRUE;
    }
    else
        return FALSE;
}

unsigned
WGL_Context::GetExtensionCount()
{
    return extensions_supported_count;
}

BOOL
WGL_Context::EnableExtension(unsigned idx, const uni_char **cpp_define)
{
    OP_ASSERT(idx < 8 * sizeof(unsigned));
    if ((extensions_supported & (0x1 << idx)) != 0)
    {
        if ((extensions_enabled & (0x1 << idx)) != 0)
            return FALSE;
        else if (processing_fragment && WGL_GLSLBuiltins::extension_info[idx].fragment_name ||
                 !processing_fragment && WGL_GLSLBuiltins::extension_info[idx].vertex_name)
        {
            extensions_enabled |= (0x1 << idx);
            if (cpp_define)
                *cpp_define = WGL_GLSLBuiltins::extension_info[idx].cpp_enabled_define;
            return TRUE;
        }
        else
            return FALSE;
    }
    else
        return FALSE;
}

BOOL
WGL_Context::EnableExtension(const uni_char *extension, unsigned extension_length, const uni_char **cpp_define)
{
    for (unsigned i = 0; WGL_GLSLBuiltins::extension_info_elements; i++)
        if (processing_fragment && WGL_GLSLBuiltins::extension_info[i].fragment_name && uni_strncmp(WGL_GLSLBuiltins::extension_info[i].fragment_name, extension, extension_length) == 0 ||
            !processing_fragment && WGL_GLSLBuiltins::extension_info[i].vertex_name && uni_strncmp(WGL_GLSLBuiltins::extension_info[i].vertex_name, extension, extension_length) == 0)
        {
            if ((extensions_enabled & WGL_GLSLBuiltins::extension_info[i].id) != 0)
                return FALSE;
            else
            {
                extensions_enabled |= WGL_GLSLBuiltins::extension_info[i].id;
                if (cpp_define)
                    *cpp_define = WGL_GLSLBuiltins::extension_info[i].cpp_enabled_define;
                return TRUE;
            }
        }

    return FALSE;
}

BOOL
WGL_Context::DisableExtension(unsigned idx, const uni_char **cpp_define)
{
    OP_ASSERT(idx < 8 * sizeof(unsigned));
    if ((extensions_supported & (0x1 << idx)) != 0)
    {
        if ((extensions_enabled & (0x1 << idx)) == 0)
            return FALSE;
        else if (processing_fragment && WGL_GLSLBuiltins::extension_info[idx].fragment_name ||
                 !processing_fragment && WGL_GLSLBuiltins::extension_info[idx].vertex_name)
        {
            extensions_enabled &= ~(0x1 << idx);
            if (cpp_define)
                *cpp_define = WGL_GLSLBuiltins::extension_info[idx].cpp_enabled_define;
            return TRUE;
        }
        else
            return FALSE;
    }
    else
        return FALSE;
}

BOOL
WGL_Context::DisableExtension(const uni_char *extension, unsigned extension_length, const uni_char **cpp_define)
{
    for (unsigned i = 0; WGL_GLSLBuiltins::extension_info_elements; i++)
        if (processing_fragment && WGL_GLSLBuiltins::extension_info[i].fragment_name && uni_strncmp(WGL_GLSLBuiltins::extension_info[i].fragment_name, extension, extension_length) == 0 ||
            !processing_fragment && WGL_GLSLBuiltins::extension_info[i].vertex_name && uni_strncmp(WGL_GLSLBuiltins::extension_info[i].vertex_name, extension, extension_length) == 0)
        {
            if ((extensions_enabled & WGL_GLSLBuiltins::extension_info[i].id) == 0)
                return FALSE;
            else
            {
                extensions_enabled &= ~(WGL_GLSLBuiltins::extension_info[i].id);
                if (cpp_define)
                    *cpp_define = WGL_GLSLBuiltins::extension_info[i].cpp_enabled_define;
                return TRUE;
            }
        }

    return FALSE;
}

BOOL
WGL_Context::GetExtensionIndex(unsigned idx, WGL_GLSLBuiltins::Extension &id)
{
    if (idx >= GetExtensionCount())
        return FALSE;

    unsigned flag = 1;
    while (idx > 0)
    {
        flag = flag << 1;
        idx--;
    }
    id = static_cast<WGL_GLSLBuiltins::Extension>(flag);
    return TRUE;
}

BOOL
WGL_Context::GetExtensionInfo(WGL_GLSLBuiltins::Extension id, const uni_char **vertex_name, const uni_char **fragment_name, const uni_char **supported_define)
{
    for (unsigned i = 0; i < WGL_GLSLBuiltins::extension_info_elements; i++)
    {
        if (id == WGL_GLSLBuiltins::extension_info[i].id)
        {
            if (vertex_name)
                *vertex_name = WGL_GLSLBuiltins::extension_info[i].vertex_name;
            if (fragment_name)
                *fragment_name = WGL_GLSLBuiltins::extension_info[i].fragment_name;
            if (supported_define)
                *supported_define = WGL_GLSLBuiltins::extension_info[i].cpp_supported_define;
            return TRUE;
        }
    }

    return FALSE;
}

BOOL
WGL_Context::IsExtensionEnabled(unsigned int i, BOOL fragment, const uni_char *&define)
{
    OP_ASSERT(i < GetExtensionCount());
    if (extensions_enabled & (1 << i))
        if (fragment && WGL_GLSLBuiltins::extension_info[i].fragment_name || !fragment && WGL_GLSLBuiltins::extension_info[i].vertex_name)
        {
            define = WGL_GLSLBuiltins::extension_info[i].cpp_supported_define;
            return TRUE;
        }
    return FALSE;
}

void
WGL_Context::AddError(WGL_Error::Type t, const uni_char *msg)
{
    WGL_Error *item = OP_NEWGRO_L(WGL_Error, (this, t, GetSourceContext(), line_number), Arena());
    item->SetMessageL(msg);
    AddError(item);
}

WGL_Type *
WGL_Context::NewTypeName(uni_char const *name)
{
    WGL_String *str = WGL_String::Make(this, name);
    WGL_NameType *name_type = OP_NEWGRO_L(WGL_NameType, (str, FALSE), Arena());

    unsigned position;
    types_table->AppendL(this, str, position);
    return name_type;
}

WGL_Type *
WGL_Context::NewTypeName(WGL_String *str)
{
    WGL_NameType *name_type = OP_NEWGRO_L(WGL_NameType, (str, FALSE), Arena());

    unsigned position;
    types_table->AppendL(this, str, position);

    return name_type;
}

WGL_Literal *
WGL_Context::BuildLiteral(WGL_Literal::Type t)
{
    return OP_NEWGRO_L(WGL_Literal, (t), Arena());
}

OP_STATUS
WGL_Context::RecordResult(BOOL for_vertex_shader, WGL_ShaderVariables *result)
{
    TRAPD(status, RecordResultL(for_vertex_shader, result));
    if (OpStatus::IsError(status))
        result->Release();
    return status;
}

/* private */ void
WGL_Context::RecordResultL(BOOL for_vertex_shader, WGL_ShaderVariables *result)
{
    for (WGL_VaryingOrUniform *uni = validation_state.uniforms.First(); uni; uni = uni->Suc())
    {
        /* Declared uniforms must be 'statically used'; otherwise declarations are ignored. */
        if (uni->usages == 0)
            continue;

        uni_char *name = OP_NEWA_L(uni_char, WGL_String::Length(uni->var) + 1);
        uni_strcpy(name, Storage(this, uni->var));
        ANCHOR_ARRAY(uni_char, name);

        WGL_Type *type = WGL_Type::CopyL(uni->type);
        OpStackAutoPtr<WGL_Type> anchor_type(type);

        WGL_ShaderVariable *var = OP_NEW_L(WGL_ShaderVariable, (WGL_ShaderVariable::Uniform, name, uni->precision, type, TRUE));
        ANCHOR_ARRAY_RELEASE(name);
        anchor_type.release();
        OpStackAutoPtr<WGL_ShaderVariable> anchor_var(var);

        if (WGL_VarName *alias = FindAlias(name))
            var->SetAliasL(Storage(this, alias), TRUE);

        anchor_var.release();
        result->AddUniform(var);
    }
    for (WGL_VaryingOrUniform *varying = validation_state.varyings.First(); varying; varying = varying->Suc())
    {
        /* When outputting to GLSL, declared varying variables without any uses
           in the fragment shader are ignored. We can't do this when outputting
           to HLSL as shader signatures need to match up (CT-2182). */
        if (WGL_Validator::IsGL(configuration->output_format) && !for_vertex_shader && varying->usages == 0)
            continue;

        uni_char *name = OP_NEWA_L(uni_char, WGL_String::Length(varying->var) + 1);
        uni_strcpy(name, Storage(this, varying->var));
        ANCHOR_ARRAY(uni_char, name);

        WGL_Type *type = WGL_Type::CopyL(varying->type);
        OpStackAutoPtr<WGL_Type> anchor_type(type);

        WGL_ShaderVariable *var = OP_NEW_L(WGL_ShaderVariable, (WGL_ShaderVariable::Varying, name, varying->precision, type, TRUE));
        ANCHOR_ARRAY_RELEASE(name);
        anchor_type.release();
        OpStackAutoPtr<WGL_ShaderVariable> anchor_var(var);

        if (WGL_VarName *alias = FindAlias(name))
            var->SetAliasL(Storage(this, alias), TRUE);

        anchor_var.release();
        result->AddVarying(var);
    }
    for (WGL_Attribute *attr = validation_state.attributes.First(); attr; attr = attr->Suc())
    {
        if (attr->usages == 0)
            continue;

        uni_char *name = OP_NEWA_L(uni_char, WGL_String::Length(attr->var) + 1);
        uni_strcpy(name, Storage(this, attr->var));
        ANCHOR_ARRAY(uni_char, name);

        WGL_Type *type = WGL_Type::CopyL(attr->type);
        OpStackAutoPtr<WGL_Type> anchor_type(type);

        WGL_ShaderVariable *var = OP_NEW_L(WGL_ShaderVariable, (WGL_ShaderVariable::Attribute, name, WGL_Type::NoPrecision, type, TRUE));
        ANCHOR_ARRAY_RELEASE(name);
        anchor_type.release();
        OpStackAutoPtr<WGL_ShaderVariable> anchor_var(var);

        if (WGL_VarName *alias = FindAlias(name))
            var->SetAliasL(Storage(this, alias), TRUE);

        anchor_var.release();
        result->AddAttribute(var);
    }
}

/* WGL_ValidationState */

void
WGL_ValidationState::Release()
{
    builtins_map.RemoveAll();
    types_map.RemoveAll();
}

void
WGL_ValidationState::ResetL()
{
    attributes.RemoveAll();
    uniforms.RemoveAll();
    varyings.RemoveAll();
    global_scope.RemoveAll();
    function_scope.RemoveAll();
    variable_environment.RemoveAll();
    precision_environment.RemoveAll();
    alias_scope.RemoveAll();
    scope_level = 0;

    WGL_BindingScope<WGL_Precision> *new_scope = OP_NEWGRO_L(WGL_BindingScope<WGL_Precision>, (), context->Arena());
    List<WGL_Precision> *list = OP_NEWGRO_L(List<WGL_Precision>, (), context->Arena());

    new_scope->list = list;
    new_scope->IntoStart(&precision_environment);

    WGL_BindingScope<WGL_VarAlias> *new_alias = OP_NEWGRO_L(WGL_BindingScope<WGL_VarAlias>, (), context->Arena());
    new_alias->IntoStart(&alias_scope);
}

void
WGL_ValidationState::EnterScope(WGL_Type *return_type)
{
    WGL_BindingScope<WGL_Precision> *new_scope = OP_NEWGRO_L(WGL_BindingScope<WGL_Precision>, (), context->Arena());
    List<WGL_Precision> *list = OP_NEWGRO_L(List<WGL_Precision>, (), context->Arena());
    new_scope->list = list;

    new_scope->IntoStart(&precision_environment);

    WGL_BindingScope<WGL_VarBinding> *new_var_scope = OP_NEWGRO_L(WGL_BindingScope<WGL_VarBinding>, (), context->Arena());
    List<WGL_VarBinding> *bind_list = OP_NEWGRO_L(List<WGL_VarBinding>, (), context->Arena());
    new_var_scope->list = bind_list;

    new_var_scope->IntoStart(&variable_environment);

    WGL_BindingScope<WGL_VarAlias> *new_alias_scope = OP_NEWGRO_L(WGL_BindingScope<WGL_VarAlias>, (), context->Arena());

    new_alias_scope->IntoStart(&alias_scope);

    scope_level++;

    OP_ASSERT(!return_type || scope_level == 1);
}

void
WGL_ValidationState::LeaveScope()
{
    WGL_BindingScope<WGL_Precision> *prec_scope = precision_environment.First();
    prec_scope->Out();

    WGL_BindingScope<WGL_VarBinding> *var_scope = variable_environment.First();
    var_scope->Out();

    WGL_BindingScope<WGL_VarAlias> *ali_scope = alias_scope.First();
    ali_scope->Out();

    scope_level--;
}

void
WGL_ValidationState::AddAttribute(WGL_Type *t, WGL_VarName *i, unsigned l)
{
    WGL_SourceLoc loc(context->GetSourceContext(), l);
    WGL_Attribute *attr = OP_NEWGRO_L(WGL_Attribute, (loc), context->Arena());

    if (t && t->GetType() == WGL_Type::Name)
        t = LookupTypeName(static_cast<WGL_NameType *>(t)->name);

    attr->type = t;
    attr->var = i;
    attr->Into(&attributes);
}

void
WGL_ValidationState::AddUniform(WGL_Type *t, WGL_VarName *v, WGL_Type::Precision p, unsigned line_number)
{
    WGL_SourceLoc loc(context->GetSourceContext(), line_number);
    WGL_VaryingOrUniform *unif = OP_NEWGRO_L(WGL_VaryingOrUniform, (t, v, p, loc), context->Arena());

    if (t && t->GetType() == WGL_Type::Name)
        t = LookupTypeName(static_cast<WGL_NameType *>(t)->name);

    unif->type = t;
    unif->Into(&uniforms);
}

void
WGL_ValidationState::AddVarying(WGL_Type *t, WGL_VarName *v, WGL_Type::Precision p, unsigned line_number)
{
    WGL_SourceLoc loc(context->GetSourceContext(), line_number);
    WGL_VaryingOrUniform *unif = OP_NEWGRO_L(WGL_VaryingOrUniform, (t, v, p, loc), context->Arena());

    if (t && t->GetType() == WGL_Type::Name)
        t = LookupTypeName(static_cast<WGL_NameType *>(t)->name);

    unif->type = t;
    unif->Into(&varyings);
}

void
WGL_ValidationState::AddPrecision(WGL_Type *t, WGL_Type::Precision p)
{
    WGL_BindingScope<WGL_Precision> *current_scope = precision_environment.First();
    WGL_Precision *prec = OP_NEWGRO_L(WGL_Precision, (t, p), context->Arena());
    prec->IntoStart(current_scope->list);
}

WGL_VarBinding *
WGL_ValidationState::AddVariable(WGL_Type *t, WGL_VarName *v, BOOL is_read_only)
{
    WGL_VarBinding *binding = OP_NEWGRO_L(WGL_VarBinding, (t, v, is_read_only), context->Arena());
    WGL_BindingScope<WGL_VarBinding> *current_scope = variable_environment.First();

    if (current_scope)
        binding->IntoStart(current_scope->list);
    else
        binding->IntoStart(&global_scope);

    return binding;
}

WGL_VarName *
WGL_Context::NewUniqueVar(BOOL record_name)
{
    uni_char unique_name[100]; // ARRAY OK 2011-02-09 sof
    uni_snprintf(unique_name, ARRAY_SIZE(unique_name), UNI_L("%s_%u"), WGL_UNIQUE_VAR_PREFIX, id_unique++);

    WGL_VarName *name = WGL_String::Make(this, unique_name);
    unsigned dummy;
    if (record_name)
        GetUniques()->AppendL(this, name, dummy);
    return name;
}

WGL_VarName *
WGL_Context::NewUniqueHashVar(WGL_String *existing)
{
    uni_char unique_name[128]; // ARRAY OK 2011-10-22 sof

    /* Use the hash as the unique; the risk of SHA2 collisions
       is slight. Only intended used with longer existing identifiers,
       where the same unique value (across shaders) must be computed/used. */

#ifndef WGL_STANDALONE
    OpString8 hash_string;
    hash_string.SetUTF8FromUTF16L(Storage(this, existing));
    CryptoHash *hasher = CryptoHash::Create(CRYPTO_HASH_TYPE_SHA256);
    LEAVE_IF_NULL(hasher);
    OpStackAutoPtr<CryptoHash> anchor_hasher(hasher);
    LEAVE_IF_ERROR(hasher->InitHash());
    hasher->CalculateHash(hash_string.CStr());

    unsigned char result[32]; // ARRAY OK 2011-10-26 sof
    OP_ASSERT(ARRAY_SIZE(result) == hasher->Size());
    hasher->ExtractHash(result);

    uni_char result_digits[65]; // ARRAY OK 2011-10-26 sof
    const uni_char *hex = UNI_L("0123456789abcdef");
    for (unsigned i = 0; i < ARRAY_SIZE(result); i++)
    {
        result_digits[2 * i + 1] = hex[result[i] & 0x0f];
        result_digits[2 * i] = hex[(result[i] & 0xf0) >> 4];
    }
    result_digits[ARRAY_SIZE(result_digits) - 1] = 0;

    uni_snprintf(unique_name, ARRAY_SIZE(unique_name), UNI_L("%s_h%s"), WGL_UNIQUE_VAR_PREFIX, result_digits);
#else
    /* Simpler hashing in standalone. */
    unsigned hash = existing->CalculateHash();
    uni_snprintf(unique_name, ARRAY_SIZE(unique_name), UNI_L("%s_h%u"), WGL_UNIQUE_VAR_PREFIX, hash);
#endif // !WGL_STANDALONE


    /* If collisions should occur; append a unique sequence number. */
    unsigned i = 0;
    while (FindAlias(unique_name))
#ifndef WGL_STANDALONE
        uni_snprintf(unique_name, ARRAY_SIZE(unique_name), UNI_L("%s_h%s_%u"), WGL_UNIQUE_VAR_PREFIX, result_digits, i++);
#else
        uni_snprintf(unique_name, ARRAY_SIZE(unique_name), UNI_L("%s_h%u_%u"), WGL_UNIQUE_VAR_PREFIX, hash, i++);
#endif // !WGL_STANDALONE

    return WGL_String::Make(this, unique_name);
}

WGL_VarName *
WGL_Context::FindAlias(WGL_VarName *name)
{
    for (WGL_BindingScope<WGL_VarAlias> *scope = GetValidationState().alias_scope.First(); scope; scope = scope->Suc())
        for (WGL_VarAlias *alias = scope->list ? scope->list->First() : NULL; alias; alias = alias->Suc())
            if (name->Equals(alias->var2))
                return alias->var1;

    return NULL;
}

WGL_VarName *
WGL_Context::FindAlias(const uni_char *name)
{
    for (WGL_BindingScope<WGL_VarAlias> *scope = GetValidationState().alias_scope.First(); scope; scope = scope->Suc())
        for (WGL_VarAlias *alias = scope->list ? scope->list->First() : NULL; alias; alias = alias->Suc())
            if (alias->var2->Equals(name))
                return alias->var1;

    return NULL;
}
void
WGL_ValidationState::AddAlias(WGL_VarName *old_name, WGL_VarName *new_name)
{
    WGL_VarAlias *var_alias = OP_NEWGRO_L(WGL_VarAlias, (old_name, new_name), context->Arena());
    WGL_BindingScope<WGL_VarAlias> *current_scope = alias_scope.First();

    if (current_scope)
    {
        if (!current_scope->list)
        {
            List<WGL_VarAlias> *new_list = OP_NEWGRO_L(List<WGL_VarAlias>, (), context->Arena());
            current_scope->list = new_list;
        }
        var_alias->IntoStart(current_scope->list);
    }
}

WGL_String*
WGL_ValidationState::AddStructType(WGL_String *str, WGL_Type *type)
{
    OP_ASSERT(type && type->GetType() == WGL_Type::Struct);

    List<WGL_Type> *existing_type;
    if (OpStatus::IsSuccess(types_map.GetData(str, &existing_type)))
    {
        WGL_Error *error = OP_NEWGRO_L(WGL_Error, (context, WGL_Error::DUPLICATE_NAME, context->GetSourceContext(), 0), context->Arena());
        error->SetMessageL(Storage(context, str));
        context->AddError(error);
    }
    else
    {
        List<WGL_Type> *new_list = OP_NEWGRO_L(List<WGL_Type>, (), context->Arena());
        type->Into(new_list);
        types_map.Add(str, new_list);
    }

    return str;
}

void
WGL_ValidationState::AddFunction(WGL_VarName *fun_name, WGL_Type *return_type, WGL_ParamList *params, BOOL is_proto, WGL_SourceLoc &loc)
{
    WGL_FunctionDecl *fun_decl = OP_NEWGRO_L(WGL_FunctionDecl, (loc, return_type, params, is_proto), context->Arena());
    WGL_FunctionData *fun = function_scope.First();
    for (; fun; fun = fun->Suc())
    {
        if (fun->name->Equals(fun_name))
        {
            BOOL is_duplicate = TRUE;
            WGL_FunctionDecl *overlap = NULL;
            if (IsOverlappingOverload(params, return_type, is_proto, overlap, is_duplicate, fun->decls))
            {
                WGL_Error::Type err = is_duplicate ? WGL_Error::ILLEGAL_FUNCTION_OVERLOAD_DUPLICATE : WGL_Error::ILLEGAL_FUNCTION_OVERLOAD_MISMATCH;
                context->AddError(err, Storage(context, fun_name));
            }
            else if (ConflictsWithBuiltin(fun_name, params, return_type))
                context->AddError(WGL_Error::ILLEGAL_FUNCTION_OVERLOAD_BUILTIN_OVERLAP, Storage(context, fun_name));
            else
                fun_decl->Into(&fun->decls);
            return;
        }
    }

    fun = OP_NEWGRO_L(WGL_FunctionData, (fun_name), context->Arena());
    fun_decl->Into(&fun->decls);
    fun->Into(&function_scope);
}

WGL_Type::Precision
WGL_ValidationState::LookupPrecision(WGL_Type *t)
{
    WGL_BindingScope<WGL_Precision> *prec_scope = precision_environment.First();

    while (prec_scope)
    {
        for (WGL_Precision *binding = prec_scope->list ? prec_scope->list->First() : NULL; binding; binding = binding->Suc())
            if (IsSameType(t, binding->type, TRUE))
                return binding->precision;

        prec_scope = prec_scope->Suc();
    }
    return WGL_Type::NoPrecision;
}

BOOL
WGL_ValidationState::IsOverlappingOverload(WGL_ParamList *params, WGL_Type *return_type, BOOL is_proto, WGL_FunctionDecl *&overlap, BOOL &duplicate, List<WGL_FunctionDecl> &existing)
{
    for(WGL_FunctionDecl *f = existing.First(); f; f = f->Suc())
    {
        BOOL matches_params = TRUE;
        if (!f->parameters || !params)
            matches_params = (f->parameters == params);
        else if (f->parameters->list.Cardinal() == params->list.Cardinal())
        {
            WGL_Param *p1 = params->list.First();
            for (WGL_Param *p2 = f->parameters->list.First(); p2; p1 = p1->Suc(), p2 = p2->Suc())
                if (!IsSameType(p1->type, p2->type, TRUE))
                {
                    matches_params = FALSE;
                    break;
                }
        }
        else
            continue;

        if (matches_params)
        {
            BOOL is_same = IsSameType(return_type, f->return_type, TRUE);
            if (!is_same || f->is_proto == is_proto)
            {
                overlap = f;
                duplicate = is_same;
                return TRUE;
            }
        }
    }
    return FALSE;
}

BOOL
WGL_ValidationState::ConflictsWithBuiltin(WGL_VarName *fun_name, WGL_ParamList *params, WGL_Type *return_type)
{
    List<WGL_Builtin> *builtins;
    if (OpStatus::IsSuccess(builtins_map.GetData(fun_name, &builtins)))
    {
        for (WGL_Builtin *b = builtins->First(); b; b = b->Suc())
            if (!b->IsBuiltinVar())
            {
                WGL_BuiltinFun *bfun = static_cast<WGL_BuiltinFun *>(b);
                if (bfun->parameters.Empty() && !params || bfun->parameters.Cardinal() == params->list.Cardinal())
                {
                    BOOL matches_builtin = TRUE;
                    WGL_Param *p1 = params->list.First();
                    for (WGL_BuiltinType *p2 = bfun->parameters.First(); p2; p1 = p1->Suc(), p2 = p2->Suc())
                        if (!MatchesBuiltinType(p2, p1->type))
                        {
                            matches_builtin = FALSE;
                            break;
                        }

                    if (matches_builtin)
                        return TRUE;
                }
            }
    }
    return FALSE;
}

BOOL
WGL_ValidationState::IsLegalReferenceArg(WGL_Expr *e)
{
    switch(e->GetType())
    {
    case WGL_Expr::Var:
        if (WGL_VarBinding *bind = LookupVarBinding(static_cast<WGL_VarExpr *>(e)->name))
            return (!bind->is_read_only && (!bind->type->type_qualifier || !(bind->type->type_qualifier->storage == WGL_TypeQualifier::Const || bind->type->type_qualifier->storage == WGL_TypeQualifier::Uniform)));
        else
            return FALSE;
    default:
        return FALSE;
    }
}

List<WGL_Builtin> *
WGL_Context::NewBuiltinVar(const uni_char *name, WGL_VectorType::Type vt, WGL_Type::Precision prec, BOOL is_read_only, BOOL for_output, BOOL for_vertex, const char *symbolic_size)
{
    WGL_BuiltinVar *bv = OP_NEWGRO_L(WGL_BuiltinVar, (), Arena());
    List <WGL_Builtin> *list = OP_NEWGRO_L(List<WGL_Builtin>, (), Arena());

    bv->name = name;
    bv->precision = prec;
    bv->type.type = WGL_BuiltinType::Vector;
    bv->type.value.vector_type = vt;
    bv->array_size = WGL_ArraySize(symbolic_size);
    bv->is_read_only = is_read_only;
    bv->is_output = for_output;
    bv->is_const = FALSE;
    bv->const_value = -1;
    bv->for_vertex = for_vertex;

    bv->Into(list);
    return list;
}

List<WGL_Builtin> *
WGL_Context::NewBuiltinVar(const uni_char *name, WGL_BasicType::Type t, WGL_Type::Precision prec, BOOL is_read_only, BOOL is_out, BOOL for_vertex)
{
    WGL_BuiltinVar *bv = OP_NEWGRO_L(WGL_BuiltinVar, (), Arena());
    List <WGL_Builtin> *list = OP_NEWGRO_L(List<WGL_Builtin>, (), Arena());

    bv->name = name;
    bv->precision = prec;
    bv->type.type = WGL_BuiltinType::Basic;
    bv->type.value.basic_type = t;
    bv->is_read_only = is_read_only;
    bv->is_output = is_out;
    bv->is_const = FALSE;
    bv->const_value = -1;
    bv->for_vertex = for_vertex;

    bv->Into(list);
    return list;
}

List<WGL_Builtin> *
WGL_Context::NewBuiltinConstant(const uni_char *name, WGL_BasicType::Type t, WGL_Type::Precision prec, int v)
{
    WGL_BuiltinVar *bv = OP_NEWGRO_L(WGL_BuiltinVar, (), Arena());
    List <WGL_Builtin> *list = OP_NEWGRO_L(List<WGL_Builtin>, (), Arena());

    bv->name = name;
    bv->precision = prec;
    bv->type.type = WGL_BuiltinType::Basic;
    bv->type.value.basic_type = t;
    bv->is_read_only = TRUE;
    bv->is_output = FALSE;
    bv->is_const = TRUE;
    bv->const_value = v;
    bv->for_vertex = TRUE;

    bv->Into(list);
    return list;
}

WGL_BuiltinType*
ToBuiltinType(WGL_Context *context, const char *ty_spec)
{
    WGL_BuiltinType *ret_type = OP_NEWGRO_L(WGL_BuiltinType, (), context->Arena());

    if (ty_spec[0] == 'G' && !ty_spec[1])
        ret_type->type = WGL_BuiltinType::Gen;
    else if (ty_spec[0] == 'V' && !ty_spec[1])
        ret_type->type = WGL_BuiltinType::Vec;
    else if (ty_spec[0] == 'V')
    {
        ret_type->type = WGL_BuiltinType::Vector;
        char x = ty_spec[1];
        OP_ASSERT(!ty_spec[2] && (x == '2' || x == '3' || x == '4'));
        ret_type->value.vector_type = x == '2' ? WGL_VectorType::Vec2 : (x == '3' ? WGL_VectorType::Vec3 : WGL_VectorType::Vec4);
    }
    else if (ty_spec[0] == 'B'  && !ty_spec[1])
    {
        ret_type->type = WGL_BuiltinType::Basic;
        ret_type->value.basic_type = WGL_BasicType::Bool;
    }
    else if (ty_spec[0] == 'B'  && ty_spec[1] == 'V')
    {
        ret_type->type = WGL_BuiltinType::BVec;
        OP_ASSERT(!ty_spec[2]);
    }
    else if (ty_spec[0] == 'I'  && ty_spec[1] == 'V')
    {
        ret_type->type = WGL_BuiltinType::IVec;
        OP_ASSERT(!ty_spec[2]);
    }
    else if (ty_spec[0] == 'M'  && !ty_spec[1])
        ret_type->type = WGL_BuiltinType::Mat;
    else if (ty_spec[0] == 'F'  && !ty_spec[1])
    {
        ret_type->type = WGL_BuiltinType::Basic;
        ret_type->value.basic_type = WGL_BasicType::Float;
    }
    else if (ty_spec[0] == 'S'  && ty_spec[1] == 'C' && !ty_spec[2])
    {
        ret_type->type = WGL_BuiltinType::Sampler;
        ret_type->value.sampler_type = WGL_SamplerType::SamplerCube;
    }
    else if (ty_spec[0] == 'S'  && ty_spec[1] == '2' && ty_spec[2] == 'D' && !ty_spec[3])
    {
        ret_type->type = WGL_BuiltinType::Sampler;
        ret_type->value.sampler_type = WGL_SamplerType::Sampler2D;
    }
    else
        OP_ASSERT(!"unexpected type specifier");

    return ret_type;
}

void
WGL_Context::Initialise(WGL_Validator::Configuration *config)
{
    OpPointerHashTable<WGL_String, List<WGL_Builtin> > &builtins_map = GetValidationState().builtins_map;
    builtins_map.RemoveAll();
    if (!config || config->for_vertex)
    {
        builtins_map.Add(WGL_String::Make(this, UNI_L("gl_Position")), NewBuiltinVar(UNI_L("gl_Position"), WGL_VectorType::Vec4, WGL_Type::High, FALSE, TRUE, TRUE));
        builtins_map.Add(WGL_String::Make(this, UNI_L("gl_PointSize")), NewBuiltinVar(UNI_L("gl_PointSize"), WGL_BasicType::Float, WGL_Type::Medium, FALSE, TRUE, TRUE));
        builtins_map.Add(WGL_String::Make(this, UNI_L("gl_DepthRange")), NewBuiltinVar(UNI_L("gl_DepthRange"), WGL_VectorType::Vec3, WGL_Type::High, FALSE, FALSE, TRUE));
    }
    else
    {
        builtins_map.Add(WGL_String::Make(this, UNI_L("gl_FragCoord")), NewBuiltinVar(UNI_L("gl_FragCoord"), WGL_VectorType::Vec4, WGL_Type::Medium, FALSE, TRUE, FALSE));
        builtins_map.Add(WGL_String::Make(this, UNI_L("gl_FrontFacing")), NewBuiltinVar(UNI_L("gl_FrontFacing"), WGL_BasicType::Bool, WGL_Type::NoPrecision, TRUE, FALSE, FALSE));
        builtins_map.Add(WGL_String::Make(this, UNI_L("gl_FragColor")), NewBuiltinVar(UNI_L("gl_FragColor"), WGL_VectorType::Vec4, WGL_Type::Medium, FALSE, TRUE, FALSE));
        builtins_map.Add(WGL_String::Make(this, UNI_L("gl_FragData")), NewBuiltinVar(UNI_L("gl_FragData"), WGL_VectorType::Vec4, WGL_Type::Medium, FALSE, TRUE, FALSE, "gl_MaxDrawBuffers"));
        builtins_map.Add(WGL_String::Make(this, UNI_L("gl_PointCoord")), NewBuiltinVar(UNI_L("gl_PointCoord"), WGL_VectorType::Vec2, WGL_Type::Medium, FALSE, FALSE, FALSE));
        builtins_map.Add(WGL_String::Make(this, UNI_L("gl_DepthRange")), NewBuiltinVar(UNI_L("gl_DepthRange"), WGL_VectorType::Vec3, WGL_Type::High, FALSE, FALSE, FALSE));
    }

    /* Constants available to both */
    if (config)
    {
        for (unsigned i = 0; i < WGL_GLSLBuiltins::vertex_shader_constant_elements; i++)
        {
            WGL_GLSLBuiltins::ConstantInfo info = WGL_GLSLBuiltins::vertex_shader_constant[i];
            builtins_map.Add(WGL_String::Make(this, info.name), NewBuiltinConstant(info.name, info.basic_type, info.precision, WGL_GLSLBuiltins::GetConstantValue(*config, info.const_value)));
        }
    }

    for (unsigned i = 0; i < WGL_GLSLBuiltins::shader_intrinsics_elements; i++)
    {
        WGL_GLSLBuiltins::FunctionInfo info = WGL_GLSLBuiltins::shader_intrinsics[i];
        WGL_BuiltinFun *fun = OP_NEWGRO_L(WGL_BuiltinFun, (), Arena());

        fun->name = info.name;
        fun->intrinsic = info.intrinsic;
        fun->arity = info.arity;
        fun->return_type = ToBuiltinType(this, info.return_type);
        if (info.arg1_type)
        {
            WGL_BuiltinType *arg = ToBuiltinType(this, info.arg1_type);
            arg->Into(&fun->parameters);
        }
        if (info.arg2_type)
        {
            WGL_BuiltinType *arg = ToBuiltinType(this, info.arg2_type);
            arg->Into(&fun->parameters);
        }
        if (info.arg3_type)
        {
            WGL_BuiltinType *arg = ToBuiltinType(this, info.arg3_type);
            arg->Into(&fun->parameters);
        }

        List <WGL_Builtin> *list;
        WGL_String *builtin_name = WGL_String::Make(this, info.name);
        if (OpStatus::IsError(builtins_map.GetData(builtin_name, &list)))
        {
            list = OP_NEWGRO_L(List<WGL_Builtin>, (), Arena());
            builtins_map.Add(builtin_name, list);
        }
        fun->Into(list);
    }

    for (unsigned i = 0; i < GetExtensionCount(); i++)
    {
        WGL_GLSLBuiltins::FunctionInfo *funs = NULL;
        unsigned count = 0;
        if (IsEnabledExtension(i, &funs, &count))
        {
            for (unsigned j = 0; j < count; j++)
            {
                WGL_GLSLBuiltins::FunctionInfo info = funs[j];

                WGL_BuiltinFun *fun = OP_NEWGRO_L(WGL_BuiltinFun, (), Arena());

                fun->name = info.name;
                fun->intrinsic = info.intrinsic;
                fun->arity = info.arity;
                fun->return_type = ToBuiltinType(this, info.return_type);
                if (info.arg1_type)
                {
                    WGL_BuiltinType *arg = ToBuiltinType(this, info.arg1_type);
                    arg->Into(&fun->parameters);
                }
                if (info.arg2_type)
                {
                    WGL_BuiltinType *arg = ToBuiltinType(this, info.arg2_type);
                    arg->Into(&fun->parameters);
                }
                if (info.arg3_type)
                {
                    WGL_BuiltinType *arg = ToBuiltinType(this, info.arg3_type);
                    arg->Into(&fun->parameters);
                }

                List <WGL_Builtin> *list;
                WGL_String *builtin_name = WGL_String::Make(this, info.name);
                if (OpStatus::IsError(builtins_map.GetData(builtin_name, &list)))
                {
                    list = OP_NEWGRO_L(List<WGL_Builtin>, (), Arena());
                    builtins_map.Add(builtin_name, list);
                }
                fun->Into(list);
            }
        }
    }

    configuration = config;
}

BOOL
WGL_BuiltinType::Matches(WGL_BasicType::Type t, BOOL strict)
{
    switch(t)
    {
    case WGL_BasicType::Void:
        return FALSE;
    case WGL_BasicType::UInt:
        t = WGL_BasicType::Int;
        /* fall through */
    case WGL_BasicType::Float:
        if (t == WGL_BasicType::Float && this->type == Gen)
            return TRUE;
    case WGL_BasicType::Bool:
    case WGL_BasicType::Int:
        return (this->type == Basic && (this->value.basic_type == t || !strict && this->value.basic_type != WGL_BasicType::Void));
    default:
        return FALSE;
    }
}

BOOL
WGL_BuiltinType::Matches(WGL_VectorType::Type t, BOOL strict)
{
    /* NOTE: strict argument is currently unused; determine if there will be a need for it. */
    switch(t)
    {
    case WGL_VectorType::Vec2:
    case WGL_VectorType::Vec3:
    case WGL_VectorType::Vec4:
        return (this->type == Vector && this->value.vector_type == t || this->type == Gen || this->type == Vec);
    case WGL_VectorType::BVec2:
    case WGL_VectorType::BVec3:
    case WGL_VectorType::BVec4:
        return (this->type == BVec || this->type == Vector && this->value.vector_type == t);
    case WGL_VectorType::IVec2:
    case WGL_VectorType::IVec3:
    case WGL_VectorType::IVec4:
        return (this->type == IVec || this->type == Vector && this->value.vector_type == t);
    case WGL_VectorType::UVec2:
    case WGL_VectorType::UVec3:
    case WGL_VectorType::UVec4:
        return (this->type == Vector && this->value.vector_type == t);
    default:
        return FALSE;
    }
}

BOOL
WGL_BuiltinType::Matches(WGL_MatrixType::Type t, BOOL strict)
{
    switch(t)
    {
    case WGL_MatrixType::Mat2:
    case WGL_MatrixType::Mat3:
    case WGL_MatrixType::Mat4:
        return (this->type == Matrix && this->value.matrix_type == t || this->type == Mat);

    case WGL_MatrixType::Mat2x2:
    case WGL_MatrixType::Mat2x3:
    case WGL_MatrixType::Mat2x4:
    case WGL_MatrixType::Mat3x2:
    case WGL_MatrixType::Mat3x3:
    case WGL_MatrixType::Mat3x4:
    case WGL_MatrixType::Mat4x2:
    case WGL_MatrixType::Mat4x3:
    case WGL_MatrixType::Mat4x4:
        return (this->type == Matrix && this->value.matrix_type == t);

    default:
        return FALSE;
    }
}

BOOL
WGL_BuiltinType::Matches(WGL_SamplerType::Type t, BOOL strict)
{
    return (this->type == Sampler && this->value.sampler_type == t);
}

#endif // CANVAS3D_SUPPORT
