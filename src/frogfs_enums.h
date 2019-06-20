/*
 * frogfs.h
 *
 *  Created on: 20 giu 2019
 *      Author: lorenzo
 */

#ifndef FROGFS_ENUMS_H_
#define FROGFS_ENUMS_H_

typedef enum
{
    FROGFS_ERR_OK,
    FROGFS_ERR_NULL_POINTER,
    FROGFS_ERR_IO,
    FROGFS_ERR_NOT_FORMATTED,
    FROGFS_ERR_INVALID_RECORD,
    FROGFS_ERR_NOSPACE,
    FROGFS_ERR_NOT_WRITABLE,
    FROGFS_ERR_NOT_READABLE,
    FROGFS_ERR_INVALID_OPERATION,
    FROGFS_ERR_OUT_OF_RANGE
} t_e_frogfs_error;

#endif /* FROGFS_ENUMS_H_ */
