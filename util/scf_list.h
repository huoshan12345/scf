#ifndef SCF_LIST_H
#define SCF_LIST_H

#include"scf_def.h"

typedef struct scf_list_s	scf_list_t;

struct scf_list_s {
	struct scf_list_s*		prev;
	struct scf_list_s*		next;
};

static inline void scf_list_init(scf_list_t* h)
{
	h->prev = h;
	h->next = h;
}

static inline void scf_list_del(scf_list_t* n)
{
	n->prev->next = n->next;
	n->next->prev = n->prev;

	// only to avoid some wrong operations for these 2 invalid pointers
	n->prev = NULL;
	n->next = NULL;
}

static inline void scf_list_add_tail(scf_list_t* h, scf_list_t* n)
{
	h->prev->next = n;
	n->prev = h->prev;
	n->next = h;
	h->prev = n;
}

static inline void scf_list_add_front(scf_list_t* h, scf_list_t* n)
{
	h->next->prev = n;
	n->next = h->next;
	n->prev = h;
	h->next = n;
}

static inline void scf_list_mov2(scf_list_t* h0, scf_list_t* h1)
{
	if (h1->next == h1)
		return;

	h0->prev->next = h1->next;
	h1->next->prev = h0->prev;
	h1->prev->next = h0;
	h0->prev       = h1->prev;
}

#define SCF_LIST_INIT(h)	{&h, &h}

#define scf_list_data(l, type, member)	((type*)((char*)l - offsetof(type, member)))

#define scf_list_head(h)		((h)->next)
#define scf_list_tail(h)		((h)->prev)
#define scf_list_sentinel(h)	(h)
#define scf_list_next(l)		((l)->next)
#define scf_list_prev(l)		((l)->prev)
#define scf_list_empty(h)		((h)->next == (h))

#define scf_list_clear(h, type, member, type_free) \
	do {\
		scf_list_t* l;\
		type*       t;\
		for (l = scf_list_head(h); l != scf_list_sentinel(h);) {\
			t  = scf_list_data(l, type, member);\
			l  = scf_list_next(l);\
			scf_list_del(&t->member);\
			type_free(t);\
			t = NULL;\
		}\
	} while(0)

#define scf_slist_clear(h, type, next, type_free) \
	do { \
		while (h) { \
			type* p = h; \
			h = p->next; \
			type_free(p); \
			p = NULL; \
		} \
	} while (0)

#define scf_list_mov(dst, src, type, member) \
	do {\
		scf_list_t* l;\
		type*       t;\
		for (l = scf_list_head(src); l != scf_list_sentinel(src);) {\
			t  = scf_list_data(l, type, member);\
			l  = scf_list_next(l);\
			scf_list_del(&t->member);\
			scf_list_add_tail(dst, &t->member);\
		}\
	} while(0)

#endif
