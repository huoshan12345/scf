int printf(const char* fmt, ...);

int scf_async(uintptr_t funcptr, const char* fmt, ...)
{
	printf("fmt: %s\n", fmt);
	return 0;
}

int main()
{
	async printf("hello world, %d\n", 1, 2, 3, 4, 5, 6, 7, 8);
	return 0;
}
