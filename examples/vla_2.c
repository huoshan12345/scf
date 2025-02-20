int printf(const char* fmt, ...);

int main(int argc, char* argv[])
{
	int i;
	int j;

	for (i = 1; i < 4; i++) {
		char a[i];

		for (j = 0; j < i; j++) {

			a[j] = '1';

			printf("i: %d, j: %d, a: %c, %lg\n", i, j, a[j], 3.14);
		}
	}

	return 0;
}
