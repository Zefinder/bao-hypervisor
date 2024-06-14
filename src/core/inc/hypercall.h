/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef HYPERCALL_H
#define HYPERCALL_H

#include <bao.h>
#include <arch/hypercall.h>

// TODO Get CPU id is for testing, remove when it works
enum { HC_INVAL = 0, HC_IPC = 1, HC_REQUEST_MEM_ACCESS = 2, HC_REVOKE_MEM_ACCESS = 3, HC_GET_CPU_ID = 4, HC_NOTIFY_CPU = 5, HC_EMPTY_CALL = 6, HC_REQUEST_MEM_ACCESS_TIMER = 7, HC_DISPLAY_STRING=8 };

enum { HC_E_SUCCESS = 0, HC_E_FAILURE = 1, HC_E_INVAL_ID = 2, HC_E_INVAL_ARGS = 3 };

typedef unsigned long (*hypercall_handler)(unsigned long arg0, unsigned long arg1,
    unsigned long arg2);

long int hypercall(unsigned long id);

#endif /* HYPERCALL_H */
