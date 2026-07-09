#include "dpapp/dpefd.h"
#include "dpapp/os/dpevp_pri.h"
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <unistd.h>

bool _dpefd_feed(dpele_t* self, uint32_t evs);

#ifndef IN_ACCESS
#define IN_ACCESS        0x00000001
#define IN_MODIFY        0x00000002
#define IN_ATTRIB        0x00000004
#define IN_CLOSE_WRITE   0x00000008
#define IN_CLOSE_NOWRITE 0x00000010
#define IN_OPEN          0x00000020
#define IN_MOVED_FROM    0x00000040
#define IN_MOVED_TO      0x00000080
#define IN_CREATE        0x00000100
#define IN_DELETE        0x00000200
#define IN_DELETE_SELF   0x00000400
#define IN_MOVE_SELF     0x00000800
#define IN_UNMOUNT       0x00002000
#endif

#define DPFSM_MAX_WATCH 64

typedef struct
{
    dpele_t* ele;
    int wd;
    int vnode_fd;
    char name[NAME_MAX + 1];
    uint32_t inotify_mask;
} dpfsm_watch_t;

typedef struct
{
    int pipe_w;
    int next_wd;
    int watch_cnt;
    dpfsm_watch_t watches[DPFSM_MAX_WATCH];
} dpfsm_iop_t;

static uint32_t _inotify_to_note(uint32_t mask)
{
    uint32_t note = 0;
    if (mask & IN_DELETE) {
        note |= NOTE_DELETE;
    }
    if (mask & (IN_MODIFY | IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO)) {
        note |= NOTE_WRITE;
    }
    if (mask & IN_ATTRIB) {
        note |= NOTE_ATTRIB;
    }
    if (mask & IN_OPEN) {
        note |= NOTE_OPEN;
    }
    if (mask & (IN_MOVED_FROM | IN_MOVED_TO | IN_MOVE_SELF)) {
        note |= NOTE_RENAME;
    }
    if (mask & IN_ACCESS) {
        note |= NOTE_ACCESS;
    }
    if (note == 0) {
        note = NOTE_WRITE | NOTE_DELETE | NOTE_ATTRIB;
    }
    return note;
}

static uint32_t _note_to_inotify(uint32_t note)
{
    uint32_t mask = 0;
    if (note & NOTE_DELETE) {
        mask |= IN_DELETE | IN_DELETE_SELF;
    }
    if (note & NOTE_WRITE) {
        mask |= IN_MODIFY;
    }
    if (note & NOTE_EXTEND) {
        mask |= IN_MODIFY;
    }
    if (note & NOTE_ATTRIB) {
        mask |= IN_ATTRIB;
    }
    if (note & NOTE_LINK) {
        mask |= IN_CREATE;
    }
    if (note & NOTE_RENAME) {
        mask |= IN_MOVED_FROM | IN_MOVED_TO;
    }
    if (note & NOTE_REVOKE) {
        mask |= IN_UNMOUNT;
    }
    if (mask == 0) {
        mask = IN_MODIFY;
    }
    return mask;
}

dpret_t dpfsm_vopen(void* udata, va_list varg)
{
    (void)varg;
    dpfsm_iop_t* fsm = (dpfsm_iop_t*)udata;
    fsm->pipe_w = -1;
    fsm->next_wd = 1;
    fsm->watch_cnt = 0;

    int fds[2];
    if (pipe(fds) != 0) {
        return -errno;
    }
    fsm->pipe_w = fds[1];
    return fds[0];
}

static void _dpfsm_fini(void* udata)
{
    dpfsm_iop_t* fsm = (dpfsm_iop_t*)udata;
    for (int i = 0; i < fsm->watch_cnt; i++) {
        dpfsm_watch_t* w = &fsm->watches[i];
        if (w->vnode_fd >= 0) {
            _dpevp_kq_vnode_del(w->vnode_fd);
            close(w->vnode_fd);
            w->vnode_fd = -1;
        }
    }
    fsm->watch_cnt = 0;
    if (fsm->pipe_w >= 0) {
        close(fsm->pipe_w);
        fsm->pipe_w = -1;
    }
}

static dpele_type_t _dpfsm_type = {
    .name = "fsm",
    .type = DPELE_TYPE_EFD,
    .size = sizeof(dpfsm_iop_t),
    .iotype = DPAIO_TYPE_GFD,
    .events = DPEVT_IN,
    .init = dpfsm_vopen,
    .fini = _dpfsm_fini,
};

const dpele_type_t* dpfsm_type()
{
    return &_dpfsm_type;
}

dpret_t dpfsm_addev(dpele_t* self, const char* path, uint32_t mask)
{
    if (path == NULL) {
        return DPE_INVAL;
    }

    dpfsm_iop_t* fsm = (dpfsm_iop_t*)dpele_aux_data(self);
    if (fsm->watch_cnt >= DPFSM_MAX_WATCH) {
        return DPE_NOMEM;
    }

#ifdef __APPLE__
    int vfd = open(path, O_EVTONLY);
#else
    int vfd = open(path, O_RDONLY);
#endif
    if (vfd < 0) {
        return -errno;
    }

    dpfsm_watch_t* w = &fsm->watches[fsm->watch_cnt];
    w->ele = self;
    w->wd = fsm->next_wd++;
    w->vnode_fd = vfd;
    w->inotify_mask = mask;
    const char* base = strrchr(path, '/');
    if (base) {
        base++;
    } else {
        base = path;
    }
    strncpy(w->name, base, sizeof(w->name) - 1);
    w->name[sizeof(w->name) - 1] = '\0';

    dpret_t r = _dpevp_kq_vnode_add(vfd, w, _inotify_to_note(mask));
    if (dpret_iserr(r)) {
        close(vfd);
        return r;
    }

    fsm->watch_cnt++;
    return w->wd;
}

dpret_t dpfsm_delev(dpele_t* self, int wd)
{
    dpfsm_iop_t* fsm = (dpfsm_iop_t*)dpele_aux_data(self);
    for (int i = 0; i < fsm->watch_cnt; i++) {
        dpfsm_watch_t* w = &fsm->watches[i];
        if (w->wd != wd) {
            continue;
        }
        _dpevp_kq_vnode_del(w->vnode_fd);
        close(w->vnode_fd);
        if (i < fsm->watch_cnt - 1) {
            fsm->watches[i] = fsm->watches[fsm->watch_cnt - 1];
        }
        fsm->watch_cnt--;
        return DPE_OK;
    }
    return DPE_INVAL;
}

void _dpfsm_kq_notify(void* udata, uint32_t note_fflags)
{
    dpfsm_watch_t* w = (dpfsm_watch_t*)udata;
    if (w == NULL || w->ele == NULL) {
        return;
    }

    dpfsm_iop_t* fsm = (dpfsm_iop_t*)dpele_aux_data(w->ele);
    if (fsm->pipe_w < 0) {
        return;
    }

    struct
    {
        int wd;
        uint32_t mask;
        uint32_t cookie;
        uint32_t len;
        char name[NAME_MAX + 1];
    } ev;

    memset(&ev, 0, sizeof(ev));
    ev.wd = w->wd;
    ev.mask = _note_to_inotify(note_fflags);
    ev.len = (uint32_t)strlen(w->name);
    if (ev.len > NAME_MAX) {
        ev.len = NAME_MAX;
    }
    memcpy(ev.name, w->name, ev.len);

    size_t ev_len = sizeof(ev.wd) + sizeof(ev.mask) + sizeof(ev.cookie)
        + sizeof(ev.len) + ev.len;
    (void)write(fsm->pipe_w, &ev, ev_len);

    _dpefd_feed(w->ele, DPEVT_IN);
}
