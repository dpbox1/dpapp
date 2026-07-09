local dplog = require("dplog")
local dpret = require("dpret")
local dpasc = require("dpasc")

local M = {}

function M.hello(task)
    if dplua.type_id == 0 then
        dplog.notice("test", "hello world0")
    else
        dplog.notice("test", "hello world1")
    end
    return dpret.OK
end

function M.__init00(args)
    local res = dpasc.ctc_each(1, "hello", nil)
    print(res)

    -- 在当前线程执行，打印 "hello world0"
    res = dpasc.ctc_each(0, "hello", nil)

    print(res)

    -- os.exit(0)
end

function M.__init01(args)
end

function M.__main__(args, hdrs)
    hdrs[0] = { init = "__init00" }
    hdrs[1] = { init = "__init01" }
    return 2
end

return M
