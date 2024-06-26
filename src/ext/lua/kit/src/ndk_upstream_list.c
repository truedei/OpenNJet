

/* TODO : generalize this into a generic list module, with weight */


typedef struct {
    njt_uint_t      weight;
    njt_str_t       s;
    njt_conf_t     *cf;
} ndk_upstream_list_parse_t;



static njt_int_t
ndk_upstream_list_parse_weight (ndk_upstream_list_parse_t *ulp)
{
    size_t      i;
    njt_str_t   *s;

    s = &ulp->s;

    for (i=0; i<s->len; i++) {

        if (s->data[i] < '0' || s->data[i] > '9')
            break;
    }

    if (!i) {
        ulp->weight = 1;
        return  NJT_OK;
    }

    if (i == s->len) {
        njt_conf_log_error (NJT_LOG_EMERG, ulp->cf, 0,
            "upstream list just consists of number \"%V\"", s);

        return  NJT_ERROR;
    }

    if (s->data[i] != ':') {
        njt_conf_log_error (NJT_LOG_EMERG, ulp->cf, 0,
            "upstream list not correct format \"%V\"", s);

        return  NJT_ERROR;
    }


    ulp->weight = njt_atoi (s->data, i);

    s->data += (i + 1);
    s->len -= (i + 1);

    return  NJT_OK;
}



static char *
ndk_upstream_list (njt_conf_t *cf, njt_command_t *cmd, void *conf)
{
    /* TODO : change this for getting upstream pointer if available */

    njt_uint_t                   buckets, count, i, j;
    njt_str_t                   *value, **bucket, *us;
    njt_array_t                 *ula;
    ndk_upstream_list_t         *ul, *ule;
    ndk_upstream_list_parse_t    ulp;

    ndk_http_main_conf_t        *mcf;

    mcf = njt_http_conf_get_module_main_conf (cf, ndk_http_module);

    ula = mcf->upstreams;

    /* create array of upstream lists it doesn't already exist */

    if (ula == NULL) {

        ula = njt_array_create (cf->pool, 4, sizeof (ndk_upstream_list_t));
        if (ula == NULL)
            return  NJT_CONF_ERROR;

        mcf->upstreams = ula;
    }


    /* check to see if the list already exists */

    value = cf->args->elts;
    value++;

    ul = ula->elts;
    ule = ul + ula->nelts;

    for ( ; ul<ule; ul++) {

        if (ul->name.len == value->len &&
            njt_strncasecmp (ul->name.data, value->data, value->len) == 0) {

            njt_conf_log_error (NJT_LOG_EMERG, cf, 0,
                           "duplicate upstream list name \"%V\"", value);

            return  NJT_CONF_ERROR;
        }
    }


    /* create a new list */

    ul = njt_array_push (ula);
    if (ul == NULL)
        return  NJT_CONF_ERROR;

    ul->name = *value;



    /* copy all the upstream names */

    count = cf->args->nelts - 2;

    us = njt_palloc (cf->pool, count * sizeof (njt_str_t));
    if (us == NULL)
        return  NJT_CONF_ERROR;

    njt_memcpy (us, value + 1, count * sizeof (njt_str_t));


    /* calculate the total number of buckets */

    buckets = 0;

    ulp.cf = cf;

    for (i=0; i<count; i++, us++) {

        ulp.s = *us;

        if (ndk_upstream_list_parse_weight (&ulp) != NJT_OK)
            return  NJT_CONF_ERROR;

        buckets += ulp.weight;
    }


    /* allocate space for all buckets */

    bucket = njt_palloc (cf->pool, buckets * sizeof (njt_str_t *));
    if (bucket == NULL)
        return  NJT_CONF_ERROR;

    ul->elts = bucket;
    ul->nelts = buckets;


    /* set values for each bucket */

    us -= count;

    for (i=0; i<count; i++, us++) {

        ulp.s = *us;

        if (ndk_upstream_list_parse_weight (&ulp) != NJT_OK)
            return  NJT_CONF_ERROR;

        us->data = ulp.s.data;
        us->len = ulp.s.len;

        /* TODO : check format of upstream */
        /* TODO : add automatic adding of http:// in upstreams? */

        for (j=0; j<ulp.weight; j++, bucket++) {

            *bucket = us;
        }
    }

    return  NJT_CONF_OK;
}


ndk_upstream_list_t *
ndk_get_upstream_list (ndk_http_main_conf_t *mcf, u_char *data, size_t len)
{
    ndk_upstream_list_t         *ul, *ule;
    njt_array_t                 *ua = mcf->upstreams;

    if (ua == NULL) {
        return NULL;
    }

    ul = ua->elts;
    ule = ul + ua->nelts;

    for (; ul < ule; ul++) {
        if (ul->name.len == len && njt_strncasecmp(ul->name.data, data, len) == 0)
        {
            return ul;
        }
    }

    return NULL;
}

