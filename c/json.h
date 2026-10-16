/* A small JSON reader (a tree of values) for the link's presence files and messages. */
#ifndef JSON_H
#define JSON_H
#include <stddef.h>

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JType;
typedef struct J {
    JType type;
    double num;        /* J_NUM, J_BOOL (0/1) */
    int is_int;        /* J_NUM written without a fraction or exponent */
    char *str;         /* J_STR */
    struct J **kids;   /* J_ARR items / J_OBJ values */
    char **keys;       /* J_OBJ */
    int n;
} J;

J *json_parse(const char *s, size_t len); /* NULL if malformed (or trailing junk) */
void json_free(J *j);
const J *json_get(const J *obj, const char *key); /* NULL if not an object or no such key (the last one wins, like Python) */
#endif
