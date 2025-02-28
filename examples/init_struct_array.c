int printf(const char* fmt, ...);

struct S
{
	char* name;
	int   type;
};

S s =
{
	.name = "sss",
	.type = 123,
};

#define A 1

int a[4] =
{
	[0] = 1,
	[A] = 2,
	[2] = 3,
	[3] = 4,
};

int main()
{
	printf("s->name: %s\n", s->name);
	printf("s->type: %d\n", s.type);

	printf("a[0]: %d\n", a[0]);
	printf("a[1]: %d\n", a[1]);
	printf("a[2]: %d\n", a[2]);
	printf("a[3]: %d\n", a[3]);
	return 0;
}
