/*
 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */
#include <njt_rpc_result_util.h>
#include "njt_str_util.h"

static u_char * njt_rpc_result_strerror(njt_int_t err_code);
njt_rpc_result_t * njt_rpc_result_create(){
    njt_rpc_result_t * rpc_result = NULL;
    njt_pool_t * pool = NULL;
    pool = njt_create_pool(NJT_MIN_POOL_SIZE, njt_cycle->log);
    if(!pool) {
        return NULL;
    }

    rpc_result = (njt_rpc_result_t *)njt_palloc(pool,sizeof(njt_rpc_result_t));
    if(!rpc_result) {
        return NULL;
    }
    rpc_result->pool = pool;
    rpc_result->data = njt_array_create(pool,4,sizeof(njt_str_t));
    njt_str_null(&rpc_result->conf_path);
    rpc_result->success_count = 0;
    rpc_result->fail_count = 0;
    return rpc_result;
}

void  njt_rpc_result_add_success_count(njt_rpc_result_t * rpc_result){
    if(rpc_result){
        rpc_result->success_count++;
    }
}
void  njt_rpc_result_add_fail_count(njt_rpc_result_t * rpc_result){
    if(rpc_result){
        rpc_result->fail_count++;
    }
}

void  njt_rpc_result_update_code(njt_rpc_result_t * rpc_result){
    if(!rpc_result){
        return;
    }
    if(rpc_result->fail_count==0){
        njt_rpc_result_set_code(rpc_result,NJT_RPC_RSP_SUCCESS);
    } else {
        njt_rpc_result_set_code(rpc_result,rpc_result->success_count>0?NJT_RPC_RSP_PARTIAL_SUCCESS:NJT_RPC_RSP_ERR);
    }
}


void njt_rpc_result_set_code(njt_rpc_result_t * rpc_result,njt_int_t code){
    if(rpc_result){
        rpc_result->code = code;
        if(NJT_IS_PUBLIC_ERROR(code)){
            njt_rpc_result_set_msg(rpc_result,njt_rpc_result_strerror(code));
        }
    }
}
void njt_rpc_result_set_msg(njt_rpc_result_t * rpc_result,u_char * msg){
    size_t len;
    njt_str_t tmp_msg;
    if(!rpc_result) {
        return;
    }

    if(msg){
        len = njt_strlen(msg);
        tmp_msg.len = len;
        tmp_msg.data = msg;
        rpc_result->msg.len = len;
        rpc_result->msg.data=njt_pstrdup(rpc_result->pool,&tmp_msg);
    }
}


void njt_rpc_result_set_msg2(njt_rpc_result_t * rpc_result,njt_str_t * msg){
    if(!rpc_result) {
        return;
    }
    if(msg && msg->len>0){
        rpc_result->msg.len = msg->len;
        rpc_result->msg.data=njt_pstrdup(rpc_result->pool,msg);
    } else {
        rpc_result->msg.len = 0;
    }
}
void njt_rpc_result_set_conf_path(njt_rpc_result_t * rpc_result,njt_str_t *json_str){
    if(!rpc_result || !json_str){
        return;
    }
    if(json_str->len>0){
        njt_str_copy_pool(rpc_result->pool,rpc_result->conf_path,(*json_str),return);
    } else {
        njt_str_null(&rpc_result->conf_path);
    }
    // copy
}

void njt_rpc_result_append_conf_path(njt_rpc_result_t * rpc_result,njt_str_t *json_str){
    njt_str_t old_path;
    if(!rpc_result || !json_str || json_str->len == 0){
        return;
    }
    old_path = rpc_result->conf_path;
    // append
    njt_str_concat(rpc_result->pool,rpc_result->conf_path,old_path,(*json_str),return);
}
void njt_rpc_result_add_error_data(njt_rpc_result_t * rpc_result,njt_str_t * msg) {
    njt_str_t * str_msg;
    if(!rpc_result || !msg || msg->len == 0){
        return;
    }
    rpc_result->fail_count++;
    str_msg = (njt_str_t * )njt_array_push(rpc_result->data);
    if(rpc_result->conf_path.len>0){
        njt_str_concat(rpc_result->pool,(*str_msg),rpc_result->conf_path, (*msg), return)
    }
    else{
        njt_str_copy_pool(rpc_result->pool,(*str_msg),(*msg),return);
    }
}

njt_int_t njt_rpc_result_to_json_str(njt_rpc_result_t * rpc_result,njt_str_t *json_str) {
    njt_json_manager json_manager;
    njt_int_t rc;
    size_t i;
    njt_pool_t *init_pool = NULL;
    njt_str_t *p;
    njt_str_t str_val;
    njt_str_t out_json;
    njt_json_element *element;
    njt_json_element *data_element = NULL;
    if(!rpc_result){
        rc = NJT_ERROR;
        goto out;
    }

    njt_memzero(&json_manager, sizeof(njt_json_manager));
    //创建 pool
    init_pool= rpc_result->pool;
    if (init_pool== NULL)
    {
        rc = NJT_ERROR;
        goto out;
    }
    // 添加code
    element = njt_json_int_element(init_pool, njt_json_fast_key("code"), rpc_result->code);

    rc = njt_struct_top_add(&json_manager, element,NJT_JSON_OBJ,init_pool);
    if(rc != NJT_OK){
        njt_log_error(NJT_LOG_ALERT, njt_cycle->log, 0,
                      "====njt_struct_add code error");
        goto out;
    }
    // 添加msg
    str_val.data = rpc_result->msg.data;
    str_val.len = rpc_result->msg.len;
    element = njt_json_str_element(init_pool, njt_json_fast_key("msg"), &str_val);

    rc = njt_struct_top_add(&json_manager, element,NJT_JSON_OBJ,init_pool);
    if(rc != NJT_OK){
        njt_log_error(NJT_LOG_ALERT, njt_cycle->log, 0,
                      "====njt_struct_add msg error");
        goto out;
    }

    // 添加data
    data_element = njt_json_arr_element(init_pool, njt_json_fast_key("data"));
    p = rpc_result->data->elts;
    for(i = 0; i < rpc_result->data->nelts; ++i) {
        str_val.len = (p + i)->len;
        str_val.data = (p + i)->data;
        element = njt_json_str_element(init_pool, njt_json_null_key, &str_val);

        rc = njt_struct_add(data_element, element, init_pool);
        if(rc != NJT_OK){
            njt_log_error(NJT_LOG_ALERT, njt_cycle->log, 0,
                          "====njt_struct_add data error");
            goto out;

        }
    }

    // add data if not empty
    if(rpc_result->data->nelts > 0){
        rc = njt_struct_top_add(&json_manager, data_element, NJT_JSON_OBJ,init_pool);
        if(rc != NJT_OK){
            njt_log_error(NJT_LOG_ALERT, njt_cycle->log, 0,
                          "====njt_struct_add msg error");
            goto out;
        }
    }

    // 转string
    //struct转json
    njt_str_null(&out_json);
    rc = njt_structure_2_json(&json_manager, &out_json, init_pool);
    if(rc != NJT_OK){
        njt_log_error(NJT_LOG_ALERT, njt_cycle->log, 0,
                      "====njt_structure_2_json error");
        goto out;
    }
    // 重新分配内容， 不使用pool
    json_str->len = out_json.len;
    json_str->data = njt_calloc(out_json.len+1,njt_cycle->log);
    njt_memcpy(json_str->data,out_json.data,out_json.len);

    out:
    //最后一定记得释放掉pool
    return rc;
}
void njt_rpc_result_destroy(njt_rpc_result_t * rpc_result){
    if(rpc_result && rpc_result->pool) {
        njt_destroy_pool(rpc_result->pool);
    }
}


static u_char *njt_rpc_result_strerror(njt_int_t err_code) {
    switch(err_code){
        case NJT_RPC_RSP_SUCCESS:
            return (u_char *)"success.";
        case NJT_RPC_RSP_PARTIAL_SUCCESS:
            return (u_char *)"partial success.";
        case NJT_RPC_RSP_ERR_MEM_ALLOC:
            return (u_char *)"mem alloc error happened.";
        case NJT_RPC_RSP_ERR_JSON:
            return (u_char *)"invalid json format.";
        case NJT_RPC_RSP_ERR:
            return (u_char *)"update dyn conf error.";
        default:
            return (u_char *)"other errors.";
    }
}
