var assert = require('assert');
var PicoHTTPParser = require('../');

describe('parse_http_request', function() {
  it('example', function() {
    var env = {};
    var req = "GET /blakjsdfkas HTTP/1.1\r\n";
    req += "Host: blooperblorp\r\n";
    req += "Cookie: blah=woop\r\n\r\n";

    var ret = PicoHTTPParser.parse_http_request(req, env);

    assert.equal('/blakjsdfkas', env.PATH_INFO);
    assert.equal('', env.QUERY_STRING);
    assert.equal('GET', env.REQUEST_METHOD);
    assert.equal('/blakjsdfkas', env.REQUEST_URI);
    assert.equal('', env.SCRIPT_NAME);
    assert.equal('HTTP/1.1', env.SERVER_PROTOCOL);
  });

  it('result of GET /', function() {
    var env = {};
    var ret = PicoHTTPParser.parse_http_request("GET /abc?x=%79 HTTP/1.0\r\n\r\n", env);

    assert.equal('/abc', env.PATH_INFO);
    assert.equal('x=%79', env.QUERY_STRING);
    assert.equal('GET', env.REQUEST_METHOD);
    assert.equal('/abc?x=%79', env.REQUEST_URI);
    assert.equal('', env.SCRIPT_NAME);
    assert.equal('HTTP/1.0', env.SERVER_PROTOCOL);
  });

  it.skip('result of POST with headers', function() {
    var env = {};
    var req = "POST /hoge HTTP/1.1\r\n";
    req += "Content-Type: text/plain\r\n";
    req += "Content-Length: 15\r\n";
    req += "Host: example.com\r\n";
    req += "User-Agent: hoge\r\n\r\n";

    var ret = PicoHTTPParser.parse_http_request(req, env);

    assert.equal('15', env.CONTENT_LENGTH);
    assert.equal('text/plain', env.CONTENT_TYPE);
    assert.equal('example.com', env.HTTP_HOST);
    assert.equal('hoge', env.HTTP_USER_AGENT);
    assert.equal('/hoge', env.PATH_INFO);
    assert.equal('', env.QUERY_STRING);
    assert.equal('POST', env.REQUEST_METHOD);
    assert.equal('/hoge', env.REQUEST_URI);
    assert.equal('', env.SCRIPT_NAME);
    assert.equal('HTTP/1.1', env.SERVER_PROTOCOL);
  });
});
