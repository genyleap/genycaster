/*!
 * @file        tests/unit/storage_behavior.cppm
 * @brief       SQLite storage integration test contract.
 *
 * @details
 * Exposes the verification entry point for durable local-state behavior
 * and migration expectations.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

export module farcaster.test.storage;

export namespace farcaster::test::storage {

/**
 * @brief Runs the local SQLite storage integration contract.
 *
 * The contract verifies canonical entity, timeline, continuation, and sync
 * checkpoint round-trips through the public storage modules.
 */
void runLocalStoreRoundTrip();

}
