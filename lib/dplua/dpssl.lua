-- dpssl 模块：TLS 会话（对齐 lib/dpapp/dpssl.h）
local dpret = require("dpret")
local ffi = require("ffi")

ffi.cdef([[
bool dpssl_enable();

dpret_t dpssl_add(const char* group, dprole_e role, uint16_t min_version, uint16_t max_version);
dpret_t dpssl_del(const char* group);
dprole_e dpssl_role(const char* group);
dpret_t dpssl_add_alpn(const char* group, const char* alpn);
dpret_t dpssl_has_alpn(const char* group, const char* alpn);
dpret_t dpssl_has_version(const char* group, uint16_t min_version,
    uint16_t max_version);
const char* dpssl_get_alpn(const char* group, int idx);
dpret_t dpssl_add_ctx(const char* group, const char* sni, const char* crt, const char* key);
typedef struct ssl_ctx_st dpssl_ori_ctx_t;
dpssl_ori_ctx_t* dpssl_get_ctx(const char* group, const char* sni);
dpret_t dpssl_del_ctx(const char* group, const char* sni);
void dpssl_del_all_ctx(const char* group);

const dpele_type_t* dpssl_client_type();
const dpele_type_t* dpssl_server_type();
]])

local C = ffi.C
local ffi_cast = ffi.cast
local ffi_new = ffi.new
local tonumber = tonumber

local DPE_OK = dpret.OK
local DPE_WAIT = dpret.WAIT
local DPE_INVAL = dpret.INVAL

local M = {
    SSL3 = 0x0300,
    TLS1 = 0x0301,
    TLS1_1 = 0x0302,
    TLS1_2 = 0x0303,
    TLS1_3 = 0x0304,
    DTLS1 = 0xfeff,
    DTLS1_2 = 0xfefd,
    DTLS1_3 = 0xfefc,
    DTLS1_BAD = 0x00ff,
    ['SSLv3'] = 0x0300,
    ['TLSv1'] = 0x0301,
    ['TLSv1.1'] = 0x0302,
    ['TLSv1.2'] = 0x0303,
    ['TLSv1.3'] = 0x0304,
    ['DTLSv1'] = 0xfeff,
    ['DTLSv1.2'] = 0xfefd,
    ['DTLSv1.3'] = 0xfefc,
}

--- 是否启用 SSL。
function M.enable()
    local ssl_enable = C.dpssl_enable()
    return (ssl_enable == true or ssl_enable == 1)
end

if M.enable() then

    --- 注册 TLS 协议组。
    function M.add(group, role, min_version_, max_version_)
        role = ffi_cast("dprole_e", tonumber(role))
        local min_version = ffi_cast("uint16_t", tonumber(min_version_) or M.TLS1_3)
        local max_version = ffi_cast("uint16_t", tonumber(max_version_) or M.TLS1_3)
        return C.dpssl_add(tostring(group), role, min_version, max_version)
    end

    function M.add_ctx(group, sni, crt, key)
        local sni_p = sni == nil and nil or tostring(sni)
        return C.dpssl_add_ctx(tostring(group), sni_p, tostring(crt), tostring(key))
    end

    function M.add_alpn(group, alpn)
        return C.dpssl_add_alpn(tostring(group), tostring(alpn))
    end

    function M.has_alpn(group, alpn)
        return C.dpssl_has_alpn(tostring(group), tostring(alpn))
    end

    function M.has_version(group, min_version_, max_version_)
        local min_version = ffi_cast("uint16_t", tonumber(min_version_) or 0)
        local max_version = ffi_cast("uint16_t", tonumber(max_version_) or 0)
        return C.dpssl_has_version(tostring(group), min_version, max_version)
    end

    function M.get_alpn(group, idx_)
        local idx = ffi_cast("int", tonumber(idx_) or 0)
        local alpn = C.dpssl_get_alpn(tostring(group), idx)
        if alpn == nil then
            return nil
        end
        return ffi.string(alpn)
    end

    function M.del(group)
        return C.dpssl_del(tostring(group))
    end

    function M.role(group)
        return tonumber(C.dpssl_role(tostring(group)))
    end

    function M.get_ctx(group, sni_)
        local ctx = C.dpssl_get_ctx(tostring(group), sni_ and tostring(sni_) or nil)
        return ctx
    end

    function M.del_ctx(group, sni)
        local sni_p = sni == nil and nil or tostring(sni)
        return C.dpssl_del_ctx(tostring(group), sni_p)
    end

    function M.del_all_ctx(group)
        C.dpssl_del_all_ctx(tostring(group))
        return DPE_OK
    end

    M.server_type = C.dpssl_server_type()
    M.client_type = C.dpssl_client_type()

    --- `dpele_new(dpssl_server_type(), tcp_efd, group)`。
    function M.server(efd, group)
        local ssn = C.dpele_new(M.server_type, efd, tostring(group))
        if ssn == nil then
            return nil
        end
        return ssn
    end

    --- `dpele_new(dpssl_client_type(), tcp_efd, group, sni)`。
    function M.client(efd, group, sni)
        local ssn = C.dpele_new(M.client_type, efd, tostring(group), tostring(sni))
        if ssn == nil then
            return nil
        end
        return ssn
    end

end

return M
