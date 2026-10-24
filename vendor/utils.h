/*
utils.h - A high level stb-style utility library for writing contemporary C code.

Provides:

- Custom memory allocators
- String views and builders
- Defer functionality
- Dynamic arrays, hash maps and other collections
- File utilities
- Command-line utils_flag parsing

Inspired by modern systems programming languages and
common patterns used in contemporary C code.

Some parts were written from utils_scratch,
while others are adapted from public-domain code by various authors.

Usage:
  // In exactly one translation unit:
  #define UTILS_IMPLEMENTATION
  #include "utils.h"

  // Optional: expose the short legacy names too.
  #define UTILS_STRIP_PREFIX
  #include "utils.h"

Notes:
  - utils.h requires C23 or later.
  - `utils_defer`, `utils_da_*`, and `utils_map_*` rely on GNU C extensions.
  - Any section that relies on allocations:
      * `str_builder`
      * `da`
      * `map`
      * `flags`
    MUST have a valid allocator set in the `alloc` field of the struct before use.
    Using these without a valid allocator will cause an instant assertion failure.

Dual-licensed under either of these, pick one:

A) The Unlicense (Public Domain)

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org/>

B) MIT

Copyright (c) 2026 Antoni Marzec

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the “Software”),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error utils.h requires C23 or later
#endif

static_assert(sizeof(void*) == 8, "utils.h requires 64-bit pointers");

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef float    f32;
typedef double   f64;

typedef size_t    usz;
typedef ptrdiff_t isz;

#define U8_MAX  UINT8_MAX
#define U16_MAX UINT16_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX

#define I8_MIN  INT8_MIN
#define I8_MAX  INT8_MAX
#define I16_MIN INT16_MIN
#define I16_MAX INT16_MAX
#define I32_MIN INT32_MIN
#define I32_MAX INT32_MAX
#define I64_MIN INT64_MIN
#define I64_MAX INT64_MAX

#define nil nullptr

#define UTILS_TODO(message) assert(0 && "UTILS_TODO:" message)

#ifndef UTILS_DEF
#define UTILS_DEF extern
#endif

typedef struct {
  void* ctx;

  void* (*alloc)(void* ctx, usz size);
  void* (*realloc)(void* ctx, void* ptr, usz new_size);
  void  (*free)(void* ctx, void* ptr);
  void  (*reset)(void* ctx);
} utils_allocator;

#define utils_a_alloc(a, size) ((a).alloc((a).ctx, (size)))
#define utils_a_new(a, T) ((T*)utils_a_alloc((a), sizeof(T)))
#define utils_a_realloc(a, ptr, new_size) ((a).realloc((a).ctx, (ptr), (new_size)))
#define utils_a_free(a, ptr) ((a).free((a).ctx, (ptr)))
#define utils_a_reset(a) ((a).reset((a).ctx))

typedef struct {
  void** allocations;
  usz count;
  usz capacity;
} utils_tracking_allocator;

typedef struct utils_arena_block utils_arena_block;

typedef struct {
  utils_arena_block* first;
  utils_arena_block* current;
  usz block_size;
} utils_arena;

#ifndef UTILS_ARENA_MIN_BLOCK_SIZE
#define UTILS_ARENA_MIN_BLOCK_SIZE 256
#endif

typedef struct {
  utils_arena_block* block;
  usz used;
} utils_arena_save_point;

typedef struct {
  utils_arena* backing;
  utils_arena_save_point save;
  utils_allocator alloc;
} utils_scratch;

UTILS_DEF bool utils_tracking_allocator_track_ptr(utils_tracking_allocator* tracker, void* ptr);
UTILS_DEF void utils_tracking_allocator_untrack_ptr(utils_tracking_allocator* tracker, void* ptr);

UTILS_DEF utils_arena_save_point utils_arena_save(utils_arena* a);
UTILS_DEF void utils_arena_restore(utils_arena* a, utils_arena_save_point save);
UTILS_DEF utils_scratch utils_scratch_begin_with(const utils_scratch* conflict);
UTILS_DEF utils_scratch utils_scratch_begin(void);
UTILS_DEF void utils_scratch_end(utils_scratch s);

UTILS_DEF utils_allocator utils_make_simple_allocator(void);
UTILS_DEF utils_allocator utils_make_tracking_allocator_from(utils_tracking_allocator* tracker);
UTILS_DEF utils_allocator utils_make_allocator_from_arena(utils_arena* arena_storage);

#define utils_make_tracking_allocator(tracker) utils_make_tracking_allocator_from((tracker))
#define UTILS__MAKE_ARENA_ALLOCATOR_DEFAULT() utils_make_allocator_from_arena(&(utils_arena){0})

#define UTILS__MAKE_ALLOCATOR_0() utils_make_simple_allocator()

#define UTILS__MAKE_ALLOCATOR_1(arg)                               \
  _Generic((arg),                                                  \
    utils_tracking_allocator*: utils_make_tracking_allocator_from, \
    utils_arena*: utils_make_allocator_from_arena                  \
  )(arg)

#define UTILS__GET_MACRO(_0, _1, NAME, ...) NAME

#define utils_make_arena_allocator(...)         \
  UTILS__GET_MACRO(_ __VA_OPT__(,) __VA_ARGS__, \
    utils_make_allocator_from_arena,            \
    UTILS__MAKE_ARENA_ALLOCATOR_DEFAULT)        \
  (__VA_ARGS__)

#define utils_make_allocator(...)               \
  UTILS__GET_MACRO(_ __VA_OPT__(,) __VA_ARGS__, \
    UTILS__MAKE_ALLOCATOR_1,                    \
    UTILS__MAKE_ALLOCATOR_0)                    \
  (__VA_ARGS__)

#if defined(__clangd__)

#define utils_defer(code) code

#elif defined(__GNUC__) && !defined(__clang__) && !defined(__cplusplus)

#define UTILS__CONCAT_INTERNAL(x, y) x##y
#define UTILS__CONCAT(x, y) UTILS__CONCAT_INTERNAL(x, y)

#define UTILS__DEFER_INTERNAL(id, code)                           \
  void UTILS__CONCAT(utils__defer_func_, id)(void* p) {           \
    (void)p;                                                      \
    code;                                                         \
  }                                                               \
                                                                  \
  __attribute__((cleanup(UTILS__CONCAT(utils__defer_func_, id)))) \
  int UTILS__CONCAT(utils__defer_var_, id) = 0

#define utils_defer(code) UTILS__DEFER_INTERNAL(__COUNTER__, code)

#else

#define utils_defer(...) \
  static_assert(0, "utils_defer is only supported with GCC that has nested functions support enabled")

#endif

#include <string.h>

typedef struct {
  usz count;
  const char* data;
} utils_str_view;

UTILS_DEF utils_str_view utils_str_view_make(const char* data, usz count);
UTILS_DEF utils_str_view utils_str_view_consume_while(utils_str_view* sv, int (*p)(int x));
UTILS_DEF utils_str_view utils_str_view_consume_by_delim(utils_str_view* sv, char delim);
UTILS_DEF utils_str_view utils_str_view_consume_left(utils_str_view* sv, usz n);
UTILS_DEF utils_str_view utils_str_view_consume_right(utils_str_view* sv, usz n);
UTILS_DEF utils_str_view utils_str_view_from_cstr(const char* cstr);

#define utils_str_view_eq(a, b) _Generic((a), \
  utils_str_view: utils_str_view_eq_sv,       \
  const char*: utils_str_view_eq_cstr,        \
  char*: utils_str_view_eq_cstr               \
)(a, b)

UTILS_DEF bool utils_str_view_eq_sv(utils_str_view a, utils_str_view b);
UTILS_DEF bool utils_str_view_eq_cstr(utils_str_view sv, const char* cstr);

#define utils_str_view_ends_with(sv, suffix) _Generic((suffix), \
  utils_str_view: utils_str_view_ends_with_sv,                  \
  const char*: utils_str_view_ends_with_cstr,                   \
  char*: utils_str_view_ends_with_cstr                          \
)(sv, suffix)

UTILS_DEF bool utils_str_view_ends_with_sv(utils_str_view sv, utils_str_view suffix);
UTILS_DEF bool utils_str_view_ends_with_cstr(utils_str_view sv, const char* cstr);

#define utils_str_view_starts_with(sv, prefix) _Generic((prefix), \
  utils_str_view: utils_str_view_starts_with_sv,                  \
  const char*: utils_str_view_starts_with_cstr,                   \
  char*: utils_str_view_starts_with_cstr                          \
)(sv, prefix)

UTILS_DEF bool utils_str_view_starts_with_sv(utils_str_view sv, utils_str_view expected_prefix);
UTILS_DEF bool utils_str_view_starts_with_cstr(utils_str_view sv, const char* cstr);

#define utils_str_view_remove_prefix(sv, prefix) _Generic((prefix), \
  utils_str_view: utils_str_view_remove_prefix_sv,                  \
  const char*: utils_str_view_remove_prefix_cstr,                   \
  char*: utils_str_view_remove_prefix_cstr                          \
)(sv, prefix)

UTILS_DEF bool utils_str_view_remove_prefix_sv(utils_str_view* sv, utils_str_view prefix);
UTILS_DEF bool utils_str_view_remove_prefix_cstr(utils_str_view* sv, const char* prefix);

#define utils_str_view_remove_suffix(sv, suffix) _Generic((suffix), \
  utils_str_view: utils_str_view_remove_suffix_sv,                  \
  const char*: utils_str_view_remove_suffix_cstr,                   \
  char*: utils_str_view_remove_suffix_cstr                          \
)(sv, suffix)

UTILS_DEF bool utils_str_view_remove_suffix_sv(utils_str_view* sv, utils_str_view suffix);
UTILS_DEF bool utils_str_view_remove_suffix_cstr(utils_str_view* sv, const char* suffix);

UTILS_DEF utils_str_view utils_str_view_trim_left(utils_str_view sv);
UTILS_DEF utils_str_view utils_str_view_trim_right(utils_str_view sv);
UTILS_DEF utils_str_view utils_str_view_trim(utils_str_view sv);

UTILS_DEF int utils_str_view_find(utils_str_view sv, char c);
UTILS_DEF utils_str_view utils_str_view_substr(utils_str_view sv, usz start, usz length);
UTILS_DEF char utils_str_view_at(utils_str_view sv, isz index);

UTILS_DEF char* utils_str_view_to_cstr(utils_str_view sv, utils_allocator alloc);

typedef struct {
  char* data;
  usz count;
  usz capacity;
  utils_allocator alloc;
} utils_str_builder;

#define utils_str_builder_new(a) { .alloc = (a) }

#define utils_sfmt "%.*s"
#define utils_sfmtarg(sv) (int)(sv).count, (sv).data

UTILS_DEF bool utils_str_builder_reserve(utils_str_builder* sb, usz additional);
UTILS_DEF bool utils_str_builder_append_bytes(utils_str_builder* sb, const void* data, usz size);
UTILS_DEF bool utils_str_builder_append_cstr(utils_str_builder* sb, const char* cstr);
UTILS_DEF bool utils_str_builder_append_sb(utils_str_builder* sb, const utils_str_builder* other);
UTILS_DEF bool utils_str_builder_append_sb_value(utils_str_builder* sb, utils_str_builder other);
UTILS_DEF bool utils_str_builder_append_sv(utils_str_builder* sb, utils_str_view sv);
UTILS_DEF utils_str_view utils_str_builder_view(const utils_str_builder* sb);

#define utils_str_builder_append(sb, data)                 \
  _Generic((data),                                         \
    char*: utils_str_builder_append_cstr,                  \
    const char*: utils_str_builder_append_cstr,            \
    utils_str_builder: utils_str_builder_append_sb_value,  \
    utils_str_builder*: utils_str_builder_append_sb,       \
    const utils_str_builder*: utils_str_builder_append_sb, \
    utils_str_view: utils_str_builder_append_sv            \
  )(sb, data)

UTILS_DEF void utils_str_builder_clear(utils_str_builder* sb);
UTILS_DEF void utils_str_builder_free(utils_str_builder* sb);

typedef struct {
  void* data;
  usz count;
  usz capacity;
  utils_allocator alloc;
} utils_impl_da_base;

#define utils_da(T) struct { \
  T* data;                   \
  usz count;                 \
  usz capacity;              \
  utils_allocator alloc;     \
}

#define utils_da_new(a) { .alloc = (a) }

UTILS_DEF void* utils_impl_da_base_resize(utils_impl_da_base* arr, usz elem_size, usz new_capacity);
UTILS_DEF bool utils_impl_da_base_append_impl(utils_impl_da_base* arr, void* value, usz elem_size);
UTILS_DEF void utils_impl_da_free(utils_impl_da_base* arr);

#define utils_da_append(arr, value)            \
  ({                                           \
    typeof(*(arr)->data) utils__tmp = (value); \
    utils_impl_da_base_append_impl(            \
      (utils_impl_da_base*)(arr),              \
      &utils__tmp,                             \
      sizeof(utils__tmp)                       \
    );                                         \
  })

#define utils_da_at(arr, index) ((arr)->data[(index)])
#define utils_da_last(arr) ((arr)->data[(arr)->count - 1])
#define utils_da_free(arr) utils_impl_da_free((utils_impl_da_base*)(arr))

#define UTILS__DA_FOREACH_1(arr) UTILS__DA_FOREACH_2(arr, it)

#define UTILS__DA_FOREACH_2(arr, it)                          \
  for (usz utils__i = 0; utils__i < (arr)->count; ++utils__i) \
    for (typeof(*(arr)->data) it = (arr)->data[utils__i],     \
         *utils__once = &it;                                  \
         utils__once != nil;                                  \
         utils__once = nil)

#define UTILS__DA_FOREACH_GET(_1, _2, NAME, ...) NAME

#define utils_da_foreach(...) \
  UTILS__DA_FOREACH_GET(__VA_ARGS__, UTILS__DA_FOREACH_2, UTILS__DA_FOREACH_1)(__VA_ARGS__)

#define UTILS__DA_FOREACH_I_1(arr) UTILS__DA_FOREACH_I_3(arr, idx, it)

#define UTILS__DA_FOREACH_I_3(arr, i, it)          \
  for (usz i = 0; i < (arr)->count; ++i)           \
    for (typeof(*(arr)->data) it = (arr)->data[i], \
         *utils__once = &it;                       \
         utils__once != nil;                       \
         utils__once = nil)                        \

#define UTILS__DA_FOREACH_I_GET(_1, _2, _3, NAME, ...) NAME

#define utils_da_foreach_i(...) \
  UTILS__DA_FOREACH_I_GET(__VA_ARGS__, UTILS__DA_FOREACH_I_3, _, UTILS__DA_FOREACH_I_1)(__VA_ARGS__)

typedef struct {
  void* data;
  usz count;
  usz capacity;
  utils_allocator alloc;
} utils_impl_map_base;

#define utils_impl_map_entry(T) struct { \
  const char* key;                       \
  T value;                               \
  bool occupied;                         \
}

#define utils_map(T) struct {    \
  utils_impl_map_entry(T)* data; \
  usz count;                     \
  usz capacity;                  \
  utils_allocator alloc;         \
}

#define utils_map_new(a) { .alloc = (a) }

UTILS_DEF bool utils_impl_map_insert_impl(
  utils_impl_map_base* utils_map,
  const char* key,
  void* value,
  usz elem_size
);
UTILS_DEF void* utils_impl_map_get_impl(utils_impl_map_base* utils_map, const char* key, usz elem_size);
UTILS_DEF void utils_impl_map_free(utils_impl_map_base* utils_map);

#define utils_map_insert(utils_map, k, v) ({           \
  typeof((utils_map)->data[0].value) utils__tmp = (v); \
  utils_impl_map_insert_impl(                          \
    (utils_impl_map_base*)(utils_map),                 \
    (k),                                               \
    &utils__tmp,                                       \
    sizeof((utils_map)->data[0])                       \
  );                                                   \
})

#define utils_map_get(utils_map, k)      \
  ((typeof(&(utils_map)->data[0].value)) \
    utils_impl_map_get_impl(             \
      (utils_impl_map_base*)(utils_map), \
      (k),                               \
      sizeof((utils_map)->data[0])       \
    ))

#define utils_map_free(utils_map) \
  utils_impl_map_free((utils_impl_map_base*)(utils_map))

UTILS_DEF bool utils_read_entire_file(const char* path, utils_str_builder* sb);
UTILS_DEF bool utils_write_entire_file_cstr(const char* path, const char* data);
UTILS_DEF bool utils_write_entire_file_sv(const char* path, utils_str_view sv);
UTILS_DEF bool utils_write_entire_file_sv_ptr(const char* path, utils_str_view* sv);
UTILS_DEF bool utils_write_entire_file_sb(const char* path, utils_str_builder sb);
UTILS_DEF bool utils_write_entire_file_sb_ptr(const char* path, utils_str_builder* sb);

#define utils_write_entire_file(path, data)                  \
  _Generic((data),                                           \
    char*: utils_write_entire_file_cstr,                     \
    const char*: utils_write_entire_file_cstr,               \
    utils_str_view: utils_write_entire_file_sv,              \
    utils_str_view*: utils_write_entire_file_sv_ptr,         \
    const utils_str_view*: utils_write_entire_file_sv_ptr,   \
    utils_str_builder: utils_write_entire_file_sb,           \
    utils_str_builder*: utils_write_entire_file_sb_ptr,      \
    const utils_str_builder*: utils_write_entire_file_sb_ptr \
  )(path, data)

typedef enum utils_flag_type {
  UTILS_FLAG_BOOL,
  UTILS_FLAG_STRING,
  UTILS_FLAG_NUMBER,
} utils_flag_type;

typedef struct {
  const char* name;
  const char* desc;
  utils_flag_type type;
  union {
    bool bool_value;
    utils_str_view string_value;
    int number_value;
  } value;
  bool is_set;
} utils_flag;

#define utils_get_flag(pvalue) ((utils_flag*)((char*)(pvalue) - offsetof(utils_flag, value)))
#define utils_flag_is_set(pvalue) (utils_get_flag(pvalue)->is_set)
#define utils_flag_name(pvalue) (utils_get_flag(pvalue)->name)
#define utils_flag_desc(pvalue) (utils_get_flag(pvalue)->desc)

#ifndef UTILS_FLAGS_MAX_FLAGS
#define UTILS_FLAGS_MAX_FLAGS 64
#endif

static_assert(UTILS_FLAGS_MAX_FLAGS > 0, "UTILS_FLAGS_MAX_FLAGS must be greater than 0");

typedef struct utils_flags {
  const char* positional_args_req;

  utils_flag flags[UTILS_FLAGS_MAX_FLAGS];
  usz flags_count;

  utils_da(utils_str_view) positional_args;

  bool got_help;

  bool _parsed;
  bool failed_adding;

  utils_allocator alloc;
} utils_flags;

#define utils_flags_new(a) { .alloc = (a) }

#define utils_add_flag(a, name, description, def) \
  _Generic((def),                                 \
    char*: utils_impl_add_flag_string,            \
    const char*: utils_impl_add_flag_string,      \
    utils_str_view: utils_impl_add_flag_str_view, \
    int: utils_impl_add_flag_int,                 \
    bool: utils_impl_add_flag_bool                \
  )(a, name, description, def)

UTILS_DEF utils_str_view* utils_impl_add_flag_string(utils_flags* f, const char* name, const char* description, const char* def);
UTILS_DEF utils_str_view* utils_impl_add_flag_str_view(utils_flags* f, const char* name, const char* description, utils_str_view def);
UTILS_DEF int* utils_impl_add_flag_int(utils_flags* f, const char* name, const char* description, int def);
UTILS_DEF bool* utils_impl_add_flag_bool(utils_flags* f, const char* name, const char* description, bool def);

UTILS_DEF void utils_flags_reset(utils_flags* f);
UTILS_DEF void utils_flags_print_help(utils_flags* f, const char* prog_name);
UTILS_DEF bool utils_flags_parse(utils_flags* f, int flagc, char** flagv);

#ifdef UTILS_STRIP_PREFIX
typedef utils_allocator allocator;
typedef utils_tracking_allocator tracking_allocator;
typedef utils_arena_block arena_block;
typedef utils_arena arena;
typedef utils_arena_save_point arena_save_point;
typedef utils_scratch scratch;
typedef utils_str_view str_view;
typedef utils_str_builder str_builder;
typedef utils_flag_type flag_type;
typedef utils_flag flag;
typedef utils_flags flags;

#define TODO UTILS_TODO
#define a_alloc utils_a_alloc
#define a_new utils_a_new
#define a_realloc utils_a_realloc
#define a_free utils_a_free
#define a_reset utils_a_reset
#define ARENA_MIN_BLOCK_SIZE UTILS_ARENA_MIN_BLOCK_SIZE
#define make_tracking_allocator utils_make_tracking_allocator
#define make_arena_allocator utils_make_arena_allocator
#define make_allocator utils_make_allocator
#define defer utils_defer
#define str_view_eq utils_str_view_eq
#define str_view_ends_with utils_str_view_ends_with
#define str_view_starts_with utils_str_view_starts_with
#define str_view_remove_prefix utils_str_view_remove_prefix
#define str_view_remove_suffix utils_str_view_remove_suffix
#define str_view_make utils_str_view_make
#define str_view_consume_while utils_str_view_consume_while
#define str_view_consume_by_delim utils_str_view_consume_by_delim
#define str_view_consume_left utils_str_view_consume_left
#define str_view_consume_right utils_str_view_consume_right
#define str_view_from_cstr utils_str_view_from_cstr
#define str_view_eq_sv utils_str_view_eq_sv
#define str_view_eq_cstr utils_str_view_eq_cstr
#define str_view_ends_with_sv utils_str_view_ends_with_sv
#define str_view_ends_with_cstr utils_str_view_ends_with_cstr
#define str_view_starts_with_sv utils_str_view_starts_with_sv
#define str_view_starts_with_cstr utils_str_view_starts_with_cstr
#define str_view_remove_prefix_sv utils_str_view_remove_prefix_sv
#define str_view_remove_prefix_cstr utils_str_view_remove_prefix_cstr
#define str_view_remove_suffix_sv utils_str_view_remove_suffix_sv
#define str_view_remove_suffix_cstr utils_str_view_remove_suffix_cstr
#define str_view_trim_left utils_str_view_trim_left
#define str_view_trim_right utils_str_view_trim_right
#define str_view_trim utils_str_view_trim
#define str_view_find utils_str_view_find
#define str_view_at utils_str_view_at
#define str_view_substr utils_str_view_substr
#define str_view_to_cstr utils_str_view_to_cstr
#define str_builder_reserve utils_str_builder_reserve
#define str_builder_append_bytes utils_str_builder_append_bytes
#define str_builder_append_cstr utils_str_builder_append_cstr
#define str_builder_append_sb utils_str_builder_append_sb
#define str_builder_append_sb_value utils_str_builder_append_sb_value
#define str_builder_append_sv utils_str_builder_append_sv
#define str_builder_view utils_str_builder_view
#define str_builder_append utils_str_builder_append
#define str_builder_clear utils_str_builder_clear
#define str_builder_free utils_str_builder_free
#define sfmt utils_sfmt
#define sfmtarg utils_sfmtarg
#define da utils_da
#define da_append utils_da_append
#define da_at utils_da_at
#define da_last utils_da_last
#define da_free utils_da_free
#define da_foreach utils_da_foreach
#define da_foreach_i utils_da_foreach_i
#define map utils_map
#define map_insert utils_map_insert
#define map_get utils_map_get
#define map_free utils_map_free
#define read_entire_file utils_read_entire_file
#define write_entire_file_cstr utils_write_entire_file_cstr
#define write_entire_file_sv utils_write_entire_file_sv
#define write_entire_file_sv_ptr utils_write_entire_file_sv_ptr
#define write_entire_file_sb utils_write_entire_file_sb
#define write_entire_file_sb_ptr utils_write_entire_file_sb_ptr
#define write_entire_file utils_write_entire_file
#define get_flag utils_get_flag
#define flag_is_set utils_flag_is_set
#define flag_name utils_flag_name
#define flag_desc utils_flag_desc
#define FLAGS_MAX_FLAGS UTILS_FLAGS_MAX_FLAGS
#define FLAG_BOOL UTILS_FLAG_BOOL
#define FLAG_STRING UTILS_FLAG_STRING
#define FLAG_NUMBER UTILS_FLAG_NUMBER
#define add_flag utils_add_flag
#define flags_reset utils_flags_reset
#define flags_print_help utils_flags_print_help
#define flags_parse utils_flags_parse

#define str_builder_new utils_str_builder_new
#define da_new utils_da_new
#define map_new utils_map_new
#define flags_new utils_flags_new
#endif

#ifdef UTILS_IMPLEMENTATION

#undef UTILS_DEF
#define UTILS_DEF

#include <ctype.h>
#include <limits.h>
#include <stdio.h>

struct utils_arena_block {
  utils_arena_block* next;
  utils_arena_block* prev;
  usz capacity;
  usz used;
  bool dedicated;
};

static void* utils__libc_alloc(void* ctx, usz size) {
  (void)ctx;
  return malloc(size);
}

static void* utils__libc_realloc(void* ctx, void* ptr, usz new_size) {
  (void)ctx;
  return realloc(ptr, new_size);
}

static void utils__libc_free(void* ctx, void* ptr) {
  (void)ctx;
  free(ptr);
}

static void utils__libc_reset(void* ctx) {
  (void)ctx;
  assert(0 && "libc_allocator does not support reset");
}

static bool utils__tracking_allocator_resize(utils_tracking_allocator* tracker) {
  usz new_capacity = tracker->capacity * 2;
  void** new_allocations = realloc(tracker->allocations, sizeof(void*) * new_capacity);

  if (new_allocations == nil) {
    return false;
  }

  tracker->allocations = new_allocations;
  tracker->capacity = new_capacity;
  return true;
}

UTILS_DEF bool utils_tracking_allocator_track_ptr(utils_tracking_allocator* tracker, void* ptr) {
  if (ptr == nil) {
    return false;
  }

  if (tracker->allocations == nil) {
    tracker->capacity = 16;
    tracker->allocations = malloc(sizeof(void*) * tracker->capacity);

    if (tracker->allocations == nil) {
      tracker->capacity = 0;
      return false;
    }
  }

  if (tracker->count >= tracker->capacity) {
    if (!utils__tracking_allocator_resize(tracker)) {
      return false;
    }
  }

  tracker->allocations[tracker->count++] = ptr;
  return true;
}

UTILS_DEF void utils_tracking_allocator_untrack_ptr(utils_tracking_allocator* tracker, void* ptr) {
  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      tracker->allocations[i] = tracker->allocations[tracker->count - 1];
      tracker->count -= 1;
      return;
    }
  }
}

static void* utils__tracked_alloc(void* ctx, usz size) {
  utils_tracking_allocator* tracker = ctx;
  void* ptr = malloc(size);

  if (ptr == nil) {
    return nil;
  }

  if (!utils_tracking_allocator_track_ptr(tracker, ptr)) {
    free(ptr);
    return nil;
  }

  return ptr;
}

static void* utils__tracked_realloc(void* ctx, void* ptr, usz new_size) {
  utils_tracking_allocator* tracker = ctx;

  if (ptr == nil) {
    void* new_ptr = malloc(new_size);

    if (new_ptr == nil) {
      return nil;
    }

    if (!utils_tracking_allocator_track_ptr(tracker, new_ptr)) {
      free(new_ptr);
      return nil;
    }

    return new_ptr;
  }

  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      void* new_ptr = realloc(ptr, new_size);

      if (new_ptr == nil) {
        return nil;
      }

      tracker->allocations[i] = new_ptr;
      return new_ptr;
    }
  }

  return nil;
}

static void utils__tracked_free(void* ctx, void* ptr) {
  utils_tracking_allocator* tracker = ctx;
  utils_tracking_allocator_untrack_ptr(tracker, ptr);
  free(ptr);
}

static void utils__tracking_allocator_reset(void* ctx) {
  utils_tracking_allocator* tracker = ctx;

  for (usz i = 0; i < tracker->count; i++) {
    free(tracker->allocations[i]);
  }

  free(tracker->allocations);
  tracker->allocations = nil;
  tracker->count = 0;
  tracker->capacity = 0;
}

static usz utils__arena_align_up(usz size) {
  usz align = sizeof(void*) - 1;
  return (size + align) & ~align;
}

static utils_arena_block* utils__arena_new_block(usz capacity, bool dedicated) {
  utils_arena_block* block = malloc(sizeof(utils_arena_block) + capacity);

  if (block == nil) {
    return nil;
  }

  block->next = nil;
  block->prev = nil;
  block->capacity = capacity;
  block->used = 0;
  block->dedicated = dedicated;
  return block;
}

static void utils__arena_link_after(utils_arena_block* prev, utils_arena_block* block) {
  block->prev = prev;
  block->next = prev->next;

  if (prev->next != nil) {
    prev->next->prev = block;
  }

  prev->next = block;
}

static void utils__arena_release_block(utils_arena* a, utils_arena_block* block) {
  if (block->prev != nil) {
    block->prev->next = block->next;
  } else {
    a->first = block->next;
  }

  if (block->next != nil) {
    block->next->prev = block->prev;
  }

  if (a->current == block) {
    a->current = block->prev;
  }

  free(block);
}

static void* utils__arena_alloc(void* ctx, usz size) {
  utils_arena* a = ctx;
  size = utils__arena_align_up(size);

  usz total_size = utils__arena_align_up(sizeof(utils_arena_save_point)) + size;

  if (a->block_size < UTILS_ARENA_MIN_BLOCK_SIZE) {
    a->block_size = UTILS_ARENA_MIN_BLOCK_SIZE;
  }

  if (a->current == nil) {
    utils_arena_block* block = utils__arena_new_block(a->block_size, false);

    if (block == nil) {
      return nil;
    }

    a->first = block;
    a->current = block;
  }

  if (total_size > a->current->capacity / 2) {
    utils_arena_block* block = utils__arena_new_block(total_size, true);

    if (block == nil) {
      return nil;
    }

    utils__arena_link_after(a->current, block);

    utils_arena_save_point* header = (utils_arena_save_point*)(block + 1);
    header->block = block;
    header->used = size;
    block->used = total_size;
    return header + 1;
  }

  if (a->current->used + total_size > a->current->capacity) {
    usz capacity = a->current->capacity * 2;

    if (capacity < total_size) {
      capacity = total_size;
    }

    utils_arena_block* block = utils__arena_new_block(capacity, false);

    if (block == nil) {
      return nil;
    }

    utils__arena_link_after(a->current, block);
    a->current = block;
  }

  utils_arena_save_point* header = (utils_arena_save_point*)((u8*)(a->current + 1) + a->current->used);
  header->block = a->current;
  header->used = size;
  a->current->used += total_size;
  return header + 1;
}

static usz utils__usz_min(usz a, usz b) {
  return a < b ? a : b;
}

static void utils__arena_free(void* ctx, void* ptr) {
  utils_arena* a = ctx;

  if (ptr == nil) {
    return;
  }

  utils_arena_save_point* header = (utils_arena_save_point*)ptr - 1;
  utils_arena_block* block = header->block;
  usz total_size = utils__arena_align_up(sizeof(utils_arena_save_point)) + header->used;

  u8* end = (u8*)header + total_size;
  u8* block_end = (u8*)(block + 1) + block->used;

  if (end != block_end) {
    return;
  }

  block->used -= total_size;

  if (block->used != 0) {
    return;
  }

  if (block->dedicated) {
    utils__arena_release_block(a, block);
  } else if (block == a->current && block->prev != nil) {
    utils__arena_release_block(a, block);
  }
}

static void* utils__arena_realloc(void* ctx, void* ptr, usz new_size) {
  if (ptr == nil) {
    return utils__arena_alloc(ctx, new_size);
  }

  new_size = utils__arena_align_up(new_size);

  utils_arena_save_point* header = (utils_arena_save_point*)ptr - 1;
  utils_arena_block* block = header->block;

  usz old_size = header->used;
  usz old_total = utils__arena_align_up(sizeof(utils_arena_save_point)) + old_size;
  usz new_total = utils__arena_align_up(sizeof(utils_arena_save_point)) + new_size;

  u8* end = (u8*)header + old_total;
  u8* block_end = (u8*)(block + 1) + block->used;

  if (end == block_end) {
    usz used_without_this = block->used - old_total;

    if (used_without_this + new_total <= block->capacity) {
      block->used = used_without_this + new_total;
      header->used = new_size;
      return ptr;
    }
  }

  void* new_ptr = utils__arena_alloc(ctx, new_size);

  if (new_ptr == nil) {
    return nil;
  }

  memcpy(new_ptr, ptr, utils__usz_min(old_size, new_size));
  utils__arena_free(ctx, ptr);
  return new_ptr;
}

static void utils__arena_reset(void* ctx) {
  utils_arena* a = ctx;
  utils_arena_block* block = a->first;

  while (block != nil) {
    utils_arena_block* next = block->next;
    free(block);
    block = next;
  }

  a->first = nil;
  a->current = nil;
}

UTILS_DEF utils_allocator utils_make_simple_allocator(void) {
  return (utils_allocator){
    .ctx = nil,
    .alloc = utils__libc_alloc,
    .realloc = utils__libc_realloc,
    .free = utils__libc_free,
    .reset = utils__libc_reset,
  };
}

UTILS_DEF utils_allocator utils_make_tracking_allocator_from(utils_tracking_allocator* tracker) {
  return (utils_allocator){
    .ctx = tracker,
    .alloc = utils__tracked_alloc,
    .realloc = utils__tracked_realloc,
    .free = utils__tracked_free,
    .reset = utils__tracking_allocator_reset,
  };
}

UTILS_DEF utils_allocator utils_make_allocator_from_arena(utils_arena* arena_storage) {
  if (arena_storage->block_size < UTILS_ARENA_MIN_BLOCK_SIZE) {
    arena_storage->block_size = UTILS_ARENA_MIN_BLOCK_SIZE;
  }

  return (utils_allocator){
    .ctx = arena_storage,
    .alloc = utils__arena_alloc,
    .realloc = utils__arena_realloc,
    .free = utils__arena_free,
    .reset = utils__arena_reset,
  };
}

UTILS_DEF utils_arena_save_point utils_arena_save(utils_arena* a) {
  return (utils_arena_save_point){
    .block = a->current,
    .used = a->current ? a->current->used : 0,
  };
}

UTILS_DEF void utils_arena_restore(utils_arena* a, utils_arena_save_point save) {
  utils_arena_block* block = a->current;

  while (block && block != save.block) {
    utils_arena_block* prev = block->prev;
    utils__arena_release_block(a, block);
    block = prev;
  }

  a->current = save.block;

  if (a->current) {
    a->current->used = save.used;
  }
}

static thread_local utils_arena utils__scratch_arenas[2];

UTILS_DEF utils_scratch utils_scratch_begin_with(const utils_scratch* conflict) {
  utils_arena* a = nil;

  for (usz i = 0; i < 2; ++i) {
    if (conflict == nil || &utils__scratch_arenas[i] != conflict->backing) {
      a = &utils__scratch_arenas[i];
      break;
    }
  }

  assert(a != nil);

  return (utils_scratch){
    .backing = a,
    .save = utils_arena_save(a),
    .alloc = utils_make_allocator(a),
  };
}

UTILS_DEF utils_scratch utils_scratch_begin(void) {
  return utils_scratch_begin_with(nil);
}

UTILS_DEF void utils_scratch_end(utils_scratch s) {
  utils_arena_restore(s.backing, s.save);
}

UTILS_DEF utils_str_view utils_str_view_make(const char* data, usz count) {
  utils_str_view sv;
  sv.count = count;
  sv.data = data;
  return sv;
}

UTILS_DEF utils_str_view utils_str_view_consume_while(utils_str_view* sv, int (*p)(int x)) {
  usz i = 0;

  while (i < sv->count && p(sv->data[i])) {
    i += 1;
  }

  utils_str_view result = utils_str_view_make(sv->data, i);
  sv->count -= i;
  sv->data += i;
  return result;
}

UTILS_DEF utils_str_view utils_str_view_consume_by_delim(utils_str_view* sv, char delim) {
  usz i = 0;

  while (i < sv->count && sv->data[i] != delim) {
    i += 1;
  }

  utils_str_view result = utils_str_view_make(sv->data, i);

  if (i < sv->count) {
    sv->count -= i + 1;
    sv->data += i + 1;
  } else {
    sv->count -= i;
    sv->data += i;
  }

  return result;
}

UTILS_DEF utils_str_view utils_str_view_consume_left(utils_str_view* sv, usz n) {
  if (n > sv->count) {
    n = sv->count;
  }

  utils_str_view result = utils_str_view_make(sv->data, n);
  sv->data += n;
  sv->count -= n;
  return result;
}

UTILS_DEF utils_str_view utils_str_view_consume_right(utils_str_view* sv, usz n) {
  if (n > sv->count) {
    n = sv->count;
  }

  utils_str_view result = utils_str_view_make(sv->data + sv->count - n, n);
  sv->count -= n;
  return result;
}

UTILS_DEF utils_str_view utils_str_view_trim_left(utils_str_view sv) {
  usz i = 0;

  while (i < sv.count && isspace((unsigned char)sv.data[i])) {
    i += 1;
  }

  return utils_str_view_make(sv.data + i, sv.count - i);
}

UTILS_DEF utils_str_view utils_str_view_trim_right(utils_str_view sv) {
  usz i = 0;

  while (i < sv.count && isspace((unsigned char)sv.data[sv.count - 1 - i])) {
    i += 1;
  }

  return utils_str_view_make(sv.data, sv.count - i);
}

UTILS_DEF utils_str_view utils_str_view_trim(utils_str_view sv) {
  return utils_str_view_trim_right(utils_str_view_trim_left(sv));
}

UTILS_DEF utils_str_view utils_str_view_from_cstr(const char* cstr) {
  return utils_str_view_make(cstr, strlen(cstr));
}

UTILS_DEF bool utils_str_view_eq_sv(utils_str_view a, utils_str_view b) {
  if (a.count != b.count) {
    return false;
  }

  return memcmp(a.data, b.data, a.count) == 0;
}

UTILS_DEF bool utils_str_view_eq_cstr(utils_str_view sv, const char* cstr) {
  return utils_str_view_eq_sv(sv, utils_str_view_from_cstr(cstr));
}

UTILS_DEF bool utils_str_view_ends_with_sv(utils_str_view sv, utils_str_view suffix) {
  if (sv.count >= suffix.count) {
    utils_str_view sv_tail = {
      .count = suffix.count,
      .data = sv.data + sv.count - suffix.count,
    };
    return utils_str_view_eq_sv(sv_tail, suffix);
  }

  return false;
}

UTILS_DEF bool utils_str_view_ends_with_cstr(utils_str_view sv, const char* cstr) {
  return utils_str_view_ends_with_sv(sv, utils_str_view_from_cstr(cstr));
}

UTILS_DEF bool utils_str_view_starts_with_sv(utils_str_view sv, utils_str_view expected_prefix) {
  if (expected_prefix.count <= sv.count) {
    utils_str_view actual_prefix = utils_str_view_make(sv.data, expected_prefix.count);
    return utils_str_view_eq_sv(expected_prefix, actual_prefix);
  }

  return false;
}

UTILS_DEF bool utils_str_view_starts_with_cstr(utils_str_view sv, const char* cstr) {
  return utils_str_view_starts_with_sv(sv, utils_str_view_from_cstr(cstr));
}

UTILS_DEF bool utils_str_view_remove_prefix_sv(utils_str_view* sv, utils_str_view prefix) {
  if (utils_str_view_starts_with_sv(*sv, prefix)) {
    utils_str_view_consume_left(sv, prefix.count);
    return true;
  }

  return false;
}

UTILS_DEF bool utils_str_view_remove_prefix_cstr(utils_str_view* sv, const char* prefix) {
  return utils_str_view_remove_prefix_sv(sv, utils_str_view_from_cstr(prefix));
}

UTILS_DEF bool utils_str_view_remove_suffix_sv(utils_str_view* sv, utils_str_view suffix) {
  if (utils_str_view_ends_with(*sv, suffix)) {
    utils_str_view_consume_right(sv, suffix.count);
    return true;
  }

  return false;
}

UTILS_DEF bool utils_str_view_remove_suffix_cstr(utils_str_view* sv, const char* suffix) {
  return utils_str_view_remove_suffix_sv(sv, utils_str_view_from_cstr(suffix));
}

UTILS_DEF utils_str_view utils_str_view_substr(utils_str_view sv, usz start, usz length) {
  if (start >= sv.count) {
    return utils_str_view_make("", 0);
  }

  if (start + length > sv.count) {
    length = sv.count - start;
  }

  return utils_str_view_make(sv.data + start, length);
}

UTILS_DEF char utils_str_view_at(utils_str_view sv, isz index) {
  if (index < 0) {
    index = (isz)sv.count + index;
  }
  if (index < 0 || (usz)index >= sv.count) {
    return '\0';
  }
  return sv.data[index];
}

UTILS_DEF int utils_str_view_find(utils_str_view sv, char c) {
  for (usz i = 0; i < sv.count; ++i) {
    if (sv.data[i] == c) {
      return (int)i;
    }
  }

  return -1;
}

UTILS_DEF char* utils_str_view_to_cstr(utils_str_view sv, utils_allocator alloc) {
  char* cstr = alloc.alloc(alloc.ctx, sv.count + 1);

  if (cstr == nil) {
    return nil;
  }

  memcpy(cstr, sv.data, sv.count);
  cstr[sv.count] = '\0';
  return cstr;
}

static void utils__str_builder_ensure_allocator(utils_str_builder* sb) {
  assert(sb != nil);
  assert(sb->alloc.alloc != nil && sb->alloc.realloc != nil && sb->alloc.free != nil);
}

#define UTILS__STR_BUILDER_REALLOC(sb, new_capacity) \
  (sb)->alloc.realloc((sb)->alloc.ctx, (sb)->data, (new_capacity))

#define UTILS__STR_BUILDER_FREE(sb, ptr) \
  do { \
    if ((ptr) != nil) { \
      (sb)->alloc.free((sb)->alloc.ctx, (ptr)); \
    } \
  } while (0)

UTILS_DEF bool utils_str_builder_reserve(utils_str_builder* sb, usz additional) {
  usz required = sb->count + additional + 1;

  if (required <= sb->capacity) {
    return true;
  }

  usz new_capacity = sb->capacity > 0 ? sb->capacity : 64;

  while (new_capacity < required) {
    usz next = new_capacity * 2;

    if (next <= new_capacity) {
      return false;
    }

    new_capacity = next;
  }

  utils__str_builder_ensure_allocator(sb);

  char* new_data = UTILS__STR_BUILDER_REALLOC(sb, new_capacity);

  if (new_data == nil) {
    return false;
  }

  sb->data = new_data;
  sb->capacity = new_capacity;
  return true;
}

UTILS_DEF bool utils_str_builder_append_bytes(utils_str_builder* sb, const void* data, usz size) {
  if (!utils_str_builder_reserve(sb, size)) {
    return false;
  }

  memcpy(sb->data + sb->count, data, size);
  sb->count += size;
  sb->data[sb->count] = '\0';
  return true;
}

UTILS_DEF bool utils_str_builder_append_cstr(utils_str_builder* sb, const char* cstr) {
  return utils_str_builder_append_bytes(sb, cstr, strlen(cstr));
}

UTILS_DEF bool utils_str_builder_append_sb(utils_str_builder* sb, const utils_str_builder* other) {
  return utils_str_builder_append_bytes(sb, other->data, other->count);
}

UTILS_DEF bool utils_str_builder_append_sb_value(utils_str_builder* sb, utils_str_builder other) {
  return utils_str_builder_append_sb(sb, &other);
}

UTILS_DEF bool utils_str_builder_append_sv(utils_str_builder* sb, utils_str_view sv) {
  return utils_str_builder_append_bytes(sb, sv.data, sv.count);
}

UTILS_DEF utils_str_view utils_str_builder_view(const utils_str_builder* sb) {
  return utils_str_view_make(sb->data ? sb->data : "", sb->count);
}

UTILS_DEF void utils_str_builder_clear(utils_str_builder* sb) {
  sb->count = 0;

  if (sb->data != nil) {
    sb->data[0] = '\0';
  }
}

UTILS_DEF void utils_str_builder_free(utils_str_builder* sb) {
  utils__str_builder_ensure_allocator(sb);
  UTILS__STR_BUILDER_FREE(sb, sb->data);
  sb->data = nil;
  sb->count = 0;
  sb->capacity = 0;
}

static void utils__da_base_ensure_allocator(utils_impl_da_base* arr) {
  assert(arr != nil);
  assert(arr->alloc.alloc != nil && arr->alloc.realloc != nil && arr->alloc.free != nil);
}

#define UTILS__DA_BASE_REALLOC(arr, elem_size, new_capacity) \
  (arr)->alloc.realloc((arr)->alloc.ctx, (arr)->data, (new_capacity) * (elem_size))

#define UTILS__DA_BASE_FREE(arr) \
  do { \
    if ((arr)->data != nil) { \
      (arr)->alloc.free((arr)->alloc.ctx, (arr)->data); \
    } \
  } while (0)

UTILS_DEF void* utils_impl_da_base_resize(utils_impl_da_base* arr, usz elem_size, usz new_capacity) {
  utils__da_base_ensure_allocator(arr);
  return UTILS__DA_BASE_REALLOC(arr, elem_size, new_capacity);
}

UTILS_DEF bool utils_impl_da_base_append_impl(utils_impl_da_base* arr, void* value, usz elem_size) {
  if (arr->count >= arr->capacity) {
    usz new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 4;
    void* new_data = utils_impl_da_base_resize(arr, elem_size, new_capacity);

    if (new_data == nil) {
      return false;
    }

    arr->data = new_data;
    arr->capacity = new_capacity;
  }

  memcpy((u8*)arr->data + arr->count * elem_size, value, elem_size);
  arr->count += 1;
  return true;
}

UTILS_DEF void utils_impl_da_free(utils_impl_da_base* arr) {
  utils__da_base_ensure_allocator(arr);
  UTILS__DA_BASE_FREE(arr);
  arr->data = nil;
  arr->count = 0;
  arr->capacity = 0;
}

static void utils__map_ensure_allocator(utils_impl_map_base* utils_map) {
  assert(utils_map != nil);
  assert(utils_map->alloc.alloc != nil && utils_map->alloc.realloc != nil && utils_map->alloc.free != nil);
}

#define UTILS__MAP_ALLOC(utils_map, size) \
  (utils_map)->alloc.alloc((utils_map)->alloc.ctx, (size))

#define UTILS__MAP_FREE_MEM(utils_map, ptr) \
  (utils_map)->alloc.free((utils_map)->alloc.ctx, (ptr))

static u64 utils__map_hash(const char* str) {
  u64 hash = 14695981039346656037ull;

  while (*str) {
    hash ^= (u8)*str;
    hash *= 1099511628211ull;
    str += 1;
  }

  return hash;
}

static bool utils__map_init_slots(utils_impl_map_base* utils_map, usz elem_size, usz capacity) {
  utils__map_ensure_allocator(utils_map);

  void* data = UTILS__MAP_ALLOC(utils_map, capacity * elem_size);

  if (data == nil) {
    return false;
  }

  memset(data, 0, capacity * elem_size);
  utils_map->data = data;
  utils_map->capacity = capacity;
  utils_map->count = 0;
  return true;
}

static bool utils__map_insert_no_rehash(
  utils_impl_map_base* utils_map,
  const char* key,
  void* value,
  usz elem_size
) {
  usz cap = utils_map->capacity;
  u8* base = utils_map->data;
  usz idx = utils__map_hash(key) % cap;
  usz probes = 0;

  while (probes < cap) {
    void* entry = base + idx * elem_size;
    bool* occupied = (bool*)((u8*)entry + elem_size - sizeof(bool));

    if (!*occupied) {
      memcpy(entry, &key, sizeof key);
      memcpy((u8*)entry + sizeof(char*), value, elem_size - sizeof(char*) - sizeof(bool));
      *occupied = true;
      utils_map->count += 1;
      return true;
    }

    idx += 1;

    if (idx >= cap) {
      idx = 0;
    }

    probes += 1;
  }

  return false;
}

static bool utils__map_rehash(utils_impl_map_base* utils_map, usz elem_size, usz new_capacity) {
  utils_impl_map_base new_map = {0};
  new_map.alloc = utils_map->alloc;

  if (!utils__map_init_slots(&new_map, elem_size, new_capacity)) {
    return false;
  }

  u8* base = utils_map->data;

  for (usz i = 0; i < utils_map->capacity; i += 1) {
    void* entry = base + i * elem_size;
    bool* occupied = (bool*)((u8*)entry + elem_size - sizeof(bool));

    if (*occupied) {
      const char* key = *(const char**)entry;
      void* value = (u8*)entry + sizeof(char*);

      if (!utils__map_insert_no_rehash(&new_map, key, value, elem_size)) {
        UTILS__MAP_FREE_MEM(&new_map, new_map.data);
        return false;
      }
    }
  }

  if (utils_map->data != nil) {
    UTILS__MAP_FREE_MEM(utils_map, utils_map->data);
  }

  utils_map->data = new_map.data;
  utils_map->count = new_map.count;
  utils_map->capacity = new_map.capacity;
  return true;
}

UTILS_DEF bool utils_impl_map_insert_impl(
  utils_impl_map_base* utils_map,
  const char* key,
  void* value,
  usz elem_size
) {
  if (utils_map->capacity == 0) {
    if (!utils__map_init_slots(utils_map, elem_size, 16)) {
      return false;
    }
  }

  if ((utils_map->count + 1) * 10 >= utils_map->capacity * 7) {
    if (!utils__map_rehash(utils_map, elem_size, utils_map->capacity * 2)) {
      return false;
    }
  }

  usz cap = utils_map->capacity;
  u8* base = utils_map->data;
  usz idx = utils__map_hash(key) % cap;
  usz probes = 0;

  while (probes < cap) {
    void* entry = base + idx * elem_size;
    bool* occupied = (bool*)((u8*)entry + elem_size - sizeof(bool));

    if (!*occupied) {
      memcpy(entry, &key, sizeof key);
      memcpy((u8*)entry + sizeof(char*), value, elem_size - sizeof(char*) - sizeof(bool));
      *occupied = true;
      utils_map->count += 1;
      return true;
    }

    const char* existing = *(const char**)entry;

    if (strcmp(existing, key) == 0) {
      memcpy((u8*)entry + sizeof(char*), value, elem_size - sizeof(char*) - sizeof(bool));
      return true;
    }

    idx += 1;

    if (idx >= cap) {
      idx = 0;
    }

    probes += 1;
  }

  return false;
}

UTILS_DEF void* utils_impl_map_get_impl(utils_impl_map_base* utils_map, const char* key, usz elem_size) {
  if (utils_map->capacity == 0) {
    return nil;
  }

  usz idx = utils__map_hash(key) % utils_map->capacity;
  usz probes = 0;
  u8* base = utils_map->data;

  while (probes < utils_map->capacity) {
    void* entry = base + idx * elem_size;
    bool* occupied = (bool*)((u8*)entry + elem_size - sizeof(bool));

    if (!*occupied) {
      return nil;
    }

    const char* existing = *(const char**)entry;

    if (strcmp(existing, key) == 0) {
      return (u8*)entry + sizeof(char*);
    }

    idx += 1;

    if (idx >= utils_map->capacity) {
      idx = 0;
    }

    probes += 1;
  }

  return nil;
}

UTILS_DEF void utils_impl_map_free(utils_impl_map_base* utils_map) {
  utils__map_ensure_allocator(utils_map);

  if (utils_map->data != nil) {
    UTILS__MAP_FREE_MEM(utils_map, utils_map->data);
  }

  utils_map->data = nil;
  utils_map->count = 0;
  utils_map->capacity = 0;
}

UTILS_DEF bool utils_read_entire_file(const char* path, utils_str_builder* sb) {
  FILE* f = fopen(path, "rb");

  if (f == nil) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }

  long size = ftell(f);

  if (size < 0) {
    fclose(f);
    return false;
  }

  rewind(f);
  utils_str_builder_clear(sb);

  if (!utils_str_builder_reserve(sb, (usz)size)) {
    fclose(f);
    return false;
  }

  usz read = fread(sb->data, 1, (usz)size, f);
  fclose(f);

  if (read != (usz)size) {
    return false;
  }

  sb->count = read;
  sb->data[sb->count] = '\0';
  return true;
}

UTILS_DEF bool utils_write_entire_file_cstr(const char* path, const char* data) {
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz size = strlen(data);
  usz written = fwrite(data, 1, size, f);
  fclose(f);
  return written == size;
}

UTILS_DEF bool utils_write_entire_file_sv(const char* path, utils_str_view sv) {
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz written = fwrite(sv.data, 1, sv.count, f);
  fclose(f);
  return written == sv.count;
}

UTILS_DEF bool utils_write_entire_file_sv_ptr(const char* path, utils_str_view* sv) {
  if (sv == nil) {
    return false;
  }

  return utils_write_entire_file_sv(path, *sv);
}

UTILS_DEF bool utils_write_entire_file_sb(const char* path, utils_str_builder sb) {
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz written = fwrite(sb.data, 1, sb.count, f);
  fclose(f);
  return written == sb.count;
}

UTILS_DEF bool utils_write_entire_file_sb_ptr(const char* path, utils_str_builder* sb) {
  if (sb == nil) {
    return false;
  }

  return utils_write_entire_file_sb(path, *sb);
}

static void* utils__add_flag(utils_flags* f, const char* name, const char* description, utils_flag_type type) {
  if (f->flags_count >= UTILS_FLAGS_MAX_FLAGS) {
    fprintf(
      stderr,
      "Maximum number of utils_flags exceeded (%d). "
      "#define UTILS_FLAGS_MAX_FLAGS before including utils.h to increase this limit.\n",
      UTILS_FLAGS_MAX_FLAGS
    );
    f->failed_adding = true;
    return nil;
  }

  if (f->_parsed) {
    fprintf(stderr, "cannot add utils_flags after parsing\n");
    f->failed_adding = true;
    return nil;
  }

  if (name == nil || description == nullptr) {
    fprintf(stderr, "utils_flag name and/or description cannot be null\n");
    f->failed_adding = true;
    return nil;
  }

  for (const char* p = name; *p != '\0'; p++) {
    if (*p == '=') {
      fprintf(stderr, "utils_flag name cannot contain '=': %s\n", name);
      f->failed_adding = true;
      return nil;
    }
  }

  if (strcmp(name, "h") == 0) {
    fprintf(stderr, "-h is reserved for help\n");
    f->failed_adding = true;
    return nil;
  }

  for (usz i = 0; i < f->flags_count; i++) {
    if (strcmp(f->flags[i].name, name) == 0) {
      fprintf(stderr, "duplicate utils_flag name: %s\n", name);
      f->failed_adding = true;
      return nil;
    }
  }

  utils_flag* flag_slot = &f->flags[f->flags_count];
  flag_slot->name = name;
  flag_slot->desc = description;
  flag_slot->type = type;
  flag_slot->is_set = false;
  f->flags_count += 1;
  return &f->flags[f->flags_count - 1].value;
}

UTILS_DEF utils_str_view* utils_impl_add_flag_string(utils_flags* f, const char* name, const char* description, const char* def) {
  if (def == nil) {
    def = "";
  }

  void* got = utils__add_flag(f, name, description, UTILS_FLAG_STRING);

  if (!got) {
    return nil;
  }

  f->flags[f->flags_count - 1].value.string_value = utils_str_view_from_cstr(def);
  return (utils_str_view*)got;
}

UTILS_DEF utils_str_view* utils_impl_add_flag_str_view(utils_flags* f, const char* name, const char* description, utils_str_view def) {
  void* got = utils__add_flag(f, name, description, UTILS_FLAG_STRING);

  if (!got) {
    return nil;
  }

  f->flags[f->flags_count - 1].value.string_value = def;
  return (utils_str_view*)got;
}

UTILS_DEF int* utils_impl_add_flag_int(utils_flags* f, const char* name, const char* description, int def) {
  void* got = utils__add_flag(f, name, description, UTILS_FLAG_NUMBER);

  if (!got) {
    return nil;
  }

  f->flags[f->flags_count - 1].value.number_value = def;
  return (int*)got;
}

UTILS_DEF bool* utils_impl_add_flag_bool(utils_flags* f, const char* name, const char* description, bool def) {
  void* got = utils__add_flag(f, name, description, UTILS_FLAG_BOOL);

  if (!got) {
    return nil;
  }

  f->flags[f->flags_count - 1].value.bool_value = def;
  return (bool*)got;
}

UTILS_DEF void utils_flags_reset(utils_flags* f) {
  f->flags_count = 0;
  utils_da_free(&f->positional_args);
  f->got_help = false;
  f->_parsed = false;
  f->failed_adding = false;
}

static bool utils__is_flag(utils_str_view flag_sv) {
  return flag_sv.count > 0 && flag_sv.data[0] == '-';
}

static bool utils__add_positional_arg(utils_flags* f, utils_str_view arg) {
  if (f->positional_args.alloc.alloc == nil) {
    assert(f->alloc.alloc != nil && f->alloc.realloc != nil && f->alloc.free != nil);
    f->positional_args.alloc = f->alloc;
  }

  return utils_da_append(&f->positional_args, arg);
}

static bool utils__parse_int(utils_str_view sv, int* out) {
  if (sv.count == 0) {
    return false;
  }

  bool negative = false;
  usz i = 0;

  if (sv.data[0] == '-') {
    negative = true;
    i = 1;

    if (i == sv.count) {
      return false;
    }
  }

  long value = 0;

  for (; i < sv.count; i++) {
    char c = sv.data[i];

    if (c < '0' || c > '9') {
      return false;
    }

    value = value * 10 + (c - '0');

    if ((!negative && value > INT_MAX) || (negative && -value < INT_MIN)) {
      return false;
    }
  }

  *out = negative ? -(int)value : (int)value;
  return true;
}

static bool utils__set_flag_value(utils_flag* flag_slot, utils_str_view sv) {
  if (flag_slot->is_set) {
    fprintf(stderr, "utils_flag -%s specified multiple times\n", flag_slot->name);
    return false;
  }

  switch (flag_slot->type) {
    case UTILS_FLAG_BOOL:
      if (sv.count > 0) {
        fprintf(stderr, "boolean utils_flag -%s does not take a value\n", flag_slot->name);
        return false;
      }

      flag_slot->value.bool_value = true;
      break;
    case UTILS_FLAG_STRING:
      flag_slot->value.string_value = sv;
      break;
    case UTILS_FLAG_NUMBER: {
      int value;

      if (!utils__parse_int(sv, &value)) {
        fprintf(stderr, "invalid integer value for utils_flag -%s: '" utils_sfmt "'\n", flag_slot->name, utils_sfmtarg(sv));
        return false;
      }

      flag_slot->value.number_value = value;
      break;
    }
  }

  flag_slot->is_set = true;
  return true;
}

UTILS_DEF void utils_flags_print_help(utils_flags* f, const char* prog_name) {
  printf("Usage: %s [options]", prog_name);

  if (f->positional_args_req) {
    if (strcmp(f->positional_args_req, "+") == 0) {
      printf(" <arg1> [arg2] ...");
    } else if (strcmp(f->positional_args_req, "?") == 0) {
      printf(" [arg]");
    } else if (strcmp(f->positional_args_req, "*") == 0) {
      printf(" [arg1] [arg2] ...");
    } else {
      printf(" ");
      int expected = atoi(f->positional_args_req);

      for (long j = 0; j < expected; j++) {
        printf("<arg%ld> ", j + 1);
      }
    }
  }

  printf("\n");

  if (f->flags_count > 0) {
    printf("\nOptions:\n");

    usz max_name_len = 0;

    for (usz j = 0; j < f->flags_count; j++) {
      usz len = strlen(f->flags[j].name);

      if (len > max_name_len) {
        max_name_len = len;
      }
    }

    for (usz j = 0; j < f->flags_count; j++) {
      utils_flag* flag_slot = &f->flags[j];
      printf("  -%-*s  %s", (int)max_name_len, flag_slot->name, flag_slot->desc);

      switch (flag_slot->type) {
        case UTILS_FLAG_STRING:
          if (flag_slot->value.string_value.count > 0) {
            printf(" (default: " utils_sfmt ")", utils_sfmtarg(flag_slot->value.string_value));
          }
          break;
        case UTILS_FLAG_NUMBER:
          printf(" (default: %d)", flag_slot->value.number_value);
          break;
        default:
          break;
      }

      printf("\n");
    }
  }
}

UTILS_DEF bool utils_flags_parse(utils_flags* f, int flagc, char** flagv) {
  if (f->positional_args.alloc.alloc == nil) {
    assert(f->alloc.alloc != nil && f->alloc.realloc != nil && f->alloc.free != nil);
    f->positional_args.alloc = f->alloc;
  }

  if (f->failed_adding) {
    return false;
  }

  if (!f->positional_args_req) {
  } else if (strcmp(f->positional_args_req, "+") == 0) {
  } else if (strcmp(f->positional_args_req, "?") == 0) {
  } else if (strcmp(f->positional_args_req, "*") == 0) {
  } else {
    int expected = atoi(f->positional_args_req);

    if (expected < 0) {
      fprintf(stderr, "invalid positional_args_req: %s\n", f->positional_args_req);
      return false;
    }
  }

  for (int i = 0; i < flagc; i++) {
    if (strcmp(flagv[i], "-h") == 0) {
      f->got_help = true;
      return true;
    }
  }

  for (int i = 1; i < flagc; i++) {
    utils_str_view got = utils_str_view_from_cstr(flagv[i]);

    if (!utils__is_flag(got)) {
      if (!utils__add_positional_arg(f, got)) {
        return false;
      }

      continue;
    }

    utils_str_view_consume_left(&got, 1);

    bool found = false;

    for (usz j = 0; j < f->flags_count; j++) {
      utils_flag* flag_slot = &f->flags[j];

      if (utils_str_view_eq_cstr(got, flag_slot->name)) {
        found = true;
        utils_str_view value = {0};

        if (flag_slot->type != UTILS_FLAG_BOOL) {
          if (i + 1 >= flagc) {
            fprintf(stderr, "utils_flag -" utils_sfmt " requires a value\n", utils_sfmtarg(got));
            return false;
          }

          value = utils_str_view_from_cstr(flagv[i + 1]);
          i += 1;
        }

        if (!utils__set_flag_value(flag_slot, value)) {
          return false;
        }

        break;
      }

      usz candidate_len = strlen(flag_slot->name);

      if (!utils_str_view_starts_with_cstr(got, flag_slot->name)) {
        continue;
      }

      utils_str_view suffix = got;
      utils_str_view_consume_left(&suffix, candidate_len);

      if (suffix.count == 0 || suffix.data[0] != '=') {
        continue;
      }

      found = true;
      utils_str_view value = suffix;
      utils_str_view_consume_left(&value, 1);

      if (!utils__set_flag_value(flag_slot, value)) {
        return false;
      }
    }

    if (!found) {
      int equal_sign = utils_str_view_find(got, '=');

      if (equal_sign != -1) {
        utils_str_view utils_flag_name = got;
        utils_flag_name.count = (usz)equal_sign;
        fprintf(stderr, "unknown utils_flag -" utils_sfmt "\n", utils_sfmtarg(utils_flag_name));
        return false;
      }

      fprintf(stderr, "unknown utils_flag -" utils_sfmt "\n", utils_sfmtarg(got));
      return false;
    }
  }

  if (!f->positional_args_req) {
    if (f->positional_args.count > 0) {
      fprintf(stderr, "expected no positional arguments, got %zu\n", f->positional_args.count);
      return false;
    }
  } else if (strcmp(f->positional_args_req, "+") == 0) {
    if (f->positional_args.count == 0) {
      fprintf(stderr, "expected at least one positional argument\n");
      return false;
    }
  } else if (strcmp(f->positional_args_req, "?") == 0) {
    if (f->positional_args.count > 1) {
      fprintf(stderr, "expected at most one positional argument\n");
      return false;
    }
  } else if (strcmp(f->positional_args_req, "*") == 0) {
  } else {
    int expected = atoi(f->positional_args_req);

    if (f->positional_args.count != (usz)expected) {
      fprintf(stderr, "expected %d positional arguments, got %zu\n", expected, f->positional_args.count);
      return false;
    }
  }

  f->_parsed = true;
  return true;
}

#endif

#endif
