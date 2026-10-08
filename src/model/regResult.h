#ifndef REG_RESULT_H
#define REG_RESULT_H

#include <QString>
#include <cstdint>

namespace model {

enum class RegError
{
    None = 0,
    InvalidInput,
    RateLimited,
    NetworkError,
    ServerRejected,
    ServerNotConfirmed,
    InvalidResponse,
    LocalStorageError,
};

struct RegResult
{
    RegError error = RegError::None;
    QString message;
    QString serverCode;
    uint64_t account = 0;
};

} // namespace model

#endif // REG_RESULT_H
