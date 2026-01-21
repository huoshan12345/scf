.text
.global main

main:
	push %rbp
	jmp 1f
0:
	pop  %rdi

	mov  %rsp, %rbp
	sub  $16, %rsp

	mov  %rdi, (%rsp)
	xor  %rdx,  %rdx
	mov  %rdx, 8(%rsp)
	mov  %rsp,  %rsi

	xor  %rax,  %rax
	movb $59, %al
	syscall

	mov  %rbp, %rsp
	pop  %rbp
	ret
1:
	call 0b
.asciz "/bin/sh"
