int printf(const char* fmt, ...);

int main(int argc, char* argv[])
{
	int i;
	for (i = 1; i < 4; i++) {
		char a[i];

		a[0] = '1';

		printf("i: %d, a[0]: %c, %lg\n", i, a[0], 3.14);
	}

	return 0;
}
