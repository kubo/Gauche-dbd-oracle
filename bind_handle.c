#include "dbd_oracle.h"


/*
 * BIND_STRING
 */

typedef struct {
    sb4 size;
    char buf[1];
} lvc_string_t;

static void str_init(bind_handle_t *hndl, u_int size)
{
    lvc_string_t *lvc;
    sb4 value_sz = sizeof(sb4) + size;

    hndl->value_sz = value_sz;
    hndl->valuep = malloc(value_sz);
    hndl->ind = -1;
    lvc = hndl->valuep;
    lvc->size = 0;
}

static void str_clear(bind_handle_t *hndl)
{
    free(hndl->valuep);
}

static void str_set(bind_handle_t *hndl, ScmObj val)
{
    if (SCM_NULLP(val)) {
        hndl->ind = -1;
    } else if (SCM_STRINGP(val)) {
        unsigned int length;
        const char *str = Scm_GetStringContent(SCM_STRING(val), NULL, &length, NULL);
	lvc_string_t *lvc = hndl->valuep;
	sb4 max_size = hndl->value_sz - sizeof(sb4);

	if (length > max_size) {
	  Scm_Error("too large string to bind: %d for %d", length, max_size);
	}
        hndl->ind = 0;
	lvc->size = length;
	memcpy(lvc->buf, str, length);
    } else {
        Scm_Error("neither string nor null");
    }
}

static ScmObj str_get(bind_handle_t *hndl)
{
    if (hndl->ind) {
        return SCM_NIL;
    } else {
	lvc_string_t *lvc = hndl->valuep;
	return Scm_MakeString(lvc->buf, lvc->size, -1, SCM_STRING_COPYING);
    }
}

/*
 * BIND_INTEGER
 */

static void int_init(bind_handle_t *hndl, u_int size)
{
    hndl->value_sz = sizeof(long);
    hndl->valuep = &hndl->value.l;
    hndl->ind = -1;
}

static void int_clear(bind_handle_t *hndl)
{
    /* do noting */
}

static void int_set(bind_handle_t *hndl, ScmObj val)
{
    if (SCM_NULLP(val)) {
        hndl->ind = -1;
    } else if (SCM_INTEGERP(val)) {
        hndl->value.l = Scm_GetInteger(val);
        hndl->ind = 0;
    } else {
        Scm_Error("neither integer nor null");
    }
}

static ScmObj int_get(bind_handle_t *hndl)
{
    if (hndl->ind) {
        return SCM_NIL;
    } else {
        return Scm_MakeInteger(hndl->value.l);
    }
}


/*
 * BIND_FLONUM
 */

static void flt_init(bind_handle_t *hndl, u_int size)
{
    hndl->value_sz = sizeof(double);
    hndl->valuep = &hndl->value.d;
    hndl->ind = -1;
}

static void flt_clear(bind_handle_t *hndl)
{
    /* do noting */
}

static void flt_set(bind_handle_t *hndl, ScmObj val)
{
    if (SCM_NULLP(val)) {
        hndl->ind = -1;
    } else if (SCM_REALP(val)) {
        hndl->value.d = Scm_GetDouble(val);
        hndl->ind = 0;
    } else {
        Scm_Error("neither real nor null");
    }
}

static ScmObj flt_get(bind_handle_t *hndl)
{
    if (hndl->ind) {
        return SCM_NIL;
    } else {
        return Scm_MakeFlonum(hndl->value.d);
    }
}


/*
 * Common part
 */

static const struct {
    enum dbd_oracle_bind_type type;
    bind_handle_vptr_t vptr;
} bind_handle_vptr_map[] = {
    {BIND_STRING,  {SQLT_LVC, str_init, str_clear, str_set, str_get}},
    {BIND_INTEGER, {SQLT_INT, int_init, int_clear, int_set, int_get}},
    {BIND_REAL,    {SQLT_FLT, flt_init, flt_clear, flt_set, flt_get}},
};

#define NUM_BIND_HANDLE_VPTR_MAP (sizeof(bind_handle_vptr_map)/sizeof(bind_handle_vptr_map[0]))

void bind_handle_init(bind_handle_t *hndl, int type, u_int size)
{
    int idx;
    const bind_handle_vptr_t *vptr;

    for (idx = 0; idx < NUM_BIND_HANDLE_VPTR_MAP; idx++) {
        if (type == bind_handle_vptr_map[idx].type) {
	    break;
	}
    }
    if (idx == NUM_BIND_HANDLE_VPTR_MAP) {
        Scm_Error("unknown data type: %d", type);
    }
    if (hndl->vptr != NULL) {
        hndl->vptr->clear(hndl);
	hndl->vptr = NULL;
    }
    vptr = &bind_handle_vptr_map[idx].vptr;
    vptr->init(hndl, size);
    hndl->vptr = vptr;
}
