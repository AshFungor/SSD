// Abseil
#include <absl/status/status.h>

// pulseaudio
#include <pulse/utf8.h>
#include <pulse/xmalloc.h>

// STD
#include <cassert>
#include <cstdint>
#include <cstring>

// UTF-8 validation utility functions.

namespace {

    static constexpr char FilterCharacter = '_';

    bool isUnicodeValid(std::uint32_t character) {
        if (character >= 0x110000) {                            /* End of unicode space */
            return false;
        } else if ((character & 0xFFFFF800) == 0xD800) {        /* Reserved area for UTF-16 */
            return false;
        } if ((character >= 0xFDD0) && (character <= 0xFDEF)) { /* Reserved */
            return false;
        } if ((character & 0xFFFE) == 0xFFFE) {                 /* BOM (Byte Order Mark) */
            return false;
        }

        return true;
    }

    bool isContinuationChar(std::uint8_t ch) {
        return (ch & 0xc0) == 0x80;
    }

    void mergeContinuationChar(std::uint32_t* uCh, std::uint8_t ch) {
        *uCh <<= 6;
        *uCh |= ch & 0x3f;
    }

    char* utf8Validate(const char* str, char* output) {
        std::uint32_t value = 0;
        std::uint32_t min = 0;
        std::uint8_t* out;

        static auto errorFallback = [](std::uint8_t*& out, const std::uint8_t*& pointer, const std::uint8_t* last) -> bool {
            if (out) {
                *out = FilterCharacter;
                /* We retry at the next character */
                pointer = last;
                ++out;
                return true;
            }
            return false;
        };

        assert(str);

        out = reinterpret_cast<std::uint8_t*>(output);
        for (const std::uint8_t* pointer = reinterpret_cast<const uint8_t*>(str); *pointer; ++pointer) {
            if (*pointer < 128) {
                if (out) {
                    *out = *pointer;
                }

                continue;
            }

            const std::uint8_t* last = pointer;
            std::size_t size = 0;

            if ((*pointer & 0xe0) == 0xc0) { /* 110xxxxx two-char seq. */
                size = 2;
                min = 128;
                value = static_cast<uint32_t>(*pointer & 0x1e);
            } else if ((*pointer & 0xf0) == 0xe0) { /* 1110xxxx three-char seq.*/
                size = 3;
                min = (1 << 11);
                value = static_cast<uint32_t>(*pointer & 0x0f);
            } else if ((*pointer & 0xf8) == 0xf0) { /* 11110xxx four-char seq */
                size = 4;
                min = (1 << 16);
                value = static_cast<uint32_t>(*pointer & 0x07);
            } else {
                if (bool status = errorFallback(out, pointer, last); !status) {
                    return nullptr;
                } else {
                    continue;
                }
            }

            std::size_t i = size - 1;
            while (i--) {
                ++pointer;
                if (!isContinuationChar(*pointer)) {
                    if (bool status = errorFallback(out, pointer, last); !status) {
                        return nullptr;
                    } else {
                        continue;
                    }
                }
                mergeContinuationChar(&value, *pointer);
            }

            if (value < min || !isUnicodeValid(value)) {
                if (bool status = errorFallback(out, pointer, last); !status) {
                    return nullptr;
                } else {
                    continue;
                }
            }

            if (out) {
                std::memcpy(out, last, static_cast<std::size_t>(size));
                out += size;
            }
        }

        if (out) {
            *out = '\0';
            return output;
        }

        return const_cast<char*>(str);
    }

}

char* pa_utf8_valid(const char* str) {
    return utf8Validate(str, nullptr);
}

char* pa_utf8_filter(const char* str) {
    char *new_str;

    assert(str);
    new_str = static_cast<char*>(pa_xmalloc(std::strlen(str) + 1));
    return utf8Validate(str, new_str);
}

char* pa_utf8_to_locale (const char* str) {
    assert(str);

    return pa_ascii_filter(str);
}

char* pa_locale_to_utf8 (const char* str) {
    assert(str);

    if (pa_utf8_valid(str)) {
        return pa_xstrdup(str);
    }

    return nullptr;
}

char* pa_ascii_valid(const char* str) {
    assert(str);

    for (const char* p = str; *p; ++p) {
        if (static_cast<unsigned char>(*p) >= 128) {
            return nullptr;
        }
    }

    return const_cast<char*>(str);
}

char* pa_ascii_filter(const char* str) {
    char* s;
    char* d;
    assert(str);

    char* r = pa_xstrdup(str);

    for (s = r, d = r; *s; ++s) {
        if (static_cast<unsigned char>(*s) < 128) {
            *(d++) = *s;
        }
    }

    *d = 0;
    return r;
}