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

#include "../storage_api.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <avr/eeprom.h>

#define EEPROM_SIZE         ((uint16_t)E2END + (uint16_t)1U)

#define NULL_PTR_CHECK_RETURN(handle)  do              \
                                       {               \
                                           if (handle == NULL) return FROGFS_ERR_NULL_POINTER; \
                                       } while(0);     \

uint16_t eeprom_pos = 0;

uint16_t storage_size(void)
{
    return EEPROM_SIZE;
}

t_e_frogfs_error storage_advance(uint16_t size)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;

    if ((uint16_t)(E2END - eeprom_pos) >= size)
    {
        eeprom_pos += size;
        retval = FROGFS_ERR_OK;
    }

    return retval;
}

t_e_frogfs_error storage_pos(uint16_t *offset)
{
    t_e_frogfs_error retval = FROGFS_ERR_OK;

    NULL_PTR_CHECK_RETURN(offset);
    *offset = eeprom_pos;

    return retval;
}

t_e_frogfs_error storage_end_of_storage(void)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;

    if ((EEPROM_SIZE == 0) || (eeprom_pos == (EEPROM_SIZE - 1)))
    {
        retval = FROGFS_ERR_OK;
    }

    return retval;
}

t_e_frogfs_error storage_seek(uint16_t offset)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;

    if (offset <= E2END)
    {
        eeprom_pos = offset;
        retval = FROGFS_ERR_OK;
    }

    return retval;
}

t_e_frogfs_error storage_read(uint8_t *data, uint16_t size)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;

    NULL_PTR_CHECK_RETURN(data);

    if ((uint16_t)(eeprom_pos+size) <= E2END)
    {
        eeprom_read_block(data, (void*)eeprom_pos, size);
        retval = FROGFS_ERR_OK;
        eeprom_pos += size;
    }

    return retval;
}

t_e_frogfs_error storage_write(const uint8_t *data, uint16_t size)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;

    NULL_PTR_CHECK_RETURN(data);

    if ((uint16_t)(eeprom_pos + size) <= (uint16_t)EEPROM_SIZE)
    {
        eeprom_write_block(data, (void*)eeprom_pos, size);
        retval = FROGFS_ERR_OK;
        eeprom_pos += size;
    }

    return retval;
}

void storage_sync(void)
{
    /* Nothing to do */
}

t_e_frogfs_error storage_close(void)
{
    /* Nothing to do */
    return FROGFS_ERR_OK;
}
