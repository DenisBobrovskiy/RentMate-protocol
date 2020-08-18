#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aes-gcm.h"
#include "mbedtls/gcm.h"


//#define ivLen 12
//#define tagLen 16

void printCryptData(unsigned char* cryptData,size_t length){
    //printf("Printing crypt data:\n");
    for(int i =0; i<length; i++){
        printf("%c ", cryptData[i]);
    }
    printf("\n");
    
}

//Normally use 128 bit key so set keybits to 128
int initGCM(mbedtls_gcm_context *ctx, unsigned char *gcmKey, unsigned int keybits){
    printf("Initializing AES-GCM\n");
    //Initialize gcm context
    mbedtls_gcm_init(ctx);
    printf("Context initialized\n");
    //Associate gcm context with key and set cipher to AES
    if(mbedtls_gcm_setkey(ctx,MBEDTLS_CIPHER_ID_AES,gcmKey,keybits)!=0){
        //ERROR
        printf("Failed to mbedtls_gcm_setkey");
        return 1;
    }

    return 0;
}

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
        )
{
    int result;
    if((result = mbedtls_gcm_crypt_and_tag(gcmCtx,MBEDTLS_GCM_ENCRYPT,msgLen,iv,ivLen,add,addLen,msg,msgOut,tagLen,tagOut))!=0){
        //ERROR
        if(result==MBEDTLS_ERR_GCM_BAD_INPUT){
            printf("Bad gcm encryption input\n");
            return 1;
        }else{
            printf("gcm encryption failed\n");
            return 2;
        }
    }
    printf("gcm encryption success!\n");
    return 0;
}

int decryptGcm(mbedtls_gcm_context *gcmCtx,
        const unsigned char *msg,
        size_t msgLen,
        const unsigned char *iv,
        size_t ivLen,
        const unsigned char *add,
        size_t addLen,
        const unsigned char *tag,
        size_t tagLen,
        unsigned char *msgOut)
{
    int result;
    if((result=mbedtls_gcm_auth_decrypt(gcmCtx,msgLen,iv,ivLen,add,addLen,tag,tagLen,msg,msgOut))!=0){
        //ERROR
        if(result==MBEDTLS_ERR_GCM_AUTH_FAILED){
            printf("Tag doesnt match in decryption!\n");
            return 1;
        }else if(result==MBEDTLS_ERR_GCM_BAD_INPUT){
            printf("Bad gcm decrypt input!\n");
            return 2;
        }else{
            printf("gcm decrypt failed!\n");
            return 3;
        }
    }
    printf("gcm decrypt success!\n");
    return 0;
}
