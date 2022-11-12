#include "simple_sqlite.h"

#include <cstring>

namespace sql
{
  db::db(void) noexcept
    : m_db(nullptr)
  {
  }

  db::~db(void) noexcept
  {
    while(!close()); // wait for sqlite3_close to finish
  }

  bool db::open(const std::string_view& filename) noexcept
  {
    m_last_error = sqlite3_open_v2(filename.data(),
                                   &m_db,
                                   SQLITE_OPEN_READWRITE |
                                   SQLITE_OPEN_CREATE |
                                   SQLITE_OPEN_EXRESCODE
                                   , NULL);
    return m_last_error == SQLITE_OK;
  }

  bool db::close(void) noexcept
  {
    m_last_error = sqlite3_close_v2(m_db);
    if(m_last_error == SQLITE_OK)
      m_db = nullptr;
    return m_last_error == SQLITE_OK;
  }

  bool db::execute(const std::string_view& sql_str) noexcept
  {
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql_str.data(), NULL, NULL, &err);
    if(rc != SQLITE_OK)
    {
      sqlite3_free(err);
      return false;
    }
    return true;
  }

  query db::build_query(const std::string_view& query_str)
  {
    sqlite3_stmt* statement = nullptr;
    int rval = sqlite3_prepare_v2(m_db, query_str.data(), query_str.size(), &statement, NULL);
    if(rval != SQLITE_OK)
      throw "build query: " + std::string(sqlite3_errstr(rval)).append("\ninput: ").append(query_str);
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
      m_last_error = sqlite3_finalize(m_statement);
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

  void query::throw_if_error_binding(int errval)
  {
    m_last_error = errval;
    if(m_last_error != SQLITE_OK)
      throw "error setting argument: " + std::to_string(m_arg) +
        " of " + std::to_string(sqlite3_bind_parameter_count(m_statement)) +
        "\n" + sqlite3_errstr(m_last_error);
  }

  void query::throw_if_bad_field(int expected_type)
  {
    int field_type = sqlite3_column_type(m_statement, m_field);
    if(!valid() || sqlite3_column_type(m_statement, m_field) != expected_type)
      throw "field type mismatch for field: " + std::to_string(m_field) +
        " of " + std::to_string(sqlite3_column_count(m_statement)) +
        " expected type id: " + std::to_string(field_type);
  }

  int query::bind(const std::string& text, use_t use)
    { return sqlite3_bind_text(m_statement, m_arg, text.c_str(), text.size(), reinterpret_cast<sqlite3_destructor_type>(use)); }

  int query::bind(const std::string_view& text, use_t use)
    { return sqlite3_bind_text(m_statement, m_arg, text.data(), text.size(), reinterpret_cast<sqlite3_destructor_type>(use)); }

  int query::bind(const std::wstring& text, use_t use)
    { return sqlite3_bind_text16(m_statement, m_arg, text.c_str(), text.size(), reinterpret_cast<sqlite3_destructor_type>(use)); }

  int query::bind(const std::u16string_view& text, use_t use)
    { return sqlite3_bind_text16(m_statement, m_arg, text.data(), text.size(), reinterpret_cast<sqlite3_destructor_type>(use)); }

  int query::bind(const std::vector<uint8_t>& blob, use_t use)
    { return sqlite3_bind_blob(m_statement, m_arg, blob.data(), blob.size(), reinterpret_cast<sqlite3_destructor_type>(use)); }

  void query::field(std::string& text)
  {
    throw_if_bad_field(SQLITE_TEXT);
    text = reinterpret_cast<const char*>(sqlite3_column_text(m_statement, m_field));
  }

  void query::field(std::wstring& text)
  {
    throw_if_bad_field(SQLITE_TEXT);
    text = static_cast<const wchar_t*>(sqlite3_column_text16(m_statement, m_field));
  }

  void query::field(std::u16string& text)
  {
    throw_if_bad_field(SQLITE_TEXT);
    text = static_cast<const char16_t*>(sqlite3_column_text16(m_statement, m_field));
  }

  void query::field(std::vector<uint8_t>& blob)
  {
    throw_if_bad_field(SQLITE_BLOB);
    blob.resize(sqlite3_column_bytes(m_statement, m_field));
    std::memcpy(blob.data(), sqlite3_column_blob(m_statement, m_field), blob.size());
  }
}
