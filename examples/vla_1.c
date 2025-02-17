int printf(const char* fmt, ...);

int main(int argc, char* argv[])
{
	char a[argc - 1];

	a[0] = '1';

	printf("a[0]: %c, %lg\n", a[0], 3.14);
	return 0;
}
