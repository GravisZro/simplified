#include "simple_jscore.h"

#include <crypto/strhash.h>
#include <curl/curl.h>

#include <glib-object.h>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <string>
#include <queue>

/*
constexpr const char* boolstring(gboolean val)
  { return val == TRUE ? "true" : "false"; }

void print_value(JSCValue* value) noexcept
{
  std::cerr << "undefined? " << boolstring(jsc_value_is_undefined (value)) << std::endl
            << "null?      " << boolstring(jsc_value_is_null      (value)) << std::endl
            << "number?    " << boolstring(jsc_value_is_number    (value)) << std::endl
            << "boolean?   " << boolstring(jsc_value_is_boolean   (value)) << std::endl
            << "string?    " << boolstring(jsc_value_is_string    (value)) << std::endl
            << "array?     " << boolstring(jsc_value_is_array     (value)) << std::endl
            << "object?    " << boolstring(jsc_value_is_object    (value)) << std::endl;

  if(jsc_value_is_string(value) == TRUE)
      std::cerr << "string: " << jsc_value_to_string(value) << std::endl;

}
*/
void exception_handler (JSCContext* context,
                        JSCException* exception,
                        gpointer user_data)
{
  (void)context, (void)user_data;
  std::cerr << jsc_exception_report(exception)
            << std::endl;
}

std::string get_curl_cookies(CURL* handle)
{
  static const char* const deliminators = "\t"
                                          " !\"#$%&\'()*+,-./"
                                          ";<=>?@"
                                          "[\\]^_`"
                                          "{|}~";
  std::string rval, tmp;
  size_t start = 0, split = 0, count = 0;
  const char* delim = deliminators;
  curl_slist* cookies = NULL;

  curl_easy_getinfo(handle, CURLINFO_COOKIELIST, &cookies);

  if(cookies != NULL)
  {
    for(curl_slist *each = cookies; each != NULL; each = each->next)
    {
      tmp = each->data;
      if(!tmp.empty())
      {
        delim = deliminators;
        count = 0;
        do
        {
          for(size_t subpos = 0; subpos < tmp.size(); ++subpos)
            if(tmp.at(subpos) == *delim)
              ++count;
        } while(count != 6 && *++delim);
        if(count == 6)
        {
          split = tmp.rfind(*delim);
          start = tmp.rfind(*delim, split - 1);
          tmp = tmp.substr(start + 1, split - start - 1) + '=' + tmp.substr(split + 1); // remove restriction data
        }
        rval.append(tmp).append("; ");
      }
    }
    rval.pop_back(); // remove trailing ' '
    rval.pop_back(); // remove trailing ';'
    curl_slist_free_all(cookies);
  }
  return rval;
}


namespace HTTPConnection
{
  CURLM* multi_handle;
  std::string url_base;

  std::string hostbased_url(const std::string& url) noexcept
  {
    if(url.find("http") != 0)
      return url_base + url;
    else
      return url;
  }

  static std::size_t record_data(char* data, std::size_t size, std::size_t nmemb, struct connection_t* connection) noexcept;

  struct connection_t
  {
    CURL* handle;
    std::vector<uint8_t> data;

    connection_t(JSCValue* obj) noexcept
    {
      handle = curl_easy_init();
      curl_multi_add_handle(multi_handle, handle);

      setOpt(CURLOPT_WRITEDATA, this);
      setOpt(CURLOPT_WRITEFUNCTION, record_data);
      setOpt(CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64; rv:81.0) Gecko/20100101 Firefox/81.0");
      setOpt(CURLOPT_TCP_KEEPALIVE, 1);
      setOpt(CURLOPT_COOKIEFILE, "");

      JSCValue* document = jsc_context_get_value(jsc_value_get_context(obj), "document");
      JSCValue* cookie = jsc_value_object_get_property(document, "cookie");
      if(jsc_value_is_string(cookie))
        setOpt(CURLOPT_COOKIELIST, jsc_value_to_string(cookie));
    }   

    template<typename T>
    bool setOpt(CURLoption option, T* option_data) noexcept
      { return curl_easy_setopt(handle, option, option_data) == CURLE_OK; }

    template<typename T>
    bool setOpt(CURLoption option, T option_data) noexcept
      { return curl_easy_setopt(handle, option, option_data) == CURLE_OK; }

    bool setOpt(CURLoption option, std::string option_data) noexcept
      { return curl_easy_setopt(handle, option, option_data.c_str()) == CURLE_OK; }

    bool exec(void) noexcept
      { return curl_easy_perform(handle) == CURLE_OK; }
  };

  std::map<JSCValue*, connection_t> active;

  void unref(gpointer instance) noexcept
  {
    g_assert(JSC_IS_VALUE(instance));
    JSCValue* obj = JSC_VALUE(instance);
    connection_t& connection = active.at(obj);
    curl_multi_remove_handle(multi_handle, connection.handle);
    curl_easy_cleanup(connection.handle);
    active.erase(obj);
    g_object_unref(obj);
  }


  std::size_t record_data(char* data, std::size_t size, std::size_t nmemb, connection_t* connection) noexcept
  {
    if (!connection)
      return 0;
    connection->data.reserve(connection->data.size() + size * nmemb);
    for(size_t pos = 0, end = size * nmemb; pos < end; ++pos, ++data)
      connection->data.push_back(*data);
    return size * nmemb;
  }

  void init(void) noexcept
  {
    multi_handle = curl_multi_init();
  }

  void deinit(void) noexcept
  {
    curl_multi_cleanup(multi_handle);
  }
}

namespace DOMElement
{
  using HTTPConnection::connection_t;
  JSCClass* class_instance;

  static JSCValue* Constructor(const char* tagname) noexcept
  {
    JSCContext* context = jsc_context_get_current();
    JSCValue* obj = jsc_value_new_object(context, NULL, NULL);
    jsc_value_object_set_property(obj, "tag", jsc_value_new_string(context, tagname));
    HTTPConnection::active.emplace(std::make_pair(obj, connection_t(obj)));
    return obj;
  }

  static void set_source(JSCValue* obj, const char* url)
  {
    g_assert_true(JSC_IS_VALUE(obj));
    connection_t& connection = HTTPConnection::active.at(obj);
    JSCContext* context = jsc_value_get_context(obj);
    connection.setOpt(CURLOPT_URL, HTTPConnection::hostbased_url(url).c_str());
    connection.exec();

    std::string cookies = get_curl_cookies(connection.handle);
    if(!cookies.empty())
      jsc_context_set_value(context, "cookie", jsc_value_new_string(context, cookies.c_str()));
  }

  JSCClass* init(JSCContext* context) noexcept
  {
    class_instance =
        jsc_context_register_class(context,          // engine context
                                   "DOMElement",     // new object type name to define
                                   NULL,             // user data
                                   NULL,             // virtual table
                                   g_object_unref);  // cleanup function

    jsc_class_add_property(class_instance,            // class to add to
                           "src",                     // property name
                            G_TYPE_STRING,            // type of property
                            NULL,                     // getter function
                            G_CALLBACK(set_source), // setter function
                            NULL,                     // user data
                            g_object_unref);          // cleanup function


    JSCValue* constructor =
        jsc_class_add_constructor(class_instance,           // class to add to
                                  NULL,                     // name (NULL is default)
                                  G_CALLBACK(Constructor),  // constructor function
                                  NULL,                     // user data
                                  HTTPConnection::unref,    // destructor function
                                  JSC_TYPE_VALUE,           // return type
                                  1, G_TYPE_STRING);        // arguments (number then types)

    jsc_context_set_value(context,                            // engine context
                          jsc_class_get_name(class_instance), // use object type name for constructor
                          constructor);                       // value of constructor

    return class_instance;
  }
}

namespace XMLHttpRequest
{
  using HTTPConnection::connection_t;
  JSCClass* class_instance;

  JSCValue* Constructor(void) noexcept
  {
    JSCContext* context = jsc_context_get_current();
    JSCValue* obj = jsc_value_new_object(context, NULL, NULL);
    jsc_value_object_set_property(obj, "onreadystatechange", jsc_value_new_null(context));
    jsc_value_object_set_property(obj, "readyState", jsc_value_new_number(context, 0));
    jsc_value_object_set_property(obj, "status", jsc_value_new_number(context, 0));

    HTTPConnection::active.emplace(std::make_pair(obj, connection_t(obj)));
    return obj;
  }

  void open(JSCValue* obj, const char* type, const char* url, gboolean async, const char* username, const char* password) noexcept
  {
    g_assert_true(JSC_IS_VALUE(obj));
    connection_t& connection = HTTPConnection::active.at(obj);

    connection.setOpt(CURLOPT_URL, HTTPConnection::hostbased_url(url).c_str());

    switch(hash(type))
    {
      case "GET"_hash:
        connection.setOpt(CURLOPT_HTTPGET, 1);
        break;
      case "POST"_hash:
        connection.setOpt(CURLOPT_POST, 1);
        break;
      case "PUT"_hash:
        connection.setOpt(CURLOPT_PUT, 1);
        break;
      default:
        g_assert(false);
    }

    g_assert(async == FALSE);

    if(username != NULL)
      connection.setOpt(CURLOPT_USERNAME, username);

    if(password != NULL)
      connection.setOpt(CURLOPT_PASSWORD, password);
  }


  void send(JSCValue* obj, const char* post_data) noexcept
  {
    g_assert_true(JSC_IS_VALUE(obj));
    connection_t& connection = HTTPConnection::active.at(obj);
    JSCContext* context = jsc_value_get_context(obj);

    if(post_data != NULL)
      connection.setOpt(CURLOPT_POSTFIELDS, post_data);

    connection.exec();

    long response;
    curl_easy_getinfo(connection.handle, CURLINFO_RESPONSE_CODE, &response);
    std::string cookies = get_curl_cookies(connection.handle);
    if(!cookies.empty())
      jsc_context_set_value(context, "cookie", jsc_value_new_string(context, cookies.c_str()));

    jsc_value_object_set_property(obj, "state", jsc_value_new_number(context, response));
    jsc_value_object_set_property(obj, "readyState", jsc_value_new_number(context, 4));

    JSCValue* func = jsc_value_object_get_property(obj, "onreadystatechange");
    if(jsc_value_is_function(func))
      g_object_unref(jsc_value_function_call(func, G_TYPE_NONE));
    g_object_unref(func);
  }

  void set_onreadystatechange(JSCValue* obj, JSCValue* func) noexcept
  {
    jsc_value_object_set_property(obj, "onreadystatechange", func);
  }

  gint32 get_status(JSCValue* obj)
    { return jsc_value_to_int32(jsc_value_object_get_property(obj, "status")); }

  gint32 get_readyState(JSCValue* obj)
    { return jsc_value_to_int32(jsc_value_object_get_property(obj, "readyState")); }

  JSCClass* init(JSCContext* context) noexcept
  {
    class_instance =
        jsc_context_register_class(context,
                                   "XMLHttpRequest",
                                   NULL,
                                   NULL,
                                   g_object_unref);

    jsc_class_add_method(class_instance,
                         "open",
                         G_CALLBACK(open),
                         NULL,
                         g_object_unref,
                         G_TYPE_NONE,
                         5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING);

    jsc_class_add_method(class_instance,
                         "send",
                         G_CALLBACK(send),
                         NULL,
                         g_object_unref,
                         G_TYPE_NONE,
                         1, G_TYPE_STRING);

    jsc_class_add_property(class_instance,              // class to add to
                           "onreadystatechange",        // property name
                            JSC_TYPE_VALUE,             // type of property
                            NULL,                       // getter function
                            G_CALLBACK(set_onreadystatechange), // setter function
                            NULL,                       // user data
                            g_object_unref);            // cleanup function

    jsc_class_add_property(class_instance,              // class to add to
                           "status",                    // property name
                            G_TYPE_INT,                 // type of property
                            G_CALLBACK(get_status),     // getter function
                            NULL,                       // setter function
                            NULL,                       // user data
                            g_object_unref);            // cleanup function

    jsc_class_add_property(class_instance,              // class to add to
                           "readyState",                // property name
                            G_TYPE_INT,                 // type of property
                            G_CALLBACK(get_readyState), // getter function
                            NULL,                       // setter function
                            NULL,                       // user data
                            g_object_unref);            // cleanup function

    JSCValue* constructor_function =
        jsc_class_add_constructor(class_instance,
                                  NULL,
                                  G_CALLBACK(Constructor),
                                  NULL,
                                  HTTPConnection::unref,
                                  JSC_TYPE_VALUE,
                                  0, G_TYPE_NONE);

    jsc_context_set_value(context,
                          jsc_class_get_name(class_instance),
                          constructor_function);

    return class_instance;
  }
}

namespace Document
{
  JSCClass* class_instance;

  JSCValue* Constructor(void) noexcept
  {
    JSCContext* context = jsc_context_get_current();
    JSCValue* obj = jsc_value_new_object(context, NULL, NULL);
    return obj;
  }

  void set_cookie(JSCValue* obj, const char* value) noexcept
  {
    JSCContext* context = jsc_value_get_context(obj);
    std::string cookie_value = value;

    size_t pos = cookie_value.rfind(';');
    if(cookie_value.find("path=", pos) != std::string::npos) // if there is a path
      cookie_value.erase(pos); // remove it

    pos = cookie_value.rfind(';');
    if(cookie_value.find("expires=", pos) != std::string::npos) // if there is a expiration date
      cookie_value.erase(pos); // remove it

    jsc_value_object_set_property(obj, "cookie", jsc_value_new_string(context, cookie_value.c_str()));
  }

  const char* get_cookie(JSCValue* obj)
    { return jsc_value_to_string(jsc_value_object_get_property(obj, "cookie")); }

  JSCClass* init(JSCContext* context) noexcept
  {
    class_instance =
        jsc_context_register_class(context,
                                   "HTMLDocument",
                                   NULL,
                                   NULL,
                                   g_object_unref);

    jsc_class_add_property(class_instance,            // class to add to
                           "cookie",                  // property name
                            G_TYPE_STRING,            // type of property
                            G_CALLBACK(get_cookie),   // getter function
                            G_CALLBACK(set_cookie),   // setter function
                            NULL,                     // user data
                            g_object_unref);          // cleanup function

    JSCValue* constructor_function =
        jsc_class_add_constructor(class_instance,
                                  NULL,
                                  G_CALLBACK(Constructor),
                                  NULL,
                                  HTTPConnection::unref,
                                  JSC_TYPE_VALUE,
                                  0, G_TYPE_NONE);

    jsc_context_set_value(context,
                          jsc_class_get_name(class_instance),
                          constructor_function);

    return class_instance;
  }
}

SimpleJSCore::SimpleJSCore(void) noexcept
  : m_handle(NULL)
{
  JSCContext* context =
  m_handle = jsc_context_new();

  jsc_context_set_value(context,
                        "window",
                        jsc_context_get_global_object(context)); // window is the global object


  jsc_context_set_value(context,
                        "onunload",
                        jsc_value_new_null(context));


  HTTPConnection::init();
  Document::init(context);
  m_classes.push_back(DOMElement::init(context));
  m_classes.push_back(XMLHttpRequest::init(context));
  jsc_context_push_exception_handler(context, // log all errors
                                     exception_handler,
                                     NULL,
                                     NULL);

  const char* startup = "var toString=function(){return'[object Window]'},document=new HTMLDocument;document.cookie='',HTMLDocument.prototype.createElement=function(e){return new DOMElement(e)};var WebGLRenderingContext=function(){},location={reload:function(){}},constructor={toString:function(){return'function Window() { [native code] }'}},outerWidth=1920,outerHeight=1013,WebAssembly=new Object,navigator={vendor:'',appName:'Netscape',plugins:new Array,platform:'Linux x86_64',oscpu:'Linux x86_64',webdriver:!1,globalThis:window,language:'en-US'},console={log:''};";
  g_object_unref(jsc_context_evaluate(context, startup, -1));
}

SimpleJSCore::~SimpleJSCore(void) noexcept
{
  HTTPConnection::deinit();

  for(JSCClass*& jsc_class : m_classes)
    g_object_unref(jsc_class), jsc_class = NULL;

  if(m_handle != NULL)
    g_object_unref(m_handle), m_handle = NULL;
}

bool SimpleJSCore::eval(const std::string& url_base, const std::string& script, const std::string& cookies) noexcept
{
  JSCContext* context = m_handle;
  HTTPConnection::url_base = url_base;
  JSCValue* document = jsc_context_get_value(context, "document");

  if(!cookies.empty())
    jsc_value_object_set_property(document, "cookie", jsc_value_new_string(context, cookies.c_str()));

  g_object_unref(jsc_context_evaluate(context, script.c_str(), -1));

  JSCValue* onunload = jsc_context_get_value(context, "onunload");
  if(jsc_value_is_function(onunload))
    g_object_unref(jsc_value_function_call(onunload, G_TYPE_NONE));

  JSCValue* document_cookie = jsc_value_object_get_property(document, "cookie");
  if(jsc_value_is_string(document_cookie) == TRUE)
    m_cookies = jsc_value_to_string(document_cookie);

  g_object_unref(document_cookie);
  g_object_unref(document);

  return true;
}
