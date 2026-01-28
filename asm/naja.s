.text
.global main, printf

main:
	push  lr
	jmp   1f
0:
	movq  r0, lr
	call  printf
	mov   r0, 0
	pop   lr
	ret
.align 3
.org 508
1:
	call 0b
	.asciz "hello world\n"
