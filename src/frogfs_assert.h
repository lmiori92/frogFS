/*
 * frogfs_assert.h
 *
 *  Created on: 20 giu 2019
 *      Author: lorenzo
 */

#ifndef FROGFS_ASSERT_H_
#define FROGFS_ASSERT_H_

#define FROGFS_DEBUG_VERBOSE(...)      do { printf("line\t%d:\t", __LINE__); \
                                             printf(__VA_ARGS__);           \
                                             printf("\n");                  \
                                        } while(0);

#define FROGFS_ASSERT(x,y,...)             do { uint32_t line = __LINE__;           \
                                                  if (x != y)                        \
                                                  {                                  \
                                                      printf("assertion failed at line %d: ", line); \
                                                      printf("was %08x, expected %08x", (uint32_t)x, (uint32_t)y);  \
                                                      printf("\n");                  \
                                                      exit(1);                       \
                                                  }                                  \
                                             }while(0);

#define FROGFS_ASSERT_VERBOSE(x,y,...)      do { uint32_t line = __LINE__;          \
                                                  if (x != y)                        \
                                                  {                                  \
                                                      printf("assertion failed at line %d: ", line); \
                                                      printf(__VA_ARGS__);           \
                                                      printf("was %08x, expected %08x", (uint32_t)x, (uint32_t)y);  \
                                                      printf("\n");                  \
                                                      exit(1);                       \
                                                  }                                  \
                                             }while(0);

#endif /* FROGFS_ASSERT_H_ */
