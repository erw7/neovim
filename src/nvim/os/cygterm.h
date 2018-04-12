#ifndef NVIM_OS_CYGTERM_H
#define NVIM_OS_CYGTERM_H

#include <windows.h>

#ifdef __x86_64__
#include <subauth.h>

// THese struct came from ntdll.h of Cygwin
//
/* Checked on 64 bit. */
typedef struct _PEB_LDR_DATA
{
  ULONG Length;
  BOOLEAN Initialized;
  PVOID SsHandle;
  /* Heads up!  The pointers within the LIST_ENTRYs don't point to the
     start of the next LDR_DATA_TABLE_ENTRY, but rather they point to the
     start of their respective LIST_ENTRY *within* LDR_DATA_TABLE_ENTRY. */
  LIST_ENTRY InLoadOrderModuleList;
  LIST_ENTRY InMemoryOrderModuleList;
  LIST_ENTRY InInitializationOrderModuleList;
  PVOID EntryInProgress;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

/* Checked on 64 bit. */
typedef struct _RTL_USER_PROCESS_PARAMETERS
{
  ULONG AllocationSize;
  ULONG Size;
  ULONG Flags;
  ULONG DebugFlags;
  HANDLE hConsole;
  ULONG ProcessGroup;
  HANDLE hStdInput;
  HANDLE hStdOutput;
  HANDLE hStdError;
  UNICODE_STRING CurrentDirectoryName;
  HANDLE CurrentDirectoryHandle;
  UNICODE_STRING DllPath;
  UNICODE_STRING ImagePathName;
  UNICODE_STRING CommandLine;
  PWSTR Environment;
  ULONG dwX;
  ULONG dwY;
  ULONG dwXSize;
  ULONG dwYSize;
  ULONG dwXCountChars;
  ULONG dwYCountChars;
  ULONG dwFillAttribute;
  ULONG dwFlags;
  ULONG wShowWindow;
  UNICODE_STRING WindowTitle;
  UNICODE_STRING DesktopInfo;
  UNICODE_STRING ShellInfo;
  UNICODE_STRING RuntimeInfo;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;


/* Checked on 64 bit. */
typedef struct _CLIENT_ID
{
  HANDLE UniqueProcess;
  HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

/* Checked on 64 bit. */
typedef struct _PEB
{
  BYTE Reserved1[2];
  BYTE BeingDebugged;
  BYTE Reserved2[1];
  PVOID Reserved3[2];
  PPEB_LDR_DATA Ldr;
  PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
  PVOID Reserved4;
  PVOID ProcessHeap;
  PRTL_CRITICAL_SECTION FastPebLock;
  PVOID Reserved5[2];
  ULONG EnvironmentUpdateCount;
  BYTE Reserved6[228];
  PVOID Reserved7[49];
  ULONG SessionId;
  /* A lot more follows... */
} PEB, *PPEB;

/* Checked on 64 bit. */
typedef struct _GDI_TEB_BATCH
{
  ULONG Offset;
  HANDLE HDC;
  ULONG Buffer[0x136];
} GDI_TEB_BATCH, *PGDI_TEB_BATCH;

typedef struct _TEB
{
  NT_TIB Tib;
  PVOID EnvironmentPointer;
  CLIENT_ID ClientId;
  PVOID ActiveRpcHandle;
  PVOID ThreadLocalStoragePointer;
  PPEB Peb;
  ULONG LastErrorValue;
  ULONG CountOfOwnedCriticalSections;
  PVOID CsrClientThread;
  PVOID Win32ThreadInfo;
  ULONG User32Reserved[26];
  ULONG UserReserved[5];
  PVOID WOW32Reserved;
  LCID CurrentLocale;
  ULONG FpSoftwareStatusRegister;
  PVOID SystemReserved1[54];
  LONG ExceptionCode;
  PVOID ActivationContextStackPointer;
  UCHAR SpareBytes1[0x30 - 3 * sizeof(PVOID)];
  ULONG TxFsContext;
  GDI_TEB_BATCH GdiTebBatch;
  CLIENT_ID RealClientId;
  PVOID GdiCachedProcessHandle;
  ULONG GdiClientPID;
  ULONG GdiClientTID;
  PVOID GdiThreadLocalInfo;
  SIZE_T Win32ClientInfo[62];
  PVOID glDispatchTable[233];
  SIZE_T glReserved1[29];
  PVOID glReserved2;
  PVOID glSectionInfo;
  PVOID glSection;
  PVOID glTable;
  PVOID glCurrentRC;
  PVOID glContext;
  ULONG LastStatusValue;
  UNICODE_STRING StaticUnicodeString;
  WCHAR StaticUnicodeBuffer[261];
  PVOID DeallocationStack;
  PVOID TlsSlots[64];
  BYTE Reserved3[8];
  PVOID Reserved4[26];
  PVOID ReservedForOle;
  PVOID Reserved5[4];
  PVOID TlsExpansionSlots;
  /* A lot more follows... */
} TEB, *PTEB;
#endif // __x86_64__

typedef enum {
  NONE_MINTTY,
  MINTTY_CYGWIN,
  MINTTY_MSYS
} MinttyType;

// These definition came from header file of Cygwin
#define EINTR      4
/* iflag bits */
#define INLCR      0x00040
#define ICRNL      0x00100
#define IXON       0x00400

/* lflag bits */
#define ISIG       0x0001
#define ICANON     0x0002
#define ECHO       0x0004
#define IEXTEN     0x0100

#define VMIN       9
#define VTIME      16

#define NCCS       18

#define TCSANOW    2

#define TIOCGWINSZ (('T' << 8) | 1)

#define CYG_O_BINARY   0x10000

typedef unsigned char cc_t;
typedef unsigned int  tcflag_t;
typedef unsigned int  speed_t;
typedef unsigned short otcflag_t;
typedef unsigned char ospeed_t;

// struct __oldtermios
struct termios
{
  otcflag_t     c_iflag;
  otcflag_t     c_oflag;
  otcflag_t     c_cflag;
  otcflag_t     c_lflag;
  char          c_line;
  cc_t          c_cc[NCCS];
  ospeed_t      c_ispeed;
  ospeed_t      c_ospeed;
};

// struct termios
// {
//   tcflag_t      c_iflag;
//   tcflag_t      c_oflag;
//   tcflag_t      c_cflag;
//   tcflag_t      c_lflag;
//   char          c_line;
//   cc_t          c_cc[NCCS];
//   speed_t       c_ispeed;
//   speed_t       c_ospeed;
// };


struct winsize
{
  unsigned short ws_row, ws_col;
  unsigned short ws_xpixel, ws_ypixel;
};

typedef struct {
  HMODULE hmodule;
  int (*tcgetattr) (int, struct termios *);
  int (*tcsetattr) (int, int, const struct termios *);
  int (*ioctl) (int, int, ...);
  int (*open) (const char*, int);
  int (*close) (int);
  int* (*__errno) (void);
  char *tty;
  bool is_started;
  struct termios restore_termios;
  bool restore_termios_valid;
} CygTerm;

#define PADDING_SIZE    32768

struct padding {
  char *end;
  size_t delta;
  char block[PADDING_SIZE];
  char padding[PADDING_SIZE];
};

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "os/cygterm.h.generated.h"
#endif
#endif  // NVIM_OS_CYGTERM_H
