/*
 * ModSecurity connector for njet, http://www.modsecurity.org/
 * Copyright (c) 2015 Trustwave Holdings, Inc. (http://www.trustwave.com/)
 * Copyright (C) 2021-2023 TMLake(Beijing) Technology Co., Ltd.
 *
 * You may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * If any of the files related to licensing are missing or if you have any
 * other questions related to licensing please contact Trustwave Holdings, Inc.
 * directly using the email address security@modsecurity.org.
 *
 */

#ifndef MODSECURITY_DDEBUG
#define MODSECURITY_DDEBUG 0
#endif
#include "ddebug.h"

#include "njt_http_modsecurity_common.h"

static njt_http_output_body_filter_pt njt_http_next_body_filter;

/* XXX: check behaviour on few body filters installed */
njt_int_t
njt_http_modsecurity_body_filter_init(void)
{
    njt_http_next_body_filter = njt_http_top_body_filter;
    njt_http_top_body_filter = njt_http_modsecurity_body_filter;

    return NJT_OK;
}

njt_int_t
njt_http_modsecurity_body_filter(njt_http_request_t *r, njt_chain_t *in)
{
    njt_chain_t *chain = in;
    njt_http_modsecurity_ctx_t *ctx = NULL;
#if defined(MODSECURITY_SANITY_CHECKS) && (MODSECURITY_SANITY_CHECKS)
    njt_http_modsecurity_conf_t *mcf;
    njt_list_part_t *part = &r->headers_out.headers.part;
    njt_table_elt_t *data = part->elts;
    njt_uint_t i = 0;
#endif

    if (in == NULL) {
        return njt_http_next_body_filter(r, in);
    }

    ctx = njt_http_get_module_ctx(r, njt_http_modsecurity_module);

    dd("body filter, recovering ctx: %p", ctx);

    if (ctx == NULL) {
        return njt_http_next_body_filter(r, in);
    }

    if (ctx->intervention_triggered) {
        return njt_http_next_body_filter(r, in);
    }

#if defined(MODSECURITY_SANITY_CHECKS) && (MODSECURITY_SANITY_CHECKS)
    mcf = njt_http_get_module_loc_conf(r, njt_http_modsecurity_module);
    if (mcf != NULL && mcf->sanity_checks_enabled != NJT_CONF_UNSET)
    {
#if 0
        dd("dumping stored ctx headers");
        for (i = 0; i < ctx->sanity_headers_out->nelts; i++)
        {
            njt_http_modsecurity_header_t *vals = ctx->sanity_headers_out->elts;
            njt_str_t *s2 = &vals[i].name, *s3 = &vals[i].value;
            dd(" dump[%d]: name = '%.*s', value = '%.*s'", (int)i,
                (int)s2->len, (char*)s2->data,
                (int)s3->len, (char*)s3->data);
        }
#endif
        /*
         * Identify if there is a header that was not inspected by ModSecurity.
         */
        int worth_to_fail = 0;

        for (i = 0; ; i++)
        {
            int found = 0;
            njt_uint_t j = 0;
            njt_table_elt_t *s1;
            njt_http_modsecurity_header_t *vals;

            if (i >= part->nelts)
            {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                data = part->elts;
                i = 0;
            }

            vals = ctx->sanity_headers_out->elts;
            s1 = &data[i];

            /*
             * Headers that were inspected by ModSecurity.
             */
            while (j < ctx->sanity_headers_out->nelts)
            {
                njt_str_t *s2 = &vals[j].name;
                njt_str_t *s3 = &vals[j].value;

                if (s1->key.len == s2->len && njt_strncmp(s1->key.data, s2->data, s1->key.len) == 0)
                {
                    if (s1->value.len == s3->len && njt_strncmp(s1->value.data, s3->data, s1->value.len) == 0)
                    {
                        found = 1;
                        break;
                    }
                }
                j++;
            }
            if (!found) {
                dd("header: `%.*s' with value: `%.*s' was not inspected by ModSecurity",
                    (int) s1->key.len,
                    (const char *) s1->key.data,
                    (int) s1->value.len,
                    (const char *) s1->value.data);
                worth_to_fail++;
            }
        }

        if (worth_to_fail)
        {
            dd("%d header(s) were not inspected by ModSecurity, so exiting", worth_to_fail);
            return njt_http_filter_finalize_request(r,
                &njt_http_modsecurity_module, NJT_HTTP_INTERNAL_SERVER_ERROR);
        }
    }
#endif

    int is_request_processed = 0;
    for (; chain != NULL; chain = chain->next)
    {
        u_char *data = chain->buf->pos;
        int ret;

        msc_append_response_body(ctx->modsec_transaction, data, chain->buf->last - data);
        ret = njt_http_modsecurity_process_intervention(ctx->modsec_transaction, r, 0);
        if (ret > 0) {
            return njt_http_filter_finalize_request(r,
                &njt_http_modsecurity_module, ret);
        }

/* XXX: chain->buf->last_buf || chain->buf->last_in_chain */
        is_request_processed = chain->buf->last_buf;

        if (is_request_processed) {
            njt_pool_t *old_pool;

            old_pool = njt_http_modsecurity_pcre_malloc_init(r->pool);
            msc_process_response_body(ctx->modsec_transaction);
            njt_http_modsecurity_pcre_malloc_done(old_pool);

/* XXX: I don't get how body from modsec being transferred to njet's buffer.  If so - after adjusting of njet's
   XXX: body we can proceed to adjust body size (content-length).  see xslt_body_filter() for example */
            ret = njt_http_modsecurity_process_intervention(ctx->modsec_transaction, r, 0);
            if (ret > 0) {
                return ret;
            }
            else if (ret < 0) {
                return njt_http_filter_finalize_request(r,
                    &njt_http_modsecurity_module, NJT_HTTP_INTERNAL_SERVER_ERROR);

            }
        }
    }
    if (!is_request_processed)
    {
        dd("buffer was not fully loaded! ctx: %p", ctx);
    }

/* XXX: xflt_filter() -- return NJT_OK here */
    return njt_http_next_body_filter(r, in);
}
