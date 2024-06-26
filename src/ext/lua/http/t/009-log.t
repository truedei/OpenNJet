# vim:set ft= ts=4 sw=4 et fdm=marker:

use Test::Nginx::Socket::Lua;

#worker_connections(1014);
#master_process_enabled(1);
log_level('debug'); # to ensure any log-level can be outputted

repeat_each(2);

plan tests => repeat_each() * (blocks() * 3 + 4);

#no_diff();
#no_long_string();
run_tests();

__DATA__

=== TEST 1: test log-level STDERR
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            njt.log(njt.STDERR, "hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 2: test log-level EMERG
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            njt.log(njt.EMERG, "hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[emerg\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 3: test log-level ALERT
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            njt.log(njt.ALERT, "hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[alert\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 4: test log-level CRIT
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            njt.log(njt.CRIT, "hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[crit\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 5: test log-level ERR
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            njt.log(njt.ERR, "hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[error\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 6: test log-level WARN
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            njt.log(njt.WARN, "hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[warn\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 7: test log-level NOTICE
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            njt.log(njt.NOTICE, "hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[notice\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 8: test log-level INFO
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            njt.log(njt.INFO, "hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[info\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 9: test log-level DEBUG
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            njt.log(njt.DEBUG, "hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[debug\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 10: regression test print()
--- config
    location /log {
        content_by_lua '
            njt.say("before log")
            print("hello, log", 1234, 3.14159)
            njt.say("after log")
        ';
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[notice\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: hello, log12343.14159/



=== TEST 11: print(nil)
--- config
    location /log {
        content_by_lua '
            print()
            print(nil)
            print("nil: ", nil)
            njt.say("hi");
        ';
    }
--- request
GET /log
--- response_body
hi
--- error_log eval
[
qr/\[lua\] content_by_lua\(nginx\.conf:\d+\):2: ,/,
qr/\[lua\] content_by_lua\(nginx\.conf:\d+\):3: nil,/,
qr/\[lua\] content_by_lua\(nginx\.conf:\d+\):4: nil: nil,/,
]



=== TEST 12: njt.log in set_by_lua
--- config
    location /log {
        set_by_lua $a '
            njt.log(njt.ERR, "HELLO")
            return 32;
        ';
        echo $a;
    }
--- request
GET /log
--- response_body
32
--- error_log eval
qr/\[error\] \S+: \S+ \[lua\] set_by_lua:2: HELLO,/



=== TEST 13: test booleans and nil
--- config
    location /log {
        set_by_lua $a '
            njt.log(njt.ERR, true, false, nil)
            return 32;
        ';
        echo $a;
    }
--- request
GET /log
--- response_body
32
--- error_log eval
qr/\[error\] \S+: \S+ \[lua\] set_by_lua:2: truefalsenil,/



=== TEST 14: test table with metamethod
--- config
    location /log {
        content_by_lua_block {
            njt.say("before log")
            local t = setmetatable({v = "value"}, {
                __tostring = function(self)
                    return "tostring "..self.v
                end
            })
            njt.log(njt.ERR, t)
            njt.say("after log")
        }
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[error\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):8: tostring value,/



=== TEST 15: test table without metamethod
--- config
    location /log {
        content_by_lua_block {
            njt.log(njt.ERR, {})
            njt.say("done")
        }
    }
--- request
GET /log
--- response_body_like: 500 Internal Server Error
--- error_code: 500
--- error_log
bad argument #1 to 'log' (expected table to have __tostring metamethod)



=== TEST 16: test tables mixed with other types
--- config
    location /log {
        content_by_lua_block {
            njt.say("before log")
            local t = setmetatable({v = "value"}, {
                __tostring = function(self)
                    return "tostring: "..self.v
                end
            })
            njt.log(njt.ERR, t, " hello ", t)
            njt.say("after log")
        }
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[error\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):8: tostring: value hello tostring: value,/



=== TEST 17: test print with tables
--- config
    location /log {
        content_by_lua_block {
            njt.say("before log")
            local t = setmetatable({v = "value"}, {
                __tostring = function(self)
                    return "tostring: "..self.v
                end
            })
            print(t, " hello ", t)
            njt.say("after log")
        }
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[notice\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):8: tostring: value hello tostring: value,/



=== TEST 18: print() in header filter
--- config
    location /log {
        header_filter_by_lua '
            print("hello world")
            njt.header.foo = 32
        ';
        echo hi;
    }
--- request
GET /log
--- response_headers
foo: 32
--- error_log eval
qr/\[notice\] .*? \[lua\] header_filter_by_lua:2: hello world/
--- response_body
hi



=== TEST 19: njt.log in header filter
--- config
    location /log {
        header_filter_by_lua '
            njt.log(njt.ERR, "howdy, lua!")
            njt.header.foo = 32
        ';
        echo hi;
    }
--- request
GET /log
--- response_headers
foo: 32
--- response_body
hi
--- error_log eval
qr/\[error\] .*? \[lua\] header_filter_by_lua:2: howdy, lua!/



=== TEST 20: njt.log big data
--- config
    location /log {
        content_by_lua '
            njt.log(njt.ERR, "a" .. string.rep("h", 1970) .. "b")
            njt.say("hi")
        ';
    }
--- request
GET /log
--- response_headers
--- error_log eval
[qr/ah{1970}b/]



=== TEST 21: njt.log in Lua function calls & inlined lua
--- config
    location /log {
        content_by_lua '
            local foo
            local bar
            function foo()
                bar()
            end

            function bar()
                njt.log(njt.ERR, "hello, log", 1234, 3.14159)
            end

            foo()
            njt.say("done")
        ';
    }
--- request
GET /log
--- response_body
done
--- error_log eval
qr/\[error\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):9: bar\(\): hello, log12343.14159/



=== TEST 22: njt.log in Lua function tail-calls & inlined lua
--- config
    location /log {
        content_by_lua '
            local foo
            local bar
            function foo()
                return bar(5)
            end

            function bar(n)
                if n < 1 then
                    njt.log(njt.ERR, "hello, log", 1234, 3.14159)
                    return n
                end

                return bar(n - 1)
            end

            foo()
            njt.say("done")
        ';
    }
--- request
GET /log
--- response_body
done
--- error_log eval
qr/\[error\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):10:(?: foo\(\):)? hello, log12343.14159/



=== TEST 23: njt.log in Lua files
--- config
    location /log {
        content_by_lua_file 'html/test.lua';
    }
--- user_files
>>> test.lua
local foo
local bar
function foo()
    bar()
end

function bar()
    njt.log(njt.ERR, "hello, log", 1234, 3.14159)
end

foo()
njt.say("done")

--- request
GET /log
--- response_body
done
--- error_log eval
qr/\[error\] \S+: \S+ \[lua\] test.lua:8: bar\(\): hello, log12343.14159/



=== TEST 24: njt.log with bad levels (njt.ERROR, -1)
--- config
    location /log {
        content_by_lua '
            njt.log(njt.ERROR, "hello lua")
            njt.say("done")
        ';
    }
--- request
GET /log
--- response_body_like: 500 Internal Server Error
--- error_code: 500
--- error_log
bad log level: -1



=== TEST 25: njt.log with bad levels (9)
--- config
    location /log {
        content_by_lua '
            njt.log(9, "hello lua")
            njt.say("done")
        ';
    }
--- request
GET /log
--- response_body_like: 500 Internal Server Error
--- error_code: 500
--- error_log
bad log level: 9



=== TEST 26: \0 in the log message
--- config
    location = /t {
        content_by_lua '
            njt.log(njt.WARN, "hello\\0world")
            njt.say("ok")
        ';
    }
--- request
GET /t
--- response_body
ok
--- no_error_log
[error]
--- error_log eval
"2: hello\0world, client: "



=== TEST 27: test log-level STDERR
Note: maximum number of digits after the decimal-point character is 13
--- config
    location /log {
        content_by_lua_block {
            njt.say("before log")
            njt.log(njt.STDERR, 3.14159265357939723846)
            njt.say("after log")
        }
    }
--- request
GET /log
--- response_body
before log
after log
--- error_log eval
qr/\[\] \S+: \S+ \[lua\] content_by_lua\(nginx\.conf:\d+\):3: 3.1415926535794/
