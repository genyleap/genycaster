#include "signer_keychain_boundary.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace farcaster::platform::signer_vault::keychain_boundary {
namespace {

struct CfRelease final {
    void operator()(const void* value) const noexcept
    {
        if (value != nullptr) {
            CFRelease(value);
        }
    }
};

template <typename T>
using CfPtr = std::unique_ptr<std::remove_pointer_t<T>, CfRelease>;

[[nodiscard]] CfPtr<CFStringRef> makeString(const std::string_view value)
{
    return CfPtr<CFStringRef>{CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(value.data()),
        static_cast<CFIndex>(value.size()),
        kCFStringEncodingUTF8,
        false)};
}

[[nodiscard]] Failure allocationFailure(std::string context)
{
    return Failure{.context = std::move(context)};
}

[[nodiscard]] Failure nativeFailure(std::string context, const OSStatus status)
{
    return Failure{
        .context = std::move(context),
        .nativeStatus = static_cast<std::int64_t>(status),
    };
}

}

LoadResult load(
    const std::string_view service,
    const std::string_view account,
    const std::span<unsigned char, seedSize> destination)
{
    auto serviceRef = makeString(service);
    auto accountRef = makeString(account);
    if (!serviceRef || !accountRef) {
        return std::unexpected(allocationFailure(
            "Failed to allocate Apple Keychain query strings."));
    }

    const void* keys[] = {
        kSecClass,
        kSecAttrService,
        kSecAttrAccount,
        kSecReturnData,
        kSecMatchLimit,
    };
    const void* values[] = {
        kSecClassGenericPassword,
        serviceRef.get(),
        accountRef.get(),
        kCFBooleanTrue,
        kSecMatchLimitOne,
    };
    CfPtr<CFDictionaryRef> query{CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        5,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks)};
    if (!query) {
        return std::unexpected(allocationFailure(
            "Failed to allocate Apple Keychain query."));
    }

    CFTypeRef result{};
    const OSStatus status = SecItemCopyMatching(query.get(), &result);
    if (status == errSecItemNotFound) {
        return LoadState::NotFound;
    }
    if (status != errSecSuccess || result == nullptr
        || CFGetTypeID(result) != CFDataGetTypeID()) {
        if (result != nullptr) {
            CFRelease(result);
        }
        return std::unexpected(nativeFailure(
            "Failed to load protocol signer from Apple Keychain", status));
    }

    CfPtr<CFDataRef> data{static_cast<CFDataRef>(result)};
    if (CFDataGetLength(data.get()) != static_cast<CFIndex>(seedSize)) {
        return std::unexpected(allocationFailure(
            "Apple Keychain protocol signer contains an invalid Ed25519 seed length."));
    }
    CFDataGetBytes(
        data.get(),
        CFRangeMake(0, static_cast<CFIndex>(seedSize)),
        destination.data());
    return LoadState::Found;
}

Status save(
    const std::string_view service,
    const std::string_view account,
    const std::span<const unsigned char, seedSize> seed)
{
    auto serviceRef = makeString(service);
    auto accountRef = makeString(account);
    CfPtr<CFDataRef> data{CFDataCreate(
        kCFAllocatorDefault,
        seed.data(),
        static_cast<CFIndex>(seed.size()))};
    if (!serviceRef || !accountRef || !data) {
        return std::unexpected(allocationFailure(
            "Failed to allocate Apple Keychain signer values."));
    }

    const void* keys[] = {
        kSecClass,
        kSecAttrService,
        kSecAttrAccount,
        kSecValueData,
        kSecAttrAccessible,
        kSecAttrSynchronizable,
    };
    const void* values[] = {
        kSecClassGenericPassword,
        serviceRef.get(),
        accountRef.get(),
        data.get(),
        kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
        kCFBooleanFalse,
    };
    CfPtr<CFDictionaryRef> attributes{CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        6,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks)};
    if (!attributes) {
        return std::unexpected(allocationFailure(
            "Failed to allocate Apple Keychain signer attributes."));
    }

    const OSStatus status = SecItemAdd(attributes.get(), nullptr);
    if (status != errSecSuccess) {
        return std::unexpected(nativeFailure(
            "Failed to save protocol signer in Apple Keychain", status));
    }
    return {};
}

Status erase(const std::string_view service, const std::string_view account)
{
    auto serviceRef = makeString(service);
    auto accountRef = makeString(account);
    if (!serviceRef || !accountRef) {
        return std::unexpected(allocationFailure(
            "Failed to allocate Apple Keychain delete query strings."));
    }

    const void* keys[] = {kSecClass, kSecAttrService, kSecAttrAccount};
    const void* values[] = {kSecClassGenericPassword, serviceRef.get(), accountRef.get()};
    CfPtr<CFDictionaryRef> query{CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        3,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks)};
    if (!query) {
        return std::unexpected(allocationFailure(
            "Failed to allocate Apple Keychain delete query."));
    }

    const OSStatus status = SecItemDelete(query.get());
    if (status == errSecSuccess || status == errSecItemNotFound) {
        return {};
    }
    return std::unexpected(nativeFailure(
        "Failed to delete protocol signer from Apple Keychain", status));
}

}
