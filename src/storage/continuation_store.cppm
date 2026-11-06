/*!
 * @file        src/storage/continuation_store.cppm
 * @brief       SQLite provider continuation mapping.
 *
 * @details
 * Persists opaque provider-affine cursors while keeping their
 * implementation details outside local application ports.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <filesystem>
#include <memory>
#include <string>

export module farcaster.storage.continuation;

import farcaster.capability.model;
import farcaster.capability.pagination;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.provider.model;

export namespace farcaster::storage::continuation {

/**
 * @brief Infrastructure-only provider-affine continuation record.
 *
 * The provider cursor is intentionally confined to routing/storage composition
 * and must never cross into domain, application, presentation, or QML APIs.
 */
struct ContinuationRecord final {
    capability::pagination::ContinuationToken token{};
    capability::model::CapabilityId capabilityId{};
    provider::model::ProviderId providerId{};
    std::string providerCursor{};
    foundation::time::Timestamp createdAt{};
};

/**
 * @brief Move-only SQLite-backed continuation mapping adapter.
 *
 * This is a concrete infrastructure adapter, not an application abstraction.
 * Construction performs database I/O and reports recoverable failures through
 * openContinuationStore().
 */
class ContinuationStore final {
public:
    /** @brief Releases the owned SQLite continuation implementation. */
    ~ContinuationStore();

    /** @brief Copy construction is disabled because the store uniquely owns its implementation. */
    ContinuationStore(const ContinuationStore&) = delete;

    /** @brief Copy assignment is disabled because the store uniquely owns its implementation. */
    auto operator=(const ContinuationStore&) -> ContinuationStore& = delete;

    /** @brief Transfers ownership from another continuation store. */
    ContinuationStore(ContinuationStore&& other) noexcept;

    /** @brief Transfers ownership from another continuation store. */
    auto operator=(ContinuationStore&& other) noexcept -> ContinuationStore&;

    /**
     * @brief Persists a provider cursor and returns an application-safe token.
     * @param capabilityId Capability that owns the provider cursor.
     * @param providerId Provider that issued the cursor.
     * @param providerCursor Opaque provider cursor moved into storage.
     * @return Application-safe token or a structured storage failure.
     */
    [[nodiscard]] foundation::result::Result<capability::pagination::ContinuationToken> bind(
        const capability::model::CapabilityId& capabilityId,
        const provider::model::ProviderId& providerId,
        std::string providerCursor);

    /**
     * @brief Resolves an application-safe token to infrastructure-only state.
     * @param token Application-safe continuation token.
     * @return Provider-affine record or a structured not-found/storage failure.
     */
    [[nodiscard]] foundation::result::Result<ContinuationRecord> resolve(
        const capability::pagination::ContinuationToken& token);

    /**
     * @brief Removes a continuation mapping.
     * @param token Application-safe continuation token.
     * @return Success or a structured storage failure.
     */
    [[nodiscard]] foundation::result::Status erase(
        const capability::pagination::ContinuationToken& token);

private:
    struct Impl;
    explicit ContinuationStore(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> m_impl{};

    friend foundation::result::Result<ContinuationStore>
    openContinuationStore(const std::filesystem::path& path);
};

/**
 * @brief Opens the SQLite continuation adapter at path.
 * @param path SQLite database path owned by the returned store after open completes.
 * @return Move-only store or a structured storage failure.
 */
[[nodiscard]] foundation::result::Result<ContinuationStore>
openContinuationStore(const std::filesystem::path& path);

}
