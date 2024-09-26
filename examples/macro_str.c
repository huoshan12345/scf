#define A(x) #x

int printf(const char* fmt, ...);

int main()
{
	printf("%s\n", A(zzz));
	return 0;
}
