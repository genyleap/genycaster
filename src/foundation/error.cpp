module;

#include <string>
#include <utility>

module farcaster.foundation.error;

namespace farcaster::foundation::error {

Error Error::make(const ErrorCode code, std::string message)
{
    return Error{code, std::move(message)};
}

}
