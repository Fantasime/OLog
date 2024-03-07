#ifndef OLOG_PORTABILITY_H
#define OLOG_PORTABILITY_H

#ifdef __GNUC__
#define OLOG_PRINTF_FORMAT
#define OLOG_PRINTF_FORMAT_ATTR(string_index, first_to_check)                  \
    __attribute__((__format__(__printf__, string_index, first_to_check)))
#endif

#endif