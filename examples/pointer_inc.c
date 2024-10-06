int printf(const char* fmt, ...);

int main()
{
	int a[4] = {1, 2, 3, 4};

	int* p = a;

	++p;

	printf("*p: %d\n", *p);
	return 0;
}
