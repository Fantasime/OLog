#ifndef OLOG_UTILS_H
#define OLOG_UTILS_H

#include <chrono>
#include <cstdint>

namespace olog {
namespace utils {

/**
 * @brief
 * 获取系统使用的时钟自开始到现在所经过的毫秒数。
 *
 * @return int64_t
 */
inline int64_t GetMsSystemClockInterval() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace utils
}  // namespace olog

#endif