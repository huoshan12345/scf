.text
.global main, printf

main:
	push %rbp
	movq pointer, %rdi
	xorq %rax, %rax
	call printf
	pop  %rbp
	ret

.data
pointer: .quad hello
hello: .asciz "hello world\n"
