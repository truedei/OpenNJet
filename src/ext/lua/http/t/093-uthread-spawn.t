# vim:set ft= ts=4 sw=4 et fdm=marker:

use Test::Nginx::Socket::Lua;
use t::StapThread;

our $GCScript = $t::StapThread::GCScript;
our $StapScript = $t::StapThread::StapScript;

repeat_each(2);

plan tests => repeat_each() * (blocks() * 4);

$ENV{TEST_NGINX_RESOLVER} ||= '8.8.8.8';
$ENV{TEST_NGINX_MEMCACHED_PORT} ||= '11211';

#no_shuffle();
worker_connections(256);
no_long_string();
run_tests();

__DATA__

=== TEST 1: simple user thread without I/O
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("hello in thread")
            end

            njt.say("before")
            njt.thread.spawn(f)
            njt.say("after")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
terminate 1: ok
delete thread 2
delete thread 1

--- response_body
before
hello in thread
after
--- no_error_log
[error]



=== TEST 2: two simple user threads without I/O
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("in thread 1")
            end

            local function g()
                njt.say("in thread 2")
            end

            njt.say("before 1")
            njt.thread.spawn(f)
            njt.say("after 1")

            njt.say("before 2")
            njt.thread.spawn(g)
            njt.say("after 2")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
create 3 in 1
spawn user thread 3 in 1
terminate 3: ok
terminate 1: ok
delete thread 2
delete thread 3
delete thread 1

--- response_body
before 1
in thread 1
after 1
before 2
in thread 2
after 2
--- no_error_log
[error]



=== TEST 3: simple user thread with sleep
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("before sleep")
                njt.sleep(0.1)
                njt.say("after sleep")
            end

            njt.say("before thread create")
            njt.thread.spawn(f)
            njt.say("after thread create")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2

--- response_body
before thread create
before sleep
after thread create
after sleep
--- no_error_log
[error]



=== TEST 4: two simple user threads with sleep
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("1: before sleep")
                njt.sleep(0.2)
                njt.say("1: after sleep")
            end

            local function g()
                njt.say("2: before sleep")
                njt.sleep(0.1)
                njt.say("2: after sleep")
            end

            njt.say("1: before thread create")
            njt.thread.spawn(f)
            njt.say("1: after thread create")

            njt.say("2: before thread create")
            njt.thread.spawn(g)
            njt.say("2: after thread create")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
terminate 1: ok
delete thread 1
terminate 3: ok
delete thread 3
terminate 2: ok
delete thread 2

--- wait: 0.1
--- response_body
1: before thread create
1: before sleep
1: after thread create
2: before thread create
2: before sleep
2: after thread create
2: after sleep
1: after sleep
--- no_error_log
[error]



=== TEST 5: error in user thread
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.blah()
            end

            njt.thread.spawn(f)
            njt.say("after")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 2: fail
terminate 1: ok
delete thread 2
delete thread 1

--- response_body
after
--- error_log eval
qr/lua user thread aborted: runtime error: content_by_lua\(nginx\.conf:\d+\):3: attempt to call field 'blah' \(a nil value\)/



=== TEST 6: simple user threads doing a single subrequest (entry quits early)
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("before capture")
                local res = njt.location.capture("/proxy")
                njt.say("after capture: ", res.body)
            end

            njt.say("before thread create")
            njt.thread.spawn(f)
            njt.say("after thread create")
        ';
    }

    location /proxy {
        proxy_pass http://127.0.0.1:$server_port/foo;
    }

    location /foo {
        echo_sleep 0.1;
        echo -n hello world;
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2

--- response_body
before thread create
before capture
after thread create
after capture: hello world
--- no_error_log
[error]



=== TEST 7: simple user threads doing a single subrequest (entry also does a subrequest and quits early)
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("before capture")
                local res = njt.location.capture("/proxy?foo")
                njt.say("after capture: ", res.body)
            end

            njt.say("before thread create")
            njt.thread.spawn(f)
            njt.say("after thread create")
            local res = njt.location.capture("/proxy?bar")
            njt.say("capture: ", res.body)
        ';
    }

    location /proxy {
        proxy_pass http://127.0.0.1:$server_port/$args;
    }

    location /foo {
        echo_sleep 0.1;
        echo -n hello foo;
    }

    location /bar {
        echo -n hello bar;
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2

--- response_body
before thread create
before capture
after thread create
capture: hello bar
after capture: hello foo
--- no_error_log
[error]



=== TEST 8: simple user threads doing a single subrequest (entry also does a subrequest and quits late)
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("before capture")
                local res = njt.location.capture("/proxy?foo")
                njt.say("after capture: ", res.body)
            end

            njt.say("before thread create")
            njt.thread.spawn(f)
            njt.say("after thread create")
            local res = njt.location.capture("/proxy?bar")
            njt.say("capture: ", res.body)
        ';
    }

    location /proxy {
        proxy_pass http://127.0.0.1:$server_port/$args;
    }

    location /foo {
        echo_sleep 0.1;
        echo -n hello foo;
    }

    location /bar {
        echo_sleep 0.2;
        echo -n hello bar;
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
terminate 1: ok
delete thread 2
delete thread 1

--- response_body
before thread create
before capture
after thread create
after capture: hello foo
capture: hello bar
--- no_error_log
[error]



=== TEST 9: two simple user threads doing single subrequests (entry also does a subrequest and quits between)
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("f: before capture")
                local res = njt.location.capture("/proxy?foo")
                njt.say("f: after capture: ", res.body)
            end

            local function g()
                njt.say("g: before capture")
                local res = njt.location.capture("/proxy?bah")
                njt.say("g: after capture: ", res.body)
            end

            njt.say("before thread 1 create")
            njt.thread.spawn(f)
            njt.say("after thread 1 create")

            njt.say("before thread 2 create")
            njt.thread.spawn(g)
            njt.say("after thread 2 create")

            local res = njt.location.capture("/proxy?bar")
            njt.say("capture: ", res.body)
        ';
    }

    location /proxy {
        proxy_pass http://127.0.0.1:$server_port/$args;
    }

    location /foo {
        echo_sleep 0.1;
        echo -n hello foo;
    }

    location /bar {
        echo_sleep 0.2;
        echo -n hello bar;
    }

    location /bah {
        echo_sleep 0.3;
        echo -n hello bah;
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
terminate 2: ok
terminate 1: ok
delete thread 2
delete thread 1
terminate 3: ok
delete thread 3

--- response_body
before thread 1 create
f: before capture
after thread 1 create
before thread 2 create
g: before capture
after thread 2 create
f: after capture: hello foo
capture: hello bar
g: after capture: hello bah
--- no_error_log
[error]



=== TEST 10: nested user threads
--- config
    location /lua {
        content_by_lua '
            local g
            local function f()
                njt.say("before g")
                njt.thread.spawn(g)
                njt.say("after g")
            end

            function g()
                njt.say("hello in g()")
            end

            njt.say("before f")
            njt.thread.spawn(f)
            njt.say("after f")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
create 3 in 2
spawn user thread 3 in 2
terminate 3: ok
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 3
delete thread 2

--- response_body
before f
before g
hello in g()
after f
after g
--- no_error_log
[error]



=== TEST 11: nested user threads (with I/O)
--- config
    location /lua {
        content_by_lua '
            local g
            local function f()
                njt.say("before g")
                njt.thread.spawn(g)
                njt.say("after g")
            end

            function g()
                njt.sleep(0.1)
                njt.say("hello in g()")
            end

            njt.say("before f")
            njt.thread.spawn(f)
            njt.say("after f")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
create 3 in 2
spawn user thread 3 in 2
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2
terminate 3: ok
delete thread 3

--- response_body
before f
before g
after f
after g
hello in g()
--- no_error_log
[error]



=== TEST 12: coroutine status of a running user thread
--- config
    location /lua {
        content_by_lua '
            local co
            local function f()
                co = coroutine.running()
                njt.sleep(0.1)
            end

            njt.thread.spawn(f)
            njt.say("status: ", coroutine.status(co))
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2

--- response_body
status: running
--- no_error_log
[error]



=== TEST 13: coroutine status of a dead user thread
--- config
    location /lua {
        content_by_lua '
            local co
            local function f()
                co = coroutine.running()
            end

            njt.thread.spawn(f)
            njt.say("status: ", coroutine.status(co))
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
terminate 1: ok
delete thread 2
delete thread 1

--- response_body
status: zombie
--- no_error_log
[error]



=== TEST 14: coroutine status of a "normal" user thread
--- config
    location /lua {
        content_by_lua '
            local co
            local g
            local function f()
                co = coroutine.running()
                local co2 = coroutine.create(g)
                coroutine.resume(co2)
            end

            function g()
                njt.sleep(0.1)
            end

            njt.thread.spawn(f)
            njt.say("status: ", coroutine.status(co))
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
create 3 in 2
terminate 1: ok
delete thread 1
terminate 3: ok
terminate 2: ok
delete thread 2

--- response_body
status: normal
--- no_error_log
[error]



=== TEST 15: creating user threads in a user coroutine
--- config
    location /lua {
        content_by_lua '
            local g
            local function f()
                njt.say("before g")
                njt.thread.spawn(g)
                njt.say("after g")
            end

            function g()
                njt.say("hello in g()")
            end

            njt.say("before f")
            local co = coroutine.create(f)
            coroutine.resume(co)
            njt.say("after f")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
create 3 in 2
spawn user thread 3 in 2
terminate 3: ok
terminate 2: ok
delete thread 3
terminate 1: ok
delete thread 1

--- response_body
before f
before g
hello in g()
after g
after f
--- no_error_log
[error]



=== TEST 16: manual time slicing between a user thread and the entry thread
--- config
    location /lua {
        content_by_lua '
            local yield = coroutine.yield

            local function f()
                local self = coroutine.running()
                njt.say("f 1")
                yield(self)
                njt.say("f 2")
                yield(self)
                njt.say("f 3")
            end

            local self = coroutine.running()
            njt.say("0")
            yield(self)
            njt.say("1")
            njt.thread.spawn(f)
            njt.say("2")
            yield(self)
            njt.say("3")
            yield(self)
            njt.say("4")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
terminate 1: ok
delete thread 2
delete thread 1

--- response_body
0
1
f 1
2
f 2
3
f 3
4
--- no_error_log
[error]



=== TEST 17: manual time slicing between two user threads
--- config
    location /lua {
        content_by_lua '
            local yield = coroutine.yield

            local function f()
                local self = coroutine.running()
                njt.say("f 1")
                yield(self)
                njt.say("f 2")
                yield(self)
                njt.say("f 3")
            end

            local function g()
                local self = coroutine.running()
                njt.say("g 1")
                yield(self)
                njt.say("g 2")
                yield(self)
                njt.say("g 3")
            end

            njt.thread.spawn(f)
            njt.thread.spawn(g)
            njt.say("done")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2
terminate 3: ok
delete thread 3

--- response_body
f 1
g 1
f 2
done
g 2
f 3
g 3
--- no_error_log
[error]



=== TEST 18: entry thread and a user thread flushing at the same time
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("hello in thread")
                coroutine.yield(coroutine.running)
                njt.flush(true)
            end

            njt.say("before")
            njt.thread.spawn(f)
            njt.say("after")
            njt.flush(true)
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2

--- response_body
before
hello in thread
after
--- no_error_log
[error]



=== TEST 19: two user threads flushing at the same time
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.say("hello from f")
                njt.flush(true)
            end

            local function g()
                njt.say("hello from g")
                njt.flush(true)
            end

            njt.thread.spawn(f)
            njt.thread.spawn(g)
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out_like
^(?:create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2
terminate 3: ok
delete thread 3|create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
create 3 in 1
spawn user thread 3 in 1
terminate 3: ok
terminate 1: ok
delete thread 2
delete thread 3
delete thread 1)$

--- response_body
hello from f
hello from g
--- no_error_log
[error]



=== TEST 20: user threads + njt.socket.tcp
--- config
    location /lua {
        content_by_lua '
            local function f()
                local sock = njt.socket.tcp()
                local ok, err = sock:connect("127.0.0.1", $TEST_NGINX_MEMCACHED_PORT)
                if not ok then
                    njt.say("failed to connect: ", err)
                    return
                end
                local bytes, err = sock:send("flush_all\\r\\n")
                if not bytes then
                    njt.say("failed to send query: ", err)
                    return
                end

                local line, err = sock:receive()
                if not line then
                    njt.say("failed to receive: ", err)
                    return
                end

                njt.say("received: ", line)
            end

            njt.say("before")
            njt.thread.spawn(f)
            njt.say("after")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2

--- response_body
before
after
received: OK
--- no_error_log
[error]



=== TEST 21: user threads + njt.socket.udp
--- config
    location /lua {
        content_by_lua '
            local function f()
                local sock = njt.socket.udp()
                local ok, err = sock:setpeername("127.0.0.1", 12345)
                local bytes, err = sock:send("blah")
                if not bytes then
                    njt.say("failed to send query: ", err)
                    return
                end

                local line, err = sock:receive()
                if not line then
                    njt.say("failed to receive: ", err)
                    return
                end

                njt.say("received: ", line)
            end

            njt.say("before")
            njt.thread.spawn(f)
            njt.say("after")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out_like chop
^(?:create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2
|create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
terminate 1: ok
delete thread 2
delete thread 1
)$

--- udp_listen: 12345
--- udp_query: blah
--- udp_reply: hello udp
--- response_body_like chop
^(?:before
after
received: hello udp
|before
received: hello udp
after)$

--- no_error_log
[error]



=== TEST 22: simple user thread with njt.req.read_body()
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.req.read_body()
                local body = njt.req.get_body_data()
                njt.say("body: ", body)
            end

            njt.say("before")
            njt.thread.spawn(f)
            njt.say("after")
        ';
    }
--- request
POST /lua
hello world
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out_like chop
^(?:create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
terminate 1: ok
delete thread 2
delete thread 1|create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2)$

--- response_body_like chop
^(?:before
body: hello world
after|before
after
body: hello world)$

--- no_error_log
[error]



=== TEST 23: simple user thread with njt.req.socket()
--- config
    location /lua {
        content_by_lua '
            local function f()
                local sock = njt.req.socket()
                local body, err = sock:receive(11)
                if not body then
                    njt.say("failed to read body: ", err)
                    return
                end

                njt.say("body: ", body)
            end

            njt.say("before")
            njt.thread.spawn(f)
            njt.say("after")
        ';
    }
--- request
POST /lua
hello world
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out_like chop
^(?:create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
terminate 1: ok
delete thread 2
delete thread 1|create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2)$

--- response_body_like chop
^(?:before
body: hello world
after|before
after
body: hello world)$

--- no_error_log
[error]



=== TEST 24: simple user thread with args
--- config
    location /lua {
        content_by_lua '
            local function f(a, b)
                njt.say("hello ", a, " and ", b)
            end

            njt.say("before")
            njt.thread.spawn(f, "foo", 3.14)
            njt.say("after")
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 2: ok
terminate 1: ok
delete thread 2
delete thread 1

--- response_body
before
hello foo and 3.14
after
--- no_error_log
[error]



=== TEST 25: multiple user threads + subrequests returning 404 immediately
--- config
    location /t {
        content_by_lua '
            local capture = njt.location.capture
            local insert = table.insert

            local function f(i)
                local res = capture("/proxy/" .. i)
                njt.say("status: ", res.status)
            end

            local threads = {}
            for i = 1, 2 do
                local co = njt.thread.spawn(f, i)
                insert(threads, co)
            end

            njt.say("ok")
        ';
    }

    location ~ ^/proxy/(\d+) {
        return 404;
    }
--- request
    GET /t
--- stap2 eval: $::StapScript
--- stap eval
"$::GCScript"
.
'
F(njt_http_finalize_request) {
    printf("finalize request %s: rc:%d c:%d a:%d\n", njt_http_req_uri($r), $rc, $r->main->count, $r == $r->main);
    #if ($rc == -1) {
        #print_ubacktrace()
    #}
}

M(http-subrequest-done) {
    printf("subrequest %s done\n", njt_http_req_uri($r))
}

F(njt_http_lua_post_subrequest) {
    printf("post subreq: %s rc=%d, status=%d a=%d\n", njt_http_req_uri($r), $rc,
         $r->headers_out->status, $r == $r->main)
    #print_ubacktrace()
}
'
--- stap_out_like chop
^create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
terminate 1: ok
delete thread 1
finalize request /t: rc:-4 c:4 a:1
finalize request /proxy/1: rc:404 c:3 a:0
post subreq: /proxy/1 rc=404, status=0 a=0
subrequest /proxy/1 done
terminate 2: ok
delete thread 2
finalize request /proxy/2: rc:404 c:2 a:0
post subreq: /proxy/2 rc=404, status=0 a=0
subrequest /proxy/2 done
terminate 3: ok
delete thread 3
finalize request /t: rc:0 c:1 a:1
(?:finalize request /t: rc:0 c:1 a:1)?$

--- response_body
ok
status: 404
status: 404
--- no_error_log
[error]
--- timeout: 3



=== TEST 26: multiple user threads + subrequests returning 404 remotely (no wait)
--- config
    location /t {
        content_by_lua '
            local capture = njt.location.capture
            local insert = table.insert

            local function f(i)
                local res = capture("/proxy/" .. i)
                njt.say("status: ", res.status)
            end

            local threads = {}
            for i = 1, 5 do
                local co = njt.thread.spawn(f, i)
                insert(threads, co)
            end

            njt.say("ok")
        ';
    }

    location ~ ^/proxy/(\d+) {
        proxy_pass http://127.0.0.1:$server_port/d/$1;
    }

    location /d {
        return 404;
        #echo $uri;
    }
--- request
    GET /t
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out_like chop
^create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
create 4 in 1
spawn user thread 4 in 1
create 5 in 1
spawn user thread 5 in 1
create 6 in 1
spawn user thread 6 in 1
terminate 1: ok
delete thread 1
(?:terminate 2: ok
delete thread 2
terminate 3: ok
delete thread 3
terminate 4: ok
delete thread 4
terminate 5: ok
delete thread 5
terminate 6: ok
delete thread 6
|terminate 6: ok
delete thread 6
terminate 5: ok
delete thread 5
terminate 4: ok
delete thread 4
terminate 3: ok
delete thread 3
terminate 2: ok
delete thread 2)$

--- response_body
ok
status: 404
status: 404
status: 404
status: 404
status: 404
--- no_error_log
[error]
--- timeout: 6



=== TEST 27: multiple user threads + subrequests returning 201 immediately
--- config
    location /t {
        content_by_lua '
            local capture = njt.location.capture
            local insert = table.insert

            local function f(i)
                local res = capture("/proxy/" .. i)
                njt.say("status: ", res.status)
            end

            local threads = {}
            for i = 1, 2 do
                local co = njt.thread.spawn(f, i)
                insert(threads, co)
            end

            njt.say("ok")
        ';
    }

    location ~ ^/proxy/(\d+) {
        content_by_lua 'njt.exit(201)';
    }
--- request
    GET /t
--- stap2 eval: $::StapScript
--- stap eval
"$::GCScript"
.
'
F(njt_http_finalize_request) {
    printf("finalize request %s: rc:%d c:%d a:%d\n", njt_http_req_uri($r), $rc, $r->main->count, $r == $r->main);
    #if ($rc == -1) {
        #print_ubacktrace()
    #}
}

M(http-subrequest-done) {
    printf("subrequest %s done\n", njt_http_req_uri($r))
}

F(njt_http_lua_post_subrequest) {
    printf("post subreq: %s rc=%d, status=%d a=%d\n", njt_http_req_uri($r), $rc,
         $r->headers_out->status, $r == $r->main)
    #print_ubacktrace()
}
'

--- stap_out_like chop
^create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
terminate 1: ok
delete thread 1
finalize request /t: rc:-4 c:4 a:1
terminate 4: ok
delete thread 4
finalize request /proxy/1: rc:0 c:3 a:0
post subreq: /proxy/1 rc=0, status=201 a=0
subrequest /proxy/1 done
terminate 2: ok
delete thread 2
terminate 5: ok
delete thread 5
finalize request /proxy/2: rc:0 c:2 a:0
post subreq: /proxy/2 rc=0, status=201 a=0
subrequest /proxy/2 done
terminate 3: ok
delete thread 3
finalize request /t: rc:0 c:1 a:1
(?:finalize request /t: rc:0 c:1 a:1)?$

--- response_body
ok
status: 201
status: 201
--- no_error_log
[error]
--- timeout: 3



=== TEST 28: multiple user threads + subrequests returning 204 immediately
--- config
    location /t {
        content_by_lua '
            local capture = njt.location.capture
            local insert = table.insert

            local function f(i)
                local res = capture("/proxy/" .. i)
                njt.say("status: ", res.status)
            end

            local threads = {}
            for i = 1, 2 do
                local co = njt.thread.spawn(f, i)
                insert(threads, co)
            end

            njt.say("ok")
        ';
    }

    location ~ ^/proxy/(\d+) {
        content_by_lua 'njt.exit(204)';
    }
--- request
    GET /t
--- stap2 eval: $::StapScript
--- stap eval
"$::GCScript"
.
'
F(njt_http_finalize_request) {
    printf("finalize request %s: rc:%d c:%d a:%d\n", njt_http_req_uri($r), $rc, $r->main->count, $r == $r->main);
    #if ($rc == -1) {
        #print_ubacktrace()
    #}
}

M(http-subrequest-done) {
    printf("subrequest %s done\n", njt_http_req_uri($r))
}

F(njt_http_lua_post_subrequest) {
    printf("post subreq: %s rc=%d, status=%d a=%d\n", njt_http_req_uri($r), $rc,
         $r->headers_out->status, $r == $r->main)
    #print_ubacktrace()
}
'
--- stap_out_like chop
^create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
terminate 1: ok
delete thread 1
finalize request /t: rc:-4 c:4 a:1
terminate 4: ok
delete thread 4
finalize request /proxy/1: rc:204 c:3 a:0
post subreq: /proxy/1 rc=204, status=204 a=0
subrequest /proxy/1 done
terminate 2: ok
delete thread 2
terminate 5: ok
delete thread 5
finalize request /proxy/2: rc:204 c:2 a:0
post subreq: /proxy/2 rc=204, status=204 a=0
subrequest /proxy/2 done
terminate 3: ok
delete thread 3
finalize request /t: rc:0 c:1 a:1
(?:finalize request /t: rc:0 c:1 a:1)?$

--- response_body
ok
status: 204
status: 204
--- no_error_log
[error]
--- timeout: 3



=== TEST 29: multiple user threads + subrequests returning 404 remotely (wait)
--- config
    location /t {
        content_by_lua '
            local n = 5
            local capture = njt.location.capture
            local insert = table.insert

            local function f(i)
                local res = capture("/proxy/" .. i)
                return res.status
            end

            local threads = {}
            for i = 1, n do
                local co = njt.thread.spawn(f, i)
                insert(threads, co)
            end

            for i = 1, n do
                local ok, res = njt.thread.wait(threads[i])
                njt.say(i, ": ", res)
            end

            njt.say("ok")
        ';
    }

    location ~ ^/proxy/(\d+) {
        proxy_pass http://127.0.0.1:$server_port/d/$1;
    }

    location /d {
        return 404;
        #echo $uri;
    }
--- request
    GET /t
--- stap2 eval: $::StapScript
--- stap3 eval: $::GCScript
--- stap_out3
create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
create 4 in 1
spawn user thread 4 in 1
create 5 in 1
spawn user thread 5 in 1
create 6 in 1
spawn user thread 6 in 1
terminate 2: ok
delete thread 2
terminate 3: ok
delete thread 3
terminate 4: ok
delete thread 4
terminate 5: ok
delete thread 5
terminate 6: ok
delete thread 6
terminate 1: ok
delete thread 1

--- response_body
1: 404
2: 404
3: 404
4: 404
5: 404
ok
--- no_error_log
[error]
--- timeout: 6



=== TEST 30: multiple user threads + subrequests remotely (wait)
--- config
    location /t {
        content_by_lua '
            local n = 20
            local capture = njt.location.capture
            local insert = table.insert

            local function f(i)
                local res = capture("/proxy/" .. i)
                return res.status
            end

            local threads = {}
            for i = 1, n do
                local co = njt.thread.spawn(f, i)
                insert(threads, co)
            end

            for i = 1, n do
                local ok, res = njt.thread.wait(threads[i])
                njt.say(i, ": ", res)
            end

            njt.say("ok")
        ';
    }

    location ~ ^/proxy/(\d+) {
        proxy_pass http://127.0.0.1:$server_port/d/$1;
    }

    location /d {
        echo_sleep 0.001;
        echo $uri;
    }
--- request
    GET /t
--- stap2 eval: $::StapScript
--- stap3 eval: $::GCScript
--- stap_out3
create 2 in 1
spawn user thread 2 in 1
create 3 in 1
spawn user thread 3 in 1
create 4 in 1
spawn user thread 4 in 1
create 5 in 1
spawn user thread 5 in 1
create 6 in 1
spawn user thread 6 in 1
terminate 2: ok
delete thread 2
terminate 3: ok
delete thread 3
terminate 4: ok
delete thread 4
terminate 5: ok
delete thread 5
terminate 6: ok
delete thread 6
terminate 1: ok
delete thread 1

--- response_body
1: 200
2: 200
3: 200
4: 200
5: 200
6: 200
7: 200
8: 200
9: 200
10: 200
11: 200
12: 200
13: 200
14: 200
15: 200
16: 200
17: 200
18: 200
19: 200
20: 200
ok
--- no_error_log
[error]
[alert]
--- timeout: 10



=== TEST 31: simple user thread without I/O
--- config
    location /lua {
        content_by_lua '
            local function f()
                njt.sleep(0.1)
                njt.say("f")
            end

            njt.thread.spawn(f)
            collectgarbage()
        ';
    }
--- request
GET /lua
--- stap2 eval: $::StapScript
--- stap eval: $::GCScript
--- stap_out
create 2 in 1
spawn user thread 2 in 1
terminate 1: ok
delete thread 1
terminate 2: ok
delete thread 2

--- response_body
f
--- no_error_log
[error]
