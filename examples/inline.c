//第 7 章/inline.c

int printf(const char* fmt, ...);

inline int add(int i, int j)
{
	return i + j;
}

int main()
{
	int a = 1;
	int b = 2;
	int c = add(a, b);
	printf("%d\n", c);
	return 0;
}
