#define pty_process_spawn ut_pty_process_spawn
#define pty_process_resize ut_gpty_process_resize
#define pty_process_close ut_gpty_process_close
#define pty_process_close_master ut_gpty_process_close_master
#define pty_process_teardown ut_gpty_process_teardown

#include "nvim/os/pty_process_win.c"

void ut_gquote_cmd_arg(char *dist, size_t dist_remaining, const char *src)
{
  quote_cmd_arg(dist, dist_remaining, src);
}
