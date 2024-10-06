#include"../lib/scf_capi.c"

#define assert(x)\
	do {\
		if (!(x))\
			printf("assert '%s' fail, %s, %d\n", #x, __func__, __LINE__);\
	} while (0)

struct buf_t
{
	int  refs;
	int  n;
	int  data[0];
};

struct mat_t
{
	int    i;
	int    j;
	int    n;
	buf_t* b;
};

int g_buf_size = 0;
int g_buf_size_max = 0;

buf_t* buf_alloc(int n)
{
	buf_t* b = malloc(sizeof(buf_t) + sizeof(int) * n * n);

	g_buf_size += n * n;
	if (g_buf_size_max < g_buf_size)
		g_buf_size_max = g_buf_size;

	b->refs = 1;
	b->n    = n;
	return b;
}

void buf_free(buf_t* b)
{
	if (b && 0 == --b->refs) {

		g_buf_size -= b->n * b->n;

		free(b);
	}
}

mat_t* mat_alloc(int i, int j, int n, buf_t* b)
{
	mat_t* m = malloc(sizeof(mat_t));
	assert(m);

	m->i = i;
	m->j = j;
	m->n = n;

	if (!b) {
		b = buf_alloc(n);
		assert(b);
	} else {
		b->refs++;
	}
	m->b = b;
	return m;
}

void mat_free(mat_t* m)
{
	if (m) {
		buf_free(m->b);
		free(m);
	}
}

void mat_add(mat_t* m, mat_t* m0, mat_t* m1)
{
	assert(m0->n == m1->n);
	assert(m->n  == m0->n);

	int i;
	int j;
	int n = m->n;

	for (i = 0; i < n; i++) {

		int* d0 = m0->b->data + (i + m0->i) * m0->b->n + m0->j;
		int* d1 = m1->b->data + (i + m1->i) * m1->b->n + m1->j;
		int* d  = m ->b->data + (i + m->i)  * m->b->n  + m->j;

		for (j = 0; j < n; j++)
			d[j] = d0[j] + d1[j];
	}
}

void mat_sub(mat_t* m, mat_t* m0, mat_t* m1)
{
	int i;
	int j;
	int n = m->n;

	for (i = 0; i < n; i++) {

		int* d0 = m0->b->data + (i + m0->i) * m0->b->n + m0->j;
		int* d1 = m1->b->data + (i + m1->i) * m1->b->n + m1->j;
		int* d  = m ->b->data + (i + m->i)  * m->b->n  + m->j;

		for (j = 0; j < n; j++)
			d[j] = d0[j] - d1[j];
	}
}

void mat_trans(mat_t* t, mat_t* m)
{
	assert(m->n == t->n);

	int i;
	int j;
	int n = m->n;

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {

			int ij = (i + m->i) * m->b->n + (j + m->j);
			int ji = (j + t->i) * t->b->n + (i + t->j);

			t->b->data[ji] = m->b->data[ij];
		}
	}
}

void mat_mul(mat_t* m, mat_t* m0, mat_t* m1)
{
	assert(m0->n == m1->n);
	assert(m->n  == m0->n);

	int i;
	int j;
	int k;
	int n = m->n;

	mat_t* t1 = mat_alloc(0, 0, n, NULL);

	mat_trans(t1, m1);

	for (i = 0; i < n; i++) {
		int* d0 = m0->b->data + (i + m0->i) * m0->b->n + m0->j;
		int* d  = m ->b->data + (i + m->i)  * m->b->n  + m->j;

		for (j = 0; j < n; j++) {
			int* d1 = t1->b->data + (j + t1->i) * t1->b->n + t1->j;

			int sum = 0;
			for (k  = 0; k < n; k++) {
				sum += d0[k] * d1[k];
			}

			d[j] = sum;
		}
	}

	mat_free(t1);
}

void mat_mul_strassen(mat_t* m, mat_t* m0, mat_t* m1, int n_min)
{
	assert(m0->n == m1->n);
	assert(m->n  == m0->n);

	if (m->n <= n_min) {
		mat_mul(m, m0, m1);
		return;
	}

	if (n_min < 16)
		printf("%s(), m->n: %d, n_min: %d\n", __func__, m->n, n_min);

	int n = m->n / 2;

	mat_t* a = mat_alloc(m0->i,     m0->j,     n, m0->b);
	mat_t* b = mat_alloc(m0->i,     m0->j + n, n, m0->b);
	mat_t* c = mat_alloc(m0->i + n, m0->j,     n, m0->b);
	mat_t* d = mat_alloc(m0->i + n, m0->j + n, n, m0->b);

	mat_t* e = mat_alloc(m1->i,     m1->j,     n, m1->b);
	mat_t* f = mat_alloc(m1->i,     m1->j + n, n, m1->b);
	mat_t* g = mat_alloc(m1->i + n, m1->j,     n, m1->b);
	mat_t* h = mat_alloc(m1->i + n, m1->j + n, n, m1->b);

	mat_t* r = mat_alloc(m->i,      m->j,      n, m->b);
	mat_t* s = mat_alloc(m->i,      m->j + n,  n, m->b);
	mat_t* t = mat_alloc(m->i + n,  m->j,      n, m->b);
	mat_t* u = mat_alloc(m->i + n,  m->j + n,  n, m->b);

	mat_t* p1 = mat_alloc(0, 0, n, NULL);
	mat_t* p2 = mat_alloc(0, 0, n, NULL);
	mat_t* p3 = mat_alloc(0, 0, n, NULL);

	// tmp mat t0
	mat_t* t0 = mat_alloc(0, 0, n, NULL);

	// p1 = a * (f - h)
	mat_sub(t0, f, h);
	mat_mul_strassen(p1, a, t0, n_min);
	// p2 = (a + b) * h
	mat_add(t0, a,  b);
	mat_mul_strassen(p2, t0, h, n_min);
	// s = p1 + p2
	mat_add(s, p1,  p2);

	// p3 = (c + d) * e
	mat_add(t0, c,  d);
	mat_mul_strassen(p3, t0, e, n_min);

	mat_sub(u, p1,  p3);

	// p4 = d * (g - e)
	mat_t* p4 = p1;
	p1 = NULL;

	mat_sub(t0, g,  e);
	mat_mul_strassen(p4, d,  t0, n_min);
	// t = p3 + p4
	mat_add(t, p3,  p4);
	mat_sub(r, p4,  p2);
//	mat_free(p2);
//	mat_free(p3);
//	mat_free(p4);

	// tmp mat t1
	mat_t* t1 = p2;
	mat_t* p5 = p3;
	mat_t* p6 = p4;
	p2 = NULL;
	p3 = NULL;
	p4 = NULL;

	//p5 = (a + d) * (e + h)
	mat_add(t0, a,  d);
	mat_add(t1, e,  h);
	mat_mul_strassen(p5, t0, t1, n_min);

	//p6 = (b - d) * (g + h)
	mat_sub(t0, b,  d);
	mat_add(t1, g,  h);
	mat_mul_strassen(p6, t0, t1, n_min);

	// r = p5 + p4 - p2 + p6
	mat_add(r, r,   p5);
	mat_add(r, r,   p6);

	//p7 = (a - c) * (e + f)
	mat_t* p7 = p6;
	p6 = NULL;

	mat_sub(t0, a,  c);
	mat_add(t1, e,  f);
	mat_mul_strassen(p7, t0, t1, n_min);

	// u = p5 + p1 -p3 -p7
	mat_add(u, u,   p5);
	mat_sub(u, u,   p7);

	// free unused mats
	mat_free(t0);
	mat_free(t1);

	mat_free(p5);
	mat_free(p7);

	mat_free(a);
	mat_free(b);
	mat_free(c);
	mat_free(d);

	mat_free(e);
	mat_free(f);
	mat_free(g);
	mat_free(h);

	mat_free(r);
	mat_free(s);
	mat_free(t);
	mat_free(u);
}

void mat_fill(mat_t* m)
{
	assert(m && m->b);

	int i;
	int j;

	for (i = 0; i < m->n; i++) {
		for (j = 0; j < m->n; j++) {

			int ij = (i + m->i) * m->b->n + (j + m->j);

			m->b->data[ij] = rand() % 10;
		}
	}
}

void mat_print(mat_t* m)
{
	assert(m && m->b);

	int i;
	int j;

	printf("m: %p, i: %d, j: %d, n: %d, m->b->n: %d, m->b: %p\n",
			m, m->i, m->j, m->n, m->b->n, m->b);

	for (i = 0; i < m->n; i++) {
		for (j = 0; j < m->n; j++) {

			int ij = (i + m->i) * m->b->n + (j + m->j);

			printf("%8d ", m->b->data[ij]);
		}
		printf("\n");
	}
	printf("\n");
}

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("./mat_mul n flag:\n");
		printf("n: nxn mat, n = 2^N, N > 0\n");
		printf("flag: 0 (normal), 1 (strassen), 2 (all & print)\n");
		return -1;
	}

	int n    = atoi(argv[1]);
	int flag = atoi(argv[2]);

	if (n < 2) {
		printf("n must >= 2, n: %d\n", n);
		return -1;
	}

	if (n & (n - 1)) {
		printf("n: %d, not 2^N\n", n);
		return -1;
	}

	srand(time(NULL));

	mat_t* m0 = mat_alloc(0, 0, n, NULL); 
	mat_t* m1 = mat_alloc(0, 0, n, NULL);
	mat_t* m2 = mat_alloc(0, 0, n, NULL);
	mat_t* m3 = mat_alloc(0, 0, n, NULL);

	mat_fill(m0);
	mat_fill(m1);

	printf("%s(), g_buf_size_max: %d\n", __func__, g_buf_size_max);

	switch (flag) {
		case 0:
			mat_mul(m2, m0, m1);
			break;
		case 1:
			mat_mul_strassen(m3, m0, m1, 64);
			break;
		case 2:
			mat_mul(m2, m0, m1);
			mat_mul_strassen(m3, m0, m1, 2);

			mat_print(m0);
			mat_print(m1);
			mat_print(m2);
			mat_print(m3);
			break;
		default:
			printf("trans: \n");
			mat_trans(m1, m0);
			mat_print(m0);
			mat_print(m1);
			break;
	};

	printf("%s(), g_buf_size_max: %d\n", __func__, g_buf_size_max);
	return 0;
}
