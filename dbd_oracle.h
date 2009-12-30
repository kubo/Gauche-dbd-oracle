/*
 * dbd_oracle.h
 */

/* Prologue */
#ifndef GAUCHE_DBD_ORACLE_H
#define GAUCHE_DBD_ORACLE_H

#include <gauche.h>
#include <gauche/extend.h>
#include <oci.h>

SCM_DECL_BEGIN

enum dbd_oracle_bind_type {
    BIND_STRING,
    BIND_INTEGER,
    BIND_REAL,
};

/* oracle-error */
SCM_CLASS_DECL(Scm_OCIErrorClass);
#define SCM_CLASS_OCIERROR   (&Scm_OCIErrorClass)
#define SCM_ORACLE_ERROR(obj)    ((Scm_OCIError*)obj)
#define SCM_ORACLE_ERROR_P(obj)   SCM_XTYPEP(obj, SCM_CLASS_OCIERROR)

typedef struct Scm_OCIError Scm_OCIError;

/* oracle-svcctx */
SCM_CLASS_DECL(Scm_OCISvcCtxClass);
#define SCM_CLASS_OCISVCCTX   (&Scm_OCISvcCtxClass)
#define SCM_ORACLE_SVCCTX(obj)    ((Scm_OCISvcCtx*)obj)
#define SCM_ORACLE_SVCCTX_P(obj)   SCM_XTYPEP(obj, SCM_CLASS_OCISVCCTX)

typedef struct Scm_OCISvcCtx Scm_OCISvcCtx;

/* oracle-stmt */
SCM_CLASS_DECL(Scm_OCIStmtClass);
#define SCM_CLASS_OCISTMT   (&Scm_OCIStmtClass)
#define SCM_ORACLE_STMT(obj)    ((Scm_OCIStmt*)obj)
#define SCM_ORACLE_STMT_P(obj)   SCM_XTYPEP(obj, SCM_CLASS_OCISTMT)

typedef struct Scm_OCIStmt Scm_OCIStmt;

/* oracle-param-metadata */
SCM_CLASS_DECL(Scm_OCIParamMetadataClass);
#define SCM_CLASS_OCIPARAMMETADATA   (&Scm_OCIParamMetadataClass)
#define SCM_ORACLE_PARAMMETADATA(obj)    ((Scm_OCIParamMetadata*)obj)
#define SCM_ORACLE_PARAMMETADATA_P(obj)   SCM_XTYPEP(obj, SCM_CLASS_OCIPARAMMETADATA)

typedef struct Scm_OCIParamMetadata Scm_OCIParamMetadata;

/* Module initialization function. */
extern void Scm_Init_dbd_oraclelib(ScmModule*);


extern ScmObj Scm_make_oracle_error(void);
extern void Scm_oracle_error_close(Scm_OCIError *err);
extern ScmObj Scm_error_to_message(int status, Scm_OCIError *err);

extern ScmObj Scm_oracle_connect(Scm_OCIError *err, const char *user, const char *passwd, const char *dbname);
extern ScmObj Scm_oracle_disconnect(Scm_OCIError *err, Scm_OCISvcCtx *svcctx);
extern ScmObj Scm_oracle_stmt_prepare(Scm_OCIError *err, const char *sql);
extern void Scm_oracle_stmt_close(Scm_OCIStmt *stmt);
extern ScmObj Scm_oracle_stmt_bind_count(Scm_OCIError *err, Scm_OCIStmt *stmt);
extern ScmObj Scm_oracle_stmt_bind_init(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos, int type, u_int size);
extern ScmObj Scm_oracle_stmt_bind_set(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos, ScmObj val);
extern ScmObj Scm_oracle_stmt_bind_ref(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos);
extern ScmObj Scm_oracle_stmt_column_init(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos, int type, u_int size);
extern ScmObj Scm_oracle_stmt_column_ref(Scm_OCIError *err, Scm_OCIStmt *stmt, u_int pos);
extern ScmObj Scm_oracle_stmt_execute(Scm_OCIError *err, Scm_OCISvcCtx *svcctx, Scm_OCIStmt *stmt);
extern ScmObj Scm_oracle_stmt_fetch(Scm_OCIError *err, Scm_OCIStmt *stmt);
extern ScmObj Scm_oracle_stmt_type(Scm_OCIError *err, Scm_OCIStmt *stmt);
extern ScmObj Scm_oracle_stmt_row_count(Scm_OCIError *err, Scm_OCIStmt *stmt);
extern ScmObj Scm_oracle_stmt_params(Scm_OCIError *err, Scm_OCIStmt *stmt);

/* bind values */
typedef struct bind_handle_vptr bind_handle_vptr_t;
typedef struct bind_handle bind_handle_t;

struct bind_handle {
    const bind_handle_vptr_t *vptr;
    void *bindp; /* OCIBInd* or OCIDefine* */
    void *valuep;
    sb4 value_sz;
    sb2 ind;
    union {
        long l;
        double d;
    } value;
};

struct bind_handle_vptr {
    sb2 dty;
    void (*init)(bind_handle_t *hndl, u_int size);
    void (*clear)(bind_handle_t *hndl);
    void (*set)(bind_handle_t *hndl, ScmObj val);
    ScmObj (*get)(bind_handle_t *hndl);
};

extern void bind_handle_init(bind_handle_t *hndl, int dty, u_int size);

/* Epilogue */
SCM_DECL_END

#endif  /* GAUCHE_DBD_ORACLE_H */
