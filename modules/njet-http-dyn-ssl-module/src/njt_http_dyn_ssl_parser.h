

/* This file was generated by JSON Schema to C.
 * Any changes made to it will be lost on regeneration. 

 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */

#ifndef PARSER_SSL_H
#define PARSER_SSL_H
#include <stdint.h>
#include <stdbool.h>
#include "njt_core.h"
#include "js2c_njet_builtins.h"
/* ===================== Generated type declarations ===================== */
typedef njt_str_t dyn_ssl_servers_item_listens_item_t;

typedef njt_array_t  dyn_ssl_servers_item_listens_t;
typedef njt_str_t dyn_ssl_servers_item_serverNames_item_t;

typedef njt_array_t  dyn_ssl_servers_item_serverNames_t;
typedef enum dyn_ssl_servers_item_certificates_item_cert_type_t_e{
    DYN_SSL_SERVERS_ITEM_CERTIFICATES_ITEM_CERT_TYPE_RSA,
    DYN_SSL_SERVERS_ITEM_CERTIFICATES_ITEM_CERT_TYPE_NTLS,
    DYN_SSL_SERVERS_ITEM_CERTIFICATES_ITEM_CERT_TYPE_ECC,
    DYN_SSL_SERVERS_ITEM_CERTIFICATES_ITEM_CERT_TYPE_OTHER
} dyn_ssl_servers_item_certificates_item_cert_type_t;

typedef njt_str_t dyn_ssl_servers_item_certificates_item_certificate_t;

typedef njt_str_t dyn_ssl_servers_item_certificates_item_certificateKey_t;

typedef njt_str_t dyn_ssl_servers_item_certificates_item_certificateEnc_t;

typedef njt_str_t dyn_ssl_servers_item_certificates_item_certificateKeyEnc_t;

typedef struct dyn_ssl_servers_item_certificates_item_t_s {
    dyn_ssl_servers_item_certificates_item_cert_type_t cert_type;
    dyn_ssl_servers_item_certificates_item_certificate_t certificate;
    dyn_ssl_servers_item_certificates_item_certificateKey_t certificateKey;
    dyn_ssl_servers_item_certificates_item_certificateEnc_t certificateEnc;
    dyn_ssl_servers_item_certificates_item_certificateKeyEnc_t certificateKeyEnc;
    unsigned int is_cert_type_set:1;
    unsigned int is_certificate_set:1;
    unsigned int is_certificateKey_set:1;
    unsigned int is_certificateEnc_set:1;
    unsigned int is_certificateKeyEnc_set:1;
} dyn_ssl_servers_item_certificates_item_t;

typedef njt_array_t  dyn_ssl_servers_item_certificates_t;
typedef struct dyn_ssl_servers_item_t_s {
    dyn_ssl_servers_item_listens_t *listens;
    dyn_ssl_servers_item_serverNames_t *serverNames;
    dyn_ssl_servers_item_certificates_t *certificates;
    unsigned int is_listens_set:1;
    unsigned int is_serverNames_set:1;
    unsigned int is_certificates_set:1;
} dyn_ssl_servers_item_t;

typedef njt_array_t  dyn_ssl_servers_t;
typedef struct dyn_ssl_t_s {
    dyn_ssl_servers_t *servers;
    unsigned int is_servers_set:1;
} dyn_ssl_t;

dyn_ssl_servers_item_listens_item_t* get_dyn_ssl_servers_item_listens_item(dyn_ssl_servers_item_listens_t *out, size_t idx);
dyn_ssl_servers_item_serverNames_item_t* get_dyn_ssl_servers_item_serverNames_item(dyn_ssl_servers_item_serverNames_t *out, size_t idx);
dyn_ssl_servers_item_certificates_item_cert_type_t get_dyn_ssl_servers_item_certificates_item_cert_type(dyn_ssl_servers_item_certificates_item_t *out);
dyn_ssl_servers_item_certificates_item_certificate_t* get_dyn_ssl_servers_item_certificates_item_certificate(dyn_ssl_servers_item_certificates_item_t *out);
dyn_ssl_servers_item_certificates_item_certificateKey_t* get_dyn_ssl_servers_item_certificates_item_certificateKey(dyn_ssl_servers_item_certificates_item_t *out);
dyn_ssl_servers_item_certificates_item_certificateEnc_t* get_dyn_ssl_servers_item_certificates_item_certificateEnc(dyn_ssl_servers_item_certificates_item_t *out);
dyn_ssl_servers_item_certificates_item_certificateKeyEnc_t* get_dyn_ssl_servers_item_certificates_item_certificateKeyEnc(dyn_ssl_servers_item_certificates_item_t *out);
dyn_ssl_servers_item_certificates_item_t* get_dyn_ssl_servers_item_certificates_item(dyn_ssl_servers_item_certificates_t *out, size_t idx);
// CHECK ARRAY not exceeding bounds before calling this func
dyn_ssl_servers_item_listens_t* get_dyn_ssl_servers_item_listens(dyn_ssl_servers_item_t *out);
// CHECK ARRAY not exceeding bounds before calling this func
dyn_ssl_servers_item_serverNames_t* get_dyn_ssl_servers_item_serverNames(dyn_ssl_servers_item_t *out);
// CHECK ARRAY not exceeding bounds before calling this func
dyn_ssl_servers_item_certificates_t* get_dyn_ssl_servers_item_certificates(dyn_ssl_servers_item_t *out);
dyn_ssl_servers_item_t* get_dyn_ssl_servers_item(dyn_ssl_servers_t *out, size_t idx);
// CHECK ARRAY not exceeding bounds before calling this func
dyn_ssl_servers_t* get_dyn_ssl_servers(dyn_ssl_t *out);
int add_item_dyn_ssl_servers_item_listens(dyn_ssl_servers_item_listens_t *src, dyn_ssl_servers_item_listens_item_t* items);
dyn_ssl_servers_item_listens_t* create_dyn_ssl_servers_item_listens(njt_pool_t *pool, size_t nelts);
void set_dyn_ssl_servers_item_listens(dyn_ssl_servers_item_t* obj, dyn_ssl_servers_item_listens_t* field);
int add_item_dyn_ssl_servers_item_serverNames(dyn_ssl_servers_item_serverNames_t *src, dyn_ssl_servers_item_serverNames_item_t* items);
dyn_ssl_servers_item_serverNames_t* create_dyn_ssl_servers_item_serverNames(njt_pool_t *pool, size_t nelts);
void set_dyn_ssl_servers_item_serverNames(dyn_ssl_servers_item_t* obj, dyn_ssl_servers_item_serverNames_t* field);
void set_dyn_ssl_servers_item_certificates_item_cert_type(dyn_ssl_servers_item_certificates_item_t* obj, dyn_ssl_servers_item_certificates_item_cert_type_t field);
void set_dyn_ssl_servers_item_certificates_item_certificate(dyn_ssl_servers_item_certificates_item_t* obj, dyn_ssl_servers_item_certificates_item_certificate_t* field);
void set_dyn_ssl_servers_item_certificates_item_certificateKey(dyn_ssl_servers_item_certificates_item_t* obj, dyn_ssl_servers_item_certificates_item_certificateKey_t* field);
void set_dyn_ssl_servers_item_certificates_item_certificateEnc(dyn_ssl_servers_item_certificates_item_t* obj, dyn_ssl_servers_item_certificates_item_certificateEnc_t* field);
void set_dyn_ssl_servers_item_certificates_item_certificateKeyEnc(dyn_ssl_servers_item_certificates_item_t* obj, dyn_ssl_servers_item_certificates_item_certificateKeyEnc_t* field);
dyn_ssl_servers_item_certificates_item_t* create_dyn_ssl_servers_item_certificates_item(njt_pool_t *pool);
int add_item_dyn_ssl_servers_item_certificates(dyn_ssl_servers_item_certificates_t *src, dyn_ssl_servers_item_certificates_item_t* items);
dyn_ssl_servers_item_certificates_t* create_dyn_ssl_servers_item_certificates(njt_pool_t *pool, size_t nelts);
void set_dyn_ssl_servers_item_certificates(dyn_ssl_servers_item_t* obj, dyn_ssl_servers_item_certificates_t* field);
dyn_ssl_servers_item_t* create_dyn_ssl_servers_item(njt_pool_t *pool);
int add_item_dyn_ssl_servers(dyn_ssl_servers_t *src, dyn_ssl_servers_item_t* items);
dyn_ssl_servers_t* create_dyn_ssl_servers(njt_pool_t *pool, size_t nelts);
void set_dyn_ssl_servers(dyn_ssl_t* obj, dyn_ssl_servers_t* field);
dyn_ssl_t* create_dyn_ssl(njt_pool_t *pool);
dyn_ssl_t* json_parse_dyn_ssl(njt_pool_t *pool, const njt_str_t *json_string, js2c_parse_error_t *err_ret);
njt_str_t* to_json_dyn_ssl(njt_pool_t *pool, dyn_ssl_t *out, njt_int_t flags);
#endif /* PARSER_SSL_H */
