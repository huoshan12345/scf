int printf(const char* fmt, ...);

enum {
	ANON_0,
	ANON_1,
	ANON_2 = ANON_1,
	ANON_3,
};

enum TEST {
	TEST_0 = 7,
	TEST_1,
	TEST_2,
	TEST_3
};

int main()
{
	printf("ANON_0: %d, ANON_1: %d, ANON_2: %d, ANON_3: %d\n", ANON_0, ANON_1, ANON_2, ANON_3);
	printf("TEST_0: %d, TEST_1: %d, TEST_2: %d, TEST_3: %d\n", TEST_0, TEST_1, TEST_2, TEST_3);

	return 0;
}
