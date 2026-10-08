#ifndef REG_MODEL_H
#define REG_MODEL_H

#include "regResult.h"

namespace model {
struct RegModel
{
    enum class Status { Idle, Registering, Success, Failed };
    Status status = Status::Idle;
    RegResult result;
};
}

#endif
