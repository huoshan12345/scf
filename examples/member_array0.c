#include"../lib/scf_capi.c"

struct mat
{
	int refs;
	int n;
	int data[0];
};

int main()
{
	mat* m = calloc(1, sizeof(mat) + 4 * sizeof(int));

	m->data[0] = 1;
	m->data[1] = 2;
	m->data[2] = 3;
	m->data[3] = 4;

	int i;
	for (i = 0; i < 4; i++)
		printf("%d\n", m->data[i]);

	return 0;
}
