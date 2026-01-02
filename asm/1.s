.text
.global main, printf

main:
	push %rbp
	leaq hello, %rdi
	xorq %rax, %rax
	call printf
	pop  %rbp
	ret

.data
hello: .asciz "hello world\n"
