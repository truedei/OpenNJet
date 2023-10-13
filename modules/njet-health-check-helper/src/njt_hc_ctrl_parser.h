

/* This file was generated by JSON Schema to C.
 * Any changes made to it will be lost on regeneration. 

 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */

#ifndef PARSER_HEALTH_CHECK_H
#define PARSER_HEALTH_CHECK_H
#include <stdint.h>
#include <stdbool.h>
#include "njt_core.h"
#include "js2c_njet_builtins.h"
/* ===================== Generated type declarations ===================== */
typedef njt_str_t health_checks_item_hc_type_t;

typedef njt_str_t health_checks_item_upstream_name_t;

typedef struct health_checks_item_t_s {
    health_checks_item_hc_type_t hc_type;
    health_checks_item_upstream_name_t upstream_name;
    unsigned int is_hc_type_set:1;
    unsigned int is_upstream_name_set:1;
} health_checks_item_t;

typedef njt_array_t  health_checks_t;
health_checks_item_hc_type_t* get_health_checks_item_hc_type(health_checks_item_t *out);
health_checks_item_upstream_name_t* get_health_checks_item_upstream_name(health_checks_item_t *out);
health_checks_item_t* get_health_checks_item(health_checks_t *out, size_t idx);
void set_health_checks_item_hc_type(health_checks_item_t* obj, health_checks_item_hc_type_t* field);
void set_health_checks_item_upstream_name(health_checks_item_t* obj, health_checks_item_upstream_name_t* field);
health_checks_item_t* create_health_checks_item(njt_pool_t *pool);
int add_item_health_checks(health_checks_t *src, health_checks_item_t* items);
health_checks_t* create_health_checks(njt_pool_t *pool, size_t nelts);
health_checks_t* json_parse_health_checks(njt_pool_t *pool, const njt_str_t *json_string, js2c_parse_error_t *err_ret);
njt_str_t* to_json_health_checks(njt_pool_t *pool, health_checks_t *out, njt_int_t flags);
#endif /* PARSER_HEALTH_CHECK_H */