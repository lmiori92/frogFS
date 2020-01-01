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


#ifndef STORAGE_STORAGE_API_H_
#define STORAGE_STORAGE_API_H_

#include "frogfs_enums.h"

#include <stdint.h>

t_e_frogfs_error storage_close(void);
void             storage_sync(void);
uint16_t         storage_size(void);
t_e_frogfs_error storage_advance(uint16_t size);
t_e_frogfs_error storage_backtrack(uint16_t size);
t_e_frogfs_error storage_pos(uint16_t *offset);
t_e_frogfs_error storage_end_of_storage(void);
t_e_frogfs_error storage_seek(uint16_t offset);
t_e_frogfs_error storage_read(uint8_t *data, uint16_t size);
t_e_frogfs_error storage_write(const uint8_t *data, uint16_t size);

#endif /* STORAGE_STORAGE_API_H_ */
