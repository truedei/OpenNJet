

/* This file was generated by JSON Schema to C.
 * Any changes made to it will be lost on regeneration. 

 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */

#ifndef PARSER_RANGE_H
#define PARSER_RANGE_H
#include <stdint.h>
#include <stdbool.h>
#include "njt_core.h"
#include "js2c_njet_builtins.h"
/* ===================== Generated type declarations ===================== */
typedef enum dyn_range_ranges_item_type_t_e{
    DYN_RANGE_RANGES_ITEM_TYPE_TCP,
    DYN_RANGE_RANGES_ITEM_TYPE_UDP
} dyn_range_ranges_item_type_t;

typedef njt_str_t dyn_range_ranges_item_src_ports_t;

typedef int64_t dyn_range_ranges_item_dst_port_t;
typedef int64_t dyn_range_ranges_item_try_del_times_t;
typedef njt_str_t dyn_range_ranges_item_iptables_path_t;

typedef struct dyn_range_ranges_item_t_s {
    dyn_range_ranges_item_type_t type;
    dyn_range_ranges_item_src_ports_t src_ports;
    dyn_range_ranges_item_dst_port_t dst_port;
    dyn_range_ranges_item_try_del_times_t try_del_times;
    dyn_range_ranges_item_iptables_path_t iptables_path;
    unsigned int is_type_set:1;
    unsigned int is_src_ports_set:1;
    unsigned int is_dst_port_set:1;
    unsigned int is_try_del_times_set:1;
    unsigned int is_iptables_path_set:1;
} dyn_range_ranges_item_t;

typedef njt_array_t  dyn_range_ranges_t;
typedef struct dyn_range_t_s {
    dyn_range_ranges_t *ranges;
    unsigned int is_ranges_set:1;
} dyn_range_t;

dyn_range_ranges_item_type_t get_dyn_range_ranges_item_type(dyn_range_ranges_item_t *out);
dyn_range_ranges_item_src_ports_t* get_dyn_range_ranges_item_src_ports(dyn_range_ranges_item_t *out);
dyn_range_ranges_item_dst_port_t get_dyn_range_ranges_item_dst_port(dyn_range_ranges_item_t *out);
dyn_range_ranges_item_try_del_times_t get_dyn_range_ranges_item_try_del_times(dyn_range_ranges_item_t *out);
dyn_range_ranges_item_iptables_path_t* get_dyn_range_ranges_item_iptables_path(dyn_range_ranges_item_t *out);
dyn_range_ranges_item_t* get_dyn_range_ranges_item(dyn_range_ranges_t *out, size_t idx);
// CHECK ARRAY not exceeding bounds before calling this func
dyn_range_ranges_t* get_dyn_range_ranges(dyn_range_t *out);
void set_dyn_range_ranges_item_type(dyn_range_ranges_item_t* obj, dyn_range_ranges_item_type_t field);
void set_dyn_range_ranges_item_src_ports(dyn_range_ranges_item_t* obj, dyn_range_ranges_item_src_ports_t* field);
void set_dyn_range_ranges_item_dst_port(dyn_range_ranges_item_t* obj, dyn_range_ranges_item_dst_port_t field);
void set_dyn_range_ranges_item_try_del_times(dyn_range_ranges_item_t* obj, dyn_range_ranges_item_try_del_times_t field);
void set_dyn_range_ranges_item_iptables_path(dyn_range_ranges_item_t* obj, dyn_range_ranges_item_iptables_path_t* field);
dyn_range_ranges_item_t* create_dyn_range_ranges_item(njt_pool_t *pool);
int add_item_dyn_range_ranges(dyn_range_ranges_t *src, dyn_range_ranges_item_t* items);
dyn_range_ranges_t* create_dyn_range_ranges(njt_pool_t *pool, size_t nelts);
void set_dyn_range_ranges(dyn_range_t* obj, dyn_range_ranges_t* field);
dyn_range_t* create_dyn_range(njt_pool_t *pool);
dyn_range_t* json_parse_dyn_range(njt_pool_t *pool, const njt_str_t *json_string, js2c_parse_error_t *err_ret);
njt_str_t* to_json_dyn_range(njt_pool_t *pool, dyn_range_t *out, njt_int_t flags);
#endif /* PARSER_RANGE_H */
