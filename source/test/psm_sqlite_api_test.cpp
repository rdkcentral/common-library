/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
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
 * psm_sqlite_api_test.cpp — Unit tests for the SQLite Get/Set helpers
 * in ccsp_base_api.c (tasks 4.1 and 4.2).
 *
 * These tests are built as part of the common-library test binary.
 * They exercise PSM_Get_Record_Value2 / PSM_Set_Record_Value2 end-to-end
 * through the SQLite path, using a disposable database under /tmp so
 * that /nvram is never touched.
 *
 * Build-time override:  -DPSM_DB_PATH=\"/tmp/psm_sqlite_api_test.db\"
 */

#include <gtest/gtest.h>
#include <sqlite3.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

/* ccsp_base_api.h and ccsp_psm_helper.h are available via CPPFLAGS
 * inherited from the common-library build tree.                      */
extern "C" {
#include "ccsp_base_api.h"
#include "ccsp_psm_helper.h"
#include "ccsp_message_bus.h"
}

/* -------------------------------------------------------------------------
 * Minimal CCSP_MESSAGE_BUS_INFO stub — only the fields the SQLite helpers
 * actually dereference (mallocfunc / freefunc).
 * -------------------------------------------------------------------------*/
static void *stub_malloc(size_t n) { return ::malloc(n); }
static void  stub_free(void *p)    { ::free(p); }

static CCSP_MESSAGE_BUS_INFO g_bus_info;

static void init_bus_info(void)
{
    memset(&g_bus_info, 0, sizeof(g_bus_info));
    g_bus_info.mallocfunc = stub_malloc;
    g_bus_info.freefunc   = stub_free;
}

/* -------------------------------------------------------------------------
 * Fixture: creates a fresh SQLite database (matching the schema psm_db_init
 * would create) and tears it down after each test.
 * -------------------------------------------------------------------------*/
class PsmSqliteApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        unlink(PSM_DB_PATH);
        init_bus_info();
        create_schema();
    }

    void TearDown() override {
        /* Force the module-level cached connection closed between tests.
         * The static s_psm_db in ccsp_base_api.c is file-scoped, so we
         * exercise re-open by deleting and recreating the DB file.      */
        unlink(PSM_DB_PATH);
    }

    void create_schema() {
        sqlite3 *db = nullptr;
        ASSERT_EQ(SQLITE_OK, sqlite3_open_v2(PSM_DB_PATH, &db,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL));
        const char *ddl =
            "PRAGMA journal_mode=WAL;"
            "PRAGMA synchronous=NORMAL;"
            "CREATE TABLE IF NOT EXISTS psm_records ("
            "  name  TEXT    NOT NULL PRIMARY KEY,"
            "  type  INTEGER NOT NULL,"
            "  value TEXT    NOT NULL"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_psm_records_name"
            "  ON psm_records(name);";
        char *errmsg = nullptr;
        ASSERT_EQ(SQLITE_OK, sqlite3_exec(db, ddl, nullptr, nullptr, &errmsg))
            << (errmsg ? errmsg : "");
        sqlite3_free(errmsg);
        sqlite3_close(db);
    }

    void seed(const char *name, int type, const char *value) {
        sqlite3 *db = nullptr;
        ASSERT_EQ(SQLITE_OK, sqlite3_open(PSM_DB_PATH, &db));
        sqlite3_stmt *stmt = nullptr;
        ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO psm_records(name,type,value) VALUES(?1,?2,?3);",
            -1, &stmt, nullptr));
        sqlite3_bind_text(stmt, 1, name,  -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 2, type);
        sqlite3_bind_text(stmt, 3, value, -1, SQLITE_STATIC);
        ASSERT_EQ(SQLITE_DONE, sqlite3_step(stmt));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    void *bus_handle() { return &g_bus_info; }
};

/* =========================================================================
 * Task 4.1 — PSM_Get_Record_Value2 via SQLite
 * =========================================================================*/

TEST_F(PsmSqliteApiTest, GetRecordExists_ReturnsSuccessAndValue)
{
    seed("Device.Test.StringParam", ccsp_string, "hello_world");

    char *pValue = nullptr;
    unsigned int type = 0;
    int ret = PSM_Get_Record_Value2(bus_handle(), "eRT.",
                                    "Device.Test.StringParam", &type, &pValue);

    EXPECT_EQ(CCSP_SUCCESS, ret);
    ASSERT_NE(nullptr, pValue);
    EXPECT_STREQ("hello_world", pValue);
    EXPECT_EQ((unsigned int)ccsp_string, type);
    free(pValue);
}

TEST_F(PsmSqliteApiTest, GetRecordMissing_ReturnsNonSuccess)
{
    char *pValue = nullptr;
    unsigned int type = 0;
    int ret = PSM_Get_Record_Value2(bus_handle(), "eRT.",
                                    "Device.Does.Not.Exist", &type, &pValue);

    EXPECT_NE(CCSP_SUCCESS, ret);
    EXPECT_EQ(nullptr, pValue);
}

TEST_F(PsmSqliteApiTest, GetRecordDatabaseUnavailable_ReturnsNonSuccess)
{
    unlink(PSM_DB_PATH);   /* remove DB so connection fails */

    char *pValue = nullptr;
    unsigned int type = 0;
    int ret = PSM_Get_Record_Value2(bus_handle(), "eRT.",
                                    "Device.Test.Param", &type, &pValue);

    EXPECT_NE(CCSP_SUCCESS, ret);
    EXPECT_EQ(nullptr, pValue);
}

TEST_F(PsmSqliteApiTest, GetRecord_BooleanTrue_ReturnsStoredValue)
{
    /* PSM_TRUE is "1" — Get must return the raw stored value so callers
     * comparing against PSM_TRUE ("1") work correctly. */
    seed("Device.Bool.TrueParam", ccsp_boolean, "1");

    char *pValue = nullptr;
    unsigned int type = 0;
    int ret = PSM_Get_Record_Value2(bus_handle(), "eRT.",
                                    "Device.Bool.TrueParam", &type, &pValue);

    EXPECT_EQ(CCSP_SUCCESS, ret);
    ASSERT_NE(nullptr, pValue);
    EXPECT_STREQ("1", pValue);   /* raw stored value, as PSM_TRUE is defined */
    free(pValue);
}

TEST_F(PsmSqliteApiTest, GetRecord_BooleanFalse_ReturnsStoredValue)
{
    /* PSM_FALSE is "0" — Get must return the raw stored value. */
    seed("Device.Bool.FalseParam", ccsp_boolean, "0");

    char *pValue = nullptr;
    unsigned int type = 0;
    int ret = PSM_Get_Record_Value2(bus_handle(), "eRT.",
                                    "Device.Bool.FalseParam", &type, &pValue);

    EXPECT_EQ(CCSP_SUCCESS, ret);
    ASSERT_NE(nullptr, pValue);
    EXPECT_STREQ("0", pValue);   /* raw stored value, as PSM_FALSE is defined */
    free(pValue);
}

/* =========================================================================
 * Task 4.2 — PSM_Set_Record_Value2 via SQLite
 * =========================================================================*/

TEST_F(PsmSqliteApiTest, SetRecordNew_InsertSucceeds)
{
    int ret = PSM_Set_Record_Value2(bus_handle(), "eRT.",
                                    "Device.New.Param", ccsp_string, "inserted");
    EXPECT_EQ(CCSP_SUCCESS, ret);

    /* Verify via read-back */
    char *pValue = nullptr;
    unsigned int type = 0;
    ret = PSM_Get_Record_Value2(bus_handle(), "eRT.",
                                "Device.New.Param", &type, &pValue);
    ASSERT_EQ(CCSP_SUCCESS, ret);
    EXPECT_STREQ("inserted", pValue);
    free(pValue);
}

TEST_F(PsmSqliteApiTest, SetRecordExisting_UpdateSucceeds)
{
    seed("Device.Update.Param", ccsp_string, "old");

    int ret = PSM_Set_Record_Value2(bus_handle(), "eRT.",
                                    "Device.Update.Param", ccsp_string, "new");
    EXPECT_EQ(CCSP_SUCCESS, ret);

    char *pValue = nullptr;
    unsigned int type = 0;
    ret = PSM_Get_Record_Value2(bus_handle(), "eRT.",
                                "Device.Update.Param", &type, &pValue);
    ASSERT_EQ(CCSP_SUCCESS, ret);
    EXPECT_STREQ("new", pValue);
    free(pValue);
}

TEST_F(PsmSqliteApiTest, SetRecordInvalidBoolean_ReturnsInvalidParam)
{
    int ret = PSM_Set_Record_Value2(bus_handle(), "eRT.",
                                    "Device.Bool.Param", ccsp_boolean, "yes");
    EXPECT_EQ(CCSP_CR_ERR_INVALID_PARAM, ret);

    /* Confirm no row was written */
    char *pValue = nullptr;
    unsigned int type = 0;
    ret = PSM_Get_Record_Value2(bus_handle(), "eRT.",
                                "Device.Bool.Param", &type, &pValue);
    EXPECT_NE(CCSP_SUCCESS, ret);
    EXPECT_EQ(nullptr, pValue);
}

TEST_F(PsmSqliteApiTest, SetRecord_ValidBooleanPSM_TRUE_Succeeds)
{
    int ret = PSM_Set_Record_Value2(bus_handle(), "eRT.",
                                    "Device.Bool.T", ccsp_boolean, PSM_TRUE);
    EXPECT_EQ(CCSP_SUCCESS, ret);
}

TEST_F(PsmSqliteApiTest, SetRecord_ValidBooleanPSM_FALSE_Succeeds)
{
    int ret = PSM_Set_Record_Value2(bus_handle(), "eRT.",
                                    "Device.Bool.F", ccsp_boolean, PSM_FALSE);
    EXPECT_EQ(CCSP_SUCCESS, ret);
}

TEST_F(PsmSqliteApiTest, SetRecordDatabaseUnavailable_ReturnsNonSuccess)
{
    unlink(PSM_DB_PATH);

    int ret = PSM_Set_Record_Value2(bus_handle(), "eRT.",
                                    "Device.NoDb.Param", ccsp_string, "val");
    EXPECT_NE(CCSP_SUCCESS, ret);
}
