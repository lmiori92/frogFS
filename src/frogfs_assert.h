/*
 * frogfs_assert.h
 *
 *  Created on: 20 giu 2019
 *      Author: lorenzo
 */

#ifndef FROGFS_ASSERT_H_
#define FROGFS_ASSERT_H_

#ifdef __AVR__
#include <stdio.h>
#include <avr/pgmspace.h>
#define FROGFS_DEBUG_STR_MEM(x)     PSTR(x)
#define FROGFS_PRINTF               printf_P      /**< AVR program space printf variant */
#else
#define FROGFS_DEBUG_STR_MEM(x)     x           /**< Normal hosted string memory space */
#define FROGFS_PRINTF               printf      /**< Normal hosted printfs */
#endif

#define FROGFS_DEBUG_VERBOSE(fmt, ...)      do {  FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("line\t%d:\t"), __LINE__); \
                                             FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM(fmt), ## __VA_ARGS__);           \
                                             FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("\r\n"));                  \
                                        } while(0);

#define FROGFS_ASSERT(x,y,...)             do { uint32_t line = __LINE__;           \
                                                  if (x != y)                        \
                                                  {                                  \
                                                      FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("assertion failed at line %lu: "), (unsigned long)line); \
                                                      FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("was %08lx, expected %08lx"), (unsigned long)x, (unsigned long)y);  \
                                                      FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("\r\n"));                  \
                                                      exit(1);                       \
                                                  }                                  \
                                             }while(0);

#define FROGFS_ASSERT_VERBOSE(x,y,fmt,...)      do { uint32_t line = __LINE__;          \
                                                  if (x != y)                        \
                                                  {                                  \
                                                      FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("assertion failed at line %lu: "), (unsigned long)line); \
                                                      FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM(fmt), ## __VA_ARGS__);           \
                                                      FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("was %08lx, expected %08lx"), (unsigned long)x, (unsigned long)y);  \
                                                      FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("\r\n"));                  \
                                                      exit(1);                       \
                                                  }                                  \
                                             }while(0);

#define FROGFS_ASSERT_UNCHECKED(fmt,...)      do { uint32_t line = __LINE__;          \
                                                  FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("assertion failed at line %lu: "), (unsigned long)line); \
                                                  FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM(fmt), ## __VA_ARGS__);           \
                                                  FROGFS_PRINTF(FROGFS_DEBUG_STR_MEM("\r\n"));                  \
                                                  exit(1);                       \
                                             }while(0);

#endif /* FROGFS_ASSERT_H_ */
