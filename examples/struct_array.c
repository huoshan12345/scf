int printf(const char* fmt, ...);

struct A {
	int x;
};

struct B
{
	A a[2];
};

B b = {
	{{1}, {2}}
};

int main()
{
	printf("%d\n", b.a[0].x);
	return 0;
}
