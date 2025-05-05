// vim: set colorcolumn=85
// vim: fdm=marker

// {{{ include
#include "munit.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "koh_timerman.h"
#include "time.h"
#include "raylib.h"

// }}}

static bool verbose = false;

typedef struct T {
    struct timespec start, now;
    int    duration_sec;
    double t;
} T;

static void t_init(T *t) {
    assert(t);
    t->t = 0.;
    // Получаем начальное время
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

static bool t_update(T *t) {
    assert(t);
    clock_gettime(CLOCK_MONOTONIC, &t->now);

    // Вычисляем прошедшее время
    time_t elapsed_sec = t->now.tv_sec - t->start.tv_sec;
    long elapsed_nsec = t->now.tv_nsec - t->start.tv_nsec;

    if (elapsed_nsec < 0) {
        elapsed_sec--;
        elapsed_nsec += 1000000000L;
    }
    
    t->t = (double)elapsed_sec + (double)elapsed_nsec / 1e9;

    if (elapsed_sec >= t->duration_sec) {
        return false;
    }

    // Пауза 1 мс, чтобы разгрузить CPU
    struct timespec sleep_time = {0, 1000000L}; // 1 миллисекунда
    nanosleep(&sleep_time, NULL);
    return true;
}

__attribute__((unused))
static MunitResult test_new_free(const MunitParameter params[], void* data) {
    TimerMan *tm = timerman_new(10, "x");
    T t = { .duration_sec = 1, };
    InitWindow(800, 600, "testing koh_timerman");
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED);
    SetTargetFPS(120);
    t_init(&t);
    while (!WindowShouldClose() && t_update(&t)) {
        //static int i = 0;
        //printf("test_new_free: i %d, t %f\n", i++, t.t);
    }
    CloseWindow();
    timerman_free(tm);
    return MUNIT_OK;
}

static bool tmr_on_update_duration(Timer *t) {
    // printf("tmr_on_update:\n");
    T *tt = t->data;
    printf("tmr_on_update: amount %f, t %f\n", t->amount, tt->t);
    return false;
}

static MunitResult test_duration(const MunitParameter params[], void* data) {
    printf("test_duration:\n");

    InitWindow(800, 600, "testing koh_timerman");
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED);
    SetTargetFPS(120);

    T t = { .duration_sec = 1, };
    t_init(&t);
    TimerMan *tm = timerman_new(10, "x");
    timerman_add(tm, (TimerDef) {
        .on_update = tmr_on_update_duration,
        .duration = 0.5,
        .data = &t,
        .sz = 0,
    });

    while (!WindowShouldClose() && t_update(&t)) {
        printf("test_duration: t %f\n", t.t);
        timerman_update(tm);
    }
    CloseWindow();
    timerman_free(tm);
    return MUNIT_OK;
}

static MunitTest t_suite_common[] = {

    /*
    {
        .name =  "/test_new_free",
        .test = test_new_free,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },
    */

    {
        .name =  "/test_duration",
        .test = test_duration,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },

    {
        .name =  NULL,
        .test = NULL,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },

};

static const MunitSuite suite_root = {
    .prefix = (char*) "b2",
    .tests =  t_suite_common,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
    .verbose = &verbose,
};

int main(int argc, char **argv) {
    return munit_suite_main(&suite_root, (void*) "µnit", argc, argv);
}

