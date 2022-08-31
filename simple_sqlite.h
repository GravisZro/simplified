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
    query& arg(int number);

    query& getField(std::string& text);
    query& getField(int& number);

  protected:
    query(sqlite3_stmt* statement);
  private:
    sqlite3_stmt* m_statement;
    int m_last_error;
    int m_arg;
    int m_field;
    bool m_buffered_filled;
  };

}

#endif // SIMPLE_SQLITE_H
