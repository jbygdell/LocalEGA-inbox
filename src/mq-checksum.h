#ifndef __MQ_CHECKSUM_H_INCLUDED__
#define __MQ_CHECKSUM_H_INCLUDED__

#include <openssl/sha.h>

#define MQ_CHECKSUM_TYPE "sha256"
#define MQ_CHECKSUM_SIZE SHA256_DIGEST_LENGTH

typedef SHA256_CTX checksum_t;

int checksum_init(checksum_t* checksum);

int checksum_add(checksum_t* checksum, const void *data, size_t dlen);

int checksum_final(checksum_t* checksum, unsigned char out[MQ_CHECKSUM_SIZE]);

#endif /* !__MQ_CHECKSUM_H_INCLUDED__ */




