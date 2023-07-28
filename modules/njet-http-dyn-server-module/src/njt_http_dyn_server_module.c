/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */
#include <njt_config.h>
#include <njt_core.h>
#include <njt_http.h>
#include <njt_stream.h>
#include <njt_json_api.h>
#include <math.h>
#include <njt_http_kv_module.h>
#include <njt_http_util.h>
#include <njt_http_sendmsg_module.h>
#include <njt_http_dyn_server_module.h>
#include <njt_rpc_result_util.h>
extern njt_uint_t njt_worker;
extern njt_module_t  njt_http_rewrite_module;
extern  njt_int_t
njt_http_optimize_servers(njt_conf_t *cf, njt_http_core_main_conf_t *cmcf,
                          njt_array_t *ports); 

njt_str_t njt_del_headtail_space(njt_str_t src);

static njt_int_t
njt_http_dyn_server_init_worker(njt_cycle_t *cycle);



static void njt_http_dyn_server_write_data(njt_http_dyn_server_info_t *server_info);
typedef struct njt_http_dyn_server_ctx_s {
} njt_http_dyn_server_ctx_t, njt_stream_http_dyn_server_ctx_t;


static njt_http_module_t njt_http_dyn_server_module_ctx = {
        NULL,                              /* preconfiguration */
        NULL,                              /* postconfiguration */

        NULL,                              /* create main configuration */
        NULL,                              /* init main configuration */

        NULL,                              /* create server configuration */
        NULL,                              /* merge server configuration */

        NULL, /* create server configuration */
        NULL   /* merge server configuration */
};

njt_module_t njt_http_dyn_server_module = {
        NJT_MODULE_V1,
        &njt_http_dyn_server_module_ctx, /* module context */
        NULL,                               /* module directives */
        NJT_HTTP_MODULE,                    /* module type */
        NULL,                               /* init master */
        NULL,                               /* init module */
        njt_http_dyn_server_init_worker, /* init process */
        NULL,                               /* init thread */
        NULL,                               /* exit thread */
        NULL,                               /* exit process */
        NULL,                               /* exit master */
        NJT_MODULE_V1_PADDING
};








static njt_int_t
njt_http_dyn_server_delete_handler(njt_http_dyn_server_info_t *server_info) {
    njt_http_core_srv_conf_t *cscf;
    u_char *p;
    njt_http_core_main_conf_t *cmcf;
    njt_str_t server_name,msg;
    njt_pool_t *old_pool;
     njt_conf_t conf;
     njt_int_t rc = NJT_OK;
    msg.len = 1024;
    msg.data = njt_pcalloc(server_info->pool,msg.len);

    cscf = server_info->cscf;
    if (cscf == NULL ) {
	if(msg.data != NULL && cscf == NULL){
                    p = njt_snprintf(msg.data, 1024, "error:host[%V],no find server [%V]!", &server_info->addr_port,&server_info->server_name);
                    msg.len = p - msg.data;
                    server_info->msg = msg;
                    njt_log_error(NJT_LOG_NOTICE, njt_cycle->log, 0, "host[%V],no find server [%V]!",&server_info->addr_port,&server_info->server_name);
            } else if(cscf != NULL){
                    njt_str_set(&server_info->msg,"error:server is null!");
                    njt_log_error(NJT_LOG_DEBUG,njt_cycle->pool->log, 0, "error:server is null!");
            } else {
                njt_str_set(&server_info->msg,"no find server!");
                njt_log_error(NJT_LOG_DEBUG,njt_cycle->pool->log, 0, "host[%V],no find server [%V]!",&server_info->addr_port,&server_info->server_name);
            }
        return NJT_ERROR;
    }
    cmcf = njt_http_cycle_get_module_main_conf(njt_cycle, njt_http_core_module);

    old_pool = cmcf->dyn_vs_pool;
    cmcf->dyn_vs_pool = NULL;
    cmcf->dyn_vs_pool = njt_create_dynamic_pool(NJT_MIN_POOL_SIZE, njt_cycle->log);
    if(cmcf->dyn_vs_pool == NULL) {
            rc = NJT_ERROR;
            goto out;
    } else {
            njt_sub_pool(conf.cycle->pool,cmcf->dyn_vs_pool);
    }
    conf.pool = cmcf->dyn_vs_pool;
    conf.temp_pool = cmcf->dyn_vs_pool;
    conf.module_type = NJT_CORE_MODULE;
    conf.cmd_type = NJT_MAIN_CONF;
    conf.ctx = njt_cycle->conf_ctx;
    if (njt_http_optimize_servers(&conf, cmcf, cmcf->ports) != NJT_OK) {
        rc = NJT_ERROR;
        goto out;
    }
 
    if(old_pool != NULL){
	njt_destroy_pool(old_pool);
    }
    //note: delete queue memory, which delete when remove queue 
    njt_log_error(NJT_LOG_NOTICE, njt_cycle->log, 0, "delete  server [%V] succ!",&server_name);
    return NJT_OK;
out:
    return rc;
	
}


njt_int_t njt_http_check_upstream_exist(njt_cycle_t *cycle,njt_pool_t *pool, njt_str_t *name) {
    njt_uint_t i;
    njt_http_upstream_srv_conf_t **uscfp;
    njt_http_upstream_main_conf_t *umcf;
    njt_url_t u;
    size_t add,len;
    u_short port;
    u_char *p;

    if (name->len < 8) {
        return NJT_ERROR;
    }
    if (njt_strncasecmp(name->data, (u_char *) "http://", 7) == 0) {
        add = 7;
        port = 80;
    } else if (njt_strncasecmp(name->data, (u_char *) "https://", 8) == 0) {
        add = 8;
        port = 443;
    } else {
        return NJT_ERROR;
    }
    len = name->len;
    p = (u_char *) njt_strlchr(name->data,name->data+name->len,'$');
    if(p != NULL){
	len = p - name->data;
    } 
    njt_memzero(&u, sizeof(njt_url_t));
    
    u.url.len =  len - add;
    u.url.data = name->data + add;
    u.default_port = port;
    u.uri_part = 1;
    u.no_resolve = 1;

    if (njt_parse_url(pool, &u) != NJT_OK) {
        if (u.err) {
            return NJT_ERROR;
        }
    }
    umcf = njt_http_cycle_get_module_main_conf(cycle, njt_http_upstream_module);

    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        if (uscfp[i]->host.len == u.host.len
            && njt_strncasecmp(uscfp[i]->host.data, u.host.data, u.host.len)
               == 0) {
            return NJT_OK;
        }
    }
    return NJT_ERROR;
}



static njt_int_t njt_http_add_server_handler(njt_http_dyn_server_info_t *server_info,njt_uint_t from_api_add) {
    njt_conf_t conf;
    njt_int_t rc = NJT_OK;
	njt_uint_t  msg_len;
    char *rv = NULL;
    njt_pool_t  *old_pool = NULL;
    njt_http_conf_ctx_t* http_ctx;
    njt_str_t server_name,msg;
    njt_str_t server_path; // = njt_string("./conf/add_server.txt");
     njt_http_core_main_conf_t *cmcf;
    njt_http_core_srv_conf_t *cscf;
    njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "add server start +++++++++++++++");

    msg_len = 1024;
    msg.len = msg_len;
    msg.data = njt_pcalloc(server_info->pool,msg.len);

    server_path.len = 0;
    server_path.data = NULL;
    if (server_info->file.len != 0) {
        server_path = server_info->file;
    }
	

    if (server_path.len == 0) {
    	njt_log_error(NJT_LOG_ERR, njt_cycle->log, 0, "add server error:server_path=0");
		njt_str_set(&server_info->msg,"add server error:server_path=0");
        rc = NJT_ERROR;
        goto out;
    }


    if (rc == NJT_ERROR || rc > NJT_OK) {
    	njt_log_error(NJT_LOG_ERR, njt_cycle->log, 0, "add server error!");
		njt_str_set(&server_info->msg,"add server error!");
        rc = NJT_ERROR;
        goto out;
    }

        server_name = server_info->server_name;
	


    

    njt_memzero(&conf, sizeof(njt_conf_t));
    conf.args = njt_array_create(server_info->pool, 10, sizeof(njt_str_t));
    if (conf.args == NULL) {
	njt_log_error(NJT_LOG_ERR, njt_cycle->log, 0, "add  server[%V] error:args allocate fail!",&server_name);
	rc = NJT_ERROR;
	goto out;
    }

    server_info->msg.len = NJT_MAX_CONF_ERRSTR;
    server_info->msg.data = njt_pcalloc(server_info->pool,server_info->msg.len);
    if(server_info->msg.data != NULL){ 
	conf.errstr = &server_info->msg;
    }
	
    cmcf = njt_http_cycle_get_module_main_conf(njt_cycle, njt_http_core_module);
    http_ctx = (njt_http_conf_ctx_t*)njt_get_conf(njt_cycle->conf_ctx, njt_http_module);
    conf.pool = server_info->pool; 
    conf.temp_pool = server_info->pool;
    conf.ctx = http_ctx;
    conf.cycle = (njt_cycle_t *) njt_cycle;
    conf.log = njt_cycle->log;
    conf.module_type = NJT_HTTP_MODULE;
    conf.cmd_type = NJT_HTTP_MAIN_CONF;
    conf.dynamic = 1;

    //clcf->locations = NULL; // clcf->old_locations;
    njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "njt_conf_parse start +++++++++++++++");
    rv = njt_conf_parse(&conf, &server_path);
    if (rv != NULL) {
	
		//njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "njt_conf_parse  server[%V] error:%s",&server_name,rv);
		//njt_str_set(&server_info->msg,"njt_conf_parse error!");
        rc = NJT_ERROR;
        goto out;
    }
    njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "njt_conf_parse end +++++++++++++++");
   
    cscf = http_ctx->srv_conf[njt_http_core_module.ctx_index];
    conf.pool = cscf->pool;
    conf.temp_pool = cscf->pool;
    njt_http_variables_init_vars_dyn(&conf);




    //merge servers
    njt_http_module_t *module;
    njt_uint_t mi, m;
    njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "merge start +++++++++++++++");
    for (m = 0; conf.cycle->modules[m]; m++) {
        if (conf.cycle->modules[m]->type != NJT_HTTP_MODULE) {
            continue;
        }

        module = conf.cycle->modules[m]->ctx;
	mi = conf.cycle->modules[m]->ctx_index;
        rv = njt_http_merge_servers(&conf, cmcf, module, mi);
            if (rv != NJT_CONF_OK) {
                rc = NJT_ERROR;
		njt_str_set(&server_info->msg,"add server error:merge_servers");
    		njt_log_error(NJT_LOG_ERR, njt_cycle->log, 0, "add server error:merge_servers!");
                goto out;
            }
    }
    

    old_pool = cmcf->dyn_vs_pool;
    cmcf->dyn_vs_pool = NULL;
    cmcf->dyn_vs_pool = njt_create_dynamic_pool(NJT_MIN_POOL_SIZE, njt_cycle->log);
    if(cmcf->dyn_vs_pool == NULL) {
	    rc = NJT_ERROR;
	    goto out;
    } else {
	    njt_sub_pool(conf.cycle->pool,cmcf->dyn_vs_pool);
    }
    conf.pool = cmcf->dyn_vs_pool;
    conf.temp_pool = cmcf->dyn_vs_pool;
    conf.module_type = NJT_CORE_MODULE;
    conf.cmd_type = NJT_MAIN_CONF;
    conf.ctx = njt_cycle->conf_ctx;	
    if (njt_http_optimize_servers(&conf, cmcf, cmcf->ports) != NJT_OK) {
        rc = NJT_ERROR;
	goto out;
    }

    njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "merge end +++++++++++++++");

    njt_log_error(NJT_LOG_DEBUG,njt_cycle->log, 0, "add server end +++++++++++++++");
out:
    if(old_pool != NULL) {
	njt_destroy_pool(old_pool);
    }
    if(rc != NJT_OK) {
    	  njt_log_error(NJT_LOG_ERR, njt_cycle->log, 0, "add  server [%V] error!",&server_name);
    } else {
	    njt_log_error(NJT_LOG_NOTICE, njt_cycle->log, 0, "add  server [%V] succ!",&server_name);
    }
    return rc;
}





static int njt_agent_server_change_handler_internal(njt_str_t *key, njt_str_t *value, void *data,njt_str_t *out_msg) {
	njt_str_t  add = njt_string("add");
	njt_str_t  del = njt_string("del");
	njt_str_t  del_topic = njt_string("");
	njt_str_t  worker_str = njt_string("/worker_0");
	njt_str_t  new_key;
	njt_rpc_result_t * rpc_result;
	njt_uint_t from_api_add = 0;

	njt_int_t rc = NJT_OK;
	njt_http_dyn_server_info_t *server_info;
	njt_log_error(NJT_LOG_INFO, njt_cycle->log, 0, "get topic  key=%V,value=%V",key,value);

	server_info = njt_http_parser_server_data(*value,0);
	if(server_info == NULL) {
		njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "topic msg error key=%V,value=%V",key,value);
		return NJT_ERROR;
	}
	rpc_result = njt_rpc_result_create();
    if(rpc_result == NULL){
		njt_log_error(NJT_LOG_ERR, njt_cycle->log, 0, "rpc_result allocate null");
       return NJT_ERROR;
    }

	if(server_info->type.len == add.len && njt_strncmp(server_info->type.data,add.data,server_info->type.len) == 0 ) {
		njt_http_dyn_server_write_data(server_info);
		if(key->len > worker_str.len && njt_strncmp(key->data,worker_str.data,worker_str.len) == 0) {
			from_api_add = 1;
		} 
		rc = njt_http_add_server_handler(server_info,from_api_add);  //njt_http_dyn_server_delete_handler
		if(rc != NJT_OK) {
			if(from_api_add == 0){
				njt_log_error(NJT_LOG_ERR, njt_cycle->log, 0, "add topic_kv_change_handler error key=%V,value=%V",key,value);
				njt_kv_sendmsg(key,&del_topic,0);
			}
			njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "add topic_kv_change_handler error key=%V,value=%V",key,value);
		} else {
			if(key->len > worker_str.len && njt_strncmp(key->data,worker_str.data,worker_str.len) == 0) {
				new_key.data = key->data + worker_str.len;
				new_key.len  = key->len - worker_str.len;
				njt_kv_sendmsg(&new_key,value,1);
			}
			njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "add topic_kv_change_handler succ key=%V,value=%V",key,value);
		}
	} else if(server_info->type.len == del.len && njt_strncmp(server_info->type.data,del.data,server_info->type.len) == 0 ){
		njt_http_dyn_server_write_data(server_info);
		rc = njt_http_dyn_server_delete_handler(server_info);
		njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "delete topic_kv_change_handler key=%V,value=%V",key,value);
	}
	if(rc == NJT_OK) {
		njt_rpc_result_set_code(rpc_result,NJT_RPC_RSP_SUCCESS);
	} else {
		njt_rpc_result_set_code(rpc_result,NJT_RPC_RSP_ERR);
		njt_rpc_result_set_msg2(rpc_result,&server_info->msg);
	}
	if(out_msg){
        njt_rpc_result_to_json_str(rpc_result,out_msg);
    }
	if(rpc_result){
        njt_rpc_result_destroy(rpc_result);
    }

	njt_destroy_pool(server_info->pool);
	
	return NJT_OK;
}


static u_char* njt_agent_server_put_handler(njt_str_t *topic, njt_str_t *request, int* len, void *data) {
    njt_str_t err_json_msg;
    njt_str_null(&err_json_msg);
    // 新增字符串参数err_json_msg用于返回到客户端。
    njt_agent_server_change_handler_internal(topic,request,data,&err_json_msg);
    *len = err_json_msg.len;
    return err_json_msg.data;

}

static int  topic_kv_change_handler(njt_str_t *key, njt_str_t *value, void *data){
    return  njt_agent_server_change_handler_internal(key,value,data,NULL);
}

static njt_int_t
njt_http_dyn_server_init_worker(njt_cycle_t *cycle) {

	njt_str_t  key = njt_string("srv");
	if (njt_process != NJT_PROCESS_WORKER && njt_process != NJT_PROCESS_SINGLE) {
		/*only works in the worker 0 prcess.*/
		return NJT_OK;
	}
    njt_kv_reg_handler_t h;
    njt_memzero(&h, sizeof(njt_kv_reg_handler_t));
    h.key = &key;
    h.rpc_put_handler = njt_agent_server_put_handler;
    h.handler = topic_kv_change_handler;
    h.api_type = NJT_KV_API_TYPE_INSTRUCTIONAL;
    njt_kv_reg_handler(&h);

    return NJT_OK;
}



njt_int_t njt_http_check_sub_server(njt_json_element *in_items,njt_http_dyn_server_info_t *server_info) {
 
	njt_json_element  *items;
	njt_str_t   str;
	njt_str_t   error = njt_string("invalid parameter:");
	njt_queue_t   *q;
	if(in_items->type != NJT_JSON_OBJ) {
		njt_str_set(&server_info->msg, "json error!!!");
		return NJT_ERROR;
	}

        for (q = njt_queue_head(&in_items->objdata.datas);
         q != njt_queue_sentinel(&in_items->objdata.datas);
         q = njt_queue_next(q)) {

		items = njt_queue_data(q, njt_json_element, ele_queue);
		if(items == NULL){
			break;
		}
	  njt_str_set(&str,"server_name");
	  if(items->key.len == str.len && njt_strncmp(str.data,items->key.data,str.len) == 0){
		continue;
	  }
	  njt_str_set(&str,"server_body");
	  if(items->key.len == str.len && njt_strncmp(str.data,items->key.data,str.len) == 0){
		continue;
	  }
	  njt_str_set(&str,"servers");
	  if(items->key.len == str.len && njt_strncmp(str.data,items->key.data,str.len) == 0){
		continue;
	  }
	  str.len = error.len + items->key.len + 1;
	  str.data = njt_pcalloc(server_info->pool,str.len);
	  if(str.data != NULL) {
	     njt_snprintf(str.data,str.len,"%V%V!",&error,&items->key);	
	     server_info->msg = str;
	  } else {
		njt_str_set(&server_info->msg, "json error!!!");
	  }
	}
	if(server_info->msg.len > 0){
	  return NJT_ERROR;
	}
	return NJT_OK;
}

njt_int_t njt_http_check_top_server( njt_json_manager *json_body,njt_http_dyn_server_info_t *server_info) {
 
	njt_json_element  *items;
	njt_str_t   str;
	njt_queue_t   *q;
	njt_str_t   error = njt_string("invalid parameter:");
	if(json_body->json_val == NULL || json_body->json_val->type != NJT_JSON_OBJ) {
		njt_str_set(&server_info->msg, "json error!!!");
		return NJT_ERROR;
	}

        for (q = njt_queue_head(&json_body->json_val->objdata.datas);
         q != njt_queue_sentinel(&json_body->json_val->objdata.datas);
         q = njt_queue_next(q)) {

		items = njt_queue_data(q, njt_json_element, ele_queue);
		if(items == NULL){
			break;
		}
	  njt_str_set(&str,"type");
	  if(items->key.len == str.len && njt_strncmp(str.data,items->key.data,str.len) == 0){
		continue;
	  }
	  njt_str_set(&str,"addr_port");
	  if(items->key.len == str.len && njt_strncmp(str.data,items->key.data,str.len) == 0){
		continue;
	  }
	  njt_str_set(&str,"server_name");
	  if(items->key.len == str.len && njt_strncmp(str.data,items->key.data,str.len) == 0){
		continue;
	  }
	  njt_str_set(&str,"server_body");
	  if(items->key.len == str.len && njt_strncmp(str.data,items->key.data,str.len) == 0){
		continue;
	  }
	  str.len = error.len + items->key.len + 1;
	  str.data = njt_pcalloc(server_info->pool,str.len);
	  if(str.data != NULL) {
	     njt_snprintf(str.data,str.len,"%V%V!",&error,&items->key);	
	     server_info->msg = str;
	  } else {
		njt_str_set(&server_info->msg, "json error!!!");
	  }
	}
	if(server_info->msg.len > 0){
	  return NJT_ERROR;
	}
	return NJT_OK;
}
njt_http_dyn_server_info_t * njt_http_parser_server_data(njt_str_t json_str,njt_uint_t method) {
	 njt_json_manager json_body;
	 njt_pool_t  *server_pool;
	  njt_http_dyn_server_info_t *server_info;
	 njt_int_t rc;
	 //njt_http_sub_server_info_t   sub_server;
	 //u_char *last;
	 //u_char *p;
	 njt_str_t  add = njt_string("add");
	njt_str_t  del = njt_string("del");
	njt_str_t  key;
	 njt_json_element *items;
	 

	server_pool = njt_create_pool(NJT_DEFAULT_POOL_SIZE, njt_cycle->log);
	if(server_pool == NULL) {
		return NULL;
	}

	rc = njt_json_2_structure(&json_str, &json_body, server_pool);
    if (rc != NJT_OK) {
        rc = NJT_ERROR;
		njt_destroy_pool(server_pool);
        return NULL;
    }
	server_info = njt_pcalloc(server_pool, sizeof(njt_http_dyn_server_info_t));
    if (server_info == NULL) {
		njt_destroy_pool(server_pool);
        return NULL;
    }

	//server_info->type = -1;
	server_info->pool = server_pool;

	rc = njt_http_check_top_server(&json_body,server_info);
	if(rc == NJT_ERROR) {
	   goto end;
	}
	
	njt_str_set(&key,"addr_port");
	rc = njt_struct_top_find(&json_body, &key, &items);
	if(rc != NJT_OK || items->type != NJT_JSON_STR){
		//server_info->code = 1; 
		njt_str_set(&server_info->msg, "addr_port error!!!");
		goto end;
	} else {
		server_info->addr_port = njt_del_headtail_space(items->strval);
		if(server_info->addr_port.len == 0){
		  njt_str_set(&server_info->msg, "addr_port null!!!");
		  goto end;
		}
		/*
		 last = server_info->addr_port.data + server_info->addr_port.len;
            p = njt_strlchr(server_info->addr_port.data, last, ':');
            if (p != NULL) {
                p = p + 1;
                server_info->sport.data = p;
                server_info->sport.len = server_info->addr_port.data + server_info->addr_port.len - p;
            } else {
                server_info->sport = server_info->addr_port;
            }*/
	}
	njt_str_set(&key,"type");
	rc = njt_struct_top_find(&json_body, &key, &items);
	if(rc != NJT_OK || items->type != NJT_JSON_STR){
		njt_str_set(&server_info->msg, "type error!!!");
		goto end;
	} else {
		server_info->type = njt_del_headtail_space(items->strval);
		if(method != 0 && server_info->type.len == add.len && njt_strncmp(server_info->type.data,add.data,server_info->type.len) == 0 && method != NJT_HTTP_POST) {
		     njt_str_set(&server_info->msg, "no support method when add server!");
		     goto end;
		}
		if(method != 0 && server_info->type.len == del.len && njt_strncmp(server_info->type.data,del.data,server_info->type.len) == 0 && method != NJT_HTTP_PUT) {
		     njt_str_set(&server_info->msg, "no support method when del server!");
		     goto end;
		}
		if((server_info->type.len == add.len && njt_strncmp(server_info->type.data,add.data,server_info->type.len) == 0) || (server_info->type.len == del.len && njt_strncmp(server_info->type.data,del.data,server_info->type.len) == 0)) {
		} else {
			njt_str_set(&server_info->msg, "type error!!!");
			goto end;
		}
	}

	njt_str_set(&key,"server_name");
	rc = njt_struct_top_find(&json_body, &key, &items);
	if(rc == NJT_OK ){
		 if (items->type != NJT_JSON_STR) {
	   	njt_str_set(&server_info->msg, "server_name error!");
			   goto end;
          	}
		server_info->server_name = njt_del_headtail_space(items->strval);
		njt_log_error(NJT_LOG_DEBUG, njt_cycle->log, 0, "server_name[%V,%V]",&items->strval,&server_info->server_name);
		if(server_info->server_name.len == 0) {
                                   njt_str_set(&server_info->msg, "server_name is null!");
                                   goto end;
                                }
	}else {
		if( server_info->type.len == del.len && njt_strncmp(server_info->type.data,del.data,server_info->type.len) == 0) {
		    njt_str_set(&server_info->msg, "server_name is null!");
			goto end;
		}
	}

	

	njt_str_set(&key,"server_body");
	rc = njt_struct_top_find(&json_body, &key, &items);
	if(rc == NJT_OK ){
		 if (items->type != NJT_JSON_STR) {
	   	njt_str_set(&server_info->msg, "server_body error!");
			   goto end;
          	}
		server_info->server_body = njt_del_headtail_space(items->strval);
		if(server_info->server_body.len == 0) {
		  goto end;
		}
	} else {
		if(server_info->type.len == add.len && njt_strncmp(server_info->type.data,add.data,server_info->type.len) == 0) {
	   	njt_str_set(&server_info->msg, "server_body is null!");
		goto end;
		}
	} 
	
end:
	return server_info;


}



static njt_int_t njt_http_server_write_data(njt_fd_t fd,njt_http_dyn_server_info_t *server_info) {

	u_char *p,*data;
	int32_t  rlen,buffer_len,remain;

	buffer_len = server_info->buffer_len;
	remain = buffer_len;
	data = server_info->buffer;

	
	if(server_info) {
		njt_memzero(data,buffer_len);
		p = data;
		p = njt_snprintf(p, remain, "server {\n");
		remain = data + buffer_len - p;

		if(server_info->server_body.len != 0 && server_info->server_body.data != NULL){
			p = njt_snprintf(p, remain, " %V; \n}\n",&server_info->server_body);
		} else {
			p = njt_snprintf(p, remain, "}\n");
		}
		remain = data + buffer_len - p;

		rlen = njt_write_fd(fd, data, p - data);
		if(rlen < 0) {
			return NJT_ERROR;
		}
	}
	return NJT_OK;

}
static void njt_http_dyn_server_write_data(njt_http_dyn_server_info_t *server_info) {

    
    //njt_str_t  dport;
    njt_fd_t fd;
    njt_int_t  rc; 
    //njt_uint_t i;
    
    u_char *p; // *data;
    njt_http_core_srv_conf_t *cscf;
  
    njt_str_t server_file = njt_string("add_server.txt");
    njt_str_t server_path;
    njt_str_t server_full_file;
	//njt_str_t  tag = njt_string("\n}\n");
	//njt_http_sub_server_info_t **loc_array;
    

    cscf = njt_http_get_srv_by_port((njt_cycle_t  *)njt_cycle,&server_info->addr_port,&server_info->server_name);	
    (*server_info).cscf = cscf;

        server_path = njt_cycle->prefix;

        //todo
        //njt_str_set(&server_path, "/tmp/");
        server_full_file.len = server_path.len + server_file.len + 10;//  workid_add_server.txt
        server_full_file.data = njt_pcalloc(server_info->pool, server_full_file.len);
        p = njt_snprintf(server_full_file.data, server_full_file.len, "%Vlogs/%d_%V", &server_path, njt_worker,
                         &server_file);
        server_full_file.len = p - server_full_file.data;
    fd = njt_open_file(server_full_file.data, NJT_FILE_CREATE_OR_OPEN | NJT_FILE_RDWR, NJT_FILE_TRUNCATE,
                       NJT_FILE_DEFAULT_ACCESS);
    if (fd == NJT_INVALID_FILE) {
        return;
    }

    
		if(server_info->buffer == NULL) {
			server_info->buffer_len = 10240;
			server_info->buffer     = njt_pcalloc(server_info->pool, server_info->buffer_len);
			if(server_info->buffer == NULL) {
				return;
			}
		}
	rc = njt_http_server_write_data(fd,server_info);
	
	if (njt_close_file(fd) == NJT_FILE_ERROR) {

	}
    

    if (rc  == NJT_ERROR) {
        return;
    }
    (*server_info).file = server_full_file;
}


