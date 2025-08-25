int printf(const char* fmt, ...);

enum {
	ANON_0,
	ANON_1,
	ANON_2 = ANON_1,
	ANON_3,
};

int main(int argc)
{
	switch (argc) {
		case ANON_0:
			printf("ANON_0: %d\n", ANON_0);
			break;

		case ANON_1:
			printf("ANON_1: %d\n", ANON_1);
			break;

		case ANON_3:
			printf("ANON_3: %d\n", ANON_3);
			break;
		default:
			break;
	};

	printf("%d,%d,%d,%d\n", ANON_0, ANON_1, ANON_2, ANON_3);
	return 0;
}
