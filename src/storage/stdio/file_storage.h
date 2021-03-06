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

#ifndef STORAGE_FILE_H_
#define STORAGE_FILE_H_

#include "storage/storage_api.h"
#include "frogfs_enums.h"

#include <stdint.h>

/**
 * Set and use the given file as storage emulation.
 * @param storage_filename
 */
void file_storage_set_file(char *storage_filename);

/**
 * Initialize the storage by giving a file size.
 * The file "eeprom.bin" is generated.
 * @param storage_size  the size of the emulated storage
 */
void file_storage_set_size(uint16_t storage_size);

#endif /* STORAGE_FILE_H_ */
