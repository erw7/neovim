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
    return false;
  }
  // Cygwin/msys's pty is a pipe.
  if (GetFileType(h) != FILE_TYPE_PIPE) {
    return false;
  }
  FILE_NAME_INFO *nameinfo = xmalloc(size);
  if (nameinfo == NULL) {
    return false;
  }
  // Check the name of the pipe:
  // '\{cygwin,msys}-XXXXXXXXXXXXXXXX-ptyN-{from,to}-master'
  MinttyType result = kNoneMintty;
  if (GetFileInformationByHandleEx(h, FileNameInfo, nameinfo, size)) {
    nameinfo->FileName[nameinfo->FileNameLength / sizeof(WCHAR)] = L'\0';
    p = nameinfo->FileName;
    if (wcsstr(p, L"\\cygwin-") == p) {
      p += 8;
      result = kMinttyCygwin;
    } else if (wcsstr(p, L"\\msys-") == p) {
      p += 6;
      result = kMinttyMsys;
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
  return p != NULL ? result : kNoneMintty;
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
      if (mintty == kMinttyCygwin) {
        dll = CYGWDLL;
        init_func = CYG_INIT_FUNC;
        break;
      } else if (mintty == kMinttyMsys) {
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

CygTerm *cygterm_new(int fd)
{
  MinttyType mintty = detect_mintty_type(fd);
  if (mintty == kNoneMintty) {
    return NULL;
  }

  CygTerm *cygterm = (CygTerm *)xmalloc(sizeof(CygTerm));
  if (!cygterm) {
    return NULL;
  }

  cygterm->hmodule = get_cygwin_dll_handle();
  cygterm->tcgetattr =
    (tcgetattr_fn)GetProcAddress(cygterm->hmodule, "tcgetattr");
  cygterm->tcsetattr =
    (tcsetattr_fn)GetProcAddress(cygterm->hmodule, "tcsetattr");
  cygterm->ioctl = (ioctl_fn)GetProcAddress(cygterm->hmodule, "ioctl");
  cygterm->open = (open_fn)GetProcAddress(cygterm->hmodule, "open");
  cygterm->close = (close_fn)GetProcAddress(cygterm->hmodule, "close");
  cygterm->__errno = (errno_fn)GetProcAddress(cygterm->hmodule, "__errno");

  if (!cygterm->tcgetattr
      || !cygterm->tcsetattr
      || !cygterm->ioctl
      || !cygterm->open
      || !cygterm->close
      || !cygterm->__errno) {
    goto abort;
  }
  cygterm->is_started = false;
  const char *tty = os_getenv("TTY");
  if (!tty) {
    goto abort;
  }
  size_t len = strlen(tty) + 1;
  cygterm->tty = xmalloc(len);
  snprintf(cygterm->tty, len, "%s", tty);
  cygterm->fd = -1;
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

  if (cygterm->fd == -1) {
    int fd = cygterm->open(cygterm->tty, O_RDWR | CYG_O_BINARY);
    if (fd == -1) {
      return;
    }
    cygterm->fd = fd;
  }

  struct termios termios;
  if (cygterm->tcgetattr(cygterm->fd, &termios) == 0) {
    cygterm->restore_termios = termios;
    cygterm->restore_termios_valid = true;

    termios.c_iflag &= ~(IXON|INLCR|ICRNL);
    termios.c_lflag &= ~(ICANON|ECHO|IEXTEN);
    termios.c_cc[VMIN] = 1;
    termios.c_cc[VTIME] = 0;
    termios.c_lflag &= ~ISIG;

    cygterm->tcsetattr(cygterm->fd, TCSANOW, &termios);
  }

  cygterm->is_started = true;
}

void cygterm_stop(CygTerm *cygterm)
{
  if (!cygterm->is_started) {
    return;
  }

  if (cygterm->fd == -1) {
    int fd = cygterm->open(cygterm->tty, O_RDWR | CYG_O_BINARY);
    if (fd == -1) {
      return;
    }
    cygterm->fd = fd;
  }
  if (cygterm->restore_termios_valid) {
    cygterm->tcsetattr(cygterm->fd, TCSANOW, &cygterm->restore_termios);
  }

  cygterm->is_started = false;
  cygterm->close(cygterm->fd);
}

bool cygterm_get_winsize(CygTerm *cygterm, int *width, int *height)
{
  struct winsize ws;
  int err, err_no;

  if (cygterm->fd == -1) {
    int fd = cygterm->open(cygterm->tty, O_RDWR | CYG_O_BINARY);
    if (fd == -1) {
      return false;
    }
    cygterm->fd = fd;
  }

  do {
    err = cygterm->ioctl(cygterm->fd, TIOCGWINSZ, &ws);
    int *e = cygterm->__errno();
    if (e == NULL) {
      err_no = -1;
    } else {
      err_no = *e;
    }
  } while (err == -1 && err_no == EINTR);

  if (err == -1) {
    return false;
  }

  *width = ws.ws_col;
  *height = ws.ws_row;

  return true;
}
