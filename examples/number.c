int printf(const char* fmt, ...);

int main()
{
	printf("0: %d, 10: %d, double: %lg, %lg, %lg, base16: %#lx, base8: %d, base2: %#lx\n",
			0, 1234, 3.14, .5, 1.1e-4, 0x7f1a, 0016, 0b110);

	return 0;
}
