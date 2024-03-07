#ifndef OLOG_OLOG_CONFIG_H
#define OLOG_OLOG_CONFIG_H

#include <fcntl.h>
#include <liburing.h>
#include <unistd.h>

namespace olog {
namespace config {

static const uint32_t STORAGE_BUFFER_SIZE = 1024 * 1024;

static const int LOG_FILE_FLAGS =
    O_CREAT | O_APPEND | O_RDWR | O_DSYNC | O_NOATIME;

static const size_t DOUBLE_BUFFER_SIZE = 1024 * 1024 * 8;

static const uint32_t IO_URING_ENTRIES = 1;

static const unsigned int IO_URING_INIT_FLAGS = 0;

}  // namespace config
}  // namespace olog

#endif