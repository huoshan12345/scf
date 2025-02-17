#include"scf_parse.h"
#include"scf_3ac.h"
#include"scf_x64.h"
#include"scf_elf_link.h"

static char* __objs[] =
{
	"_start.o",
	"scf_object.o",
	"scf_atomic.o",
};

static char* __sofiles[] =
{
	"/lib64/ld-linux-x86-64.so.2",
	"libc.so.6",
};

static char* __arm64_objs[] =
{
	"_start.o",
};

static char* __arm64_sofiles[] =
{
	"/lib/ld-linux-aarch64.so.1",
	"libc.so.6",
};

static char* __arm32_objs[] =
{
	"_start.o",
};

static char* __arm32_sofiles[] =
{
	"/lib/ld-linux-armhf.so.3",
	"libc.so.6",
};

void usage(char* path)
{
	fprintf(stderr, "Usage: %s [-c] [-t] [-a arch] [-s sysroot] [-o out] src0 [src1]\n\n", path);
	fprintf(stderr, "-c: only compile,  not link\n");
	fprintf(stderr, "-t: only 3ac code, not compile\n");
	fprintf(stderr, "-a: select cpu arch (x64, arm64, naja, or eda), default is x64\n");
	fprintf(stderr, "-s: sysroot dir, default is '../lib'\n");
}

int add_sys_files(scf_vector_t* vec, const char* sysroot, const char* arch, char* files[], int n_files)
{
	int len0 = strlen(sysroot);
	int len1 = strlen(arch);
	int i;

	for (i = 0; i < n_files; i++) {

		int len2 = strlen(files[i]);

		char* fname = calloc(1, len0 + len1 + len2 + 3);
		if (!fname)
			return -ENOMEM;

		int len = len0;
		memcpy(fname, sysroot, len0);
		if (fname[len - 1] != '/') {
			fname[len]      = '/';
			len++;
		}

		memcpy(fname + len, arch, len1);
		len += len1;
		if (fname[len - 1] != '/') {
			fname[len]      = '/';
			len++;
		}

		memcpy(fname + len, files[i], len2);
		len += len2;
		fname[len] = '\0';

		scf_logi("add file: %s\n", fname);

		int ret = scf_vector_add(vec, fname);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		usage(argv[0]);
		return -EINVAL;
	}

	scf_vector_t* afiles  = scf_vector_alloc();
	scf_vector_t* sofiles = scf_vector_alloc();
	scf_vector_t* srcs    = scf_vector_alloc();
	scf_vector_t* objs    = scf_vector_alloc();

	char* sysroot = "../lib";
	char* arch    = "x64";
	char* out     = NULL;
	int   link    = 1;
	int   _3ac    = 0;

	int   i;

	for (i = 1; i < argc; i++) {

		if ('-' == argv[i][0]) {

			if ('c' == argv[i][1]) {
				link = 0;
				continue;
			}

			if ('t' == argv[i][1]) {
				link = 0;
				_3ac = 1;
				continue;
			}

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

		if (!strcmp(fname + len - 2, ".a"))
			vec = afiles;

		else if (!strcmp(fname + len - 2, ".o"))
			vec = objs;

		else if (!strcmp(fname + len - 2, ".c"))
			vec = srcs;

		else if (!strcmp(fname + len - 3, ".so"))
			vec = sofiles;
		else {
			fprintf(stderr, "file '%s' invalid\n", fname);
			return -1;
		}

		scf_logi("fname: %s\n", fname);
		if (scf_vector_add(vec, fname) < 0)
			return -ENOMEM;
	}

	printf("\n");

	scf_parse_t*  parse = NULL;

	if (scf_parse_open(&parse) < 0) {
		scf_loge("\n");
		return -1;
	}

	for (i = 0; i  < srcs->size; i++) {
		char* file = srcs->data[i];

		assert(file);

		if (scf_parse_file(parse, file) < 0) {
			scf_loge("parse file '%s' failed\n", file);
			return -1;
		}
	}

	char* obj  = "1.o";
	char* exec = "1.out";

	if (out) {
		if (!link)
			obj  = out;
		else
			exec = out;
	}

	if (scf_parse_compile(parse, obj, arch, _3ac) < 0) {
		scf_loge("\n");
		return -1;
	}

	scf_parse_close(parse);

	if (!link) {
		printf("%s(),%d, main ok\n", __func__, __LINE__);
		return 0;
	}

#define MAIN_ADD_FILES(_objs, _sofiles, _arch) \
	do { \
		int ret = add_sys_files(objs, sysroot, _arch, _objs, sizeof(_objs) / sizeof(_objs[0])); \
		if (ret < 0) \
		    return ret; \
		\
		ret = add_sys_files(sofiles, sysroot, _arch, _sofiles, sizeof(_sofiles) / sizeof(_sofiles[0])); \
		if (ret < 0) \
		    return ret; \
	} while (0)


	if (!strcmp(arch, "arm64") || !strcmp(arch, "naja"))
		MAIN_ADD_FILES(__arm64_objs, __arm64_sofiles, "arm64");

	else if (!strcmp(arch, "arm32"))
		MAIN_ADD_FILES(__arm32_objs, __arm32_sofiles, "arm32");
	else
		MAIN_ADD_FILES(__objs, __sofiles, "x64");


	if (scf_vector_add(objs, obj) < 0) {
		scf_loge("\n");
		return -1;
	}

	if (scf_elf_link(objs, afiles, sofiles, sysroot, arch, exec) < 0) {
		scf_loge("\n");
		return -1;
	}

	printf("%s(),%d, main ok\n", __func__, __LINE__);
	return 0;
}
