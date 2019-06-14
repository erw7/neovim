local buffer = {}
local ffi = require('ffi')
ffi.cdef[[
typedef struct { int __handle; } buf_t;
]]

buffer.insert = function(self, newline, ...)
  local pos = ...
  if pos  == nil then
    pos = vim.api.nvim_buf_line_count(self.__handle)
  end
  vim.api.nvim_buf_set_lines(self.__handle, pos, pos, true, {newline})
end

buffer.next = function(self)
  if vim.api.nvim_buf_is_valid(self.buf + 1) then
    return buffer.buffer(self.buf + 1)
  else
    return nil
  end
end

buffer.previous = function(self)
  if self.buf - 1 > 0 and vim.api.nvim_buf_is_valid(self.buf - 1) then
    return buffer.buffer(self.buf - 1)
  else
    return nil
  end
end

buffer.isvalid = function(self)
  return vim.api.nvim_buf_is_valid(self.__handle)
end

buffer.__call = function(self)
  vim.api.nvim_set_current_buf(self.__handle)
end

buffer.__newindex = function(self, index, newline)
  if type(index) == 'number' then
    vim.api.nvim_buf_set_lines(self.buf, index - 1, index, true, {newline})
  else
    error([[bad argument #2 to '__newindex' (number expected, got ]] .. type(index) .. ')')
  end
end

buffer.__len = function(self)
  return vim.api.nvim_buf_line_count(self.__handle)
end

buffer.__index = function(self, key)
  if key == 'name' then
    return vim.eval('bufname(' .. self.__handle .. ')')
  elseif key == 'fname' then
    return vim.api.nvim_buf_get_name(self.__handle)
  elseif key == 'number' then
    return self.__handle
  else
    return buffer[key]
  end
end

local mt = {
  __index = buffer.__index,
  __call = buffer.__call,
  __newindex = buffer.__newindex,
  __len = buffer.__len,
}
--[[
Lua 5.1 does not support __len on metatable events, So we use ffi.metatype().
]]--
local buf = ffi.metatype("buf_t", mt)

function buffer.buffer(self, arg)
  local obj= nil
  if arg then
    local bufs = vim.api.nvim_list_bufs()
    if type(arg) == 'number' then
      if bufs[arg] then
        obj = buf(arg)
      end
    elseif type(arg) == 'string' then
      for _, handle in pairs(bufs) do
        if arg == vim.api.nvim_buf_get_name(handle)then
          obj = buf(handle)
          break
        elseif arg == vim.eval('bufname(' .. handle .. ')') then
          obj = buf(handle)
          break
        end
      end
    else
      obj = buf(bufs[1])
    end
  else
    obj = buf(vim.api.nvim_get_current_buf())
  end

  return obj
end

setmetatable(buffer, { __call = function(self, arg) return buffer.buffer(self, arg) end })

return buffer
