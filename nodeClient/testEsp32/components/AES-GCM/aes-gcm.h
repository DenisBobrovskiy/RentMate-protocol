#ifndef AES_GCM_HEADER_H
#define AES_GCM_HEADER_H
#include "mbedtls/gcm.h"


void printCryptData(unsigned char* cryptData,size_t length);

int initGCM(mbedtls_gcm_context *ctx, unsigned char *gcmKey, unsigned int keybits);

int encryptGcm(mbedtls_gcm_context *gcmCtx,
        const unsigned char *msg,
        size_t msgLen,
        const unsigned char *iv,
        size_t ivLen,
        const unsigned char *add,
        size_t addLen,
        unsigned char *msgOut,
        unsigned char *tagOut,
        size_t tagLen
        );

int decryptGcm(mbedtls_gcm_context *gcmCtx,
        const unsigned char *msg,
        size_t msgLen,
        const unsigned char *iv,
        size_t ivLen,
        const unsigned char *add,
        size_t addLen,
        const unsigned char *tag,
        size_t tagLen,
        unsigned char *msgOut);



#endif
