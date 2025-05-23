#ifndef SCF_BASIC_BLOCK_H
#define SCF_BASIC_BLOCK_H

#include"scf_core_types.h"
#include"scf_list.h"
#include"scf_vector.h"

typedef struct scf_basic_block_s  scf_basic_block_t;

typedef struct scf_bb_edge_s      scf_bb_edge_t;
typedef struct scf_bb_group_s     scf_bb_group_t;

struct scf_bb_edge_s
{
	scf_basic_block_t*  start;
	scf_basic_block_t*  end;
};

struct scf_bb_group_s
{
	scf_basic_block_t*  entry;

	scf_basic_block_t*  pre;

	scf_vector_t*       posts;

	scf_vector_t*       entries;
	scf_vector_t*       exits;

	scf_vector_t*       body;

	scf_bb_group_t*     loop_parent;
	scf_vector_t*       loop_childs;
	int                 loop_layers;
};

struct scf_basic_block_s
{
	scf_list_t      list; // for function's basic block list

	scf_list_t      dag_list_head;

	scf_list_t      code_list_head;
	scf_list_t      save_list_head;

	scf_vector_t*   var_dag_nodes;

	scf_vector_t*   prevs; // prev basic blocks
	scf_vector_t*   nexts; // next basic blocks

	scf_vector_t*   dominators;
	int             dfo;

	scf_vector_t*   entry_dn_delivery;
	scf_vector_t*   entry_dn_inactives;
	scf_vector_t*   entry_dn_actives;
	scf_vector_t*   exit_dn_actives;

	scf_vector_t*   dn_updateds;
	scf_vector_t*   dn_loads;
	scf_vector_t*   dn_saves;

	scf_vector_t*   dn_colors_entry;
	scf_vector_t*   dn_colors_exit;

	scf_vector_t*   dn_status_initeds;

	scf_vector_t*   dn_pointer_aliases;
	scf_vector_t*   entry_dn_aliases;
	scf_vector_t*   exit_dn_aliases;

	scf_vector_t*   dn_reloads;
	scf_vector_t*   dn_resaves;

	scf_vector_t*   ds_malloced;
	scf_vector_t*   ds_freed;

	void*           ds_auto_gc;

	int             code_bytes;
	int             index;

	uint32_t        call_flag   :1;
	uint32_t        cmp_flag    :1;
	uint32_t        jmp_flag    :1;
	uint32_t        jcc_flag    :1;
	uint32_t        ret_flag    :1;
	uint32_t        vla_flag    :1;
	uint32_t        end_flag    :1;
	uint32_t        varg_flag   :1;
	uint32_t        dump_flag   :1;
	uint32_t        jmp_dst_flag:1;

	uint32_t        dereference_flag:1;
	uint32_t        array_index_flag:1;

	uint32_t        auto_ref_flag :1;
	uint32_t        auto_free_flag:1;

	uint32_t        back_flag   :1;
	uint32_t        loop_flag   :1;
	uint32_t        group_flag  :1;
	uint32_t        visit_flag  :1;
	uint32_t        native_flag :1;

	scf_basic_block_t*  vla_free;
};

typedef int       (*scf_basic_block_bfs_pt)(scf_basic_block_t* bb, void* data, scf_vector_t* queue);
typedef int       (*scf_basic_block_dfs_pt)(scf_basic_block_t* bb, void* data);

int                 scf_basic_block_search_bfs     (scf_basic_block_t* root, scf_basic_block_bfs_pt find, void* data);
int                 scf_basic_block_search_dfs_prev(scf_basic_block_t* root, scf_basic_block_dfs_pt find, void* data, scf_vector_t* results);


scf_basic_block_t*  scf_basic_block_alloc();
scf_basic_block_t*  scf_basic_block_jcc (scf_basic_block_t* to, scf_function_t* f, int jcc);
void                scf_basic_block_free(scf_basic_block_t* bb);

scf_bb_group_t*     scf_bb_group_alloc();
void                scf_bb_group_free (scf_bb_group_t* bbg);
void                scf_bb_group_print(scf_bb_group_t* bbg);
void                scf_bb_loop_print (scf_bb_group_t* loop);

void                scf_basic_block_print(scf_basic_block_t* bb, scf_list_t* sentinel);
void                scf_basic_block_print_list(scf_list_t* h);

int                 scf_basic_block_vars(scf_basic_block_t* bb, scf_list_t* bb_list_head);
int                 scf_basic_block_dag (scf_basic_block_t* bb, scf_list_t* dag_list_head);

int                 scf_basic_block_active_vars(scf_basic_block_t* bb);
int                 scf_basic_block_inited_vars(scf_basic_block_t* bb, scf_list_t* bb_list_head);
int                 scf_basic_block_loads_saves(scf_basic_block_t* bb, scf_list_t* bb_list_head);

int                 scf_basic_block_connect(scf_basic_block_t* prev_bb, scf_basic_block_t* next_bb);

int                 scf_basic_block_split(scf_basic_block_t* bb_parent, scf_basic_block_t** pbb_child);

int                 scf_basic_block_inited_by3ac(scf_basic_block_t* bb);

void                scf_basic_block_mov_code(scf_basic_block_t* to, scf_list_t* start, scf_basic_block_t* from);
void                scf_basic_block_add_code(scf_basic_block_t* bb, scf_list_t* h);

scf_bb_group_t*     scf_basic_block_find_min_loop(scf_basic_block_t* bb, scf_vector_t* loops);

static inline void scf_basic_block_visit_flag(scf_list_t* h, int visit_flag)
{
	scf_basic_block_t* bb;
	scf_list_t*        l;

	for (l = scf_list_head(h); l != scf_list_sentinel(h); l = scf_list_next(l)) {

		bb = scf_list_data(l, scf_basic_block_t, list);

		bb->visit_flag = visit_flag;
	}
}

#endif
