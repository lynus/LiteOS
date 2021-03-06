#define _GNU_SOURCE
#include "los_base.h"
#include "los_task.ph"
#include <stdlib.h>
#include <signal.h>
#include <ucontext.h>
#include <errno.h>

#define GET_UCONTEXT(tcb)  (ucontext_t *)((tcb)->uwTopOfStack + (tcb)->uwStackSize - sizeof(ucontext_t))
void populateHeap() __attribute__ ((constructor));
extern sigset_t registerred_signals;
UINT32 __LOS_HEAP_ADDR_START__;
UINT32 __LOS_HEAP_ADDR_END__;
extern TSKSWITCHHOOK g_pfnTskSwitchHook;
static sigset_t saveInt;
//los_config.h
BOOL g_bInterruptState = 0;

void LOS_StartToRun()
{
    ucontext_t *context;
    extern BOOL g_bTaskScheduled;
    g_bTaskScheduled = 1;
    g_stLosTask.pstRunTask = g_stLosTask.pstNewTask;
    g_stLosTask.pstRunTask->usTaskStatus |= OS_TASK_STATUS_RUNNING;
    context = (ucontext_t *) g_stLosTask.pstRunTask->pStackPointer;
    LOS_IntUnLock();
    setcontext(context);
}

void osTaskSchedule()
{
    raise(SIGUSR1);
}

void switcher(int sig, siginfo_t *info, void *context)
{
    ucontext_t *oldcontext = GET_UCONTEXT(g_stLosTask.pstRunTask);
    ucontext_t *newcontext = GET_UCONTEXT(g_stLosTask.pstNewTask);
#ifndef PORTABLE
    UINT32 cs, ss, ds, es, gs, fs;
    *oldcontext = *(ucontext_t *) context;
    *(ucontext_t *) context = *newcontext;
    cs = oldcontext->uc_mcontext.gregs[REG_CS];
    ss = oldcontext->uc_mcontext.gregs[REG_SS];
    ds = oldcontext->uc_mcontext.gregs[REG_DS];
    es = oldcontext->uc_mcontext.gregs[REG_ES];
    gs = oldcontext->uc_mcontext.gregs[REG_GS];
    fs = oldcontext->uc_mcontext.gregs[REG_FS];

    ((ucontext_t *) context)->uc_mcontext.gregs[REG_CS] = cs;
    ((ucontext_t *) context)->uc_mcontext.gregs[REG_SS] = ss;
    ((ucontext_t *) context)->uc_mcontext.gregs[REG_DS] = ds;
    ((ucontext_t *) context)->uc_mcontext.gregs[REG_ES] = es;
    ((ucontext_t *) context)->uc_mcontext.gregs[REG_GS] = gs;
    ((ucontext_t *) context)->uc_mcontext.gregs[REG_FS] = fs;
#endif
    LOS_IntLock();
    if (g_pfnTskSwitchHook != NULL)
        (*g_pfnTskSwitchHook)();
    //XXX not sure if it is working, need to dive in.....
    g_stLosTask.pstRunTask->pStackPointer = (void *) ((ucontext_t *) oldcontext)->uc_mcontext.gregs[REG_ESP];
    g_stLosTask.pstRunTask->usTaskStatus &= ~(OS_TASK_STATUS_RUNNING);
    g_stLosTask.pstRunTask = g_stLosTask.pstNewTask;
    g_stLosTask.pstRunTask->usTaskStatus |= OS_TASK_STATUS_RUNNING;
    LOS_IntUnLock();
#ifdef PORTABLE
    swapcontext(oldcontext, newcontext);
#endif
}
//following declared in los_hwi.h
UINTPTR LOS_IntLock()
{
    g_bInterruptState = 0;
    sigprocmask(SIG_BLOCK, &registerred_signals, &saveInt);
    return (UINTPTR)&saveInt;
}

void LOS_IntRestore(UINTPTR uvIntSave)
{
    g_bInterruptState = 1;
    sigprocmask(SIG_UNBLOCK, &saveInt, NULL);
}

UINTPTR LOS_IntUnLock()
{
    g_bInterruptState = 0;
    sigprocmask(SIG_UNBLOCK, &registerred_signals, &saveInt);
    return (UINTPTR)&saveInt;
}

void populateHeap()
{
    puts("constructor called!\n");
    if ((__LOS_HEAP_ADDR_START__ = (UINT32) malloc(1 << 24)) == NULL) 
    {
        perror("malloc failed!");
        exit(-1);
    }
    __LOS_HEAP_ADDR_END__ = __LOS_HEAP_ADDR_START__ + (1 << 20) - 1;
    main();
}
