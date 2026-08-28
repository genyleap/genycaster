module;

#include <chrono>

module farcaster.foundation.time;

namespace farcaster::foundation::time {

Timestamp now()
{
    return std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::now());
}

}
