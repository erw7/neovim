#ifndef NVIM_TYPES_H
#define NVIM_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Shorthand for unsigned variables. Many systems, but not all, have u_char
// already defined, so we use char_u to avoid trouble.
typedef unsigned char char_u;

// Can hold one decoded UTF-8 character.
typedef uint32_t u8char_T;

// Opaque handle used by API clients to refer to various objects in vim
typedef int handle_T;

// Opaque handle to a lua value. Must be free with `api_free_luaref` when
// not needed anymore! LUA_NOREF represents missing reference, i e to indicate
// absent callback etc.
typedef int LuaRef;

typedef handle_T NS;

typedef struct expand expand_T;

typedef enum {
  kNone  = -1,
  kFalse = 0,
  kTrue  = 1,
} TriState;

typedef enum {
  kQNone  = -1,
  kQFalse = 0,
  kLevel1 = 1,
  kLevel2 = 2,
} QuadState;

#endif  // NVIM_TYPES_H
