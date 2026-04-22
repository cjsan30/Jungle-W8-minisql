#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "db_guard.h"

typedef struct {
    DbGuard *guard;
    atomic_int start_flag;
    atomic_int active_readers;
    atomic_int active_writers;
    atomic_int max_readers;
    atomic_int max_writers;
    atomic_int overlap_error;
    atomic_int worker_failures;
} GuardTestState;

typedef struct {
    GuardTestState *state;
    int is_writer;
} GuardWorkerArgs;

static void sleep_ms(int duration_ms) {
#ifdef _WIN32
    Sleep((DWORD) duration_ms);
#else
    struct timespec duration;

    duration.tv_sec = duration_ms / 1000;
    duration.tv_nsec = (long) (duration_ms % 1000) * 1000000L;
    (void) nanosleep(&duration, NULL);
#endif
}

static void update_maximum(atomic_int *target, int candidate) {
    int current = atomic_load_explicit(target, memory_order_acquire);

    while (candidate > current &&
           !atomic_compare_exchange_weak_explicit(target,
                                                  &current,
                                                  candidate,
                                                  memory_order_acq_rel,
                                                  memory_order_acquire)) {
    }
}

static int read_work(void *work_ctx, SqlError *error) {
    GuardTestState *state = work_ctx;
    int active_readers;

    (void) error;
    active_readers = (int) atomic_fetch_add_explicit(&state->active_readers, 1, memory_order_acq_rel) + 1;
    update_maximum(&state->max_readers, active_readers);
    if (atomic_load_explicit(&state->active_writers, memory_order_acquire) != 0) {
        atomic_store_explicit(&state->overlap_error, 1, memory_order_release);
    }

    sleep_ms(50);
    (void) atomic_fetch_sub_explicit(&state->active_readers, 1, memory_order_acq_rel);
    return 1;
}

static int write_work(void *work_ctx, SqlError *error) {
    GuardTestState *state = work_ctx;
    int active_writers;

    (void) error;
    active_writers = (int) atomic_fetch_add_explicit(&state->active_writers, 1, memory_order_acq_rel) + 1;
    update_maximum(&state->max_writers, active_writers);
    if (active_writers != 1 || atomic_load_explicit(&state->active_readers, memory_order_acquire) != 0) {
        atomic_store_explicit(&state->overlap_error, 1, memory_order_release);
    }

    sleep_ms(50);
    (void) atomic_fetch_sub_explicit(&state->active_writers, 1, memory_order_acq_rel);
    return 1;
}

static void *guard_worker_main(void *arg) {
    GuardWorkerArgs *worker = arg;
    SqlError error = {0};
    int ok;

    while (atomic_load_explicit(&worker->state->start_flag, memory_order_acquire) == 0) {
        sleep_ms(1);
    }

    if (worker->is_writer) {
        ok = db_guard_execute_write(worker->state->guard, write_work, worker->state, &error);
    } else {
        ok = db_guard_execute_read(worker->state->guard, read_work, worker->state, &error);
    }

    if (!ok) {
        atomic_fetch_add_explicit(&worker->state->worker_failures, 1, memory_order_acq_rel);
    }
    return NULL;
}

static void initialize_test_state(GuardTestState *state, DbGuard *guard) {
    memset(state, 0, sizeof(*state));
    state->guard = guard;
    atomic_init(&state->start_flag, 0);
    atomic_init(&state->active_readers, 0);
    atomic_init(&state->active_writers, 0);
    atomic_init(&state->max_readers, 0);
    atomic_init(&state->max_writers, 0);
    atomic_init(&state->overlap_error, 0);
    atomic_init(&state->worker_failures, 0);
}

static void test_db_guard_allows_parallel_reads(void) {
    DbContext context = {0};
    SqlError error = {0};
    DbGuard *guard = db_guard_create(&context, &error);
    GuardTestState state;
    enum { READ_THREAD_COUNT = 6 };
    GuardWorkerArgs workers[READ_THREAD_COUNT];
    pthread_t threads[READ_THREAD_COUNT];
    int index;

    assert(guard != NULL);
    initialize_test_state(&state, guard);

    for (index = 0; index < READ_THREAD_COUNT; index++) {
        workers[index].state = &state;
        workers[index].is_writer = 0;
        assert(pthread_create(&threads[index], NULL, guard_worker_main, &workers[index]) == 0);
    }

    atomic_store_explicit(&state.start_flag, 1, memory_order_release);
    for (index = 0; index < READ_THREAD_COUNT; index++) {
        pthread_join(threads[index], NULL);
    }

    assert(atomic_load_explicit(&state.worker_failures, memory_order_acquire) == 0);
    assert(atomic_load_explicit(&state.overlap_error, memory_order_acquire) == 0);
    assert(atomic_load_explicit(&state.max_readers, memory_order_acquire) >= 2);

    db_guard_destroy(guard);
}

static void test_db_guard_serializes_writes_against_reads_and_writes(void) {
    DbContext context = {0};
    SqlError error = {0};
    DbGuard *guard = db_guard_create(&context, &error);
    GuardTestState state;
    enum { THREAD_COUNT = 8 };
    GuardWorkerArgs workers[THREAD_COUNT];
    pthread_t threads[THREAD_COUNT];
    int index;

    assert(guard != NULL);
    initialize_test_state(&state, guard);

    for (index = 0; index < THREAD_COUNT; index++) {
        workers[index].state = &state;
        workers[index].is_writer = index >= (THREAD_COUNT / 2);
        assert(pthread_create(&threads[index], NULL, guard_worker_main, &workers[index]) == 0);
    }

    atomic_store_explicit(&state.start_flag, 1, memory_order_release);
    for (index = 0; index < THREAD_COUNT; index++) {
        pthread_join(threads[index], NULL);
    }

    assert(atomic_load_explicit(&state.worker_failures, memory_order_acquire) == 0);
    assert(atomic_load_explicit(&state.overlap_error, memory_order_acquire) == 0);
    assert(atomic_load_explicit(&state.max_writers, memory_order_acquire) == 1);

    db_guard_destroy(guard);
}

int main(void) {
    test_db_guard_allows_parallel_reads();
    test_db_guard_serializes_writes_against_reads_and_writes();
    printf("db guard unit tests passed\n");
    return 0;
}
