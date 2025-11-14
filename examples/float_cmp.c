int printf(const char* fmt, ...);

int f(double a, double b)
{
	return a < b;
}

int main()
{
	printf("%d\n", f(0.0, 1.0));
	return 0;
}
