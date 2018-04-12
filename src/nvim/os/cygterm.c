#include <fcntl.h>
#include <io.h>
#include <stdbool.h>

#include "nvim/os/os.h"
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

HMODULE get_cygwin_dll_handle(void)
{
  static HMODULE hmodule = NULL;
  void (*init)(void);
  if (hmodule) {
    return hmodule;
  } else {
    MinttyType mintty;
    const char *dll = NULL;
    const char *init_func = NULL;
    for (int i = 0; i < 3; i++) {
      mintty = detect_mintty_type(i);
      if (mintty == MINTTY_CYGWIN) {
        dll = CYGWDLL;
        init_func = CYG_INIT_FUNC;
        break;
      } else if (mintty == MINTTY_MSYS) {
        dll = MSYSDLL;
        init_func = MSYS_INIT_FUNC;
        break;
      }
    }
    if (dll) {
      hmodule = LoadLibrary(dll);
      if (!hmodule) {
        return NULL;
      }
      init = (void (*)(void))GetProcAddress(hmodule, init_func);
      if (init) {
        init();
      } else {
        hmodule = NULL;
      }
    }
  }
  return hmodule;
}
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

  cygterm->hmodule = get_cygwin_dll_handle();
  cygterm->tcgetattr = (int (*)(int, struct termios *))GetProcAddress(cygterm->hmodule, "tcgetattr");
  cygterm->tcsetattr = (int (*)(int, int, const struct termios *))GetProcAddress(cygterm->hmodule, "tcsetattr");
  cygterm->ioctl = (int (*)(int, int, ...))GetProcAddress(cygterm->hmodule, "ioctl");
  cygterm->open = (int (*)(const char*, int))GetProcAddress(cygterm->hmodule, "open");
  cygterm->close = (int (*)(int))GetProcAddress(cygterm->hmodule, "close");
  cygterm->__errno = (int* (*)(void))GetProcAddress(cygterm->hmodule, "__errno");

  if (!cygterm->tcgetattr || !cygterm->tcsetattr || !cygterm->ioctl || !cygterm->open || !cygterm->close || !cygterm->__errno) {
    goto abort;
  }
  cygterm->is_started = FALSE;
  const char* tty = os_getenv("TTY");
  if (!tty){
    goto abort;
  }
  size_t len = strlen(tty) + 1;
  cygterm->tty = xmalloc(len);
  strncpy(cygterm->tty, tty, len);
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

  int fd = cygterm->open(cygterm->tty, O_RDWR | CYG_O_BINARY);
  if (fd == -1) {
    return;
  }

  struct termios termios;
  if (cygterm->tcgetattr(fd, &termios) == 0) {
    cygterm->restore_termios = termios;
    cygterm->restore_termios_valid = TRUE;

    termios.c_iflag &= ~(IXON|INLCR|ICRNL);
    termios.c_lflag &= ~(ICANON|ECHO|IEXTEN);
    termios.c_cc[VMIN] = 1;
    termios.c_cc[VTIME] = 0;
    termios.c_lflag &= ~ISIG;

    cygterm->tcsetattr(fd, TCSANOW, &termios);
  }

    cygterm->is_started = TRUE;
    cygterm->close(fd);
}

void cygterm_stop(CygTerm *cygterm) {
  if (!cygterm->is_started) {
    return;
  }

  int fd = cygterm->open(cygterm->tty, O_RDWR | CYG_O_BINARY);
  if (fd == -1) {
    return;
  }
  if (cygterm->restore_termios_valid) {
    cygterm->tcsetattr(fd, TCSANOW, &cygterm->restore_termios);
  }

  cygterm->is_started = FALSE;
  cygterm->close(fd);
}

bool cygterm_get_winsize(CygTerm *cygterm, int *width, int *height) {
  struct winsize ws;
  int err, err_no;

  int fd = cygterm->open(cygterm->tty, O_RDONLY | CYG_O_BINARY);
  if (fd == -1) {
    return FALSE;
  }

  do {
    err = cygterm->ioctl(fd, TIOCGWINSZ, &ws);
    int *e = cygterm->__errno();
    if (e == NULL) {
      err_no = -1;
    } else {
      err_no = *e;
    }
  } while(err == -1 && err_no == EINTR);

  if (err == -1) {
    return FALSE;
  }

  *width = ws.ws_col;
  *height = ws.ws_row;

  return TRUE;
}
