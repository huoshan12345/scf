int printf(const char* fmt, ...);

#define assert(x) \
	do { \
		if (!(x)) {\
			printf("assert: '%s' failed. file: %s, line: %d\n", #x, __FILE__, __LINE__); \
			*(int*)0 = 0; \
		} \
	} while (0)

int main()
{
	assert(0);
	return 0;
}
