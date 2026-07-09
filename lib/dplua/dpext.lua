-- dpext 扩展 API（Snowflake ID、comspec 辅助；对齐 dpcwc_ext.h）
local ffi = require("ffi")

ffi.cdef([[
uint64_t dpid_next(char* out_str);
uint64_t dpid_2u64(const char* str);
void dpid_2str(uint64_t id, char* out_str);

struct dptask_comspec
{
    char* info;
    char* body;
    char* args;
    char* result;
    int ok;
    int reto_node;
    int reto_name;
};
typedef struct dptask_comspec dptask_comspec_t;

dptask_comspec_t* dptask_comspec_new(
    const char* info, const char* body, const char* args, int reto_node, int reto_name);
void dptask_comspec_set_result(dptask_comspec_t* comspec, bool ok, const char* result);
void dptask_comspec_del(dptask_comspec_t* comspec);
void dptask_comspec_del2(void* comspec);
]])

local C = ffi.C

local M = {}

local _id_str = ffi.new("char[12]", {})

--- 生成 Snowflake 风格字符串 ID（同 dpcwc dpid_next）。
function M.next_id()
    C.dpid_next(_id_str)
    return ffi.string(_id_str)
end

--- 字符串 ID → uint64。
function M.id_2u64(str)
    return tonumber(C.dpid_2u64(tostring(str)))
end

--- uint64 → 字符串 ID。
function M.id_2str(id)
    C.dpid_2str(ffi.cast("uint64_t", tonumber(id)), _id_str)
    return ffi.string(_id_str)
end

--- 创建任务规格（dptask_comspec_new）。
function M.comspec_new(info, body, args, reto_node, reto_name)
    return C.dptask_comspec_new(info, body, args,
        ffi.cast("int", tonumber(reto_node) or 0),
        ffi.cast("int", tonumber(reto_name) or 0))
end

--- 设置任务执行结果。
function M.comspec_set_result(comspec, ok, result)
    C.dptask_comspec_set_result(comspec, ffi.cast("bool", ok and true or false),
        result)
end

--- 释放任务规格字段（不 free 结构体本身）。
function M.comspec_del(comspec)
    C.dptask_comspec_del(comspec)
end

return M
