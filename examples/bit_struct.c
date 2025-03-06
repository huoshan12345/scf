int printf(const char* fmt, ...);

struct S
{
	uint32_t a:3;
	uint16_t b:13;
	uint8_t c:5;
};

int main()
{
	S s;

	s.a = 1;
	s.b = 2;
	s.c = 3;

//	uint32_t* p = &s.a;

	printf("%d, %d, %d\n", s.a, s.b, s.c);
	return 0;
}
