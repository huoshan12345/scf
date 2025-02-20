#include "../lib/scf_capi.c"

struct str
{
	uint8_t* data;
	int      len;
	int      capacity;

	int __init(str* this)
	{
		this->len = 0;
		this->capacity = 16;

		this->data = scf__auto_malloc(16);
		if (!this->data)
			return -1;

		strncpy(this->data, "hello", 5);
		return 0;
	}

	void __release(str* this)
	{
		if (this->data)
			scf__auto_freep(&this->data, NULL);
	}
};

int main()
{
	str* p0;
	str* p1;

	p0 = create str();
	p1 = p0;

	printf("%s\n", p1->data);
	return 0;
}
