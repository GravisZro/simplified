#ifndef SIMPLE_JSCORE_H
#define SIMPLE_JSCORE_H

#include <list>
#include <string>
#include <jsc/jsc.h>


typedef void CURL;
std::string get_curl_cookies(CURL* handle);

class SimpleJSCore
{
public:
  SimpleJSCore(void) noexcept;
  ~SimpleJSCore(void) noexcept;

  SimpleJSCore(const SimpleJSCore&) = delete;
  SimpleJSCore& operator=(const SimpleJSCore&) = delete;

  bool eval(const std::string& script_url, const std::string& script, const std::string& cookies) noexcept;

  std::string getCookies(void) noexcept { return m_cookies; }
  constexpr JSCContext* getHandle(void) noexcept { return m_handle; }
private:
  std::string m_cookies;
  JSCContext* m_handle;
  std::list<JSCClass*> m_classes;
};

#endif // SIMPLE_JSCORE_H
