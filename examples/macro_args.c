#define Plus(a, b) (a + b)

int printf(const char* fmt, ...);

int main()
{
	printf("%d\n", Plus(Plus(1, 2), 3));
	return 0;
}
