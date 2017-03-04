#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "nvim/memory.h"
#include "nvim/os/pty_process_win.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "os/pty_process_win.c.generated.h"
#endif

static void wait_eof_timer_cb(uv_timer_t* wait_eof_timer)
  FUNC_ATTR_NONNULL_ALL
{
  PtyProcess *ptyproc =
    (PtyProcess *)((uv_handle_t *)wait_eof_timer->data);
  Process *proc = (Process *)ptyproc;

  if (!uv_is_readable(proc->out->uvstream)) {
    uv_timer_stop(&ptyproc->wait_eof_timer);
    pty_process_finish2(ptyproc);
  }
}

static void CALLBACK pty_process_finish1(void *context, BOOLEAN unused)
  FUNC_ATTR_NONNULL_ALL
{
  PtyProcess *ptyproc = (PtyProcess *)context;
  Process *proc = (Process *)ptyproc;

  uv_timer_init(&proc->loop->uv, &ptyproc->wait_eof_timer);
  ptyproc->wait_eof_timer.data = (void *)ptyproc;
  uv_timer_start(&ptyproc->wait_eof_timer, wait_eof_timer_cb, 200, 200);
}

int pty_process_spawn(PtyProcess *ptyproc)
  FUNC_ATTR_NONNULL_ALL
{
  Process *proc = (Process *)ptyproc;
  int status = 0;
  winpty_error_ptr_t err = NULL;
  winpty_config_t *cfg = NULL;
  winpty_spawn_config_t *spawncfg = NULL;
  winpty_t *wp = NULL;
  char *in_name = NULL, *out_name = NULL;
  HANDLE process_handle = NULL;
  uv_connect_t *in_req = NULL, *out_req = NULL;
  wchar_t *cmdline = NULL, *cwd = NULL;

  assert(proc->in && proc->out && !proc->err);

  if (!(cfg = winpty_config_new(
      WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION, &err))) {
    goto cleanup;
  }
  winpty_config_set_initial_size(
      cfg,
      ptyproc->width,
      ptyproc->height);

  if (!(wp = winpty_open(cfg, &err))) {
    goto cleanup;
  }

  if ((status = utf16_to_utf8(winpty_conin_name(wp), &in_name))) {
    goto cleanup;
  }
  if ((status = utf16_to_utf8(winpty_conout_name(wp), &out_name))) {
    goto cleanup;
  }
  in_req = xmalloc(sizeof(uv_connect_t));
  out_req = xmalloc(sizeof(uv_connect_t));
  uv_pipe_connect(
      in_req,
      &proc->in->uv.pipe,
      in_name,
      pty_process_connect_cb);
  uv_pipe_connect(
      out_req,
      &proc->out->uv.pipe,
      out_name,
      pty_process_connect_cb);

  if (proc->cwd != NULL && (status = utf8_to_utf16(proc->cwd, &cwd))) {
    goto cleanup;
  }
  if ((status = build_cmdline(proc->argv, &cmdline))) {
    goto cleanup;
  }
  if (!(spawncfg = winpty_spawn_config_new(
      WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
      NULL, cmdline, cwd, NULL, &err))) {
    goto cleanup;
  }
  if (!winpty_spawn(wp, spawncfg, &process_handle, NULL, NULL, &err)) {
    goto cleanup;
  }

  if (!RegisterWaitForSingleObject(
        &ptyproc->finish_wait,
        process_handle, pty_process_finish1, ptyproc,
        INFINITE, WT_EXECUTEDEFAULT | WT_EXECUTEONLYONCE)) {
    abort();
  }

  while (in_req->handle || out_req->handle) {
    uv_run(&proc->loop->uv, UV_RUN_ONCE);
  }

  ptyproc->wp = wp;
  ptyproc->process_handle = process_handle;
  wp = NULL;
  process_handle = NULL;

cleanup:
  if (err != NULL) {
    status = (int)winpty_error_code(err);
  }
  winpty_error_free(err);
  winpty_config_free(cfg);
  winpty_spawn_config_free(spawncfg);
  winpty_free(wp);
  xfree(in_name);
  xfree(out_name);
  if (process_handle != NULL) {
    CloseHandle(process_handle);
  }
  xfree(in_req);
  xfree(out_req);
  xfree(cmdline);
  xfree(cwd);
  return status;
}

void pty_process_resize(PtyProcess *ptyproc, uint16_t width,
                        uint16_t height)
  FUNC_ATTR_NONNULL_ALL
{
  winpty_error_ptr_t err = NULL;
  if (ptyproc->wp != NULL) {
    winpty_set_size(ptyproc->wp, width, height, NULL);
  }
}

void pty_process_close(PtyProcess *ptyproc)
  FUNC_ATTR_NONNULL_ALL
{
  Process *proc = (Process *)ptyproc;

  pty_process_close_master(ptyproc);

  if (proc->internal_close_cb) {
    proc->internal_close_cb(proc);
  }
}

void pty_process_close_master(PtyProcess *ptyproc)
  FUNC_ATTR_NONNULL_ALL
{
  if (ptyproc->wp != NULL) {
    winpty_free(ptyproc->wp);
    ptyproc->wp = NULL;
  }
}

void pty_process_teardown(Loop *loop)
  FUNC_ATTR_NONNULL_ALL
{
}

static void pty_process_connect_cb(uv_connect_t *req, int status)
  FUNC_ATTR_NONNULL_ALL
{
  assert(status == 0);
  req->handle = NULL;
}

static void pty_process_finish2(PtyProcess *ptyproc)
  FUNC_ATTR_NONNULL_ALL
{
  Process *proc = (Process *)ptyproc;

  UnregisterWaitEx(ptyproc->finish_wait, NULL);
  uv_close((uv_handle_t *)&ptyproc->wait_eof_timer, NULL);

  DWORD exit_code = 0;
  GetExitCodeProcess(ptyproc->process_handle, &exit_code);
  proc->status = (int)exit_code;

  CloseHandle(ptyproc->process_handle);
  ptyproc->process_handle = NULL;

  proc->internal_exit_cb(proc);
}

int build_cmdline(char **argv, wchar_t **cmdline)
  FUNC_ATTR_NONNULL_ALL
{
  char *cmd = NULL, *args = NULL, *eargv = NULL, *qargv = NULL, *fmt;
  size_t len;
  int ret = 0, argc = 1;
  bool need_quote = false;

  len = STRLEN(argv[0]) + 1;
  fmt = "%s";
  if (strstr(argv[0], " ")) {
    len += 2;
    fmt = "\"%s\"";
  }
  cmd = xmalloc(len);
  snprintf(cmd, len, fmt, argv[0]);

  for (int i = 1; argv[i] != NULL; ++i) {
    ++argc;
  }

  if (argc == 3 && STRCMP(p_sh, argv[0]) == 0
      && STRCMP(p_shcf, argv[1]) == 0) {
    size_t args_len = STRLEN(argv[2]);
    if (*p_sxq != NUL) {
      if (STRCMP(p_sxq, "(") == 0) {
        if (argv[2][0] != '('  || argv[2][args_len - 1] != ')') {
          need_quote = true;
        }
      } else if (STRCMP(p_sxq, "\"(") == 0) {
        if (argv[2][0] != '"' || argv[2][1] != ')'
            || argv[2][args_len - 2] != ')' || argv[2][args_len - 1] != '"') {
          need_quote = true;
        }
      } else {
        if (strstr(argv[2], (char *)p_sxq) != argv[2] &&
            STRCMP(argv[2] - STRLEN(p_sxq) - 1, p_sxq) != 0) {
          need_quote = true;
        }
      }
    }
    if (need_quote) {
      eargv = argv[2];
      if (*p_sxe != NUL && STRCMP(p_sxq, "(") == 0) {
        eargv = (char *)vim_strsave_escaped_ext((char_u *)argv[2], p_sxe, '^', false);
      }
      size_t qargv_len = STRLEN(eargv) + STRLEN(p_sxq) * 2 + 1;
      qargv = xmalloc(qargv_len);
      if (STRCMP(p_sxq, "(") == 0) {
        snprintf(qargv, qargv_len, "(%s)", eargv);
      } else if (STRCMP(p_sxq, "\"(") == 0) {
        snprintf(qargv, qargv_len, "\"(%s)\"", eargv);
      } else {
        snprintf(qargv, qargv_len, "%s%s%s", p_sxq, eargv, p_sxq);
      }
      if (eargv != argv[2]) {
        xfree((void *)eargv);
      }
    } else {
      qargv = argv[2];
    }
    len += STRLEN(p_shcf) + STRLEN(qargv) + 3;
    args = xmalloc(len);
    snprintf(args, len, "%s %s %s", argv[0], p_shcf, qargv);
    if (qargv != argv[2]) {
      xfree(qargv);
    }
  } else {
    for (int i = 1; argv[i] != NULL; ++i) {
      if (strstr(argv[i], " ") != 0) {
        len += STRLEN(argv[i]) + 3;
        for (int n = 0; argv[i][n] != '\0'; ++n) {
          if (argv[i][n] == '"') {
            ++len;
          }
        }
      } else {
        len += STRLEN(argv[i]) + 1;
      }
    }
    args = xmalloc(len);
    STRCPY(args, cmd);
    for (int i = 1; argv[i] != NULL; ++i) {
      eargv = NULL;
      qargv = NULL;
      STRCAT(args, " ");
      if (strstr(argv[i], " ")) {
        eargv = (char *)vim_strsave_escaped((char_u *)argv[i], (char_u *)"\"");
        len = STRLEN(eargv) + 3;
        qargv = xmalloc(len);
        snprintf(qargv, len, "\"%s\"", eargv);
        STRCAT(args, qargv);
        xfree(eargv);
        xfree(qargv);
      } else {
        STRCAT(args, argv[i]);
      }
    }
  }
  fprintf(stderr, "%s\n", args);
  ret = utf8_to_utf16(args, cmdline);

  xfree(cmd);
  xfree(args);
  return ret;
}
