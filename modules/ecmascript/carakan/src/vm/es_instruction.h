/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Table of instruction names.
 *
 * @author modules/ecmascript/carakan/src/scripts/es_instruction.h.py
 */

#ifndef ES_INSTRUCTION_H
#define ES_INSTRUCTION_H

/**
 * Instruction set definition
 * ==========================
 *
 * Note about try/catch/finally:
 *
 * The presence of an exception handler is handled in some way external to the
 * bytecode stream; some mapping between each bytecode location and the bytecode
 * offset of the start of the exception handler.  A catch block will fetch the
 * caught exception into a register using the ESI_CATCH instruction.  A finally
 * block will not use ESI_CATCH, leaving the "caught" exception untouched;
 * instead it uses a ESI_RETHROW (with no operands) to throw it upwards when it
 * ends (if it at all caught an exception -- finally will run regardless of
 * whether an exception was thrown or not.)
 *
 * Other constructs that internally needs to clean up at stack unwinds,
 * primarily with statements, will register exception handlers and catch
 * exceptions like finally (no ESI_CATCH but ending with ESI_RETHROW.)
 *
 * Note about property access caching:
 *
 * The instructions ESI_GETN_IMM_CACHED, ESI_PUTN_IMM_CACHED and
 * ESI_PUTN_IMM_CACHED_NEW are never emitted by the compiler.  Instead, the VM
 * dynamicly replaces ESI_GETN_IMM and ESI_PUTN_IMM instructions with the cached
 * variants when it has performed a successful operation to enable caching.  To
 * enable simple replacement, the GET variants have 5 operands and the PUT
 * variants 6, even in cases when not all are used by the actual instruction.
 *
 * Note about operand types:
 *
 * 'reg()' operands are unsigned indeces into the register array.  There are no
 * special values to enable use without range checking.  The name within the
 * parentheses is just documentation and has no special meaning.
 *
 * Note about function calling convention:
 *
 * A function call sets up the top of the register frame so that register N is
 * the this object, register N+1 is the function object, and register N+2
 * through N+1+argc are the arguments.  This range of register then forms
 * registers 0 through 1+argc in the called function's register frame.  Register
 * N (0 in the called function's register frame) is used to pass the return
 * value from the function when it returns.  Note that the "top" of the register
 * frame is not necessarily the absolute end of the calling functions register
 * frame; it will typically use registers with as low indeces as possible.
 *
 * The ESI_CALL instruction has two operands: register N/0 and argc.  Unlike
 * other register type operands, the first argument to the ESI_CALL instruction
 * is not used as a single register, but rather as a pointer to the first
 * register in an array of registers (though it's still just a register index in
 * the bytecode stream.)
 *
 * The ESI_CONSTRUCT instruction works exactly the same way, operand-wise.  The
 * difference is that the 'this' register is not populated initially, but
 * instead is set to a newly created object by the instruction itself prior to
 * the actual call.
 */

enum ES_Instruction
{
    ESI_LOAD_STRING,
    /**< Load string immediate into register.

         Operands: reg(dst), unusued */

    ESI_LOAD_DOUBLE,
    /**< Load double immediate into register.

         Operands: reg(dst), unusued */

    ESI_LOAD_INT32,
    /**< Load int32 immediate into register.

         Operands: reg(dst), imm(value) */

    ESI_LOAD_NULL,
    /**< Load null into register.

         Operands: reg(dst) */

    ESI_LOAD_UNDEFINED,
    /**< Load undefined into register.

         Operands: reg(dst) */

    ESI_LOAD_TRUE,
    /**< Load true into register.

         Operands: reg(dst) */

    ESI_LOAD_FALSE,
    /**< Load false into register.

         Operands: reg(dst) */

    ESI_LOAD_GLOBAL_OBJECT,
    /**< Loads global object into register.

         Operands: reg(dst) */

    ESI_COPY,
    /**< Copy value from one register to another.

         Operands: reg(dst), reg(src) */

    ESI_COPYN,
    /**< Copy values from two or more registers into a consequtive range of
         target registers.  None of the destination registers can also occur as
         source registers.  The dst index of the register into which the value
         in the first source register is copied.  Additional source register's
         values are copied into registers dst+N.

         Operands: reg(dst), imm(count), reg(srcN) */

    ESI_TYPEOF,
    /**< Load typeof string into register.

         Operands: reg(dst), reg(src) */

    ESI_TONUMBER,
    /**< Convert value to number.  This is normally not used; instructions
         expecting numeric operands are responsible for converting their
         operands themselves.  The primary situation in which this instruction
         is emitted is for the unary '+' operator; whose only function in
         practice is to convert its operand to a number.

         Operands: reg(dst), reg(src) */

    ESI_TOOBJECT,
    /**< Convert value to object.  This is normally not used; instructions
         expecting object operands are responsible for converting their operands
         themselves.  The primary situation in which this instruction is emitted
         is for with statements; that run-time only need to store an object in a
         register.

         Operands: reg(dst), reg(src) */

    ESI_TOPRIMITIVE,
    /**< Convert value to its primitive value representation guided by a type
         conversion hint.  The instruction is normally not used; instructions
         expecting non-object operands are responsible for converting their
         operands themselves.  The main use for this instruction is when
         converting the operands of numeric instructions into primitives as part
         of compiling expressions.  The conversion isn't performed by the
         corresponding instructions, as the order of evaluation of operands and
         conversion to primitive wouldn't be correct then.  The type conversion
         hint immediate resolves which primitive value representation should be
         opted for and is an untyped encoding of the ES_Value_Internal::HintType
         enumeration.  The ESI_ADD(N) instructions use hint=none, all others
         hint=number, as required by the specification.

         Operands: reg(dst), reg(src), imm(hint) */

    ESI_TOPRIMITIVE1,
    /**< Like ESI_TOPRIMITIVE but in-place.

         Operands: reg(dst), imm(hint) */

    ESI_IS_NULL_OR_UNDEFINED,
    /**< Set implicit boolean register to TRUE if value is null or undefined and
         FALSE otherwise.

         Operands: reg(src) */

    ESI_IS_NOT_NULL_OR_UNDEFINED,
    /**< Set implicit boolean register to FALSE if value is null or undefined
         and TRUE otherwise.

         Operands: reg(src) */

    ESI_ADD,
    /**< Add strings or numbers.

         Operands: reg(dst), reg(lhs), reg(rhs) */

    ESI_ADD_IMM,
    /**< Add immediate integer to string or number.

         Operands: reg(dst), reg(lhs), imm(imm) */

#ifdef ES_COMBINED_ADD_SUPPORT
    ESI_ADDN,
    /**< Add strings or numbers.  Multiple operand version.  Each operand is
         guaranteed to be a primitive value in a temporary register.

         Operands: reg(dst), imm(count), reg(srcN) */
#endif

    ESI_FORMAT_STRING,
    /**< Create a string by prepending and/or appending a string constant to the
         toString:ed value in a register.  Similar to ESI_ADDN, buy specialized
         to handle a common case.

         Operands: reg(dst), reg(value), unusued, unusued, imm(cache) */

    ESI_SUB,
    /**< Subtract numbers.

         Operands: reg(dst), reg(lhs), reg(rhs) */

    ESI_SUB_IMM,
    /**< Subtract immediate integer from number.

         Operands: reg(dst), reg(lhs), imm(imm) */

    ESI_MUL,
    /**< Multiply two numbers.

         Operands: reg(dst), reg(lhs), reg(rhs) */

    ESI_MUL_IMM,
    /**< Multiply number and immediate integer.

         Operands: reg(dst), reg(lhs), imm(imm) */

    ESI_DIV,
    /**< Divide number by number.

         Operands: reg(dst), reg(dividend), reg(divisor) */

    ESI_DIV_IMM,
    /**< Divide number by immediate integer.

         Operands: reg(dst), reg(dividend), imm(imm) */

    ESI_REM,
    /**< Calculate remainder.

         Operands: reg(dst), reg(dividend), reg(divisor) */

    ESI_REM_IMM,
    /**< Calculate remainder.

         Operands: reg(dst), reg(dividend), imm(imm) */

    ESI_LSHIFT,
    /**< Shift 'src' left 'count' bits and store int32 result in 'dst'.  The
         'count' operand is actually an unsigned integer, but only the lower 5
         bits in it are used, and the lower 5 bits are the same regardless of
         whether the input is treated as a signed or unsigned integer.

         Operands: reg(dst), reg(src), reg(count) */

    ESI_LSHIFT_IMM,
    /**< Shift 'src' left 'count' bits and store int32 result in 'dst'.

         Operands: reg(dst), reg(src), imm(count) */

    ESI_RSHIFT_SIGNED,
    /**< Shift 'src' right 'count' bits and store int32 result in 'dst'.  The
         'count' operand is actually an unsigned integer, but only the lower 5
         bits in it are used, and the lower 5 bits are the same regardless of
         whether the input is treated as a signed or unsigned integer.

         Operands: reg(dst), reg(src), reg(count) */

    ESI_RSHIFT_SIGNED_IMM,
    /**< Shift 'src' right 'count' bits and store int32 result in 'dst'.

         Operands: reg(dst), reg(src), imm(count) */

    ESI_RSHIFT_UNSIGNED,
    /**< Shift 'src' unsigned right 'count' bits and store uint32 result in
         'dst'.  The 'count' operand is actually an unsigned integer, but only
         the lower 5 bits in it are used, and the lower 5 bits are the same
         regardless of whether the input is treated as a signed or unsigned
         integer.  Since uint32 values need to be represented as doubles (if
         they are outside int32 range) this instruction kills type analyzis
         slightly.  But if the 'src' operand is an int32, the result will be in
         int32 range as well, which the type analyzer recognizes.

         Operands: reg(dst), reg(src), reg(count) */

    ESI_RSHIFT_UNSIGNED_IMM,
    /**< Shift 'src' unsigned right 'count' bits and store int32 result in
         'dst'.

         Operands: reg(dst), reg(src), imm(count) */

    ESI_NEG,
    /**< Negate number.

         Operands: reg(dst), reg(src) */

    ESI_COMPL,
    /**< Applies bitwise complement to 'src' and store int32 result in 'dst'.

         Operands: reg(dst), reg(src) */

    ESI_INC,
    /**< Increment number by one in-place in register.

         Operands: reg(dst) */

    ESI_DEC,
    /**< Decrement number by one in-place in register.

         Operands: reg(dst) */

    ESI_BITAND,
    /**< Calculate bitwise AND.

         Operands: reg(dst), reg(lhs), reg(rhs) */

    ESI_BITAND_IMM,
    /**< Calculate bitwise AND.

         Operands: reg(dst), reg(lhs), imm(imm) */

    ESI_BITOR,
    /**< Calculate bitwise OR.

         Operands: reg(dst), reg(lhs), reg(rhs) */

    ESI_BITOR_IMM,
    /**< Calculate bitwise OR.

         Operands: reg(dst), reg(lhs), imm(imm) */

    ESI_BITXOR,
    /**< Calculate bitwise XOR.

         Operands: reg(dst), reg(lhs), reg(rhs) */

    ESI_BITXOR_IMM,
    /**< Calculate bitwise XOR.

         Operands: reg(dst), reg(lhs), imm(imm) */

    ESI_NOT,
    /**< Logical NOT.

         Operands: reg(dst), reg(src) */

    ESI_EQ,
    /**< Equality comparison.

         Operands: reg(lhs), reg(rhs) */

    ESI_EQ_IMM,
    /**< Equality comparison to signed integer.

         Operands: reg(lhs), imm(imm) */

    ESI_NEQ,
    /**< Inequality comparison.

         Operands: reg(lhs), reg(rhs) */

    ESI_NEQ_IMM,
    /**< Inequality comparison to signed integer.

         Operands: reg(lhs), imm(imm) */

    ESI_EQ_STRICT,
    /**< Strict equality comparison.

         Operands: reg(lhs), reg(rhs) */

    ESI_EQ_STRICT_IMM,
    /**< Strict equality comparison to signed integer.

         Operands: reg(lhs), imm(imm) */

    ESI_NEQ_STRICT,
    /**< Strict inequality comparison.

         Operands: reg(lhs), reg(rhs) */

    ESI_NEQ_STRICT_IMM,
    /**< Strict inequality comparison.

         Operands: reg(lhs), imm(imm) */

    ESI_LT,
    /**< Less than comparison.

         Operands: reg(lhs), reg(rhs) */

    ESI_LT_IMM,
    /**< Less than comparison to signed integer.

         Operands: reg(lhs), imm(imm) */

    ESI_LTE,
    /**< Less than or equal comparison.

         Operands: reg(lhs), reg(rhs) */

    ESI_LTE_IMM,
    /**< Less than or equal comparison to signed integer.

         Operands: reg(lhs), imm(imm) */

    ESI_GT,
    /**< Greater than comparison.

         Operands: reg(lhs), reg(rhs) */

    ESI_GT_IMM,
    /**< Greater than comparison to signed integer.

         Operands: reg(lhs), imm(imm) */

    ESI_GTE,
    /**< Greater than or equal comparison.

         Operands: reg(lhs), reg(rhs) */

    ESI_GTE_IMM,
    /**< Greater than or equal comparison to signed integer.

         Operands: reg(lhs), imm(imm) */

    ESI_CONDITION,
    /**< Set implicit boolean register to ToBoolean(register).  The input
         operand isn't modified unless it holds a hidden object, in which case
         it is updated to hold undefined.  This matches expected behaviour for
         these very rare objects.

         Operands: reg(src) */

    ESI_JUMP,
    /**< Unconditional jump.

         Operands: imm(offset) */

    ESI_JUMP_INDIRECT,
    /**< Indirect jump to contents of 'address'

         Operands: reg(address) */

    ESI_JUMP_IF_TRUE,
    /**< Conditional jump (jumps if implicit boolean register is true.)

         Operands: imm(offset), imm(data) */

    ESI_JUMP_IF_FALSE,
    /**< Conditional jump (jumps if implicit boolean register is false.)

         Operands: imm(offset), imm(data) */

    ESI_START_LOOP,
    /**< Signals the start of a loop.  The operand 'end' is the absolute code
         word index of the first instruction following the loop.

         Operands: imm(end) */

    ESI_STORE_BOOLEAN,
    /**< Store implicit boolean register into regular register.

         Operands: reg(dst) */

    ESI_GETN_IMM,
    /**< Get property where property name is known compile time.  To allow for
         conversion into ESI_GETN_IMM_CACHED, this instruction has two unused
         operands.

         Operands: reg(value), reg(object), identifier(name), imm(cache) */

    ESI_PUTN_IMM,
    /**< Put property where property name is known compile time.

         Operands: reg(object), identifier(name), reg(value), imm(cache) */

    ESI_INIT_PROPERTY,
    /**< Initialize property with given property name.

         Operands: reg(object), identifier(name), reg(value), imm(cache) */

    ESI_GET_LENGTH,
    /**< Special case of ESI_GETN_IMM when then property name is 'length'.  Has
         special handling of strings and arrays, otherwise very similar to
         ESI_GETN_IMM.  There is no special _CACHED variant, instead the class
         operand is zero initially, meaning the cache is not set.

         Operands: reg(value), reg(object), identifier(name), imm(cache) */

    ESI_PUT_LENGTH,
    /**< Special case of ESI_PUTN_IMM when then property name is 'length'.  Has
         special handling of arrays, otherwise very similar to ESI_PUTN_IMM.
         There is no special _CACHED or _CACHED_NEW variants, instead the class
         operands is zero initially, meaning the cache is not set.  If oldClass
         is set but not newClass, it acts as ESI_PUTN_IMM_CACHED; if both
         oldClass and newClass are set it acts as ESI_PUTN_IMM_CACHED_NEW.

         Operands: reg(object), identifier(name), reg(value), imm(cache) */

    ESI_GETI_IMM,
    /**< Get indexed property where the index is a compile-time constant.

         Operands: reg(value), reg(object), imm(index) */

    ESI_PUTI_IMM,
    /**< Put indexed property where the index is a compile-time constant.

         Operands: reg(object), imm(index), reg(value) */

    ESI_GET,
    /**< Get property where the name is a compile-time non-constant.  The
         property accessed can be either named or indexed.

         Operands: reg(value), reg(object), reg(name) */

    ESI_PUT,
    /**< Put property where the name is a compile-time non-constant.  The
         property accessed can be either named or indexed.

         Operands: reg(object), reg(name), reg(value) */

    ESI_DEFINE_GETTER,
    /**< Define property getter function for property.

         Operands: reg(object), identifier(name), imm(getter),
                   imm(innerScopes) */

    ESI_DEFINE_SETTER,
    /**< Define property setter function for property.

         Operands: reg(object), identifier(name), imm(setter),
                   imm(innerScopes) */

    ESI_GET_SCOPE,
    /**< Get property in scope chain.  Throws ReferenceError if property is not
         found.  If 'inner scopes' is not UINT_MAX, it specifies an array of
         register indeces holding objects that have been pushed onto the scope
         chain "inside" the local scope.  If 'local' is not UINT_MAX, it
         specifies a register index in the local register frame where the named
         local variable is stored.  The local variable is used if the property
         is not found in an inner scope.

         Operands: reg(value), identifier(name), imm(innerScopes), reg(local),
                   imm(cache) */

    ESI_GET_SCOPE_REF,
    /**< Get property in scope chain and returns both its value and the object
         on which it was found.  Throws ReferenceError if property is not found.
         If 'inner scopes' is not UINT_MAX, it specifies an array of register
         indeces holding objects that have been pushed onto the scope chain
         "inside" the local scope.  If 'local' is not UINT_MAX, it specifies a
         register index in the local register frame where the named local
         variable is stored.  The local variable is used if the property is not
         found in an inner scope.

         Operands: reg(value), reg(object), identifier(name), imm(innerScopes),
                   reg(local), imm(cache) */

    ESI_PUT_SCOPE,
    /**< Put property in scope chain.  If not found, the property is put on the
         global object.  If 'inner scopes' is not UINT_MAX, it specifies an
         array of register indeces holding objects that have been pushed onto
         the scope chain "inside" the local scope.  If 'local' is not UINT_MAX,
         it specifies a register index in the local register frame where the
         named local variable is stored.  The local variable is used if the
         property is not found in an inner scope.

         Operands: identifier(name), reg(value), imm(innerScopes), reg(local) */

    ESI_DELETE_SCOPE,
    /**< Delete property in scope chain and return true unless it the property
         found had the DontDelete flag.  If not found, true is returned.  If
         'inner scopes' is not UINT_MAX, it specifies an array of register
         indeces holding objects that have been pushed onto the scope chain
         "inside" the local scope.  If 'local' is not UINT_MAX, it specifies a
         register index in the local register frame where the named local
         variable is stored.  The local variable is used if the property is not
         found in an inner scope, but since the local is guaranteed to have the
         DontDelete flag, 'local' not being UINT_MAX simply means that false is
         returned.

         Operands: identifier(name), imm(innerScopes), reg(local) */

    ESI_GET_GLOBAL,
    /**< Get variable in the global object.  This is a special case of
         ESI_GET_SCOPE when the scope chain is known to be empty aside from the
         local scope, which is known not to contain the name, and thus only the
         global object needs to be searched.  At program start time, this
         instruction can be transformed into ESI_GET_GLOBAL_IMM if the named
         variable is a DontDelete property of the global object.

         Operands: reg(value), identifier(name), imm(cache) */

    ESI_PUT_GLOBAL,
    /**< Get variable in the global object.  This is a special case of
         ESI_PUT_SCOPE when the scope chain is known to be empty aside from the
         local scope, which is known not to contain the name, and thus no scope
         chain search is necessary.  At program start time, this instruction can
         be transformed into ESI_PUT_GLOBAL_IMM if the named variable is a
         DontDelete property of the global object.

         Operands: identifier(name), reg(value), imm(cache) */

    ESI_GET_LEXICAL,
    /**< Get a local variable in an enclosing function's scope.  The 'scope'
         operand is an index into the current function's
         ES_Function::scope_chain array, and the 'index' operand is a regular
         property index on the ES_Object found there.

         Operands: reg(value), imm(scope), imm(index) */

    ESI_PUT_LEXICAL,
    /**< Put a local variable in an enclosing function's scope.  The 'scope'
         operand is an index into the current function's
         ES_Function::scope_chain array, and the 'index' operand is a regular
         property index on the ES_Object found there.

         Operands: imm(scope), imm(index), reg(value) */

    ESI_DELETEN_IMM,
    /**< Delete a property where the name is known compile time.  If the
         property is found and has the DontDelete flag, the implicit boolean
         register is set to false, otherwise it is set to true.

         Operands: reg(object), identifier(name) */

    ESI_DELETEI_IMM,
    /**< Delete indexed property where the index is a compile-time constant.

         Operands: reg(object), imm(index) */

    ESI_DELETE,
    /**< Delete property where the name is a compile-time non-constant.

         Operands: reg(object), reg(name) */

    ESI_DECLARE_GLOBAL,
    /**< Declares a global variable.  Only used when it is not initialized to
         determine if it should be set to undefined or not, i.e., if it has
         already been declared.

         Operands: identifier(name) */

    ESI_HASPROPERTY,
    /**< Check object or its prototypes has property.  This instruction
         corresponds to the 'in' operator and is not used otherwise.

         Operands: reg(object), reg(name) */

    ESI_INSTANCEOF,
    /**< Check if an object is an "instance of" a constructor.  This instruction
         corresponds to the 'instanceof' operator and is not used otherwise.

         Operands: reg(object), reg(constructor) */

    ESI_ENUMERATE,
    /**< Initiate enumeration of an object's properties.  The value stored in
         the register operand 'enum' is an intermediate value of undefined type
         used by the VM.  The register is the same as the 'enum' operand to
         ESI_NEXT_PROPERTY.  The object stored in 'object' is the object whose
         properties is enumerated.  The the incoming value of object need not be
         an object, but it should be convertible to an object.  The result of
         that conversion is written back to 'object'.  If 'object' is null or
         undefined ESI_ENUMERATE does nothing.  The 'count' operand is a
         property counter used by ESI_NEXT_PROPERTY but initialised by
         ESI_ENUMERATE.

         Operands: reg(enum), reg(object), reg(count) */

    ESI_NEXT_PROPERTY,
    /**< Continue property enumeration previously started by ESI_ENUMERATE.  If
         there are more properties, the next one's name is stored in the
         register specified by the 'name' operand, and the implicit boolean
         register is set to true.  If all properties have been processed
         already, the implicit boolean register is set to false.  The 'object'
         operand is the same object as the corresponding operand to
         ESI_ENUMERATE.  If 'object' is null or undefined, the enumeration is
         stopped at once.  The 'count' operand keeps track of how many
         properties that has been enumerated when it isn't clear from the
         enumeration structure in 'enum' how many properties that has been
         enumerated or how many properties that are left to enumerate.

         Operands: reg(name), reg(enum), reg(object), reg(count) */

    ESI_EVAL,
    /**< Calls eval function in the plain case, i.e., eval().  If the function
         is not eval it fallbacks to ESI_CALL.  If the the innerScopes operand
         is UINT_MAX, the eval call is not within a with() or catch().  If the
         innerScopes operand is UINT_MAX-1, the same is true, and also the scope
         situation is simple as long as the eval call itself doesn't declare any
         local names.  This means essentially that the eval call is the only one
         in the function, and that it will only run once (that is, it is not in
         a loop.) In this case, the eval:ed code can be generated assuming that
         the enclosing function's local scope is predictable and stable.

         Operands: reg(frame), imm(argc), imm(innerScopes), imm(cache) */

    ESI_CALL,
    /**< Call function.  The register operand 'frame' specifies the first of
         2+argc registers that the instruction uses.  The instruction can modify
         all its register operands.  The first register doubles as 'this' object
         and return value.  The second register is the function object to call.
         The rest are the arguments.  If the most signifcant bit in 'argc' is
         set, the global object should be used as the 'this' object, and the
         corresponding register's initial value is undefined.

         Note: this instruction's impact on the register set is too complex to
         be described in this format, so it needs to be handled especially.

         Operands: reg(frame), imm(argc) */

    ESI_REDIRECTED_CALL,
    /**< Call function with same this object and arguments as current call.  This
         instruction is generated in special cases when ".apply(this,
         arguments)" is used, as an optimization.  What it essentially does is
         it verifies that the object in the register operand 'apply' is the real
         built-in apply function, and if it is, copies the value in the register
         operand 'function' to register 1 and calls it with a fully overlapping
         register frame.

         This instruction must be followed by an ESI_RETURN_NO_VALUE
         instruction, or an ESI_RETURN_VALUE instruction whose value operand is
         zero (that is, one which returns the value returned by redirected
         function call.) No other instruction can be relied on to work correctly
         after this instruction has finished.

         Operands: reg(function), reg(apply) */

    ESI_APPLY,
    /**< Used for function calls on the form "x.apply(a, [b, c, d])" on the
         assumption that '.apply' will be the built-in apply function.  Flattens
         the array literal on the stack instead of allocating an object.  The
         three registers beginning with the 'frame' register contains 'apply',
         'a' and 'x' and the 'argc' operand is the flattened number of arguments
         (4 in the example.) That is, if the apply function checks out, the call
         can be performed directly as if ESI_CALL had been used with 'frame'
         being one higher.  Otherwise, the register frame is shuffled, and an
         array object is created, and the call to 'not actually apply' is
         performed instead.

         Operands: reg(frame), imm(argc) */

    ESI_CONSTRUCT,
    /**< Construct object.  This instruction is very similar to ESI_CALL.  The
         first register operand is used only for the return value, since the
         'this' object, if there is one, is automatically created before the
         constructor is called.  The return value is always an object; if the
         constructor tries to return anything else it is either ignored, or an
         error is thrown.

         Note: this instruction's impact on the register set is too complex to
         be described in this format, so it needs to be handled especially.

         Operands: reg(frame), imm(argc) */

    ESI_RETURN_VALUE,
    /**< Return from function with a return value provided.

         Operands: reg(value) */

    ESI_RETURN_NO_VALUE,
    /**< Return from function without return value provided (return value then
         defaults to 'undefined'.) */

    ESI_RETURN_FROM_EVAL,
    /**< Return from a "program" started via eval().

         Operands: reg(value) */

    ESI_NEW_OBJECT,
    /**< Create plain object.  Same as "new Object" assuming the Object
         constructor hasn't been overridden.  Generated by object literal
         expressions.

         Operands: reg(object), imm(class) */

    ESI_CONSTRUCT_OBJECT,
    /**< Create plain object and initialize a given set of properties from
         registers.  The specified class determines the number of properties to
         initialize, and the values are given as a variable number of extra
         register operands.  This instruction is essentially equivalent to
         ESI_NEW_OBJECT followed by a set of ESI_PUTN_IMM instructions.

         Operands: reg(object), imm(class), reg(srcN) */

    ESI_NEW_ARRAY,
    /**< Create an Array object.  Same as "new Array" assuming the Array
         constructor hasn't been overridden.  Generated by array literal
         expressions.

         Operands: reg(array), imm(length) */

    ESI_CONSTRUCT_ARRAY,
    /**< Create an Array object and initialize it from an element from the array
         ES_Code::constant_array_literals.  Used for array literals where many
         initializer elements are constants.  Non-constant elements may be added
         later using ESI_PUTI_IMM instructions.

         Operands: reg(array), imm(length), imm(template) */

    ESI_NEW_FUNCTION,
    /**< Create a Function object for a nested function or function expression.
         The 'function' operand is an index into the ES_Code::functions array.
         The created function's scope chain is set to the current scope.

         Operands: reg(object), imm(function), imm(innerScopes) */

    ESI_NEW_REGEXP,
    /**< Create a RegExp object.  The 'regexp' operand is an index into the
         ES_Code::regexps array.

         Operands: reg(object), imm(regexp) */

    ESI_TABLE_SWITCH,
    /**< Perform jump based on value in register.  This is used for switch
         statements where all cases are constant integers.

         Operands: reg(value), imm(table) */

    ESI_CATCH,
    /**< "Catch" exception by copying the 'current exception' register into a
         regular register and then clearing the 'current exception' register.  It
         is possible that the 'current exception' register is already empty when
         this instruction is executed, in which case the 'exception' register is
         set to a special value representing this.  That will however only
         happen for 'finally' clauses, in which case the 'exception' register
         value will only be used by a subsequent ESI_RETHROW instruction which
         is prepared to handle the special value.  (That is, the ESI_CATCH
         belonging to a 'catch' clause will never be executed unless there is a
         current exception to catch.)

         Operands: reg(exception) */

    ESI_CATCH_SCOPE,
    /**< "Catch" exception by creating an empty object and putting a property
         named 'name' on it whose initial value is copied from the 'current
         exception' register.  The 'current exception' register is then cleared.

         Operands: reg(object), identifier(name) */

    ESI_THROW,
    /**< Throw exception.

         Operands: reg(exception) */

    ESI_THROW_BUILTIN,
    /**< Throw built-in exception.  Operands value is interpreted as follows:

         1: assignment to non-lvalue

         Operands: imm(exception) */

    ESI_RETHROW,
    /**< If the 'exception' operand contains an exception value, throw it as by
         ESI_THROW.  Otherwise, lookup the closest finally handler that would be
         crossed by jumping to target.  If such a handler exists set ip to this,
         set the implicit boolean register and copy the contents of target to
         nexttarget.

         Operands: reg(exception), reg(target), reg(nexttarget) */

    ESI_EXIT,
    /**< Exit execution of program.  This instruction is not generated by the
         compiler, it is injected automatically by the VM to detect when the
         top-most "real" stack frame finishes. */

    ESI_EXIT_TO_BUILTIN,
    /**< Like ESI_EXIT but used when a built-in function calls a native
         function. */

    ESI_EXIT_TO_EVAL,
    /**< Like ESI_EXIT but used when eval() executes code. */

#ifdef ECMASCRIPT_DEBUGGER
    ESI_DEBUGGER_STOP,
    /**< Calls debugger callback with the event type provided.

         Operands: imm(index) */
#endif

    ESI_LAST_INSTRUCTION
    /**< Not a real instruction.  :-) */
};

#endif // ES_INSTRUCTION_H
