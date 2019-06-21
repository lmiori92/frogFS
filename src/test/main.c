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
#include "storage/stdio/file_storage.h"

/* Filesystem includes */
#include "frogfs.h"
#include "frogfs_assert.h"

const char* TEST_CONTENT = "Hello, World! This is a record stored in the FrogFS.";

uint8_t read_buffer[4096*2];

/* Some internal variables are exported here to perform some grey-box testing
 * and analyze internal structure state occasionally as a test expectation. */
extern t_s_frogfsram_record frogfs_RAM[FROGFS_MAX_RECORD_COUNT];

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

    printf("Formatting media\r\n");
    fserr = frogfs_format();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);
    fserr = frogfs_init();
    printf_frogfserror(fserr);
    FROGFS_ASSERT(fserr, FROGFS_ERR_OK);

    for (i = 0; i < FROGFS_MAX_RECORD_COUNT; i++)
    {

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

/* TODO tests to be implemented:
 * - write non-opened file
 * - read non-opened file
 * - write maximum storage record, read and finally try to add new record
 * - fragmentation test: worst case 1-byte data fragments (with initial EEPROM image)
 * - test frogfs_find_contiguous_space in A) filesystem empty B) filesystem full C) other mixed cases...
 */

int main(void)
{
    t_e_frogfs_error fserr;

    /* Initialize the stdio-file storage backend for FrogFS */
    file_storage_set_size(4U * 1024U);      /* 4KB */

    test_contiguous();
    test_reopen();
    test_contiguous_and_remove();
    test_contiguous_and_remove_at_end();
    test_record_limit();
    test_fragmentation();
    test_0_byte_record();

    fserr = storage_close();
    FROGFS_ASSERT_VERBOSE(fserr, FROGFS_ERR_OK, "assertion failed at closing the storage layer.");

    FROGFS_DEBUG_VERBOSE("test passed");

    return 0;
}
