/* Project hello-ok onto the fields this node consumes before cJSON allocation.
 * jsmn (MIT), pinned at 25647e692c7906b96ffd2b05ca54c097948e879c.
 * No credentials or frame content are logged. Other messages stay unchanged.
 */
#pragma once
#include <stdlib.h>
#include <string.h>
#define JSMN_STATIC
#define JSMN_STRICT
#include "vendor/jsmn.h"

static int hello_field(const char *s, const jsmntok_t *t, int n, int object,
                       const char *key)
{
    if (object < 0 || t[object].type != JSMN_OBJECT) return -1;
    for (int i = object + 1; i + 1 < n && t[i].start < t[object].end;) {
        if (t[i].type != JSMN_STRING || t[i].size != 1) return -1;
        int value = i + 1;
        if ((size_t)(t[i].end - t[i].start) == strlen(key) &&
            memcmp(s + t[i].start, key, strlen(key)) == 0) return value;
        int end = t[value].end;
        i = value + 1;
        while (i < n && t[i].start < end) ++i;
    }
    return -1;
}

static int hello_matches(const char *s, const jsmntok_t *t, int i,
                         jsmntype_t type, const char *value)
{
    return i >= 0 && t[i].type == type &&
        (size_t)(t[i].end - t[i].start) == strlen(value) &&
        memcmp(s + t[i].start, value, strlen(value)) == 0;
}

/* NULL means use the ordinary parser, including failure/error messages. */
static char *esp_openclaw_node_compact_hello(const char *s)
{
    const size_t length = strlen(s);
    jsmn_parser parser;
    jsmn_init(&parser);
    int n = jsmn_parse(&parser, s, length, NULL, 0);
    if (n <= 0 || n > 4096) return NULL;
    jsmntok_t *tokens = malloc((size_t)n * sizeof(*tokens));
    if (!tokens) return NULL;
    jsmn_init(&parser);
    if (jsmn_parse(&parser, s, length, tokens, n) != n ||
        tokens[0].type != JSMN_OBJECT) { free(tokens); return NULL; }
    size_t tail = (size_t)tokens[0].end;
    while (tail < length && (s[tail] == ' ' || s[tail] == '\t' ||
           s[tail] == '\r' || s[tail] == '\n')) ++tail;
    if (tail != length) { free(tokens); return NULL; }
    int type = hello_field(s, tokens, n, 0, "type");
    int ok = hello_field(s, tokens, n, 0, "ok");
    int id = hello_field(s, tokens, n, 0, "id");
    int payload = hello_field(s, tokens, n, 0, "payload");
    int hello_type = hello_field(s, tokens, n, payload, "type");
    if (!hello_matches(s, tokens, type, JSMN_STRING, "res") ||
        !hello_matches(s, tokens, ok, JSMN_PRIMITIVE, "true") ||
        id < 0 || tokens[id].type != JSMN_STRING ||
        !hello_matches(s, tokens, hello_type, JSMN_STRING, "hello-ok")) {
        free(tokens); return NULL;
    }
    const char *keys[] = {"type", "id", "ok", "type", "auth", "pluginSurfaceUrls"};
    int fields[] = {type, id, ok, hello_type,
        hello_field(s, tokens, n, payload, "auth"),
        hello_field(s, tokens, n, payload, "pluginSurfaceUrls")};
    size_t bytes = 32;
    for (int j = 0; j < 6; ++j) if (fields[j] >= 0)
        bytes += strlen(keys[j]) + 8 + tokens[fields[j]].end - tokens[fields[j]].start;
    char *out = malloc(bytes);
    if (!out) { free(tokens); return NULL; }
    char *write = out;
    *write++ = '{';
    for (int j = 0; j < 6; ++j) {
        if (j == 3) {
            const char prefix[] = ",\"payload\":{";
            memcpy(write, prefix, sizeof(prefix) - 1); write += sizeof(prefix) - 1;
        }
        int i = fields[j];
        if (i < 0) continue;
        if (j != 0 && j != 3) *write++ = ',';
        *write++ = '"';
        size_t keylen = strlen(keys[j]);
        memcpy(write, keys[j], keylen); write += keylen;
        *write++ = '"'; *write++ = ':';
        int start = tokens[i].start, end = tokens[i].end;
        if (tokens[i].type == JSMN_STRING) { --start; ++end; }
        memcpy(write, s + start, end - start); write += end - start;
    }
    *write++ = '}'; *write++ = '}'; *write = '\0';
    free(tokens);
    return out;
}
