# vim:set ft= ts=4 sw=4 et fdm=marker:
use Test::Nginx::Socket::Lua;

#worker_connections(1014);
#master_on();
#workers(2);
#log_level('warn');

repeat_each(5);

plan tests => repeat_each() * (blocks() * 2 + 7);

our $HtmlDir = html_dir;

#no_diff();
#no_long_string();
run_tests();

__DATA__

=== TEST 1: gmatch
--- config
    location /re {
        content_by_lua '
            for m in njt.re.gmatch("hello, world", "[a-z]+") do
                if m then
                    njt.say(m[0])
                else
                    njt.say("not matched: ", m)
                end
            end
        ';
    }
--- request
    GET /re
--- response_body
hello
world



=== TEST 2: fail to match
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, world", "[0-9]")
            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end

            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end

            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end
        ';
    }
--- request
    GET /re
--- response_body
nil
nil
nil



=== TEST 3: match but iterate more times (not just match at the end)
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, world!", "[a-z]+")
            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end

            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end

            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end

            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end
        ';
    }
--- request
    GET /re
--- response_body
hello
world
nil
nil



=== TEST 4: match but iterate more times (just matched at the end)
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, world", "[a-z]+")
            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end

            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end

            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end

            local m = it()
            if m then njt.say(m[0]) else njt.say(m) end
        ';
    }
--- request
    GET /re
--- response_body
hello
world
nil
nil



=== TEST 5: anchored match (failed)
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, 1234", "([0-9]+)", "a")
            njt.say(it())
        ';
    }
--- request
    GET /re
--- response_body
nil



=== TEST 6: anchored match (succeeded)
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("12 hello 34", "[0-9]", "a")
            local m = it()
            njt.say(m[0])
            m = it()
            njt.say(m[0])
            njt.say(it())
        ';
    }
--- request
    GET /re
--- response_body
1
2
nil



=== TEST 7: non-anchored gmatch (without regex cache)
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("12 hello 34", "[0-9]")
            local m = it()
            njt.say(m and m[0])
            m = it()
            njt.say(m and m[0])
            m = it()
            njt.say(m and m[0])
            m = it()
            njt.say(m and m[0])
            m = it()
            njt.say(m and m[0])
        ';
    }
--- request
    GET /re
--- response_body
1
2
3
4
nil



=== TEST 8: non-anchored gmatch (with regex cache)
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("12 hello 34", "[0-9]", "o")
            local m = it()
            njt.say(m and m[0])
            m = it()
            njt.say(m and m[0])
            m = it()
            njt.say(m and m[0])
            m = it()
            njt.say(m and m[0])
            m = it()
            njt.say(m and m[0])
        ';
    }
--- request
    GET /re
--- response_body
1
2
3
4
nil



=== TEST 9: anchored match (succeeded)
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("12 hello 34", "[0-9]", "a")
            local m = it()
            njt.say(m[0])
            m = it()
            njt.say(m[0])
            njt.say(it())
        ';
    }
--- request
    GET /re
--- response_body
1
2
nil



=== TEST 10: anchored match (succeeded, set_by_lua)
--- config
    location /re {
        set_by_lua $res '
            local it = njt.re.gmatch("12 hello 34", "[0-9]", "a")
            local m = it()
            return m[0]
        ';
        echo $res;
    }
--- request
    GET /re
--- response_body
1



=== TEST 11: gmatch (look-behind assertion)
--- config
    location /re {
        content_by_lua '
            for m in njt.re.gmatch("{foobar}, {foobaz}", "(?<=foo)ba[rz]") do
                if m then
                    njt.say(m[0])
                else
                    njt.say("not matched: ", m)
                end
            end
        ';
    }
--- request
    GET /re
--- response_body
bar
baz



=== TEST 12: gmatch (look-behind assertion 2)
--- config
    location /re {
        content_by_lua '
            for m in njt.re.gmatch("{foobarbaz}", "(?<=foo)bar|(?<=bar)baz") do
                if m then
                    njt.say(m[0])
                else
                    njt.say("not matched: ", m)
                end
            end
        ';
    }
--- request
    GET /re
--- response_body
bar
baz



=== TEST 13: with regex cache
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, 1234", "([A-Z]+)", "io")
            local m = it()
            njt.say(m and m[0])

            it = njt.re.gmatch("1234, okay", "([A-Z]+)", "io")
            m = it()
            njt.say(m and m[0])

            it = njt.re.gmatch("hi, 1234", "([A-Z]+)", "o")
            m = it()
            njt.say(m and m[0])
        ';
    }
--- request
    GET /re
--- stap2
F(njt_http_lua_njt_re_gmatch_iterator) { println("iterator") }
F(njt_http_lua_njt_re_gmatch_gc) { println("gc") }
F(njt_http_lua_njt_re_gmatch_cleanup) { println("cleanup") }
--- response_body
hello
okay
nil



=== TEST 14: exceeding regex cache max entries
--- http_config
    lua_regex_cache_max_entries 2;
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, 1234", "([0-9]+)", "o")
            local m = it()
            njt.say(m and m[0])

            it = njt.re.gmatch("howdy, 567", "([0-9]+)", "oi")
            m = it()
            njt.say(m and m[0])

            it = njt.re.gmatch("hiya, 98", "([0-9]+)", "ox")
            m = it()
            njt.say(m and m[0])
        ';
    }
--- request
    GET /re
--- response_body
1234
567
98



=== TEST 15: disable regex cache completely
--- http_config
    lua_regex_cache_max_entries 0;
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, 1234", "([0-9]+)", "o")
            local m = it()
            njt.say(m and m[0])

            it = njt.re.gmatch("howdy, 567", "([0-9]+)", "oi")
            local m = it()
            njt.say(m and m[0])

            it = njt.re.gmatch("hiya, 98", "([0-9]+)", "ox")
            local m = it()
            njt.say(m and m[0])
        ';
    }
--- request
    GET /re
--- response_body
1234
567
98



=== TEST 16: gmatch matched but no iterate
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, world", "[a-z]+")
            njt.say("done")
        ';
    }
--- request
    GET /re
--- response_body
done



=== TEST 17: gmatch matched but only iterate once and still matches remain
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, world", "[a-z]+")
            local m = it()
            if m then
                njt.say(m[0])
            else
                njt.say("not matched")
            end
        ';
    }
--- request
    GET /re
--- response_body
hello



=== TEST 18: gmatch matched but no iterate and early forced GC
--- config
    location /re {
        content_by_lua '
            local a = {}
            for i = 1, 3 do
                local it = njt.re.gmatch("hello, world", "[a-z]+")
                it()
                collectgarbage()
                table.insert(a, {"hello", "world"})
            end
            njt.say("done")
        ';
    }
--- request
    GET /re
--- response_body
done



=== TEST 19: gmatch iterator used by another request
--- http_config eval
    "lua_package_path '$::HtmlDir/?.lua;;';"
--- config
    location /main {
        content_by_lua '
            package.loaded.foo = nil
            collectgarbage()

            local res = njt.location.capture("/t")
            if res.status == 200 then
                njt.print(res.body)
            else
                njt.say("sr failed: ", res.status)
            end

            res = njt.location.capture("/t")
            if res.status == 200 then
                njt.print(res.body)
            else
                njt.say("sr failed: ", res.status)
            end
        ';
    }

    location /t {
        content_by_lua '
            local foo = require "foo"
            local m = foo.go()
            njt.say(m and "matched" or "no")
        ';
    }
--- user_files
>>> foo.lua
module("foo", package.seeall)

local it

function go()
    if not it then
        it = njt.re.gmatch("hello, world", "[a-z]+")
    end

    return it()
end
--- request
    GET /main
--- response_body
matched
matched
--- no_error_log
[error]



=== TEST 20: gmatch (empty matched string)
--- config
    location /re {
        content_by_lua '
            for m in njt.re.gmatch("hello", "a|") do
                if m then
                    njt.say("matched: [", m[0], "]")
                else
                    njt.say("not matched: ", m)
                end
            end
        ';
    }
--- request
    GET /re
--- response_body
matched: []
matched: []
matched: []
matched: []
matched: []
matched: []



=== TEST 21: gmatch with named pattern
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("1234, 1234", "(?<first>[0-9]+)")
            local m = it()
            if m then
                njt.say(m[0])
                njt.say(m[1])
                njt.say(m["first"])
            else
                njt.say("not matched!")
            end

            m = it()
            if m then
                njt.say(m[0])
                njt.say(m[1])
                njt.say(m["first"])
            else
                njt.say("not matched!")
            end
        ';
    }
--- request
    GET /re
--- response_body
1234
1234
1234
1234
1234
1234



=== TEST 22: gmatch with multiple named pattern
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("1234, abcd, 1234", "(?<first>[0-9]+)|(?<second>[a-z]+)")

            local m = it()
            if m then
                njt.say(m[0])
                njt.say(m[1])
                njt.say(m[2])
                njt.say(m["first"])
                njt.say(m["second"])
            else
                njt.say("not matched!")
            end

            m = it()
            if m then
                njt.say(m[0])
                njt.say(m[1])
                njt.say(m[2])
                njt.say(m["first"])
                njt.say(m["second"])
            else
                njt.say("not matched!")
            end
        ';
    }
--- request
    GET /re
--- response_body
1234
1234
false
1234
false
abcd
false
abcd
false
abcd



=== TEST 23: gmatch with duplicate named pattern w/ extraction
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("hello, 1234", "(?<first>[a-z]+), (?<first>[0-9]+)", "D")
            local m = it()
            if m then
                njt.say(m[0])
                njt.say(m[1])
                njt.say(m[2])
                njt.say(table.concat(m.first,"-"))
            else
                njt.say("not matched!")
            end

            m = it()
            if m then
             njt.say(m[0])
                njt.say(m[1])
                njt.say(m[2])
                njt.say(table.concat(m.first,"-"))
            else
                njt.say("not matched!")
            end
        ';
    }
--- request
    GET /re
--- response_body
hello, 1234
hello
1234
hello-1234
not matched!



=== TEST 24: named captures are empty
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("1234", "(?<first>[a-z]*)([0-9]+)", "")
            local m = it()
            if m then
                njt.say(m[0])
                njt.say(m.first)
                njt.say(m[1])
                njt.say(m[2])
            else
                njt.say("not matched!")
            end
        ';
    }
--- request
    GET /re
--- response_body
1234


1234



=== TEST 25: named captures are empty (with regex cache)
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("1234", "(?<first>[a-z]*)([0-9]+)", "o")
            local m = it()
            if m then
                njt.say(m[0])
                njt.say(m.first)
                njt.say(m[1])
                njt.say(m[2])
            else
                njt.say("not matched!")
            end
        ';
    }
--- request
    GET /re
--- response_body
1234


1234



=== TEST 26: bad pattern
--- config
    location /re {
        content_by_lua '
            local it, err = njt.re.gmatch("hello\\nworld", "(abc")
            if not err then
                njt.say("good")

            else
                njt.say("error: ", err)
            end
        ';
    }
--- request
    GET /re
--- response_body
error: pcre_compile() failed: missing ) in "(abc"
--- no_error_log
[error]



=== TEST 27: bad UTF-8
--- config
    location = /t {
        content_by_lua '
            local target = "你好"
            local regex = "你好"

            -- Note the D here
            local it, err = njt.re.gmatch(string.sub(target, 1, 4), regex, "u")

            if err then
                njt.say("error: ", err)
                return
            end

            local m, err = it()
            if err then
                njt.say("error: ", err)
                return
            end

            if m then
                njt.say("matched: ", m[0])
            else
                njt.say("not matched")
            end
        ';
    }
--- request
GET /t
--- response_body_like chop
error: pcre_exec\(\) failed: -10

--- no_error_log
[error]



=== TEST 28: UTF-8 mode without UTF-8 sequence checks
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("你好", ".", "U")
            local m = it()
            if m then
                njt.say(m[0])
            else
                njt.say("not matched!")
            end
        ';
    }
--- stap
probe process("$LIBPCRE_PATH").function("pcre_compile") {
    printf("compile opts: %x\n", $options)
}

probe process("$LIBPCRE_PATH").function("pcre_exec") {
    printf("exec opts: %x\n", $options)
}

--- stap_out
compile opts: 800
exec opts: 2000

--- request
    GET /re
--- response_body
你
--- no_error_log
[error]



=== TEST 29: UTF-8 mode with UTF-8 sequence checks
--- config
    location /re {
        content_by_lua '
            local it = njt.re.gmatch("你好", ".", "u")
            local m = it()
            if m then
                njt.say(m[0])
            else
                njt.say("not matched!")
            end
        ';
    }
--- stap
probe process("$LIBPCRE_PATH").function("pcre_compile") {
    printf("compile opts: %x\n", $options)
}

probe process("$LIBPCRE_PATH").function("pcre_exec") {
    printf("exec opts: %x\n", $options)
}

--- stap_out
compile opts: 800
exec opts: 0

--- request
    GET /re
--- response_body
你
--- no_error_log
[error]



=== TEST 30: just hit match limit
--- http_config
    lua_regex_match_limit 5000;
--- config
    location /re {
        content_by_lua_file html/a.lua;
    }

--- user_files
>>> a.lua
local re = [==[(?i:([\s'\"`´’‘\(\)]*)?([\d\w]+)([\s'\"`´’‘\(\)]*)?(?:=|<=>|r?like|sounds\s+like|regexp)([\s'\"`´’‘\(\)]*)?\2|([\s'\"`´’‘\(\)]*)?([\d\w]+)([\s'\"`´’‘\(\)]*)?(?:!=|<=|>=|<>|<|>|\^|is\s+not|not\s+like|not\s+regexp)([\s'\"`´’‘\(\)]*)?(?!\6)([\d\w]+))]==]

local s = string.rep([[ABCDEFG]], 10)

local start = njt.now()

local it, err = njt.re.gmatch(s, re, "o")
if not it then
    njt.say("failed to gen iterator: ", err)
    return
end

local res, err = it()

--[[
njt.update_time()
local elapsed = njt.now() - start
njt.say(elapsed, " sec elapsed.")
]]

if not res then
    if err then
        njt.say("error: ", err)
        return
    end
    njt.say("failed to match")
    return
end

--- request
    GET /re
--- response_body
error: pcre_exec() failed: -8



=== TEST 31: just not hit match limit
--- http_config
    lua_regex_match_limit 5100;
--- config
    location /re {
        content_by_lua_file html/a.lua;
    }

--- user_files
>>> a.lua
local re = [==[(?i:([\s'\"`´’‘\(\)]*)?([\d\w]+)([\s'\"`´’‘\(\)]*)?(?:=|<=>|r?like|sounds\s+like|regexp)([\s'\"`´’‘\(\)]*)?\2|([\s'\"`´’‘\(\)]*)?([\d\w]+)([\s'\"`´’‘\(\)]*)?(?:!=|<=|>=|<>|<|>|\^|is\s+not|not\s+like|not\s+regexp)([\s'\"`´’‘\(\)]*)?(?!\6)([\d\w]+))]==]

local s = string.rep([[ABCDEFG]], 10)

local start = njt.now()

local it, err = njt.re.gmatch(s, re, "o")
if not it then
    njt.say("failed to gen iterator: ", err)
    return
end

local res, err = it()

--[[
njt.update_time()
local elapsed = njt.now() - start
njt.say(elapsed, " sec elapsed.")
]]

if not res then
    if err then
        njt.say("error: ", err)
        return
    end
    njt.say("failed to match")
    return
end

--- request
    GET /re
--- response_body
failed to match
