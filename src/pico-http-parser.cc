#include <cstdlib>
#include <ctype.h>
#include <nan.h>
#include "picohttpparser/picohttpparser.c"
#include <stdio.h>

using namespace v8;

#define MAX_HEADER_NAME_LEN 1024
#define MAX_HEADERS         128
#define TOU(ch) (('a' <= ch && ch <= 'z') ? ch - ('a' - 'A') : ch)

struct common_header {
  const char * name;
  size_t name_len;
  Local<String> key;
};
static int common_headers_num = 0;
static struct common_header common_headers[20];

static void set_common_header(const char * key, int key_len, const int raw) {
  char tmp[MAX_HEADER_NAME_LEN + sizeof("HTTP_") - 1];
  const char* name;
  size_t name_len;
  const char * s;
  char* d;
  size_t n;

  if ( raw == 1 ) {
    for (s = key, n = key_len, d = tmp;
      n != 0;
      s++, --n, d++) {
      *d = *s == '-' ? '_' : TOU(*s);
      name = tmp;
      name_len = key_len;
    }
  } else {
    strcpy(tmp, "HTTP_");
    for (s = key, n = key_len, d = tmp + 5;
      n != 0;
      s++, --n, d++) {
      *d = *s == '-' ? '_' : TOU(*s);
      name = tmp;
      name_len = key_len + 5;
    }
  }
  Local<String> env_key = NanNew<String>(name, name_len);
  common_headers[common_headers_num].name = key;
  common_headers[common_headers_num].name_len = key_len;
  common_headers[common_headers_num].key = env_key;
  common_headers_num++;
}

static size_t find_ch(const char* s, size_t len, char ch) {
  size_t i;
  for (i = 0; i != len; ++i, ++s)
    if (*s == ch)
      break;
  return i;
}

static int header_is(const struct phr_header* header, const char* name,                    size_t len) {
  const char* x, * y;
  if (header->name_len != len)
    return 0;
  for (x = header->name, y = name; len != 0; --len, ++x, ++y)
    if (TOU(*x) != *y)
      return 0;
  return 1;
}

static Local<String> find_common_header(const struct phr_header* header) {
  int i;
  for ( i = 0; i < common_headers_num; i++ ) {
    if ( header_is(header, common_headers[i].name, common_headers[i].name_len) ) {
      return common_headers[i].key;
    }
  }
  return String::Empty();
}

static int store_path_info(Local<Object> envref, const char* src, size_t src_len) {
  size_t dlen = 0, i = 0;
  char *d;
  char s2, s3;

  d = (char*)std::malloc(src_len * 3 + 1);
  for (i = 0; i < src_len; i++ ) {
    if ( src[i] == '%' ) {
      if ( !isxdigit(src[i+1]) || !isxdigit(src[i+2]) ) {
        std::free(d);
        return -1;
      }
      s2 = src[i+1];
      s3 = src[i+2];
      s2 -= s2 <= '9' ? '0'
          : s2 <= 'F' ? 'A' - 10
          : 'a' - 10;
      s3 -= s3 <= '9' ? '0'
          : s3 <= 'F' ? 'A' - 10
          : 'a' - 10;
       d[dlen++] = s2 * 16 + s3;
       i += 2;
    }
    else {
      d[dlen++] = src[i];
    }
  }
  d[dlen]='0';
  envref->Set(NanNew<String>("PATH_INFO"), NanNew<String>(d, dlen));
  std::free(d);
  return dlen;
}

NAN_METHOD(phr_parse_http_request) {
  NanScope();

  if (args.Length() < 2) {
    NanThrowTypeError("phr_parse_request() requires a string, object argument");             \
    NanReturnUndefined();
  }

  if (!args[0]->IsString() || !args[1]->IsObject()) {
    NanThrowTypeError("phr_parse_request() requires a string, object argument");             \
    NanReturnUndefined();
  }

  Local<String> buf = args[0]->ToString();
  Local<Object> envref = args[1]->ToObject();

  size_t buf_len;
  const char* method;
  size_t method_len;
  const char* path;
  size_t path_len;
  // size_t o_path_len;
  int minor_version;
  struct phr_header headers[MAX_HEADERS];
  size_t num_headers, question_at;
  size_t i;
  int ret;
  char tmp[MAX_HEADER_NAME_LEN + sizeof("HTTP_") - 1];
  Local<String> last_value;

  Local<String> request_method_key = NanNew<String>("REQUEST_METHOD");
  Local<String> request_uri_key = NanNew<String>("REQUEST_URI");
  Local<String> script_name_key = NanNew<String>("SCRIPT_NAME");
  Local<String> server_protocol_key = NanNew<String>("SERVER_PROTOCOL");
  Local<String> query_string_key = NanNew<String>("QUERY_STRING");

  String::AsciiValue buf_str(buf);
  buf_len = buf->Length();
  num_headers = MAX_HEADERS;
  ret = phr_parse_request(*buf_str, buf_len, &method, &method_len, &path,
                          &path_len, &minor_version, headers, &num_headers, 0);
  if (ret < 0)
    goto done;

  envref->Set(request_method_key, NanNew<String>(method, method_len));
  envref->Set(request_uri_key, NanNew<String>(path, path_len));
  envref->Set(script_name_key, NanNew<String>(""));
  strcpy(tmp, "HTTP/1.");
  tmp[7] = 48 + ((minor_version > 1 || minor_version < 0 ) ? 0 : minor_version);
  envref->Set(server_protocol_key, NanNew<String>(tmp, sizeof("HTTP/1.0") - 1));

  /* PATH_INFO QUERY_STRING */
  path_len = find_ch(path, path_len, '#'); /* strip off all text after # after storing request_uri */
  question_at = find_ch(path, path_len, '?');
  if ( store_path_info(envref, path, question_at) < 0 ) {
    ret = -1;
    goto done;
  }
  if (question_at != path_len) ++question_at;
  envref->Set(query_string_key, NanNew<String>(path + question_at, path_len - question_at));

  last_value = String::Empty();
  for (i = 0; i < num_headers; ++i) {
    if (headers[i].name != NULL) {
      const char* name;
      size_t name_len;
      Local<String> slot;
      Local<String> env_key;
      env_key = find_common_header(headers + i);
      if ( env_key == String::Empty() ) {
        const char* s;
        char* d;
        size_t n;
        if (sizeof(tmp) - 5 < headers[i].name_len) {
          ret = -1;
          goto done;
        }
        strcpy(tmp, "HTTP_");
        for (s = headers[i].name, n = headers[i].name_len, d = tmp + 5;
          n != 0;
          s++, --n, d++) {
            *d = *s == '-' ? '_' : TOU(*s);
            name = tmp;
            name_len = headers[i].name_len + 5;
            env_key = NanNew<String>(name, name_len);
        }
      }
      slot = envref->Get(env_key).IsEmpty() ? String::Empty() : envref->Get(env_key).As<String>();
      if ( slot != String::Empty() ) {
        String::Concat(slot, NanNew<String>(", "));
        String::Concat(slot, NanNew<String>(headers[i].value, headers[i].value_len));
      } else {
        slot = NanNew<String>(headers[i].value, headers[i].value_len);
        envref->Set(env_key, slot);
        last_value = slot;
      }
    } else {
      /* continuing lines of a mulitiline header */
      if ( last_value != String::Empty() )
        String::Concat(last_value, NanNew<String>(headers[i].value, headers[i].value_len));
    }
  }

 done:
  NanReturnValue(NanNew<Number>(ret));
}

void Init(Handle<Object> exports) {
  set_common_header("ACCEPT",sizeof("ACCEPT") - 1, 0);
  set_common_header("ACCEPT-ENCODING",sizeof("ACCEPT-ENCODING") - 1, 0);
  set_common_header("ACCEPT-LANGUAGE",sizeof("ACCEPT-LANGUAGE") - 1, 0);
  set_common_header("CACHE-CONTROL",sizeof("CACHE-CONTROL") - 1, 0);
  set_common_header("CONNECTION",sizeof("CONNECTION") - 1, 0);
  set_common_header("CONTENT-LENGTH",sizeof("CONTENT-LENGTH") - 1, 1);
  set_common_header("CONTENT-TYPE",sizeof("CONTENT-TYPE") - 1, 1);
  set_common_header("COOKIE",sizeof("COOKIE") - 1, 0);
  set_common_header("HOST",sizeof("HOST") - 1, 0);
  set_common_header("IF-MODIFIED-SINCE",sizeof("IF-MODIFIED-SINCE") - 1, 0);
  set_common_header("REFERER",sizeof("REFERER") - 1, 0);
  set_common_header("USER-AGENT",sizeof("USER-AGENT") - 1, 0);
  set_common_header("X-FORWARDED-FOR",sizeof("X-FORWARDED-FOR") - 1, 0);

  exports->Set(NanNew("parse_http_request"), NanNew<FunctionTemplate>(phr_parse_http_request)->GetFunction());
}

NODE_MODULE(PicoHTTPParser, Init)
