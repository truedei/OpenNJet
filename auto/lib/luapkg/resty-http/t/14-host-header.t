use Test::Nginx::Socket 'no_plan';
use Cwd qw(cwd);

my $pwd = cwd();

$ENV{TEST_NGINX_RESOLVER} = '8.8.8.8';
$ENV{TEST_NGINX_PWD} ||= $pwd;
$ENV{TEST_COVERAGE} ||= 0;

our $HttpConfig = qq{
    lua_package_path "$pwd/lib/?.lua;/usr/local/share/lua/5.1/?.lua;;";
    error_log logs/error.log debug;
    resolver 8.8.8.8 ipv6=off;

    init_by_lua_block {
        if $ENV{TEST_COVERAGE} == 1 then
            jit.off()
            require("luacov.runner").init()
        end

        require("resty.http").debug(true)
    }
};

sub read_file {
    my $infile = shift;
    open my $in, $infile
        or die "cannot open $infile for reading: $!";
    my $cert = do { local $/; <$in> };
    close $in;
    $cert;
}

our $TestCertificate = read_file("t/cert/test.crt");
our $TestCertificateKey = read_file("t/cert/test.key");

no_long_string();
#no_diff();

run_tests();

__DATA__
=== TEST 1: Default HTTP port is not added to Host header
--- http_config eval: $::HttpConfig
--- config
    location /lua {
        content_by_lua '
            local http = require "resty.http"
            local httpc = http.new()

            local res, err = httpc:request_uri("http://www.google.com")
        ';
    }
--- request
GET /lua
--- no_error_log
[error]
--- error_log
Host: www.google.com


=== TEST 2: Default HTTPS port is not added to Host header
--- http_config eval: $::HttpConfig
--- config
    location /lua {
        content_by_lua '
            local http = require "resty.http"
            local httpc = http.new()

            httpc:set_timeouts(300, 1000, 1000)
            local res, err = httpc:request_uri("https://www.google.com:443", { ssl_verify = false })
        ';
    }
--- request
GET /lua
--- no_error_log
[error]
--- error_log
Host: www.google.com


=== TEST 3: Non-default HTTP port is added to Host header
--- http_config
    lua_package_path "$TEST_NGINX_PWD/lib/?.lua;;";
    error_log logs/error.log debug;
    resolver 8.8.8.8;
    server {
        listen *:8080;
    }
--- config
    location /lua {
        content_by_lua '
            require("resty.http").debug(true)
            local http = require "resty.http"
            local httpc = http.new()

            local res, err = httpc:request_uri("http://127.0.0.1:8080")
        ';
    }
--- request
GET /lua
--- no_error_log
[error]
--- error_log
Host: 127.0.0.1:8080


=== TEST 4: Non-default HTTPS port is added to Host header
--- http_config
    lua_package_path "$TEST_NGINX_PWD/lib/?.lua;;";
    error_log logs/error.log debug;
    resolver 8.8.8.8;
    server {
        listen *:8080;
        listen *:8081 ssl;
        ssl_certificate ../html/test.crt;
        ssl_certificate_key ../html/test.key;
    }
--- config
    location /lua {
        content_by_lua '
            require("resty.http").debug(true)
            local http = require "resty.http"
            local httpc = http.new()

            local res, err = httpc:request_uri("https://127.0.0.1:8081", { ssl_verify = false })
        ';
    }
--- user_files eval
">>> test.key
$::TestCertificateKey
>>> test.crt
$::TestCertificate"
--- request
GET /lua
--- no_error_log
[error]
--- error_log
Host: 127.0.0.1:8081


=== TEST 5: No host header on a unix domain socket returns a useful error.
--- http_config eval: $::HttpConfig
--- config
    location /a {
        content_by_lua_block {
            local http = require "resty.http"
            local httpc = http.new()

            local res, err = httpc:connect("unix:.test.sock")
            if not res then
                njt.log(njt.ERR, err)
            end

            local res, err = httpc:request({ path = "/" })
            if not res then
                njt.say(err)
            else
                njt.say(res:read_body())
            end
        }
    }
--- tcp_listen: .test.sock
--- tcp_reply: OK
--- request
GET /a
--- no_error_log
[error]
--- response_body
Unable to generate a useful Host header for a unix domain socket. Please provide one.

=== TEST 6: Host header is correct when http_proxy is used
--- http_config
    lua_package_path "$TEST_NGINX_PWD/lib/?.lua;;";
    error_log logs/error.log debug;
    resolver 8.8.8.8;
    server {
        listen *:8080;
    }

--- config
    location /lua {
        content_by_lua '
            require("resty.http").debug(true)
            local http = require "resty.http"
            local httpc = http.new()
            httpc:set_proxy_options({
                http_proxy = "http://127.0.0.1:8080"
            })
            local res, err = httpc:request_uri("http://127.0.0.1:8081")
        ';
    }
--- request
GET /lua
--- no_error_log
[error]
--- error_log
Host: 127.0.0.1:8081
