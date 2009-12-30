/*
 * dbd_oracle.c
 */

#include <stdlib.h>
#include "dbd_oracle.h"
#include <gauche/class.h>

#define ERROR(state, err) Scm_Cons(SCM_MAKE_INT(state), SCM_OBJ(err))
#define ALLOC_ERROR(state) Scm_Cons(SCM_MAKE_INT(state), SCM_OBJ(Scm_make_oracle_env()))
#define SUCCESS(obj) Scm_Cons(SCM_MAKE_INT(OCI_SUCCESS), SCM_OBJ(obj))

struct Scm_OCIError {
    SCM_HEADER;
    ub4 type;  /* OCI_HTYPE_ERROR or OCI_HTYPE_ENV */
    void *errhp;
};

struct Scm_OCISvcCtx {
    SCM_HEADER;
    OCISvcCtx *svchp;
};

struct Scm_OCIStmt {
    SCM_HEADER;
    OCIStmt *stmtp;
    ub4 bind_count;
    ub4 column_count;
    bind_handle_t *bind_handles;
    bind_handle_t *column_handles;
};

struct Scm_OCIParamMetadata {
    SCM_HEADER;
    ScmObj name;
    ub2 data_type;
    ub2 data_size;
    sb2 precision;
    sb1 scale;
};

static OCIEnv *envhp;
static Scm_OCIError *Scm_make_oracle_env(void);
static bind_handle_t *get_bind_handle(Scm_OCIStmt *stmt, u_int pos);
static bind_handle_t *get_column_handle(Scm_OCIStmt *stmt, u_int pos);
static ScmObj get_ub2_attr(Scm_OCIError *err, void *hndl, ub4 hndl_type, ub4 attr_type);
static ScmObj get_ub4_attr(Scm_OCIError *err, void *hndl, ub4 hndl_type, ub4 attr_type);

SCM_DEFINE_BUILTIN_CLASS_SIMPLE(Scm_OCIErrorClass, NULL);
SCM_DEFINE_BUILTIN_CLASS_SIMPLE(Scm_OCISvcCtxClass, NULL);
SCM_DEFINE_BUILTIN_CLASS_SIMPLE(Scm_OCIStmtClass, NULL);
SCM_DEFINE_BUILTIN_CLASS_SIMPLE(Scm_OCIParamMetadataClass, NULL);

static void error_finalize(ScmObj obj, void *data)
{
    Scm_OCIError *err = (Scm_OCIError *)obj;

    if (err->errhp != NULL) {
        OCIHandleFree(err->errhp, OCI_HTYPE_ERROR);
        err->errhp = NULL;
    }
}

static void svcctx_finalize(ScmObj obj, void *data)
{
    Scm_OCISvcCtx *svc = (Scm_OCISvcCtx *)obj;

    if (svc->svchp != NULL) {
        OCIHandleFree(svc->svchp, OCI_HTYPE_SVCCTX);
        svc->svchp = NULL;
    }
}

static void stmt_finalize(ScmObj obj, void *data)
{
    Scm_OCIStmt *stmt = (Scm_OCIStmt *)obj;

    if (stmt->bind_handles != NULL) {
        sb4 idx;

        for (idx = 0; idx < stmt->bind_count; idx++) {
            bind_handle_t *hndl = &stmt->bind_handles[idx];
            if (hndl->vptr != NULL) {
                hndl->vptr->clear(hndl);
            }
        }
        free(stmt->bind_handles);
    }
    if (stmt->column_handles != NULL) {
        sb4 idx;

        for (idx = 0; idx < stmt->column_count; idx++) {
            bind_handle_t *hndl = &stmt->column_handles[idx];
            if (hndl->vptr != NULL) {
                hndl->vptr->clear(hndl);
            }
        }
        free(stmt->column_handles);
    }
    if (stmt->stmtp != NULL) {
        OCIHandleFree(stmt->stmtp, OCI_HTYPE_STMT);
        stmt->stmtp = NULL;
    }
}

static Scm_OCIError *Scm_make_oracle_env(void)
{
    Scm_OCIError *err = SCM_NEW(Scm_OCIError);
    sword rv;
    err->type = OCI_HTYPE_ENV;
    err->errhp = envhp;

    SCM_SET_CLASS(err, SCM_CLASS_OCIERROR);
    return err;
}

static bind_handle_t *get_bind_handle(Scm_OCIStmt *stmt, u_int pos)
{
    if (stmt->bind_count <= pos) {
        Scm_Error("invalid bind position %d for 0 - %d", pos, stmt->bind_count);
    }
    return &stmt->bind_handles[pos];
}

static bind_handle_t *get_column_handle(Scm_OCIStmt *stmt, u_int pos)
{
    if (stmt->column_count <= pos) {
        Scm_Error("invalid column position %d for 0 - %d", pos, stmt->column_count);
    }
    return &stmt->column_handles[pos];
}

static ScmObj get_ub2_attr(Scm_OCIError *err, void *hndl, ub4 hndl_type, ub4 attr_type)
{
    ub2 val = 0;
    sword rv;

    rv = OCIAttrGet(hndl, hndl_type, &val, NULL, attr_type, err->errhp);
    if (rv != OCI_SUCCESS) {
        return ERROR(rv, err);
    }
    return SUCCESS(SCM_MAKE_INT(val));
}

static ScmObj get_ub4_attr(Scm_OCIError *err, void *hndl, ub4 hndl_type, ub4 attr_type)
{
    ub4 val = 0;
    sword rv;

    rv = OCIAttrGet(hndl, hndl_type, &val, NULL, attr_type, err->errhp);
    if (rv != OCI_SUCCESS) {
        return ERROR(rv, err);
    }
    return SUCCESS(Scm_MakeIntegerU(val));
}


ScmObj Scm_make_oracle_error(void)
{
    Scm_OCIError *err = SCM_NEW(Scm_OCIError);
    sword rv;

    SCM_SET_CLASS(err, SCM_CLASS_OCIERROR);
    Scm_RegisterFinalizer(SCM_OBJ(err), error_finalize, NULL);

    err->type = OCI_HTYPE_ERROR;
    rv = OCIHandleAlloc(envhp, (dvoid**)&err->errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS) {
        return ALLOC_ERROR(rv);
    }
    return SUCCESS(err);
}

void Scm_oracle_error_close(Scm_OCIError *err)
{
    error_finalize(SCM_OBJ(err), NULL);
}

ScmObj Scm_error_to_message(int status, Scm_OCIError *err)
{
    char buf[512];
    sb4 errcode = - status;
    ScmObj msg;

    switch (status) {
    case OCI_SUCCESS:
        msg = SCM_MAKE_STR("SUCCESS");
        break;
    case OCI_SUCCESS_WITH_INFO:
    case OCI_ERROR:
        OCIErrorGet(err->errhp, 1, NULL, &errcode, buf, sizeof(buf), err->type);
        msg = SCM_MAKE_STR_COPYING(buf);
        break;
    case OCI_NEED_DATA:
        msg = SCM_MAKE_STR("OCI_NEED_DATA");
        break;
    case OCI_NO_DATA:
        msg = SCM_MAKE_STR("OCI_NODATA");
        break;
    case OCI_INVALID_HANDLE:
        msg = SCM_MAKE_STR("OCI_INVALID_HANDLE");
        break;
    case OCI_STILL_EXECUTING:
        msg = SCM_MAKE_STR("OCI_STILL_EXECUTE");
        break;
    case OCI_CONTINUE:
        msg = SCM_MAKE_STR("OCI_CONTINUE");
        break;
    default:
        msg = Scm_Sprintf("Unknown error status: %d", status);
        break;
    }
    return Scm_Cons(SCM_MAKE_INT(errcode), msg);
}

ScmObj Scm_oracle_connect(Scm_OCIError *err, const char *user, const char *passwd, const char *dbname)
{
    Scm_OCISvcCtx *svc = SCM_NEW(Scm_OCISvcCtx);
    sword rv;

    svc->svchp = NULL;
    SCM_SET_CLASS(svc, SCM_CLASS_OCISVCCTX);
    Scm_RegisterFinalizer(SCM_OBJ(svc), svcctx_finalize, NULL);

    rv = OCILogon(envhp, err->errhp, &svc->svchp, user, strlen(user), passwd, strlen(passwd),
                  dbname, strlen(dbname));
    if (rv != OCI_SUCCESS) {
        return ERROR(rv, err);
    }
    return SUCCESS(svc);
}

ScmObj Scm_oracle_disconnect(Scm_OCIError *err, Scm_OCISvcCtx *svc)
{
    sword rv;

    rv = OCILogoff(svc->svchp, err->errhp);
    svcctx_finalize(SCM_OBJ(svc), NULL);
    if (rv != OCI_SUCCESS) {
        return ERROR(rv, err);
    }
    return SUCCESS(SCM_UNDEFINED);
}

ScmObj Scm_oracle_stmt_prepare(Scm_OCIError *err, const char *sql)
{
    Scm_OCIStmt *stmt = SCM_NEW(Scm_OCIStmt);
    sb4 found;
    OraText *bvnp[1];
    ub1 bvnl[1];
    OraText *invp[1];
    ub1 inpl[1];
    ub1 dupl[1];
    OCIBind *hndl[1];
    sword rv;

    stmt->stmtp = NULL;
    stmt->bind_count = 0;
    stmt->column_count = 0;
    stmt->bind_handles = NULL;
    stmt->column_handles = NULL;

    SCM_SET_CLASS(stmt, SCM_CLASS_OCISTMT);
    Scm_RegisterFinalizer(SCM_OBJ(stmt), stmt_finalize, NULL);

    rv = OCIHandleAlloc(envhp, (dvoid **)&stmt->stmtp, OCI_HTYPE_STMT, 0, NULL);
    if (rv != OCI_SUCCESS) {
        return ALLOC_ERROR(rv);
    }
    rv = OCIStmtPrepare(stmt->stmtp, err->errhp, sql, strlen(sql), OCI_NTV_SYNTAX, OCI_DEFAULT);
    if (rv != OCI_SUCCESS) {
        return ERROR(rv, err);
    }
    rv = OCIStmtGetBindInfo(stmt->stmtp, err->errhp, 0, 1, &found, bvnp, bvnl, invp, inpl, dupl, hndl);
    if (rv == OCI_NO_DATA) {
        stmt->bind_count = 0;
        stmt->bind_handles = NULL;
    } else if (rv == OCI_SUCCESS) {
        stmt->bind_count = abs(found);
        stmt->bind_handles = calloc(stmt->bind_count, sizeof(bind_handle_t));
    } else {
        return ERROR(rv, err);
    }
    return SUCCESS(stmt);
}

void Scm_oracle_stmt_close(Scm_OCIStmt *stmt)
{
    stmt_finalize(SCM_OBJ(stmt), NULL);
}

ScmObj Scm_oracle_stmt_bind_count(Scm_OCIError *err, Scm_OCIStmt *stmt)
{
    return SUCCESS(SCM_MAKE_INT(stmt->bind_count));
}

ScmObj Scm_oracle_stmt_bind_init(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos, int type, u_int size)
{
    bind_handle_t *hndl = get_bind_handle(stmt, pos);
    sword rv;

    bind_handle_init(hndl, type, size);
    rv = OCIBindByPos(stmt->stmtp, (dvoid*)&hndl->bindp, err->errhp,
                      pos + 1, hndl->valuep, hndl->value_sz, hndl->vptr->dty, &hndl->ind, NULL, NULL,
                      0, NULL, OCI_DEFAULT);
    if (rv != OCI_SUCCESS) {
        return ERROR(rv, err);
    }
    return SUCCESS(SCM_NIL);
}

ScmObj Scm_oracle_stmt_bind_set(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos, ScmObj val)
{
    bind_handle_t *hndl = get_bind_handle(stmt, pos);

    hndl->vptr->set(hndl, val);
    return SUCCESS(SCM_NIL);
}

ScmObj Scm_oracle_stmt_bind_ref(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos)
{
    bind_handle_t *hndl = get_bind_handle(stmt, pos);

    return SUCCESS(hndl->vptr->get(hndl));
}

ScmObj Scm_oracle_stmt_column_init(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos, int type, u_int size)
{
    bind_handle_t *hndl = get_column_handle(stmt, pos);
    sword rv;

    bind_handle_init(hndl, type, size);
    rv = OCIDefineByPos(stmt->stmtp, (dvoid*)&hndl->bindp, err->errhp,
                        pos + 1, hndl->valuep, hndl->value_sz, hndl->vptr->dty, &hndl->ind, NULL, NULL,
                        OCI_DEFAULT);
    if (rv != OCI_SUCCESS) {
        return ERROR(rv, err);
    }
    return SUCCESS(SCM_NIL);
}

ScmObj Scm_oracle_stmt_column_ref(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos)
{
    bind_handle_t *hndl = get_column_handle(stmt, pos);

    return SUCCESS(hndl->vptr->get(hndl));
}

ScmObj Scm_oracle_stmt_execute(Scm_OCIError *err, Scm_OCISvcCtx *svc, Scm_OCIStmt *stmt)
{
    sword rv;
    ub2 stmt_type;
    ub4 iters;
    ub4 mode;

    rv = OCIAttrGet(stmt->stmtp, OCI_HTYPE_STMT, &stmt_type, NULL, OCI_ATTR_STMT_TYPE, err->errhp);
    if (rv != OCI_SUCCESS) {
        return ERROR(rv, err);
    }
    if (stmt_type == OCI_STMT_SELECT) {
        iters = 0;
        mode = OCI_DEFAULT;
    } else {
        iters = 1;
        mode = OCI_COMMIT_ON_SUCCESS;
    }
    rv = OCIStmtExecute(svc->svchp, stmt->stmtp, err->errhp, iters, 0, NULL, NULL, mode);
    if (rv != OCI_SUCCESS) {
        return ERROR(rv, err);
    }
    if (stmt_type == OCI_STMT_SELECT) {
        ub4 param_count;

        rv = OCIAttrGet(stmt->stmtp, OCI_HTYPE_STMT, &param_count, NULL, OCI_ATTR_PARAM_COUNT, err->errhp);
        if (rv != OCI_SUCCESS) {
          return ERROR(rv, err);
        }
        stmt->column_count = param_count;
        stmt->column_handles = calloc(param_count, sizeof(bind_handle_t));
    }
    return SUCCESS(SCM_NIL);
}

ScmObj Scm_oracle_stmt_fetch(Scm_OCIError *err, Scm_OCIStmt *stmt)
{
    sword rv;

    rv = OCIStmtFetch(stmt->stmtp, err->errhp, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
    if (rv == OCI_SUCCESS) {
        return SUCCESS(SCM_TRUE);
    } else if (rv == OCI_NO_DATA) {
        return SUCCESS(SCM_FALSE);
    } else {
        return ERROR(rv, err);
    }
}

ScmObj Scm_oracle_stmt_type(Scm_OCIError *err, Scm_OCIStmt *stmt)
{
    return get_ub2_attr(err, stmt->stmtp, OCI_HTYPE_STMT, OCI_ATTR_STMT_TYPE);
}

ScmObj Scm_oracle_stmt_row_count(Scm_OCIError *err, Scm_OCIStmt *stmt)
{
    return get_ub4_attr(err, stmt->stmtp, OCI_HTYPE_STMT, OCI_ATTR_ROW_COUNT);
}

ScmObj Scm_oracle_stmt_params(Scm_OCIError *err, Scm_OCIStmt *stmt)
{
    ScmObj params = Scm_MakeVector(stmt->column_count, SCM_NIL);
    ub4 pos;
    sword rv;

    for (pos = 0; pos < stmt->column_count; pos++) {
        Scm_OCIParamMetadata *param = SCM_NEW(Scm_OCIParamMetadata);
        OCIParam *parmhp = NULL;
        union {
            char *_text;
            ub2 _ub2;
            sb2 _sb2;
            sb1 _sb1;
        } val;
        ub4 size;

        SCM_SET_CLASS(param, SCM_CLASS_OCIPARAMMETADATA);

        /* get parameter */
        rv = OCIParamGet(stmt->stmtp, OCI_HTYPE_STMT, err->errhp, (dvoid*)&parmhp, pos + 1);
        if (rv != OCI_SUCCESS) {
            return ERROR(rv, err);
        }

        /* get name */
        rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, &size, OCI_ATTR_NAME, err->errhp);
        if (rv != OCI_SUCCESS) {
            return ERROR(rv, err);
        }
        param->name = Scm_MakeString(val._text, size, -1, SCM_STRING_COPYING);

        /* get data_type */
        rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, NULL, OCI_ATTR_DATA_TYPE, err->errhp);
        if (rv != OCI_SUCCESS) {
            return ERROR(rv, err);
        }
        param->data_type = val._ub2;

        /* get data_size */
        rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, NULL, OCI_ATTR_DATA_SIZE, err->errhp);
        if (rv != OCI_SUCCESS) {
            return ERROR(rv, err);
        }
        param->data_size = val._ub2;

        /* get precision */
        rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, NULL, OCI_ATTR_PRECISION, err->errhp);
        if (rv != OCI_SUCCESS) {
            return ERROR(rv, err);
        }
        param->precision = val._sb2;

        /* get scale */
        rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, NULL, OCI_ATTR_SCALE, err->errhp);
        if (rv != OCI_SUCCESS) {
            return ERROR(rv, err);
        }
        param->scale = val._sb1;

        Scm_VectorSet(SCM_VECTOR(params), pos, SCM_OBJ(param));
    }
    return SUCCESS(params);
}

static ScmObj param_metadata_get_name(ScmObj obj)
{
    Scm_OCIParamMetadata *md = (Scm_OCIParamMetadata*)obj;
    return md->name;
}

static ScmObj param_metadata_get_data_type(ScmObj obj)
{
    Scm_OCIParamMetadata *md = (Scm_OCIParamMetadata*)obj;
    return SCM_MAKE_INT(md->data_type);
}

static ScmObj param_metadata_get_data_size(ScmObj obj)
{
    Scm_OCIParamMetadata *md = (Scm_OCIParamMetadata*)obj;
    return SCM_MAKE_INT(md->data_size);
}

static ScmObj param_metadata_get_precision(ScmObj obj)
{
    Scm_OCIParamMetadata *md = (Scm_OCIParamMetadata*)obj;
    return SCM_MAKE_INT(md->precision);
}

static ScmObj param_metadata_get_scale(ScmObj obj)
{
    Scm_OCIParamMetadata *md = (Scm_OCIParamMetadata*)obj;
    return SCM_MAKE_INT(md->scale);
}

static ScmClassStaticSlotSpec param_metadata_slots[] = {
    SCM_CLASS_SLOT_SPEC("name", param_metadata_get_name, NULL),
    SCM_CLASS_SLOT_SPEC("data-type", param_metadata_get_data_type, NULL),
    SCM_CLASS_SLOT_SPEC("data-size", param_metadata_get_data_size, NULL),
    SCM_CLASS_SLOT_SPEC("precision", param_metadata_get_precision, NULL),
    SCM_CLASS_SLOT_SPEC("scale", param_metadata_get_scale, NULL),
    SCM_CLASS_SLOT_SPEC_END()
};

void Scm_Init_dbd_oracle(void)
{
    ScmModule *mod;
    sword rv;

    rv = OCIEnvCreate(&envhp, OCI_THREADED|OCI_OBJECT, NULL, NULL, NULL, NULL, 0, NULL);
    if (rv != OCI_SUCCESS) {
      Scm_Error("OCI Initialization Error (code = %d)", rv);
    }

    /* Register this DSO to Gauche */
    SCM_INIT_EXTENSION(dbd_oracle);

    /* Create the module if it doesn't exist yet. */
    mod = SCM_MODULE(SCM_FIND_MODULE("dbd.oracle", TRUE));

    Scm_InitStaticClass(&Scm_OCIErrorClass, "<oracle-error>", mod, NULL, 0);
    Scm_InitStaticClass(&Scm_OCISvcCtxClass, "<oracle-svcctx>", mod, NULL, 0);
    Scm_InitStaticClass(&Scm_OCIStmtClass, "<oracle-stmt>", mod, NULL, 0);
    Scm_InitStaticClass(&Scm_OCIParamMetadataClass, "<oracle-param-metadata>", mod, param_metadata_slots, 0);

    /* Register stub-generated procedures */
    Scm_Init_dbd_oraclelib(mod);
}
