
/*
 * !!! DO NOT EDIT DIRECTLY !!!
 * This file was automatically generated from the following template:
 *
 * src/subsys/njt_subsys_lua_uthread.c.tt2
 */


/*
 * Copyright (C) Yichun Zhang (agentzh)
 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */


#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"


#include "njt_stream_lua_uthread.h"
#include "njt_stream_lua_coroutine.h"
#include "njt_stream_lua_util.h"
#include "njt_stream_lua_probe.h"


#if 1
#undef njt_stream_lua_probe_info
#define njt_stream_lua_probe_info(msg)
#endif


static int njt_stream_lua_uthread_spawn(lua_State *L);
static int njt_stream_lua_uthread_wait(lua_State *L);
static int njt_stream_lua_uthread_kill(lua_State *L);


void
njt_stream_lua_inject_uthread_api(njt_log_t *log, lua_State *L)
{
    /* new thread table */
    lua_createtable(L, 0 /* narr */, 3 /* nrec */);

    lua_pushcfunction(L, njt_stream_lua_uthread_spawn);
    lua_setfield(L, -2, "spawn");

    lua_pushcfunction(L, njt_stream_lua_uthread_wait);
    lua_setfield(L, -2, "wait");

    lua_pushcfunction(L, njt_stream_lua_uthread_kill);
    lua_setfield(L, -2, "kill");

    lua_setfield(L, -2, "thread");
}


static int
njt_stream_lua_uthread_spawn(lua_State *L)
{
    int                              n;
    njt_stream_lua_request_t        *r;
    njt_stream_lua_ctx_t            *ctx;
    njt_stream_lua_co_ctx_t         *coctx = NULL;

    n = lua_gettop(L);

    r = njt_stream_lua_get_req(L);
    if (r == NULL) {
        return luaL_error(L, "no request found");
    }

    ctx = njt_stream_lua_get_module_ctx(r, njt_stream_lua_module);
    if (ctx == NULL) {
        return luaL_error(L, "no request ctx found");
    }

    njt_stream_lua_coroutine_create_helper(L, r, ctx, &coctx);

    /* anchor the newly created coroutine into the Lua registry */

    lua_pushlightuserdata(L, njt_stream_lua_lightudata_mask(
                          coroutines_key));
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, -2);
    coctx->co_ref = luaL_ref(L, -2);
    lua_pop(L, 1);

    if (n > 1) {
        lua_replace(L, 1);
        lua_xmove(L, coctx->co, n - 1);
    }

    coctx->is_uthread = 1;
    ctx->uthreads++;

    coctx->co_status = NJT_STREAM_LUA_CO_RUNNING;
    ctx->co_op = NJT_STREAM_LUA_USER_THREAD_RESUME;

    ctx->cur_co_ctx->thread_spawn_yielded = 1;

    if (njt_stream_lua_post_thread(r, ctx, ctx->cur_co_ctx) != NJT_OK) {
        return luaL_error(L, "no memory");
    }

    coctx->parent_co_ctx = ctx->cur_co_ctx;
    ctx->cur_co_ctx = coctx;

    njt_stream_lua_attach_co_ctx_to_L(coctx->co, coctx);

    njt_stream_lua_probe_user_thread_spawn(r, L, coctx->co);

    dd("yielding with arg %s, top=%d, index-1:%s", luaL_typename(L, -1),
       (int) lua_gettop(L), luaL_typename(L, 1));
    return lua_yield(L, 1);
}


static int
njt_stream_lua_uthread_wait(lua_State *L)
{
    int                          i, nargs, nrets;
    lua_State                   *sub_co;
    njt_stream_lua_request_t    *r;

    njt_stream_lua_ctx_t                *ctx;
    njt_stream_lua_co_ctx_t             *coctx, *sub_coctx;

    r = njt_stream_lua_get_req(L);
    if (r == NULL) {
        return luaL_error(L, "no request found");
    }

    ctx = njt_stream_lua_get_module_ctx(r, njt_stream_lua_module);
    if (ctx == NULL) {
        return luaL_error(L, "no request ctx found");
    }

    njt_stream_lua_check_context(L, ctx, NJT_STREAM_LUA_CONTEXT_YIELDABLE);

    coctx = ctx->cur_co_ctx;

    nargs = lua_gettop(L);

    for (i = 1; i <= nargs; i++) {
        sub_co = lua_tothread(L, i);

        luaL_argcheck(L, sub_co, i, "lua thread expected");

        sub_coctx = njt_stream_lua_get_co_ctx(sub_co, ctx);
        if (sub_coctx == NULL) {
            return luaL_error(L, "no co ctx found");
        }

        if (!sub_coctx->is_uthread) {
            return luaL_error(L, "attempt to wait on a coroutine that is "
                              "not a user thread");
        }

        if (sub_coctx->parent_co_ctx != coctx) {
            return luaL_error(L, "only the parent coroutine can wait on the "
                              "thread");
        }

        switch (sub_coctx->co_status) {
        case NJT_STREAM_LUA_CO_ZOMBIE:

            njt_stream_lua_probe_info("found zombie child");

            nrets = lua_gettop(sub_coctx->co);

            dd("child retval count: %d, %s: %s", (int) nrets,
               luaL_typename(sub_coctx->co, -1),
               lua_tostring(sub_coctx->co, -1));

            if (nrets) {
                lua_xmove(sub_coctx->co, L, nrets);
            }

#if 1
            njt_stream_lua_del_thread(r, L, ctx, sub_coctx);
            ctx->uthreads--;
#endif

            return nrets;

        case NJT_STREAM_LUA_CO_DEAD:
            dd("uthread already waited: %p (parent %p)", sub_coctx,
               coctx);

            if (i < nargs) {
                /* just ignore it if it is not the last one */
                continue;
            }

            /* being the last one */
            lua_pushnil(L);
            lua_pushliteral(L, "already waited or killed");
            return 2;

        default:
            dd("uthread %p still alive, status: %d, parent %p", sub_coctx,
               sub_coctx->co_status, coctx);
            break;
        }

        njt_stream_lua_probe_user_thread_wait(L, sub_coctx->co);
        sub_coctx->waited_by_parent = 1;
    }

    return lua_yield(L, 0);
}


static int
njt_stream_lua_uthread_kill(lua_State *L)
{
    lua_State                   *sub_co;
    njt_stream_lua_request_t    *r;

    njt_stream_lua_ctx_t                *ctx;
    njt_stream_lua_co_ctx_t             *coctx, *sub_coctx;

    r = njt_stream_lua_get_req(L);
    if (r == NULL) {
        return luaL_error(L, "no request found");
    }

    ctx = njt_stream_lua_get_module_ctx(r, njt_stream_lua_module);
    if (ctx == NULL) {
        return luaL_error(L, "no request ctx found");
    }

    njt_stream_lua_check_context(L, ctx, NJT_STREAM_LUA_CONTEXT_CONTENT

                               | NJT_STREAM_LUA_CONTEXT_PREREAD
                               | NJT_STREAM_LUA_CONTEXT_SSL_CLIENT_HELLO
                               | NJT_STREAM_LUA_CONTEXT_SSL_CERT
                               | NJT_STREAM_LUA_CONTEXT_TIMER);

    coctx = ctx->cur_co_ctx;

    sub_co = lua_tothread(L, 1);
    luaL_argcheck(L, sub_co, 1, "lua thread expected");

    sub_coctx = njt_stream_lua_get_co_ctx(sub_co, ctx);

    if (sub_coctx == NULL) {
        return luaL_error(L, "no co ctx found");
    }

    if (!sub_coctx->is_uthread) {
        lua_pushnil(L);
        lua_pushliteral(L, "not user thread");
        return 2;
    }

    if (sub_coctx->parent_co_ctx != coctx) {
        lua_pushnil(L);
        lua_pushliteral(L, "killer not parent");
        return 2;
    }


    switch (sub_coctx->co_status) {
    case NJT_STREAM_LUA_CO_ZOMBIE:
        njt_stream_lua_del_thread(r, L, ctx, sub_coctx);
        ctx->uthreads--;

        lua_pushnil(L);
        lua_pushliteral(L, "already terminated");
        return 2;

    case NJT_STREAM_LUA_CO_DEAD:
        lua_pushnil(L);
        lua_pushliteral(L, "already waited or killed");
        return 2;

    default:
        njt_stream_lua_cleanup_pending_operation(sub_coctx);
        njt_stream_lua_del_thread(r, L, ctx, sub_coctx);
        ctx->uthreads--;

        lua_pushinteger(L, 1);
        return 1;
    }

    /* not reachable */
}

/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
