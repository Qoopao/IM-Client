#ifndef AUTH_MODEL_H
#define AUTH_MODEL_H

#include <QString>

namespace model {
struct AuthModel
{
    enum class Status { Idle, LoggingIn, Success, Failed };
    Status status = Status::Idle;
    QString account;
    QString errorMessage;
    QString token;
};
}

#endif
