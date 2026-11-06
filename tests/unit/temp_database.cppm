/*!
 * @file        tests/unit/temp_database.cppm
 * @brief       RAII temporary SQLite test database.
 *
 * @details
 * Creates uniquely owned disposable database files and removes journal
 * sidecars on destruction.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <string>

export module farcaster.test.tempdatabase;

export namespace farcaster::test::tempdatabase {

/**
 * @brief Owns a unique temporary SQLite path for integration tests.
 *
 * The object removes the database file and SQLite WAL/SHM sidecars on
 * destruction. It is verification-only infrastructure and is never part of
 * the production API.
 */
class TemporaryDatabase final {
public:
    /** @brief Creates a unique path in the system temporary directory. */
    TemporaryDatabase();

    /** @brief Removes the temporary database and SQLite sidecar files. */
    ~TemporaryDatabase();

    /** @brief Copy construction is disabled because one object owns one cleanup path. */
    TemporaryDatabase(const TemporaryDatabase&) = delete;

    /** @brief Copy assignment is disabled because one object owns one cleanup path. */
    auto operator=(const TemporaryDatabase&) -> TemporaryDatabase& = delete;

    /** @brief Move construction is disabled to keep cleanup ownership stationary in tests. */
    TemporaryDatabase(TemporaryDatabase&&) = delete;

    /** @brief Move assignment is disabled to keep cleanup ownership stationary in tests. */
    auto operator=(TemporaryDatabase&&) -> TemporaryDatabase& = delete;

    /**
     * @brief Returns the owned database path as UTF-8-compatible native text.
     * @return Stable path text valid for this object's lifetime.
     */
    [[nodiscard]] const std::string& path() const noexcept;

private:
    std::string m_path{};
};

}
