#ifndef SCF_X64_REG_UTIL_H
#define SCF_X64_REG_UTIL_H

#include"scf_native.h"
#include"scf_x64_util.h"

#define X64_COLOR(type, id, mask)   ((type) << 24 | (id) << 16 | (mask))
#define X64_COLOR_TYPE(c)           ((c) >> 24)
#define X64_COLOR_ID(c)             (((c) >> 16) & 0xff)
#define X64_COLOR_MASK(c)           ((c) & 0xffff)
#define X64_COLOR_CONFLICT(c0, c1)  ( (c0) >> 16 == (c1) >> 16 && (c0) & (c1) & 0xffff )

#define X64_COLOR_BYTES(c) \
	({ \
	     int n = 0;\
	     intptr_t minor = (c) & 0xffff; \
	     while (minor) { \
	         minor &= minor - 1; \
	         n++;\
	     } \
	     n;\
	 })

typedef struct {
	scf_register_t* base;
	scf_register_t* index;

	int32_t         scale;
	int32_t         disp;
	int32_t         size;
} x64_sib_t;

scf_register_t*     x64_find_register(const char* name);

scf_register_t*     x64_find_register_type_id_bytes(uint32_t type, uint32_t id, int bytes);

scf_register_t*     x64_find_register_color(intptr_t color);

scf_register_t*     x64_find_register_color_bytes(intptr_t color, int bytes);

static inline int   x64_inst_data_is_reg(scf_inst_data_t* id)
{
	scf_register_t* rsp = x64_find_register("rsp");
	scf_register_t* rbp = x64_find_register("rbp");

	if (!id->flag && id->base && id->base != rsp && id->base != rbp && 0 == id->imm_size)
		return 1;
	return 0;
}

static inline int   x64_inst_data_is_local(scf_inst_data_t* id)
{
	scf_register_t* rbp = x64_find_register("rbp");
	scf_register_t* rsp = x64_find_register("rsp");

	if (id->flag && (id->base == rbp || id->base == rsp))
		return 1;
	return 0;
}

static inline int   x64_inst_data_is_global(scf_inst_data_t* id)
{
	if (id->flag && !id->base)
		return 1;
	return 0;
}

static inline int   x64_inst_data_is_const(scf_inst_data_t* id)
{
	if (!id->flag && id->imm_size > 0)
		return 1;
	return 0;
}

static inline int   x64_inst_data_is_pointer(scf_inst_data_t* id)
{
	scf_register_t* rbp = x64_find_register("rbp");
	scf_register_t* rsp = x64_find_register("rsp");

	if (id->flag && id->base && id->base != rbp && id->base != rsp)
		return 1;
	return 0;
}

#endif
