OPTION PROLOGUE:NONE

; must be kept in sync with the declaration in crashlog_windows.cpp!
_mg_config STRUCT
	stacksize dd ?
	max_alloccount dd ?
	malloc_offset dq ?
	func1 dq ?
	free_offset dq ?
	func2 dq ?
	realloc_offset dq ?
	func3 dq ?
_mg_config ENDS

EXTERN GetAllocFrame:proc, DoMalloc:proc, DoFree:proc, DoRealloc:proc, WriteLongStack:proc, mg_config:_mg_config

.CODE

MemGuard_malloc PROC
	push rcx
	lea rcx, [rsp+8]
	call GetAllocFrame
	pop rcx
	mov rdx, rax
MemGuard_malloc ENDP
;fall through
internal_malloc PROC
	push rcx			; make room for local variable "mem"
	mov r8, rsp		; &mem
	push rcx
	sub rsp, 20h		; parameter stackspace
	call DoMalloc   ; rcx = size, rdx = stack_frame
	add rsp, 20h
	pop rcx

	test eax, eax
	pop rax			; return value = mem
	jne retrn

	mov rax, mg_config.malloc_offset
	add rax, 15
	mov [rsp+8], rbx
	mov [rsp+10h], rsi
	push rdi
	sub rsp, 20h
	jmp rax			; jump to original malloc

retrn:
	ret
internal_malloc ENDP


MemGuard_free PROC
	test rcx, rcx	; null delete?
	je retrn

	mov rdx, rsp		; stack_frame
	push rcx
	sub rsp, 20h		; parameter stackspace
	call DoFree
	add rsp, 20h
	pop r8

	test eax,eax
	jne retrn

	mov rax, mg_config.free_offset
	add rax, 13
	push rbx
	sub rsp, 20h
	jmp rax			; jump to original free
retrn:
	ret
MemGuard_free ENDP


MemGuard_realloc PROC
	mov r8, rsp		; stack_frame

	push rcx			; make room for local variable "new_mem"
	mov r9, rsp		; &new_mem
	push rcx
	push rdx
	sub rsp, 20h		; parameter stackspace
	call DoRealloc	; rcx = size, rdx = new_size
	add rsp, 20h
	pop rdx
	pop rcx

	test eax,eax
	pop rax			; return value = new_mem
	jne retrn

	mov rax, mg_config.realloc_offset
	add rax, 15
	mov [rsp+8], rbx
	mov [rsp+10h], rsi
	push rdi
	sub rsp, 20h
	jmp rax			; jump to original realloc

retrn:
	ret	
MemGuard_realloc ENDP


MemGuard_breakpoint PROC
	push rsp
	push rbp
	push r15
	push r14
	push r13
	push r12
	push r11
	push r10
	push r9
	push r8
	push rdi
	push rsi
	push rdx
	push rcx
	push rbx
	push rax
	mov r8,rsp
	push 15
	pop r9
	add qword ptr [r8+120],16	; modify rsp content to match dump
	lea rcx,[r8+144]
	sub dword ptr [rcx],13		; adjust return address to point at the breakpoint instruction
	sub rsp, 20h		; parameter stackspace
	call WriteLongStack
	add rsp, 20h
	pop rax
	pop rbx
	pop rcx
	pop rdx
	pop rsi
	pop rdi
	pop r8
	pop r9
	pop r10
	pop r11
	add rsp,48					; other registers haven't been modified
	ret
MemGuard_breakpoint ENDP

END
