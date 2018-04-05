#ifndef NVIM_OS_CYGTERM_H
#define NVIM_OS_CYGTERM_H

#include <windows.h>

typedef enum {
  NONE_MINTTY,
  MINTTY_CYGWIN,
  MINTTY_MSYS
} MinttyType;

/* iflag bits */
#define INLCR   0x00040
#define ICRNL   0x00100
#define IXON    0x00400

/* lflag bits */
#define ISIG    0x0001
#define ICANON  0x0002
#define ECHO    0x0004
#define IEXTEN  0x0100

#define VMIN            9
#define VTIME           16

#define NCCS            18

#define TCSANOW         2

#define TIOCGWINSZ (('T' << 8) | 1)

typedef unsigned char cc_t;
typedef unsigned int  tcflag_t;
typedef unsigned int  speed_t;

struct termios
{
  tcflag_t      c_iflag;
  tcflag_t      c_oflag;
  tcflag_t      c_cflag;
  tcflag_t      c_lflag;
  char          c_line;
  cc_t          c_cc[NCCS];
  speed_t       c_ispeed;
  speed_t       c_ospeed;
};

struct winsize
{
  unsigned short ws_row, ws_col;
  unsigned short ws_xpixel, ws_ypixel;
};

typedef struct {
  HMODULE hmodule;
  void (*init) (void);
  int (*tcgetattr) (int, struct termios *);
  int (*tcsetattr) (int, int, const struct termios *);
  int (*ioctl) (int, int, ...);
  int fd;
  bool is_started;
  struct termios restore_termios;
  bool restore_termios_valid;
} CygTerm;

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "os/cygterm.h.generated.h"
#endif
#endif  // NVIM_OS_CYGTERM_H
