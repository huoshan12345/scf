int printf(const char* fmt, ...);

int main()
{
	for (int i = 0; ; i++) {
		printf("%d\n", i);
		if (i >= 9)
			break;
	}

	return 0;
}
