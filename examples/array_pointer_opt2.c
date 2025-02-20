int printf(const char* fmt, ...);

int f()
{
	int a[2];

	int* p = a;

	a[0] = 1;
	a[1] = 2;

	*p += 3;
	int b = *p;
	return b;
}

int main()
{
	int i = 1;
	int a = 2;
	int b = 3;

	printf("%d\n", f());
	return 0;
}
