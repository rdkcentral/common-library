/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * psm_api_speed_test [-f] [iterations]
 *
 * -f also prints the failed call count per API.
 *
 * Measures the average latency of each PSM helper API in libccsp_common.
 * The same source is built for CORD and non-CORD images (CORD_ENABLED is set
 * by configure --enable-cord), so results can be compared directly.
 *
 * Only the API call itself is timed; fixture set-up and freeing of returned
 * memory are outside the timed region. Failed calls are counted but excluded
 * from the timing statistics.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ccsp_message_bus.h"
#include "ccsp_base_api.h"
#include "ccsp_psm_helper.h"
#include "ccsp_custom.h"

#define DEFAULT_ITERATIONS  1000000ULL
#define COMPONENT_ID        "com.cisco.spvtg.ccsp.psm.apispeedtest"
/* Matches psmcli's default prefix; the empty prefix makes PSM_Del_Record fail. */
#define SUBSYS              "eRT."

#define PERF_ROOT           "dmsb.perftest."
#define KEY_STR             PERF_ROOT "StringParam"
#define KEY_INT             PERF_ROOT "IntParam"
#define KEY_DEL             PERF_ROOT "DelParam"
#define INST_PARENT         PERF_ROOT "Inst."
#define NUM_INST            5U
#define STR_VALUE           "perftest123"
#define INT_VALUE           12345

typedef struct {
    uint64_t ok;
    uint64_t fails;
    uint64_t total_ns;
    uint64_t min_ns;
    uint64_t max_ns;
} stats_t;

typedef void (*bench_fn)(uint64_t n, stats_t *st);

static void *g_bus = NULL;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void stats_add(stats_t *st, uint64_t ns, int rc)
{
    if (rc != CCSP_SUCCESS) {
        st->fails++;
        return;
    }
    if (st->ok == 0 || ns < st->min_ns) st->min_ns = ns;
    if (ns > st->max_ns) st->max_ns = ns;
    st->ok++;
    st->total_ns += ns;
}

static void bus_free(void *p)
{
    if (p) ((CCSP_MESSAGE_BUS_INFO *)g_bus)->freefunc(p);
}

static void inst_key(char *buf, size_t len, unsigned int idx)
{
    snprintf(buf, len, INST_PARENT "%u.Name", idx);
}

static int set_int_record(int value)
{
    SLAP_VARIABLE var;
    memset(&var, 0, sizeof(var));
    var.Syntax = SLAP_VAR_SYNTAX_int;
    var.Variant.varInt = value;
    return PSM_Set_Record_Value(g_bus, SUBSYS, KEY_INT, ccsp_int, &var);
}

/* ---- benchmarks ------------------------------------------------------- */

static void bench_get_next_level_instances(uint64_t n, stats_t *st)
{
    for (uint64_t i = 0; i < n; i++) {
        unsigned int  num = 0;
        unsigned int *arr = NULL;
        uint64_t t0 = now_ns();
        int rc = PsmGetNextLevelInstances(g_bus, SUBSYS, INST_PARENT, &num, &arr);
        uint64_t t1 = now_ns();
        stats_add(st, t1 - t0, rc);
        bus_free(arr);
    }
}

static void bench_enum_records(uint64_t n, stats_t *st)
{
    for (uint64_t i = 0; i < n; i++) {
        unsigned int      num = 0;
        PCCSP_BASE_RECORD arr = NULL;
        uint64_t t0 = now_ns();
        int rc = PsmEnumRecords(g_bus, SUBSYS, INST_PARENT, 1, &num, &arr);
        uint64_t t1 = now_ns();
        stats_add(st, t1 - t0, rc);
        bus_free(arr);
    }
}

static void bench_get_record_value2(uint64_t n, stats_t *st)
{
    for (uint64_t i = 0; i < n; i++) {
        char        *val  = NULL;
        unsigned int type = 0;
        uint64_t t0 = now_ns();
        int rc = PSM_Get_Record_Value2(g_bus, SUBSYS, KEY_STR, &type, &val);
        uint64_t t1 = now_ns();
        stats_add(st, t1 - t0, rc);
        bus_free(val);
    }
}

static void bench_group_get(uint64_t n, stats_t *st)
{
    const char *names[2] = { KEY_STR, KEY_INT };

    for (uint64_t i = 0; i < n; i++) {
        parameterValStruct_t **recs = NULL;
        int nrec = 0;
        uint64_t t0 = now_ns();
        int rc = PsmGroupGet(g_bus, SUBSYS, names, 2, &recs, &nrec);
        uint64_t t1 = now_ns();
        stats_add(st, t1 - t0, rc);
        if (rc == CCSP_SUCCESS && recs)
            PsmFreeRecords(g_bus, recs, nrec);
    }
}

static void bench_del_record(uint64_t n, stats_t *st)
{
    int reported = 0;

    for (uint64_t i = 0; i < n; i++) {
        /* Re-create the record so every delete has something to remove. */
        int set_rc = PSM_Set_Record_Value2(g_bus, SUBSYS, KEY_DEL, ccsp_string, STR_VALUE);
        uint64_t t0 = now_ns();
        int rc = PSM_Del_Record(g_bus, SUBSYS, KEY_DEL);
        uint64_t t1 = now_ns();
        if (rc != CCSP_SUCCESS && !reported) {
            fprintf(stderr, "PSM_Del_Record: first failure rc=%d (preceding set rc=%d)\n",
                    rc, set_rc);
            reported = 1;
        }
        stats_add(st, t1 - t0, rc);
    }
}

static void bench_set_record_value2(uint64_t n, stats_t *st)
{
    for (uint64_t i = 0; i < n; i++) {
        uint64_t t0 = now_ns();
        int rc = PSM_Set_Record_Value2(g_bus, SUBSYS, KEY_STR, ccsp_string, STR_VALUE);
        uint64_t t1 = now_ns();
        stats_add(st, t1 - t0, rc);
    }
}

static void bench_get_record_value(uint64_t n, stats_t *st)
{
    for (uint64_t i = 0; i < n; i++) {
        SLAP_VARIABLE var;
        unsigned int  type = 0;
        memset(&var, 0, sizeof(var));
        uint64_t t0 = now_ns();
        int rc = PSM_Get_Record_Value(g_bus, SUBSYS, KEY_INT, &type, &var);
        uint64_t t1 = now_ns();
        stats_add(st, t1 - t0, rc);
    }
}

static void bench_set_record_value(uint64_t n, stats_t *st)
{
    SLAP_VARIABLE var;
    memset(&var, 0, sizeof(var));
    var.Syntax = SLAP_VAR_SYNTAX_int;
    var.Variant.varInt = INT_VALUE;

    for (uint64_t i = 0; i < n; i++) {
        uint64_t t0 = now_ns();
        int rc = PSM_Set_Record_Value(g_bus, SUBSYS, KEY_INT, ccsp_int, &var);
        uint64_t t1 = now_ns();
        stats_add(st, t1 - t0, rc);
    }
}

static const struct {
    const char *name;
    bench_fn    fn;
} k_bench[] = {
    { "PsmGetNextLevelInstances", bench_get_next_level_instances },
    { "PsmEnumRecords",           bench_enum_records },
    { "PSM_Get_Record_Value2",    bench_get_record_value2 },
    { "PsmGroupGet",              bench_group_get },
    { "PSM_Del_Record",           bench_del_record },
    { "PSM_Set_Record_Value2",    bench_set_record_value2 },
    { "PSM_Get_Record_Value",     bench_get_record_value },
    { "PSM_Set_Record_Value",     bench_set_record_value },
};

/* ---- fixture ---------------------------------------------------------- */

static int fixture_setup(void)
{
    char key[128];
    int  rc;

    rc = PSM_Set_Record_Value2(g_bus, SUBSYS, KEY_STR, ccsp_string, STR_VALUE);
    if (rc != CCSP_SUCCESS) return rc;

    rc = set_int_record(INT_VALUE);
    if (rc != CCSP_SUCCESS) return rc;

    for (unsigned int i = 1; i <= NUM_INST; i++) {
        inst_key(key, sizeof(key), i);
        rc = PSM_Set_Record_Value2(g_bus, SUBSYS, key, ccsp_string, STR_VALUE);
        if (rc != CCSP_SUCCESS) return rc;
    }
    return CCSP_SUCCESS;
}

static void fixture_cleanup(void)
{
    char key[128];

    PSM_Del_Record(g_bus, SUBSYS, KEY_STR);
    PSM_Del_Record(g_bus, SUBSYS, KEY_INT);
    PSM_Del_Record(g_bus, SUBSYS, KEY_DEL);
    for (unsigned int i = 1; i <= NUM_INST; i++) {
        inst_key(key, sizeof(key), i);
        PSM_Del_Record(g_bus, SUBSYS, key);
    }
}

/* ---- main ------------------------------------------------------------- */

static int parse_iterations(const char *arg, uint64_t *out)
{
    char *end = NULL;

    if (arg[0] < '0' || arg[0] > '9') return -1;
    errno = 0;
    unsigned long long v = strtoull(arg, &end, 10);
    if (errno != 0 || *end != '\0' || v == 0) return -1;
    *out = (uint64_t)v;
    return 0;
}

int main(int argc, char *argv[])
{
    static char component_id[] = COMPONENT_ID;
    uint64_t    iterations     = DEFAULT_ITERATIONS;
    int         show_fails     = 0;
    int         have_iters     = 0;
    int         rc;

    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "-f") == 0) {
            show_fails = 1;
        } else if (!have_iters && parse_iterations(argv[a], &iterations) == 0) {
            have_iters = 1;
        } else {
            fprintf(stderr,
                    "Usage: %s [-f] [iterations]\n"
                    "  -f          also print the failed call count per API\n"
                    "  iterations: positive integer (default %llu)\n",
                    argv[0], DEFAULT_ITERATIONS);
            return 1;
        }
    }

    rc = CCSP_Message_Bus_Init(component_id, CCSP_MSG_BUS_CFG, &g_bus, NULL, NULL);
    if (rc == -1 || g_bus == NULL) {
        fprintf(stderr, "ERROR: CCSP_Message_Bus_Init failed (rc=%d)\n", rc);
        return 1;
    }

    rc = fixture_setup();
    if (rc != CCSP_SUCCESS) {
        fprintf(stderr, "ERROR: fixture set-up failed (rc=%d)\n", rc);
        fixture_cleanup();
        CCSP_Message_Bus_Exit(g_bus);
        return 1;
    }

#ifdef CORD_ENABLED
    printf("Mode       : CORD\n");
#else
    printf("Mode       : non-CORD\n");
#endif
    printf("Iterations : %llu per API\n\n", (unsigned long long)iterations);
    if (show_fails)
        printf("%-26s %14s %10s\n", "API", "Avg (ms)", "Failed");
    else
        printf("%-26s %14s\n", "API", "Avg (ms)");
    fflush(stdout);

    int failed_any = 0;
    for (size_t i = 0; i < sizeof(k_bench) / sizeof(k_bench[0]); i++) {
        stats_t st;
        char    avg[32];
        memset(&st, 0, sizeof(st));
        k_bench[i].fn(iterations, &st);

        if (st.ok > 0)
            snprintf(avg, sizeof(avg), "%.6f", (double)st.total_ns / (double)st.ok / 1e6);
        else
            snprintf(avg, sizeof(avg), "n/a");
        printf("%-26s %14s", k_bench[i].name, avg);
        if (show_fails)
            printf(" %10llu", (unsigned long long)st.fails);
        printf("\n");
        fflush(stdout);

        if (st.fails > 0) {
            fprintf(stderr, "WARNING: %s: %llu of %llu calls failed (excluded from average)\n",
                    k_bench[i].name, (unsigned long long)st.fails,
                    (unsigned long long)(st.ok + st.fails));
            failed_any = 1;
        }
    }

    fixture_cleanup();
    CCSP_Message_Bus_Exit(g_bus);
    return failed_any ? 2 : 0;
}
