#include <stdio.h>

#include "mq-checksum.h"

int
checksum_init(checksum_t* checksum)
{
  return SHA256_Init(checksum);
}

int
checksum_add(checksum_t* checksum, const void *data, size_t dlen)
{
  return SHA256_Update(checksum, data, dlen);
}

int
checksum_final(checksum_t* checksum,
	       unsigned char out[MQ_CHECKSUM_SIZE])
{
  return SHA256_Final(out, checksum);
}
