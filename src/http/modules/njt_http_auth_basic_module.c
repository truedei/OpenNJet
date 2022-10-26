
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 * Copyright (C) TMLake, Inc.
 */


#include <njt_config.h>
#include <njt_core.h>
#include <njt_http.h>
#include <njt_crypt.h>


#define NJT_HTTP_AUTH_BUF_SIZE  2048


typedef struct {
    njt_http_complex_value_t  *realm;
    njt_http_complex_value_t  *user_file;
} njt_http_auth_basic_loc_conf_t;


static njt_int_t njt_http_auth_basic_handler(njt_http_request_t *r);
static njt_int_t njt_http_auth_basic_crypt_handler(njt_http_request_t *r,
    njt_str_t *passwd, njt_str_t *realm);
static njt_int_t njt_http_auth_basic_set_realm(njt_http_request_t *r,
    njt_str_t *realm);
static void *njt_http_auth_basic_create_loc_conf(njt_conf_t *cf);
static char *njt_http_auth_basic_merge_loc_conf(njt_conf_t *cf,
    void *parent, void *child);
static njt_int_t njt_http_auth_basic_init(njt_conf_t *cf);
static char *njt_http_auth_basic_user_file(njt_conf_t *cf, njt_command_t *cmd,
    void *conf);


static njt_command_t  njt_http_auth_basic_commands[] = {

    { njt_string("auth_basic"),
      NJT_HTTP_MAIN_CONF|NJT_HTTP_SRV_CONF|NJT_HTTP_LOC_CONF|NJT_HTTP_LMT_CONF
                        |NJT_CONF_TAKE1,
      njt_http_set_complex_value_slot,
      NJT_HTTP_LOC_CONF_OFFSET,
      offsetof(njt_http_auth_basic_loc_conf_t, realm),
      NULL },

    { njt_string("auth_basic_user_file"),
      NJT_HTTP_MAIN_CONF|NJT_HTTP_SRV_CONF|NJT_HTTP_LOC_CONF|NJT_HTTP_LMT_CONF
                        |NJT_CONF_TAKE1,
      njt_http_auth_basic_user_file,
      NJT_HTTP_LOC_CONF_OFFSET,
      offsetof(njt_http_auth_basic_loc_conf_t, user_file),
      NULL },

      njt_null_command
};


static njt_http_module_t  njt_http_auth_basic_module_ctx = {
    NULL,                                  /* preconfiguration */
    njt_http_auth_basic_init,              /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    njt_http_auth_basic_create_loc_conf,   /* create location configuration */
    njt_http_auth_basic_merge_loc_conf     /* merge location configuration */
};


njt_module_t  njt_http_auth_basic_module = {
    NJT_MODULE_V1,
    &njt_http_auth_basic_module_ctx,       /* module context */
    njt_http_auth_basic_commands,          /* module directives */
    NJT_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NJT_MODULE_V1_PADDING
};


static njt_int_t
njt_http_auth_basic_handler(njt_http_request_t *r)
{
    off_t                            offset;
    ssize_t                          n;
    njt_fd_t                         fd;
    njt_int_t                        rc;
    njt_err_t                        err;
    njt_str_t                        pwd, realm, user_file;
    njt_uint_t                       i, level, login, left, passwd;
    njt_file_t                       file;
    njt_http_auth_basic_loc_conf_t  *alcf;
    u_char                           buf[NJT_HTTP_AUTH_BUF_SIZE];
    enum {
        sw_login,
        sw_passwd,
        sw_skip
    } state;

    alcf = njt_http_get_module_loc_conf(r, njt_http_auth_basic_module);

    if (alcf->realm == NULL || alcf->user_file == NULL) {
        return NJT_DECLINED;
    }

    if (njt_http_complex_value(r, alcf->realm, &realm) != NJT_OK) {
        return NJT_ERROR;
    }

    if (realm.len == 3 && njt_strncmp(realm.data, "off", 3) == 0) {
        return NJT_DECLINED;
    }

    rc = njt_http_auth_basic_user(r);

    if (rc == NJT_DECLINED) {

        njt_log_error(NJT_LOG_INFO, r->connection->log, 0,
                      "no user/password was provided for basic authentication");

        return njt_http_auth_basic_set_realm(r, &realm);
    }

    if (rc == NJT_ERROR) {
        return NJT_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (njt_http_complex_value(r, alcf->user_file, &user_file) != NJT_OK) {
        return NJT_ERROR;
    }

    fd = njt_open_file(user_file.data, NJT_FILE_RDONLY, NJT_FILE_OPEN, 0);

    if (fd == NJT_INVALID_FILE) {
        err = njt_errno;

        if (err == NJT_ENOENT) {
            level = NJT_LOG_ERR;
            rc = NJT_HTTP_FORBIDDEN;

        } else {
            level = NJT_LOG_CRIT;
            rc = NJT_HTTP_INTERNAL_SERVER_ERROR;
        }

        njt_log_error(level, r->connection->log, err,
                      njt_open_file_n " \"%s\" failed", user_file.data);

        return rc;
    }

    njt_memzero(&file, sizeof(njt_file_t));

    file.fd = fd;
    file.name = user_file;
    file.log = r->connection->log;

    state = sw_login;
    passwd = 0;
    login = 0;
    left = 0;
    offset = 0;

    for ( ;; ) {
        i = left;

        n = njt_read_file(&file, buf + left, NJT_HTTP_AUTH_BUF_SIZE - left,
                          offset);

        if (n == NJT_ERROR) {
            rc = NJT_HTTP_INTERNAL_SERVER_ERROR;
            goto cleanup;
        }

        if (n == 0) {
            break;
        }

        for (i = left; i < left + n; i++) {
            switch (state) {

            case sw_login:
                if (login == 0) {

                    if (buf[i] == '#' || buf[i] == CR) {
                        state = sw_skip;
                        break;
                    }

                    if (buf[i] == LF) {
                        break;
                    }
                }

                if (buf[i] != r->headers_in.user.data[login]) {
                    state = sw_skip;
                    break;
                }

                if (login == r->headers_in.user.len) {
                    state = sw_passwd;
                    passwd = i + 1;
                }

                login++;

                break;

            case sw_passwd:
                if (buf[i] == LF || buf[i] == CR || buf[i] == ':') {
                    buf[i] = '\0';

                    pwd.len = i - passwd;
                    pwd.data = &buf[passwd];

                    rc = njt_http_auth_basic_crypt_handler(r, &pwd, &realm);
                    goto cleanup;
                }

                break;

            case sw_skip:
                if (buf[i] == LF) {
                    state = sw_login;
                    login = 0;
                }

                break;
            }
        }

        if (state == sw_passwd) {
            left = left + n - passwd;
            njt_memmove(buf, &buf[passwd], left);
            passwd = 0;

        } else {
            left = 0;
        }

        offset += n;
    }

    if (state == sw_passwd) {
        pwd.len = i - passwd;
        pwd.data = njt_pnalloc(r->pool, pwd.len + 1);
        if (pwd.data == NULL) {
            return NJT_HTTP_INTERNAL_SERVER_ERROR;
        }

        njt_cpystrn(pwd.data, &buf[passwd], pwd.len + 1);

        rc = njt_http_auth_basic_crypt_handler(r, &pwd, &realm);
        goto cleanup;
    }

    njt_log_error(NJT_LOG_ERR, r->connection->log, 0,
                  "user \"%V\" was not found in \"%s\"",
                  &r->headers_in.user, user_file.data);

    rc = njt_http_auth_basic_set_realm(r, &realm);

cleanup:

    if (njt_close_file(file.fd) == NJT_FILE_ERROR) {
        njt_log_error(NJT_LOG_ALERT, r->connection->log, njt_errno,
                      njt_close_file_n " \"%s\" failed", user_file.data);
    }

    njt_explicit_memzero(buf, NJT_HTTP_AUTH_BUF_SIZE);

    return rc;
}


static njt_int_t
njt_http_auth_basic_crypt_handler(njt_http_request_t *r, njt_str_t *passwd,
    njt_str_t *realm)
{
    njt_int_t   rc;
    u_char     *encrypted;

    rc = njt_crypt(r->pool, r->headers_in.passwd.data, passwd->data,
                   &encrypted);

    njt_log_debug3(NJT_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "rc: %i user: \"%V\" salt: \"%s\"",
                   rc, &r->headers_in.user, passwd->data);

    if (rc != NJT_OK) {
        return NJT_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (njt_strcmp(encrypted, passwd->data) == 0) {
        return NJT_OK;
    }

    njt_log_debug1(NJT_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "encrypted: \"%s\"", encrypted);

    njt_log_error(NJT_LOG_ERR, r->connection->log, 0,
                  "user \"%V\": password mismatch",
                  &r->headers_in.user);

    return njt_http_auth_basic_set_realm(r, realm);
}


static njt_int_t
njt_http_auth_basic_set_realm(njt_http_request_t *r, njt_str_t *realm)
{
    size_t   len;
    u_char  *basic, *p;

    r->headers_out.www_authenticate = njt_list_push(&r->headers_out.headers);
    if (r->headers_out.www_authenticate == NULL) {
        return NJT_HTTP_INTERNAL_SERVER_ERROR;
    }

    len = sizeof("Basic realm=\"\"") - 1 + realm->len;

    basic = njt_pnalloc(r->pool, len);
    if (basic == NULL) {
        r->headers_out.www_authenticate->hash = 0;
        r->headers_out.www_authenticate = NULL;
        return NJT_HTTP_INTERNAL_SERVER_ERROR;
    }

    p = njt_cpymem(basic, "Basic realm=\"", sizeof("Basic realm=\"") - 1);
    p = njt_cpymem(p, realm->data, realm->len);
    *p = '"';

    r->headers_out.www_authenticate->hash = 1;
    r->headers_out.www_authenticate->next = NULL;
    njt_str_set(&r->headers_out.www_authenticate->key, "WWW-Authenticate");
    r->headers_out.www_authenticate->value.data = basic;
    r->headers_out.www_authenticate->value.len = len;

    return NJT_HTTP_UNAUTHORIZED;
}


static void *
njt_http_auth_basic_create_loc_conf(njt_conf_t *cf)
{
    njt_http_auth_basic_loc_conf_t  *conf;

    conf = njt_pcalloc(cf->pool, sizeof(njt_http_auth_basic_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->realm = NJT_CONF_UNSET_PTR;
    conf->user_file = NJT_CONF_UNSET_PTR;

    return conf;
}


static char *
njt_http_auth_basic_merge_loc_conf(njt_conf_t *cf, void *parent, void *child)
{
    njt_http_auth_basic_loc_conf_t  *prev = parent;
    njt_http_auth_basic_loc_conf_t  *conf = child;

    njt_conf_merge_ptr_value(conf->realm, prev->realm, NULL);
    njt_conf_merge_ptr_value(conf->user_file, prev->user_file, NULL);

    return NJT_CONF_OK;
}


static njt_int_t
njt_http_auth_basic_init(njt_conf_t *cf)
{
    njt_http_handler_pt        *h;
    njt_http_core_main_conf_t  *cmcf;

    cmcf = njt_http_conf_get_module_main_conf(cf, njt_http_core_module);

    h = njt_array_push(&cmcf->phases[NJT_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NJT_ERROR;
    }

    *h = njt_http_auth_basic_handler;

    return NJT_OK;
}


static char *
njt_http_auth_basic_user_file(njt_conf_t *cf, njt_command_t *cmd, void *conf)
{
    njt_http_auth_basic_loc_conf_t *alcf = conf;

    njt_str_t                         *value;
    njt_http_compile_complex_value_t   ccv;

    if (alcf->user_file != NJT_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    alcf->user_file = njt_palloc(cf->pool, sizeof(njt_http_complex_value_t));
    if (alcf->user_file == NULL) {
        return NJT_CONF_ERROR;
    }

    value = cf->args->elts;

    njt_memzero(&ccv, sizeof(njt_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = alcf->user_file;
    ccv.zero = 1;
    ccv.conf_prefix = 1;

    if (njt_http_compile_complex_value(&ccv) != NJT_OK) {
        return NJT_CONF_ERROR;
    }

    return NJT_CONF_OK;
}
