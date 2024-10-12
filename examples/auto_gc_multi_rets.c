
#include "../lib/scf_capi.c"

int*, int* f()
{
	int* p0 = scf__auto_malloc(sizeof(int));
	int* p1 = scf__auto_malloc(sizeof(int));

	*p0 = 1;
	*p1 = 2;

	return p0, p1;
}

int main(int argc, char* argv[])
{
	int* p0;
	int* p1;

	p0, p1 = f();

	if (argc > 1)
		return 1;

	printf("%d, %d\n", *p0, *p1);
	return 0;
}
