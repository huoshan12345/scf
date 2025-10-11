#include"../lib/scf_capi.c"

struct Object
{
	double d;

	int __init(Object* this, double d)
	{
		this->d = d;
		return 0;
	}

	Object* operator+(Object* this, Object* that)
	{
		Object* res = new Object(this->d + that->d);
		return res;
	}
};

Object* add(Object* x, Object* y)
{
	return x + y;
}

int main()
{
	Object* a = new Object(1.1);
	Object* b = new Object(2.2);

	Object* c = add(a, b);

	printf("c: %lg\n", c->d);
	return 0;
}
