local buffer = require('vim/buffer')

vim.eval = vim.api.nvim_eval
vim.buffer = buffer

return vim
