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
