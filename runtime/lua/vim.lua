local buffer = require('vim/buffer')

vim.eval = vim.api.nvim_eval
vim.buffer = buffer
vim.type = function(obj)
  local ok, name = pcall(function() return obj.__vim_type end)
  if ok then
    return name
  else
    return type(obj)
  end
end

return vim
