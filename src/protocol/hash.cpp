module;

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

module farcaster.protocol.hash;

namespace farcaster::protocol::hash {
namespace {

using Word = std::uint32_t;
using ChainingValue = std::array<Word, 8>;
using BlockWords = std::array<Word, 16>;
using CompressOutput = std::array<Word, 16>;

constexpr std::size_t blockLength = 64U;
constexpr std::size_t chunkLength = 1024U;
constexpr std::uint32_t chunkStart = 1U << 0U;
constexpr std::uint32_t chunkEnd = 1U << 1U;
constexpr std::uint32_t parent = 1U << 2U;
constexpr std::uint32_t root = 1U << 3U;

constexpr ChainingValue initialValue{
    0x6A09E667U,
    0xBB67AE85U,
    0x3C6EF372U,
    0xA54FF53AU,
    0x510E527FU,
    0x9B05688CU,
    0x1F83D9ABU,
    0x5BE0CD19U,
};

constexpr std::array<std::size_t, 16> messagePermutation{
    2U, 6U, 3U, 10U, 7U, 0U, 4U, 13U,
    1U, 11U, 12U, 5U, 9U, 14U, 15U, 8U,
};

struct Output final {
    ChainingValue inputCv{};
    BlockWords blockWords{};
    std::uint64_t counter{};
    std::uint32_t blockLen{};
    std::uint32_t flags{};
};

[[nodiscard]] constexpr Word rotateRight(const Word value, const int distance) noexcept
{
    return std::rotr(value, distance);
}

void mix(
    std::array<Word, 16>& state,
    const std::size_t a,
    const std::size_t b,
    const std::size_t c,
    const std::size_t d,
    const Word x,
    const Word y) noexcept
{
    state[a] = state[a] + state[b] + x;
    state[d] = rotateRight(state[d] ^ state[a], 16);
    state[c] += state[d];
    state[b] = rotateRight(state[b] ^ state[c], 12);
    state[a] = state[a] + state[b] + y;
    state[d] = rotateRight(state[d] ^ state[a], 8);
    state[c] += state[d];
    state[b] = rotateRight(state[b] ^ state[c], 7);
}

void round(std::array<Word, 16>& state, const BlockWords& message) noexcept
{
    mix(state, 0U, 4U, 8U, 12U, message[0], message[1]);
    mix(state, 1U, 5U, 9U, 13U, message[2], message[3]);
    mix(state, 2U, 6U, 10U, 14U, message[4], message[5]);
    mix(state, 3U, 7U, 11U, 15U, message[6], message[7]);

    mix(state, 0U, 5U, 10U, 15U, message[8], message[9]);
    mix(state, 1U, 6U, 11U, 12U, message[10], message[11]);
    mix(state, 2U, 7U, 8U, 13U, message[12], message[13]);
    mix(state, 3U, 4U, 9U, 14U, message[14], message[15]);
}

[[nodiscard]] BlockWords permute(const BlockWords& message) noexcept
{
    BlockWords permuted{};
    for (std::size_t index{}; index < permuted.size(); ++index) {
        permuted[index] = message[messagePermutation[index]];
    }
    return permuted;
}

[[nodiscard]] CompressOutput compress(
    const ChainingValue& cv,
    const BlockWords& blockWords,
    const std::uint64_t counter,
    const std::uint32_t blockLen,
    const std::uint32_t flags) noexcept
{
    std::array<Word, 16> state{
        cv[0], cv[1], cv[2], cv[3], cv[4], cv[5], cv[6], cv[7],
        initialValue[0], initialValue[1], initialValue[2], initialValue[3],
        static_cast<Word>(counter), static_cast<Word>(counter >> 32U), blockLen, flags,
    };

    auto message = blockWords;
    for (std::size_t roundIndex{}; roundIndex < 7U; ++roundIndex) {
        round(state, message);
        if (roundIndex + 1U < 7U) {
            message = permute(message);
        }
    }

    CompressOutput output{};
    for (std::size_t index{}; index < 8U; ++index) {
        output[index] = state[index] ^ state[index + 8U];
        output[index + 8U] = state[index + 8U] ^ cv[index];
    }
    return output;
}

[[nodiscard]] ChainingValue chainingValue(const Output& output) noexcept
{
    const auto compressed = compress(
        output.inputCv,
        output.blockWords,
        output.counter,
        output.blockLen,
        output.flags);
    ChainingValue cv{};
    std::copy_n(compressed.begin(), cv.size(), cv.begin());
    return cv;
}

[[nodiscard]] Word loadWord(const std::byte* bytes, const std::size_t count) noexcept
{
    Word value{};
    for (std::size_t index{}; index < count; ++index) {
        value |= static_cast<Word>(std::to_integer<unsigned char>(bytes[index]))
            << static_cast<unsigned int>(index * 8U);
    }
    return value;
}

[[nodiscard]] BlockWords blockWordsFor(std::span<const std::byte> block) noexcept
{
    BlockWords words{};
    for (std::size_t wordIndex{}; wordIndex < words.size(); ++wordIndex) {
        const auto offset = wordIndex * sizeof(Word);
        if (offset >= block.size()) {
            break;
        }
        const auto count = std::min(sizeof(Word), block.size() - offset);
        words[wordIndex] = loadWord(block.data() + offset, count);
    }
    return words;
}

[[nodiscard]] Output chunkOutput(
    const std::span<const std::byte> chunk,
    const std::uint64_t chunkCounter) noexcept
{
    auto cv = initialValue;
    const auto blockCount = std::max<std::size_t>(1U, (chunk.size() + blockLength - 1U) / blockLength);

    for (std::size_t blockIndex{}; blockIndex < blockCount; ++blockIndex) {
        const auto offset = blockIndex * blockLength;
        const auto remaining = offset < chunk.size() ? chunk.size() - offset : 0U;
        const auto currentLength = std::min(blockLength, remaining);
        const auto currentBlock = currentLength == 0U
            ? std::span<const std::byte>{}
            : chunk.subspan(offset, currentLength);
        const auto words = blockWordsFor(currentBlock);
        auto flags = std::uint32_t{};
        if (blockIndex == 0U) {
            flags |= chunkStart;
        }
        if (blockIndex + 1U == blockCount) {
            flags |= chunkEnd;
            return Output{
                .inputCv = cv,
                .blockWords = words,
                .counter = chunkCounter,
                .blockLen = static_cast<std::uint32_t>(currentLength),
                .flags = flags,
            };
        }
        const auto compressed = compress(
            cv,
            words,
            chunkCounter,
            static_cast<std::uint32_t>(currentLength),
            flags);
        std::copy_n(compressed.begin(), cv.size(), cv.begin());
    }

    return {};
}

[[nodiscard]] Output parentOutput(
    const ChainingValue& left,
    const ChainingValue& right) noexcept
{
    BlockWords words{};
    std::copy(left.begin(), left.end(), words.begin());
    std::copy(right.begin(), right.end(), words.begin() + 8);
    return Output{
        .inputCv = initialValue,
        .blockWords = words,
        .counter = 0U,
        .blockLen = static_cast<std::uint32_t>(blockLength),
        .flags = parent,
    };
}

[[nodiscard]] std::size_t largestPowerOfTwoLessThan(const std::size_t value) noexcept
{
    auto power = std::size_t{1U};
    while ((power << 1U) < value) {
        power <<= 1U;
    }
    return power;
}

[[nodiscard]] Output subtreeOutput(
    const std::span<const std::byte> bytes,
    const std::size_t chunkStartIndex,
    const std::size_t chunkCount) noexcept
{
    if (chunkCount == 1U) {
        const auto offset = chunkStartIndex * chunkLength;
        const auto currentLength = offset < bytes.size()
            ? std::min(chunkLength, bytes.size() - offset)
            : 0U;
        const auto chunk = currentLength == 0U
            ? std::span<const std::byte>{}
            : bytes.subspan(offset, currentLength);
        return chunkOutput(chunk, static_cast<std::uint64_t>(chunkStartIndex));
    }

    const auto leftCount = largestPowerOfTwoLessThan(chunkCount);
    const auto left = subtreeOutput(bytes, chunkStartIndex, leftCount);
    const auto right = subtreeOutput(bytes, chunkStartIndex + leftCount, chunkCount - leftCount);
    return parentOutput(chainingValue(left), chainingValue(right));
}

void storeWord(const Word value, std::byte* destination) noexcept
{
    destination[0] = static_cast<std::byte>(value & 0xFFU);
    destination[1] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    destination[2] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    destination[3] = static_cast<std::byte>((value >> 24U) & 0xFFU);
}

}

Hash20 blake3Digest20(const std::span<const std::byte> bytes) noexcept
{
    const auto chunkCount = std::max<std::size_t>(1U, (bytes.size() + chunkLength - 1U) / chunkLength);
    const auto output = subtreeOutput(bytes, 0U, chunkCount);
    const auto rootOutput = compress(
        output.inputCv,
        output.blockWords,
        0U,
        output.blockLen,
        output.flags | root);

    std::array<std::byte, 32> digest{};
    for (std::size_t index{}; index < 8U; ++index) {
        storeWord(rootOutput[index], digest.data() + index * sizeof(Word));
    }

    Hash20 truncated{};
    std::copy_n(digest.begin(), truncated.size(), truncated.begin());
    return truncated;
}

}
