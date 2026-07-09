-- dpasc 模块：异步 syscall 绑定（对齐 lib/dpapp/dpasc.h）
--
-- 调用链：dpevp_add(ele, asc, ...) → 若 DPE_WAIT 则 dplua.await(ele)。
local ffi = require("ffi")

ffi.cdef([[
typedef struct dpele dpele_t;
typedef struct dpele dpefd_t;

typedef unsigned int socklen_t;
typedef uint16_t sa_family_t;

struct sockaddr_storage
{
    sa_family_t ss_family;
    char __ss_padding[118];
    uint64_t __ss_align;
};

typedef struct
{
    struct sockaddr_storage addr;
    socklen_t real;
} dpsockaddr_t;

/* 调度 */
const dpasc_t* dpctc_submit();
dpret_t dplua_add_ctc_submit(dpele_t* ctc, int64_t topic_id, dpv64_t arg);
const dpasc_t* dptmr_timeout();
const dpasc_t* dptmr_callback();
const dpasc_t* dpefd_poll();

/* GFD */
const dpasc_t* dpgfd_read();
const dpasc_t* dpgfd_write();
const dpasc_t* dpgfd_readv();
const dpasc_t* dpgfd_writev();
const dpasc_t* dpgfd_splice();
const dpasc_t* dpgfd_tee();

/* SKT */
const dpasc_t* dpskt_recv();
const dpasc_t* dpskt_send();
const dpasc_t* dpskt_recv2();
const dpasc_t* dpskt_send2();
const dpasc_t* dpskt_recvmsg();
const dpasc_t* dpskt_sendmsg();
const dpasc_t* dpskt_accept();
const dpasc_t* dpskt_connect();
const dpasc_t* dpskt_shutdown();

/* EFD */
const dpasc_t* dpefd_fsync();
const dpasc_t* dpefd_fallocate();
const dpasc_t* dpefd_fadvise();
const dpasc_t* dpefd_sync_file_range();
const dpasc_t* dpefd_close();

/* SYC */
const dpasc_t* dpsyc_madvise();
const dpasc_t* dpsyc_openat();
const dpasc_t* dpsyc_open();
const dpasc_t* dpsyc_openat2();
const dpasc_t* dpsyc_statx();
const dpasc_t* dpsyc_unlinkat();
const dpasc_t* dpsyc_unlink();
const dpasc_t* dpsyc_renameat();
const dpasc_t* dpsyc_rename();
const dpasc_t* dpsyc_mkdirat();
const dpasc_t* dpsyc_mkdir();
const dpasc_t* dpsyc_symlinkat();
const dpasc_t* dpsyc_symlink();
const dpasc_t* dpsyc_linkat();
const dpasc_t* dpsyc_link();
const dpasc_t* dpsyc_getxattr();
const dpasc_t* dpsyc_setxattr();
const dpasc_t* dpsyc_fgetxattr();
const dpasc_t* dpsyc_fsetxattr();
const dpasc_t* dpsyc_nop();

/* AIO */
const dpasc_t* dpaio_read_some();
const dpasc_t* dpaio_write_some();
const dpasc_t* dpaio_read_must();
const dpasc_t* dpaio_write_must();
const dpasc_t* dpaio_read_until();
const dpasc_t* dpaio_read_data();
const dpasc_t* dpaio_write_data();

/* SSL */
const dpasc_t* dpssl_handshake();
const dpasc_t* dpssl_shutdown();
const dpasc_t* dpssl_recv();
const dpasc_t* dpssl_send();

/* QIC */
const dpasc_t* dpqic_connect();
const dpasc_t* dpqic_stream();
const dpasc_t* dpqic_accept();
const dpasc_t* dpqic_recv();
const dpasc_t* dpqic_send();
const dpasc_t* dpqic_recvv();
const dpasc_t* dpqic_sendv();
const dpasc_t* dpqic_recv_hdrset();
const dpasc_t* dpqic_send_hdrset();
]])

local C = ffi.C
local ffi_new = ffi.new
local AEXEC = dplua.aexec

local M = {}

-- 调度
M.ctc_submit_type = C.dpctc_submit()
M.tmr_timeout_type = C.dptmr_timeout()
M.efd_poll_type = C.dpefd_poll()

-- GFD
M.gfd_read_type = C.dpgfd_read()
M.gfd_write_type = C.dpgfd_write()
M.gfd_readv_type = C.dpgfd_readv()
M.gfd_writev_type = C.dpgfd_writev()
M.gfd_splice_type = C.dpgfd_splice()
M.gfd_tee_type = C.dpgfd_tee()

-- SKT
M.skt_recv_type = C.dpskt_recv()
M.skt_send_type = C.dpskt_send()
M.skt_recv2_type = C.dpskt_recv2()
M.skt_send2_type = C.dpskt_send2()
M.skt_recvmsg_type = C.dpskt_recvmsg()
M.skt_sendmsg_type = C.dpskt_sendmsg()
M.skt_accept_type = C.dpskt_accept()
M.skt_connect_type = C.dpskt_connect()
M.skt_shutdown_type = C.dpskt_shutdown()

-- EFD
M.efd_fsync_type = C.dpefd_fsync()
M.efd_fallocate_type = C.dpefd_fallocate()
M.efd_fadvise_type = C.dpefd_fadvise()
M.efd_sync_file_range_type = C.dpefd_sync_file_range()
M.efd_close_type = C.dpefd_close()

-- SYC
M.syc_madvise_type = C.dpsyc_madvise()
M.syc_openat_type = C.dpsyc_openat()
M.syc_open_type = C.dpsyc_open()
M.syc_openat2_type = C.dpsyc_openat2()
M.syc_statx_type = C.dpsyc_statx()
M.syc_unlinkat_type = C.dpsyc_unlinkat()
M.syc_unlink_type = C.dpsyc_unlink()
M.syc_renameat_type = C.dpsyc_renameat()
M.syc_rename_type = C.dpsyc_rename()
M.syc_mkdirat_type = C.dpsyc_mkdirat()
M.syc_mkdir_type = C.dpsyc_mkdir()
M.syc_symlinkat_type = C.dpsyc_symlinkat()
M.syc_symlink_type = C.dpsyc_symlink()
M.syc_linkat_type = C.dpsyc_linkat()
M.syc_link_type = C.dpsyc_link()
M.syc_getxattr_type = C.dpsyc_getxattr()
M.syc_setxattr_type = C.dpsyc_setxattr()
M.syc_fgetxattr_type = C.dpsyc_fgetxattr()
M.syc_fsetxattr_type = C.dpsyc_fsetxattr()
M.syc_nop_type = C.dpsyc_nop()

-- AIO
M.aio_read_some_type = C.dpaio_read_some()
M.aio_write_some_type = C.dpaio_write_some()
M.aio_read_must_type = C.dpaio_read_must()
M.aio_write_must_type = C.dpaio_write_must()
M.aio_read_until_type = C.dpaio_read_until()
M.aio_read_data_type = C.dpaio_read_data()
M.aio_write_data_type = C.dpaio_write_data()

-- SSL
M.ssl_handshake_type = C.dpssl_handshake()
M.ssl_shutdown_type = C.dpssl_shutdown()
M.ssl_recv_type = C.dpssl_recv()
M.ssl_send_type = C.dpssl_send()

-- QIC
M.qic_connect_type = C.dpqic_connect()
M.qic_stream_type = C.dpqic_stream()
M.qic_accept_type = C.dpqic_accept()
M.qic_recv_type = C.dpqic_recv()
M.qic_send_type = C.dpqic_send()
M.qic_recvv_type = C.dpqic_recvv()
M.qic_sendv_type = C.dpqic_sendv()
M.qic_recv_hdrset_type = C.dpqic_recv_hdrset()
M.qic_send_hdrset_type = C.dpqic_send_hdrset()

local ffi_cast = ffi.cast
local dpret = require("dpret")
local dpele = require("dpele")
local dpbuf = require("dpbuf")
local DPE_INVAL = dpret.INVAL
local DPE_OK = dpret.OK
local dpv64_null = ffi_new("dpv64_t")
local dpbuf_istype = dpbuf.istype
local dpret_isok = dpret.isok
local SEEK_CUR = dpbuf.SEEK_CUR

--- 将 nil/数字/cdata/dpv64_t 打包为 dpv64_t（CTC req 用）。
local function _convert_spec(v)
    if v == nil then
        return nil, DPE_OK
    elseif ffi.istype("dpv64_t", v) then
        return v, DPE_OK
    elseif type(v) == 'number' then
        local spec = ffi_new("dpv64_t")
        spec.s64 = v
        return spec, DPE_OK
    elseif type(v) == 'cdata' then
        local spec = ffi_new("dpv64_t")
        spec.ptr = ffi_cast("void*", v)
        return spec, DPE_OK
    else
        return nil, DPE_INVAL
    end
end

--- 将 topic/req 打包为 wire id 与 dpv64 参数。
local function _ctc_args(topic_, req_)
    local topic_id = dplua.topic_id(topic_)
    if topic_id == nil then
        return nil, DPE_INVAL
    end

    local spec, err = _convert_spec(req_)
    if dpret.iserr(err) then
        return nil, err
    end

    return topic_id, spec or dpv64_null
end

local function _ctc_aexec(ctc, topic_id, v1)
    local ret = C.dplua_add_ctc_submit(ctc, topic_id, v1)
    if ret == dpret.WAIT then
        ret = dplua.await(ctc)
    end
    return ret
end

--- alloc `dpsockaddr_t`。
function M.sockaddr()
    return ffi_new("dpsockaddr_t")
end

--- 转换参数为 int；失败返回 nil。
local function _as_int(v, def)
    v = tonumber(v)
    if v ~= nil then
        return ffi_cast("int", v)
    end
    return def and ffi_cast("int", def) or nil
end

--- 转换参数为 uint64_t；失败返回 nil。
local function _as_u64(v)
    v = tonumber(v)
    return v and ffi_cast("uint64_t", tonumber(v)) or nil
end

--- 转换参数为 long；失败返回 nil。
local function _as_long(v)
    v = tonumber(v)
    return v and ffi_cast("long", v) or nil
end

--- 转换数据指针（cdata 或 string → char*）。
local function _as_cptr(data)
    local t = type(data)
    if t == 'cdata' then
        return ffi_cast("char*", data)
    end
    if t == 'string' then
        return ffi_cast("const char*", data)
    end
    return nil
end

--- 发送区：string/cdata；len_ 省略时取 #string。
local function _as_send_bin(buf, len_)
    if type(buf) == 'string' then
        len_ = _as_int(len_, #buf)
        if not len_ or len_ <= 0 then
            return nil, nil
        end
        return ffi_cast("const void*", buf), len_
    end
    if type(buf) == 'cdata' then
        local ptr = ffi_cast("const void*", buf)
        len_ = _as_int(len_)
        if not ptr or not len_ or len_ <= 0 then
            return nil, nil
        end
        return ptr, len_
    end
    return nil, nil
end

--- 接收区：cdata；len_ 必填。
local function _as_recv_bin(buf, len_)
    if type(buf) == 'cdata' then
        local ptr = ffi_cast("void*", buf)
        len_ = _as_int(len_)
        if not ptr or not len_ or len_ <= 0 then
            return nil, nil
        end
        return ptr, len_
    end
    return nil, nil
end

--- dpbuf/cdata recv → AEXEC；dpbuf 成功时推进 wseek。
local function _aexec_recv(ele, asc_type, buf, len_, ...)
    if dpbuf_istype(buf) then
        len_ = _as_int(len_, buf:cwsize())
        if not len_ or len_ <= 0 then
            return DPE_INVAL
        end
        local ret = AEXEC(ele, asc_type, ffi_cast("void*", buf:cwdata()), len_, ...)
        if ret > 0 and dpret_isok(ret) then
            buf:wseek(ret, SEEK_CUR)
        end
        return ret
    end
    local cb, len = _as_recv_bin(buf, len_)
    if not cb then
        return DPE_INVAL
    end
    return AEXEC(ele, asc_type, cb, len, ...)
end

--- dpbuf/string/cdata send → AEXEC；dpbuf 成功时推进 rseek。
local function _aexec_send(ele, asc_type, buf, len_, ...)
    if dpbuf_istype(buf) then
        len_ = _as_int(len_, buf:crsize())
        if not len_ or len_ <= 0 then
            return DPE_INVAL
        end
        local ret = AEXEC(ele, asc_type, ffi_cast("const void*", buf:crdata()), len_,
            ...)
        if ret > 0 and dpret_isok(ret) then
            buf:rseek(ret, SEEK_CUR)
        end
        return ret
    end
    local cb, len = _as_send_bin(buf, len_)
    if not cb then
        return DPE_INVAL
    end
    return AEXEC(ele, asc_type, cb, len, ...)
end

--- xattr 等中间参数场景：仅准备 recv 区。
local function _prep_recv(buf, len_)
    if dpbuf_istype(buf) then
        len_ = _as_int(len_, buf:cwsize())
        if not len_ or len_ <= 0 then
            return nil, nil, nil
        end
        return ffi_cast("void*", buf:cwdata()), len_, buf
    end
    local cb, len = _as_recv_bin(buf, len_)
    if not cb then
        return nil, nil, nil
    end
    return cb, len, nil
end

local function _finish_recv(ret, buf_obj)
    if buf_obj and ret > 0 and dpret_isok(ret) then
        buf_obj:wseek(ret, SEEK_CUR)
    end
    return ret
end

--- xattr 等中间参数场景：仅准备 send 区。
local function _prep_send(buf, len_)
    if dpbuf_istype(buf) then
        len_ = _as_int(len_, buf:crsize())
        if not len_ or len_ <= 0 then
            return nil, nil, nil
        end
        return ffi_cast("const void*", buf:crdata()), len_, buf
    end
    local cb, len = _as_send_bin(buf, len_)
    if not cb then
        return nil, nil, nil
    end
    return cb, len, nil
end

local function _finish_send(ret, buf_obj)
    if buf_obj and ret > 0 and dpret_isok(ret) then
        buf_obj:rseek(ret, SEEK_CUR)
    end
    return ret
end

local function _aexec_xattr_recv(ele, asc_type, name, value, path, size_)
    if not name or not path then
        return DPE_INVAL
    end
    local cv, sz, b = _prep_recv(value, size_)
    if not cv then
        return DPE_INVAL
    end
    return _finish_recv(AEXEC(ele, asc_type, tostring(name), cv, tostring(path), sz),
        b)
end

local function _aexec_xattr_send(ele, asc_type, name, value, path, flags_, size_)
    flags_ = _as_int(flags_, 0)
    if not name or not path then
        return DPE_INVAL
    end
    local cv, sz, b = _prep_send(value, size_)
    if not cv then
        return DPE_INVAL
    end
    return _finish_send(AEXEC(ele, asc_type, tostring(name), cv, tostring(path),
        flags_, sz), b)
end

local function _aexec_fxattr_recv(ele, asc_type, fd, name, value, size_)
    fd = _as_int(fd)
    if not fd or not name then
        return DPE_INVAL
    end
    local cv, sz, b = _prep_recv(value, size_)
    if not cv then
        return DPE_INVAL
    end
    return _finish_recv(AEXEC(ele, asc_type, fd, tostring(name), cv, sz), b)
end

local function _aexec_fxattr_send(ele, asc_type, fd, name, value, flags_, size_)
    fd = _as_int(fd)
    if not fd or not name then
        return DPE_INVAL
    end
    flags_ = _as_int(flags_, 0)
    local cv, sz, b = _prep_send(value, size_)
    if not cv then
        return DPE_INVAL
    end
    return _finish_send(AEXEC(ele, asc_type, fd, tostring(name), cv, flags_, sz), b)
end

-- ============================== 调度 ==============================

--- CTC 派发（`topic` handler 名，`req` 附带参数）。
function M.ctc_submit(ctc, topic, req)
    local topic_id, v1 = _ctc_args(topic, req)
    if topic_id == nil then
        return v1
    end
    return _ctc_aexec(ctc, topic_id, v1)
end

function M.ctc_once(toid, topic, ro_req)
    toid = _as_int(toid)
    if not toid then
        return DPE_INVAL
    end

    local topic_id, v1 = _ctc_args(topic, ro_req)
    if topic_id == nil then
        return v1
    end

    local ctc = dpele.new_ctc(toid, false)
    if ctc == nil then
        return ffi.errno()
    end
    local ret = _ctc_aexec(ctc, topic_id, v1)
    C.dpele_del(ctc)
    return ret
end

--- 派发 CTC 不等待。
function M.ctc_detach(toid, topic, ro_req)
    toid = _as_int(toid)
    if not toid then
        return DPE_INVAL
    end

    local topic_id, v1 = _ctc_args(topic, ro_req)
    if topic_id == nil then
        return v1
    end

    local ctc = dpele.new_ctc(toid, true)
    if ctc == nil then
        return ffi.errno()
    end
    local ret = C.dplua_add_ctc_submit(ctc, topic_id, v1)
    C.dpele_del(ctc)
    return ret
end

--- 向同 type 全部 worker 广播 CTC。
function M.ctc_each(totype, topic, ro_req)
    local n = tonumber(totype)
    if n == nil then
        n = -1
    end
    totype = math.abs(n)
    local toids = dplua.each_ids[totype]
    if not toids then
        return DPE_INVAL
    end

    local topic_id, v1 = _ctc_args(topic, ro_req)
    if topic_id == nil then
        return v1
    end

    -- 阶段 1：并行派发到所有 worker
    local ctcs = {}
    local err = 0
    for _, toid in ipairs(toids) do
        local toid_ = _as_int(toid)
        if not toid_ then
            err = DPE_INVAL
            break
        end
        local ctc = dpele.new_ctc(toid_, false)
        if ctc == nil then
            err = ffi.errno()
            break
        end
        err = C.dplua_add_ctc_submit(ctc, topic_id, v1)
        if err == dpret.WAIT then
            ctcs[#ctcs + 1] = ctc
        else
            C.dpele_del(ctc)
            break
        end
    end

    -- 阶段 2：依次等待所有结果，释放元素
    for _, ctc in ipairs(ctcs) do
        err = dplua.await(ctc)
        C.dpele_del(ctc)
    end

    return err
end

-- ============================== GFD ==============================

--- dpgfd_read: (dpbuf_t* buf, int len_) 或 (void* buf, int len_)
function M.gfd_read(ele, buf, len_)
    return _aexec_recv(ele, M.gfd_read_type, buf, len_)
end

--- dpgfd_write: (dpbuf_t* buf, int len_) 或 (const void* buf, int len_)
function M.gfd_write(ele, buf, len_)
    return _aexec_send(ele, M.gfd_write_type, buf, len_)
end

--- dpgfd_readv: (const struct iovec* iov, int iovcnt)
function M.gfd_readv(ele, iov, iovcnt)
    if not (iov and iovcnt and type(iov) == 'cdata') then
        return DPE_INVAL
    end
    iov = ffi_cast("const struct iovec*", iov)
    iovcnt = _as_int(iovcnt)
    if not iovcnt then
        return DPE_INVAL
    end
    return AEXEC(ele, M.gfd_readv_type, iov, iovcnt)
end

--- dpgfd_writev: (const struct iovec* iov, int iovcnt)
function M.gfd_writev(ele, iov, iovcnt)
    if not (iov and iovcnt and type(iov) == 'cdata') then
        return DPE_INVAL
    end
    iov = ffi_cast("const struct iovec*", iov)
    iovcnt = _as_int(iovcnt)
    if not iovcnt then
        return DPE_INVAL
    end
    return AEXEC(ele, M.gfd_writev_type, iov, iovcnt)
end

--- dpgfd_splice: (int in_fd, int len, int flags)
function M.gfd_splice(ele, in_fd, len, flags_)
    in_fd = _as_int(in_fd)
    len = _as_int(len)
    flags_ = _as_int(flags_, 0)
    if not in_fd or not len then
        return DPE_INVAL
    end
    return AEXEC(ele, M.gfd_splice_type, in_fd, len, flags_)
end

--- dpgfd_tee: (int in_fd, unsigned len, unsigned flags)
function M.gfd_tee(ele, in_fd, len, flags_)
    in_fd = _as_int(in_fd)
    len = _as_int(len)
    flags_ = _as_int(flags_, 0)
    if not in_fd or not len then
        return DPE_INVAL
    end
    return AEXEC(ele, M.gfd_tee_type, in_fd, len, flags_)
end

-- ============================== SKT ==============================

--- dpskt_recv: (buf, len_, flags_)
function M.skt_recv(ele, buf, len_, flags_)
    return _aexec_recv(ele, M.skt_recv_type, buf, len_, _as_int(flags_, 0))
end

--- dpskt_send: (buf, len_, flags_)
function M.skt_send(ele, buf, len_, flags_)
    return _aexec_send(ele, M.skt_send_type, buf, len_, _as_int(flags_, 0))
end

--- dpskt_recv2: (buf, len_)
function M.skt_recv2(ele, buf, len_)
    return _aexec_recv(ele, M.skt_recv2_type, buf, len_)
end

--- dpskt_send2: (buf, len_)
function M.skt_send2(ele, buf, len_)
    return _aexec_send(ele, M.skt_send2_type, buf, len_)
end

--- dpskt_recvmsg: (struct msghdr* msg, int flags)
function M.skt_recvmsg(ele, msg, flags_)
    if not (msg and type(msg) == 'cdata') then
        return DPE_INVAL
    end
    flags_ = _as_int(flags_, 0)
    return AEXEC(ele, M.skt_recvmsg_type, msg, flags_)
end

--- dpskt_sendmsg: (struct msghdr* msg, int flags)
function M.skt_sendmsg(ele, msg, flags_)
    if not (msg and type(msg) == 'cdata') then
        return DPE_INVAL
    end
    flags_ = _as_int(flags_, 0)
    return AEXEC(ele, M.skt_sendmsg_type, msg, flags_)
end

--- dpskt_accept: (dpsockaddr_t* addr)
function M.skt_accept(ele, addr_)
    if addr_ and type(addr_) ~= 'cdata' then
        return DPE_INVAL
    end
    addr_ = addr_ or M.sockaddr()
    return AEXEC(ele, M.skt_accept_type, addr_)
end

--- dpskt_connect: (const dpsockaddr_t* addr)
--- 省略 addr 时从元素 aux_data 取 sockaddr（类型私有区，非 asc_data）。
function M.skt_connect(ele, addr_)
    addr_ = addr_ or ele:aux_data()
    if not addr_ or type(addr_) ~= 'cdata' then
        return DPE_INVAL
    end
    return AEXEC(ele, M.skt_connect_type, addr_)
end

--- dpskt_shutdown: (int how)
function M.skt_shutdown(ele, how)
    how = _as_int(how)
    if not how then
        return DPE_INVAL
    end
    return AEXEC(ele, M.skt_shutdown_type, how)
end

-- ============================== EFD ==============================

--- dpefd_poll: (int evs) — 等待 `DPEVT_*` 就绪，返回就绪掩码
function M.efd_poll(ele, evs)
    evs = _as_int(evs)
    if not evs then
        return DPE_INVAL
    end
    return AEXEC(ele, M.efd_poll_type, evs)
end

--- dpefd_fsync: (unsigned flags) -- bit0 表示 fdatasync
function M.efd_fsync(ele, flags_)
    flags_ = _as_int(flags_, 0)
    return AEXEC(ele, M.efd_fsync_type, flags_)
end

--- dpefd_fallocate: (int mode, uint64_t offset, uint64_t len)
function M.efd_fallocate(ele, mode, offset, len)
    mode = _as_int(mode)
    offset = _as_u64(offset)
    len = _as_u64(len)
    if not mode or not offset or not len then
        return DPE_INVAL
    end
    return AEXEC(ele, M.efd_fallocate_type, mode, offset, len)
end

--- dpefd_fadvise: (uint64_t offset, long len, int advice)
function M.efd_fadvise(ele, offset, len, advice)
    offset = _as_u64(offset)
    len = _as_long(len)
    advice = _as_int(advice)
    if not offset or not len or not advice then
        return DPE_INVAL
    end
    return AEXEC(ele, M.efd_fadvise_type, offset, len, advice)
end

--- dpefd_sync_file_range: (uint64_t offset, unsigned nbytes, int flags)
function M.efd_sync_file_range(ele, offset, nbytes, flags_)
    offset = _as_u64(offset)
    nbytes = _as_int(nbytes)
    flags_ = _as_int(flags_, 0)
    if not offset or not nbytes then
        return DPE_INVAL
    end
    return AEXEC(ele, M.efd_sync_file_range_type, offset, nbytes, flags_)
end

--- dpefd_close: 无额外参数
function M.efd_close(ele)
    return AEXEC(ele, M.efd_close_type)
end

-- ============================== SYC ==============================

--- dpsyc_madvise: (void* addr, long len, int advice)
function M.syc_madvise(ele, addr, len, advice)
    if not (addr and type(addr) == 'cdata') then
        return DPE_INVAL
    end
    len = _as_long(len)
    advice = _as_int(advice)
    if not len or not advice then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_madvise_type, addr, len, advice)
end

--- dpsyc_openat: (int dirfd, const char* path, int flags, mode_t mode)
function M.syc_openat(ele, dirfd, path, flags, mode_)
    dirfd = _as_int(dirfd)
    flags = _as_int(flags)
    mode_ = _as_int(mode_, 0)
    if not dirfd or not path or not flags then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_openat_type, dirfd, tostring(path), flags, mode_)
end

--- dpsyc_open: (const char* path, int flags, mode_t mode)
function M.syc_open(ele, path, flags, mode_)
    flags = _as_int(flags)
    mode_ = _as_int(mode_, 0)
    if not path or not flags then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_open_type, tostring(path), flags, mode_)
end

--- dpsyc_openat2: (int dirfd, const char* path, struct open_how* how)
function M.syc_openat2(ele, dirfd, path, how)
    dirfd = _as_int(dirfd)
    if not dirfd or not path or not (how and type(how) == 'cdata') then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_openat2_type, dirfd, tostring(path), how)
end

--- dpsyc_statx: (int dirfd, const char* path, int flags, unsigned mask, struct statx* buf)
function M.syc_statx(ele, dirfd, path, flags, mask, buf)
    dirfd = _as_int(dirfd)
    flags = _as_int(flags)
    mask = _as_int(mask)
    if not dirfd or not path or not flags or not (buf and type(buf) == 'cdata') then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_statx_type, dirfd, tostring(path), flags, mask, buf)
end

--- dpsyc_unlinkat: (int dirfd, const char* path, int flags)
function M.syc_unlinkat(ele, dirfd, path, flags_)
    dirfd = _as_int(dirfd)
    flags_ = _as_int(flags_, 0)
    if not dirfd or not path then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_unlinkat_type, dirfd, tostring(path), flags_)
end

--- dpsyc_unlink: (const char* path, int flags)
function M.syc_unlink(ele, path, flags_)
    flags_ = _as_int(flags_, 0)
    if not path then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_unlink_type, tostring(path), flags_)
end

--- dpsyc_renameat: (int olddirfd, const char* oldpath, int newdirfd, const char* newpath, unsigned flags)
function M.syc_renameat(ele, olddirfd, oldpath, newdirfd, newpath, flags_)
    olddirfd = _as_int(olddirfd)
    newdirfd = _as_int(newdirfd)
    flags_ = _as_int(flags_, 0)
    if not olddirfd or not oldpath or not newdirfd or not newpath then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_renameat_type, olddirfd, tostring(oldpath), newdirfd,
        tostring(newpath), flags_)
end

--- dpsyc_rename: (const char* oldpath, const char* newpath)
function M.syc_rename(ele, oldpath, newpath)
    if not oldpath or not newpath then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_rename_type, tostring(oldpath), tostring(newpath))
end

--- dpsyc_mkdirat: (int dirfd, const char* path, mode_t mode)
function M.syc_mkdirat(ele, dirfd, path, mode_)
    dirfd = _as_int(dirfd)
    mode_ = _as_int(mode_, 0)
    if not dirfd or not path then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_mkdirat_type, dirfd, tostring(path), mode_)
end

--- dpsyc_mkdir: (const char* path, mode_t mode)
function M.syc_mkdir(ele, path, mode_)
    mode_ = _as_int(mode_, 0)
    if not path then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_mkdir_type, tostring(path), mode_)
end

--- dpsyc_symlinkat: (const char* target, int dirfd, const char* linkpath)
function M.syc_symlinkat(ele, target, dirfd, linkpath)
    dirfd = _as_int(dirfd)
    if not target or not dirfd or not linkpath then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_symlinkat_type, tostring(target), dirfd,
        tostring(linkpath))
end

--- dpsyc_symlink: (const char* target, const char* linkpath)
function M.syc_symlink(ele, target, linkpath)
    if not target or not linkpath then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_symlink_type, tostring(target), tostring(linkpath))
end

--- dpsyc_linkat: (int olddirfd, const char* oldpath, int newdirfd, const char* newpath, int flags)
function M.syc_linkat(ele, olddirfd, oldpath, newdirfd, newpath, flags)
    olddirfd = _as_int(olddirfd)
    newdirfd = _as_int(newdirfd)
    flags = _as_int(flags)
    if not olddirfd or not oldpath or not newdirfd or not newpath then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_linkat_type, olddirfd, tostring(oldpath), newdirfd,
        tostring(newpath), flags)
end

--- dpsyc_link: (const char* oldpath, const char* newpath, int flags)
function M.syc_link(ele, oldpath, newpath, flags_)
    flags_ = _as_int(flags_, 0)
    if not oldpath or not newpath then
        return DPE_INVAL
    end
    return AEXEC(ele, M.syc_link_type, tostring(oldpath), tostring(newpath), flags_)
end

--- dpsyc_getxattr: (const char* name, value, const char* path, size_t size_)
function M.syc_getxattr(ele, name, value, path, size_)
    return _aexec_xattr_recv(ele, M.syc_getxattr_type, name, value, path, size_)
end

--- dpsyc_setxattr: (const char* name, value, const char* path, int flags, size_t size_)
function M.syc_setxattr(ele, name, value, path, flags_, size_)
    return _aexec_xattr_send(ele, M.syc_setxattr_type, name, value, path, flags_,
        size_)
end

--- dpsyc_fgetxattr: (int fd, const char* name, value, size_t size_)
function M.syc_fgetxattr(ele, fd, name, value, size_)
    return _aexec_fxattr_recv(ele, M.syc_fgetxattr_type, fd, name, value, size_)
end

--- dpsyc_fsetxattr: (int fd, const char* name, value, int flags, size_t size_)
function M.syc_fsetxattr(ele, fd, name, value, flags_, size_)
    return _aexec_fxattr_send(ele, M.syc_fsetxattr_type, fd, name, value, flags_,
        size_)
end

--- dpsyc_nop: 无额外参数
function M.syc_nop(ele)
    return AEXEC(ele, M.syc_nop_type)
end

-- ============================== AIO ==============================

--- dpaio_read_some: (dpbuf_t* buf, int max_len)
function M.aio_read_some(ele, buf, max_len_)
    if not dpbuf_istype(buf) then
        return DPE_INVAL
    end
    max_len_ = _as_int(max_len_, dpbuf.X_SIZE)
    if max_len_ <= 0 then
        return DPE_INVAL
    end
    return AEXEC(ele, M.aio_read_some_type, buf, max_len_)
end

--- dpaio_write_some: (dpbuf_t* buf, int min_len)
function M.aio_write_some(ele, buf, min_len_)
    if not dpbuf_istype(buf) then
        return DPE_INVAL
    end
    min_len_ = _as_int(min_len_, 1)
    if min_len_ <= 0 then
        return DPE_INVAL
    end
    return AEXEC(ele, M.aio_write_some_type, buf, min_len_)
end

--- dpaio_read_must: (dpbuf_t* buf, int len) 或 (void* buf, int len)
function M.aio_read_must(ele, buf, len_)
    if dpbuf_istype(buf) then
        len_ = _as_int(len_, buf:cwsize())
        if len_ <= 0 then
            return DPE_INVAL
        end
        return AEXEC(ele, M.aio_read_must_type, buf, len_)
    end
    return _aexec_recv(ele, M.aio_read_data_type, buf, len_)
end

--- dpaio_write_must: (dpbuf_t* buf, int len) 或 (const void* buf, int len)
function M.aio_write_must(ele, buf, len_)
    if dpbuf_istype(buf) then
        len_ = _as_int(len_, buf:crsize())
        if len_ <= 0 then
            return len_
        end
        return AEXEC(ele, M.aio_write_must_type, buf, len_)
    end
    return _aexec_send(ele, M.aio_write_data_type, buf, len_)
end

--- dpaio_read_until: (const char* until, dpbuf_t* buf, int max_len)
function M.aio_read_until(ele, buf, until_s, max_len_)
    if not until_s or not dpbuf_istype(buf) then
        return DPE_INVAL
    end
    until_s = tostring(until_s)
    local until_len = _as_int(#until_s, 0)
    max_len_ = _as_int(max_len_, dpbuf.X_SIZE)
    if max_len_ <= 0 or until_len <= 0 then
        return DPE_INVAL
    end
    return AEXEC(ele, M.aio_read_until_type, until_s, until_len, buf, max_len_)
end

-- ============================== SSL ==============================

--- dpssl_handshake: 无额外参数
function M.ssl_handshake(ele)
    return AEXEC(ele, M.ssl_handshake_type)
end

--- dpssl_shutdown: 无额外参数
function M.ssl_shutdown(ele)
    return AEXEC(ele, M.ssl_shutdown_type)
end

--- dpssl_recv: (buf, int len_)
function M.ssl_recv(ele, buf, len_)
    return _aexec_recv(ele, M.ssl_recv_type, buf, len_)
end

--- dpssl_send: (buf, int len_)
function M.ssl_send(ele, buf, len_)
    return _aexec_send(ele, M.ssl_send_type, buf, len_)
end

-- ============================== QIC ==============================

--- dpqic_connect: (const char* sni, const char* token, dpele_t** conn_out)
function M.qic_connect(ele, sni, token)
    if not sni then
        return DPE_INVAL
    end
    local conn_ptr = ffi_new("dpele_t*[1]")
    local ret = AEXEC(ele, M.qic_connect_type, tostring(sni),
        token and tostring(token) or nil, conn_ptr)
    if dpret.isok(ret) then
        return conn_ptr[0]
    end
    return nil, ret
end

--- dpqic_accept: (dpele_t** conn_out)
function M.qic_accept(ele)
    local conn_ptr = ffi_new("dpele_t*[1]")
    local ret = AEXEC(ele, M.qic_accept_type, conn_ptr)
    if dpret.isok(ret) then
        return conn_ptr[0]
    end
    return nil, ret
end

--- dpqic_stream: (dpele_t** stm_out, bool create_new)
function M.qic_stream(ele, create_new)
    local stm_ptr = ffi_new("dpele_t*[1]")
    create_new = ffi_cast("bool", create_new == true)
    local ret = AEXEC(ele, M.qic_stream_type, stm_ptr, create_new)
    if dpret.isok(ret) then
        return stm_ptr[0]
    end
    return nil, ret
end

--- dpqic_recv: (buf, int len_)
function M.qic_recv(ele, buf, len_)
    return _aexec_recv(ele, M.qic_recv_type, buf, len_)
end

--- dpqic_send: (buf, int len_) — buf 为 dpbuf/string/cdata，len_ 可省略
function M.qic_send(ele, buf, len_)
    return _aexec_send(ele, M.qic_send_type, buf, len_)
end

--- dpqic_recvv: (struct iovec* iov, int iovcnt)
function M.qic_recvv(ele, iov, iovcnt)
    if not (iov and iovcnt and type(iov) == 'cdata') then
        return DPE_INVAL
    end
    iov = ffi_cast("struct iovec*", iov)
    iovcnt = _as_int(iovcnt)
    if not iovcnt then
        return DPE_INVAL
    end
    return AEXEC(ele, M.qic_recvv_type, iov, iovcnt)
end

--- dpqic_sendv: (struct iovec* iov, int iovcnt)
function M.qic_sendv(ele, iov, iovcnt)
    if not (iov and iovcnt and type(iov) == 'cdata') then
        return DPE_INVAL
    end
    iov = ffi_cast("struct iovec*", iov)
    iovcnt = _as_int(iovcnt)
    if not iovcnt then
        return DPE_INVAL
    end
    return AEXEC(ele, M.qic_sendv_type, iov, iovcnt)
end

--- dpqic_recv_hdrset: (dpqic_hdrset_t** hdrset_out)
function M.qic_recv_hdrset(ele)
    local hdrset_ptr = ffi_new("dpqic_hdrset_t*[1]")
    local ret = AEXEC(ele, M.qic_recv_hdrset_type, hdrset_ptr)
    if dpret.isok(ret) then
        return hdrset_ptr[0]
    end
    return nil, ret
end

--- dpqic_send_hdrset: (const dpqic_hdrset_t* hdrset)
function M.qic_send_hdrset(ele, hdrset)
    if not (hdrset and type(hdrset) == 'cdata') then
        return DPE_INVAL
    end
    return AEXEC(ele, M.qic_send_hdrset_type, hdrset)
end

return M
