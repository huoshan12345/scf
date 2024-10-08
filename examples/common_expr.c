int printf(const char* fmt, ...);

int main()
{
	int a[4] = {1, 2, 3, 4};

	int* p = a;

	p += 2;

	int b = *p;
	int c = *p;
	int d = *p;

	printf("b: %d, c: %d, d: %d\n", b, c, d);
	return 0;
}
