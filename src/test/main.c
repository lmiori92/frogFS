/*
 *  Project     FrogFS
 *  @author     Lorenzo Miori
 *  @license    MIT - Copyright (c) 2019 Lorenzo Miori
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the \"Software\"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

// One wire library NO ARDUINO: https://github.com/MaJerle/onewire_uart/tree/master/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Storage includes */
#include "storage/storage_api.h"

/* Filesystem includes */
#include "frogfs.h"
#include "frogfs_assert.h"

const char* TEST_CONTENT = "Hello! This is FrogFS.";

uint8_t read_buffer[128U];

/* Some internal variables are exported here to perform some grey-box testing
 * and analyze internal structure state occasionally as a test expectation. */
extern t_s_frogfsram_record frogfs_RAM[FROGFS_MAX_RECORD_COUNT];

#warning "As debugging required some time, also prepare a test to check if zero-bytes chuncks can be flawlessly written"
#warning "...and not regarded as free storage"
/**
 * This test is used to verify that allocation of the maximum number of records
 * is successfully performed in a contiguous space (without fragmentation).
 * Sample data is written to each record and read back comparing to written data.
 * Test pass conditions are: no errors from frogfs calls; written content reads
 * back successfully and content compares equal.
 *
 * @return  0 (or asserts)
 */
int test_contiguous(void)
{
    t_e_frogfs_error fserr;
    uint16_t i = 0;
    uint16_t effective_read = 0;
    uint8_t next_record = 0;

    printf("Formatting media\r\n");
    fserr = frogfs_format();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    for (i = 0; i < FROGFS_MAX_RECORD_COUNT; i++)
    {
        /* Verify the behavior of frogfs_get_available */
        fserr = frogfs_get_available(&next_record);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
        FROGFS_ASSERT(next_record, i);

        /* Filesystem is ready: open record */
        fserr = frogfs_open(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Write to record */
        fserr = frogfs_write(i, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Close the record */
        fserr = frogfs_close(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Open the written record */
        fserr = frogfs_open(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = frogfs_read(i, read_buffer, sizeof(read_buffer), &effective_read);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Close the record */
        fserr = frogfs_close(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));

        FROGFS_ASSERT_VERBOSE(result, 0, "content does not match.");
        FROGFS_ASSERT_VERBOSE(effective_read, strlen(TEST_CONTENT), "length does not match.");
    }

    /* Check that listing the created records works as expected */
    uint8_t file_listing[FROGFS_MAX_RECORD_COUNT];
    uint8_t file_count = 0xFFU;  /* test that the variable is internally reset */
    fserr = frogfs_list(file_listing, sizeof(file_listing), &file_count);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    FROGFS_ASSERT(file_count, FROGFS_MAX_RECORD_COUNT);
    for (i = 0; i < FROGFS_MAX_RECORD_COUNT; i++)
    {
        FROGFS_ASSERT(i, file_listing[i]);
    }

    /* Verify the behavior of frogfs_get_available: no more records are available here */
    fserr = frogfs_get_available(&next_record);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OUT_OF_RANGE);
    FROGFS_ASSERT(next_record, UINT8_MAX);

    return 0;
}

/**
 * This test is replicating the same as @ref test_contiguous but it adds
 * record deletion immediately afterwards.
 * @return  0
 */
int test_contiguous_and_remove(void)
{
    t_e_frogfs_error fserr;
    uint16_t i = 0;
    uint16_t effective_read = 0;

    printf("Formatting media\r\n");
    fserr = frogfs_format();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    for (i = 0; i < FROGFS_MAX_RECORD_COUNT; i++)
    {
        /* Flush the file handle to disk at every step to enhance debugging */
        storage_sync();

        /* Filesystem is ready: open record */
        fserr = frogfs_open(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Write to record */
        fserr = frogfs_write(i, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Close the record */
        fserr = frogfs_close(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Open the written record */
        fserr = frogfs_open(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = frogfs_read(i, read_buffer, sizeof(read_buffer), &effective_read);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Close the record */
        fserr = frogfs_close(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));
        FROGFS_ASSERT_VERBOSE(result, 0, "content does not match.");
        FROGFS_ASSERT_VERBOSE(effective_read, strlen(TEST_CONTENT), "length does not match.");

        /* Remove the record */
        fserr = frogfs_erase(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
        /* Check that the allocation table has been erased */
        FROGFS_ASSERT(frogfs_RAM[i].offset, 0U);
    }

    return 0;
}

int test_contiguous_and_remove_at_end(void)
{
    t_e_frogfs_error fserr;
    uint16_t i = 0;
    uint16_t effective_read = 0;

    printf("Formatting media\r\n");
    fserr = frogfs_format();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    for (i = 0; i < FROGFS_MAX_RECORD_COUNT; i++)
    {
        /* Flush the file handle to disk at every step to enhance debugging */
        storage_sync();

        /* Filesystem is ready: open record */
        fserr = frogfs_open(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Write to record */
        fserr = frogfs_write(i, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Close the record */
        fserr = frogfs_close(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Open the written record */
        fserr = frogfs_open(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = frogfs_read(i, read_buffer, sizeof(read_buffer), &effective_read);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Close the record */
        fserr = frogfs_close(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));
        FROGFS_ASSERT_VERBOSE(result, 0, "content does not match.");
        FROGFS_ASSERT_VERBOSE(effective_read, strlen(TEST_CONTENT), "length does not match.");
    }

    for (i = 0; i < FROGFS_MAX_RECORD_COUNT; i++)
    {
        /* Flush the file handle to disk at every step to enhance debugging */
        storage_sync();

        /* Remove the record */
        fserr = frogfs_erase(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Check that the allocation table has been erased */
        FROGFS_ASSERT(frogfs_RAM[i].offset, 0U);
    }

    return 0;
}

// test that reads an existing partition and re-reads all the files
void test_reopen(void)
{
    t_e_frogfs_error fserr;
    uint8_t i = 0;
    uint16_t effective_read = 0;

    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    for (i = 0; i < FROGFS_MAX_RECORD_COUNT; i++)
    {
        /* Filesystem is ready: open record */
        fserr = frogfs_open(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = frogfs_read(i, read_buffer, sizeof(read_buffer), &effective_read);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Close the record */
        fserr = frogfs_close(i);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));
        FROGFS_ASSERT_VERBOSE(result, 0, "content does not match.");
        FROGFS_ASSERT_VERBOSE(effective_read, strlen(TEST_CONTENT), "length does not match.");
    }
}

void test_record_limit(void)
{
    t_e_frogfs_error fserr;
    uint16_t effective_read = 0;

    fserr = frogfs_open(FROGFS_MAX_RECORD_COUNT);
    FROGFS_ASSERT(FROGFS_ERR_INVALID_RECORD, fserr);

    fserr = frogfs_write(FROGFS_MAX_RECORD_COUNT, NULL, 0);
    FROGFS_ASSERT(FROGFS_ERR_INVALID_RECORD, fserr);

    fserr = frogfs_traverse(FROGFS_MAX_RECORD_COUNT, NULL, 0, &effective_read, false);
    FROGFS_ASSERT(FROGFS_ERR_INVALID_RECORD, fserr);

    fserr = frogfs_read(FROGFS_MAX_RECORD_COUNT, NULL, 0, &effective_read);
    FROGFS_ASSERT(FROGFS_ERR_INVALID_RECORD, fserr);

    fserr = frogfs_close(FROGFS_MAX_RECORD_COUNT);
    FROGFS_ASSERT(FROGFS_ERR_INVALID_RECORD, fserr);
}

void test_reopen_files(uint8_t index_record_start, uint8_t index_record_end)
{
    t_e_frogfs_error fserr;
    uint16_t effective_read = 0;

    for (;index_record_start <= index_record_end; index_record_start++)
    {
        /* Filesystem is ready: open record */
        fserr = frogfs_open(index_record_start);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
        storage_sync();

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = frogfs_read(index_record_start, read_buffer, sizeof(read_buffer), &effective_read);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));
        FROGFS_ASSERT_VERBOSE(result, 0, "content does not match.");
        FROGFS_ASSERT_VERBOSE(effective_read, strlen(TEST_CONTENT), "length does not match.");

        /* Close the record */
        fserr = frogfs_close(index_record_start);
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
        /* Flush the file handle to disk at every step to enhance debugging */
        storage_sync();
    }
}

/* Test fragmentation by creating 2 records of the same
 * size, erasing the first and trying to write another record
 */
void test_fragmentation()
{
    t_e_frogfs_error fserr;

    printf("Formatting media\r\n");
    fserr = frogfs_format();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    storage_sync();

    /* Filesystem is ready: open record 0 */
    fserr = frogfs_open(0);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    storage_sync();

    /* Write to record 0 */
    fserr = frogfs_write(0, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    storage_sync();

    /* Close the record 0 */
    fserr = frogfs_close(0);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    storage_sync();

    /* Filesystem is ready: open record 1 */
    fserr = frogfs_open(1);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    storage_sync();

    /* Write to record 1 */
    fserr = frogfs_write(1, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    storage_sync();

    /* Close the record 1 */
    fserr = frogfs_close(1);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    storage_sync();

    /* Remove record 0 */
    fserr = frogfs_erase(0);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    storage_sync();

    /* Filesystem is ready: open record 2 */
    fserr = frogfs_open(2);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    storage_sync();

    /* Write to record 2 */
    fserr = frogfs_write(2, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    storage_sync();

    /* Close the record 2 */
    fserr = frogfs_close(2);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    storage_sync();

    /* Re-read record 1 and 2 and verify their integrity */
    test_reopen_files(1, 2);

}

int test_0_byte_record(void)
{
    t_e_frogfs_error fserr;
    uint16_t effective_read = UINT16_MAX;

    printf("Formatting media\r\n");
    fserr = frogfs_format();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Filesystem is ready: open record */
    fserr = frogfs_open(0);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Write to record, 0 bytes */
    fserr = frogfs_write(0, (const uint8_t*)TEST_CONTENT, 0);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Close the record */
    fserr = frogfs_close(0);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Filesystem is ready: open record */
    fserr = frogfs_open(0);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Read: expect 0 bytes */
    (void)memset(read_buffer, 0, sizeof(read_buffer));
    fserr = frogfs_read(0, read_buffer, sizeof(read_buffer), &effective_read);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Close the record */
    fserr = frogfs_close(0);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Compare the read data */
    for(uint16_t i = 0; i < sizeof(read_buffer); i++)
    {
        FROGFS_ASSERT_VERBOSE(read_buffer[i], 0U, "content does not match.");
    }

    FROGFS_ASSERT_VERBOSE(effective_read, 0, "length does not match.");

    return 0;
}

/**
 * This test is used to verify the correct behavior if a file was open
 * but wasn't properly closed at shutdown.
 *
 * @return  0 (or asserts)
 */
int test_unclosed_file(void)
{
#define FILE_SETTINGS       (0U)
    t_e_frogfs_error fserr;

    printf("Formatting media\r\n");
    fserr = frogfs_format();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Filesystem is ready: open record */
    fserr = frogfs_open(FILE_SETTINGS);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Simulate power-cycle by re-init of frogfs */
    fserr = frogfs_init();
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    return 0;
}

/**
 * This test is used to verify the correct operation for the "Settings File"
 * use case i.e. a setting file which is always attempted to be read at the beginning
 * but for which no data exists yet.
 * Later, data is stored as well and a check is performed if the read data is still valid.
 *
 * @return  0 (or asserts)
 */
int test_use_case_settings(bool perform_format_and_init, bool check_first_open_zero_data)
{
#define FILE_SETTINGS       (0U)
    t_e_frogfs_error fserr;
    uint16_t effective_read = 0;
    typedef struct {
        uint8_t  demoVal0;
        uint32_t demoVal1;
        uint32_t demoVal2;
    } t_s_demo;
    t_s_demo demo_struct_write = { 0xAA, 0x1234, 0xABCD };
    t_s_demo demo_struct_read = { 0, 0, 0 };

    if (perform_format_and_init == true)
    {
        printf("Formatting media\r\n");
        fserr = frogfs_format();
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
        fserr = frogfs_init();
        printf_frogfserror(fserr);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    }

    /* Filesystem is ready: open record */
    fserr = frogfs_open(FILE_SETTINGS);
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_close(FILE_SETTINGS);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    if (perform_format_and_init == true)
    {
        /* Simulate power-cycle by re-init of frogfs */
        fserr = frogfs_init();
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    }

    if (check_first_open_zero_data == true)
    {
        /* Try to reload settings */
        fserr =  frogfs_read(FILE_SETTINGS, (uint8_t *)&demo_struct_read, sizeof(demo_struct_read), &effective_read);
        FROGFS_ASSERT((demo_struct_read.demoVal0 == 0 &&
                       demo_struct_read.demoVal1 == 0 &&
                       demo_struct_read.demoVal2 == 0) ? 1 : 0, 1);
        FROGFS_ASSERT(effective_read, 0);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
        fserr = frogfs_close(FILE_SETTINGS);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    }

    /* Save new settings */
    fserr = frogfs_erase(FILE_SETTINGS);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_open(FILE_SETTINGS);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_write(FILE_SETTINGS, (const uint8_t *)&demo_struct_write, sizeof(demo_struct_write));
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_close(FILE_SETTINGS);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    if (perform_format_and_init == true)
    {
        /* Simulate power-cycle by re-init of frogfs */
        fserr = frogfs_init();
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    }

    /* Try to reload settings */
    fserr = frogfs_open(FILE_SETTINGS);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr =  frogfs_read(FILE_SETTINGS, (uint8_t *)&demo_struct_read, sizeof(demo_struct_read), &effective_read);
    FROGFS_ASSERT((demo_struct_read.demoVal0 == demo_struct_write.demoVal0 &&
                   demo_struct_read.demoVal1 == demo_struct_write.demoVal1 &&
                   demo_struct_read.demoVal2 == demo_struct_write.demoVal2) ? 1 : 0, 1);
    FROGFS_ASSERT(effective_read, sizeof(demo_struct_write));
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_close(FILE_SETTINGS);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    return 0;
}

/**
 * This test is checking the behavior in a common situation that is observed
 * in the first application developed that makes use of the frogfs as storage backend.
 * @return 0 (or assertion (abort))
 */
int test_file0_and_file1(void)
{
    uint8_t i = 0;
    t_e_frogfs_error fserr;

    printf("Formatting media\r\n");
    fserr = frogfs_format();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Read - Erase - Write Record: 1st time */
    test_use_case_settings(false, false);
    /* Read - Erase - Write Record: 2nd time */
    test_use_case_settings(false, false);
    /* Simulate logging record: subsequent writes */

    /* Test and get the record name that is first available */
    uint8_t new_record;
    fserr = frogfs_get_available(&new_record);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_open(new_record);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    /* Write 128 bytes - byte by byte */
    for (i = 0; i < 128; i++)
    {
        fserr = frogfs_write(new_record, &i, 1U);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    }
    fserr = frogfs_close(new_record);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Simulate a power cycle */
    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    uint8_t read_buffer[128];
    uint16_t effective_read;

    /* Read back the data: in one go */
    fserr = frogfs_open(new_record);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_read(new_record, read_buffer, sizeof(read_buffer), &effective_read);
    FROGFS_ASSERT(effective_read, 128);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    for (i = 0; i < 128; i++)
    {
        FROGFS_ASSERT(read_buffer[i], i);
    }
    fserr = frogfs_close(new_record);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Read back the data: byte by byte */
    fserr = frogfs_open(new_record);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    for (i = 0; i < 128; i++)
    {
        fserr = frogfs_read(new_record, read_buffer, 1, &effective_read);
        FROGFS_ASSERT(effective_read, 1);
        FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
        FROGFS_ASSERT(read_buffer[i], i);
    }
    fserr = frogfs_close(new_record);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    /* Perform power cycle and repeat data read */

    return 0;
}

/* TODO tests to be implemented:
 * - write non-opened file
 * - read non-opened file
 * - write maximum storage record, read and finally try to add new record
 * - fragmentation test: worst case 1-byte data fragments (with initial EEPROM image)
 * - test frogfs_find_contiguous_space in A) filesystem empty B) filesystem full C) other mixed cases...
 */

int frogfs_execute_test(void)
{
    t_e_frogfs_error fserr;

    FROGFS_DEBUG_VERBOSE("test_contiguous");
    test_contiguous();
    FROGFS_DEBUG_VERBOSE("test_reopen");
    test_reopen();
    FROGFS_DEBUG_VERBOSE("test_contiguous_and_remove");
    test_contiguous_and_remove();
    FROGFS_DEBUG_VERBOSE("test_contiguous_and_remove_at_end");
    test_contiguous_and_remove_at_end();
    FROGFS_DEBUG_VERBOSE("test_record_limit");
    test_record_limit();
    FROGFS_DEBUG_VERBOSE("test_fragmentation");
    test_fragmentation();
    FROGFS_DEBUG_VERBOSE("test_0_byte_record");
    test_0_byte_record();
    FROGFS_DEBUG_VERBOSE("test_use_case_settings");
    test_use_case_settings(true, true);
    FROGFS_DEBUG_VERBOSE("test_unclosed_file");
    test_unclosed_file();
    FROGFS_DEBUG_VERBOSE("test_file0_and_file1");
    test_file0_and_file1();

    fserr = storage_close();
    FROGFS_ASSERT_VERBOSE(fserr, FROGFS_ERR_OK, "assertion failed at closing the storage layer.");

    FROGFS_DEBUG_VERBOSE("test passed");

    return 0;
}

#ifdef __linux__
#include "storage/stdio/file_storage.h"
/* Execute tests on a hosted linux platform */
int main(void)
{
    /* Initialize the stdio-file storage backend for FrogFS */
    file_storage_set_size(1U * 1024U);      /* 1KB */

    return frogfs_execute_test();
}
#endif
