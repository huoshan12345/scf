#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    int  a;
    int  b;
} data;

data add(const data * d1, const data* d2)
{
    data result;
    result.a = d1->a + d2->a;
    result.b = d1->b + d2->b;
    return result;
}

data fun_a()
{
    data *a = (data *)malloc(sizeof(data));
    data *b = (data *)malloc(sizeof(data));
    a->a = 0x9999;
    b->a = 0x99;
    data result = add(a, b);

    free(a);
    free(b);

    return result;
}

int main(void)
{
    data result = fun_a();
    printf("result: %d %d\n", result.a, result.b);
    return 0;
}
