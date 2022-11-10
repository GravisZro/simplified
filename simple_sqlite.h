#ifndef SIMPLE_SQLITE_H
#define SIMPLE_SQLITE_H

#include <sqlite3.h>
#include <string>
#include <string_view>
#include <optional>
#include <type_traits>
#include <vector>


namespace sql
{
  enum use_t : intptr_t
  {
    copy = 0,
    reference = -1,
  };

  template< class T >
  struct is_string_view : std::integral_constant<bool,
      std::is_same_v<T, std::string_view> ||
      std::is_same_v<T, std::wstring_view> ||
      std::is_same_v<T, std::u16string_view>
      > {};

  template< class T >
  struct is_sql_type : std::integral_constant<bool,
      std::is_enum_v<T> ||
      std::is_arithmetic_v<T> ||
      std::is_same_v<T, std::string> ||
      std::is_same_v<T, std::wstring> ||
      std::is_same_v<T, std::u16string>> {};

  class query;

  class db
  {
  public:
    db(void) noexcept;
    ~db(void) noexcept;

    bool open(const std::string_view& filename) noexcept;
    bool close(void) noexcept;

    query build_query(const std::string_view& query_str);
    bool execute(const std::string_view& sql_str) noexcept;

  private:
    sqlite3* m_db;
  };

  class query
  {
    friend class db;
  public:
    query(query&& other) noexcept;
    ~query(void) noexcept;

    query& operator= (query&& other) noexcept;

    constexpr bool valid(void) const noexcept { return m_statement != nullptr; }
    constexpr int lastError(void) const noexcept { return m_last_error; }

    bool execute(void) noexcept;
    bool fetchRow(void) noexcept;

    template <typename T>
    query& getField(std::optional<T>& generic);

    template <typename T,  std::enable_if_t<is_sql_type<T>::value, bool> = true>
    query& getField(T& generic);

    template <typename T>
    query& arg(const std::optional<T>& generic, use_t use = copy);

    template <typename T,  std::enable_if_t<is_sql_type<T>::value || is_string_view<T>::value, bool> = true>
    query& arg(T generic, use_t use = copy);

  private:
    query(sqlite3_stmt* statement);

    template <typename T,  std::enable_if_t<std::is_enum_v<T> || std::is_arithmetic_v<T>, bool> = true>
    constexpr int bind(T generic, use_t) { return bind(generic); }

    template<typename enum_type, std::enable_if_t<std::is_enum_v<enum_type>, bool> = true>
    int bind(enum_type enumeration);

    template<typename int_type, std::enable_if_t<std::is_integral_v<int_type>, bool> = true>
    int bind(int_type number);

    template <typename float_type, std::enable_if_t<std::is_floating_point_v<float_type>, bool> = true>
    int bind(float_type real);

    int bind(const std::string& text, use_t use);
    int bind(const std::string_view& text, use_t use);
    int bind(const std::wstring& text, use_t use);
    int bind(const std::u16string_view& text, use_t use);
    int bind(const std::vector<uint8_t>& blob, use_t use);

    template<typename enum_type, std::enable_if_t<std::is_enum_v<enum_type>, bool> = true>
    void field(enum_type& enumeration);

    template<typename int_type, std::enable_if_t<std::is_integral_v<int_type>, bool> = true>
    void field(int_type& number);

    template <typename float_type, std::enable_if_t<std::is_floating_point_v<float_type>, bool> = true>
    void field(float_type& real);

    void field(std::string& text);
    void field(std::wstring& text);
    void field(std::u16string& text);
    void field(std::vector<uint8_t>& blob);

    void throw_if_error_binding(int errval);
    void throw_if_bad_field(int expected_type);
  private:
    sqlite3_stmt* m_statement;
    int m_last_error;
    int m_arg;
    int m_field;
    bool m_buffered_filled;
  };

  template <typename T>
  query& query::arg(const std::optional<T>& generic, use_t use)
  {
    if(++m_arg, !valid())
      throw m_arg;

    throw_if_error_binding(
          generic.has_value() ? bind(*generic, use)
                              : sqlite3_bind_null(m_statement, m_arg)
                          );
    return *this;
  }

  template <typename T,  std::enable_if_t<is_sql_type<T>::value || is_string_view<T>::value, bool>>
  query& query::arg(T generic, use_t use)
  {
    if(++m_arg, !valid())
      throw m_arg;

    throw_if_error_binding(bind(generic, use));
    return *this;
  }


  template <typename T>
  query& query::getField(std::optional<T>& generic)
  {
    if(valid() && sqlite3_column_type(m_statement, m_field) == SQLITE_NULL)
      generic.reset();
    else
    {
      T data;
      field(data);
      generic.emplace(std::move(data));
    }
    ++m_field;
    return *this;
  }

  template <typename T,  std::enable_if_t<is_sql_type<T>::value, bool>>
  query& query::getField(T& generic)
  {
    field(generic);
    ++m_field;
    return *this;
  }


  template<typename enum_type, std::enable_if_t<std::is_enum_v<enum_type>, bool>>
  int query::bind(enum_type enumeration)
    { return sqlite3_bind_int(m_statement, m_arg, enumeration); }

  template<typename int_type, std::enable_if_t<std::is_integral_v<int_type>, bool>>
  int query::bind(int_type number)
    { return sqlite3_bind_int64(m_statement, m_arg, number); }

  template <typename float_type, std::enable_if_t<std::is_floating_point_v<float_type>, bool>>
  int query::bind(float_type real)
    { return sqlite3_bind_double(m_statement, m_arg, real); }


  template<typename enum_type, std::enable_if_t<std::is_enum_v<enum_type>, bool>>
  void query::field(enum_type& enumeration)
  {
    throw_if_bad_field(SQLITE_INTEGER);
    enumeration = sqlite3_column_int(m_statement, m_field);
  }

  template<typename int_type, std::enable_if_t<std::is_integral_v<int_type>, bool>>
  void query::field(int_type& number)
  {
    throw_if_bad_field(SQLITE_INTEGER);
    number = sqlite3_column_int64(m_statement, m_field);
  }

  template <typename float_type, std::enable_if_t<std::is_floating_point_v<float_type>, bool>>
  void query::field(float_type& real)
  {
    throw_if_bad_field(SQLITE_FLOAT);
    real = sqlite3_column_double(m_statement, m_field);
  }
}

#endif // SIMPLE_SQLITE_H
