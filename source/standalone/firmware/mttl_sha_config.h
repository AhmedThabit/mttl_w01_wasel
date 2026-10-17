#define MBEDTLS_SHA256_C
/* Loop form of the compression function: about 1.4 KB less code in SRAM, same digests
 * (checked against the unrolled form); hashing the 330 KB OTA image is slightly slower. */
#define MBEDTLS_SHA256_SMALLER
