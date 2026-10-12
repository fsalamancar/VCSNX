// C11 thread functions for the legacy switch-mesa (Mesa 20.1) libEGL.a that switch-sdl2 presents
// through. That archive was compiled against newlib's <threads.h> (thrd_success = 4, ...), but in a
// Vulkan build the names mtx_lock, cnd_wait, ... are defined by mesa-switch's own C11 layer, which
// returns Mesa's codes (0 for success). tools/build_mesa_switch.sh therefore renames the legacy
// copy's references to vcsnx_legacy_c11_<name>, and this file implements them over pthreads with
// newlib's types and return codes (Plan 2 review, Important 3). Switch Vulkan builds only.
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>

static int code_from(int pthread_result) { return pthread_result == 0 ? thrd_success : thrd_error; }

void vcsnx_legacy_c11_call_once(once_flag *flag, void (*func)(void)) { pthread_once(flag, func); }

int vcsnx_legacy_c11_mtx_init(mtx_t *mutex, int type) {
    pthread_mutexattr_t attributes;
    if (pthread_mutexattr_init(&attributes) != 0) return thrd_error;
    if ((type & mtx_recursive) != 0) pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    const int result = pthread_mutex_init(mutex, &attributes);
    pthread_mutexattr_destroy(&attributes);
    return code_from(result);
}
void vcsnx_legacy_c11_mtx_destroy(mtx_t *mutex) { pthread_mutex_destroy(mutex); }
int vcsnx_legacy_c11_mtx_lock(mtx_t *mutex) { return code_from(pthread_mutex_lock(mutex)); }
int vcsnx_legacy_c11_mtx_unlock(mtx_t *mutex) { return code_from(pthread_mutex_unlock(mutex)); }

int vcsnx_legacy_c11_cnd_init(cnd_t *condition) {
    const int result = pthread_cond_init(condition, NULL);
    return result == 0 ? thrd_success : (result == ENOMEM ? thrd_nomem : thrd_error);
}
void vcsnx_legacy_c11_cnd_destroy(cnd_t *condition) { pthread_cond_destroy(condition); }
int vcsnx_legacy_c11_cnd_signal(cnd_t *condition) { return code_from(pthread_cond_signal(condition)); }
int vcsnx_legacy_c11_cnd_broadcast(cnd_t *condition) { return code_from(pthread_cond_broadcast(condition)); }
int vcsnx_legacy_c11_cnd_wait(cnd_t *condition, mtx_t *mutex) {
    return code_from(pthread_cond_wait(condition, mutex));
}
int vcsnx_legacy_c11_cnd_timedwait(cnd_t *condition, mtx_t *mutex, const struct timespec *deadline) {
    const int result = pthread_cond_timedwait(condition, mutex, deadline);
    return result == 0 ? thrd_success : (result == ETIMEDOUT ? thrd_timedout : thrd_error);
}

struct vcsnx_thread_start {
    thrd_start_t func;
    void *arg;
};

static void *vcsnx_thread_trampoline(void *raw) {
    struct vcsnx_thread_start start = *(struct vcsnx_thread_start *)raw;
    free(raw);
    return (void *)(intptr_t)start.func(start.arg);
}

int vcsnx_legacy_c11_thrd_create(thrd_t *thread, thrd_start_t func, void *arg) {
    struct vcsnx_thread_start *start = malloc(sizeof(*start));
    if (start == NULL) return thrd_nomem;
    start->func = func;
    start->arg = arg;
    const int result = pthread_create(thread, NULL, vcsnx_thread_trampoline, start);
    if (result != 0) {
        free(start);
        return result == EAGAIN || result == ENOMEM ? thrd_nomem : thrd_error;
    }
    return thrd_success;
}
int vcsnx_legacy_c11_thrd_join(thrd_t thread, int *result) {
    void *value = NULL;
    if (pthread_join(thread, &value) != 0) return thrd_error;
    if (result != NULL) *result = (int)(intptr_t)value;
    return thrd_success;
}
void vcsnx_legacy_c11_thrd_yield(void) { sched_yield(); }
