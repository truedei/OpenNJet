# vim:set ft= ts=4 sw=4 et fdm=marker:

use Test::Nginx::Socket 'no_plan';

repeat_each(2);

run_tests();

__DATA__

=== TEST 1: one buf was linked to multiple njt_chain_t nodes
--- config
    location /t {
        content_by_lua_block {
            local str = string.rep(".", 1300)
            njt.print(str)
            njt.flush()
            njt.print("small chunk")
            njt.flush()
        }
        body_filter_by_lua_block {local dummy=1}
    }
--- request
GET /t
--- response_body_like: small chunk
