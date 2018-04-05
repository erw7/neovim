#include <io.h>
#include <stdbool.h>

#include "nvim/os/cygterm.h"
#include "nvim/memory.h"

#define CYGWDLL "cygwin1.dll"
#define MSYSDLL "msys-2.0.dll"
#define CYG_INIT_FUNC "cygwin_dll_init"
#define MSYS_INIT_FUNC "msys_dll_init"

// Hack to detect mintty, ported from vim
// https://fossies.org/linux/vim/src/iscygpty.c
// See https://github.com/BurntSushi/ripgrep/issues/94#issuecomment-261745480
// for an explanation on why this works
MinttyType detect_mintty_type(int fd)
{
  const int size = sizeof(FILE_NAME_INFO) + sizeof(WCHAR) * MAX_PATH;
  WCHAR *p = NULL;

  const HANDLE h = (HANDLE)_get_osfhandle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    return FALSE;
  }
  // Cygwin/msys's pty is a pipe.
  if (GetFileType(h) != FILE_TYPE_PIPE) {
    return FALSE;
  }
  FILE_NAME_INFO *nameinfo = xmalloc(size);
  if (nameinfo == NULL) {
    return FALSE;
  }
  // Check the name of the pipe:
  // '\{cygwin,msys}-XXXXXXXXXXXXXXXX-ptyN-{from,to}-master'
  MinttyType result = NONE_MINTTY;
  if (GetFileInformationByHandleEx(h, FileNameInfo, nameinfo, size)) {
    nameinfo->FileName[nameinfo->FileNameLength / sizeof(WCHAR)] = L'\0';
    p = nameinfo->FileName;
    if (wcsstr(p, L"\\cygwin-") == p) {
      p += 8;
      result = MINTTY_CYGWIN;
    } else if (wcsstr(p, L"\\msys-") == p) {
      p += 6;
      result = MINTTY_MSYS;
    } else {
      p = NULL;
    }
    if (p != NULL) {
      while (*p && isxdigit(*p)) {  // Skip 16-digit hexadecimal.
        p++;
      }
      if (wcsstr(p, L"-pty") == p) {
        p += 4;
      } else {
        p = NULL;
      }
    }
    if (p != NULL) {
      while (*p && isdigit(*p)) {  // Skip pty number.
        p++;
      }
      if (wcsstr(p, L"-from-master") != p && wcsstr(p, L"-to-master") != p) {
        p = NULL;
      }
    }
  }
  xfree(nameinfo);
  return p != NULL ? result : NONE_MINTTY;
}

CygTerm *cygterm_new(int fd)
{
  MinttyType mintty = detect_mintty_type(fd);
  if (mintty == NONE_MINTTY) {
    return NULL;
  }

  CygTerm *cygterm = (CygTerm *)xmalloc(sizeof(CygTerm));
  if (!cygterm) {
    return NULL;
  }

  if (mintty == MINTTY_CYGWIN) {
    cygterm->hmodule = LoadLibrary(CYGWDLL);
    if (!cygterm->hmodule) {
      goto abort;
    }
    cygterm->init = (void (*)(void))GetProcAddress(cygterm->hmodule, CYG_INIT_FUNC);
  } else {
    cygterm->hmodule = LoadLibrary(MSYSDLL);
    if (!cygterm->hmodule) {
      goto abort;
    }
    cygterm->init = (void (*)(void))GetProcAddress(cygterm->hmodule, MSYS_INIT_FUNC);
  }
  cygterm->tcgetattr = (int (*)(int, struct termios *))GetProcAddress(cygterm->hmodule, "tcsetattr");
  cygterm->tcsetattr = (int (*)(int, int, const struct termios *))GetProcAddress(cygterm->hmodule, "tcsetattr");
  cygterm->ioctl = (int (*)(int, int, ...))GetProcAddress(cygterm->hmodule, "ioctl");

  if (!cygterm->init || !cygterm->tcgetattr || !cygterm->tcsetattr || !cygterm->ioctl) {
    goto abort;
  }
  cygterm->is_started = FALSE;
  cygterm->fd = fd;
  cygterm->init();
  cygterm_start(cygterm);
  return cygterm;

abort:
  xfree(cygterm);
  return NULL;
}

void cygterm_start(CygTerm *cygterm)
{
  if (cygterm->is_started) {
    return;
  }

  struct termios termios;
  if (cygterm->tcgetattr(cygterm->fd, &termios) == 0) {
    cygterm->restore_termios = termios;
    cygterm->restore_termios_valid = TRUE;

    termios.c_iflag &= ~(IXON|INLCR|ICRNL);
    termios.c_iflag &= ~(ICANON|ECHO|IEXTEN);
    termios.c_cc[VMIN] = 1;
    termios.c_cc[VTIME] = 0;
    termios.c_iflag &= ~ISIG;

    cygterm->tcsetattr(cygterm->fd, TCSANOW, &termios);
  }

    cygterm->is_started = TRUE;
}

void cygterm_stop(CygTerm *cygterm) {
  if (!cygterm->is_started) {
    return;
  }

  if (cygterm->restore_termios_valid) {
    cygterm->tcsetattr(cygterm->fd, TCSANOW, &cygterm->restore_termios);
  }

  cygterm->is_started = FALSE;
}

bool cygterm_get_winsize(CygTerm *cygterm, int *width, int *height) {
  struct winsize ws;
  int err;

  err = cygterm->ioctl(cygterm->fd, TIOCGWINSZ, &ws);
  if (err == -1) {
    return FALSE;
  }

  *width = ws.ws_col;
  *height = ws.ws_row;

  return TRUE;
}
