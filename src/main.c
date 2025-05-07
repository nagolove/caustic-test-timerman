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
#include "koh_common.h"
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

//__attribute__((unused))
static MunitResult test_new_free(const MunitParameter params[], void* data) {
    InitWindow(800, 600, "testing koh_timerman");
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED);
    SetTargetFPS(120);
    TimerMan *tm = timerman_new(10, "x");
    T t = { .duration_sec = 1, };
    t_init(&t);
    while (!WindowShouldClose() && t_update(&t)) {
    }
    CloseWindow();
    timerman_free(tm);
    return MUNIT_OK;
}

static bool tmr_on_update_duration(Timer *t) {
    //T *tt = t->data;

    if (t->state == TS_ON_START)
        munit_assert(t->amount == 0.);
    else if (t->state == TS_ON_STOP)
        munit_assert(t->amount == 1.);

    /*
    printf(
        "tmr_on_update: amount %f, t %f, state %d\n",
        t->amount, tt->t, t->state
    );
    */

    return false;
}

static int chain_cnt = 3,
           remove_cnt_1 = 0,
           remove_cnt_2 = 0;

static bool tmr_on_update_remove_2(Timer *t) {
    if (remove_cnt_2 >= 10)
        return true;

    remove_cnt_2++;
    return false;
}

static bool tmr_on_update_remove_1(Timer *t) {
    remove_cnt_1++;
    munit_assert(t->state != TS_ON_UPDATE);
    // удалить сразу
    return true;
}

static bool tmr_on_update_chain(Timer *t) {
    //T *tt = t->data;

    if (t->state == TS_ON_START) {
        munit_assert(t->amount == 0.);
    } else if (t->state == TS_ON_STOP) {
        munit_assert(t->amount == 1.);
        TimerDef def = {
            .data = t->data,
            .sz = t->sz,
            .on_update = t->on_update,
            .on_start = t->on_start,
            .on_stop = t->on_stop,
            .duration = t->duration,
        };
        if (chain_cnt >= 0) {

            // XXX: Не отрабатывает, только один

            koh_term_color_set(KOH_TERM_YELLOW);
            printf("tmr_on_update_chain: chain_cnt %d\n", chain_cnt);
            koh_term_color_reset();

            int err = timerman_add(t->tm, def);
            munit_assert(err != -1);
            chain_cnt--;
        }
    }

    /*
    printf(
        "tmr_on_update: amount %f, t %f, state %d\n",
        t->amount, tt->t, t->state
    );
    */

    return false;
}

static MunitResult test_remove(const MunitParameter params[], void* data) {

    {
        InitWindow(800, 600, "testing koh_timerman");
        SetConfigFlags(FLAG_WINDOW_UNDECORATED);
        SetTargetFPS(60);

        const float base_duration = 1.f;
        T t = { .duration_sec = base_duration };
        t_init(&t);
        TimerMan *tm = timerman_new(1, "x");
        int id = timerman_add(tm, (TimerDef) {
            .on_start = tmr_on_update_remove_1,
            .on_update = tmr_on_update_remove_1,
            .on_stop = tmr_on_update_remove_1,
            .duration = base_duration / 2.,
            .data = &t,
            .sz = 0,
        });
        munit_assert(id != -1);

        while (!WindowShouldClose() && t_update(&t)) {
            timerman_update(tm);
        }

        munit_assert_int(remove_cnt_1, ==, 1 /* start */ + 1 /* stop */);
        CloseWindow();
        timerman_free(tm);
    }

    {
        InitWindow(800, 600, "testing koh_timerman");
        SetConfigFlags(FLAG_WINDOW_UNDECORATED);
        SetTargetFPS(60);

        const float base_duration = 1.f;
        T t = { .duration_sec = base_duration };
        t_init(&t);
        TimerMan *tm = timerman_new(1, "x");
        int id = timerman_add(tm, (TimerDef) {
            .on_start = tmr_on_update_remove_2,
            .on_update = tmr_on_update_remove_2,
            .on_stop = tmr_on_update_remove_2,
            .duration = base_duration,
            .data = &t,
            .sz = 0,
        });
        munit_assert(id != -1);

        while (!WindowShouldClose() && t_update(&t)) {
            timerman_update(tm);
        }
        munit_assert_int(remove_cnt_2, ==, 10);
        CloseWindow();
        timerman_free(tm);
    }

    return MUNIT_OK;
}

static MunitResult test_chain(const MunitParameter params[], void* data) {
    printf("test_chain:\n");

    InitWindow(800, 600, "testing koh_timerman");
    SetConfigFlags(FLAG_WINDOW_UNDECORATED);
    SetTargetFPS(120);

    /*
    rlImGuiSetup(&(struct igSetupOptions) {
        .dark = false,
        .font_path = "assets/djv.ttf",
        .font_size_pixels = 40,
        .ranges = (ImWchar[]){
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0400, 0x044F, // Cyrillic
            // XXX: symbols not displayed
            // media buttons like record/play etc. Used in dotool_gui()
            0x23CF, 0x23F5, 
            0,
        },
    });
    */

    const float base_duration = 1.f;
    T t = { .duration_sec = base_duration * (chain_cnt + 1), };
    t_init(&t);
    TimerMan *tm = timerman_new(2, "x");
    int id = timerman_add(tm, (TimerDef) {
        .on_start = tmr_on_update_chain,
        .on_update = tmr_on_update_chain,
        .on_stop = tmr_on_update_chain,
        .duration = base_duration,
        .data = &t,
        .sz = 0,
    });
    munit_assert(id != -1);

    while (!WindowShouldClose() && t_update(&t)) {
        //printf("test_duration: t %f\n", t.t);

        /*
        BeginDrawing();
        rlImGuiBegin();
        timerman_window_gui(tm);
        rlImGuiEnd();
        EndDrawing();
        */

        timerman_update(tm);
    }
    printf("chain_cnt %d\n", chain_cnt);
    munit_assert(chain_cnt == 0);
    CloseWindow();
    timerman_free(tm);
    return MUNIT_OK;
}


static MunitResult test_duration(const MunitParameter params[], void* data) {
    printf("test_duration:\n");

    InitWindow(800, 600, "testing koh_timerman");
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED);
    SetTargetFPS(120);

    T t = { .duration_sec = 1, };
    t_init(&t);
    TimerMan *tm = timerman_new(1, "");
    int id = timerman_add(tm, (TimerDef) {
        .on_start = tmr_on_update_duration,
        .on_update = tmr_on_update_duration,
        .on_stop = tmr_on_update_duration,
        .duration = 1.,
        .data = &t,
        .sz = 0,
    });
    munit_assert(id != -1);
    while (!WindowShouldClose() && t_update(&t)) {
        //printf("test_duration: t %f\n", t.t);
        timerman_update(tm);
    }
    CloseWindow();
    timerman_free(tm);
    return MUNIT_OK;
}

static MunitTest t_suite_common[] = {

    {
        .name =  "/test_new_free",
        .test = test_new_free,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },

    {
        .name =  "/test_duration",
        .test = test_duration,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },

    {
        .name =  "/test_chain",
        .test = test_chain,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },

    {
        .name =  "/test_remove",
        .test = test_remove,
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

/*
__attribute__((constructor))
static void disable_abort_on_error(void) {
    setenv("ASAN_OPTIONS", "abort_on_error=0", 1);
}
*/

int main(int argc, char **argv) {
    return munit_suite_main(&suite_root, (void*) "µnit", argc, argv);
}

