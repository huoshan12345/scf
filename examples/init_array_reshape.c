int printf(const char* fmt, ...);

int main()
{
	int a[2][2] = {1,  2,  3,  4};

	int v = 3;
	int i;
	int j;

	for (i = 0; i < 2; i++) {

		for (j = 0; j < 2; j++) {

			if (v == a[i][j])
				break;
		}

		if (j < 2)
			break;
	}

	printf("a[%d][%d] == v: %d\n", i, j, v);
	return 0;
}
