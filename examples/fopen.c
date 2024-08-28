/*#include<stdio.h>
#include<stdint.h>
*/
void*   fopen(const char* path, const char* mode);
int     fclose(void* fp);
int     fscanf(void* fp, const char* fmt, ...);

int     printf(const char* fmt, ...);

int main()
{
	void* fp = fopen("1.txt", "r");
	if (!fp) {
		printf("fp: %p\n", fp);
		return -1;
	}

	char* s[64];

	int n = fscanf(fp, "%s", s);

	printf("s: %s, n: %d\n", s, n);

	fclose(fp);
	return 0;
}
