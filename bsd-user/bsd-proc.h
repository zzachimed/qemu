/*
 *  process related system call shims and definitions
 *
 *  Copyright (c) 2013-2014 Stacey D. Son
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BSD_PROC_H_
#define BSD_PROC_H_

#include <sys/resource.h>

#include "qemu-bsd.h"
#include "gdbstub/syscalls.h"
#include "qemu/plugin.h"

extern int _getlogin(char*, int);
int bsd_get_ncpu(void);

/* exit(2) */
static inline abi_long do_bsd_exit(void *cpu_env, abi_long arg1)
{
#ifdef TARGET_GPROF
    _mcleanup();
#endif
    gdb_exit(arg1);
    qemu_plugin_user_exit();
    _exit(arg1);

    return 0;
}

/* getgroups(2) */
static inline abi_long do_bsd_getgroups(abi_long gidsetsize, abi_long arg2)
{
    abi_long ret;
    uint32_t *target_grouplist;
    g_autofree gid_t *grouplist;
    int i;

    grouplist = g_try_new(gid_t, gidsetsize);
    ret = get_errno(getgroups(gidsetsize, grouplist));
    if (gidsetsize != 0) {
        if (!is_error(ret)) {
            target_grouplist = lock_user(VERIFY_WRITE, arg2, gidsetsize * 2, 0);
            if (!target_grouplist) {
                return -TARGET_EFAULT;
            }
            for (i = 0; i < ret; i++) {
                target_grouplist[i] = tswap32(grouplist[i]);
            }
            unlock_user(target_grouplist, arg2, gidsetsize * 2);
        }
    }
    return ret;
}

/* setgroups(2) */
static inline abi_long do_bsd_setgroups(abi_long gidsetsize, abi_long arg2)
{
    uint32_t *target_grouplist;
    g_autofree gid_t *grouplist;
    int i;

    grouplist = g_try_new(gid_t, gidsetsize);
    target_grouplist = lock_user(VERIFY_READ, arg2, gidsetsize * 2, 1);
    if (!target_grouplist) {
        return -TARGET_EFAULT;
    }
    for (i = 0; i < gidsetsize; i++) {
        grouplist[i] = tswap32(target_grouplist[i]);
    }
    unlock_user(target_grouplist, arg2, 0);
    return get_errno(setgroups(gidsetsize, grouplist));
}

/* umask(2) */
static inline abi_long do_bsd_umask(abi_long arg1)
{
    return get_errno(umask(arg1));
}

/* setlogin(2) */
static inline abi_long do_bsd_setlogin(abi_long arg1)
{
    abi_long ret;
    void *p;

    p = lock_user_string(arg1);
    if (p == NULL) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(setlogin(p));
    unlock_user(p, arg1, 0);

    return ret;
}

/* getlogin(2) */
static inline abi_long do_bsd_getlogin(abi_long arg1, abi_long arg2)
{
    abi_long ret;
    void *p;

    p = lock_user(VERIFY_WRITE, arg1, arg2, 0);
    if (p == NULL) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(_getlogin(p, arg2));
    unlock_user(p, arg1, arg2);

    return ret;
}

/* getrusage(2) */
static inline abi_long do_bsd_getrusage(abi_long who, abi_ulong target_addr)
{
    abi_long ret;
    struct rusage rusage;

    ret = get_errno(getrusage(who, &rusage));
    if (!is_error(ret)) {
        host_to_target_rusage(target_addr, &rusage);
    }
    return ret;
}

/* getrlimit(2) */
static inline abi_long do_bsd_getrlimit(abi_long arg1, abi_ulong arg2)
{
    abi_long ret;
    int resource = target_to_host_resource(arg1);
    struct target_rlimit *target_rlim;
    struct rlimit rlim;

    switch (resource) {
    case RLIMIT_STACK:
        rlim.rlim_cur = target_dflssiz;
        rlim.rlim_max = target_maxssiz;
        ret = 0;
        break;

    case RLIMIT_DATA:
        rlim.rlim_cur = target_dfldsiz;
        rlim.rlim_max = target_maxdsiz;
        ret = 0;
        break;

    default:
        ret = get_errno(getrlimit(resource, &rlim));
        break;
    }
    if (!is_error(ret)) {
        if (!lock_user_struct(VERIFY_WRITE, target_rlim, arg2, 0)) {
            return -TARGET_EFAULT;
        }
        target_rlim->rlim_cur = host_to_target_rlim(rlim.rlim_cur);
        target_rlim->rlim_max = host_to_target_rlim(rlim.rlim_max);
        unlock_user_struct(target_rlim, arg2, 1);
    }
    return ret;
}

/* setrlimit(2) */
static inline abi_long do_bsd_setrlimit(abi_long arg1, abi_ulong arg2)
{
    abi_long ret;
    int resource = target_to_host_resource(arg1);
    struct target_rlimit *target_rlim;
    struct rlimit rlim;

    if (RLIMIT_STACK == resource) {
        /* XXX We should, maybe, allow the stack size to shrink */
        ret = -TARGET_EPERM;
    } else {
        if (!lock_user_struct(VERIFY_READ, target_rlim, arg2, 1)) {
            return -TARGET_EFAULT;
        }
        rlim.rlim_cur = target_to_host_rlim(target_rlim->rlim_cur);
        rlim.rlim_max = target_to_host_rlim(target_rlim->rlim_max);
        unlock_user_struct(target_rlim, arg2, 0);
        ret = get_errno(setrlimit(resource, &rlim));
    }
    return ret;
}

#endif /* !BSD_PROC_H_ */
