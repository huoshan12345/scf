int printf(const char* fmt, ...);
int strcmp(const char* s1, const char* s2);

int main()
{
	char* argv[2] = {"hello", "123"};

	switch (argv[1]) {

		case "456":
			printf("0\n");
		case "123":
			printf("1\n");
			break;
		default:
			printf("default\n");
			break;
	};

	return 0;
}
