#ifndef SIMPLE_SQLITE_H
#define SIMPLE_SQLITE_H

#include <sqlite3.h>
#include <string>


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

    query& arg(const std::string& text);

    template <typename float_type, std::enable_if_t<std::is_floating_point<float_type>::value, bool> = true>
    query& arg(float_type real);

    template<typename int_type, std::enable_if_t<std::is_integral<int_type>::value, bool> = true>
    query& arg(int_type number);

    query& getField(std::string& text);

    template <typename float_type, std::enable_if_t<std::is_floating_point<float_type>::value, bool> = true>
    query& getField(float_type& real);

    template<typename int_type, std::enable_if_t<std::is_integral<int_type>::value, bool> = true>
    query& getField(int_type& number);

  protected:
    explicit query(sqlite3_stmt* statement);
  private:
    void throw_if_error_binding(int errval);
    bool field_valid_or_throw(int expected_type);

    sqlite3_stmt* m_statement;
    int m_last_error;
    int m_arg;
    int m_field;
    bool m_buffered_filled;
  };


  template <typename float_type, std::enable_if_t<std::is_floating_point<float_type>::value, bool>>
  query& query::arg(float_type real)
  {
    if(++m_arg, !valid())
      throw m_arg;

    throw_if_error_binding(sqlite3_bind_double(m_statement, m_arg, real));
    return *this;
  }

  template<typename int_type, std::enable_if_t<std::is_integral<int_type>::value, bool>>
  query& query::arg(int_type number)
  {
    if(++m_arg, !valid())
      throw m_arg;

    throw_if_error_binding(sqlite3_bind_int64(m_statement, m_arg, number));
    return *this;
  }

  template <typename float_type, std::enable_if_t<std::is_floating_point<float_type>::value, bool>>
  query& query::getField(float_type& real)
  {
    real = field_valid_or_throw(SQLITE_FLOAT) ?
             sqlite3_column_double(m_statement, m_field) :
             0.0;
    ++m_field;
    return *this;
  }

  template<typename int_type, std::enable_if_t<std::is_integral<int_type>::value, bool>>
  query& query::getField(int_type& number)
  {
    number = field_valid_or_throw(SQLITE_INTEGER) ?
               sqlite3_column_int64(m_statement, m_field) :
               0;
    ++m_field;
    return *this;
  }
}

#endif // SIMPLE_SQLITE_H
