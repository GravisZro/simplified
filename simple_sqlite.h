#ifndef SIMPLE_SQLITE_H
#define SIMPLE_SQLITE_H

#include <sqlite3.h>
#include <string>
#include <optional>


namespace sql
{
  class query;

  class db
  {
  public:
    db(void) noexcept;
    ~db(void) noexcept;

    bool open(const std::string& filename) noexcept;
    bool close(void) noexcept;

    query build_query(const std::string& query_str);
    bool execute(const std::string& sql_str) noexcept;

  private:
    sqlite3* m_db;
  };

  class query
  {
    friend class db;
  public:;
    query(query&& other) noexcept;
    ~query(void) noexcept;

    query& operator= (query&& other) noexcept;

    constexpr bool valid(void) const noexcept { return m_statement != nullptr; }
    constexpr int lastError(void) const noexcept { return m_last_error; }

    bool execute(void) noexcept;
    bool fetchRow(void) noexcept;

    query& arg(const std::optional<std::string>& text);

    template <typename float_type, std::enable_if_t<std::is_floating_point<float_type>::value, bool> = true>
    query& arg(const std::optional<float_type>& real);

    template <typename float_type, std::enable_if_t<std::is_floating_point<float_type>::value, bool> = true>
    query& arg(float_type real) { return arg(std::optional<float_type>(real)); }


    template<typename int_type, std::enable_if_t<std::is_integral<int_type>::value, bool> = true>
    query& arg(const std::optional<int_type>& number);

    template<typename int_type, std::enable_if_t<std::is_integral<int_type>::value, bool> = true>
    query& arg(int_type number) { return arg(std::optional<int_type>(number)); }


    query& arg(const std::optional<bool>& boolean);
    query& arg(bool boolean) { return arg(std::optional<bool>(boolean)); }



    query& getField(std::optional<std::string>& text);

    template <typename float_type, std::enable_if_t<std::is_floating_point<float_type>::value, bool> = true>
    query& getField(std::optional<float_type>& real);

    template<typename int_type, std::enable_if_t<std::is_integral<int_type>::value, bool> = true>
    query& getField(std::optional<int_type>& number);

    query& getField(std::optional<bool>& boolean);



  protected:
    void throw_if_error_binding(int errval);
    bool field_valid_or_throw(int expected_type);
    query(sqlite3_stmt* statement);
  private:
    sqlite3_stmt* m_statement;
    int m_last_error;
    int m_arg;
    int m_field;
    bool m_buffered_filled;
  };

  template <typename float_type, std::enable_if_t<std::is_floating_point<float_type>::value, bool>>
  query& query::arg(const std::optional<float_type>& real)
  {
    if(++m_arg, !valid())
      throw m_arg;

    throw_if_error_binding(
          real.has_value() ? sqlite3_bind_double(m_statement, m_arg, *real)
                           : sqlite3_bind_null(m_statement, m_arg)
                          );
    return *this;
  }

  template<typename int_type, std::enable_if_t<std::is_integral<int_type>::value, bool>>
  query& query::arg(const std::optional<int_type>& number)
  {
    if(++m_arg, !valid())
      throw m_arg;

    throw_if_error_binding(
          number.has_value() ? sqlite3_bind_int64(m_statement, m_arg, *number)
                             : sqlite3_bind_null(m_statement, m_arg)
                          );
    return *this;
  }

  template <typename float_type, std::enable_if_t<std::is_floating_point<float_type>::value, bool>>
  query& query::getField(std::optional<float_type>& real)
  {
    if(field_valid_or_throw(SQLITE_FLOAT))
      real = sqlite3_column_double(m_statement, m_field);
    else
      real.reset();

    ++m_field;
    return *this;
  }

  template<typename int_type, std::enable_if_t<std::is_integral<int_type>::value, bool>>
  query& query::getField(std::optional<int_type>& number)
  {
    if(field_valid_or_throw(SQLITE_INTEGER))
      number = sqlite3_column_int64(m_statement, m_field);
    else
      number.reset();
    ++m_field;
    return *this;
  }
}

#endif // SIMPLE_SQLITE_H
