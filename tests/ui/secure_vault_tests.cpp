#include <QCoreApplication>
#include <QByteArray>
#include <QDebug>
#include <QUuid>

#include <openssl/evp.h>

#include <array>
#include <memory>
#include <string>

import farcaster.capability.signer;
import farcaster.domain.identity;
import farcaster.platform.signer_vault;

namespace {

using farcaster::capability::signer::ISignerVault;
using farcaster::domain::identity::Fid;
constexpr Fid testFid{42};

struct VaultCleanup final {
    ISignerVault* vault{};
    ~VaultCleanup() { if (vault) (void)vault->erase(testFid); }
};

bool verifies(const std::string& publicKey,const std::string& message,
              const std::string& signature)
{
    const QByteArray raw=QByteArray::fromHex(QByteArray::fromStdString(publicKey));
    if(raw.size()!=32 || signature.size()!=64)return false;
    std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)> key{
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519,nullptr,
            reinterpret_cast<const unsigned char*>(raw.constData()),32),EVP_PKEY_free};
    std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> ctx{
        EVP_MD_CTX_new(),EVP_MD_CTX_free};
    return key && ctx && EVP_DigestVerifyInit(ctx.get(),nullptr,nullptr,nullptr,key.get())==1
        && EVP_DigestVerify(ctx.get(),
            reinterpret_cast<const unsigned char*>(signature.data()),signature.size(),
            reinterpret_cast<const unsigned char*>(message.data()),message.size())==1;
}
}

int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    if(qEnvironmentVariableIntValue("GENYCASTER_TEST_SECURE_VAULT")!=1)return 77;
    const std::string prefix="org.genycaster.qa."+QUuid::createUuid()
        .toString(QUuid::WithoutBraces).toStdString();
    const std::array<std::string,4> scopes{"device-seal","social","cloud-a","cloud-b"};
    std::array<std::string,4> keys{};
    for(std::size_t i=0;i<scopes.size();++i){
        const auto service=prefix+"."+scopes[i];
        auto vault=farcaster::platform::signer_vault::open(service);
        if(!vault){qCritical()<<"Secure QA vault unavailable";return 1;}
        VaultCleanup cleanup{vault->get()};
        auto missing=(*vault)->load(testFid);
        if(!missing || *missing)return 2;
        auto signer=(*vault)->ensure(testFid);
        if(!signer || !*signer)return 3;
        auto publicKey=(*signer)->publicKey();
        if(!publicKey)return 4;
        keys[i]=publicKey->hex;
        for(std::size_t j=0;j<i;++j)if(keys[j]==keys[i])return 5;
        auto reopened=farcaster::platform::signer_vault::open(service);
        if(!reopened)return 6;
        auto restored=(*reopened)->load(testFid);
        if(!restored || !*restored)return 7;
        auto restoredKey=(*restored)->publicKey();
        if(!restoredKey || restoredKey->hex!=keys[i])return 8;
        const std::string message="Genycaster isolated secure-vault QA";
        auto signature=(*restored)->sign(message);
        if(!signature || !verifies(keys[i],message,signature->bytes))return 9;
        if(!(*reopened)->erase(testFid))return 10;
        auto erased=(*vault)->load(testFid);
        if(!erased || *erased || (*restored)->sign(message))return 11;
        cleanup.vault=nullptr;
    }
    qInfo()<<"Secure OS vault: four isolated scopes, persistent key reload, Ed25519 verification and erase passed.";
    return 0;
}
