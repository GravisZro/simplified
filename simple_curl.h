#ifndef SIMPLE_CURL_H
#define SIMPLE_CURL_H

#include <cstddef>
#include <string>
#include <set>
#include <curl/curl.h>

class SimpleCurl
{
public:
  SimpleCurl(void) noexcept
    : m_handle(curl_easy_init()),
      m_headers (nullptr), // nullify needed
      m_last_error(CURLE_OK)
    {  }

  ~SimpleCurl(void) noexcept
  {
    clearHeaderFields();
    curl_easy_cleanup(m_handle);
  }

  SimpleCurl(const SimpleCurl&) = delete;
  SimpleCurl& operator=(const SimpleCurl&) = delete;

  bool pause(int bitmask) noexcept
    { return checkError(curl_easy_pause(m_handle, bitmask)); }

  bool perform(void) noexcept
    { return checkError(curl_easy_perform(m_handle)); }

  bool recv(void* buffer, std::size_t bufferLength, std::size_t* n) noexcept
    { return checkError(curl_easy_recv(m_handle, buffer, bufferLength, n)); }

  void reset(void) noexcept
    { curl_easy_reset(m_handle); }

  bool send(const void* buffer, std::size_t bufferLength, std::size_t* n) noexcept
    { return checkError(curl_easy_send(m_handle, buffer, bufferLength, n)); }

  void clearHeaderFields(void)
  {
    if(m_headers != nullptr) // ensure non-empty
      curl_slist_free_all(m_headers), m_headers = nullptr; // free and nullify
  }

  bool setHeaderField(const std::string& name, const std::string& value) noexcept
  {
    m_headers = curl_slist_append(m_headers, std::string(name + ": " + value).c_str());
    return checkError(curl_easy_setopt(m_handle, CURLOPT_HTTPHEADER, m_headers));
  }

  template <typename T>
  bool getInfo(CURLINFO info, T* arg) noexcept
    { return checkError(curl_easy_getinfo(m_handle, info, arg)); }

  bool getInfo(CURLINFO info, std::string& arg) noexcept
  {
    char* result = nullptr;
    return checkError(curl_easy_getinfo(m_handle, info, &result)) &&
        result != nullptr &&
        !arg.assign(result).empty();
  }


  template <typename T>
  bool setOpt(CURLoption option, T arg)
    { return checkError(curl_easy_setopt(m_handle, option, arg)); }

  bool setOpt(CURLoption option, const std::string& arg)
    { return setOpt(option, arg.c_str()); }


  std::string escape(const std::string& string)
  {
    char* cEscaped = curl_easy_escape(m_handle, string.c_str(), string.size());
    std::string escaped(cEscaped);
    curl_free(cEscaped);
    return escaped;
  }

  std::string unescape(const std::string& escapedString)
  {
    int size = 0;
    char* cstr = curl_easy_unescape(m_handle, escapedString.c_str(), escapedString.size(), &size);
    std::string unescaped(cstr, size);
    curl_free(cstr);
    return unescaped;
  }

  constexpr CURL* getHandle(void) noexcept { return m_handle; }
  constexpr CURLcode getLastError(void) noexcept { return m_last_error; }
private:
  constexpr bool checkError(CURLcode code) noexcept
  { return (m_last_error = code, code == CURLE_OK); }

  CURL* m_handle;
  struct curl_slist* m_headers;
  CURLcode m_last_error;
};

#endif // SIMPLE_CURL_H
