module;

#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

module farcaster.test.tempdatabase;

namespace farcaster::test::tempdatabase {

TemporaryDatabase::TemporaryDatabase()
{
    const auto uniqueValue = std::chrono::steady_clock::now().time_since_epoch().count();
    m_path = (std::filesystem::temp_directory_path()
        / ("farcaster-native-client-" + std::to_string(uniqueValue) + ".sqlite3"))
                 .string();
}

TemporaryDatabase::~TemporaryDatabase()
{
    std::error_code error{};
    std::filesystem::remove(m_path, error);
    std::filesystem::remove(m_path + "-wal", error);
    std::filesystem::remove(m_path + "-shm", error);
}

const std::string& TemporaryDatabase::path() const noexcept
{
    return m_path;
}

}
