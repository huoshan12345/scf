
int printf(const char* fmt, ...);

int scf_async(uintptr_t funcptr, const char* fmt, ...);

int __async_loop();

int main()
{
	async printf("hello world, %d,%d,%d,%d, %lg,%lg,%lg,%lg, %lg,%lg,%lg,%lg, %lg-%lg\n", 1, 2, 3, 4,
			0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.1);

	__async_loop();
	return 0;
}
