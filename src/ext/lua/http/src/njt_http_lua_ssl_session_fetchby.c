
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
#include "njt_http_lua_ssl_session_fetchby.h"
#include "njt_http_lua_ssl.h"
#include "njt_http_lua_directive.h"


/* Lua SSL cached session loading routines */
static void njt_http_lua_ssl_sess_fetch_done(void *data);
static void njt_http_lua_ssl_sess_fetch_aborted(void *data);
static u_char *njt_http_lua_log_ssl_sess_fetch_error(njt_log_t *log,
    u_char *buf, size_t len);
static njt_int_t njt_http_lua_ssl_sess_fetch_by_chunk(lua_State *L,
    njt_http_request_t *r);


/* load Lua code from a file for fetching cached SSL session */
njt_int_t
njt_http_lua_ssl_sess_fetch_handler_file(njt_http_request_t *r,
    njt_http_lua_srv_conf_t *lscf, lua_State *L)
{
    njt_int_t           rc;

    rc = njt_http_lua_cache_loadfile(r->connection->log, L,
                                     lscf->srv.ssl_sess_fetch_src.data,
                                     &lscf->srv.ssl_sess_fetch_src_ref,
                                     lscf->srv.ssl_sess_fetch_src_key);
    if (rc != NJT_OK) {
        return rc;
    }

    /*  make sure we have a valid code chunk */
    njt_http_lua_assert(lua_isfunction(L, -1));

    return njt_http_lua_ssl_sess_fetch_by_chunk(L, r);
}


/* load lua code from an inline snippet for fetching cached SSL session */
njt_int_t
njt_http_lua_ssl_sess_fetch_handler_inline(njt_http_request_t *r,
    njt_http_lua_srv_conf_t *lscf, lua_State *L)
{
    njt_int_t           rc;

    rc = njt_http_lua_cache_loadbuffer(r->connection->log, L,
                                       lscf->srv.ssl_sess_fetch_src.data,
                                       lscf->srv.ssl_sess_fetch_src.len,
                                       &lscf->srv.ssl_sess_fetch_src_ref,
                                       lscf->srv.ssl_sess_fetch_src_key,
                             (const char *) lscf->srv.ssl_sess_fetch_chunkname);
    if (rc != NJT_OK) {
        return rc;
    }

    /*  make sure we have a valid code chunk */
    njt_http_lua_assert(lua_isfunction(L, -1));

    return njt_http_lua_ssl_sess_fetch_by_chunk(L, r);
}


char *
njt_http_lua_ssl_sess_fetch_by_lua_block(njt_conf_t *cf, njt_command_t *cmd,
    void *conf)
{
    char        *rv;
    njt_conf_t   save;

    save = *cf;
    cf->handler = njt_http_lua_ssl_sess_fetch_by_lua;
    cf->handler_conf = conf;

    rv = njt_http_lua_conf_lua_block_parse(cf, cmd);

    *cf = save;

    return rv;
}


/* conf parser for directive ssl_session_fetch_by_lua */
char *
njt_http_lua_ssl_sess_fetch_by_lua(njt_conf_t *cf, njt_command_t *cmd,
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

    if (lscf->srv.ssl_sess_fetch_handler) {
        return "is duplicate";
    }

    if (njt_http_lua_ssl_init(cf->log) != NJT_OK) {
        return NJT_CONF_ERROR;
    }

    value = cf->args->elts;

    lscf->srv.ssl_sess_fetch_handler =
        (njt_http_lua_srv_conf_handler_pt) cmd->post;

    if (cmd->post == njt_http_lua_ssl_sess_fetch_handler_file) {
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

        lscf->srv.ssl_sess_fetch_src.data = name;
        lscf->srv.ssl_sess_fetch_src.len = njt_strlen(name);

    } else {
        cache_key = njt_http_lua_gen_chunk_cache_key(cf,
                                                     "ssl_session_fetch_by_lua",
                                                     value[1].data,
                                                     value[1].len);
        if (cache_key == NULL) {
            return NJT_CONF_ERROR;
        }

        chunkname = njt_http_lua_gen_chunk_name(cf, "ssl_session_fetch_by_lua",
                        sizeof("ssl_session_fetch_by_lua") - 1, &chunkname_len);
        if (chunkname == NULL) {
            return NJT_CONF_ERROR;
        }

        /* Don't eval njet variables for inline lua code */
        lscf->srv.ssl_sess_fetch_src = value[1];
        lscf->srv.ssl_sess_fetch_chunkname = chunkname;
    }

    lscf->srv.ssl_sess_fetch_src_key = cache_key;

    return NJT_CONF_OK;
}


/* cached session fetching callback to be set with SSL_CTX_sess_set_get_cb */
njt_ssl_session_t *
njt_http_lua_ssl_sess_fetch_handler(njt_ssl_conn_t *ssl_conn,
#if OPENSSL_VERSION_NUMBER >= 0x10100003L
    const
#endif
    u_char *id, int len, int *copy)
{
#if defined(NJT_SSL_TLSv1_3) && defined(TLS1_3_VERSION)
    int                              tls_version;
#endif
    lua_State                       *L;
    njt_int_t                        rc;
    njt_connection_t                *c, *fc = NULL;
    njt_http_request_t              *r = NULL;
    njt_pool_cleanup_t              *cln;
    njt_http_connection_t           *hc;
    njt_http_lua_ssl_ctx_t          *cctx;
    njt_http_lua_srv_conf_t         *lscf;
    njt_http_core_loc_conf_t        *clcf;

    /* set copy to 0 as we expect OpenSSL to handle
     * the memory of returned session */

    *copy = 0;

    c = njt_ssl_get_connection(ssl_conn);

#if defined(NJT_SSL_TLSv1_3) && defined(TLS1_3_VERSION)
    tls_version = SSL_version(ssl_conn);

    if (tls_version >= TLS1_3_VERSION) {
        njt_log_debug1(NJT_LOG_DEBUG_HTTP, c->log, 0,
                       "ssl_session_fetch_by_lua*: skipped since "
                       "TLS version >= 1.3 (%xd)", tls_version);

        return 0;
    }
#endif

    njt_log_debug1(NJT_LOG_DEBUG_HTTP, c->log, 0,
                   "ssl session fetch: connection reusable: %ud", c->reusable);

    cctx = njt_http_lua_ssl_get_ctx(c->ssl->connection);

    dd("ssl sess_fetch handler, sess_fetch-ctx=%p", cctx);

    if (cctx && cctx->entered_sess_fetch_handler) {
        /* not the first time */

        dd("here: %d", (int) cctx->entered_sess_fetch_handler);

        if (cctx->done) {
            njt_log_debug1(NJT_LOG_DEBUG_HTTP, c->log, 0,
                           "ssl_session_fetch_by_lua*: "
                           "sess get cb exit code: %d",
                           cctx->exit_code);

            dd("lua ssl sess_fetch done, finally");
            return cctx->session;
        }

#ifdef SSL_ERROR_PENDING_SESSION
        return SSL_magic_pending_session_ptr();
#else
        njt_log_error(NJT_LOG_CRIT, c->log, 0,
                      "lua: cannot yield in sess get cb: "
                      "missing async sess get cb support in OpenSSL");
        return NULL;
#endif
    }

    dd("first time");

#if (njet_version < 1017009)
    njt_reusable_connection(c, 0);
#endif

    hc = c->data;

    fc = njt_http_lua_create_fake_connection(NULL);
    if (fc == NULL) {
        goto failed;
    }

    fc->log->handler = njt_http_lua_log_ssl_sess_fetch_error;
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

    cctx->exit_code = 1;  /* successful by default */
    cctx->connection = c;
    cctx->request = r;
    cctx->session_id.data = (u_char *) id;
    cctx->session_id.len = len;
    cctx->entered_sess_fetch_handler = 1;
    cctx->done = 0;

    dd("setting cctx = %p", cctx);

    if (SSL_set_ex_data(c->ssl->connection, njt_http_lua_ssl_ctx_index, cctx)
        == 0)
    {
        njt_ssl_error(NJT_LOG_ALERT, c->log, 0, "SSL_set_ex_data() failed");
        goto failed;
    }

    lscf = njt_http_get_module_srv_conf(r, njt_http_lua_module);

    /* TODO honor lua_code_cache off */
    L = njt_http_lua_get_lua_vm(r, NULL);

    c->log->action = "fetching SSL session by lua";

    rc = lscf->srv.ssl_sess_fetch_handler(r, lscf, L);

    if (rc >= NJT_OK || rc == NJT_ERROR) {
        cctx->done = 1;

        if (cctx->cleanup) {
            *cctx->cleanup = NULL;
        }

        njt_log_debug2(NJT_LOG_DEBUG_HTTP, c->log, 0,
                       "ssl_session_fetch_by_lua*: handler return value: %i, "
                       "sess get cb exit code: %d", rc, cctx->exit_code);

        c->log->action = "SSL handshaking";
        return cctx->session;
    }

    /* rc == NJT_DONE */

    cln = njt_pool_cleanup_add(fc->pool, 0);
    if (cln == NULL) {
        goto failed;
    }

    cln->handler = njt_http_lua_ssl_sess_fetch_done;
    cln->data = cctx;

    if (cctx->cleanup == NULL)  {
        /* we only want exactly one cleanup handler to be registered with the
         * connection to clean up cctx when connection is aborted */
        cln = njt_pool_cleanup_add(c->pool, 0);
        if (cln == NULL) {
            goto failed;
        }

        cln->data = cctx;
        cctx->cleanup = &cln->handler;
    }

    *cctx->cleanup = njt_http_lua_ssl_sess_fetch_aborted;

#ifdef SSL_ERROR_PENDING_SESSION
    return SSL_magic_pending_session_ptr();
#else
    njt_log_error(NJT_LOG_CRIT, c->log, 0,
                  "lua: cannot yield in sess get cb: "
                  "missing async sess get cb support in OpenSSL");

    /* fall through to the "failed" label below */
#endif

failed:

    if (r && r->pool) {
        njt_http_lua_free_fake_request(r);
    }

    if (fc) {
        njt_http_lua_close_fake_connection(fc);
    }

    return NULL;
}


static void
njt_http_lua_ssl_sess_fetch_done(void *data)
{
    njt_connection_t                *c;
    njt_http_lua_ssl_ctx_t          *cctx = data;

    dd("lua ssl sess_fetch done");

    if (cctx->aborted) {
        return;
    }

    njt_http_lua_assert(cctx->done == 0);

    cctx->done = 1;

    if (cctx->cleanup) {
        *cctx->cleanup = NULL;
    }

    c = cctx->connection;

    c->log->action = "SSL handshaking";

    njt_post_event(c->write, &njt_posted_events);
}


static void
njt_http_lua_ssl_sess_fetch_aborted(void *data)
{
    njt_http_lua_ssl_ctx_t          *cctx = data;

    dd("lua ssl sess_fetch done");

    if (cctx->done) {
        /* completed successfully already */
        return;
    }

    njt_log_debug0(NJT_LOG_DEBUG_HTTP, cctx->connection->log, 0,
                   "ssl_session_fetch_by_lua*: sess_fetch cb aborted");

    cctx->aborted = 1;
    cctx->request->connection->ssl = NULL;

    njt_http_lua_finalize_fake_request(cctx->request, NJT_ERROR);
}


static u_char *
njt_http_lua_log_ssl_sess_fetch_error(njt_log_t *log, u_char *buf, size_t len)
{
    u_char              *p;
    njt_connection_t    *c;

    if (log->action) {
        p = njt_snprintf(buf, len, " while %s", log->action);
        len -= p - buf;
        buf = p;
    }

    p = njt_snprintf(buf, len, ", context: ssl_session_fetch_by_lua*");
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


/* initialize lua coroutine for fetching cached session */
static njt_int_t
njt_http_lua_ssl_sess_fetch_by_chunk(lua_State *L, njt_http_request_t *r)
{
    int                      co_ref;
    njt_int_t                rc;
    lua_State               *co;
    njt_http_lua_ctx_t      *ctx;
    njt_pool_cleanup_t      *cln;

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

    /*  {{{ new coroutine to handle request */
    co = njt_http_lua_new_thread(r, L, &co_ref);

    if (co == NULL) {
        njt_log_error(NJT_LOG_ERR, r->connection->log, 0,
                      "lua: failed to create new coroutine to handle request");

        rc = NJT_ERROR;
        njt_http_lua_finalize_request(r, rc);
        return rc;
    }

    /*  move code closure to new coroutine */
    lua_xmove(L, co, 1);

#ifndef OPENRESTY_LUAJIT
    /*  set closure's env table to new coroutine's globals table */
    njt_http_lua_get_globals_table(co);
    lua_setfenv(co, -2);
#endif

    /* save njet request in coroutine globals table */
    njt_http_lua_set_req(co, r);

    ctx->cur_co_ctx = &ctx->entry_co_ctx;
    ctx->cur_co_ctx->co = co;
    ctx->cur_co_ctx->co_ref = co_ref;
#ifdef NJT_LUA_USE_ASSERT
    ctx->cur_co_ctx->co_top = 1;
#endif

    njt_http_lua_attach_co_ctx_to_L(co, ctx->cur_co_ctx);

    /* register request cleanup hooks */
    if (ctx->cleanup == NULL) {
        cln = njt_pool_cleanup_add(r->pool, 0);
        if (cln == NULL) {
            rc = NJT_ERROR;
            njt_http_lua_finalize_request(r, rc);
            return rc;
        }

        cln->handler = njt_http_lua_request_cleanup_handler;
        cln->data = ctx;
        ctx->cleanup = &cln->handler;
    }

    ctx->context = NJT_HTTP_LUA_CONTEXT_SSL_SESS_FETCH;

    rc = njt_http_lua_run_thread(L, r, ctx, 0);

    if (rc == NJT_ERROR || rc >= NJT_OK) {
        /* do nothing */

    } else if (rc == NJT_AGAIN) {
        rc = njt_http_lua_content_run_posted_threads(L, r, ctx, 0);

    } else if (rc == NJT_DONE) {
        rc = njt_http_lua_content_run_posted_threads(L, r, ctx, 1);

    } else {
        rc = NJT_OK;
    }

    njt_http_lua_finalize_request(r, rc);
    return rc;
}


/* de-serialized a SSL session and set it back to the request at lua context */
int
njt_http_lua_ffi_ssl_set_serialized_session(njt_http_request_t *r,
    const unsigned char *data, int len, char **err)
{
    u_char                          *p;
    u_char                           buf[NJT_SSL_MAX_SESSION_SIZE];
    njt_ssl_conn_t                  *ssl_conn;
    njt_connection_t                *c;
    njt_ssl_session_t               *session = NULL;
    njt_ssl_session_t               *old_session;
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

    njt_memcpy(buf, data, len);
    p = buf;
    session = d2i_SSL_SESSION(NULL, (const unsigned char **)&p,  len);
    if (session == NULL) {
        ERR_clear_error();
        *err = "failed to de-serialize session";
        return NJT_ERROR;
    }

    cctx = njt_http_lua_ssl_get_ctx(c->ssl->connection);
    if (cctx == NULL) {
        *err = "bad lua context";
        return NJT_ERROR;
    }

    old_session = cctx->session;
    cctx->session = session;

    if (old_session != NULL) {
        njt_ssl_free_session(old_session);
    }

    return NJT_OK;
}


#endif /* NJT_HTTP_SSL */
