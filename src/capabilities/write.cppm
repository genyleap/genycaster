module;

#include <memory>

export module farcaster.capability.write;

import farcaster.foundation.result;
import farcaster.domain.write;
import farcaster.domain.signer;

export namespace farcaster::capability::write {

/**
 * @brief Provider-neutral canonical Farcaster write codec.
 *
 * Protocol implementations own protobuf/hash/envelope details. Application
 * supplies canonical intents and an already network-verified signer public key.
 */
class ICanonicalWriteCodec {
public:
    /** @brief Destroys the codec. */
    virtual ~ICanonicalWriteCodec() = default;

    /** @brief Builds canonical MessageData/hash and optimistic cast identity. */
    [[nodiscard]] virtual foundation::result::Result<domain::write::PreparedOperation>
    preparePublishCast(
        const domain::write::PublishCastIntent& intent,
        const domain::signer::PublicKey& signerPublicKey) = 0;

    /** @brief Seals a prepared operation with the Ed25519 signature over hashBytes. */
    [[nodiscard]] virtual foundation::result::Result<domain::write::SignedOperation> seal(
        const domain::write::PreparedOperation& operation,
        const domain::signer::Signature& signature) = 0;
};

} // namespace farcaster::capability::write
