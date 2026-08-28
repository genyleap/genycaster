// Textual compatibility boundary: SQLite C API types require a private .hpp bridge.
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <sqlite3.h>

namespace farcaster::storage::sqlite {

struct Error final {
    int code{};
    std::string message{};
};

class Connection final {
public:
    [[nodiscard]] static auto open(const std::filesystem::path& path)
        -> std::expected<Connection, Error>;

    ~Connection();

    Connection(const Connection&) = delete;
    auto operator=(const Connection&) -> Connection& = delete;
    Connection(Connection&& other) noexcept;
    auto operator=(Connection&& other) noexcept -> Connection&;

    [[nodiscard]] sqlite3* native() const noexcept;
    [[nodiscard]] auto execute(std::string_view sql) -> std::expected<void, Error>;

private:
    explicit Connection(sqlite3* database) noexcept;
    sqlite3* m_database{};
};

class Statement final {
public:
    [[nodiscard]] static auto prepare(Connection& connection, std::string_view sql)
        -> std::expected<Statement, Error>;

    ~Statement();

    Statement(const Statement&) = delete;
    auto operator=(const Statement&) -> Statement& = delete;
    Statement(Statement&& other) noexcept;
    auto operator=(Statement&& other) noexcept -> Statement&;

    [[nodiscard]] sqlite3_stmt* native() const noexcept;
    void reset() noexcept;

private:
    explicit Statement(sqlite3_stmt* statement) noexcept;
    sqlite3_stmt* m_statement{};
};

[[nodiscard]] auto bindText(sqlite3_stmt* statement, int index, std::string_view value)
    -> std::expected<void, Error>;
[[nodiscard]] auto bindBlob(sqlite3_stmt* statement, int index, std::string_view value)
    -> std::expected<void, Error>;
[[nodiscard]] auto bindOptionalText(
    sqlite3_stmt* statement,
    int index,
    const std::optional<std::string>& value) -> std::expected<void, Error>;
[[nodiscard]] auto columnText(sqlite3_stmt* statement, int column) -> std::string;
[[nodiscard]] auto columnBlob(sqlite3_stmt* statement, int column) -> std::string;
[[nodiscard]] auto optionalColumnText(sqlite3_stmt* statement, int column)
    -> std::optional<std::string>;
[[nodiscard]] auto stepDone(Connection& connection, Statement& statement)
    -> std::expected<void, Error>;
[[nodiscard]] auto makeError(sqlite3* database, std::string_view context) -> Error;

class Transaction final {
public:
    explicit Transaction(Connection& connection) noexcept;
    ~Transaction();

    Transaction(const Transaction&) = delete;
    auto operator=(const Transaction&) -> Transaction& = delete;

    [[nodiscard]] auto begin() -> std::expected<void, Error>;
    [[nodiscard]] auto commit() -> std::expected<void, Error>;

private:
    Connection& m_connection;
    bool m_isActive{false};
};

} // namespace farcaster::storage::sqlite
