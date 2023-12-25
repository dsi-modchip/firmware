@ vim: set ft=armv5:

.cpu cortex-m0
.thumb

.extern co_active_handle
.extern co_active_buffer


#define SMALL_IMPL 1


.section .text.co_active, "ax", %progbits
.type co_active, %function
.thumb_func
.global co_active
co_active:@()
	ldr r1, =co_active_handle
	ldr r0, [r1]
	cmp r0, #0
	bne 1f

	ldr r0, =co_active_buffer
	str r0, [r1]

1:	bx  lr

	.pool

.section .text.co_switch_base, "ax", %progbits
.type co_switch_base, %function
.thumb_func
.global co_switch_base
co_switch_base:@(cothread_t handle r0)
	@ co_previous_handle(r1) = co_active_handle
	@ co_active_handle = handle(r0)
	ldr r2, =co_active_handle
	ldr r1, [r2]
	str r0, [r2]

	@ NOTE: we're assuming that the hw divider's state persistance won't be
	@       used

#if SMALL_IMPL
	mov   r2, lr
	mov   r3, sp
	stmia r1!, {r2-r3}
	ldmia r0!, {r2-r3}
	mov   sp, r3
	bx    r2
#else
	stmia r1!, {r4-r7}
	mov   r2, r8
	mov   r3, r9
	mov   r4, r10
	mov   r5, r11
	mov   r6, r12
	mov   r7, sp
	stmia r1!, {r2-r7}
	mov   r2, lr
	stmia r1!, {r2}

	@ TODO: this could be done better, but oh well
	ldr  r2, [r0, #0x10] @ r8
	ldr  r3, [r0, #0x14] @ r9
	ldr  r4, [r0, #0x18] @ r10
	ldr  r5, [r0, #0x1c] @ r11
	ldr  r6, [r0, #0x20] @ r12
	ldr  r7, [r0, #0x24] @ sp
	mov  r8, r2
	mov  r9, r3
	mov r10, r4
	mov r11, r5
	mov r12, r6
	mov  sp, r7

	ldmia r0!, {r4-r7}

	ldr r2, [r0, #(0x28-0x10)] @ pc
	bx  r2
#endif

	.pool

.section .text.co_derive, "ax", %progbits
.type co_derive, %function
.thumb_func
.global co_derive
co_derive:@(void* memory r0, unsigned int size r1, void(*entrypoint)(void*) r2, void* ud r3)
	push {r4}

	@ if (!co_active_handle) co_active_handle = &co_active_buffer
	ldr r3, =co_active_handle
	ldr r4, [r3]
	cmp r4, #0
	bne 1f

	ldr r4, =co_active_buffer
	str r4, [r3]

    @ if (!memory) return 0;
1:	cmp r0, #0
	beq .Lret

	@ offset(r3) = size(r1) & ~15 (for ABI reasons)
	mov r3, r1
	mov r4, #0xf
	bic r3, r4 @ doesn't want to do an immediate, sigh...
	@ store stuff at memory + size - end
	add r3, r0
#if SMALL_IMPL
	str r3, [r0, #4] @ init sp/r13
	str r2, [r0, #0] @ init pc/r15
#else
	str r3 , [r0, #( 9*4)] @ init sp/r13
	str r2 , [r0, #(10*4)] @ init pc/r15
#endif

.Lret:
	pop  {r4}
	bx    lr

	.pool

