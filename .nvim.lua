-- Project-local Neovim config, loaded by misc\nv.bat (nvim -S)
local overseer = require("overseer")
local root = vim.fs.dirname(debug.getinfo(1, "S").source:sub(2))

-- Parse MSVC output, same as hh.sublime-project's file_regex, e.g.
--   W:\hh\code\win32_hh.cpp(42): error C2065: 'foo': undeclared identifier
--   W:\hh\code\win32_hh.cpp(42,7): warning C4100: 'bar': unreferenced parameter
-- https://github.com/stevearc/overseer.nvim/blob/master/doc/guides.md#parsing-output
local msvc_types = { error = "E", fatal = "E", warning = "W", note = "N" }
local function parse_msvc(line)
	local filename, lnum, col, text = line:match("^%s*(.-)%((%d+),?(%d*)%)%s?:%s(.+)$")
	if filename then
		return {
			filename = filename,
			lnum = tonumber(lnum),
			col = tonumber(col),
			text = text,
			type = msvc_types[text:match("^%a+")],
		}
	end
end

-- Run build.bat, parsing its output into diagnostics and the quickfix list
vim.keymap.set("n", "<leader>ob", function()
	local task = overseer.new_task({
		name = "build.bat",
		cmd = { "cmd.exe", "/c", ".\\build.bat" },
		cwd = root,
		-- Plain pipes instead of a terminal: ConPTY injects escape codes and
		-- hard-wraps long lines, which breaks parsing
		strategy = { "jobstart", use_terminal = false },
		components = {
			{ "on_output_parse", parser = parse_msvc },
			{ "on_result_diagnostics", remove_on_restart = true },
			{ "on_result_diagnostics_quickfix", open = false, close = true },
			"unique",
			"default",
		},
	})
	-- Show problems in fzf-lua instead of the quickfix window
	task:subscribe("on_complete", function(_, _, result)
		if result and not vim.tbl_isempty(result.diagnostics or {}) then
			require("fzf-lua").quickfix()
		end
	end)
	task:start()
end, { desc = "Build" })
