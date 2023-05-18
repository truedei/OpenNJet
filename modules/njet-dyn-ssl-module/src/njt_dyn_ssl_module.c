/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */
#include <njt_config.h>
#include <njt_core.h>
#include <njt_http.h>
#include <njet.h>
#include "njt_http_kv_module.h"
#include "njt_json_util.h"
#include "njt_http_util.h"
#include "njt_str_util.h"

typedef struct  {
    njt_str_t cert_type;              //ntls  or regular
    njt_str_t certificate;
    njt_str_t certificate_enc;        //if type is ntls, should not empty 
    njt_str_t certificate_key;
    njt_str_t certificate_key_enc;    //if type is ntls, should not empty

}njt_http_dyn_ssl_cert_group_t;

typedef struct {
    njt_array_t listens;
    njt_array_t server_names;
    njt_array_t certificates;
}njt_http_dyn_ssl_api_srv_t;
typedef struct {
    njt_array_t servers;
}njt_http_dyn_ssl_api_main_t;


static njt_json_define_t njt_http_dyn_ssl_cert_group_json_dt[] ={
        {
                njt_string("cert_type"),
                offsetof(njt_http_dyn_ssl_cert_group_t, cert_type),
                0,
                NJT_JSON_STR,
                0,
                NULL,
                NULL,
        },
        {
                njt_string("certificate"),
                offsetof(njt_http_dyn_ssl_cert_group_t, certificate),
                0,
                NJT_JSON_STR,
                0,
                NULL,
                NULL,
        },
        {
                njt_string("certificateEnc"),
                offsetof(njt_http_dyn_ssl_cert_group_t, certificate_enc),
                0,
                NJT_JSON_STR,
                0,
                NULL,
                NULL,
        },
        {
                njt_string("certificateKey"),
                offsetof(njt_http_dyn_ssl_cert_group_t, certificate_key),
                0,
                NJT_JSON_STR,
                0,
                NULL,
                NULL,
        },
        {
                njt_string("certificateKeyEnc"),
                offsetof(njt_http_dyn_ssl_cert_group_t, certificate_key_enc),
                0,
                NJT_JSON_STR,
                0,
                NULL,
                NULL,
        },
        njt_json_define_null,
};

static njt_json_define_t njt_http_dyn_ssl_api_srv_json_dt[] ={
        {
                njt_string("listens"),
                offsetof(njt_http_dyn_ssl_api_srv_t, listens),
                sizeof(njt_str_t),
                NJT_JSON_ARRAY,
                NJT_JSON_STR,
                NULL,
                NULL,
        },
        {
                njt_string("serverNames"),
                offsetof(njt_http_dyn_ssl_api_srv_t, server_names),
                sizeof(njt_str_t),
                NJT_JSON_ARRAY,
                NJT_JSON_STR,
                NULL,
                NULL,
        },
        {
                njt_string("certificates"),
                offsetof(njt_http_dyn_ssl_api_srv_t, certificates),
                sizeof(njt_http_dyn_ssl_cert_group_t),
                NJT_JSON_ARRAY,
                NJT_JSON_OBJ,
                njt_http_dyn_ssl_cert_group_json_dt,
                NULL,
        },
        njt_json_define_null,
};

static njt_json_define_t njt_http_dyn_ssl_api_main_json_dt[] ={
        {
                njt_string("servers"),
                offsetof(njt_http_dyn_ssl_api_main_t, servers),
                sizeof(njt_http_dyn_ssl_api_srv_t),
                NJT_JSON_ARRAY,
                NJT_JSON_OBJ,
                njt_http_dyn_ssl_api_srv_json_dt,
                NULL,
        },
        njt_json_define_null,
};



static njt_int_t njt_http_update_server_ssl(njt_pool_t *pool,njt_http_dyn_ssl_api_main_t *api_data){
    njt_cycle_t                     *cycle;
    njt_http_core_srv_conf_t        *cscf;
    njt_http_ssl_srv_conf_t         *hsscf;
    njt_http_dyn_ssl_api_srv_t      *daas;
    njt_http_dyn_ssl_cert_group_t   *cert;
    njt_str_t                        cert_str;
    njt_str_t                        key_str;
    njt_str_t                       *tmp_str;
    njt_uint_t                       i,j;
    njt_conf_t                       cf;
    u_char                          *data;
    njt_str_t                       *p_port;
    njt_str_t                       *p_sname;
    

    njt_memzero(&cf,sizeof(njt_conf_t));
    cf.pool = pool;
    cf.log = njt_cycle->log;
    cf.cycle = (njt_cycle_t *)njt_cycle;

    cycle = (njt_cycle_t*)njt_cycle;

    daas = api_data->servers.elts;
    for(i = 0; i < api_data->servers.nelts; ++i){
        p_port = (njt_str_t*)daas[i].listens.elts;
        p_sname = (njt_str_t*)daas[i].server_names.elts;

        if(p_port == NULL || p_sname == NULL){
            njt_log_error(NJT_LOG_INFO, pool->log, 0, "listen or server_name is NULL, just continue");            
            continue;
        }

        cscf = njt_http_get_srv_by_port(cycle,(njt_str_t*)daas[i].listens.elts,(njt_str_t*)daas[i].server_names.elts);
        if(cscf == NULL){
            njt_log_error(NJT_LOG_INFO, pool->log, 0, "dyn ssl, can`t find server by listen:%V server_name:%V ",
                          (njt_str_t*)daas[i].listens.elts,(njt_str_t*)daas[i].server_names.elts);

            continue;
        }
        hsscf = njt_http_get_module_srv_conf(cscf->ctx,njt_http_ssl_module);
        if(hsscf == NULL){
            continue;
        }
        if(hsscf->ssl.ctx == NULL){
            continue;
        }
        cert =  daas[i].certificates.elts;
        for(j = 0 ; j < daas[i].certificates.nelts; ++j ){
            //todo 此处内存泄露
            hsscf->ssl.log = njt_cycle->log;
            //check param is ntls or regular cert, if empty, default regular
            if( cert[j].cert_type.len < 1 ||
                (cert[j].cert_type.len == 7 && njt_strncmp(cert[j].cert_type.data, "regular", 7) == 0)){
                if (njt_ssl_certificate(&cf, &hsscf->ssl, &cert[j].certificate, &cert[j].certificate_key, NULL)
                    != NJT_OK)
                {
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,"njt_ssl_certificate error");
                    continue;
                }

                tmp_str =njt_array_push(hsscf->certificates);
                if(tmp_str != NULL){
                    njt_str_copy_pool(hsscf->certificates->pool,(*tmp_str),cert[j].certificate, continue;);
                }
                tmp_str =njt_array_push(hsscf->certificate_keys);
                if(tmp_str != NULL){
                    njt_str_copy_pool(hsscf->certificate_keys->pool,(*tmp_str),cert[j].certificate_key, continue;);
                }
            }else if(cert[j].cert_type.len == 4 && njt_strncmp(cert[j].cert_type.data, "ntls", 4) == 0){
#if (NJT_HAVE_NTLS)
                //check valid
                if(cert[j].certificate.len < 1 || cert[j].certificate_enc.len < 1
                   || cert[j].certificate_key.len < 1 || cert[j].certificate_key_enc.len < 1){
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                        "dyn ssl, njt_ssl_certificate params size should > 0");
                    
                    continue;
                }

                //update sign
                tmp_str = &cert_str;                
                tmp_str->len = sizeof("sign:") - 1 + cert[j].certificate.len;
                tmp_str->data = njt_pcalloc(hsscf->certificates->pool, tmp_str->len + 1);
                if (tmp_str->data == NULL) {
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                        "dyn ssl, njt_ssl_certificate sign cert calloc error");
                    return NJT_ERROR;
                }
                data = njt_cpymem(tmp_str->data, "sign:", sizeof("sign:") - 1);
                njt_memcpy(data, cert[j].certificate.data, cert[j].certificate.len);

                tmp_str = &key_str;                
                tmp_str->len = sizeof("sign:") - 1 + cert[j].certificate_key.len;
                tmp_str->data = njt_pcalloc(hsscf->certificate_keys->pool, tmp_str->len + 1);
                if (tmp_str->data == NULL) {
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                        "dyn ssl, njt_ssl_certificate key sign cert calloc error");
                    return NJT_ERROR;
                }
                data = njt_cpymem(tmp_str->data, "sign:", sizeof("sign:") - 1);
                njt_memcpy(data, cert[j].certificate_key.data, cert[j].certificate_key.len);

                if (njt_ssl_certificate(&cf, &hsscf->ssl, &cert_str, &key_str, NULL)
                    != NJT_OK)
                {
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,"dyn ssl, sign certificate error");
                    continue;
                }

                tmp_str =njt_array_push(hsscf->certificates);
                if(tmp_str != NULL){
                    njt_str_copy_pool(hsscf->certificates->pool,(*tmp_str),cert_str, continue;);
                }else{
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                        "dyn ssl, njt_ssl_certificate cert sign arrary push error");
    
                    return NJT_ERROR;
                }

                tmp_str =njt_array_push(hsscf->certificate_keys);
                if(tmp_str != NULL){
                    njt_str_copy_pool(hsscf->certificate_keys->pool,(*tmp_str),key_str, continue;);
                }else{
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                        "dyn ssl, njt_ssl_certificate key sign arrary push error");
    
                    return NJT_ERROR;
                }

                //update enc
                tmp_str = &cert_str;                
                tmp_str->len = sizeof("enc:") - 1 + cert[j].certificate_enc.len;
                tmp_str->data = njt_pcalloc(hsscf->certificates->pool, tmp_str->len + 1);
                if (tmp_str->data == NULL) {
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                        "dyn ssl, njt_ssl_certificate enc cert calloc error");
                    return NJT_ERROR;
                }
                data = njt_cpymem(tmp_str->data, "enc:", sizeof("enc:") - 1);
                njt_memcpy(data, cert[j].certificate_enc.data, cert[j].certificate_enc.len);

                tmp_str = &key_str;                
                tmp_str->len = sizeof("enc:") - 1 + cert[j].certificate_key_enc.len;
                tmp_str->data = njt_pcalloc(hsscf->certificate_keys->pool, tmp_str->len + 1);
                if (tmp_str->data == NULL) {
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                        "dyn ssl, njt_ssl_certificate key sign cert calloc error");
                    return NJT_ERROR;
                }
                data = njt_cpymem(tmp_str->data, "enc:", sizeof("enc:") - 1);
                njt_memcpy(data, cert[j].certificate_key_enc.data, cert[j].certificate_key_enc.len);

                if (njt_ssl_certificate(&cf, &hsscf->ssl, &cert_str, &key_str, NULL)
                    != NJT_OK)
                {
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,"dyn ssl, enc certificate error");
                    continue;
                }

                tmp_str =njt_array_push(hsscf->certificates);
                if(tmp_str != NULL){
                    njt_str_copy_pool(hsscf->certificates->pool,(*tmp_str),cert_str, continue;);
                }else{
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                        "dyn ssl, njt_ssl_certificate cert enc arrary push error");
    
                    return NJT_ERROR;
                }

                tmp_str =njt_array_push(hsscf->certificate_keys);
                if(tmp_str != NULL){
                    njt_str_copy_pool(hsscf->certificate_keys->pool,(*tmp_str),key_str, continue;);
                }else{
                    njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                        "dyn ssl, njt_ssl_certificate key enc arrary push error");
    
                    return NJT_ERROR;
                }

#else

    njt_conf_log_error(NJT_LOG_EMERG, cf, 0,
                       "dyn ssl, NTLS support is not enabled, dual certs not supported");

    return NJT_CONF_ERROR;

#endif
            }else{
                njt_log_error(NJT_LOG_EMERG, pool->log, 0,
                   "dyn ssl, njt_ssl_certificate cert_type not support, should ntls or regular");
            }
        }
    }
    return NJT_OK;
}


static int  njt_http_ssl_change_handler(njt_str_t *key, njt_str_t *value, void *data){
    njt_int_t rc;
    njt_http_dyn_ssl_api_main_t *api_data = NULL;
    njt_pool_t *pool = NULL;
    if(value->len < 2 ){
        return NJT_OK;
    }
    pool = njt_create_pool(njt_pagesize,njt_cycle->log);
    if(pool == NULL){
        njt_log_error(NJT_LOG_EMERG, pool->log, 0, "njt_http_ssl_change_handler create pool error");
        return NJT_OK;
    }
    api_data = njt_pcalloc(pool,sizeof (njt_http_dyn_ssl_api_main_t));
    if(api_data == NULL){
        njt_log_debug1(NJT_LOG_DEBUG_HTTP, pool->log, 0,
                       "could not alloc buffer in function %s", __func__);
        goto end;
    }

    rc =njt_json_parse_data(pool,value,njt_http_dyn_ssl_api_main_json_dt,api_data);
    if(rc == NJT_OK ){
        njt_http_update_server_ssl(pool,api_data);
    }

    end:
    if(pool != NULL){
        njt_destroy_pool(pool);
    }
    return NJT_OK;
}
njt_str_t njt_http_dyn_ssl_srv_err_msg=njt_string("{\"code\":500,\"msg\":\"server error\"}");

static njt_str_t njt_http_dyn_ssl_dump_conf(njt_cycle_t *cycle,njt_pool_t *pool){
    njt_http_ssl_srv_conf_t        *hsscf;
    njt_http_core_main_conf_t      *hcmcf;
    njt_http_core_srv_conf_t      **cscfp;
    njt_uint_t                      i,j;
    njt_array_t                    *array;
    njt_str_t                       json,*tmp_str;
    njt_str_t                       tmp_value_str;
    njt_http_server_name_t         *server_name;
    njt_json_manager                json_manager;
    njt_json_element               *srvs,*srv,*subs,*sub,*item;
    njt_str_t                      *key,*cert;
    njt_http_complex_value_t       *var_key,*var_cert;
    njt_uint_t                      type;
    njt_str_t                       trip_str;

    hcmcf = njt_http_cycle_get_module_main_conf(cycle,njt_http_core_module);

    njt_memzero(&json_manager, sizeof(njt_json_manager));

    srvs =  njt_json_arr_element(pool,njt_json_fast_key("servers"));
    if(srvs == NULL ){
        goto err;
    }
    cscfp = hcmcf->servers.elts;
    for( i = 0; i < hcmcf->servers.nelts; i++){
        array = njt_array_create(pool,4, sizeof(njt_str_t));
        njt_http_get_listens_by_server(array,cscfp[i]);

        srv =  njt_json_obj_element(pool,njt_json_null_key);
        if(srv == NULL ){
            goto err;
        }

        subs =  njt_json_arr_element(pool,njt_json_fast_key("listens"));
        if(subs == NULL ){
            goto err;
        }

        tmp_str = array->elts;
        for(j = 0 ; j < array->nelts ; ++j ){
            sub =  njt_json_str_element(pool,njt_json_null_key,&tmp_str[j]);
            if(sub == NULL ){
                goto err;
            }
            njt_struct_add(subs,sub,pool);
        }
        njt_struct_add(srv,subs,pool);
        subs =  njt_json_arr_element(pool,njt_json_fast_key("serverNames"));
        if(subs == NULL ){
            goto err;
        }
        server_name = cscfp[i]->server_names.elts;
        for(j = 0 ; j < cscfp[i]->server_names.nelts ; ++j ){
            sub =  njt_json_str_element(pool,njt_json_null_key,&server_name[j].name);
            if(sub == NULL ){
                goto err;
            }
            njt_struct_add(subs,sub,pool);
        }
        njt_struct_add(srv,subs,pool);
        hsscf = njt_http_get_module_srv_conf(cscfp[i]->ctx,njt_http_ssl_module);
        if(hsscf == NULL){
            goto next;
        }
        if(hsscf->certificate_values == NULL){
            if(hsscf->certificates == NULL){
                goto next;
            }
            subs =  njt_json_arr_element(pool,njt_json_fast_key("certificates"));
            if(subs == NULL ){
                goto err;
            }
            cert = hsscf->certificates->elts;
            key = hsscf->certificate_keys->elts;
            for(j = 0 ; j < hsscf->certificates->nelts ; ++j ){
                sub =  njt_json_obj_element(pool,njt_json_null_key);
                if(sub == NULL ){
                    goto err;
                }
#if (NJT_HAVE_NTLS)
                type = njt_ssl_ntls_type(&cert[j]);
                // key_type = njt_ssl_ntls_type(&key[j]);
                // if(type != key_type){
                //     njt_log_error(NJT_LOG_EMERG, njt_cycle->log, 0, "dyn ssl, error: cert type and key type not equal");
                //     continue;
                // }

                if (type == NJT_SSL_NTLS_CERT_SIGN) {
                    njt_str_set(&tmp_value_str, "ntls");
                    item = njt_json_str_element(pool,njt_json_fast_key("cert_type"), &tmp_value_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    trip_str = cert[j];
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificate"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);
                                    
                    trip_str = key[j];
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateKey"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    //next must be have and must be enc
                    if(j == (hsscf->certificates->nelts - 1)){
                        njt_struct_add(subs,sub,pool);
                        //after sign must has enc
                        njt_log_error(NJT_LOG_EMERG, njt_cycle->log, 0, "dyn ssl, error: end sign cerst loss enc");
                        continue;
                    }

                    //j+1 type must be enc
                    type = njt_ssl_ntls_type(&cert[j+1]);
                    // key_type = njt_ssl_ntls_type(&key[j+1]);
                    // if(type != NJT_SSL_NTLS_CERT_ENC || key_type != NJT_SSL_NTLS_CERT_ENC){
                    if(type != NJT_SSL_NTLS_CERT_ENC){
                        njt_struct_add(subs,sub,pool);
                        // njt_log_error(NJT_LOG_EMERG, njt_cycle->log, 0, "dyn ssl, error: after sign must has enc");
                        continue;
                    }

                    trip_str = cert[j+1];
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateEnc"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    trip_str = key[j+1];
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateKeyEnc"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    njt_struct_add(subs,sub,pool);
                    j++;
                }else if (type == NJT_SSL_NTLS_CERT_ENC){
                    njt_str_set(&tmp_value_str, "ntls");
                    item = njt_json_str_element(pool,njt_json_fast_key("cert_type"), &tmp_value_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    trip_str = cert[j];
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateEnc"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    trip_str = key[j];
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateKeyEnc"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    njt_struct_add(subs,sub,pool);
                    //should not get enc before sign
                    // njt_log_error(NJT_LOG_EMERG, njt_cycle->log, 0, "dyn ssl, error: should not get enc before sign");
                    continue;
                }else{
                    njt_str_set(&tmp_value_str, "regular");
                    item = njt_json_str_element(pool,njt_json_fast_key("cert_type"), &tmp_value_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificate"),&cert[j]);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateKey"),&key[j]);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    njt_struct_add(subs,sub,pool); 
                }
#else
                njt_str_set(&tmp_value_str, "regular");
                item = njt_json_str_element(pool,njt_json_fast_key("cert_type"), &njt_str_set);
                if(item == NULL ){
                    goto err;
                }
                njt_struct_add(sub,item,pool);
                item = njt_json_str_element(pool,njt_json_fast_key("certificate"),&cert[j]);
                if(item == NULL ){
                    goto err;
                }
                njt_struct_add(sub,item,pool);
                item = njt_json_str_element(pool,njt_json_fast_key("certificateKey"),&key[j]);
                if(item == NULL ){
                    goto err;
                }
                njt_struct_add(sub,item,pool);
                njt_struct_add(subs,sub,pool);
#endif
            }
            njt_struct_add(srv,subs,pool);
        }else{
            subs =  njt_json_arr_element(pool,njt_json_fast_key("certificates"));
            if(subs == NULL ){
                goto err;
            }
            var_cert = hsscf->certificate_values->elts;
            var_key = hsscf->certificate_key_values->elts;
            for(j = 0 ; j < hsscf->certificate_values->nelts ; ++j ){
                sub =  njt_json_obj_element(pool,njt_json_null_key);
                if(sub == NULL ){
                    goto err;
                }
#if (NJT_HAVE_NTLS)
                type = njt_ssl_ntls_type(&var_cert[j].value);
                // key_type = njt_ssl_ntls_type(&var_key[j].value);
                // if(type != key_type){
                //     njt_log_error(NJT_LOG_EMERG, njt_cycle->log, 0, "dyn ssl, error: cert type and key type not equal");
                //     continue;
                // }

                if (type == NJT_SSL_NTLS_CERT_SIGN) {
                    njt_str_set(&tmp_value_str, "ntls");
                    item = njt_json_str_element(pool,njt_json_fast_key("cert_type"), &tmp_value_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    trip_str = var_cert[j].value;
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificate"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    trip_str = var_key[j].value;
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateKey"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    //next must be have and must be enc
                    if(j == (hsscf->certificates->nelts - 1)){
                        njt_struct_add(subs,sub,pool);
                        //after sign must has enc
                        njt_log_error(NJT_LOG_EMERG, njt_cycle->log, 0, "dyn ssl, error: end sign cerst loss enc");
                        continue;
                    }

                    //j+1 type must be enc
                    type = njt_ssl_ntls_type(&var_cert[j+1].value);
                    // key_type = njt_ssl_ntls_type(&var_key[j+1].value);
                    // if(type != NJT_SSL_NTLS_CERT_ENC || key_type != NJT_SSL_NTLS_CERT_ENC){
                    if(type != NJT_SSL_NTLS_CERT_ENC){
                        njt_struct_add(subs,sub,pool);
                        // njt_log_error(NJT_LOG_EMERG, njt_cycle->log, 0, "dyn ssl, error: after sign must has enc");
                        continue;
                    }

                    trip_str = var_cert[j+1].value;
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateEnc"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    trip_str = var_key[j+1].value;
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateKeyEnc"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    njt_struct_add(subs,sub,pool);
                    j++;
                }else if (type == NJT_SSL_NTLS_CERT_ENC){
                    njt_str_set(&tmp_value_str, "ntls");
                    item = njt_json_str_element(pool,njt_json_fast_key("cert_type"), &tmp_value_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    trip_str = var_cert[j].value;
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateEnc"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    trip_str = var_key[j].value;
                    njt_ssl_ntls_prefix_strip(&trip_str);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateKeyEnc"),&trip_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    njt_struct_add(subs,sub,pool);
                    //should not get enc before sign
                    // njt_log_error(NJT_LOG_EMERG, njt_cycle->log, 0, "dyn ssl, error: should not get enc before sign");
                }else{
                    njt_str_set(&tmp_value_str, "regular");
                    item = njt_json_str_element(pool,njt_json_fast_key("cert_type"), &tmp_value_str);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificate"),&var_cert[j].value);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);
                    item = njt_json_str_element(pool,njt_json_fast_key("certificateKey"),&var_key[j].value);
                    if(item == NULL ){
                        goto err;
                    }
                    njt_struct_add(sub,item,pool);

                    njt_struct_add(subs,sub,pool); 
                }
#else
                njt_str_set(&tmp_value_str, "regular");
                item = njt_json_str_element(pool,njt_json_fast_key("cert_type"), &tmp_value_str);
                if(item == NULL ){
                    goto err;
                }
                njt_struct_add(sub,item,pool);
                item = njt_json_str_element(pool,njt_json_fast_key("certificates"),&var_cert[j].value);
                if(item == NULL ){
                    goto err;
                }
                njt_struct_add(sub,item,pool);
                item = njt_json_str_element(pool,njt_json_fast_key("certificateKey"),&var_key[j].value);
                if(item == NULL ){
                    goto err;
                }
                njt_struct_add(sub,item,pool);
                njt_struct_add(subs,sub,pool);
#endif
            }
            njt_struct_add(srv,subs,pool);
        }

        next:
        njt_struct_add(srvs,srv,pool);
    }

    njt_struct_top_add(&json_manager,srvs,NJT_JSON_OBJ,pool);
    njt_memzero(&json, sizeof(njt_str_t));
    njt_structure_2_json(&json_manager, &json, pool);

    return json;

    err:
    return njt_http_dyn_ssl_srv_err_msg;
}

static u_char* njt_http_dyn_ssl_rpc_handler(njt_str_t *topic, njt_str_t *request, int* len, void *data){
    njt_cycle_t *cycle;
    njt_str_t msg;
    u_char *buf;

    buf = NULL;
    cycle = (njt_cycle_t*) njt_cycle;
    *len = 0 ;
    njt_pool_t *pool = NULL;
    pool = njt_create_pool(njt_pagesize,njt_cycle->log);
    if(pool == NULL){
        njt_log_error(NJT_LOG_EMERG, pool->log, 0, "njt_agent_dynlog_change_handler create pool error");
        goto end;
    }
    msg = njt_http_dyn_ssl_dump_conf(cycle,pool);
    buf = njt_calloc(msg.len,cycle->log);
    if(buf == NULL){
        goto end;
    }
    njt_log_error(NJT_LOG_INFO, pool->log, 0, "send json : %V",&msg);
    njt_memcpy(buf,msg.data,msg.len);
    *len = msg.len;

    end:
    if(pool != NULL){
        njt_destroy_pool(pool);
    }

    return buf;
}

static njt_int_t njt_http_dyn_ssl_init_process(njt_cycle_t* cycle){
    njt_str_t  rpc_key = njt_string("http_ssl");
    njt_reg_kv_change_handler(&rpc_key, njt_http_ssl_change_handler,njt_http_dyn_ssl_rpc_handler, NULL);
    return NJT_OK;
}





static njt_http_module_t njt_dyn_ssl_module_ctx = {
        NULL,                                   /* preconfiguration */
        NULL,                                   /* postconfiguration */

        NULL,                                   /* create main configuration */
        NULL,                                  /* init main configuration */

        NULL,                                  /* create server configuration */
        NULL,                                  /* merge server configuration */

        NULL,                                   /* create location configuration */
        NULL                                    /* merge location configuration */
};

njt_module_t njt_dyn_ssl_module = {
        NJT_MODULE_V1,
        &njt_dyn_ssl_module_ctx,                /* module context */
        NULL,                                   /* module directives */
        NJT_HTTP_MODULE,                        /* module type */
        NULL,                                   /* init master */
        NULL,                                   /* init module */
        njt_http_dyn_ssl_init_process,          /* init process */
        NULL,                                   /* init thread */
        NULL,                                   /* exit thread */
        NULL,                                   /* exit process */
        NULL,                                   /* exit master */
        NJT_MODULE_V1_PADDING
};

