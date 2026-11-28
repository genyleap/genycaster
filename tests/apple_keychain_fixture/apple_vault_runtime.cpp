// Uses the actual Apple vault implementation and OpenSSL with a counting
// stand-in for Security.framework. No real credentials, no OS ACL bypass.
#include "signer_keychain_boundary.hpp"
#include <algorithm>
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <openssl/evp.h>

import farcaster.platform.signer_vault;
import farcaster.capability.signer;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.result;
import farcaster.foundation.error;

namespace farcaster::platform::signer_vault::keychain_boundary {
namespace {
using Slot=std::pair<std::string,std::string>;
std::map<Slot,std::array<unsigned char,seedSize>> items;
unsigned reads=0, writes=0;
bool deny=false;
}
LoadResult load(std::string_view service,std::string_view account,
                std::span<unsigned char,seedSize> destination)
{
    ++reads;
    if(deny)return std::unexpected(Failure{"simulated denial",-128,true});
    const auto it=items.find({std::string(service),std::string(account)});
    if(it==items.end())return LoadState::NotFound;
    std::copy(it->second.begin(),it->second.end(),destination.begin());
    return LoadState::Found;
}
Status save(std::string_view service,std::string_view account,
            std::span<const unsigned char,seedSize> seed)
{
    ++writes;
    auto& value=items[{std::string(service),std::string(account)}];
    std::copy(seed.begin(),seed.end(),value.begin());
    return {};
}
Status erase(std::string_view service,std::string_view account)
{
    items.erase({std::string(service),std::string(account)});
    return {};
}
}

int main()
{
    namespace vault=farcaster::platform::signer_vault;
    namespace kb=vault::keychain_boundary;
    using farcaster::domain::identity::Fid;
    using farcaster::foundation::error::ErrorCode;
    auto opened=vault::open("org.genycaster.protocol-signer");
    assert(opened&&*opened);
    auto& social=**opened;
    auto created=social.ensure(Fid{42});
    assert(created&&*created&&kb::reads==1&&kb::writes==1);
    auto pub=(*created)->publicKey();
    assert(pub&&pub->hex.size()==64&&kb::reads==1);
    auto signature=(*created)->sign("first");
    assert(signature&&signature->bytes.size()==64&&kb::reads==2);
    // Verify an actual OpenSSL Ed25519 signature from the fake OS vault.
    auto bytes=kb::items.at({"org.genycaster.protocol-signer","fid:42"});
    auto* key=EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,nullptr,bytes.data(),bytes.size());
    assert(key);
    auto* md=EVP_MD_CTX_new();
    assert(md&&EVP_DigestVerifyInit(md,nullptr,nullptr,nullptr,key)==1);
    assert(EVP_DigestVerify(md,reinterpret_cast<const unsigned char*>(signature->bytes.data()),
                           signature->bytes.size(),
                           reinterpret_cast<const unsigned char*>("first"),5)==1);
    EVP_MD_CTX_free(md);EVP_PKEY_free(key);
    // New local restore: public inspection doesn't re-read private material.
    auto normal=social.load(Fid{42});
    assert(normal&&*normal&&kb::reads==3);
    assert((*normal)->publicKey()->hex==pub->hex&&kb::reads==3);
    auto one=social.prepareSignature(Fid{42});
    assert(one&&*one&&kb::reads==4);
    assert((*one)->publicKey()->hex==pub->hex&&kb::reads==4);
    assert((*one)->sign("account challenge")&&kb::reads==4);
    auto reused=(*one)->sign("second challenge");
    assert(!reused&&reused.error().code==ErrorCode::Unauthorized&&kb::reads==4);
    // A replaced OS key cannot sign under a stale handle's public identity.
    kb::items.at({"org.genycaster.protocol-signer","fid:42"}).fill(0x51);
    auto changed=(*normal)->sign("must fail");
    assert(!changed&&changed.error().code==ErrorCode::Unauthorized&&kb::reads==5);
    auto revoked=social.prepareSignature(Fid{42});
    assert(revoked&&*revoked&&kb::reads==6);
    auto reopened=vault::open("org.genycaster.protocol-signer");
    assert(reopened&&*reopened);
    assert((**reopened).erase(Fid{42}));
    assert(!(*revoked)->sign("must not sign after erase")&&kb::reads==6);
    kb::deny=true;
    auto denied=social.prepareSignature(Fid{42},true);
    assert(!denied&&denied.error().code==ErrorCode::Unauthorized&&kb::writes==1);
    kb::deny=false;
    auto device=vault::open("org.genycaster.device-session");
    assert(device&&*device);
    auto seal=(**device).ensure(Fid{1});
    assert(seal&&*seal&&kb::writes==2);
    const auto before=kb::reads;
    assert((*seal)->sign("device session seal")&&kb::reads==before);
    assert(!(*seal)->sign("cannot reuse seal handle")&&kb::reads==before);
    std::puts("PASS Apple vault: exact read counts, Ed25519 proof, replacement, denial and revocation");
}
