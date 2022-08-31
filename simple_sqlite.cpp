#include "simple_sqlite.h"

namespace sql
{
  db::db(void) noexcept
    : m_db(nullptr)
  {
  }

  db::~db(void) noexcept
  {
    if(m_db != nullptr)
      close();
  }

  bool db::open(const std::string& filename) noexcept
  {
    return sqlite3_open(filename.c_str(), &m_db) == SQLITE_OK;
  }

  bool db::close(void) noexcept
  {
    sqlite3* tmp = m_db;
    m_db = nullptr;
    return sqlite3_close(tmp) == SQLITE_OK;
  }

  bool db::execute(const std::string& sql_str) noexcept
  {
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql_str.c_str(), NULL, NULL, &err);
    if(rc != SQLITE_OK)
    {
      sqlite3_free(err);
      return false;
    }
    return true;
  }

  query db::build_query(const std::string& query_str)
  {
    sqlite3_stmt* statement = nullptr;
    const char* errmsg;
    int rval = sqlite3_prepare(m_db, query_str.c_str(), query_str.size(), &statement, &errmsg);
    if(rval != SQLITE_OK)
      throw "build query: " + std::string(errmsg);
    return query(statement);
  }

  query::query(sqlite3_stmt* statement)
    : m_statement(statement),
      m_last_error(SQLITE_OK),
      m_arg(0),
      m_field(0),
      m_buffered_filled(false)
  {
  }

  query::query(query&& other) noexcept
  {
    operator=(std::move(other));
  }

  query::~query(void) noexcept
  {
    if(valid())
      sqlite3_finalize(m_statement);
  }

  query& query::operator= (query&& other) noexcept
  {
    m_statement = other.m_statement; other.m_statement = nullptr;
    m_last_error = other.m_last_error;
    m_arg = other.m_arg;
    m_field = other.m_field;
    m_buffered_filled = other.m_buffered_filled;
    return *this;
  }


  bool query::execute(void) noexcept
  {
    if(!valid())
      return false;

    m_last_error = sqlite3_step(m_statement);
    if(m_last_error == SQLITE_ROW)
      m_buffered_filled = true;

    return
        m_last_error == SQLITE_ROW ||
        m_last_error == SQLITE_DONE;
  }

  bool query::fetchRow(void) noexcept
  {
    if(!valid())
      return false;

    m_field = 0; // reset fetches
    if(!m_buffered_filled)
      m_last_error = sqlite3_step(m_statement);

    m_buffered_filled = false;

    return m_last_error == SQLITE_ROW;
  }

#if 0
  query& query::arg(const std::string& text)
  {
    ++m_arg;
    if(!valid())
      throw m_arg;

    m_last_error = sqlite3_bind_text(m_statement, m_arg, text.c_str(), text.size(), SQLITE_TRANSIENT);
    throw_if_error_setting();

    return *this;
  }


  query& query::arg(double real)
  {
    ++m_arg;
    if(!valid())
      throw m_arg;

    m_last_error = sqlite3_bind_double(m_statement, m_arg, real);
    if(m_last_error != SQLITE_OK)
      throw "error setting argument: " + std::to_string(m_arg) +
        " of " + std::to_string(sqlite3_bind_parameter_count(m_statement)) +
        "\n" + sqlite3_errstr(m_last_error);

    return *this;
  }

  query& query::arg(int number)
  {
    ++m_arg;
    if(!valid())
      throw m_arg;

    m_last_error = sqlite3_bind_int(m_statement, m_arg, number);
    if(m_last_error != SQLITE_OK)
      throw "error setting argument: " + std::to_string(m_arg) +
        " of " + std::to_string(sqlite3_bind_parameter_count(m_statement)) +
        "\n" + sqlite3_errstr(m_last_error);

    return *this;
  }

  query& query::getField(std::string& text)
  {
    int field_type = sqlite3_column_type(m_statement, m_field);
    if(!valid() || (field_type != SQLITE_TEXT && field_type != SQLITE_NULL))
      throw "field type mismatch for field: " + std::to_string(m_field) +
        " of " + std::to_string(sqlite3_column_count(m_statement)) +
        " expected type id: " + std::to_string(field_type);
    text.clear();
    if(field_type != SQLITE_NULL)
    {
      const unsigned char* rval = sqlite3_column_text(m_statement, m_field);
      if(rval != NULL)
        text = reinterpret_cast<const char*>(rval);
    }
    ++m_field;
    return *this;
  }

  query& query::getField(double& real)
  {
    int field_type = sqlite3_column_type(m_statement, m_field);
    if(!valid() || (field_type != SQLITE_FLOAT && field_type != SQLITE_NULL))
      throw "field type mismatch for field: " + std::to_string(m_field) +
        " of " + std::to_string(sqlite3_column_count(m_statement)) +
        " expected type id: " + std::to_string(field_type);
    if(field_type != SQLITE_NULL)
      real = sqlite3_column_double(m_statement, m_field);
    else
      real = 0.0;
    ++m_field;
    return *this;
  }

  query& query::getField(int& number)
  {
    int field_type = sqlite3_column_type(m_statement, m_field);
    if(!valid() || (field_type != SQLITE_INTEGER && field_type != SQLITE_NULL))
      throw "field type mismatch for field: " + std::to_string(m_field) +
        " of " + std::to_string(sqlite3_column_count(m_statement)) +
        " expected type id: " + std::to_string(field_type);
    if(field_type != SQLITE_NULL)
      number = sqlite3_column_int(m_statement, m_field);
    else
      number = 0;
    ++m_field;
    return *this;
  }
#endif

  query& query::arg(const std::string& text)
  {
    if(++m_arg, !valid())
      throw m_arg;

    throw_if_error_binding(sqlite3_bind_text(m_statement, m_arg, text.c_str(), text.size(), SQLITE_TRANSIENT));
    return *this;
  }

  query& query::getField(std::string& text)
  {
    if(field_valid_or_throw(SQLITE_TEXT))
      text = reinterpret_cast<const char*>(sqlite3_column_text(m_statement, m_field));
    else
      text.clear();
    ++m_field;
    return *this;
  }

  void query::throw_if_error_binding(int errval)
  {
    m_last_error = errval;
    if(m_last_error != SQLITE_OK)
      throw "error setting argument: " + std::to_string(m_arg) +
        " of " + std::to_string(sqlite3_bind_parameter_count(m_statement)) +
        "\n" + sqlite3_errstr(m_last_error);
  }

  bool query::field_valid_or_throw(int expected_type)
  {
    int field_type = sqlite3_column_type(m_statement, m_field);
    if(!valid() || (field_type != expected_type && field_type != SQLITE_NULL))
      throw "field type mismatch for field: " + std::to_string(m_field) +
        " of " + std::to_string(sqlite3_column_count(m_statement)) +
        " expected type id: " + std::to_string(field_type);

    return field_type != SQLITE_NULL;
  }
}
