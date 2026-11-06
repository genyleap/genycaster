/*!
 * @file        src/capabilities/write_submit.cppm
 * @brief       Signed message submission and confirmation port.
 *
 * @details
 * Separates transport submission from authoritative reconciliation for
 * ambiguous protocol write outcomes.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

export module farcaster.capability.write_submit;

import farcaster.foundation.result;
import farcaster.domain.write;

export namespace farcaster::capability::write {

/** @brief Network-authoritative reconciliation state for one canonical write. */
enum class ConfirmationState {
    Absent,
    Confirmed,
};

/**
 * @brief Blocking provider-neutral submission/reconciliation port.
 *
 * Implementations may perform network I/O. Application must call this port only
 * from an I/O executor. Reconciliation is required to resolve crash/timeout/
 * duplicate ambiguity before a pending operation is re-submitted.
 */
class IWriteSubmitter {
public:
    /** @brief Destroys the submitter. */
    virtual ~IWriteSubmitter() = default;

    /** @brief Submits one already-signed canonical protocol message. */
    [[nodiscard]] virtual foundation::result::Status submit(
        const domain::write::SignedOperation& operation) = 0;

    /** @brief Checks authoritative network state for the exact canonical operation identity. */
    [[nodiscard]] virtual foundation::result::Result<ConfirmationState> reconcile(
        const domain::write::SignedOperation& operation) = 0;
};

} // namespace farcaster::capability::write
