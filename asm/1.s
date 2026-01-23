.text
.global main, printf

main:
	push %rbp
	jmp 1f
0:
	pop  %rdi
	xorq %rax, %rax
	call printf
	pop  %rbp
	ret
.align 3
.org 509
1:
	call 0b
.asciz "hello world\n"
