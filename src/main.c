/**
 * The SillyFS is - as the name entitles - very simplistic
 * filesystem which is oriented at recording periodic data
 * on (very) small memories like EEPROMs in the range of
 * 1KB - 4KB.
 *
 * The design focuses on the following points:
 *
 * 1)  Fast write is important, read can be a little slower.
 * 2)  Fragmentation shall be supported, to support deletion.
 * 3)  Filenames are not needed. Numeric index is fine.
 * 4)  An amount of 127 records (files) is more than enough.
 * 5)  A certain boot-time to sync filesystem to RAM is accepted.
 * 6)  Wear-leveling is not managed by the filesystem. It can be
 *     implemented in a lower or upper layer.
 * 7)  read(record, offset) shall be implemented.
 * 8)  write(record, offset) shall be implemented.
 * 9)  open(record, mode) shall be implemented
 *     R: read
 *     W: write
 *     (Note.1)
 * (Note.1) append is not supported by design.
 *
 * 10) Sequential write and sequential read is only allowed to simplify
 *     the design.
 *
 * Append is not supported so that a "sequential" access is guaranteed thus
 * simplifying the design.
 * 1) If the file resides without fragments, it is read as-is
 * 2) If the file resides in multiple fragments and at the time of creation
 *    there was space *after* the first fragment, then further fragments will
 *    be *after* the current / main fragment.
 * 3) If the file resides in multiple fragments and at the time of creation
 *    there was no space *after*, then the storage is seek back from the
 *    beginning and more fragments will be placed there.
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

// One wire library NO ARDUINO: https://github.com/MaJerle/onewire_uart/tree/master/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define SILLYFS_SIGNATURE               (0x66594C53UL)
#define SILLYFS_VERSION                 (1)

#define SILLYFS_RECORD_INDEX(x)         ((x) & ~0x80U)
#define SILLYFS_RECORD_TYPE(x)          ((x >> 7U) & 0x1)
#define SILLYFS_RECORD_DATA(x)          ((x >> 7U) & 0x1)

#define SILLYFS_RECORD_TYPE_NORMAL      (0U)
#define SILLYFS_RECORD_TYPE_FRAGMENT    (1U)

#define SILLYFS_RECORD_DATA_POINTER     (0U)
#define SILLYFS_RECORD_DATA_SIZE        (1U)

#define SILLYFS_DEBUG_VERBOSE(...)      do { printf("line\t%d:\t", __LINE__); \
                                             printf(__VA_ARGS__);           \
                                             printf("\n");                  \
                                        } while(0);

#define SILLYFS_ASSERT(x,y,...)             do { uint32_t line = __LINE__;           \
                                                  if (x != y)                        \
                                                  {                                  \
                                                      printf("assertion failed at line %d: ", line); \
                                                      printf("was %08x, expected %08x", (uint32_t)x, (uint32_t)y);  \
                                                      printf("\n");                  \
                                                      exit(1);                       \
                                                  }                                  \
                                             }while(0);

#define SILLYFS_ASSERT_VERBOSE(x,y,...)      do { uint32_t line = __LINE__;          \
                                                  if (x != y)                        \
                                                  {                                  \
                                                      printf("assertion failed at line %d: ", line); \
                                                      printf(__VA_ARGS__);           \
                                                      printf("was %08x, expected %08x", (uint32_t)x, (uint32_t)y);  \
                                                      printf("\n");                  \
                                                      exit(1);                       \
                                                  }                                  \
                                             }while(0);

typedef struct
{
    uint8_t  index;         /**< Number of the record */
    uint16_t metadata;      /**< 15bit: record length; 1bit: record type */
} t_s_silly_record;

typedef enum
{
    SILLY_FS_ERR_OK,
    SILLY_FS_ERR_GENERIC,
    SILLY_FS_ERR_NOT_FORMATTED,
    SILLY_FS_ERR_INVALID_RECORD,
    SILLY_FS_ERR_NOSPACE,
    SILLY_FS_ERR_NOT_WRITABLE,
    SILLY_FS_ERR_NOT_READABLE,
    SILLY_FS_ERR_INVALID_OPERATION,
    SILLY_FS_ERR_OUT_OF_RANGE
} t_e_silly_error;

/** Maximum number of total records on the filesystem.
 *  Tune: adjust to match the RAM requirements for the application */
#define SILLYFS_MAX_RECORD_COUNT        (32U)
/** Maximum length of a single record. This is a hard limit that comes
 *  from the design of the filesystem. Anything below could work but has
 *  not meaning as records are dynamically allocated. */
#define SILLYFS_MAX_RECORD_SIZE         (32U*1024U)

typedef struct
{
    uint16_t offset;        /**< The allocation table of the first block of the record */

    uint16_t work_reg_1;    /**< Generic working register to support file operations.
                                 Meaning is documented for each function/module using it. */
    uint16_t work_reg_2;    /**< Generic working register to support file operations.
                                 Meaning is documented for each function/module using it. */
    uint16_t write_offset;  /**< Write pointer for write operations. If different from 0, then
                                 a record is open for writing */
} t_s_silly_ram_record;

t_s_silly_ram_record silly_RAM[SILLYFS_MAX_RECORD_COUNT];

// STORAGE HAL

#define EEPROM_SIZE     (4096)
uint8_t eeprom_image[EEPROM_SIZE];
FILE *eeprom_handle = NULL;

uint16_t storage_size(void)
{
    return EEPROM_SIZE;
}
#warning "add in all methods a check that the requested offset is not outside physical size of the storage"

#warning "ADD new design as follows"

#warning "Always allow multiple reads if no erase or write operation is ongoing. First version: one file at a time only ?"

/*
 * An internal function shall be defined that works to support:
 *  - read
 *  - remove
 *
 * Basically, this function is iterated on a record open for read (i.e. existing)
 * and it continuously returns a physical storage pointer that is used to access
 * data bytes AND control structure bytes (optionally, used for remove).
 * So the single code to traverse a file is shared across functionality.
 *
 * A read will simply read the data.
 * A remove will erase the data and the control structures so that the end result is empty
 * holes that can be filled with new records later on.
 *
 * */

t_e_silly_error storage_advance(uint16_t size)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    int fretval;

    fretval = fseek(eeprom_handle, size, SEEK_CUR);

    if (fretval == 0)
    {
        retval = SILLY_FS_ERR_OK;
    }

    return retval;
}

t_e_silly_error storage_pos(uint16_t *offset)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    int fretval;

    fretval = ftell(eeprom_handle);

    if (fretval != -1)
    {
        retval = SILLY_FS_ERR_OK;
        *offset = (uint16_t)fretval;
    }

    return retval;
}

t_e_silly_error storage_end_of_storage(void)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    int fretval;

    fretval = ftell(eeprom_handle);

    if (fretval == (EEPROM_SIZE-1))
    {
        retval = SILLY_FS_ERR_OK;
    }

    return retval;
}

t_e_silly_error storage_seek(uint16_t offset)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    int fretval;

    fretval = fseek(eeprom_handle, offset, SEEK_SET);

    if (fretval == 0)
    {
        retval = SILLY_FS_ERR_OK;
    }

    return retval;
}

t_e_silly_error storage_read(uint8_t *data, uint16_t size)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    int fretval;

    fretval = fread(data, 1, size, eeprom_handle);


    if (fretval == (int)size)
    {
        retval = SILLY_FS_ERR_OK;
    }

    return retval;
}

t_e_silly_error storage_write(const uint8_t *data, uint16_t size)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    int fretval;

    fretval = fwrite(data, 1, size, eeprom_handle);


    if (fretval == (int)size)
    {
        retval = SILLY_FS_ERR_OK;
    }

    return retval;
}

t_e_silly_error sillyfs_format(void)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    uint8_t tmp[16];
    uint16_t pos;

    (void)memset(tmp, 0, sizeof(tmp));

    /* Erase all the disk */
    uint16_t disk_size = storage_size();
    do
    {
        retval = storage_write(tmp, sizeof(tmp));
        if (disk_size > sizeof(tmp))
        {
            disk_size -= sizeof(tmp);
        }
        else
        {
            disk_size = 0U;
        }
        storage_pos(&pos);
    } while ((retval == SILLY_FS_ERR_OK) && (pos < (disk_size -1)));

    /* Prepare the signature and version */
    tmp[0] = (uint8_t)((uint32_t)(SILLYFS_SIGNATURE      ) & 0xFFUL);
    tmp[1] = (uint8_t)((uint32_t)(SILLYFS_SIGNATURE >> 8 ) & 0xFFUL);
    tmp[2] = (uint8_t)((uint32_t)(SILLYFS_SIGNATURE >> 16) & 0xFFUL);
    tmp[3] = (uint8_t)((uint32_t)(SILLYFS_SIGNATURE >> 24) & 0xFFUL);
    tmp[4] = SILLYFS_VERSION;

    /* Go to the beginning of the storage */
    retval = storage_seek(0);

    if (retval == SILLY_FS_ERR_OK)
    {
        /* Write the header */
        retval = storage_write(tmp, 5);
    }

    return retval;
}

bool sillyfs_is_nil(const uint8_t *data, uint16_t size)
{
    if (size >= 3)
    {
        /* Not too small, check */
        if (data[0] == 0 && data[1] == 0 && data[2] == 0)
        {
            /* definitely a nil */
            return true;
        }
        else
        {
            /* not a nil, something else */
            return false;
        }
    }
    else
    {
        /* Too small. It is nil for the filesystem. */
        return true;
    }
}

t_e_silly_error sillyfs_init(void)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    uint8_t tmp[5];
    uint16_t pointer;
    uint16_t pos_cur;
    uint8_t index;

    /* Erase the in-RAM allocation table */
    (void)memset(silly_RAM, 0, sizeof(silly_RAM));

    /* Go to the beginning of the storage */
    storage_seek(0);

    /* Read the header */
    retval = storage_read(tmp, 5);

    if (retval == SILLY_FS_ERR_OK)
    {
        if (((uint32_t)tmp[0] == (uint32_t)((SILLYFS_SIGNATURE      ) & 0xFFUL)) &&
            ((uint32_t)tmp[1] == (uint32_t)((SILLYFS_SIGNATURE >> 8 ) & 0xFFUL)) &&
            ((uint32_t)tmp[2] == (uint32_t)((SILLYFS_SIGNATURE >> 16) & 0xFFUL)) &&
            ((uint32_t)tmp[3] == (uint32_t)((SILLYFS_SIGNATURE >> 24) & 0xFFUL)) &&
            (tmp[4] == SILLYFS_VERSION))
        {
            /* Version and magic match, hence we have a formatted drive */
            retval = SILLY_FS_ERR_OK;

            /* Read the file offset table */
            bool nil;
            do
            {
                retval = storage_read(tmp, 3);
                if (retval == SILLY_FS_ERR_OK)
                {
                    nil = sillyfs_is_nil(tmp, sizeof(tmp));

                    if (nil == false)
                    {
                        index = SILLYFS_RECORD_INDEX(tmp[0]);

                        if (index > SILLYFS_MAX_RECORD_COUNT)
                        {
                            SILLYFS_DEBUG_VERBOSE("assertion failed. Record index out of range. %d", index);
                            retval = SILLY_FS_ERR_OUT_OF_RANGE;
                            break;
                        }

                        /* determine record type */
                        if ((SILLYFS_RECORD_TYPE(tmp[0]) == SILLYFS_RECORD_TYPE_NORMAL) &&
                            (SILLYFS_RECORD_DATA(tmp[1]) == SILLYFS_RECORD_DATA_SIZE) )
                        {
                            /* it is a Normal - Size record: indicates the start of a record */

                            pointer = (uint16_t)((uint16_t)(tmp[1] & ~0x80U) << 8U);
                            pointer |= (uint16_t)(tmp[2]);

                            if (index > SILLYFS_MAX_RECORD_COUNT)
                            {
                                SILLYFS_DEBUG_VERBOSE("assertion failed. Record index out of range. %d", index);
                                retval = SILLY_FS_ERR_OUT_OF_RANGE;
                                break;
                            }
                            if ((pointer >= storage_size()) || (pointer <= 5U))
                            {
                                SILLYFS_DEBUG_VERBOSE("assertion failed. Pointer out of range. %d", pointer);
                                retval = SILLY_FS_ERR_OUT_OF_RANGE;
                                break;
                            }

                            /* record size of next bytes. Check if first occurrence.
                             * If it is, then save this as file-start offset. */
                            if (silly_RAM[index].offset == 0)
                            {
                                /* First time that record index has been encountered,
                                 * OR
                                 * Pointer is less than the saved pointer (occurs before i.e. frst entry) */
                                retval = storage_pos(&silly_RAM[index].offset);
                                silly_RAM[index].offset -= 3U;                  /* record offset is including the record block */
                            }
                            else
                            {
                                /* already saved, skip and go on */
                                SILLYFS_DEBUG_VERBOSE("assertion failed. Cannot find two normal-size blocks for a record");
                                retval = SILLY_FS_ERR_OUT_OF_RANGE;
                                break;
                            }

                            retval = storage_advance(pointer);
                        }
                        else if ((SILLYFS_RECORD_TYPE(tmp[0]) == SILLYFS_RECORD_TYPE_FRAGMENT) &&
                                 (SILLYFS_RECORD_DATA(tmp[1]) == SILLYFS_RECORD_DATA_POINTER) )
                        {
                            /* It is a fragment-pointer */

                            /* just skip the record metadata, next will be something else */
                        }
                        else if ((SILLYFS_RECORD_TYPE(tmp[0]) == SILLYFS_RECORD_TYPE_FRAGMENT) &&
                                 (SILLYFS_RECORD_DATA(tmp[1]) == SILLYFS_RECORD_DATA_SIZE) )
                        {
                            /* It is a fragment-size */

                            /* skip the record data */
                            pointer = (uint16_t)(tmp[1] << 8);
                            pointer |= (uint16_t)(tmp[2]);
                            retval = storage_advance(pointer);
                        }
                        else
                        {
                            /* record not supported. */
                            printf("assertion failed. Invalid record found.\r\n");
                            exit(1);
                        }
                    }
                }
                else
                {
                    retval = storage_pos(&pos_cur);
                    if ((pos_cur + 3U) >= storage_size())
                    {
                        SILLYFS_DEBUG_VERBOSE("end of storage reached,");
                        retval = SILLY_FS_ERR_OK;
                        break;
                    }
                }
            } while ((retval == SILLY_FS_ERR_OK) && (storage_end_of_storage() != SILLY_FS_ERR_OK));    // TILL EOF
        }
        else
        {
            /* The drive is not formatted */
            retval = SILLY_FS_ERR_NOT_FORMATTED;
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
t_e_silly_error sillyfs_find_contiguous_space(uint16_t *space_start, uint16_t *data_start, uint16_t *data_size)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    uint8_t tmp;
    bool created_ok = false;
    bool io_error = false;
    uint16_t blank_cnt = 0;

    /* Goto after the header */
    storage_seek(5);

    /* Find the first NIL position */
     do
     {
         retval = storage_read(&tmp, 1U);

         if (retval == SILLY_FS_ERR_OK)
         {
             if (tmp == 0)
             {
                 /* empty hole has been found */
                 blank_cnt++;
             }
             else
             {
                 /* reset the blank counter */
                 blank_cnt = 0;
             }

             if ((blank_cnt >= 7U) && (created_ok == false))
             {
                 /* 3bytes for record + 1 byte min record data + 3 bytes for
                  * fragmentation pointer */
                 retval = storage_pos(space_start);

                 if (retval != SILLY_FS_ERR_OK)
                 {
                     /* Write error */
                     io_error = true;
                     break;
                 }

                 /* Seeked space is at least 7 bytes ahead. Rewind */
                 *space_start      -= 7U;
                 /* The data write offset shall not count the record */
                 *data_start = *space_start + 3U;

                 SILLYFS_DEBUG_VERBOSE("space found at 0x%04x", *space_start);
                 SILLYFS_DEBUG_VERBOSE("write offset set at 0x%04x", *data_start);

                 /* Record is created. */
                 created_ok = true;
             }
             else if ( ((blank_cnt == 0) || (storage_end_of_storage() == SILLY_FS_ERR_OK)) &&
                        (created_ok == true)
                     )
             {
                 /* Done allocating new file */
                 SILLYFS_DEBUG_VERBOSE("allocation of file for writing %d contiguous bytes", *data_size);
                 break;
             }
             else if ((blank_cnt >= 7) && (created_ok == true))
             {
                 /* Keep counting the hole for a faster writing */
                 *data_size = blank_cnt - 6U;                  /* we want at least 1 byte data to be used plus 2x record size */
             }
         }
         else
         {
             /* Write error */
             io_error = true;
             break;
         }
     } while (retval == SILLY_FS_ERR_OK);    // TILL EOF

     if (created_ok == true)
     {
         retval = SILLY_FS_ERR_OK;
     }
     else if (io_error == true)
     {
         /* retval contains the latest io-error */
         SILLYFS_DEBUG_VERBOSE("storage error %d", retval);
     }
     else
     {
         SILLYFS_DEBUG_VERBOSE("storage is full or too fragmented");
         retval = SILLY_FS_ERR_NOSPACE;
     }

    return retval;
}

t_e_silly_error sillyfs_open(uint8_t record)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    uint8_t tmp[3];

    SILLYFS_DEBUG_VERBOSE("%s: record %d", __FUNCTION__, record);

    /* Check if the file exists or not */
    if (record < SILLYFS_MAX_RECORD_COUNT)
    {
        if (silly_RAM[record].offset > 0)
        {
            /* File exists. Can read. */
            retval = SILLY_FS_ERR_OK;
            silly_RAM[record].work_reg_1 = 0;       /* reset the block start pos */
            silly_RAM[record].work_reg_2 = 0;       /* reset the read pos */
            silly_RAM[record].write_offset = 0;
        }
        else
        {
            /* File does not exists. Create record */
            retval = sillyfs_find_contiguous_space(&silly_RAM[record].offset, &silly_RAM[record].write_offset, &silly_RAM[record].work_reg_1);

            if (retval == SILLY_FS_ERR_OK)
            {
                /* Create the actual record: Normal - Size */
                tmp[0] = record | (SILLYFS_RECORD_TYPE_NORMAL << 7U);
                tmp[1] = (SILLYFS_RECORD_DATA_SIZE << 7U);
                tmp[2] = 0;

                retval = storage_seek(silly_RAM[record].offset);
                if (retval == SILLY_FS_ERR_OK)
                {
                    /* Write */
                    retval = storage_write(tmp, 3);
                }
            }
        }
    }
    else
    {
        SILLYFS_DEBUG_VERBOSE("too large record %d", record);
        retval = SILLY_FS_ERR_INVALID_RECORD;
    }

    return retval;
}

// work_reg_1: available contiguous space
// work_reg_2: written size so far

t_e_silly_error sillyfs_write(uint8_t record, const uint8_t *data, uint16_t size)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    uint8_t tmp[3];
    uint16_t tmp_size = 0;
    bool io_error = false;
    bool exit_loop = false;
    uint16_t written_bytes = 0;
    bool update_block_record = false;

    SILLYFS_DEBUG_VERBOSE("%s: record %d size %d", __FUNCTION__, record, size);

    /* Check if the file exists or not */
    if ((record < SILLYFS_MAX_RECORD_COUNT) && (size < SILLYFS_MAX_RECORD_SIZE))
    {
        if (silly_RAM[record].write_offset == 0)
        {
            /* Not open for writing */
            retval = SILLY_FS_ERR_NOT_WRITABLE;
        }
        else
        {
            do
            {
                /* Goto write pointer plus the written size pointer */
                storage_seek((uint16_t)(silly_RAM[record].write_offset + silly_RAM[record].work_reg_2));

                /* Check what has to be done */
                if (written_bytes >= size)
                {
                    /* Last chunk of data has been written */
                    exit_loop = true;
                    /* update the block record's size */
                    update_block_record = true;
                    /* No error yet */
                    retval = SILLY_FS_ERR_OK;
                }
                else if (silly_RAM[record].work_reg_2 < silly_RAM[record].work_reg_1)
                {
                    /* The contiguous space is still available: continue writing */

                    /* determine how many bytes can we write in the contiguous space */
                    tmp_size = silly_RAM[record].work_reg_1 - silly_RAM[record].work_reg_2;
                    tmp_size = (size < tmp_size) ? size : tmp_size;
                    if (tmp_size >= written_bytes)  tmp_size -= written_bytes;

                    if (tmp_size > 0)
                    {
                        /* Fits the free space */
                        SILLYFS_DEBUG_VERBOSE("contiguous write");

                        /* Write the portion of input data from written_bytes position of length tmp_size */
                        retval = storage_write(&data[written_bytes], tmp_size);

                        if (retval != SILLY_FS_ERR_OK)
                        {
                            /* IO-Error occurred */
                            io_error = true;
                            /* update the block record which shall cover what has been written so far without error */
                            exit_loop = true;
                            update_block_record = true;
                        }
                        else
                        {
                            /* Update the block written size */
                            silly_RAM[record].work_reg_2 += tmp_size;
                            /* Increment the overall bytes counter */
                            written_bytes += tmp_size;
                        }
                    }
                    else
                    {  /* Plausibility error condition */
                        SILLYFS_ASSERT_VERBOSE(0, 1, "shall never be zero here.");
                    }

                    if (silly_RAM[record].work_reg_2 >= silly_RAM[record].work_reg_1)
                    {
                        /* Update now the record, as the space has been filled */
                        update_block_record = true;
                    }

                }
                else if (silly_RAM[record].work_reg_2 >= silly_RAM[record].work_reg_1)
                {
                    /* The contiguous space has been filled completely: search new contiguous space */
#warning "TODO: optimize and check return values"
                    uint16_t space_start;
                    uint16_t data_start;
                    uint16_t data_size;

                    retval = sillyfs_find_contiguous_space(&space_start, &data_start, &data_size);

                    if (retval == SILLY_FS_ERR_OK)
                    {
                        /* Space found for stuffing the fragmented block */
                        /* Create the actual record: Fragment - Pointer and store the start address of the fragment */
                        tmp[0] = record | (SILLYFS_RECORD_TYPE_FRAGMENT << 7U);
                        tmp[1] = (SILLYFS_RECORD_DATA_POINTER << 7U) | (uint8_t)(space_start >> 8U);
                        tmp[2] = (uint8_t)space_start;

                        retval = storage_seek((uint16_t)(silly_RAM[record].work_reg_1 + silly_RAM[record].write_offset));
                        if (retval == SILLY_FS_ERR_OK)
                        {
                            /* Write */
                            retval = storage_write(tmp, 3);
                        }

                        /* set the new write pointer to the write_offset */
                        silly_RAM[record].write_offset = data_start;        /* update the data write pointer */
                        silly_RAM[record].work_reg_1 = data_size;           /* update the free space available to the write operation */
                        silly_RAM[record].work_reg_2 = 0;                   /* reset the free space written bytes counter */
                        update_block_record = true;
                    }
                    else
                    {
                        /* Out of space, sorry */
                        retval = SILLY_FS_ERR_NOSPACE;
                        io_error = true;
                    }
                }

                /* Check if the block has to be updated now */
#warning "TODO: se offset != write_offset -3u allora stiamo scrivendo su un blocco fragmentato"
                if (update_block_record == true)
                {
                    /* Check if it is the first record block */
                    if (silly_RAM[record].offset == (uint16_t)(silly_RAM[record].write_offset - 3U))
                    {
                        /* Update the record size */
                        storage_seek(silly_RAM[record].write_offset - 3U);  /* record is situated 3 bytes before */
                        storage_read(tmp, 3U);

                        tmp_size = silly_RAM[record].work_reg_2;
                        tmp[1] = (tmp[1] & 0x80) | (uint8_t)(tmp_size >> 8U);
                        tmp[2] = (uint8_t)(tmp_size);

                        storage_seek(silly_RAM[record].write_offset - 3U);  /* record is situated 3 bytes before */
                        storage_write(tmp, 3U);
                    }
                    else
                    {
                        /* Update the record size */
                        storage_seek(silly_RAM[record].write_offset - 3U);  /* record is situated 3 bytes before */
                        storage_read(tmp, 3U);

                        tmp_size = silly_RAM[record].work_reg_2;

                        tmp[0] = (uint8_t)(SILLYFS_RECORD_TYPE_FRAGMENT << 7U) | record;
                        tmp[1] = (uint8_t)((SILLYFS_RECORD_DATA_SIZE << 7U)) | (uint8_t)(tmp_size >> 8U);
                        tmp[2] = (uint8_t)(tmp_size);

                        storage_seek(silly_RAM[record].write_offset - 3U);  /* record is situated 3 bytes before */
                        storage_write(tmp, 3U);
                    }
                }

            } while ((io_error == false) && (exit_loop == false));
        }
    }
    else
    {
        SILLYFS_DEBUG_VERBOSE("too large record %d or size %d", record, size);
        retval = SILLY_FS_ERR_INVALID_RECORD;
    }

    return retval;
}

t_e_silly_error sillyfs_close(uint8_t record)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;

    SILLYFS_DEBUG_VERBOSE("%s: record %d", __FUNCTION__, record);

    /* Check if the file exists or not */
    if (record < SILLYFS_MAX_RECORD_COUNT)
    {
        if (silly_RAM[record].write_offset > 0U)
        {
            /* File was being written to. Close it and clean registers. */
            silly_RAM[record].write_offset = 0;
            silly_RAM[record].work_reg_1   = 0;
            silly_RAM[record].work_reg_2   = 0;
            retval = SILLY_FS_ERR_OK;
        }
        else if (silly_RAM[record].work_reg_1 > 0U)
        {
            silly_RAM[record].write_offset = 0;
            silly_RAM[record].work_reg_1   = 0;
            silly_RAM[record].work_reg_2   = 0;
            retval = SILLY_FS_ERR_OK;
        }
        else
        {
            /* Not open neither for read nor for write */
            retval = SILLY_FS_ERR_INVALID_OPERATION;
        }
    }
    else
    {
        SILLYFS_DEBUG_VERBOSE("too large record %d", record);
        retval = SILLY_FS_ERR_INVALID_RECORD;
    }

    return retval;
}

t_e_silly_error sillyfs_erase_range(uint16_t pos, uint16_t size)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    uint16_t i;
    uint8_t tmp = 0;
    retval = storage_seek(pos);

    if (retval == SILLY_FS_ERR_OK)
    {
        for (i = 0; i < size; i++)
        {
            retval = storage_write(&tmp, 1);
            if (retval != SILLY_FS_ERR_OK)
            {
                break;
            }
        }
    }

    return retval;
}

// work_reg_1: block start
// work_reg_2: block size
t_e_silly_error sillyfs_traverse(uint8_t record, uint16_t rsize, uint8_t *data, uint16_t size, bool erase)
{
    t_e_silly_error retval = SILLY_FS_ERR_GENERIC;
    uint8_t tmp[3];
    uint16_t effective_read = 0;
    uint16_t tmp_read_size = 0;
    bool io_error = false;
    bool exit_loop = false;
    uint8_t record_index;

    SILLYFS_DEBUG_VERBOSE("%s: record %d size %d", __FUNCTION__, record, size);

    /* Check if the file exists or not */
    if ((record < SILLYFS_MAX_RECORD_COUNT) && (size < SILLYFS_MAX_RECORD_SIZE))
    {
        if (silly_RAM[record].write_offset != 0)
        {
            /* Open for writing */
            retval = SILLY_FS_ERR_NOT_READABLE;
        }
        else
        {
            /* Continue reading the required size until it is possible */
            do
            {
                if ((silly_RAM[record].work_reg_1 > 0) && (silly_RAM[record].work_reg_2 == UINT16_MAX))
                {
                    /* Seek to the position since operations on more records might be interleaved */
                    retval = storage_seek(silly_RAM[record].work_reg_1);

                    /* The current block has been fully read:
                     * - either it is the full record in a single block
                     * - it is followed by fragments
                     */
                    retval = storage_read(tmp, 3U);

                    /* decode the record index */
                    record_index = SILLYFS_RECORD_INDEX(tmp[0]);

                    if (retval != SILLY_FS_ERR_OK)
                    {
                        io_error = true;
                    }
                    else if (record != record_index)
                    {
                        SILLYFS_DEBUG_VERBOSE("Record block found but of different record index %d. Skip.", record_index);
                        exit_loop = true;
                    }
                    else
                    {
                        /* Check if it is a fragment */
                        if ((tmp[0] >> 7U) == SILLYFS_RECORD_TYPE_FRAGMENT)
                        {
                            /* Fragment type */
                            SILLYFS_DEBUG_VERBOSE("Fragment found. File read continues.");
                            /* Determine fragment type */
                            if ((tmp[1] >> 7U) == SILLYFS_RECORD_DATA_SIZE)
                            {
                                /* Sized fragment */
                                SILLYFS_DEBUG_VERBOSE("Sized fragment. Continue reading from %d", silly_RAM[record].work_reg_1);

                                retval = storage_pos(&silly_RAM[record].work_reg_1);                    /* save the data pointer */
                                silly_RAM[record].work_reg_2 = tmp[2];                                  /* LSB */
                                silly_RAM[record].work_reg_2 |= (uint16_t)((tmp[1] & ~0x80U << 8U));    /* MSB (remove the size type bit) */
                                SILLYFS_DEBUG_VERBOSE("fragmented record size %d starting at %d", silly_RAM[record].work_reg_2, silly_RAM[record].work_reg_1);
                            }
                            else
                            {
                                /* Pointer fragment */
                                tmp_read_size = (uint16_t)(tmp[1] << 8U) | (uint16_t)tmp[2];
                                SILLYFS_DEBUG_VERBOSE("Pointer fragment. Jump to %d", tmp_read_size);
// not necessary here                                retval = storage_seek(tmp_read_size);
                                silly_RAM[record].work_reg_1 = tmp_read_size;   /* save the pointer also in the working register 1 */
                                silly_RAM[record].work_reg_2 = UINT16_MAX;      /* still no data to read, maybe in the next fragment */

                                if (retval != SILLY_FS_ERR_OK)
                                {
                                    io_error = true;
                                }
                            }

                            /* check if erasing */
                            if (erase == true)
                            {
                                /* If so, then erase the record */
                                retval = storage_pos(&tmp_read_size);
                                retval = sillyfs_erase_range((uint16_t)(tmp_read_size - 3U), 3U);

                                if (retval != SILLY_FS_ERR_OK)
                                {
                                    io_error = true;
                                }
                            }
                        }
                        else
                        {
                            /* Not a fragment. File read has been completed. */
                            SILLYFS_DEBUG_VERBOSE("not a fragment. File read done.");
                            exit_loop = true;
                        }
                    }
                }
                else if (silly_RAM[record].work_reg_1 > 0)
                {
                    /* The file is already being read, continue */

                    /* move to the current read pointer */
                    retval = storage_seek(silly_RAM[record].work_reg_1);

                    if (retval != SILLY_FS_ERR_OK)
                    {
                        io_error = true;
                    }
                    else
                    {
                        if (erase == true)
                        {
                            /* when erasing, always erase the entire record */
                            tmp_read_size = silly_RAM[record].work_reg_2;

                            /* erase the whole length */
                            retval = sillyfs_erase_range(silly_RAM[record].work_reg_1, silly_RAM[record].work_reg_2);
                        }
                        else
                        {
                            /* not erasing but reading */

                            /* read the data: min between block size and remaining data */
                            tmp_read_size = (rsize < silly_RAM[record].work_reg_2) ? rsize : silly_RAM[record].work_reg_2;

                            /* read from disk */
                            if (data != NULL)
                            {
                                retval = storage_read(&data[effective_read], tmp_read_size);
                            }
                        }

                        /* advance the effective read counter */
                        effective_read += tmp_read_size;

                        if (retval != SILLY_FS_ERR_OK)
                        {
                            io_error = true;
                        }
                        else
                        {
                            /* update the read pointer */
                            retval = storage_pos(&silly_RAM[record].work_reg_1);
                            /* update the read size */
                            silly_RAM[record].work_reg_2 -= tmp_read_size;

                            if (silly_RAM[record].work_reg_2 == 0)
                            {
                                SILLYFS_DEBUG_VERBOSE("end of block. Setting read size to UINT16_MAX");
                                silly_RAM[record].work_reg_2 = UINT16_MAX;
                            }

                            if (retval != SILLY_FS_ERR_OK)
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
                    retval = storage_seek(silly_RAM[record].offset);

                    if (retval == SILLY_FS_ERR_OK)
                    {
                        /* Read the size of the record */
                        retval = storage_read(tmp, 3);

                        if (retval == SILLY_FS_ERR_OK)
                        {
                            retval = storage_pos(&silly_RAM[record].work_reg_1);                    /* save the data pointer */
                            silly_RAM[record].work_reg_2 = (uint16_t)tmp[2];                                  /* LSB */
                            silly_RAM[record].work_reg_2 |= (uint16_t)((tmp[1] & ~0x80U) << 8U);    /* MSB (remove the size type bit) */
                            SILLYFS_DEBUG_VERBOSE("record size %d", silly_RAM[record].work_reg_2);
                        }

                        if (erase == true)
                        {
                            /* Erase the record */
                            //retval = sillyfs_erase_range(silly_RAM[record].work_reg_1, silly_RAM[record].work_reg_2);
                            retval = sillyfs_erase_range(silly_RAM[record].offset, 3U);
                            /* fake the rsize, iterating until all the record has been traversed */
                            rsize = 0xFFFFU;
                        }
                    }
                }
            } while ((effective_read < rsize) && (io_error == false) && (exit_loop == false));
        }
    }
    else
    {
        SILLYFS_DEBUG_VERBOSE("too large record %d or size %d", record, size);
        retval = SILLY_FS_ERR_INVALID_RECORD;
    }

    return retval;
}

t_e_silly_error sillyfs_read(uint8_t record, uint16_t rsize, uint8_t *data, uint16_t size)
{
    return sillyfs_traverse(record, rsize, data, size, false);
}

t_e_silly_error sillyfs_erase(uint8_t record)
{
    t_e_silly_error retval;

    retval = sillyfs_open(record);

    if (retval == SILLY_FS_ERR_OK)
    {
        retval = sillyfs_traverse(record, 0, NULL, 0, true);

        if (retval == SILLY_FS_ERR_OK)
        {
            /* successful traversal and erasure */

            /* close the record */
            retval = sillyfs_close(record);

            /* delete the record from the allocation table */
            silly_RAM[record].offset = 0U;
        }
    }

    return retval;
}

void printf_silly_error(t_e_silly_error errno)
{
    switch(errno)
    {
        case SILLY_FS_ERR_OK:
            SILLYFS_DEBUG_VERBOSE("%s: OK", __FUNCTION__);
            break;
        case SILLY_FS_ERR_GENERIC:
            SILLYFS_DEBUG_VERBOSE("%s: SILLY_FS_ERR_GENERIC", __FUNCTION__);
            break;
        case SILLY_FS_ERR_NOT_FORMATTED:
            SILLYFS_DEBUG_VERBOSE("%s: SILLY_FS_ERR_NOT_FORMATTED", __FUNCTION__);
            break;
        case SILLY_FS_ERR_INVALID_RECORD:
            SILLYFS_DEBUG_VERBOSE("%s: SILLY_FS_ERR_INVALID_RECORD", __FUNCTION__);
            break;
        case SILLY_FS_ERR_NOSPACE:
            SILLYFS_DEBUG_VERBOSE("%s: SILLY_FS_ERR_NOSPACE", __FUNCTION__);
            break;
        case SILLY_FS_ERR_NOT_WRITABLE:
            SILLYFS_DEBUG_VERBOSE("%s: SILLY_FS_ERR_NOT_WRITABLE", __FUNCTION__);
            break;
        case SILLY_FS_ERR_NOT_READABLE:
            SILLYFS_DEBUG_VERBOSE("%s: SILLY_FS_ERR_NOT_READABLE", __FUNCTION__);
            break;
        case SILLY_FS_ERR_INVALID_OPERATION:
            SILLYFS_DEBUG_VERBOSE("%s: SILLY_FS_ERR_INVALID_OPERATION", __FUNCTION__);
            break;
        case SILLY_FS_ERR_OUT_OF_RANGE:
            SILLYFS_DEBUG_VERBOSE("%s: SILLY_FS_ERR_OUT_OF_RANGE", __FUNCTION__);
            break;
        default:
            SILLYFS_DEBUG_VERBOSE("%s: ERROR IN DECODING ERROR: %d", __FUNCTION__ , (uint8_t)errno);
            break;
    }
}

//const char* TEST_CONTENT = "This is a test. We are writing to a new record. There is a lot of contiguous space currently and there is not fragmentation yet. This is a test. We are writing to a new record. There is a lot of contiguous space currently and there is not fragmentation yet. This is a test. We are writing to a new record. There is a lot of contiguous space currently and there is not fragmentation yet. This is a test. We are writing to a new record. There is a lot of contiguous space currently and there is not fragmentation yet. This is a test. We are writing to a new record. There is a lot of contiguous space currently and there is not fragmentation yet. This is a test. We are writing to a new record. There is a lot of contiguous space currently and there is not fragmentation yet. This is a test. We are writing to a new record. There is a lot of contiguous space currently and there is not fragmentation yet. This is a test. We are writing to a new record. There is a lot of contiguous space currently and there is not fragmentation yet.";

const char* TEST_CONTENT = "Hello, World! This is a record stored in the SillyFS.";

uint8_t read_buffer[4096*2];

void unit_test_eeprom_file(void)
{
    eeprom_handle = fopen("eeprom.bin", "r+");
    if (eeprom_handle == NULL)
    {
        printf("EEPROM file not found. Creating 4KB EEPROM\r\n");
        eeprom_handle = fopen("eeprom.bin", "w");
        if (eeprom_handle == NULL)
        {
            printf("Could not create EEPROM file\r\n");
            exit(1);
        }
        else
        {
            (void)fwrite(eeprom_image, 1, EEPROM_SIZE, eeprom_handle);
            (void)fclose(eeprom_handle);
        }
    }

    eeprom_handle = fopen("eeprom.bin", "r+");
}

/**
 * This test is used to verify that allocation of the maximum number of records
 * is successfully performed in a contiguous space (without fragmentation).
 * Sample data is written to each record and read back comparing to written data.
 * Test pass conditions are: no errors from sillyfs calls; written content reads
 * back successfully and content compares equal.
 *
 * @return  0 (or asserts)
 */
int test_contiguous(void)
{
    t_e_silly_error fserr;
    uint16_t i = 0;

    unit_test_eeprom_file();

    printf("Formatting media\r\n");
    fserr = sillyfs_format();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    fserr = sillyfs_init();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

    for (i = 0; i < SILLYFS_MAX_RECORD_COUNT; i++)
    {

        /* Filesystem is ready: open record */
        fserr = sillyfs_open(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Write to record */
        fserr = sillyfs_write(i, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Close the record */
        fserr = sillyfs_close(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Open the written record */
        fserr = sillyfs_open(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = sillyfs_read(i, sizeof(read_buffer), read_buffer, sizeof(read_buffer));
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Close the record */
        fserr = sillyfs_close(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));

        SILLYFS_ASSERT_VERBOSE(result, 0, "content does not match.");
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
    t_e_silly_error fserr;
    uint16_t i = 0;

    unit_test_eeprom_file();

    printf("Formatting media\r\n");
    fserr = sillyfs_format();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    fserr = sillyfs_init();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

    for (i = 0; i < SILLYFS_MAX_RECORD_COUNT; i++)
    {

        if (i == 4)
        {
            volatile uint8_t debug_helper = 23;
            debug_helper++;
        }

        /* Flush the file handle to disk at every step to enhance debugging */
        (void)fflush(eeprom_handle);

        /* Filesystem is ready: open record */
        fserr = sillyfs_open(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Write to record */
        fserr = sillyfs_write(i, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Close the record */
        fserr = sillyfs_close(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Open the written record */
        fserr = sillyfs_open(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = sillyfs_read(i, sizeof(read_buffer), read_buffer, sizeof(read_buffer));
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Close the record */
        fserr = sillyfs_close(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));
        SILLYFS_ASSERT_VERBOSE(result, 0, "content does not match.");

        /* Remove the record */
        fserr = sillyfs_erase(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
        /* Check that the allocation table has been erased */
        SILLYFS_ASSERT(silly_RAM[i].offset, 0U);
    }

    return 0;
}

int test_contiguous_and_remove_at_end(void)
{
    t_e_silly_error fserr;
    uint16_t i = 0;

    unit_test_eeprom_file();

    printf("Formatting media\r\n");
    fserr = sillyfs_format();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    fserr = sillyfs_init();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

    for (i = 0; i < SILLYFS_MAX_RECORD_COUNT; i++)
    {
        /* Flush the file handle to disk at every step to enhance debugging */
        (void)fflush(eeprom_handle);

        /* Filesystem is ready: open record */
        fserr = sillyfs_open(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Write to record */
        fserr = sillyfs_write(i, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Close the record */
        fserr = sillyfs_close(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Open the written record */
        fserr = sillyfs_open(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = sillyfs_read(i, sizeof(read_buffer), read_buffer, sizeof(read_buffer));
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Close the record */
        fserr = sillyfs_close(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));
        SILLYFS_ASSERT_VERBOSE(result, 0, "content does not match.");
    }

    for (i = 0; i < SILLYFS_MAX_RECORD_COUNT; i++)
    {
        /* Flush the file handle to disk at every step to enhance debugging */
        (void)fflush(eeprom_handle);

        /* Remove the record */
        fserr = sillyfs_erase(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Check that the allocation table has been erased */
        SILLYFS_ASSERT(silly_RAM[i].offset, 0U);
    }

    return 0;
}

// test that reads an existing partition and re-reads all the files
void test_reopen(void)
{
    t_e_silly_error fserr;
    uint8_t i = 0;

    fserr = sillyfs_init();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

    for (i = 0; i < SILLYFS_MAX_RECORD_COUNT; i++)
    {
        /* Filesystem is ready: open record */
        fserr = sillyfs_open(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = sillyfs_read(i, sizeof(read_buffer), read_buffer, sizeof(read_buffer));
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Close the record */
        fserr = sillyfs_close(i);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));
        SILLYFS_ASSERT_VERBOSE(result, 0, "content does not match.");
    }
}

void test_record_limit(void)
{
    t_e_silly_error fserr;

    fserr = sillyfs_open(SILLYFS_MAX_RECORD_COUNT);
    SILLYFS_ASSERT(SILLY_FS_ERR_INVALID_RECORD, fserr);

    fserr = sillyfs_write(SILLYFS_MAX_RECORD_COUNT, NULL, 0);
    SILLYFS_ASSERT(SILLY_FS_ERR_INVALID_RECORD, fserr);

    fserr = sillyfs_traverse(SILLYFS_MAX_RECORD_COUNT, 0, NULL, 0, false);
    SILLYFS_ASSERT(SILLY_FS_ERR_INVALID_RECORD, fserr);

    fserr = sillyfs_read(SILLYFS_MAX_RECORD_COUNT, 0, NULL, 0);
    SILLYFS_ASSERT(SILLY_FS_ERR_INVALID_RECORD, fserr);

    fserr = sillyfs_close(SILLYFS_MAX_RECORD_COUNT);
    SILLYFS_ASSERT(SILLY_FS_ERR_INVALID_RECORD, fserr);
}

void test_reopen_files(uint8_t index_record_start, uint8_t index_record_end)
{
    t_e_silly_error fserr;

    for (;index_record_start <= index_record_end; index_record_start++)
    {
        /* Filesystem is ready: open record */
        fserr = sillyfs_open(index_record_start);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
        (void)fflush(eeprom_handle);

        /* Read the record */
        (void)memset(read_buffer, 0, sizeof(read_buffer));
        fserr = sillyfs_read(index_record_start, sizeof(read_buffer), read_buffer, sizeof(read_buffer));
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

        /* Compare the read data */
        int result = memcmp(read_buffer, TEST_CONTENT, strlen(TEST_CONTENT));
        SILLYFS_ASSERT_VERBOSE(result, 0, "content does not match.");

        /* Close the record */
        fserr = sillyfs_close(index_record_start);
        printf_silly_error(fserr);
        SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
        /* Flush the file handle to disk at every step to enhance debugging */
        (void)fflush(eeprom_handle);
    }
}

/* Test fragmentation by creating 2 records of the same
 * size, erasing the first and trying to write another record
 */
void test_fragmentation()
{
    t_e_silly_error fserr;

    unit_test_eeprom_file();

    printf("Formatting media\r\n");
    fserr = sillyfs_format();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    fserr = sillyfs_init();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    (void)fflush(eeprom_handle);

    /* Filesystem is ready: open record 0 */
    fserr = sillyfs_open(0);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    (void)fflush(eeprom_handle);

    /* Write to record 0 */
    fserr = sillyfs_write(0, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    (void)fflush(eeprom_handle);

    /* Close the record 0 */
    fserr = sillyfs_close(0);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    (void)fflush(eeprom_handle);

    /* Filesystem is ready: open record 1 */
    fserr = sillyfs_open(1);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    (void)fflush(eeprom_handle);

    /* Write to record 1 */
    fserr = sillyfs_write(1, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    (void)fflush(eeprom_handle);

    /* Close the record 1 */
    fserr = sillyfs_close(1);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    (void)fflush(eeprom_handle);

    /* Remove record 0 */
    fserr = sillyfs_erase(0);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    (void)fflush(eeprom_handle);

    /* Filesystem is ready: open record 2 */
    fserr = sillyfs_open(2);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    (void)fflush(eeprom_handle);

    /* Write to record 2 */
    fserr = sillyfs_write(2, (const uint8_t*)TEST_CONTENT, strlen(TEST_CONTENT));
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    (void)fflush(eeprom_handle);

    /* Close the record 2 */
    fserr = sillyfs_close(2);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    /* Flush the file handle to disk at every step to enhance debugging */
    (void)fflush(eeprom_handle);

    /* Re-read record 1 and 2 and verify their integrity */
    test_reopen_files(1, 2);

}

int test_0_byte_record(void)
{
    t_e_silly_error fserr;

    unit_test_eeprom_file();

    printf("Formatting media\r\n");
    fserr = sillyfs_format();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);
    fserr = sillyfs_init();
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

    /* Filesystem is ready: open record */
    fserr = sillyfs_open(0);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

    /* Write to record, 0 bytes */
    fserr = sillyfs_write(0, (const uint8_t*)TEST_CONTENT, 0);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

    /* Close the record */
    fserr = sillyfs_close(0);
    printf_silly_error(fserr);
    SILLYFS_ASSERT(fserr, SILLY_FS_ERR_OK);

    return 0;
}

/* TODO tests to be implemented:
 * - write 0 byte record
 * - read 0 byte record
 * - write non-opened file
 * - read non-opened file
 * - write maximum storage record
 * -
 */

int main(void)
{
    // TODO: test sillyfs_find_contiguous_space in A) filesystem empty B) filesystem full C) other mixed cases...

    test_contiguous();
    test_reopen();
    test_contiguous_and_remove();
    test_contiguous_and_remove_at_end();
    test_record_limit();
    test_fragmentation();
    test_0_byte_record();

    SILLYFS_DEBUG_VERBOSE("test passed");

    (void)fclose(eeprom_handle);
    return 0;
}
