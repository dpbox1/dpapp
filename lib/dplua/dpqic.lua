-- dpqic 模块：QUIC / HTTP3（对齐 lib/dpapp/dpqic.h）
--
-- connect/accept/stream 经 dpqic_* + await；I/O 类同 dpssl。
-- argdoc 给出 prep 之后的 dpevp_add 可变参数。
--
local dpret = require("dpret")
local ffi = require("ffi")

ffi.cdef([[
bool dpqic_enable();

typedef struct lsquic_engine_settings dpqic_engine_settings_t;

dpret_t dpqic_add_engine(const char* group, dpqic_engine_settings_t* settings_);
dpret_t dpqic_del_engine(const char* group);

const dpele_type_t* dpqic_listen_type();
const dpele_type_t* dpqic_client_type();
const dpele_type_t* dpqic_conect_type();
const dpele_type_t* dpqic_stream_type();

typedef struct dpqic_hdrset dpqic_hdrset_t;
dpqic_hdrset_t* dpqic_hdrset_new(int pre_size);
const char* dpqic_hdrset_get(const dpqic_hdrset_t* hdrset, const char* name);
dpret_t dpqic_hdrset_set(dpqic_hdrset_t* hdrset, const char* name, const char* value);
void dpqic_hdrset_del(dpqic_hdrset_t* hdrset);
dpret_t dpqic_hdrset_count(const dpqic_hdrset_t* hdrset);
const char* dpqic_hdrset_at(const dpqic_hdrset_t* hdrset, int index, const char** name);
]])

local C = ffi.C
local ffi_cast = ffi.cast
local ffi_new = ffi.new
local tonumber = tonumber

local DPE_OK = dpret.OK
local DPE_INVAL = dpret.INVAL
local dpret_iserr = dpret.iserr

local M = {}

function M.enable()
    local qic_enable = C.dpqic_enable()
    return (qic_enable == true or qic_enable == 1)
end

if M.enable() then

    function M.add_engine(group, settings_)
        return C.dpqic_add_engine(tostring(group), settings_)
    end

    function M.del_engine(group)
        if group == nil then
            return C.dpqic_del_engine(nil)
        end
        return C.dpqic_del_engine(tostring(group))
    end

    M.listen_type = C.dpqic_listen_type()
    M.client_type = C.dpqic_client_type()
    M.conect_type = C.dpqic_conect_type()
    M.stream_type = C.dpqic_stream_type()

    function M.listen(group, host, port)
        if not group or not host or not port then
            return nil, DPE_INVAL
        end

        port = ffi_cast("int", tonumber(port))
        local lsn = C.dpele_new(M.listen_type, tostring(group), tostring(host), port)
        if lsn == nil then
            return nil, ffi.errno()
        end

        return lsn, DPE_OK
    end

    function M.client(host, port, group)
        if not host or not port or not group then
            return nil, DPE_INVAL
        end

        port = ffi_cast("int", tonumber(port))
        local cet = C.dpele_new(M.client_type, tostring(group), tostring(host), port)
        if cet == nil then
            return nil, ffi.errno()
        end

        return cet, DPE_OK
    end

    local H = {}

    function H.setgc(self, use_)
        return ffi.gc(self, use_ and C.dpqic_hdrset_del or nil)
    end

    function H.set(self, name, value)
        if name == nil then
            return DPE_INVAL
        end
        if value ~= nil then
            value = tostring(value)
        end
        return C.dpqic_hdrset_set(self, tostring(name), value)
    end

    function H.get(self, name)
        name = tostring(name)
        if name == nil then
            return nil
        end

        local value = C.dpqic_hdrset_get(self, name)
        if value == nil then
            return nil
        else
            return ffi.string(value)
        end
    end

    function H.del(self)
        C.dpqic_hdrset_del(self)
    end

    function H.count(self)
        return tonumber(C.dpqic_hdrset_count(self))
    end

    local pname = ffi_new("const char*[1]")
    function H.at(self, index)
        index = ffi_cast("int", tonumber(index))
        local value = C.dpqic_hdrset_at(self, index, pname)
        if value == nil then
            return nil, nil
        else
            return ffi.string(pname[0]), ffi.string(value)
        end
    end

    function H.todict(self)
        local dict = {}
        local count = H.count(self)
        for i = 0, count - 1 do
            local k, v = H.at(self, i)
            if k then
                dict[k] = v
            end
        end
        return dict, count
    end

    local HMT = {}

    function HMT.__tostring(self)
        local dict, count = H.todict(self)

        local items = {"{"}

        for key, value in pairs(dict) do
            count = count - 1
            local escaped_key = string.gsub(key, '["\\]', '\\%1')
            local escaped_value = string.gsub(value, '["\\]', '\\%1')
            items[#items + 1] = string.format('  "%s": "%s"%s', escaped_key,
                escaped_value, count > 0 and "," or "")
        end

        items[#items + 1] = "}"

        return table.concat(items, "\n")
    end

    function HMT.__pairs(self)
        local index = 0
        local count = H.count(self)

        return function()
            if index < count then
                local name, value = H.at(self, index)
                index = index + 1
                return name, value
            end
        end
    end

    function HMT.__ipairs(self)
        local index = 0
        local count = H.count(self)

        return function()
            if index < count then
                local name, value = H.at(self, index)
                index = index + 1
                return index, {
                    name = name,
                    value = value,
                }
            end
        end
    end

    function HMT.__newindex(self, key, value)
        return H.set(self, key, value)
    end

    function HMT.__index(self, key)
        local f = H[key]
        if f then
            return f
        else
            return H.get(self, key)
        end
    end

    local _ = ffi.metatype("dpqic_hdrset_t", HMT)

    function M.new_hdrset(pre_size)
        local pre_size = ffi_cast("int", tonumber(pre_size) or 0)
        local hdrset = C.dpqic_hdrset_new(pre_size)
        if hdrset == nil then
            return nil, ffi.errno()
        end
        return hdrset, DPE_OK
    end

    function M.new_req_hdrset(method, path, authority)
        local pre_size = ffi_cast("int", 4)
        local hdrset = C.dpqic_hdrset_new(pre_size)
        if hdrset == nil then
            return nil, ffi.errno()
        end

        local ret = C.dpqic_hdrset_set(hdrset, ":scheme", "https")
        if dpret_iserr(ret) then
            C.dpqic_hdrset_del(hdrset)
            return nil, ffi.errno()
        end

        if method then
            ret = H.set(hdrset, ":method", method)
            if dpret_iserr(ret) then
                C.dpqic_hdrset_del(hdrset)
                return nil, ret
            end
        end

        if path then
            ret = H.set(hdrset, ":path", path)
            if dpret_iserr(ret) then
                C.dpqic_hdrset_del(hdrset)
                return nil, ret
            end
        end

        if authority then
            ret = H.set(hdrset, ":authority", authority)
            if dpret_iserr(ret) then
                C.dpqic_hdrset_del(hdrset)
                return nil, ret
            end
        end
        return hdrset, DPE_OK
    end

    function M.new_res_hdrset(status)
        local pre_size = ffi_cast("int", 2)
        local hdrset = C.dpqic_hdrset_new(pre_size)
        if hdrset == nil then
            return nil, ffi.errno()
        end

        status = tonumber(status) or 200
        local ret = C.dpqic_hdrset_set(hdrset, ":status", tostring(status))
        if dpret_iserr(ret) then
            C.dpqic_hdrset_del(hdrset)
            return nil, ret
        end

        return hdrset, DPE_OK
    end
end

return M
