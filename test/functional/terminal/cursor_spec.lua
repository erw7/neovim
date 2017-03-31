local helpers = require('test.functional.helpers')(after_each)
local Screen = require('test.functional.ui.screen')
local thelpers = require('test.functional.terminal.helpers')
local feed, clear, nvim = helpers.feed, helpers.clear, helpers.nvim
local nvim_dir, execute = helpers.nvim_dir, helpers.execute
local hide_cursor = thelpers.hide_cursor
local show_cursor = thelpers.show_cursor

describe('terminal cursor', function()
  local screen

  before_each(function()
    clear()
    screen = thelpers.screen_setup()
  end)


  it('moves the screen cursor when focused', function()
    thelpers.feed_data('testing cursor')
    screen:expect([[
      tty ready                                         |
      testing cursor{1: }                                   |
                                                        |
                                                        |
                                                        |
                                                        |
      {3:-- TERMINAL --}                                    |
    ]])
  end)

  it('is highlighted when not focused', function()
    feed('<c-\\><c-n>')
    screen:expect([[
      tty ready                                         |
      {2:^ }                                                 |
                                                        |
                                                        |
                                                        |
                                                        |
                                                        |
    ]])
  end)

  describe('with number column', function()
    before_each(function()
      feed('<c-\\><c-n>:set number<cr>')
    end)

    it('is positioned correctly when unfocused', function()
      screen:expect([[
        {7:  1 }tty ready                                     |
        {7:  2 }{2:^ }                                             |
        {7:  3 }                                              |
        {7:  4 }                                              |
        {7:  5 }                                              |
        {7:  6 }                                              |
        :set number                                       |
      ]])
    end)

    it('is positioned correctly when focused', function()
      feed('i')
      helpers.wait()
      screen:expect([[
        {7:  1 }tty ready                                     |
        {7:  2 }{1: }                                             |
        {7:  3 }                                              |
        {7:  4 }                                              |
        {7:  5 }                                              |
        {7:  6 }                                              |
        {3:-- TERMINAL --}                                    |
      ]])
    end)
  end)

  describe('when invisible', function()
    it('is not highlighted and is detached from screen cursor', function()
      hide_cursor()
      screen:expect([[
        tty ready                                         |
                                                          |
                                                          |
                                                          |
                                                          |
                                                          |
        {3:-- TERMINAL --}                                    |
      ]])
      show_cursor()
      screen:expect([[
        tty ready                                         |
        {1: }                                                 |
                                                          |
                                                          |
                                                          |
                                                          |
        {3:-- TERMINAL --}                                    |
      ]])
      -- same for when the terminal is unfocused
      feed('<c-\\><c-n>')
      hide_cursor()
      screen:expect([[
        tty ready                                         |
        ^                                                  |
                                                          |
                                                          |
                                                          |
                                                          |
                                                          |
      ]])
      show_cursor()
      screen:expect([[
        tty ready                                         |
        {2:^ }                                                 |
                                                          |
                                                          |
                                                          |
                                                          |
                                                          |
      ]])
    end)
  end)
end)


describe('cursor with customized highlighting', function()
  local screen

  before_each(function()
    clear()
    nvim('command', 'highlight TermCursor ctermfg=45 ctermbg=46 cterm=NONE')
    nvim('command', 'highlight TermCursorNC ctermfg=55 ctermbg=56 cterm=NONE')
    screen = Screen.new(50, 7)
    screen:set_default_attr_ids({
      [1] = {foreground = 45, background = 46},
      [2] = {foreground = 55, background = 56},
      [3] = {bold = true},
    })
    screen:attach({rgb=false})
    execute('call termopen(["'..nvim_dir..'/tty-test"]) | startinsert')
  end)

  it('overrides the default highlighting', function()
    screen:expect([[
      tty ready                                         |
      {1: }                                                 |
                                                        |
                                                        |
                                                        |
                                                        |
      {3:-- TERMINAL --}                                    |
    ]])
    feed('<c-\\><c-n>')
    screen:expect([[
      tty ready                                         |
      {2:^ }                                                 |
                                                        |
                                                        |
                                                        |
                                                        |
                                                        |
    ]])
  end)
end)

