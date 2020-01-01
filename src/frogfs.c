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

/**
 * The FrogFS is - as the name entitles - very simplistic
 * filesystem which is oriented at recording data
 * on (very) small memories like EEPROMs in the range of
 * KBs.
 *
 * The design focuses on the following points:
 *
 * 1)  Fast write is important, read can be a little slower.
 * 2)  Fragmentation shall be supported, to support deletion.
 * 3)  Filenames are not needed. Numeric index is fine.
 * 4)  An amount of 126 records (files) is more than enough.
 * 5)  A certain boot-time to sync filesystem to RAM is accepted.
 * 6)  Wear-leveling is not managed by the filesystem. It can be
 *     implemented in a lower or upper layer.
 *
 * Record
 *
 * <rec.type:rec index>|<data.type:MSB offset/size>|<LSB offset/size>
 * rec.type : 0->normal ; 1->fragment
 * data.type: 0->pointer; 1->size
 *
 * A fragment type record with pointer data type
 *      SHALL be found after data of a normal type record.
 *      SHALL be before either empty space or another record.
 *
 * A fragment type record with size data type
 *      SHALL be found as first record at the position pointed from pointer fragment.
 *      SHALL be after either empty space, end of data of a normal record or another.
 *      fragment (FUTURE: support >32K files and storages).
 *
 * A normal type record with size data type
 *      SHALL be before record data and indicates an entire record.
 *      SHALL indicate the size of the data.
 *      after it there SHALL be either empty space or another record.
 *      indicates start of new record (ALWAYS)
 *      after size bytes, another record shall be found if file is fragmented.
 *
 * Traversal of records at boot can be simply iterating records and looking for:
 * types normal-size.
 *
 * Fragments do not carry record index information: this is given by fragments
 * pointers.
 *
 *
 * RAM
 *
 *  The file allocation table is computed in RAM and there is not such thing on the
 *  storage.
 *  At boot, all the records are loaded so that it is known which records are existing
 *  and which are not and more importantly where they are starting.
 *
 *  Holes (performance ideas)
 *
 *  For performance, holes are mapped in RAM to speedup fragmentation space retrieval.
 *  There is a maximum number of holes after which another search shall be done.
 *  -> done at boot
 *  -> done periodically when no operations due.
 *
 */

/**
 *  Failure scenario of the design
 *
 * - if power is cut in the middle of READ:
 *    A) nothing happens
 * - if power is cut in the middle of WRITE:
 *    A) last written bytes could be lost
 *    B) record cannot be written further (implicitly closed)
 * - if power is cut in the middle of REMOVE:
 *    A) the record could be only partially deleted on the storage and therefore
 *       some fragments could take space without being released, ever, unless
 *       the disk is formatted.
 *
 *  How to implement safe guards:
 *
 *  - A) nothing to implement, robust already
 *  - A) nothing to implement, the design is not implementing such scenario.
 *       The application is responsible to e.g. write 2 records to contain redundant data.
 *    B) unless append is implemented, there is no workaround.
 *       The application could detect loss of data (see A) and therefore remove the record and
 *       re-write it.
 *  - A) The filesystem design does not cover that but at initialization time there could be
 *       a module performing checks if all records are stored. If not, remove data and metadata
 *       of unlinked fragments as they are stale.
 */

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

#define FROGFS_SIGNATURE               (0x66594C53UL)
#define FROGFS_VERSION                 (1)

/** Every index that is in RAM shall be increased for writing to disk first */
#define FROGFS_RECORD_INDEX_OFFSET(x)   ((x) + FROGFS_MIN_RECORD_INDEX_OFFSET)

/** Every index written to disk needs to be subtracted */
#define FROGFS_RECORD_INDEX(x)         (((x) & ~0x80U) - FROGFS_MIN_RECORD_INDEX_OFFSET)
#define FROGFS_RECORD_TYPE(x)          ((x >> 7U) & 0x1)
#define FROGFS_RECORD_DATA(x)          ((x >> 7U) & 0x1)
#define FROGFS_RECORD_POINTER(x,y,z)   ((uint16_t)((uint16_t)((uint16_t)(y & ~0x80U) << 8U) | (uint16_t)(z)))

#define FROGFS_RECORD_TYPE_NORMAL      (0U)
#define FROGFS_RECORD_TYPE_FRAGMENT    (1U)

#define FROGFS_RECORD_DATA_POINTER     (0U)
#define FROGFS_RECORD_DATA_SIZE        (1U)

/** The size in bytes that a record metadata information
 * occupies on the actual disk.
 */
#define FROGFS_RECORD_METADATA_SIZE    (3U)

/**
 * Helper macro that sets the error flag if the storage
 * return value is not OK (aka something happened in the storage layer)
 */
#define FROGFS_SET_IOERROR_FLAG(flag, retval)   ((flag) |= (((retval) != FROGFS_ERR_OK) ? true : false))
#define FROGFS_SET_NOSPACE_FLAG(flag, retval)   ((flag) |= (((retval) != FROGFS_ERR_NOSPACE) ? true : false))

t_s_frogfsram_record frogfs_RAM[FROGFS_MAX_RECORD_COUNT];

#warning "add in all methods a check that the requested offset is not outside physical size of the storage"
#warning "Always allow multiple reads if no erase or write operation is ongoing. First version: one file at a time only ?"

t_e_frogfs_error frogfs_format(void)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    uint8_t tmp[16];
    uint16_t disk_size;
    uint8_t to_write;

    (void)memset(tmp, 0, sizeof(tmp));

    retval = storage_seek(0);
    if (retval != FROGFS_ERR_OK)
    {
        return FROGFS_ERR_IO;
    }

    /* Erase all the disk */
    disk_size = storage_size();
    do
    {
        if (disk_size >= sizeof(tmp))
        {
            to_write = sizeof(tmp);
        }
        else
        {
            to_write = disk_size;
        }
        retval = storage_write(tmp, to_write);
        disk_size -= (uint8_t)to_write;
    } while ((retval == FROGFS_ERR_OK) && (disk_size > 0));

    if (retval == FROGFS_ERR_OK)
    {
        /* Prepare the signature and version */
        tmp[0] = (uint8_t)((uint32_t)(FROGFS_SIGNATURE      ) & 0xFFUL);
        tmp[1] = (uint8_t)((uint32_t)(FROGFS_SIGNATURE >> 8 ) & 0xFFUL);
        tmp[2] = (uint8_t)((uint32_t)(FROGFS_SIGNATURE >> 16) & 0xFFUL);
        tmp[3] = (uint8_t)((uint32_t)(FROGFS_SIGNATURE >> 24) & 0xFFUL);
        tmp[4] = FROGFS_VERSION;

        /* Go to the beginning of the storage */
        retval = storage_seek(0);

        if (retval == FROGFS_ERR_OK)
        {
            /* Write the header */
            retval = storage_write(tmp, 5);
        }
    }

    return retval;
}

t_e_frogfs_error frogfs_init(void)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    uint8_t tmp[5];
    uint16_t pointer;
    uint16_t pos_cur;
    uint8_t index;

    /* Erase the in-RAM allocation table */
    (void)memset(frogfs_RAM, 0, sizeof(frogfs_RAM));

    /* Go to the beginning of the storage */
    storage_seek(0);

    /* Read the header */
    retval = storage_read(tmp, 5);

    if (retval == FROGFS_ERR_OK)
    {
        if (((uint32_t)tmp[0] == (uint32_t)((FROGFS_SIGNATURE      ) & 0xFFUL)) &&
            ((uint32_t)tmp[1] == (uint32_t)((FROGFS_SIGNATURE >> 8 ) & 0xFFUL)) &&
            ((uint32_t)tmp[2] == (uint32_t)((FROGFS_SIGNATURE >> 16) & 0xFFUL)) &&
            ((uint32_t)tmp[3] == (uint32_t)((FROGFS_SIGNATURE >> 24) & 0xFFUL)) &&
            (tmp[4] == FROGFS_VERSION))
        {
            /* Version and magic match, hence we have a formatted drive */
            retval = FROGFS_ERR_OK;

            /* Read the file offset table */
            bool nil = false;
            do
            {
                //null_counter = 0;

                do
                {
                    retval = storage_read(tmp, 1);
                    if (tmp[0] != 0x00U)
                    {
                        /* Advance by 2 (Metadata is 3 bytes) and quit */
                        retval = storage_backtrack(1);
                        if (retval == FROGFS_ERR_OK)
                        {
                            retval = storage_read(tmp, 3);
                        }
                        break;
                    }
                } while (retval == FROGFS_ERR_OK);

                if (retval == FROGFS_ERR_OK)
                {
                    if (nil == false)
                    {
                        index = FROGFS_RECORD_INDEX(tmp[0]);

                        if (index > FROGFS_MAX_RECORD_COUNT)
                        {
                            FROGFS_DEBUG_VERBOSE("assertion failed. Record index out of range. %d", index);
                            retval = FROGFS_ERR_OUT_OF_RANGE;
                            break;
                        }

                        /* Extract the pointer value */
                        pointer = FROGFS_RECORD_POINTER(tmp[0], tmp[1], tmp[2]);

                        /* determine record type */
                        if ((FROGFS_RECORD_TYPE(tmp[0]) == FROGFS_RECORD_TYPE_NORMAL) &&
                            (FROGFS_RECORD_DATA(tmp[1]) == FROGFS_RECORD_DATA_SIZE) )
                        {
                            /* it is a Normal - Size record: indicates the start of a record */

                            if (index > FROGFS_MAX_RECORD_COUNT)
                            {
                                FROGFS_DEBUG_VERBOSE("assertion failed. Record index out of range. %d", index);
                                retval = FROGFS_ERR_OUT_OF_RANGE;
                                break;
                            }

                            /* record size of next bytes. Check if first occurrence.
                             * If it is, then save this as file-start offset. */
                            if (frogfs_RAM[index].offset == 0)
                            {
                                /* First time that record index has been encountered,
                                 * OR
                                 * Pointer is less than the saved pointer (occurs before i.e. frst entry) */
                                retval = storage_pos(&frogfs_RAM[index].offset);
                                frogfs_RAM[index].offset -= 3U;                  /* record offset is including the record block */
                            }
                            else
                            {
                                /* already saved, skip and go on */
                                FROGFS_DEBUG_VERBOSE("assertion failed. Cannot find two normal-size blocks for a record");
                                retval = FROGFS_ERR_OUT_OF_RANGE;
                                break;
                            }

                            retval = storage_advance(pointer);
                        }
                        else if ((FROGFS_RECORD_TYPE(tmp[0]) == FROGFS_RECORD_TYPE_FRAGMENT) &&
                                 (FROGFS_RECORD_DATA(tmp[1]) == FROGFS_RECORD_DATA_POINTER) )
                        {
                            /* It is a fragment-pointer */

                            /* just skip the record metadata, next will be something else */

                            if ((pointer >= storage_size()) || (pointer <= 5U))
                            {
                                FROGFS_DEBUG_VERBOSE("assertion failed. Pointer out of range. %d", pointer);
                                retval = FROGFS_ERR_OUT_OF_RANGE;
                                break;
                            }

                            retval = storage_advance(pointer);
                        }
                        else if ((FROGFS_RECORD_TYPE(tmp[0]) == FROGFS_RECORD_TYPE_FRAGMENT) &&
                                 (FROGFS_RECORD_DATA(tmp[1]) == FROGFS_RECORD_DATA_SIZE) )
                        {
                            /* It is a fragment-size */
                            retval = storage_advance(pointer);
                        }
                        else
                        {
                            /* record not supported. */
                            FROGFS_ASSERT_UNCHECKED("assertion failed. Invalid record found.\r\n");
                        }
                    }
                }
                else
                {
                    retval = storage_pos(&pos_cur);
                    if ((pos_cur + 3U) >= storage_size())
                    {
                        FROGFS_DEBUG_VERBOSE("end of storage reached,");
                        retval = FROGFS_ERR_OK;
                        break;
                    }
                }
            } while ((retval == FROGFS_ERR_OK) && (storage_end_of_storage() != FROGFS_ERR_OK));    // TILL EOF
        }
        else
        {
            /* The drive is not formatted */
            retval = FROGFS_ERR_NOT_FORMATTED;
        }
    }

    return retval;
}

/**
 * Find the contiguous space which has the following space requirements:
 * - at least 3 bytes plus 1 bytes data plus 3 bytes for an additional fragment pointer record.
 * - everything is zeroed i.e. it is free space.
 * The searching process takes into account space already occupied by other fragments.
 *
 */
t_e_frogfs_error frogfs_find_contiguous_space(uint16_t *space_start, uint16_t *data_start, uint16_t *data_size)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    uint8_t tmp[3];
    bool start_zero_find = false;
    uint16_t blank_cnt = 0U;
    uint16_t size_advance = 0U;

    /* Goto after the header */
    storage_seek(5);

    do
    {
        /* Reset flags here */
        start_zero_find = false;
        blank_cnt = 0;

        /* Read record metadata */
        retval = storage_read(tmp, 3U);

        if (retval == FROGFS_ERR_OK)
        {  /* No error, could ready fully */
            if ((tmp[0] == 0) || (tmp[1] == 0) || (tmp[2] == 0))
            {
                /* This is free space: it is not a metadata */
                /* Count already 3 bytes free */
                blank_cnt += FROGFS_RECORD_METADATA_SIZE;
                /* Continue looking for more free space */
                start_zero_find = true;
                /* Save the space address (potential) and remove the metadata size from it */
                retval = storage_pos(space_start);
                *space_start -= FROGFS_RECORD_METADATA_SIZE;
            }
            else
            {
                if (FROGFS_RECORD_DATA(tmp[1]) == FROGFS_RECORD_DATA_SIZE)
                {
                    /* Retrieve the size and ask the storage layer to advance by that amount */
                    size_advance = FROGFS_RECORD_POINTER(tmp[0], tmp[1], tmp[2]);
                    retval = storage_advance(size_advance);

                    if (retval == FROGFS_ERR_OK)
                    {
                        // TODO if the error is different here it means the filesystem suffers
                        // of rotten data...
                    }

                }
                else
                {
                    /* It is of pointer type, just restart iteration */
                }
            }
        }
        else
        {  /* partly not read due to out of physical space or IO error */
            /* Get out of the loop */
            break;
        }

        if (start_zero_find == true)
        {
            for (;;)    /* The loop is eventually terminated */
            {
                /* Storage read bytes and increment the null counter */
                retval = storage_read(tmp, 1U);

                if (retval == FROGFS_ERR_OK)
                {
                    if (tmp[0] == 0)
                    {
                        /* empty hole has been found */
                        blank_cnt++;
                    }
                    else
                    {
                        /* reset the blank counter */
                        blank_cnt = 0;
                        /* Go back by one position */
                        retval = storage_backtrack(1);
                        /* Exit the loop */
                        break;
                    }
                }
                else if (retval == FROGFS_ERR_NOSPACE)
                {
                    /* Out of space: exit the loop with the bytes that could be marked as free */
                    break;
                }
                else
                {
                    /* IO error or other error: exit the loop */
                    break;
                }
            }

            if (blank_cnt >= 7U)
            {
                /* We have enough space:
                 * 1 byte of actual data
                 * 3 bytes the record metadata
                 * 3 bytes for potential further fragmented data pointer
                 */
                /* The data write offset shall not count the record */
                *data_start = *space_start + 3U;
                /* Determine the size of the data */
                *data_size = (blank_cnt - 7U);

                FROGFS_DEBUG_VERBOSE("space found at 0x%04x", *space_start);
                FROGFS_DEBUG_VERBOSE("write offset set at 0x%04x", *data_start);
                FROGFS_DEBUG_VERBOSE("of size 0x%04x", *data_size);

                /* Override error: it is "ok" even if NOSPACE or other non fatal error
                 * e.g. I/O error on some bytes at the end of the storage while other bytes
                 * are correctly accessible -> go on and allocate as necessary. */
                // TODO: maybe an API to check "storage" sanity i.e. these uncaught / masked
                // errors could be interesting for an application demanding high reliability
                retval = FROGFS_ERR_OK;

                /* Finally, exit the loop */
                break;
            }
            else
            {
                blank_cnt = 0;
            }
        } /* End of zero-find condition */

    } while(1);

    return retval;
}

t_e_frogfs_error frogfs_list(uint8_t *list, uint8_t list_size, uint8_t *file_num)
{
    uint8_t i = 0;
    uint8_t list_i = 0;
    t_e_frogfs_error retval = FROGFS_ERR_OK;

    if ((list == NULL) || (file_num == NULL))
    {
        retval = FROGFS_ERR_NULL_POINTER;
    }
    else
    {
        *file_num = 0;

        for (i = 0; i < FROGFS_MAX_RECORD_COUNT; i++)
        {
            if (frogfs_RAM[i].offset != 0)
            {
                if (list_i < list_size)
                {
                    list[list_i] = i;
                    (*file_num)++;
                }
                list_i++;
            }
        }
    }


    return retval;
}

t_e_frogfs_error frogfs_get_available(uint8_t *record)
{
    uint8_t i = 0;
    t_e_frogfs_error retval = FROGFS_ERR_IO;

    if (record == NULL)
    {
        retval = FROGFS_ERR_NULL_POINTER;
    }
    else
    {
        retval = FROGFS_ERR_OUT_OF_RANGE;
        *record = UINT8_MAX;

        for (i = 0; i < FROGFS_MAX_RECORD_COUNT; i++)
        {
            if (frogfs_RAM[i].offset == 0)
            {
                retval = FROGFS_ERR_OK;
                *record = i;
                break;
            }
        }
    }

    return retval;
}

t_e_frogfs_error frogfs_open(uint8_t record)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    uint8_t tmp[3];

#ifdef FROGFS_FORCE_INIT_AT_EVERY_OPEN
    FROGFS_DEBUG_VERBOSE("Unit Testing Enabled. You shall not see that normally.");
    retval = frogfs_init();
    FROGFS_ASSERT_VERBOSE(retval, FROGFS_ERR_OK, "not ok that init does not work.");
#endif

    FROGFS_DEBUG_VERBOSE("%s: record %d", __FUNCTION__, record);

    /* Check if the file exists or not */
    if (record < FROGFS_MAX_RECORD_COUNT)
    {
        if (frogfs_RAM[record].offset > 0)
        {
            /* File exists. Can read. */
            retval = FROGFS_ERR_OK;
            frogfs_RAM[record].work_reg_1 = 0;       /* reset the block start pos */
            frogfs_RAM[record].work_reg_2 = 0;       /* reset the read pos */
            frogfs_RAM[record].write_offset = 0;
        }
        else
        {
            /* File does not exists. Create record */
            retval = frogfs_find_contiguous_space(&frogfs_RAM[record].offset, &frogfs_RAM[record].write_offset, &frogfs_RAM[record].work_reg_1);

            if (retval == FROGFS_ERR_OK)
            {
                /* Create the actual record: Normal - Size */
                tmp[0] = FROGFS_RECORD_INDEX_OFFSET(record) | (FROGFS_RECORD_TYPE_NORMAL << 7U);
                tmp[1] = (FROGFS_RECORD_DATA_SIZE << 7U);
                tmp[2] = 0;

                retval = storage_seek(frogfs_RAM[record].offset);
                if (retval == FROGFS_ERR_OK)
                {
                    /* Write */
                    retval = storage_write(tmp, 3);
                }
            }
            else
            {
                /* No Space (more likely happening) or IO error */
                FROGFS_DEBUG_VERBOSE("could not allocate spaced.");
                printf_frogfserror(retval);
            }
        }
    }
    else
    {
        FROGFS_DEBUG_VERBOSE("too large record %d", record);
        retval = FROGFS_ERR_INVALID_RECORD;
    }

    return retval;
}

// work_reg_1: available contiguous space
// work_reg_2: written size so far

t_e_frogfs_error frogfs_write(uint8_t record, const uint8_t *data, uint16_t size)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    uint8_t tmp[3];
    uint16_t tmp_size = 0;
    bool io_error = false;
    bool exit_loop = false;
    uint16_t written_bytes = 0;
    bool update_block_record = false;
    uint16_t space_start;
    uint16_t data_start;
    uint16_t data_size;

    FROGFS_DEBUG_VERBOSE("%s: record %d size %d", __FUNCTION__, record, size);

    /* Check if the file exists or not */
    if ((record < FROGFS_MAX_RECORD_COUNT) && (size <= FROGFS_MAX_RECORD_SIZE))
    {
        if (frogfs_RAM[record].write_offset == 0)
        {
            /* Not open for writing */
            retval = FROGFS_ERR_NOT_WRITABLE;
        }
        else
        {
            do
            {
                /* Goto write pointer plus the written size pointer */
                storage_seek((uint16_t)(frogfs_RAM[record].write_offset + frogfs_RAM[record].work_reg_2));

                /* Check what has to be done */
                if (written_bytes >= size)
                {
                    /* Last chunk of data has been written */
                    exit_loop = true;
                    /* update the block record's size */
                    update_block_record = true;
                    /* No error yet */
                    retval = FROGFS_ERR_OK;
                }
                else if (frogfs_RAM[record].work_reg_2 < frogfs_RAM[record].work_reg_1)
                {
                    /* The contiguous space is still available: continue writing */

                    /* determine how many bytes can we write in the contiguous space */
                    tmp_size = frogfs_RAM[record].work_reg_1 - frogfs_RAM[record].work_reg_2;
                    tmp_size = (size < tmp_size) ? size : tmp_size;
                    if (tmp_size >= written_bytes)  tmp_size -= written_bytes;

                    if (tmp_size > 0)
                    {
                        /* Fits the free space */
                        FROGFS_DEBUG_VERBOSE("contiguous write");

                        /* Write the portion of input data from written_bytes position of length tmp_size */
                        retval = storage_write(&data[written_bytes], tmp_size);

                        if (retval != FROGFS_ERR_OK)
                        {
                            /* IO-Error occurred */
                            io_error = true;
                            /* update the block record which shall cover what has been written so far without error */
                            exit_loop = true;
#warning "We shall update the metadata only when really required to avoid early storage wear."
                            update_block_record = true;
                        }
                        else
                        {
                            /* Update the block written size */
                            frogfs_RAM[record].work_reg_2 += tmp_size;
                            /* Increment the overall bytes counter */
                            written_bytes += tmp_size;
                        }
                    }
                    else
                    {  /* Plausibility error condition */
                        FROGFS_ASSERT_VERBOSE(0, 1, "shall never be zero here.");
                    }

                    if (frogfs_RAM[record].work_reg_2 >= frogfs_RAM[record].work_reg_1)
                    {
                        /* Update now the record, as the space has been filled */
                        update_block_record = true;
                    }

                }
                else if (frogfs_RAM[record].work_reg_2 >= frogfs_RAM[record].work_reg_1)
                {
                    /* The contiguous space has been filled completely: search new contiguous space */
                    retval = frogfs_find_contiguous_space(&space_start, &data_start, &data_size);

                    if (retval == FROGFS_ERR_OK)
                    {
                        /* Space found for stuffing the fragmented block */
                        /* Create the actual record: Fragment - Pointer and store the start address of the fragment */
                        tmp[0] = FROGFS_RECORD_INDEX_OFFSET(record) | (FROGFS_RECORD_TYPE_FRAGMENT << 7U);
                        tmp[1] = (FROGFS_RECORD_DATA_POINTER << 7U) | (uint8_t)(space_start >> 8U);
                        tmp[2] = (uint8_t)space_start;

                        retval = storage_seek((uint16_t)(frogfs_RAM[record].work_reg_1 + frogfs_RAM[record].write_offset));
                        if (retval == FROGFS_ERR_OK)
                        {
                            /* Write */
                            retval = storage_write(tmp, 3);
                        }

                        /* set the new write pointer to the write_offset */
                        frogfs_RAM[record].write_offset = data_start;        /* update the data write pointer */
                        frogfs_RAM[record].work_reg_1 = data_size;           /* update the free space available to the write operation */
                        frogfs_RAM[record].work_reg_2 = 0;                   /* reset the free space written bytes counter */
                        update_block_record = true;
                    }
                    else
                    {
                        /* Out of space, sorry */
                        retval = FROGFS_ERR_NOSPACE;
                        io_error = true;
                    }
                }

                /* Check if the block has to be updated now */
                if (update_block_record == true)
                {
                    /* Check if it is the first record block */
                    if (frogfs_RAM[record].offset == (uint16_t)(frogfs_RAM[record].write_offset - 3U))
                    {
                        /* Update the record size */
                        storage_seek(frogfs_RAM[record].write_offset - 3U);  /* record is situated 3 bytes before */
                        storage_read(tmp, 3U);

                        tmp_size = frogfs_RAM[record].work_reg_2;
                        tmp[1] = (tmp[1] & 0x80) | (uint8_t)(tmp_size >> 8U);
                        tmp[2] = (uint8_t)(tmp_size);

                        storage_seek(frogfs_RAM[record].write_offset - 3U);  /* record is situated 3 bytes before */
                        storage_write(tmp, 3U);
                    }
                    else
                    {
                        /* Update the record size */
                        storage_seek(frogfs_RAM[record].write_offset - 3U);  /* record is situated 3 bytes before */
                        storage_read(tmp, 3U);

                        tmp_size = frogfs_RAM[record].work_reg_2;

                        tmp[0] = (uint8_t)(FROGFS_RECORD_TYPE_FRAGMENT << 7U) | FROGFS_RECORD_INDEX_OFFSET(record);
                        tmp[1] = (uint8_t)((FROGFS_RECORD_DATA_SIZE << 7U)) | (uint8_t)(tmp_size >> 8U);
                        tmp[2] = (uint8_t)(tmp_size);

                        storage_seek(frogfs_RAM[record].write_offset - 3U);  /* record is situated 3 bytes before */
                        storage_write(tmp, 3U);
                    }
                }

            } while ((io_error == false) && (exit_loop == false));
        }
    }
    else
    {
        FROGFS_DEBUG_VERBOSE("too large record %d or size %d", record, size);
        retval = FROGFS_ERR_INVALID_RECORD;
    }

    return retval;
}

t_e_frogfs_error frogfs_close(uint8_t record)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;

    FROGFS_DEBUG_VERBOSE("%s: record %d", __FUNCTION__, record);

    /* Check if the file exists or not */
    if (record < FROGFS_MAX_RECORD_COUNT)
    {
        if (frogfs_RAM[record].write_offset > 0U)
        {
            /* File was being written to. Close it and clean registers. */
            frogfs_RAM[record].write_offset = 0;
            frogfs_RAM[record].work_reg_1   = 0;
            frogfs_RAM[record].work_reg_2   = 0;
            retval = FROGFS_ERR_OK;
        }
        else if (frogfs_RAM[record].work_reg_1 > 0U)
        {
            frogfs_RAM[record].write_offset = 0;
            frogfs_RAM[record].work_reg_1   = 0;
            frogfs_RAM[record].work_reg_2   = 0;
            retval = FROGFS_ERR_OK;
        }
        else if (frogfs_RAM[record].offset > 0U)
        {
            /* The file has only been opened but no operation has been performed. Just do nothing close. */
            retval = FROGFS_ERR_OK;
        }
        else
        {
            /* Not open neither for read nor for write */
            retval = FROGFS_ERR_INVALID_OPERATION;
        }
    }
    else
    {
        FROGFS_DEBUG_VERBOSE("too large record %d", record);
        retval = FROGFS_ERR_INVALID_RECORD;
    }

    return retval;
}

t_e_frogfs_error frogfs_erase_range(uint16_t pos, uint16_t size)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    uint16_t i;
    uint8_t tmp = 0;
    retval = storage_seek(pos);

    if (retval == FROGFS_ERR_OK)
    {
        for (i = 0; i < size; i++)
        {
            retval = storage_write(&tmp, 1);
            if (retval != FROGFS_ERR_OK)
            {
                break;
            }
        }
    }

    return retval;
}

// work_reg_1: block start
// work_reg_2: block size
t_e_frogfs_error frogfs_traverse(uint8_t record, uint8_t *data, uint16_t size, uint16_t *effective_read, bool erase)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    uint8_t tmp[3];
    uint16_t tmp_read_size = 0;
    bool io_error = false;
    bool exit_loop = false;
    uint8_t record_index;

    *effective_read = 0;

    FROGFS_DEBUG_VERBOSE("%s: record %d size %d", __FUNCTION__, record, size);

    /* Check if the file exists or not */
    if ((record < FROGFS_MAX_RECORD_COUNT) && (size <= FROGFS_MAX_RECORD_SIZE))
    {
        if (frogfs_RAM[record].write_offset != 0)
        {
            /* Open for writing */
            retval = FROGFS_ERR_NOT_READABLE;
        }
        else
        {
            /* Continue reading the required size until it is possible */
            do
            {
                if ((frogfs_RAM[record].work_reg_1 > 0) && (frogfs_RAM[record].work_reg_2 == UINT16_MAX))
                {
                    /* Seek to the position since operations on more records might be interleaved */
                    retval = storage_seek(frogfs_RAM[record].work_reg_1);

                    /* The current block has been fully read:
                     * - either it is the full record in a single block
                     * - it is followed by fragments
                     */
                    retval = storage_read(tmp, 3U);

                    /* decode the record index */
                    record_index = FROGFS_RECORD_INDEX(tmp[0]);

                    if (retval != FROGFS_ERR_OK)
                    {
                        io_error = true;
                    }
                    else if (record != record_index)
                    {
                        FROGFS_DEBUG_VERBOSE("Record block found but of different record index %d. Skip.", record_index);
                        exit_loop = true;
                    }
                    else
                    {
                        /* Check if it is a fragment */
                        if ((tmp[0] >> 7U) == FROGFS_RECORD_TYPE_FRAGMENT)
                        {
                            /* Fragment type */
                            FROGFS_DEBUG_VERBOSE("Fragment found. File read continues.");
                            /* Determine fragment type */
                            if ((tmp[1] >> 7U) == FROGFS_RECORD_DATA_SIZE)
                            {
                                /* Sized fragment */
                                FROGFS_DEBUG_VERBOSE("Sized fragment. Continue reading from %d", frogfs_RAM[record].work_reg_1);

                                retval = storage_pos(&frogfs_RAM[record].work_reg_1);                    /* save the data pointer */
                                frogfs_RAM[record].work_reg_2 = FROGFS_RECORD_POINTER(tmp[0], tmp[1], tmp[2]);  /* Index - MSB (remove the size type bit) - LSB */
                                FROGFS_DEBUG_VERBOSE("fragmented record size %d starting at %d", frogfs_RAM[record].work_reg_2, frogfs_RAM[record].work_reg_1);
                            }
                            else
                            {
                                /* Pointer fragment */
                                tmp_read_size = (uint16_t)(tmp[1] << 8U) | (uint16_t)tmp[2];
                                FROGFS_DEBUG_VERBOSE("Pointer fragment. Jump to %d", tmp_read_size);
// not necessary here                                retval = storage_seek(tmp_read_size);
                                frogfs_RAM[record].work_reg_1 = tmp_read_size;   /* save the pointer also in the working register 1 */
                                frogfs_RAM[record].work_reg_2 = UINT16_MAX;      /* still no data to read, maybe in the next fragment */

                                if (retval != FROGFS_ERR_OK)
                                {
                                    io_error = true;
                                }
                            }

                            /* check if erasing */
                            if (erase == true)
                            {
                                /* If so, then erase the record */
                                retval = storage_pos(&tmp_read_size);
                                retval = frogfs_erase_range((uint16_t)(tmp_read_size - 3U), 3U);

                                if (retval != FROGFS_ERR_OK)
                                {
                                    io_error = true;
                                }
                            }
                        }
                        else
                        {
                            /* Not a fragment. File read has been completed. */
                            FROGFS_DEBUG_VERBOSE("not a fragment. File read done.");
                            exit_loop = true;
                        }
                    }
                }
                else if (frogfs_RAM[record].work_reg_1 > 0)
                {
                    /* The file is already being read, continue */

                    /* move to the current read pointer */
                    retval = storage_seek(frogfs_RAM[record].work_reg_1);

                    if (retval != FROGFS_ERR_OK)
                    {
                        io_error = true;
                    }
                    else
                    {
                        if (erase == true)
                        {
                            /* when erasing, always erase the entire record */
                            tmp_read_size = frogfs_RAM[record].work_reg_2;

                            /* erase the whole length */
                            retval = frogfs_erase_range(frogfs_RAM[record].work_reg_1, frogfs_RAM[record].work_reg_2);
                        }
                        else
                        {
                            /* not erasing but reading */

                            /* read the data: min between block size and remaining data */
                            tmp_read_size = (size < frogfs_RAM[record].work_reg_2) ? size : frogfs_RAM[record].work_reg_2;

                            /* read from disk */
                            if (data != NULL)
                            {
                                retval = storage_read(&data[*effective_read], tmp_read_size);
                            }
                        }

                        /* advance the effective read counter */
                        *effective_read += tmp_read_size;

                        if (retval != FROGFS_ERR_OK)
                        {
                            io_error = true;
                        }
                        else
                        {
                            /* update the read pointer */
                            retval = storage_pos(&frogfs_RAM[record].work_reg_1);
                            /* update the read size */
                            frogfs_RAM[record].work_reg_2 -= tmp_read_size;

                            if (frogfs_RAM[record].work_reg_2 == 0)
                            {
                                FROGFS_DEBUG_VERBOSE("end of block. Setting read size to UINT16_MAX");
                                frogfs_RAM[record].work_reg_2 = UINT16_MAX;
                            }

                            if (retval != FROGFS_ERR_OK)
                            {
                                io_error = true;
                            }
                        }
                    }

                }
                else
                {
                    /* First read operation. Seek to the start normal record */

                    /* Goto record start pointer */
                    retval = storage_seek(frogfs_RAM[record].offset);

                    if (retval == FROGFS_ERR_OK)
                    {
                        /* Read the size of the record */
                        retval = storage_read(tmp, 3);

                        if (retval == FROGFS_ERR_OK)
                        {
                            retval = storage_pos(&frogfs_RAM[record].work_reg_1);                            /* save the data pointer */
                            frogfs_RAM[record].work_reg_2 = FROGFS_RECORD_POINTER(tmp[0], tmp[1], tmp[2]);   /* pick the block size from the metadata block */
                            FROGFS_DEBUG_VERBOSE("record size %d", frogfs_RAM[record].work_reg_2);
                        }

                        if (erase == true)
                        {
                            /* Erase the record */
                            retval = frogfs_erase_range(frogfs_RAM[record].offset, 3U);
                            /* fake the rsize, iterating until all the record has been traversed */
                            size = 0xFFFFU;
                        }
                    }
                }
            } while ((*effective_read < size) && (io_error == false) && (exit_loop == false));
        }
    }
    else
    {
        FROGFS_DEBUG_VERBOSE("too large record %d or size %d", record, size);
        retval = FROGFS_ERR_INVALID_RECORD;
    }

    return retval;
}

t_e_frogfs_error frogfs_read(uint8_t record, uint8_t *data, uint16_t size, uint16_t *effective_read)
{
    return frogfs_traverse(record, data, size, effective_read, false);
}

t_e_frogfs_error frogfs_erase(uint8_t record)
{
    t_e_frogfs_error retval;
    uint16_t effective_erased = 0;

    retval = frogfs_open(record);

    if (retval == FROGFS_ERR_OK)
    {
        retval = frogfs_traverse(record, NULL, 0, &effective_erased, true);

        if (retval == FROGFS_ERR_OK)
        {
            /* successful traversal and erasure */

            /* close the record */
            retval = frogfs_close(record);

            /* delete the record from the allocation table */
            frogfs_RAM[record].offset = 0U;
        }
    }

    return retval;
}

void printf_frogfserror(t_e_frogfs_error errno)
{
    switch(errno)
    {
        case FROGFS_ERR_OK:
            FROGFS_DEBUG_VERBOSE("%s: OK", __FUNCTION__);
            break;
        case FROGFS_ERR_NULL_POINTER:
            FROGFS_DEBUG_VERBOSE("%s: FROGFS_ERR_NULL_POINTER", __FUNCTION__);
            break;
        case FROGFS_ERR_IO:
            FROGFS_DEBUG_VERBOSE("%s: FROGFS_ERR_IO", __FUNCTION__);
            break;
        case FROGFS_ERR_NOT_FORMATTED:
            FROGFS_DEBUG_VERBOSE("%s: FROGFS_ERR_NOT_FORMATTED", __FUNCTION__);
            break;
        case FROGFS_ERR_INVALID_RECORD:
            FROGFS_DEBUG_VERBOSE("%s: FROGFS_ERR_INVALID_RECORD", __FUNCTION__);
            break;
        case FROGFS_ERR_NOSPACE:
            FROGFS_DEBUG_VERBOSE("%s: FROGFS_ERR_NOSPACE", __FUNCTION__);
            break;
        case FROGFS_ERR_NOT_WRITABLE:
            FROGFS_DEBUG_VERBOSE("%s: FROGFS_ERR_NOT_WRITABLE", __FUNCTION__);
            break;
        case FROGFS_ERR_NOT_READABLE:
            FROGFS_DEBUG_VERBOSE("%s: FROGFS_ERR_NOT_READABLE", __FUNCTION__);
            break;
        case FROGFS_ERR_INVALID_OPERATION:
            FROGFS_DEBUG_VERBOSE("%s: FROGFS_ERR_INVALID_OPERATION", __FUNCTION__);
            break;
        case FROGFS_ERR_OUT_OF_RANGE:
            FROGFS_DEBUG_VERBOSE("%s: FROGFS_ERR_OUT_OF_RANGE", __FUNCTION__);
            break;
        default:
            FROGFS_DEBUG_VERBOSE("%s: ERROR IN DECODING ERROR: %d", __FUNCTION__ , (uint8_t)errno);
            break;
    }
}
