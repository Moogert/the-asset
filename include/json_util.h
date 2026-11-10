#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <stdbool.h>
#include <stddef.h>

bool json_get_bool(const char* json, size_t length, const char* key, bool* value);
bool json_get_number(const char* json, size_t length, const char* key, double* value);
bool json_get_string(const char* json, size_t length, const char* key, char* out, size_t out_size);
bool json_is_safe_document(const char* json, size_t length);

#endif
