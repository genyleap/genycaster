#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

import farcaster.platform.custody_recovery;
import farcaster.protocol.message;

namespace {
std::array<unsigned char,32> sha256(const std::string& text) {
    std::array<unsigned char,32> result{};
    unsigned int n{};
    if (EVP_Digest(text.data(),text.size(),result.data(),&n,EVP_sha256(),nullptr)!=1 || n!=32)
        std::abort();
    return result;
}
std::string hex(std::string_view input){
    constexpr char chars[]="0123456789abcdef";
    std::string s="0x";
    for (unsigned char c: input){s+=chars[c>>4U];s+=chars[c&0xfU];}
    return s;
}
std::string decodeHex(std::string_view input) {
    if(input.starts_with("0x"))input.remove_prefix(2);
    std::string result{};
    for(std::size_t i=0;i+1<input.size();i+=2)
        result.push_back(static_cast<char>(std::stoi(std::string{input.substr(i,2)},nullptr,16)));
    return result;
}
}
int main(){
    using namespace farcaster::platform::custody_recovery;
    constexpr std::string_view mnemonic="test test test test test test test test test test test junk";
    const auto expected=derivePublicCustodyAddress(mnemonic);
    if(!expected)return 1;
    constexpr std::uint64_t height=142;
    const std::string anchor="0x"+std::string(64,'a');
    const std::string addr=decodeHex(*expected);
    const std::string anchorBytes=decodeHex(anchor);
    std::string nonce(8,0);
    bool found=false;
    for(std::uint64_t n=0;n<2048;n++){
        for(int i=7;i>=0;i--) nonce[7-i]=static_cast<char>(n>>(i*8));
        const auto h=sha256(std::string("hyper.onboard.pow:")+addr+anchorBytes+nonce);
        if((h[0]&0x80U)==0){ found=true;break; }
    }
    if(!found)return 2;
    const auto mined = solveNativeOnboardPow(*expected, anchor, 12, 200'000);
    if (!mined || mined->size() != 18) return 8;
    const auto minedSignature = signNativeOnboard(mnemonic, height, anchor, *mined, 12);
    if (!minedSignature || minedSignature->address != *expected) return 9;
    if (solveNativeOnboardPow(*expected, anchor, 256, 32)) return 10;
    if (solveNativeOnboardPow("invalid", anchor, 12, 200)) return 11;
    const auto signedProof=signNativeOnboard(mnemonic,height,anchor,hex(nonce),1);
    if(!signedProof || signedProof->address!=*expected
        ||signedProof->signature.size()!=132)return 3;
    auto invalid=signNativeOnboard(mnemonic,height,anchor,hex(std::string(8,'\xff')),256);
    if(invalid)return 4;
    auto message=farcaster::protocol::message::encodeNativeOnboard({
        .custodyAddress=signedProof->address,
        .custodySignature=signedProof->signature,
        .anchorHeight=height,
        .anchorHash=anchor,
        .powNonce=hex(nonce),
        .difficultyBits=1,
        .timestamp=123456,
        .network=farcaster::protocol::message::Network::Mainnet
    });
    if(!message || message->empty())return 5;
    // The outer Message protobuf must contain field 1 (data),
    // field 2 (20-byte hash), field 3 (scheme BLAKE3) and field 7 (data_bytes).
    if(message->front()!=char{0x0a} ||
       message->find(std::string{"\x12\x14",2})==std::string::npos ||
       message->find(std::string{"\x18\x01",2})==std::string::npos ||
       message->find(std::string{"\x3a",1})==std::string::npos)return 6;
    auto malformed=farcaster::protocol::message::encodeNativeOnboard({
        .custodyAddress=signedProof->address,
        .custodySignature="invalid",
        .anchorHeight=height,
        .anchorHash=anchor,
        .powNonce=hex(nonce),
        .difficultyBits=1,
        .timestamp=123456
    });
    if(malformed)return 7;
    std::cout<<"HyperSnap native EIP712 proof and protobuf registration envelope passed\n";
}
