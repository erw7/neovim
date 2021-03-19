#ifndef NVIM_OS_PTY_CONPTY_WIN_H
#define NVIM_OS_PTY_CONPTY_WIN_H

#ifndef HPCON
# define HPCON VOID *
#endif

extern HRESULT (WINAPI *pCreatePseudoConsole[])  // NOLINT(whitespace/parens)
  (COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON *phPC);
extern HRESULT (WINAPI *pResizePseudoConsole[])(HPCON phPC, COORD size);
extern void (WINAPI *pClosePseudoConsole[])(HPCON phPC);

typedef enum {
  kKernel = 1,
  kDll    = 2,
} ConPtyType;

typedef struct conpty {
  HPCON pty;
  STARTUPINFOEXW si_ex;
  ConPtyType type;
} conpty_t;

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "os/pty_conpty_win.h.generated.h"
#endif

#endif  // NVIM_OS_PTY_CONPTY_WIN_H
