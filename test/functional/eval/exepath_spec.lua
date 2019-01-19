local helpers = require('test.functional.helpers')(after_each)
local eq, clear, call, iswin =
  helpers.eq, helpers.clear, helpers.call, helpers.iswin

describe('exepath() (Windows)', function()
  if not iswin() then return end  -- N/A for Unix.

  it('append extension, even if omit extension', function()
    clear({env={PATH=[[C:\Windows\system32;C:\Windows]]}})
    eq('c:\\windows\\system32\\cmd.exe', string.lower(call('exepath', 'cmd')))
  end)
end)
