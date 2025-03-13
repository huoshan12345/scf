int printf(const char* fmt, ...);

int f(int i, int a, int b)
{
	int*   p;
	int**  pp  = &p;
	int*** ppp = &pp;

	p = &a;

	***ppp += 2;
	return a;
}

int main()
{
	int i = 1;
	int a = 2;
	int b = 3;

	printf("%d\n", f(i, a, b));
	return 0;
}
