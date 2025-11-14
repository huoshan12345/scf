
int printf(const char* fmt, ...);

int main()
{
	double d;
	for (d = 0.1; d < 10.0; d++)
		printf("%lg\n", d);
	return 0;
}

