#include "dpapp/dpbuf.h"
#include "dpapp/dplog.h"
#include "dpapp/dpret.h"
#include "dpapp/which.h"
#include "dpapp_config.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void read_all(const char* path, char* buf, size_t size)
{
    int fd = open(path, O_RDONLY);
    assert(fd >= 0);
    ssize_t n = read(fd, buf, size - 1);
    assert(n >= 0);
    buf[n] = '\0';
    close(fd);
}

static void test_dpbuf(void)
{
    dpbuf_t* b = dpbuf_new(128);
    assert(b != NULL);

    assert(dpbuf_wdata(b, "hello", 5) == 5);
    assert(dpbuf_crsize(b) == 0);
    assert(dpbuf_rall(b) == 5);
    assert(dpbuf_crsize(b) == 5);
    assert(memcmp(dpbuf_crdata(b), "hello", 5) == 0);
    assert(dpbuf_cequalc(b, "hello", 5));
    assert(dpbuf_cequalc(b, "hello", -1));

    dpbuf_reset(b, DPBUF_INIT_W);
    int n = dpbuf_wstrf(b, "%s-%d", "x", 42);
    assert(n > 0);
    assert(dpbuf_rall(b) == n);
    assert(dpbuf_cbegwith(b, "x-42", 4, false));

    dpbuf_del(b);

    dpbuf_t* dup_test = dpbuf_new_f("n=%d", 7);
    assert(dup_test != NULL);
    assert(dpbuf_cbegwith(dup_test, "n=7", 3, false));
    dpbuf_del(dup_test);

    char stack_msg[] = "dup";
    dpbuf_t* d = dpbuf_new_d(stack_msg, 3, DPBUF_DUP_DATA | DPBUF_INIT_R);
    assert(d != NULL);
    assert(dpbuf_crsize(d) == 3);
    assert(dpbuf_cequalc(d, "dup", 3));
    memset(stack_msg, 'X', sizeof(stack_msg));
    assert(dpbuf_cequalc(d, "dup", 3));
    dpbuf_del(d);

    dpbuf_t* x = dpbuf_new(32);
    assert(x != NULL);
    assert(dpbuf_wdata(x, "foo.bar.baz", 11) == 11);
    assert(dpbuf_rall(x) == 11);
    dpbuf_t* y = dpbuf_new(32);
    assert(y != NULL);
    assert(dpbuf_wdata(y, "foo.bar.baz", 11) == 11);
    assert(dpbuf_rall(y) == 11);
    assert(dpbuf_ccmp(x, y) == 0);
    assert(dpbuf_cfind(x, ".", 1, 0) == 3);
    assert(dpbuf_cfind(x, "zzz", 3, 0) == -EPERM);
    dpbuf_del(y);
    dpbuf_del(x);

    dpbuf_t* r = dpbuf_new(32);
    assert(r != NULL);
    assert(dpbuf_wdata(r, "012345", 6) == 6);
    assert(dpbuf_rdata(r, 3) == 3);
    assert(dpbuf_crsize(r) == 3);
    assert(memcmp(dpbuf_crdata(r), "012", 3) == 0);
    assert(dpbuf_rdata(r, 3) == 3);
    assert(dpbuf_crsize(r) == 3);
    assert(memcmp(dpbuf_crdata(r), "345", 3) == 0);

    dpbuf_reset(r, DPBUF_INIT_W);
    assert(dpbuf_wdata(r, "ab", 2) == 2);
    dpbuf_t* det = dpbuf_new(16);
    assert(det != NULL);
    assert(dpbuf_readto(r, det, -1) == 2);
    assert(dpbuf_cequalc(det, "ab", 2));
    dpbuf_del(det);
    dpbuf_del(r);

    dpbuf_t* base = dpbuf_new_f("ref");
    assert(base != NULL);
    assert(dpbuf_refc(base) == 1);
    dpbuf_t* view = dpbuf_new_r(base);
    assert(view != NULL);
    assert(dpbuf_refc(base) == 2);
    dpbuf_del(view);
    assert(dpbuf_refc(base) == 1);
    dpbuf_del(base);

    dpbuf_t* inv = dpbuf_new(8);
    assert(inv != NULL);
    assert(dpbuf_wdata(inv, "", 0) == -EINVAL);
    assert(dpbuf_wdata(inv, NULL, 1) == -EINVAL);
    dpbuf_del(inv);

    printf("  dpbuf: ok\n");
}

static void test_dpret(void)
{
    assert(dpret_isok(DPE_OK));
    assert(dpret_isok(1));
    assert(!dpret_isok(DPE_NOMEM));

    assert(dpret_iserr(DPE_INVAL));
    assert(!dpret_iserr(DPE_OK));

    assert(dpret_isok_eof(DPE_EOF));
    assert(!dpret_isok_eof(DPE_NOMEM));

    const char* sys = dperr_detail(DPE_NOMEM);
    assert(sys != NULL && strlen(sys) > 0);

    const char* custom = dperr_detail(DPE_UNKNOWN);
    assert(custom != NULL && strstr(custom, "Unknown") != NULL);

    const char* http1xx = dperr_http_detail(100);
    assert(http1xx != NULL && strstr(http1xx, "Continue") != NULL);

    const char* http = dperr_http_detail(404);
    assert(http != NULL && strstr(http, "Not Found") != NULL);

    const char* fallback = dperr_detail(-9999);
    assert(fallback != NULL && strstr(fallback, "Unknown") != NULL);

    printf("  dpret: ok\n");
}

static void test_dplog(void)
{
    assert(strcmp(dplog_lname(DPLOG_L_DEBUG), "debug") == 0);
    assert(strcmp(dplog_lname(DPLOG_L_ALERT), "alert") == 0);
    assert(strcmp(dplog_sname(DPLOG_L_NOTICE), "N") == 0);
    assert(dplog_namel("INFO") == DPLOG_L_INFO);
    assert(dplog_namel("warning") == DPLOG_L_WARN);
    assert(dplog_namel("unknown-level") == DPLOG_L_ALERT);

    dplog_setlevel((dplog_level_e)999);
    assert(dplog_curlevel() == DPLOG_L_ALERT);
    assert(strcmp(dplog_curlname(), "alert") == 0);

    char path[128];
    snprintf(path, sizeof(path), "/tmp/dpapp-test-dplog-%ld.log", (long)getpid());
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(fd >= 0);
    close(fd);

    assert(dplog_init(path, DPLOG_L_INFO, DPLOG_TA_SECOND));
    assert(dplog_curlevel() == DPLOG_L_INFO);
    dplog_debug("test", "debug should be filtered");
    dplog_notice("test", "notice line %d", 1);
    dplog_print("plain line");

    char content[4096];
    read_all(path, content, sizeof(content));
    assert(strstr(content, "notice line 1") != NULL);
    assert(strstr(content, "(test)") != NULL);
    assert(strstr(content, "plain line") != NULL);
    assert(strstr(content, "debug should be filtered") == NULL);

    unlink(path);

    printf("  dplog: ok\n");
}

static void test_which(void)
{
    assert(strcmp(normalize_path("/a/./b/../c"), "/a/c") == 0);
    assert(strcmp(normalize_path("/foo/../bar"), "/bar") == 0);
    assert(strcmp(normalize_path("/a//b///c"), "/a/b/c") == 0);
    assert(strcmp(normalize_path("/"), "/") == 0);
    assert(strcmp(normalize_path("a/../b"), "/b") == 0);

    const char* rel = absolute_path("./tst/../README.md");
    assert(rel != NULL);
    assert(rel[0] == '/');
    assert(strlen(rel) >= strlen("/README.md"));
    assert(strcmp(rel + strlen(rel) - strlen("/README.md"), "/README.md") == 0);

    const char* shell = find_executable("sh");
    assert(shell != NULL);
    assert(shell[0] == '/');
    assert(is_file(shell));

    assert(find_executable("dpapp_command_that_should_not_exist") == NULL);
    assert(is_dir("."));
    assert(is_dir("/tmp"));
    assert(!is_file("./tst"));
    assert(!is_dir(shell));

    printf("  which: ok\n");
}

static void test_config(void)
{
    assert((DPAPP_SSL_ENABLE == 0) || (DPAPP_SSL_ENABLE == 1));
    assert((DPAPP_LSQUIC_ENABLE == 0) || (DPAPP_LSQUIC_ENABLE == 1));
    assert((DPAPP_CWC_ENABLE == 0) || (DPAPP_CWC_ENABLE == 1));
    assert((DPAPP_CPP_ENABLE == 0) || (DPAPP_CPP_ENABLE == 1));
    assert((DPAPP_LUA_ENABLE == 0) || (DPAPP_LUA_ENABLE == 1));
    assert(DPAPP_HAS_SSL == DPAPP_SSL_ENABLE);
    assert(DPAPP_HAS_LSQUIC == DPAPP_LSQUIC_ENABLE);
    assert(DPAPP_HAS_CWC == DPAPP_CWC_ENABLE);
    assert(DPAPP_HAS_CPP == 0);
    assert(DPAPP_HAS_LUA == DPAPP_LUA_ENABLE);
    assert(DPAPP_VERSION_MAJOR >= 0);
    assert(DPAPP_VERSION_MINOR >= 0);
    printf("  config: ok\n");
}

int main(void)
{
    test_dpbuf();
    test_dpret();
    test_dplog();
    test_which();
    test_config();

    printf("test_basic ok\n");
    return 0;
}
