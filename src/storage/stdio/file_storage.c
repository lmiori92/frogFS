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

#include "file_storage.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

uint8_t *eeprom_image = NULL;
FILE *eeprom_handle = NULL;

#define NULL_PTR_CHECK_RETURN(handle)  do              \
                                                {               \
                                                    if (handle == NULL) return FROGFS_ERR_NULL_POINTER; \
                                                } while(0);     \

static uint16_t file_storage_size = 0;

static void file_storage_create(void)
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
            (void)memset(eeprom_image, 0, file_storage_size);
            (void)fwrite(eeprom_image, 1, file_storage_size, eeprom_handle);
            (void)fclose(eeprom_handle);
        }
        eeprom_handle = fopen("eeprom.bin", "r+");
    }
}

void file_storage_set_file(char *storage_filename)
{
    int fretval = -1;

    file_storage_size = 0;

    eeprom_handle = fopen(storage_filename, "r+");
    if (eeprom_handle == NULL)
    {
        printf("Could not open eeprom file: %s\n", storage_filename);
    }
    else
    {
        /* Read the emulated size from the file itself */
        (void)fseek(eeprom_handle, 0, SEEK_END);
        fretval = ftell(eeprom_handle);

        if (fretval != -1)
        {
            file_storage_size = (uint16_t)fretval;
        }
    }
}

void file_storage_set_size(uint16_t storage_size)
{
    /* Set the new internal storage size */
    file_storage_size = storage_size;

    /* Check if a buffer had been previously allocated */
    if (eeprom_image != NULL)
    {
        /* deallocate previous buffer */
        free(eeprom_image);
    }
    /* create a new buffer */
    eeprom_image = malloc(file_storage_size);
    /* allocate the physical storage */
    file_storage_create();
}

uint16_t storage_size(void)
{
    return file_storage_size;
}

t_e_frogfs_error storage_advance(uint16_t size)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    int fretval;

    NULL_PTR_CHECK_RETURN(eeprom_handle);

    fretval = fseek(eeprom_handle, size, SEEK_CUR);

    if (fretval == 0)
    {
        retval = FROGFS_ERR_OK;
    }

    return retval;
}

t_e_frogfs_error storage_pos(uint16_t *offset)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    int fretval;

    NULL_PTR_CHECK_RETURN(offset);
    NULL_PTR_CHECK_RETURN(eeprom_handle);

    fretval = ftell(eeprom_handle);

    if (fretval != -1)
    {
        retval = FROGFS_ERR_OK;
        *offset = (uint16_t)fretval;
    }

    return retval;
}

t_e_frogfs_error storage_end_of_storage(void)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    int fretval;

    NULL_PTR_CHECK_RETURN(eeprom_handle);

    fretval = ftell(eeprom_handle);

    if ((file_storage_size == 0) || (fretval == (file_storage_size - 1)))
    {
        retval = FROGFS_ERR_OK;
    }

    return retval;
}

t_e_frogfs_error storage_seek(uint16_t offset)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    int fretval;

    NULL_PTR_CHECK_RETURN(eeprom_handle);

    fretval = fseek(eeprom_handle, offset, SEEK_SET);

    if (fretval == 0)
    {
        retval = FROGFS_ERR_OK;
    }

    return retval;
}

t_e_frogfs_error storage_read(uint8_t *data, uint16_t size)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    int fretval;

    NULL_PTR_CHECK_RETURN(data);
    NULL_PTR_CHECK_RETURN(eeprom_handle);

    fretval = fread(data, 1, size, eeprom_handle);


    if (fretval == (int)size)
    {
        retval = FROGFS_ERR_OK;
    }

    return retval;
}

t_e_frogfs_error storage_write(const uint8_t *data, uint16_t size)
{
    t_e_frogfs_error retval = FROGFS_ERR_IO;
    int fretval;

    NULL_PTR_CHECK_RETURN(data);
    NULL_PTR_CHECK_RETURN(eeprom_handle);

    fretval = fwrite(data, 1, size, eeprom_handle);


    if (fretval == (int)size)
    {
        retval = FROGFS_ERR_OK;
    }

    return retval;
}

void storage_sync(void)
{
    (void)fflush(eeprom_handle);
}

t_e_frogfs_error storage_close(void)
{
    int ret = -1;

    ret = fclose(eeprom_handle);

    if (ret == 0)
    {
        return FROGFS_ERR_OK;
    }
    else
    {
        return FROGFS_ERR_IO;
    }
}
