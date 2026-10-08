#ifndef LOGIN_MODEL_H
#define LOGIN_MODEL_H

#include <QString>

namespace model {

// 登录结果（Model 层的数据载体）
enum class LoginError
{
    None = 0,           // 成功
    InvalidCredentials, // 账号或密码错误（401）
    RateLimited,        // 尝试过多被限流（429）
    NetworkError,       // 断网 / 超时 / 连接失败 / TLS 校验失败
    ServerRejected,     // 服务端业务拒绝
    Cancelled,          // 用户取消
};

struct LoginResult
{
    LoginError error = LoginError::None;
    QString message;
};

} // namespace model

#endif // LOGIN_MODEL_H
