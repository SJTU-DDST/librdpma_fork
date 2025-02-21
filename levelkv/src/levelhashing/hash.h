#ifndef HASH_H_
#define HASH_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
Function: hash()
        This function is used to compute the hash value of a string key;
        For integer keys, two different and independent hash functions should be
        used. For example, Jenkins Hash is used for the first hash funciton, and murmur3
        hash is used for the second hash funciton.
*/
uint64_t hash_(const void *data, uint64_t length, uint64_t seed);

#ifdef __cplusplus
}
#endif

#endif