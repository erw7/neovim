" The clipboard provider uses shell commands to communicate with the clipboard.
" The provider function will only be registered if a supported command is
" available.

if exists('g:loaded_clipboard_provider')
  finish
endif
" Default to 1.  provider#clipboard#Executable() may set 2.
" To force a reload:
"   :unlet g:loaded_clipboard_provider
"   :runtime autoload/provider/clipboard.vim
let g:loaded_clipboard_provider = 1

let s:copy = {}
let s:paste = {}
let s:clipboard = {}

" When caching is enabled, store the jobid of the xclip/xsel process keeping
" ownership of the selection, so we know how long the cache is valid.
let s:selection = { 'owner': 0, 'data': [], 'stderr_buffered': v:true }

function! s:selection.on_exit(jobid, data, event) abort
  " At this point this nvim instance might already have launched
  " a new provider instance. Don't drop ownership in this case.
  if self.owner == a:jobid
    let self.owner = 0
  endif
  if a:data != 0
    echohl WarningMsg
    echomsg 'clipboard: error invoking '.get(self.argv, 0, '?').': '.join(self.stderr)
    echohl None
  endif
endfunction

let s:selections = { '*': s:selection, '+': copy(s:selection) }

function! s:try_cmd(cmd, ...) abort
  let out = systemlist(a:cmd, (a:0 ? a:1 : ['']), 1)
  if v:shell_error
    if !exists('s:did_error_try_cmd')
      echohl WarningMsg
      echomsg "clipboard: error: ".(len(out) ? out[0] : v:shell_error)
      echohl None
      let s:did_error_try_cmd = 1
    endif
    return 0
  endif
  return out
endfunction

" Returns TRUE if `cmd` exits with success, else FALSE.
function! s:cmd_ok(cmd) abort
  call system(a:cmd)
  return v:shell_error == 0
endfunction

function! s:split_cmd(cmd) abort
  return (type(a:cmd) == v:t_string) ? split(a:cmd, " ") : a:cmd
endfunction

let s:cache_enabled = 1
let s:err = ''

function! provider#clipboard#Error() abort
  return s:err
endfunction

function! provider#clipboard#Reload(...) abort
  unlet g:loaded_clipboard_provider
  call timer_start(0, { _ -> execute('runtime autoload/provider/clipboard.vim') })
endfunction

function! provider#clipboard#Executable() abort
  let result = ''

  if exists('g:clipboard')
    if type({}) isnot# type(g:clipboard)
          \ || type({}) isnot# type(get(g:clipboard, 'copy', v:null))
          \ || type({}) isnot# type(get(g:clipboard, 'paste', v:null))
      let g:clipboard = {}
      let s:err = 'clipboard: invalid g:clipboard'
    else
      let s:copy = {}
      let s:copy['+'] = s:split_cmd(get(g:clipboard.copy, '+', v:null))
      let s:copy['*'] = s:split_cmd(get(g:clipboard.copy, '*', v:null))

      let s:paste = {}
      let s:paste['+'] = s:split_cmd(get(g:clipboard.paste, '+', v:null))
      let s:paste['*'] = s:split_cmd(get(g:clipboard.paste, '*', v:null))

      let s:cache_enabled = get(g:clipboard, 'cache_enabled', 0)
      let result = get(g:clipboard, 'name', 'g:clipboard')
    endif
  elseif has('mac')
    let s:copy['+'] = ['pbcopy']
    let s:paste['+'] = ['pbpaste']
    let s:copy['*'] = s:copy['+']
    let s:paste['*'] = s:paste['+']
    let s:cache_enabled = 0
    let result = 'pbcopy'
    let g:clipboard = { 'name' : result,
          \ 'copy' : s:copy, 'paste' : s:paste,
          \ 'cache_enabled' : s:cache_enabled }
  elseif !empty($WAYLAND_DISPLAY) && executable('wl-copy') && executable('wl-paste')
    let s:copy['+'] = ['wl-copy', '--foreground', '--type', 'text/plain']
    let s:paste['+'] = ['wl-paste', '--no-newline']
    let s:copy['*'] = ['wl-copy', '--foreground', '--primary', '--type', 'text/plain']
    let s:paste['*'] = ['wl-paste', '--no-newline', '--primary']
    let result = 'wl-copy'
    let g:clipboard = { 'name' : result,
          \ 'copy' : s:copy, 'paste' : s:paste,
          \ 'cache_enabled' : s:cache_enabled }
  elseif !empty($DISPLAY) && executable('xclip')
    let s:copy['+'] = ['xclip', '-quiet', '-i', '-selection', 'clipboard']
    let s:paste['+'] = ['xclip', '-o', '-selection', 'clipboard']
    let s:copy['*'] = ['xclip', '-quiet', '-i', '-selection', 'primary']
    let s:paste['*'] = ['xclip', '-o', '-selection', 'primary']
    let result = 'xclip'
    let g:clipboard = { 'name' : result,
          \ 'copy' : s:copy, 'paste' : s:paste,
          \ 'cache_enabled' : s:cache_enabled }
  elseif !empty($DISPLAY) && executable('xsel') && s:cmd_ok('xsel -o -b')
    let s:copy['+'] = ['xsel', '--nodetach', '-i', '-b']
    let s:paste['+'] = ['xsel', '-o', '-b']
    let s:copy['*'] = ['xsel', '--nodetach', '-i', '-p']
    let s:paste['*'] = ['xsel', '-o', '-p']
    let result = 'xsel'
    let g:clipboard = { 'name' : result,
          \ 'copy' : s:copy, 'paste' : s:paste,
          \ 'cache_enabled' : s:cache_enabled }
  elseif executable('lemonade')
    let s:copy['+'] = ['lemonade', 'copy']
    let s:paste['+'] = ['lemonade', 'paste']
    let s:copy['*'] = ['lemonade', 'copy']
    let s:paste['*'] = ['lemonade', 'paste']
    let result = 'lemonade'
    let g:clipboard = { 'name' : result,
          \ 'copy' : s:copy, 'paste' : s:paste,
          \ 'cache_enabled' : s:cache_enabled }
  elseif executable('doitclient')
    let s:copy['+'] = ['doitclient', 'wclip']
    let s:paste['+'] = ['doitclient', 'wclip', '-r']
    let s:copy['*'] = s:copy['+']
    let s:paste['*'] = s:paste['+']
    let result = 'doitclient'
    let g:clipboard = { 'name' : result,
          \ 'copy' : s:copy, 'paste' : s:paste,
          \ 'cache_enabled' : s:cache_enabled }
  elseif executable('win32yank.exe')
    if has('wsl') && getftype(exepath('win32yank.exe')) == 'link'
      let win32yank = resolve(exepath('win32yank.exe'))
    else
      let win32yank = 'win32yank.exe'
    endif
    let s:copy['+'] = [win32yank, '-i', '--crlf']
    let s:paste['+'] = [win32yank, '-o', '--lf']
    let s:copy['*'] = s:copy['+']
    let s:paste['*'] = s:paste['+']
    let result = 'win32yank'
    let g:clipboard = { 'name' : result,
          \ 'copy' : s:copy, 'paste' : s:paste,
          \ 'cache_enabled' : s:cache_enabled }
  elseif !empty($TMUX) && executable('tmux')
    let s:copy['+'] = ['tmux', 'load-buffer', '-']
    let s:paste['+'] = ['tmux', 'save-buffer', '-']
    let s:copy['*'] = s:copy['+']
    let s:paste['*'] = s:paste['+']
    let result = 'tmux'
    let g:clipboard = { 'name' : result,
          \ 'copy' : s:copy, 'paste' : s:paste,
          \ 'cache_enabled' : s:cache_enabled }
  endif

  if empty(result) && empty(s:err)
    let s:err = 'clipboard: No clipboard tool. :help clipboard'
    let g:clipboard = {}
  endif
  call dictwatcheradd(g:clipboard, '*', 'provider#clipboard#Reload')
  return result
endfunction

function! s:clipboard.get(reg) abort
  if type(s:paste[a:reg]) == v:t_func
    return s:paste[a:reg]()
  elseif s:selections[a:reg].owner > 0
    return s:selections[a:reg].data
  end
  return s:try_cmd(s:paste[a:reg])
endfunction

function! s:clipboard.set(lines, regtype, reg) abort
  if a:reg == '"'
    call s:clipboard.set(a:lines,a:regtype,'+')
    if s:copy['*'] != s:copy['+']
      call s:clipboard.set(a:lines,a:regtype,'*')
    end
    return 0
  end

  if type(s:copy[a:reg]) == v:t_func
    call s:copy[a:reg](a:lines, a:regtype)
    return 0
  end

  if s:cache_enabled == 0
    call s:try_cmd(s:copy[a:reg], a:lines)
    return 0
  end

  if s:selections[a:reg].owner > 0
    let prev_job = s:selections[a:reg].owner
  end
  let s:selections[a:reg] = copy(s:selection)
  let selection = s:selections[a:reg]
  let selection.data = [a:lines, a:regtype]
  let selection.argv = s:copy[a:reg]
  let selection.detach = s:cache_enabled
  let selection.cwd = "/"
  let jobid = jobstart(selection.argv, selection)
  if jobid > 0
    call jobsend(jobid, a:lines)
    call jobclose(jobid, 'stdin')
    " xclip does not close stdout when receiving input via stdin
    if selection.argv[0] ==# 'xclip'
      call jobclose(jobid, 'stdout')
    endif
    let selection.owner = jobid
    let ret = 1
  else
    echohl WarningMsg
    echomsg 'clipboard: failed to execute: '.(s:copy[a:reg])
    echohl None
    let ret = 1
  endif

  " The previous provider instance should exit when the new one takes
  " ownership, but kill it to be sure we don't fill up the job table.
  if exists('prev_job')
    call timer_start(1000, {... ->
          \ jobwait([prev_job], 0)[0] == -1
          \ && jobstop(prev_job)})
  endif

  return ret
endfunction

function! provider#clipboard#Call(method, args) abort
  if get(s:, 'here', v:false)  " Clipboard provider must not recurse. #7184
    return 0
  endif
  let s:here = v:true
  try
    return call(s:clipboard[a:method],a:args,s:clipboard)
  finally
    let s:here = v:false
  endtry
endfunction

" eval_has_provider() decides based on this variable.
let g:loaded_clipboard_provider = empty(provider#clipboard#Executable()) ? 1 : 2
