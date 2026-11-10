// Bounded helpers for the local simulator request format; not a general-purpose validating JSON parser.
#include "json_util.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const char* find_key(const char* json, size_t length, const char* key)
{
    if (json == NULL || key == NULL)
        return NULL;
    const size_t key_len = strlen(key);
    if (key_len > length)
        return NULL;
    for (size_t i = 0U; i + key_len + 3U <= length; ++i)
    {
        if (json[i] == '"' && memcmp(&json[i + 1U], key, key_len) == 0 &&
            json[i + key_len + 1U] == '"')
        {
            const char* p = &json[i + key_len + 2U];
            const char* end = json + length;
            while (p < end && isspace((unsigned char)*p))
                ++p;
            if (p < end && *p == ':')
                return p + 1;
        }
    }
    return NULL;
}

bool json_get_bool(const char* json, size_t length, const char* key, bool* value)
{
    const char* p = find_key(json, length, key);
    if (p == NULL || value == NULL)
        return false;
    while ((size_t)(p - json) < length && isspace((unsigned char)*p))
        ++p;
    if ((size_t)(p - json) + 4U <= length && memcmp(p, "true", 4U) == 0)
    {
        *value = true;
        return true;
    }
    if ((size_t)(p - json) + 5U <= length && memcmp(p, "false", 5U) == 0)
    {
        *value = false;
        return true;
    }
    return false;
}

bool json_get_number(const char* json, size_t length, const char* key, double* value)
{
    const char* p = find_key(json, length, key);
    if (p == NULL || value == NULL)
        return false;
    while ((size_t)(p - json) < length && isspace((unsigned char)*p))
        ++p;
    errno = 0;
    char* end = NULL;
    double parsed = strtod(p, &end);
    if (end == p || errno == ERANGE || parsed != parsed)
        return false;
    if ((size_t)(end - json) > length)
        return false;
    *value = parsed;
    return true;
}

bool json_get_string(const char* json, size_t length, const char* key, char* out, size_t out_size)
{
    const char* p = find_key(json, length, key);
    if (p == NULL || out == NULL || out_size == 0U)
        return false;
    const char* end = json + length;
    while (p < end && isspace((unsigned char)*p))
        ++p;
    if (p >= end || *p++ != '"')
        return false;
    size_t n = 0U;
    while (p < end && *p != '"')
    {
        if (*p == '\\' || !isprint((unsigned char)*p))
            return false;
        if (n + 1U >= out_size)
            return false;
        out[n++] = *p++;
    }
    if (p >= end)
        return false;
    out[n] = '\0';
    return true;
}

bool json_is_safe_document(const char* json, size_t length)
{
    if (json == NULL || length == 0U || length > 16384U)
        return false;
    int depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (size_t i = 0U; i < length; ++i)
    {
        const unsigned char c = (unsigned char)json[i];
        if (quoted)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (c == '\\')
            {
                escaped = true;
                continue;
            }
            if (c == '"')
                quoted = false;
            else if (c < 0x20U)
                return false;
        }
        else if (c == '"')
        {
            quoted = true;
        }
        else if (c == '{' || c == '[')
        {
            if (++depth > 16)
                return false;
        }
        else if (c == '}' || c == ']')
        {
            if (--depth < 0)
                return false;
        }
    }
    return !quoted && !escaped && depth == 0;
}
