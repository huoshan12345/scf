#include"scf_asm.h"

void usage(char* path)
{
	fprintf(stderr, "Usage: %s [-a arch] [-s sysroot] [-o out] src0 [src1]\n\n", path);
	fprintf(stderr, "-a: select cpu arch (x64, arm64, naja), default is x64\n");
	fprintf(stderr, "-s: sysroot dir, default is '../lib'\n");
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		usage(argv[0]);
		return -EINVAL;
	}

	scf_vector_t* srcs = scf_vector_alloc();

	char* sysroot = "../lib";
	char* arch    = "x64";
	char* out     = NULL;

	int i;
	for (i = 1; i < argc; i++) {

		if ('-' == argv[i][0]) {

			if ('a' == argv[i][1]) {

				if (++i >= argc) {
					usage(argv[0]);
					return -EINVAL;
				}

				arch = argv[i];
				continue;
			}

			if ('s' == argv[i][1]) {

				if (++i >= argc) {
					usage(argv[0]);
					return -EINVAL;
				}

				sysroot = argv[i];
				continue;
			}

			if ('o' == argv[i][1]) {

				if (++i >= argc) {
					usage(argv[0]);
					return -EINVAL;
				}

				out = argv[i];
				continue;
			}

			usage(argv[0]);
			return -EINVAL;
		}

		char*  fname = argv[i];
		size_t len   = strlen(fname);

		if (len < 3) {
			fprintf(stderr, "file '%s' invalid\n", fname);
			return -1;
		}

		scf_logi("fname: %s\n", fname);

		scf_vector_t* vec;

		if (!strcmp(fname + len - 2, ".s") || !strcmp(fname + len - 2, ".S"))
			vec = srcs;
		else {
			fprintf(stderr, "file '%s' invalid\n", fname);
			return -1;
		}

		scf_logi("fname: %s\n", fname);
		if (scf_vector_add(vec, fname) < 0)
			return -ENOMEM;
	}

	printf("\n");

	scf_asm_t*  _asm = NULL;

	if (scf_asm_open(&_asm, arch) < 0) {
		scf_loge("\n");
		return -1;
	}

	for (i = 0; i  < srcs->size; i++) {
		char* file = srcs->data[i];

		assert(file);

		if (scf_asm_file(_asm, file) < 0) {
			scf_loge("parse file '%s' failed\n", file);
			return -1;
		}
	}

	char* obj  = "1.o";
	if (out)
		obj = out;

	if (scf_asm_to_obj(_asm, obj, arch) < 0) {
		scf_loge("\n");
		return -1;
	}

	scf_asm_close(_asm);

	printf("%s(),%d, main ok\n", __func__, __LINE__);
	return 0;
}
