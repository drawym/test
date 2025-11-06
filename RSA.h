#ifndef _RSA_H_
#define _RSA_H_
// clang-format off

//===========================================
#define 	MOD_N 			0x249EC503		// 模数N
// #define 	PUBLIC_KEY 		0xDEC9B			// 他人的公钥
#define 	PUBLIC_KEY 		0x88410D		// 自己的公钥
#define 	PRIVATE_KEY 	0x1AFE9825		// 自己的私钥

#define 	POLY 			0x8005
//===========================================

// 蒙哥马利算法
void    egcd(int64_t a, int64_t b, int64_t *g, int64_t *x, int64_t *y);
int64_t modinv(int64_t a, int64_t m);
int64_t REDC(int64_t N, int64_t logR, int64_t N_inv_neg, int64_t T);
int64_t ModMul(int64_t N, int64_t R2, int64_t logR, int64_t N_inv_neg, int64_t a, int64_t b);
void    Get_LogR(int64_t *logR, int64_t N);
int64_t ModExp(int64_t base, int64_t exp, int64_t N);
int8_t  FermatLT(int64_t p, uint8_t k);

void rsa_encryption(uint8_t *dst, const uint8_t *src, uint16_t length);
void rsa_decode(uint8_t *dst, const uint8_t *src, uint16_t length);

//===========================================

#endif