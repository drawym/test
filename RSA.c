#include "system.h"

/*rsa.c*/
// 3字节的明文变成4字节的密文

/**
 * @brief 	扩展欧几里得算法，用于计算gcd和模逆
 *
 * @param a：	第1个整数输入
 * @param b：	第2个整数输入
 * @param g：	a，b的最大公约数
 * @param x：	a mod b 的逆元
 * @param y：	b mod a 的逆元
 */
void egcd(int64_t a, int64_t b, int64_t *g, int64_t *x, int64_t *y)
{
	if (a == 0)
	{
		*g = b;
		*x = 0;
		*y = 1;
		// printf("Base Case: g=%lld, x=%lld, y=%lld\n", *g, *x, *y);
		return;
	}
	int64_t x1, y1;
	egcd(b % a, a, g, &x1, &y1);
	*x = y1 - (b / a) * x1;
	*y = x1;

	// printf("Recursive Step: x=%lld, y=%lld (for a=%lld, b=%lld)\n", *x, *y, a, b);
}

/**
 * @brief 求模逆元，模逆元存在的条件是，a和模m是互质的
 *
 * @param a：	待求逆元的整数
 * @param m：	模
 *
 * @return
 */
int64_t modinv(int64_t a, int64_t m)
{
	int64_t g, x, y;
	egcd(a, m, &g, &x, &y);
	if (g != 1)
	{
		// printf("模逆不存在\n");
		return -1;
	}
	else
	{
		return (x % m + m) % m; // 确保了返回的逆元是正的
	}
}

/**
 * @brief 蒙哥马利约简(将T转换成蒙哥马利形式的值)
 *
 * @param N：			模数
 * @param logR：		R的对数，基于2
 * @param N_inv_neg：	N模R的负逆元
 * @param T：			要简约的数
 *
 * @return 返回的是蒙哥马利形式的T值
 */
int64_t REDC(int64_t N, int64_t logR, int64_t N_inv_neg, int64_t T)
{
	int64_t m = ((T & ((1LL << logR) - 1)) * N_inv_neg) & ((1LL << logR) - 1);
	int64_t t = (T + m * N) >> logR;
	if (t >= N)
	{
		return t - N;
	}
	else
	{
		return t;
	}
}

/**
 * @brief 蒙哥马利模乘算法，计算(a * b) mod N的结果
 *
 * @param N：			模数
 * @param N_inv_neg：	N模R的负模逆元
 * @param R2：			R? mod N的预计算值
 * @param a：			乘数1
 * @param b：			乘数2
 * @param logR：		R的对数，基于2
 *
 * @return (a * b) mod N的结果
 */
int64_t ModMul(int64_t N, int64_t R2, int64_t logR, int64_t N_inv_neg, int64_t a, int64_t b)
{
	if (a >= N || b >= N) // a b需要小于N
	{
		// printf("输入必须小于模N,a:%llx,b:%llx\n", a, b);
		return -1;
	}
	int64_t aR	= REDC(N, logR, N_inv_neg, a * R2); // 将a转为蒙哥马利形式
	int64_t bR	= REDC(N, logR, N_inv_neg, b * R2); // 将b转为蒙哥马利形式
	int64_t T	= aR * bR;							// 蒙哥马利形式下的乘法
	int64_t abR = REDC(N, logR, N_inv_neg, T);		// 将2层蒙哥马利形式下的T(abRR)转为1层蒙哥马利形式下的abR
	return REDC(N, logR, N_inv_neg, abR);			// 再蒙哥马利形式下的abR转为正常的ab乘积
}

/**
 * @brief 获取比N大的最小的2^k
 *
 * @param N：		模数
 * @param logR：	R的对数，基于2
 */
void Get_LogR(int64_t *logR, int64_t N)
{
	int64_t tmp64 = N;
	while (tmp64 > 0)
	{
		tmp64 >>= 1;
		(*logR)++; // 确保R > N，如果N=3，logR=2，R=(1LL << logR)
	}
}

/**
 * @brief 计算大数幂模结果
 *
 * @param N
 * @param base
 * @param exponent
 *
 * @return
 */
int64_t ModExp(int64_t base, int64_t exp, int64_t N)
{
	int64_t logR = 0;
	Get_LogR(&logR, N); // 获取logR
	int64_t R		  = 1LL << logR;
	int64_t N_inv	  = modinv(N, R);
	int64_t N_inv_neg = R - N_inv;
	int64_t R2		  = (R * R) % N;

	int64_t result = REDC(N, logR, N_inv_neg, R2);		  // 把R2转到1层的蒙哥马利空间
	int64_t baseR  = REDC(N, logR, N_inv_neg, base * R2); // Convert base to Montgomery form

	while (exp)
	{
		if (exp & 1)
			result = ModMul(N, R2, logR, N_inv_neg, result, baseR);
		exp >>= 1;
		baseR = ModMul(N, R2, logR, N_inv_neg, baseR, baseR);
	}
	// 转化结果为正常形式
	return REDC(N, logR, N_inv_neg, result);
}

/**
 * @brief 费马小定理，计算p是否是素数，判定次数k
 *
 * @param k：	判断p是否是素数的次数
 * @param p：	待判断数
 *
 * @return 0：大概率是素数，-1不是素数
 */
int8_t FermatLT(int64_t p, uint8_t k)
{
	int64_t ab = 0;

	if (p <= 1 || p == 4) // 小于等于1或等于4的数不是素数
		return -1;

	if (p <= 3) // 2和3是素数
		return 0;

	while (k > 0)
	{
		// 随机选择一个在[2, n-2]范围内的底数
		ab = 2 + rand() % (p - 4);
		if (ModExp(ab, p - 1, p) != 1)
			return -1;
		k--;
	}

	return 0;
}

/**
 * @brief 小数的快速幂模运算
 *
 * @param C 底数
 * @param E 指数
 *
 * @return 余数(密文)
 */
int64_t QuickPow(int64_t C, int64_t E, int64_t N)
{
	int64_t res	 = 1;
	int64_t logR = 0;
	Get_LogR(&logR, N);		 // 获取logR
	int64_t R = 1LL << logR; // < 32
	// int64_t N_inv     = modinv(N, R);
	int64_t N_inv_neg = 463090261; // R - N_inv;
	int64_t R2		  = (R * R) % N;

	while (E)
	{
		if (E & 1)										  // 判断是否需要
			res = ModMul(N, R2, logR, N_inv_neg, res, C); // 需要，在结果上累乘
		E >>= 1;										  // 接着判断下一位
		C = ModMul(N, R2, logR, N_inv_neg, C, C);		  // 这个中间变量是一直都要累计的
	}
	return res;
}

/**
 * @brief 	使用RSA对数组数据加密
 *			明文和密文数组长度关系：每3字节明文对应4字节的密文
 *
 * @param dst：		目标地址，加密后数据存放地址
 * @param src：		源地址，明文存放地址
 * @param length：	明文数组大小，单位字节
 */
void rsa_encryption(uint8_t *dst, const uint8_t *src, uint16_t length)
{
	uint8_t	 en_count = 0;
	int64_t	 tmp64	  = 0;
	uint8_t *sptr	  = (uint8_t *)src;
	uint8_t *dptr	  = (uint8_t *)dst;

	en_count = length / 3;

	for (int i = 0; i < en_count; i++)
	{
		tmp64 = 0;
		tmp64 |= (*sptr++) << 16;
		tmp64 |= (*sptr++) << 8;
		tmp64 |= *sptr++;
		tmp64	= QuickPow(tmp64, PUBLIC_KEY, MOD_N);
		*dptr++ = (uint8_t)((tmp64 >> 24) & 0xFF);
		*dptr++ = (uint8_t)((tmp64 >> 16) & 0xFF);
		*dptr++ = (uint8_t)((tmp64 >> 8) & 0xFF);
		*dptr++ = (uint8_t)((tmp64) & 0xFF);
	}
	if ((length % 3) == 2)
	{
		tmp64 = 0;
		tmp64 |= (*sptr++) << 16;
		tmp64 |= (*sptr++) << 8;
		tmp64	= QuickPow(tmp64, PUBLIC_KEY, MOD_N);
		*dptr++ = (uint8_t)((tmp64 >> 24) & 0xFF);
		*dptr++ = (uint8_t)((tmp64 >> 16) & 0xFF);
		*dptr++ = (uint8_t)((tmp64 >> 8) & 0xFF);
		*dptr++ = (uint8_t)((tmp64) & 0xFF);
	}
	else if ((length % 3) == 1)
	{
		tmp64 = 0;
		tmp64 |= (*sptr++) << 16;
		tmp64	= QuickPow(tmp64, PUBLIC_KEY, MOD_N);
		*dptr++ = (uint8_t)((tmp64 >> 24) & 0xFF);
		*dptr++ = (uint8_t)((tmp64 >> 16) & 0xFF);
		*dptr++ = (uint8_t)((tmp64 >> 8) & 0xFF);
		*dptr++ = (uint8_t)((tmp64) & 0xFF);
	}
}

/**
 * @brief 	使用RSA对数组数据解密
 *			明文和密文数组长度关系：每4字节的密文对应3字节明文
 *
 * @param dst		目标地址，解密后数据存放地址
 * @param src		源地址，密文存放地址
 * @param length	密文数组大小，单位字节
 */
void rsa_decode(uint8_t *dst, const uint8_t *src, uint16_t length)
{
	uint8_t	 en_count = 0;
	int64_t	 tmp64	  = 0;
	uint8_t *sptr	  = (uint8_t *)src;
	uint8_t *dptr	  = (uint8_t *)dst;

	en_count = length / 4;

	for (int i = 0; i < en_count; i++)
	{
		tmp64 = 0;
		tmp64 |= (int64_t)(*sptr++) << 24;
		tmp64 |= (int64_t)(*sptr++) << 16;
		tmp64 |= (int64_t)(*sptr++) << 8;
		tmp64 |= (int64_t)(*sptr++);
		tmp64	= QuickPow(tmp64, PRIVATE_KEY, MOD_N);
		*dptr++ = (tmp64 >> 16) & 0xFF;
		*dptr++ = (tmp64 >> 8) & 0xFF;
		*dptr++ = (tmp64) & 0xFF;
	}
}
