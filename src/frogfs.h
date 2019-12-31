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

#ifndef FROGFS_H_
#define FROGFS_H_

#include "frogfs_enums.h"

#include <stdbool.h>

/** The simplistic design needs to avoid zero's in the metadata block,
 *  especially at the beginning as the free space allocator cannot
 *  distinguish a metadata versus empty space otherwise.
 */
#define FROGFS_MIN_RECORD_INDEX_OFFSET (1U)

/** Maximum number of total records on the filesystem.
 *  Tune: adjust to match the RAM requirements for the application but do NOT
 *        exceed 126. The record index is internally offset by FROGFS_MIN_RECORD_INDEX_OFFSET. */
#define FROGFS_MAX_RECORD_COUNT        (32U)

/** Maximum length of a single record. This is a hard limit that comes
 *  from the design of the filesystem. Anything below could work but has
 *  not meaning as records are dynamically allocated. */
#define FROGFS_MAX_RECORD_SIZE         (32U*1024U)

typedef struct
{
    uint16_t offset;        /**< The allocation table of the first block of the record */

    uint16_t work_reg_1;    /**< Generic working register to support file operations.
                                 Meaning is documented for each function/module using it. */
    uint16_t work_reg_2;    /**< Generic working register to support file operations.
                                 Meaning is documented for each function/module using it. */
    uint16_t write_offset;  /**< Write pointer for write operations. If different from 0, then
                                 a record is open for writing */
} t_s_frogfsram_record;

t_e_frogfs_error frogfs_format(void);
bool             frogfs_is_nil(const uint8_t *data, uint16_t size);
t_e_frogfs_error frogfs_init(void);
t_e_frogfs_error frogfs_find_contiguous_space(uint16_t *space_start, uint16_t *data_start, uint16_t *data_size);
t_e_frogfs_error frogfs_list(uint8_t *list, uint8_t list_size, uint8_t *file_num);
t_e_frogfs_error frogfs_get_available(uint8_t *record);
t_e_frogfs_error frogfs_open(uint8_t record);
t_e_frogfs_error frogfs_write(uint8_t record, const uint8_t *data, uint16_t size);
t_e_frogfs_error frogfs_close(uint8_t record);
t_e_frogfs_error frogfs_erase_range(uint16_t pos, uint16_t size);
t_e_frogfs_error frogfs_erase(uint8_t record);
t_e_frogfs_error frogfs_read(uint8_t record, uint8_t *data, uint16_t size, uint16_t *effective_read);
t_e_frogfs_error frogfs_traverse(uint8_t record, uint8_t *data, uint16_t size, uint16_t *effective_read, bool erase);
void printf_frogfserror(t_e_frogfs_error errno);

#endif /* FROGFS_H_ */
