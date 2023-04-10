/*
 * Copyright (C) Yichun Zhang (agentzh)
 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */

#ifndef _NJT_STREAM_LUA_SSL_CLIENT_HELLOBY_H_INCLUDED_
#define _NJT_STREAM_LUA_SSL_CLIENT_HELLOBY_H_INCLUDED_

#include "njt_stream_lua_common.h"

#if (NJT_STREAM_SSL)

njt_int_t njt_stream_lua_ssl_client_hello_handler_inline(
    njt_stream_lua_request_t *r, njt_stream_lua_srv_conf_t *lscf, lua_State *L);

njt_int_t njt_stream_lua_ssl_client_hello_handler_file(
    njt_stream_lua_request_t *r, njt_stream_lua_srv_conf_t *lscf, lua_State *L);

char *njt_stream_lua_ssl_client_hello_by_lua_block(njt_conf_t *cf,
    njt_command_t *cmd, void *conf);

char *njt_stream_lua_ssl_client_hello_by_lua(njt_conf_t *cf, njt_command_t *cmd,
    void *conf);

int njt_stream_lua_ssl_client_hello_handler(njt_ssl_conn_t *ssl_conn,
    int *al, void *arg);

#endif  /* NJT_STREAM_SSL */
#endif /* _NJT_STREAM_LUA_SSL_CLIENT_HELLOBY_H_INCLUDED_ */

/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
