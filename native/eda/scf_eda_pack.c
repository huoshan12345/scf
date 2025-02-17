#include "scf_eda_pack.h"

static int component_pins[SCF_EDA_Components_NB] =
{
	0, // None
	SCF_EDA_Battery_NB, // SCF_EDA_Battery

	2, // SCF_EDA_Resistor
	2, // SCF_EDA_Capacitor
	2, // SCF_EDA_Inductor

	SCF_EDA_Diode_NB,
	SCF_EDA_NPN_NB,
	SCF_EDA_PNP_NB,

	SCF_EDA_NAND_NB,
	SCF_EDA_NOR_NB,
	SCF_EDA_NOT_NB,
};

static int __diode_path_off(ScfEpin* p0, ScfEpin* p1)
{
	if (SCF_EDA_Diode_NEG == p0->id)
		return 1;
	return 0;
}

static int __npn_path_off(ScfEpin* p0, ScfEpin* p1)
{
	if (SCF_EDA_NPN_E == p0->id)
		return 1;

	if (!p1 || SCF_EDA_NPN_E == p1->id)
		return 0;
	return 1;
}

static int __npn_shared(ScfEpin* p)
{
	if (SCF_EDA_NPN_E == p->id)
		return 1;
	return 0;
}

static int __pnp_path_off(ScfEpin* p0, ScfEpin* p1)
{
	if (SCF_EDA_PNP_E == p0->id)
		return 0;
	return 1;
}

static int __pnp_shared(ScfEpin* p)
{
	if (SCF_EDA_PNP_E == p->id)
		return 1;
	return 0;
}

static int __nand_path_off(ScfEpin* p0, ScfEpin* p1)
{
	if (SCF_EDA_NAND_NEG == p0->id)
		return 1;

	if (SCF_EDA_NAND_POS == p0->id) {
		if (p1 && (SCF_EDA_NAND_IN0 == p1->id || SCF_EDA_NAND_IN1 == p1->id))
			return 1;

	} else if (p1 && SCF_EDA_NAND_NEG != p1->id)
		return 1;
	return 0;
}

static int __nand_shared(ScfEpin* p)
{
	if (SCF_EDA_NAND_NEG == p->id || SCF_EDA_NAND_POS == p->id)
		return 1;
	return 0;
}

static int __nor_path_off(ScfEpin* p0, ScfEpin* p1)
{
	if (SCF_EDA_NOR_NEG == p0->id)
		return 1;

	if (SCF_EDA_NOR_POS == p0->id) {
		if (p1 && (SCF_EDA_NOR_IN0 == p1->id || SCF_EDA_NOR_IN1 == p1->id))
			return 1;

	} else if (p1 && SCF_EDA_NOR_NEG != p1->id)
		return 1;
	return 0;
}

static int __nor_shared(ScfEpin* p)
{
	if (SCF_EDA_NOR_NEG == p->id || SCF_EDA_NOR_POS == p->id)
		return 1;
	return 0;
}

static int __not_path_off(ScfEpin* p0, ScfEpin* p1)
{
	if (SCF_EDA_NOT_NEG == p0->id)
		return 1;

	if (SCF_EDA_NOT_POS == p0->id) {
		if (p1 && SCF_EDA_NOT_IN == p1->id)
			return 1;

	} else if (p1 && SCF_EDA_NOT_NEG != p1->id)
		return 1;
	return 0;
}

static int __not_shared(ScfEpin* p)
{
	if (SCF_EDA_NOT_NEG == p->id || SCF_EDA_NOT_POS == p->id)
		return 1;
	return 0;
}

static ScfEops __diode_ops =
{
	__diode_path_off,
	NULL,
};

static ScfEops __npn_ops =
{
	__npn_path_off,
	__npn_shared,
};

static ScfEops __pnp_ops =
{
	__pnp_path_off,
	__pnp_shared,
};

static ScfEops __nand_ops =
{
	__nand_path_off,
	__nand_shared,
};

static ScfEops __nor_ops =
{
	__nor_path_off,
	__nor_shared,
};

static ScfEops __not_ops =
{
	__not_path_off,
	__not_shared,
};

static ScfEdata  component_datas[] =
{
	{SCF_EDA_None,       0,                   0, 0, 0,    0,   0,   0, 0, NULL, NULL, NULL},
	{SCF_EDA_Battery,    0, SCF_EDA_Battery_POS, 0, 0, 1e-9, 1e9,   0, 0, NULL, NULL, NULL},

	{SCF_EDA_Resistor,   0,                   0, 0, 0,  1e4,   0,   0, 0, NULL, NULL, NULL},
	{SCF_EDA_Capacitor,  0,                   0, 0, 0,   10, 0.1,   0, 0, NULL, NULL, NULL},
	{SCF_EDA_Inductor,   0,                   0, 0, 0,   10,   0, 1e3, 0, NULL, NULL, NULL},

	{SCF_EDA_Diode,      0,                   0, 0, 0,    0,   0,   0, 0, &__diode_ops, NULL, NULL},
	{SCF_EDA_NPN,        0,                   0, 0, 0,    0,   0,   0, 0, &__npn_ops,   NULL, "./cpk/9013.txt"},
	{SCF_EDA_PNP,        0,                   0, 0, 0,    0,   0,   0, 0, &__pnp_ops,   NULL, NULL},

	{SCF_EDA_NAND,       0,                   0, 0, 0,    0,   0,   0, 0, &__nand_ops,  "./cpk/nand.cpk", NULL},
	{SCF_EDA_NOR,        0,                   0, 0, 0,    0,   0,   0, 0, &__nor_ops,   "./cpk/nor.cpk", NULL},
	{SCF_EDA_NOT,        0,                   0, 0, 0,    0,   0,   0, 0, &__not_ops,   "./cpk/not.cpk", NULL},
};

static ScfEdata  pin_datas[] =
{
	{SCF_EDA_None,       0,                   0, 0, 0,    0,   0,   0,   0, NULL, NULL, NULL},

	{SCF_EDA_Diode,      0,   SCF_EDA_Diode_NEG, 0, 0,  750,   0,   0,   0, NULL, NULL, NULL},

	{SCF_EDA_NPN,        0,       SCF_EDA_NPN_B, 0, 0,  750,   0,   0,   0, NULL, NULL, NULL},
	{SCF_EDA_NPN,        0,       SCF_EDA_NPN_C, 0, 0,    3,   0,   0, 250, NULL, NULL, NULL},

	{SCF_EDA_PNP,        0,       SCF_EDA_PNP_B, 0, 0,  750,   0,   0,   0, NULL, NULL, NULL},
	{SCF_EDA_PNP,        0,       SCF_EDA_PNP_C, 0, 0,    3,   0,   0, 250, NULL, NULL, NULL},
};

static ScfEdata* _pin_find_data(const uint64_t type, const uint64_t model, const uint64_t pid)
{
	ScfEdata* ed;

	int i;
	for (i = 0; i < sizeof(pin_datas) / sizeof(pin_datas[0]); i++) {
		ed =              &pin_datas[i];

		if (ed->type == type && ed->model == model && ed->pid == pid)
			return ed;
	}

	return NULL;
}

ScfEdata* scf_ecomponent__find_data(const uint64_t type, const uint64_t model)
{
	ScfEdata* ed;

	int i;
	for (i = 0; i < sizeof(component_datas) / sizeof(component_datas[0]); i++) {
		ed =              &component_datas[i];

		if (ed->type == type && ed->model == model)
			return ed;
	}

	return NULL;
}

ScfEconn* scf_econn__alloc()
{
	ScfEconn* ec = calloc(1, sizeof(ScfEconn));

	return ec;
}

int scf_econn__add_cid(ScfEconn* ec, uint64_t cid)
{
	if (!ec)
		return -EINVAL;

	for (size_t i = 0; i < ec->n_cids; i++) {

		if (ec->cids[i] == cid)
			return 0;
	}

	uint64_t* p = realloc(ec->cids, sizeof(uint64_t) * (ec->n_cids + 1));
	if (!p)
		return -ENOMEM;

	ec->cids = p;
	ec->cids[ec->n_cids++] = cid;
	return 0;
}

int scf_econn__del_cid(ScfEconn* ec, uint64_t cid)
{
	if (!ec)
		return -EINVAL;

	uint64_t* p;
	size_t    i;
	size_t    j;

	for (i = 0; i < ec->n_cids; i++) {

		if (ec->cids[i] == cid) {

			for (j = i + 1;  j  < ec->n_cids; j++)
				ec->cids[j - 1] = ec->cids[j];

			ec->n_cids--;

			p = realloc(ec->cids, sizeof(uint64_t) * ec->n_cids);
			if (p)
				ec->cids = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEline* scf_eline__alloc()
{
	ScfEline* l = calloc(1, sizeof(ScfEline));

	return l;
}

int scf_eline__add_line(ScfEline* el, ScfLine*  l)
{
	if (!el || !l)
		return -EINVAL;

	for (size_t i = 0; i < el->n_lines; i++) {

		if (el->lines[i] == l)
			return 0;

		if (el->lines[i]->x0 == l->x0
		 && el->lines[i]->y0 == l->y0
		 && el->lines[i]->x1 == l->x1
		 && el->lines[i]->y1 == l->y1)
			return 0;
	}

	void* p = realloc(el->lines, sizeof(ScfLine*) * (el->n_lines + 1));
	if (!p)
		return -ENOMEM;

	el->lines = p;
	el->lines[el->n_lines++] = l;
	return 0;
}

int scf_eline__del_line(ScfEline* el, ScfLine*  l)
{
	if (!el || !l)
		return -EINVAL;

	void*   p;
	size_t  i;
	size_t  j;

	for (i = 0; i < el->n_lines; i++) {

		if (el->lines[i]->x0 == l->x0
		 && el->lines[i]->y0 == l->y0
		 && el->lines[i]->x1 == l->x1
		 && el->lines[i]->y1 == l->y1) {

			for (j = i + 1;  j  < el->n_lines; j++)
				el->lines[j - 1] = el->lines[j];

			el->n_lines--;

			p = realloc(el->lines, sizeof(ScfLine*) * el->n_lines);
			if (p)
				el->lines = p;
			return 0;
		}
	}

	return -EINVAL;
}

int scf_eline__add_pin(ScfEline* el, uint64_t  cid, uint64_t pid)
{
	if (!el)
		return -EINVAL;

	for (size_t i = 0; i + 1 < el->n_pins; i += 2) {

		if (el->pins[i] == cid && el->pins[i + 1] == pid)
			return 0;
	}

	uint64_t* p = realloc(el->pins, sizeof(uint64_t) * (el->n_pins + 2));
	if (!p)
		return -ENOMEM;

	el->pins = p;
	el->pins[el->n_pins++] = cid;
	el->pins[el->n_pins++] = pid;
	return 0;
}

int scf_eline__del_pin(ScfEline* el, uint64_t  cid, uint64_t pid)
{
	if (!el)
		return -EINVAL;

	uint64_t* p;
	size_t    i;
	size_t    j;

	for (i = 0; i + 1 < el->n_pins; i += 2) {

		if (el->pins[i] == cid && el->pins[i + 1] == pid) {

			for (j = i + 2;  j  < el->n_pins; j++)
				el->pins[j - 2] = el->pins[j];

			el->n_pins -= 2;

			p = realloc(el->pins, sizeof(uint64_t) * el->n_pins);
			if (p)
				el->pins = p;
			return 0;
		}
	}

	return -EINVAL;
}

int scf_eline__add_conn(ScfEline* el, ScfEconn* ec)
{
	if (!el || !ec)
		return -EINVAL;

	for (size_t i = 0; i < el->n_conns; i++) {

		if (el->conns[i] == ec)
			return 0;
	}

	void* p = realloc(el->conns, sizeof(ScfEconn*) * (el->n_conns + 1));
	if (!p)
		return -ENOMEM;

	el->conns = p;
	el->conns[el->n_conns++] = ec;
	return 0;
}

int scf_eline__del_conn(ScfEline* el, ScfEconn* ec)
{
	if (!el || !ec)
		return -EINVAL;

	void*   p;
	size_t  i;
	size_t  j;

	for (i = 0; i < el->n_conns; i++) {

		if (el->conns[i] == ec) {

			for (j = i + 1;  j  < el->n_conns; j++)
				el->conns[j - 1] = el->conns[j];

			el->n_conns--;

			p = realloc(el->conns, sizeof(ScfEconn*) * el->n_conns);
			if (p)
				el->conns = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEpin* scf_epin__alloc()
{
	ScfEpin* pin = calloc(1, sizeof(ScfEpin));

	return pin;
}

int scf_epin__add_component(ScfEpin* pin, uint64_t cid, uint64_t pid)
{
	if (!pin)
		return -EINVAL;

	for (size_t i = 0; i + 1 < pin->n_tos; i += 2) {

		if (pin->tos[i] == cid && pin->tos[i + 1] == pid)
			return 0;
	}

	uint64_t* p = realloc(pin->tos, sizeof(uint64_t) * (pin->n_tos + 2));
	if (!p)
		return -ENOMEM;

	pin->tos = p;
	pin->tos[pin->n_tos++] = cid;
	pin->tos[pin->n_tos++] = pid;
	return 0;
}

int scf_epin__del_component(ScfEpin* pin, uint64_t cid, uint64_t pid)
{
	if (!pin)
		return -EINVAL;

	uint64_t* p;
	size_t    i;
	size_t    j;

	for (i = 0; i + 1 < pin->n_tos; i += 2) {

		if (pin->tos[i] == cid && pin->tos[i + 1] == pid) {

			for (j = i + 2;  j  < pin->n_tos; j++)
				pin->tos[j - 2] = pin->tos[j];

			pin->n_tos -= 2;

			p = realloc(pin->tos, sizeof(uint64_t) * pin->n_tos);
			if (p)
				pin->tos = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEcomponent* scf_ecomponent__alloc(uint64_t type)
{
	ScfEcomponent* c;
	ScfEdata*      ed;

	if (type >= SCF_EDA_Components_NB)
		return NULL;

	c = calloc(1, sizeof(ScfEcomponent));
	if (!c)
		return NULL;

	c->type = type;

	ed = scf_ecomponent__find_data(c->type, c->model);
	if (ed) {
		c->v   = ed->v;
		c->a   = ed->a;
		c->r   = ed->r;
		c->uf  = ed->uf;
		c->uh  = ed->uh;
		c->ops = ed->ops;
	}

	int i;
	for (i = 0; i < component_pins[type]; i++) {

		ScfEpin* pin = scf_epin__alloc();
		if (!pin) {
			ScfEcomponent_free(c);
			return NULL;
		}

		pin->id     = i;
		pin->ic_lid = -1;

		scf_logd("pin %p, id: %ld, ic_lid: %ld\n", pin, pin->id, pin->ic_lid);

		if (scf_ecomponent__add_pin(c, pin) < 0) {
			ScfEcomponent_free(c);
			ScfEpin_free(pin);
			return NULL;
		}

		ed = _pin_find_data(c->type, c->model, pin->id);
		if (ed) {
			pin->v   = ed->v;
			pin->a   = ed->a;
			pin->r   = ed->r;
			pin->uf  = ed->uf;
			pin->uh  = ed->uh;
			pin->hfe = ed->hfe;
		}
	}

	return c;
}

int scf_ecomponent__add_curve(ScfEcomponent* c, ScfEcurve* curve)
{
	if (!c || !curve)
		return -EINVAL;

	void* p = realloc(c->curves, sizeof(ScfEcurve*) * (c->n_curves + 1));
	if (!p)
		return -ENOMEM;

	c->curves = p;
	c->curves[c->n_curves++] = curve;
	return 0;
}

int scf_ecomponent__add_pin(ScfEcomponent* c, ScfEpin* pin)
{
	if (!c || !pin)
		return -EINVAL;

	void* p = realloc(c->pins, sizeof(ScfEpin*) * (c->n_pins + 1));
	if (!p)
		return -ENOMEM;

	c->pins = p;
	c->pins[c->n_pins++] = pin;
	return 0;
}

int scf_ecomponent__del_pin(ScfEcomponent* c, ScfEpin* pin)
{
	if (!c)
		return -EINVAL;

	size_t   i;
	size_t   j;
	void*    p;

	for (i = 0; i < c->n_pins; i++) {

		if (c->pins[i] == pin) {

			for (j = i + 1; j  < c->n_pins; j++)
				c->pins[j - 1] = c->pins[j];

			c->n_pins--;

			p = realloc(c->pins, sizeof(ScfEpin*) * c->n_pins);
			if (p)
				c->pins = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEfunction*  scf_efunction__alloc(const char* name)
{
	ScfEfunction* f = calloc(1, sizeof(ScfEfunction));
	if (!f)
		return NULL;

	f->n_name = strlen(name) + 1;

	f->name = strdup(name);
	if (!f->name) {
		free(f);
		return NULL;
	}

	return f;
}

int scf_efunction__add_component(ScfEfunction* f, ScfEcomponent* c)
{
	if (!f || !c)
		return -EINVAL;

	void* p = realloc(f->components, sizeof(ScfEcomponent*) * (f->n_components + 1));
	if (!p)
		return -ENOMEM;

	f->components = p;
	f->components[f->n_components++] = c;
	return 0;
}

int scf_efunction__del_component(ScfEfunction* f, ScfEcomponent* c)
{
	if (!f || !c)
		return -EINVAL;

	size_t   i;
	size_t   j;
	void*    p;

	for (i = 0; i < f->n_components; i++) {

		if (f->components[i] == c) {

			for (j = i + 1;  j < f->n_components; j++)
				f->components[j - 1] = f->components[j];

			f->n_components--;

			p = realloc(f->components, sizeof(ScfEcomponent*) * f->n_components);
			if (p)
				f->components = p;
			return 0;
		}
	}

	return -EINVAL;
}

int scf_efunction__add_eline(ScfEfunction* f, ScfEline* el)
{
	if (!f || !el)
		return -EINVAL;

	void* p = realloc(f->elines, sizeof(ScfEline*) * (f->n_elines + 1));
	if (!p)
		return -ENOMEM;

	f->elines = p;
	f->elines[f->n_elines++] = el;
	return 0;
}

int scf_efunction__del_eline(ScfEfunction* f, ScfEline* el)
{
	if (!f || !el)
		return -EINVAL;

	size_t   i;
	size_t   j;
	void*    p;

	for (i = 0; i < f->n_elines; i++) {

		if (f->elines[i] == el) {

			for (j = i + 1;  j < f->n_elines; j++)
				f->elines[j - 1] = f->elines[j];

			f->n_elines--;

			p = realloc(f->elines, sizeof(ScfEline*) * f->n_elines);
			if (p)
				f->elines = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEboard* scf_eboard__alloc()
{
	ScfEboard* b = calloc(1, sizeof(ScfEboard));

	return b;
}

int scf_eboard__add_function(ScfEboard* b, ScfEfunction* f)
{
	if (!b || !f)
		return -EINVAL;

	void* p = realloc(b->functions, sizeof(ScfEfunction*) * (b->n_functions + 1));
	if (!p)
		return -ENOMEM;

	b->functions = p;
	b->functions[b->n_functions++] = f;
	return 0;
}

int scf_eboard__del_function(ScfEboard* b, ScfEfunction* f)
{
	if (!b)
		return -EINVAL;

	size_t   i;
	size_t   j;
	void*    p;

	for (i = 0; i < b->n_functions; i++) {

		if (b->functions[i] == f) {

			for (j = i + 1;  j < b->n_functions; j++)
				b->functions[j - 1] = b->functions[j];

			b->n_functions--;

			p = realloc(b->functions, sizeof(ScfEfunction*) * b->n_functions);
			if (p)
				b->functions = p;
			return 0;
		}
	}

	return -EINVAL;
}

static int epin_cmp(const void* v0, const void* v1)
{
	const uint64_t* t0 = v0;
	const uint64_t* t1 = v1;

	if (t0[0] < t1[0])
		return -1;

	if (t0[0] > t1[0])
		return 1;

	if (t0[1] < t1[1])
		return -1;

	if (t0[1] > t1[1])
		return 1;
	return 0;
}

int scf_pins_same_line(ScfEfunction* f)
{
	ScfEcomponent* c;
	ScfEline*      el;
	ScfEline*      el2;
	ScfEpin*       p;
	ScfEpin*       p2;

	long i;
	long j;
	long k;
	long m;
	long n;

	for (i = 0; i < f->n_components; i++) {
		c  =        f->components[i];

		for (j = 0; j < c->n_pins; j++) {
			p  =        c->pins[j];

			qsort(p->tos, p->n_tos / 2, sizeof(uint64_t) * 2, epin_cmp);

			for (k = 0; k < f->n_elines; k++) {
				el        = f->elines[k];

				for (m = 0; m + 1 < el->n_pins; m += 2) {

					if (el->pins[m] == p->cid && el->pins[m + 1] == p->id)
						goto next;
				}

				m = 0;
				n = 0;
				while (m + 1 < el->n_pins && n + 1 < p->n_tos) {

					if (el->pins[m] < p->tos[n])
						m += 2;
					else if (el->pins[m] > p->tos[n])
						n += 2;

					else if (el->pins[m + 1] < p->tos[n + 1])
						m += 2;
					else if (el->pins[m + 1] > p->tos[n + 1])
						n += 2;

					else {
						if (scf_eline__add_pin(el, p->cid, p->id) < 0)
							return -ENOMEM;

						p ->lid    = el->id;
						p ->c_lid  = el->id;
						el->flags |= p->flags;
						goto next;
					}
				}
			}

			el = scf_eline__alloc();
			if (!el)
				return -ENOMEM;
			el->id = f->n_elines;

			if (scf_efunction__add_eline(f, el) < 0) {
				ScfEline_free(el);
				return -ENOMEM;
			}

			if (scf_eline__add_pin(el, p->cid, p->id) < 0)
				return -ENOMEM;

			p ->lid    = el->id;
			p ->c_lid  = el->id;
			el->flags |= p->flags;
next:
			for (n = 0; n + 1 < p->n_tos; n += 2) {

				p2 = f->components[p->tos[n]]->pins[p->tos[n + 1]];

				if (p2->cid >  p->cid
				|| (p2->cid == p->cid && p2->id > p->id))
					break;

				el2 = f->elines[p2->lid];

				if (el2 == el)
					continue;

				if (el2->id < el->id)
					SCF_XCHG(el2, el);

				for (m = 0; m + 1 < el2->n_pins; m += 2) {
					p2 = f->components[el2->pins[m]]->pins[el2->pins[m + 1]];

					if (scf_eline__add_pin(el, p2->cid, p2->id) < 0)
						return -ENOMEM;

					p2->lid    = el->id;
					p2->c_lid  = el->id;
					el->flags |= p2->flags;
				}

				qsort(el->pins, el->n_pins / 2, sizeof(uint64_t) * 2, epin_cmp);

				el2->n_pins = 0;
			}
			p = NULL;
		}
	}

	for (i = 0; i < f->n_elines; ) {
		el        = f->elines[i];

		if (0 == el->n_pins) {
			scf_efunction__del_eline(f, el);
			ScfEline_free(el);
			continue;
		}

		el->c_pins = el->n_pins;
		i++;
	}

	for (i = 0; i < f->n_elines; ) {
		el        = f->elines[i];
		el->id    = i;

		int64_t eid = -1;

		for (j = 0; j + 1 < el->n_pins; j += 2) {

			c  = f->components[el->pins[j]];
			p  = c->pins      [el->pins[j + 1]];

			p->lid   = i;
			p->c_lid = i;

			if (p->ic_lid < 0)
				continue;

			if (eid < 0)
				eid = p->ic_lid;

			else if (eid != p->ic_lid) {
				scf_loge("IC pin number set error, prev: %ld, current: %ld\n", eid, p->ic_lid);
				return -EINVAL;
			}

			scf_logd("pin j: %ld, c%ldp%ld\n", j, el->pins[j], el->pins[j + 1]);
		}

		if (eid >= f->n_elines) {
			scf_loge("IC pin number set error, max: %ld, current: %ld\n", f->n_elines - 1, eid);
			return -EINVAL;
		}

		scf_logd("i: %ld, eid: %ld\n", i, eid);

		if (eid >= 0 && eid != i) {
			el->id = eid;

			for (j = 0; j + 1 < el->n_pins; j += 2) {

				c  = f->components[el->pins[j]];
				p  = c->pins      [el->pins[j + 1]];

				p->lid   = eid;
				p->c_lid = eid;
			}

			SCF_XCHG(f->elines[eid], f->elines[i]);
			continue;
		}

		i++;
	}

	return 0;
}
