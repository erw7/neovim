local helpers = require('test.unit.helpers')(after_each)
local itp = helpers.gen_itp(it)

local cimport = helpers.cimport
local eq = helpers.eq
local ffi = helpers.ffi
local lib = helpers.lib
local NULL = helpers.NULL
local OK = helpers.OK
local FAIL = helpers.FAIL

local pty_process_win = cimport('./src/nvim/os/pty_process_win.h')

describe('quote_cmd_line function', function()
  function quote_cmd_line(src)
    local len = #src * 2 + 3
    local dist = ffi.new('char['..len..']')
    pty_process_win.quote_cmd_line(dist, len, src)
    return ffi.string(dist)
  end

  describe('empty string', function()
    eq('""', quote_cmd_line(''))
  end)

  itp('string not including spaces, tabs, and dobule quotes', function()
    local src = 'hello'
    eq(src, quote_cmd_line(src))
  end)

  itp('string containing a space', function()
    local src = 'hello world'
    eq('"'.. src..'"', quote_cmd_line(src))
  end)

  itp('string containing a tab', function()
    local src = "hello\tworld"
    eq('"'.. src..'"', quote_cmd_line(src))
  end)

  itp('a dobule quote', function()
    eq('"\""', quote_cmd_line('"'))
  end)

  itp('string containing a double quote', function()
    eq('"hello \"world"', quote_cmd_line('hello "world'))
  end)

  itp('string containing a backslash', function()
    eq('hello\\world', quote_cmd_line('hello\\world'))
  end)

  itp('string containing two backslashes', function()
    eq('hello\\\\world', quote_cmd_line('hello\\\\world'))
  end)

  itp('stirng containing a backslash before a double quote', function()
    eq('hello \\\"world', quote_cmd_line('hello \"world'))
  end)

  itp('stirng contains two backslashes before a double quote', function()
    eq('hello \\\\\"world', quote_cmd_line('hello \\"world'))
  end)

  itp('stirng that ends with a backslash', function()
    eq('hello world\\', quote_cmd_line('hello world\\'))
  end)
end)
