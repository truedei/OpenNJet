
/*
 * Copyright (C) Yichun Zhang (agentzh)
 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.yy
 */


#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"


#if (NJT_HTTP_SSL)


#include "njt_http_lua_cache.h"
#include "njt_http_lua_initworkerby.h"
#include "njt_http_lua_util.h"
#include "njt_http_ssl_module.h"
#include "njt_http_lua_contentby.h"
#include "njt_http_lua_ssl_session_storeby.h"
#include "njt_http_lua_ssl.h"
#include "njt_http_lua_directive.h"


/* Lua SSL new session store routines */
static u_char *njt_http_lua_log_ssl_sess_store_error(njt_log_t *log,
    u_char *buf, size_t len);
static njt_int_t njt_http_lua_ssl_sess_store_by_chunk(lua_State *L,
    njt_http_request_t *r);


/* load Lua code from a file for caching new SSL session. */
njt_int_t
njt_http_lua_ssl_sess_store_handler_file(njt_http_request_t *r,
    njt_http_lua_srv_conf_t *lscf, lua_State *L)
{
    njt_int_t           rc;

    rc = njt_http_lua_cache_loadfile(r->connection->log, L,
                                     lscf->srv.ssl_sess_store_src.data,
                                     &lscf->srv.ssl_sess_store_src_ref,
                                     lscf->srv.ssl_sess_store_src_key);
    if (rc != NJT_OK) {
        return rc;
    }

    /*  make sure we have a valid code chunk */
    njt_http_lua_assert(lua_isfunction(L, -1));

    return njt_http_lua_ssl_sess_store_by_chunk(L, r);
}


/* load lua code from an inline snippet for caching new SSL session */
njt_int_t
njt_http_lua_ssl_sess_store_handler_inline(njt_http_request_t *r,
    njt_http_lua_srv_conf_t *lscf, lua_State *L)
{
    njt_int_t           rc;

    rc = njt_http_lua_cache_loadbuffer(r->connection->log, L,
                                       lscf->srv.ssl_sess_store_src.data,
                                       lscf->srv.ssl_sess_store_src.len,
                                       &lscf->srv.ssl_sess_store_src_ref,
                                       lscf->srv.ssl_sess_store_src_key,
                             (const char *) lscf->srv.ssl_sess_store_chunkname);
    if (rc != NJT_OK) {
        return rc;
    }

    /*  make sure we have a valid code chunk */
    njt_http_lua_assert(lua_isfunction(L, -1));

    return njt_http_lua_ssl_sess_store_by_chunk(L, r);
}


char *
njt_http_lua_ssl_sess_store_by_lua_block(njt_conf_t *cf, njt_command_t *cmd,
    void *conf)
{
    char        *rv;
    njt_conf_t   save;

    save = *cf;
    cf->handler = njt_http_lua_ssl_sess_store_by_lua;
    cf->handler_conf = conf;

    rv = njt_http_lua_conf_lua_block_parse(cf, cmd);

    *cf = save;

    return rv;
}


/* conf parser for directive ssl_session_store_by_lua */
char *
njt_http_lua_ssl_sess_store_by_lua(njt_conf_t *cf, njt_command_t *cmd,
    void *conf)
{
    size_t                       chunkname_len;
    u_char                      *chunkname;
    u_char                      *cache_key = NULL;
    u_char                      *name;
    njt_str_t                   *value;
    njt_http_lua_srv_conf_t     *lscf = conf;

    dd("enter");

    /*  must specify a content handler */
    if (cmd->post == NULL) {
        return NJT_CONF_ERROR;
    }

    if (lscf->srv.ssl_sess_store_handler) {
        return "is duplicate";
    }

    if (njt_http_lua_ssl_init(cf->log) != NJT_OK) {
        return NJT_CONF_ERROR;
    }

    value = cf->args->elts;

    lscf->srv.ssl_sess_store_handler =
        (njt_http_lua_srv_conf_handler_pt) cmd->post;

    if (cmd->post == njt_http_lua_ssl_sess_store_handler_file) {
        /* Lua code in an external file */
        name = njt_http_lua_rebase_path(cf->pool, value[1].data,
                                        value[1].len);
        if (name == NULL) {
            return NJT_CONF_ERROR;
        }

        cache_key = njt_http_lua_gen_file_cache_key(cf, value[1].data,
                                                    value[1].len);
        if (cache_key == NULL) {
            return NJT_CONF_ERROR;
        }

        lscf->srv.ssl_sess_store_src.data = name;
        lscf->srv.ssl_sess_store_src.len = njt_strlen(name);

    } else {
        cache_key = njt_http_lua_gen_chunk_cache_key(cf,
                                                     "ssl_session_store_by_lua",
                                                     value[1].data,
                                                     value[1].len);
        if (cache_key == NULL) {
            return NJT_CONF_ERROR;
        }

        chunkname = njt_http_lua_gen_chunk_name(cf, "ssl_session_store_by_lua",
                        sizeof("ssl_session_store_by_lua") - 1, &chunkname_len);
        if (chunkname == NULL) {
            return NJT_CONF_ERROR;
        }

        /* Don't eval njet variables for inline lua code */
        lscf->srv.ssl_sess_store_src = value[1];
        lscf->srv.ssl_sess_store_chunkname = chunkname;
    }

    lscf->srv.ssl_sess_store_src_key = cache_key;

    return NJT_CONF_OK;
}


/* callback for new session caching, to be set with SSL_CTX_sess_set_new_cb */
int
njt_http_lua_ssl_sess_store_handler(njt_ssl_conn_t *ssl_conn,
    njt_ssl_session_t *sess)
{
#if defined(NJT_SSL_TLSv1_3) && defined(TLS1_3_VERSION)
    int                              tls_version;
#endif
    const u_char                    *sess_id;
    unsigned int                     sess_id_len;
    lua_State                       *L;
    njt_int_t                        rc;
    njt_connection_t                *c, *fc = NULL;
    njt_http_request_t              *r = NULL;
    njt_http_connection_t           *hc;
    njt_http_lua_ssl_ctx_t          *cctx;
    njt_http_lua_srv_conf_t         *lscf;
    njt_http_core_loc_conf_t        *clcf;

    c = njt_ssl_get_connection(ssl_conn);

#if defined(NJT_SSL_TLSv1_3) && defined(TLS1_3_VERSION)
    tls_version = SSL_version(ssl_conn);

    if (tls_version >= TLS1_3_VERSION) {
        njt_log_debug1(NJT_LOG_DEBUG_HTTP, c->log, 0,
                       "ssl_session_store_by_lua*: skipped since "
                       "TLS version >= 1.3 (%xd)", tls_version);

        return 0;
    }
#endif

    njt_log_debug1(NJT_LOG_DEBUG_HTTP, c->log, 0,
                   "ssl session store: connection reusable: %ud", c->reusable);

    cctx = njt_http_lua_ssl_get_ctx(c->ssl->connection);

    dd("ssl sess_store handler, sess_store-ctx=%p", cctx);

    hc = c->data;

    fc = njt_http_lua_create_fake_connection(NULL);
    if (fc == NULL) {
        goto failed;
    }

    fc->log->handler = njt_http_lua_log_ssl_sess_store_error;
    fc->log->data = fc;

    fc->addr_text = c->addr_text;
    fc->listening = c->listening;

    r = njt_http_lua_create_fake_request(fc);
    if (r == NULL) {
        goto failed;
    }

    r->main_conf = hc->conf_ctx->main_conf;
    r->srv_conf = hc->conf_ctx->srv_conf;
    r->loc_conf = hc->conf_ctx->loc_conf;

    fc->log->file = c->log->file;
    fc->log->log_level = c->log->log_level;
    fc->ssl = c->ssl;

    clcf = njt_http_get_module_loc_conf(r, njt_http_core_module);

#if (njet_version >= 1009000)
    njt_set_connection_log(fc, clcf->error_log);

#else
    njt_http_set_connection_log(fc, clcf->error_log);
#endif

    if (cctx == NULL) {
        cctx = njt_pcalloc(c->pool, sizeof(njt_http_lua_ssl_ctx_t));
        if (cctx == NULL) {
            goto failed;  /* error */
        }

        cctx->ctx_ref = LUA_NOREF;
    }

#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
    sess_id = SSL_SESSION_get_id(sess, &sess_id_len);
#else
    sess_id = sess->session_id;
    sess_id_len = sess->session_id_length;
#endif

    cctx->connection = c;
    cctx->request = r;
    cctx->session = sess;
    cctx->session_id.data = (u_char *) sess_id;
    cctx->session_id.len = sess_id_len;
    cctx->done = 0;

    dd("setting cctx");

    if (SSL_set_ex_data(c->ssl->connection, njt_http_lua_ssl_ctx_index, cctx)
        == 0)
    {
        njt_ssl_error(NJT_LOG_ALERT, c->log, 0, "SSL_set_ex_data() failed");
        goto failed;
    }

    lscf = njt_http_get_module_srv_conf(r, njt_http_lua_module);

    /* TODO honor lua_code_cache off */
    L = njt_http_lua_get_lua_vm(r, NULL);

    c->log->action = "storing SSL session by lua";

    rc = lscf->srv.ssl_sess_store_handler(r, lscf, L);

    if (rc >= NJT_OK || rc == NJT_ERROR) {
        cctx->done = 1;

        njt_log_debug2(NJT_LOG_DEBUG_HTTP, c->log, 0,
                       "ssl_session_store_by_lua*: handler return value: %i, "
                       "sess new cb exit code: %d", rc, cctx->exit_code);

        c->log->action = "SSL handshaking";

        /* Return value is a flag indicating whether the passed-in session
         * has been freed by this callback; always return 0 so OpenSSL will
         * free the session. Nginx's own session caching logic has the same
         * practice. */
        return 0;
    }

    /* impossible to reach here */
    njt_http_lua_assert(0);

failed:

    if (r && r->pool) {
        njt_http_lua_free_fake_request(r);
    }

    if (fc) {
        njt_http_lua_close_fake_connection(fc);
    }

    return 0;
}


static u_char *
njt_http_lua_log_ssl_sess_store_error(njt_log_t *log, u_char *buf, size_t len)
{
    u_char              *p;
    njt_connection_t    *c;

    if (log->action) {
        p = njt_snprintf(buf, len, " while %s", log->action);
        len -= p - buf;
        buf = p;
    }

    p = njt_snprintf(buf, len, ", context: ssl_session_store_by_lua*");
    len -= p - buf;
    buf = p;

    c = log->data;

    if (c != NULL) {
        if (c->addr_text.len) {
            p = njt_snprintf(buf, len, ", client: %V", &c->addr_text);
            len -= p - buf;
            buf = p;
        }

        if (c->listening && c->listening->addr_text.len) {
            p = njt_snprintf(buf, len, ", server: %V",
                             &c->listening->addr_text);
            buf = p;
        }
    }

    return buf;
}


/* initialize lua coroutine for caching new SSL session */
static njt_int_t
njt_http_lua_ssl_sess_store_by_chunk(lua_State *L, njt_http_request_t *r)
{
    size_t                   len;
    u_char                  *err_msg;
    njt_int_t                rc;
    njt_http_lua_ctx_t      *ctx;

    ctx = njt_http_get_module_ctx(r, njt_http_lua_module);

    if (ctx == NULL) {
        ctx = njt_http_lua_create_ctx(r);
        if (ctx == NULL) {
            rc = NJT_ERROR;
            njt_http_lua_finalize_request(r, rc);
            return rc;
        }

    } else {
        dd("reset ctx");
        njt_http_lua_reset_ctx(r, L, ctx);
    }

    ctx->entered_content_phase = 1;
    ctx->context = NJT_HTTP_LUA_CONTEXT_SSL_SESS_STORE;

    /* init njet context in Lua VM */
    njt_http_lua_set_req(L, r);

#ifndef OPENRESTY_LUAJIT
    njt_http_lua_create_new_globals_table(L, 0 /* narr */, 1 /* nrec */);

    /*  {{{ make new env inheriting main thread's globals table */
    lua_createtable(L, 0, 1 /* nrec */);   /* the metatable for the new env */
    njt_http_lua_get_globals_table(L);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);    /*  setmetatable({}, {__index = _G}) */
    /*  }}} */

    lua_setfenv(L, -2);    /*  set new running env for the code closure */
#endif

    lua_pushcfunction(L, njt_http_lua_traceback);
    lua_insert(L, 1);  /* put it under chunk and args */

    /*  protected call user code */
    rc = lua_pcall(L, 0, 1, 1);

    lua_remove(L, 1);  /* remove traceback function */

    dd("rc == %d", (int) rc);

    if (rc != 0) {
        /*  error occurred when running loaded code */
        err_msg = (u_char *) lua_tolstring(L, -1, &len);

        if (err_msg == NULL) {
            err_msg = (u_char *) "unknown reason";
            len = sizeof("unknown reason") - 1;
        }

        njt_log_error(NJT_LOG_ERR, r->connection->log, 0,
                      "failed to run session_store_by_lua*: %*s", len, err_msg);

        lua_settop(L, 0); /*  clear remaining elems on stack */
        njt_http_lua_finalize_request(r, rc);

        return NJT_ERROR;
    }

    lua_settop(L, 0); /*  clear remaining elems on stack */
    njt_http_lua_finalize_request(r, rc);
    return rc;
}


/* serialize a session from lua context into buf.
 * the memory allocation of buf should be handled externally. */
int
njt_http_lua_ffi_ssl_get_serialized_session(njt_http_request_t *r,
    u_char *buf, char **err)
{
    njt_ssl_conn_t                  *ssl_conn;
    njt_connection_t                *c;
    njt_ssl_session_t               *session;
    njt_http_lua_ssl_ctx_t          *cctx;

    c = r->connection;

    if (c == NULL || c->ssl == NULL) {
        *err = "bad request";
        return NJT_ERROR;
    }

    ssl_conn = c->ssl->connection;
    if (ssl_conn == NULL) {
        *err = "bad ssl conn";
        return NJT_ERROR;
    }

    dd("get cctx session");

    cctx = njt_http_lua_ssl_get_ctx(c->ssl->connection);
    if (cctx == NULL) {
        *err = "bad lua context";
        return NJT_ERROR;
    }

    session = cctx->session;
    if (session == NULL) {
        *err = "bad session in lua context";
        return NJT_ERROR;
    }

    if (i2d_SSL_SESSION(session, &buf) == 0) {
        *err = "i2d_SSL_SESSION() failed";
        return NJT_ERROR;
    }

    return NJT_OK;
}


/* return the size of serialized session. */
int
njt_http_lua_ffi_ssl_get_serialized_session_size(njt_http_request_t *r,
    char **err)
{
    int                              len;
    njt_ssl_conn_t                  *ssl_conn;
    njt_connection_t                *c;
    njt_ssl_session_t               *session;
    njt_http_lua_ssl_ctx_t          *cctx;

    c = r->connection;

    if (c == NULL || c->ssl == NULL) {
        *err = "bad request";
        return NJT_ERROR;
    }

    ssl_conn = c->ssl->connection;
    if (ssl_conn == NULL) {
        *err = "bad ssl conn";
        return NJT_ERROR;
    }

    dd("get cctx session size");
    cctx = njt_http_lua_ssl_get_ctx(c->ssl->connection);
    if (cctx == NULL) {
        *err = "bad lua context";
        return NJT_ERROR;
    }

    session = cctx->session;
    if (session == NULL) {
        *err = "bad session in lua context";
        return NJT_ERROR;
    }

    len = i2d_SSL_SESSION(session, NULL);
    if (len == 0) {
        *err = "i2d_SSL_SESSION() failed";
        return NJT_ERROR;
    }

    return len;
}


/* serialize the session id from lua context into buf.
 * the memory allocation of buf should be handled externally. */
int
njt_http_lua_ffi_ssl_get_session_id(njt_http_request_t *r,
    u_char *buf, char **err)
{
    int                              id_len;
    u_char                          *id;
    njt_ssl_conn_t                  *ssl_conn;
    njt_connection_t                *c;
    njt_http_lua_ssl_ctx_t          *cctx;

    c = r->connection;

    if (c == NULL || c->ssl == NULL) {
        *err = "bad request";
        return NJT_ERROR;
    }

    ssl_conn = c->ssl->connection;
    if (ssl_conn == NULL) {
        *err = "bad ssl conn";
        return NJT_ERROR;
    }

    dd("get cctx session");
    cctx = njt_http_lua_ssl_get_ctx(c->ssl->connection);
    if (cctx == NULL) {
        *err = "bad lua context";
        return NJT_ERROR;
    }

    id = cctx->session_id.data;
    if (id == NULL) {
        *err = "uninitialized session id in lua context";
        return NJT_ERROR;
    }

    id_len = cctx->session_id.len;
    if (id_len == 0) {
        *err = "uninitialized session id len in lua context";
        return NJT_ERROR;
    }

    njt_hex_dump(buf, id, id_len);

    return NJT_OK;
}


/* return the size of serialized session id. */
int
njt_http_lua_ffi_ssl_get_session_id_size(njt_http_request_t *r,
    char **err)
{
    njt_ssl_conn_t                  *ssl_conn;
    njt_connection_t                *c;
    njt_http_lua_ssl_ctx_t          *cctx;

    c = r->connection;

    if (c == NULL || c->ssl == NULL) {
        *err = "bad request";
        return NJT_ERROR;
    }

    ssl_conn = c->ssl->connection;
    if (ssl_conn == NULL) {
        *err = "bad ssl conn";
        return NJT_ERROR;
    }

    dd("get cctx session");
    cctx = njt_http_lua_ssl_get_ctx(c->ssl->connection);
    if (cctx == NULL) {
        *err = "bad lua context";
        return NJT_ERROR;
    }

    if (cctx->session_id.len == 0) {
        *err = "uninitialized session id len in lua context";
        return NJT_ERROR;
    }

    /* since the session id will be hex dumped to serialize, the serialized
     * session will be twice the size of the session id: each byte will be a
     * 2-digit hex value. */

    return 2 * cctx->session_id.len;
}


#endif /* NJT_HTTP_SSL */
