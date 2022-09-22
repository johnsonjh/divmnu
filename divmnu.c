/* vim: set ts=4 sw=4 tw=0 cc=79 et : */

/****************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/****************************************************************************/

#define kd_div_max(x, y) ( (x) > (y) ? (x) : (y) )

/****************************************************************************/

unsigned int kd_div_errors = 0;

/****************************************************************************/

int
nlz (unsigned x);

int
nlz (unsigned x)
{
  int n;

  if (x == 0)
    return (32);

  n = 0;

  if (x <= 0x0000FFFF)
    {
      n = n  + 16;
      x = x << 16;
    }

  if (x <= 0x00FFFFFF)
    {
      n = n  + 8;
      x = x << 8;
    }

  if (x <= 0x0FFFFFFF)
    {
      n = n  + 4;
      x = x << 4;
    }

  if (x <= 0x3FFFFFFF)
    {
      n = n  + 2;
      x = x << 2;
    }

  if (x <= 0x7FFFFFFF)
    {
      n = n + 1;
    }

  return n;
}

/****************************************************************************/

void
dumpit (char *msg, int n, unsigned v[]);

void
dumpit (char *msg, int n, unsigned v[])
{
  int i;

  (void)fprintf (stderr, "%s", msg);

  for (i = n - 1; i >= 0; i--)
    (void)fprintf (stderr, " %08X", v[i]);

  (void)fprintf (stderr, "\n");
}

/****************************************************************************/

typedef struct
{
  uint32_t q;
  uint32_t r;
  bool     overflow;
  char pad[3];
} divrem_t;

divrem_t
divrem_64_by_32 (uint64_t n, uint32_t d);

divrem_t
divrem_64_by_32 (uint64_t n, uint32_t d)
{
  if ( (n >> 32) >= d )
      /* overflow */
    {
      return (divrem_t)
        {
          .q        = UINT32_MAX,
          .r        = 0,
          .overflow = true
        };
    }
  else
    {
      return (divrem_t)
        {
          .q        = (uint32_t)(n / d),
          .r        = n % d,
          .overflow = false
        };
    }
}

/****************************************************************************/

bool
bigmul (uint32_t qhat, unsigned product[], unsigned vn[],
        int m, int n);

bool
bigmul (uint32_t qhat, unsigned product[], unsigned vn[],
        int m, int n)
{
  (void)qhat;
  (void)product;
  (void)vn;
  (void)m;
  (void)n;

  uint32_t carry = 0;

  /* VL = n + 1 */
  /* sv.madded product.v, vn.v, qhat.s, carry.s */

  for (int i = 0; i <= n; i++)
    {
      uint32_t vn_v  = i < n ? vn[i] : 0;
      uint64_t value = (uint64_t)vn_v * (uint64_t)qhat + carry;
      carry          = (uint32_t)(value >> 32);
      product[i]     = (uint32_t)value;
    }

  return carry != 0;
}

/****************************************************************************/

bool
bigadd (unsigned result[], unsigned vn[], unsigned un[],
        int m, int n, bool ca);

bool
bigadd (unsigned result[], unsigned vn[], unsigned un[],
        int m, int n, bool ca)
{
  (void)result;
  (void)vn;
  (void)un;
  (void)m;
  (void)n;
  (void)ca;

  /* VL = n + 1 */
  /* sv.subfe un_j.v, product.v, un_j.v */

  for (int i = 0; i <= n; i++)
    {
      uint64_t value = (uint64_t)vn[i] + (uint64_t)un[i] + ca;
      ca             = value >> 32 != 0;
      result[i]      = (unsigned)value;
    }

  return ca;
}

/****************************************************************************/

bool
bigsub (unsigned result[], unsigned vn[], unsigned un[],
        int m, int n, bool ca);

bool
bigsub (unsigned result[], unsigned vn[], unsigned un[],
        int m, int n, bool ca)
{
  (void)result;
  (void)vn;
  (void)un;
  (void)m;
  (void)n;
  (void)ca;

  /* VL = n + 1 */
  /* sv.subfe un_j.v, product.v, un_j.v */

  for (int i = 0; i <= n; i++)
    {
      uint64_t value = (uint64_t) ~vn[i] + (uint64_t)un[i] + ca;
      ca             = value >> 32 != 0;
      result[i]      = (unsigned)value;
    }

  return ca;
}

/****************************************************************************/

bool
bigmulsub (unsigned long long qhat, unsigned vn[], unsigned un[],
           int j, int m, int n);

bool
bigmulsub (unsigned long long qhat, unsigned vn[], unsigned un[],
           int j, int m, int n)
{
  (void)qhat;
  (void)vn;
  (void)un;
  (void)j;
  (void)m;
  (void)n;

  /* Multiply and subtract. */

  uint32_t product[n + 1];

  (void)bigmul ( (uint32_t)qhat, product, vn, m, n );

  bool ca         = bigsub (un, product, un, m, n, true);
  bool need_fixup = !ca;

  return need_fixup;
}

/****************************************************************************/

/*
 * q[0], r[0], u[0], and v[0] contain the LEAST significant words.
 * (The sequence is in little-endian order).
 *
 * This is a fairly precise implementation of Knuth's Algorithm D, for a
 * binary computer with base b = 2**32. The caller supplies:
 *
 *   1. Space q for the quotient, m - n + 1 words (at least one).
 *   2. Space r for the remainder (optional), n words.
 *   3. The dividend u, m words, m >= 1.
 *   4. The divisor v, n words, n >= 2.
 *
 * The most significant digit of the divisor, v[n-1], must be nonzero.
 * The dividend u may have leading zeros; this just makes the algorithm
 * take longer and makes the quotient contain more leading zeros.
 * A value of NULL may be given for the address of the remainder to
 * signify that the caller does not want the remainder.
 *
 *  * The program does not alter the input parameters u and v.
 *
 *  * The quotient and remainder returned may have leading zeros.  The
 *    function itself returns a value of 0 for success and 1 for invalid
 *    parameters (e.g., division by 0).
 *
 *  * For now, we must have m >= n.  Knuth's Algorithm D also requires
 *    that the dividend be at least as long as the divisor.
 *    (In his terms: "m >= 0 (unstated), therefore, m+n >= n." )
 */

int
divmnu (unsigned q[], unsigned r[], const unsigned u[], const unsigned v[],
        int m, int n);

int
divmnu (unsigned q[], unsigned r[], const unsigned u[], const unsigned v[],
        int m, int n)
{
  const unsigned long long  b = 1LL << 32;   /* Number base (2**32).      */
  unsigned *un, *vn;                         /* Normalized form of u, v.  */
  unsigned long long qhat;                   /* Estimated quotient digit. */
  unsigned long long rhat;                   /* A remainder.              */
  unsigned long long p = 0UL;                /* Product of two digits.    */
  long long k, t = 0LL;
  int s, i, j;

  if (m < n || n <= 0 || v[n - 1] == 0)
    return 1;                                /* Return if invalid param. */

  if (n == 1)
    {
      k = 0;

      for (j = m - 1; j >= 0; j--)
        {
          uint64_t dig2 = ( (uint64_t)k << 32 ) | u[j];
          q[j]          = (unsigned int)(dig2 / v[0]);
          k             = dig2 % v[0];
        }

      if (r != NULL)
        r[0] = (unsigned)k;

      return 0;
    }

  /*
   * Normalize by shifting v left just enough so that its high-order
   * bit is on, and shift u left the same amount. We may have to append a
   * high-order digit on the dividend; we do that unconditionally.
   */

  s  = nlz (v[n - 1]);  /* 0 <= s <= 31. */
  vn = (unsigned *)alloca (4 * n);

  for (i = n - 1; i > 0; i--)
    vn[i] = (unsigned int)( (v[i] << s) |
              ( (unsigned long long)v[i - 1] >> (32 - s) ) );

  vn[0] = v[0] << s;

  un    = (unsigned *)alloca ( 4 * (m + 1) );
  un[m] = (unsigned int)( (unsigned long long)u[m - 1] >> (32 - s) );

  for (i = m - 1; i > 0; i--)
    un[i] = (unsigned int)( (u[i] << s) |
              ( (unsigned long long)u[i - 1] >> (32 - s) ) );

  un[0] = u[0] << s;

  for (j = m - n; j >= 0; j--)
    {

    uint32_t *un_j = &un[j];

    /* Compute estimate qhat of q[j] from top 2 digits. */

    uint64_t dig2  = ( (uint64_t)un[j + n] << 32 ) | un[j + n - 1];
    divrem_t qr    = divrem_64_by_32 (dig2, vn[n - 1]);
    qhat           = qr.q;
    rhat           = qr.r;

    if (qr.overflow)
      {

        /*
         * rhat can be bigger than 32-bit when the division overflows;
         * thus rhat computation cannot be folded into divrem_64_by_32
         */

        rhat = dig2 - (uint64_t)qr.q * vn[n - 1];
      }

again:

    /* Use 3rd-from-top digit to obtain better accuracy */

    if (rhat < b &&
     (unsigned)qhat * (unsigned long long)vn[n - 2] >
     b * rhat + un[j + n - 2])
      {
        qhat = qhat - 1;
        rhat = rhat + vn[n - 1];

        if (rhat < b)
          goto again;
    }

/****************************************************************************/

#ifdef ORIGINAL

    k = 0;

    for (i = 0; i < n; i++)
      {
        p = (unsigned)qhat * (unsigned long long)vn[i];

        t = (long long)( (long long)un[i + j] -
               (long long)k -
               (long long)(p & 0xFFFFFFFFLL) );

        un[i + j] = (unsigned)t;

        k  = (long long)( (long long)(p >> 32) -
               (long long)(t >> 32) );
      }

    t         = un[j + n] - k;
    un[j + n] = (unsigned)t;

    bool need_fixup = t < 0;

/****************************************************************************/

#elif defined(SUB_MUL_BORROW)
    (void)p;
    (void)t;

    uint32_t borrow = 0;

    for (int ii = 0; ii <= n; ii++)
      {
        uint32_t vn_i  = ii < n ? vn[ii] : 0;
        uint64_t value = un[ii + j] - (uint64_t)qhat * vn_i - borrow;
        borrow         = -(uint32_t)(value >> 32);
        un[ii + j]     = (uint32_t)value;
      }

    bool need_fixup = borrow != 0;

/****************************************************************************/

#elif defined(MUL_RSUB_CARRY)
    (void)p;
    (void)t;

    uint32_t carry = 1;

    for (int ii = 0; ii <= n; ii++)
      {
        uint32_t vn_i   = ii < n ? vn[ii] : 0;
        uint64_t result = un[ii + j] +
                            ~( (uint64_t)qhat * vn_i ) +
                            carry;

        uint32_t result_high = result >> 32;

        if (carry <= 1)
          result_high++;

        carry      = result_high;
        un[ii + j] = (uint32_t)result;
      }

    bool need_fixup = carry != 1;

/****************************************************************************/

#elif defined(SUB_MUL_BORROW_2_STAGE)
    (void)p;
    (void)t;

    uint32_t borrow = 0;
    uint32_t phi[2000];
    uint32_t plo[2000];

    /*
     * First, perform mul-and-sub and store in split hi-lo
     * this shows the vectorised sv.msubx which stores 128-bit in
     * two 64-bit registers
     */

    for (int ii = 0; ii <= n; ii++)
      {
        uint32_t vn_i  = ii < n ? vn[ii] : 0;
        uint64_t value = un[ii + j] - (uint64_t)qhat * vn_i;
        plo[ii]        = value & 0xffffffffLL;
        phi[ii]        = value >> 32;
      }

    /*
     * Second, reconstruct the 64-bit result, subtract borrow,
     * store top-half (-ve) in new borrow and store low-half as answer
     * this is the new (odd) instruction
     */

    for (int ii = 0; ii <= n; ii++)
      {
        uint64_t value = ( ( (uint64_t)phi[ii] << 32 ) |
                           plo[ii] ) - borrow;
        borrow         = ~(value >> 32) + 1;
        un[ii + j]     = (uint32_t)value;
      }

    bool need_fixup = borrow != 0;

/****************************************************************************/

#elif defined(MUL_RSUB_CARRY_2_STAGE)
    (void)p;
    (void)t;

    uint32_t carry = 1;
    uint32_t phi[2000];
    uint32_t plo[2000];

    for (int ii = 0; ii <= n; ii++)
      {
        uint32_t vn_i  = ii < n ? vn[ii] : 0;
        uint64_t value = un[ii + j] + ~( (uint64_t)qhat * vn_i );
        plo[ii]        = value & 0xffffffffLL;
        phi[ii]        = value >> 32;
      }

    for (int ii = 0; ii <= n; ii++)
      {
        uint64_t result = ( ( (uint64_t)phi[ii] << 32 ) |
                            plo[ii] ) +
                            carry;

        uint32_t result_high  = result >> 32;

        if (carry <= 1)
          result_high++;

        carry      = result_high;
        un[ii + j] = (uint32_t)result;
      }

    bool need_fixup = carry != 1;

/****************************************************************************/

#elif defined(MUL_RSUB_CARRY_2_STAGE1)
    (void)p;
    (void)t;

    uint32_t carry = 1;
    uint32_t phi[2000];
    uint32_t plo[2000];

    /*
     * Same mul-and-sub as SUB_MUL_BORROW but not the same
     * mul-and-sub-minus-one as MUL_RSUB_CARRY
     */

    for (int ii = 0; ii <= n; ii++)
      {
        uint32_t vn_i  = ii < n ? vn[ii] : 0;
        uint64_t value = un[ii + j] - ( (uint64_t)qhat * vn_i );
        plo[ii]         = value & 0xffffffffLL;
        phi[ii]         = value >> 32;
      }

    /*
     * Compensate for the +1 that was added by mul-and-sub
     * by subtracting it here ( as ~(0) ).
     */

    for (int ii = 0; ii <= n; ii++)
      {
        uint64_t result = (uint64_t)( ( ( (uint64_t)phi[ii] << 32 ) |
                            (uint64_t)plo[ii] ) +
                            (unsigned long)carry +
                            (unsigned long)( ~(0L) ) );  /* i.e. "-1" */

        uint32_t result_high = result >> 32;

        if (carry <= 1)
          result_high++;

        carry      = result_high;
        un[ii + j] = (uint32_t)result;
      }

    bool need_fixup = carry != 1;

/****************************************************************************/

#elif defined(MUL_RSUB_CARRY_2_STAGE2)
    (void)p;
    (void)t;

    uint32_t carry = 0;
    uint32_t phi[2000];
    uint32_t plo[2000];

    /*
     * Same mul-and-sub as SUB_MUL_BORROW but not the same
     * mul-and-sub-minus-one as MUL_RSUB_CARRY
     */

    for (int ii = 0; ii <= n; ii++)
      {
        uint32_t vn_i  = ii < n ? vn[ii] : 0;
        uint64_t value = un[ii + j] - ( (uint64_t)qhat * vn_i );
        plo[ii]        = value & 0xffffffffLL;
        phi[ii]        = value >> 32;
      }

    for (int ii = 0; ii <= n; ii++)
      {
        uint64_t result = ( ( (uint64_t)phi[ii] << 32 ) |
                            plo[ii] ) +
                            carry;

        uint32_t result_high = result >> 32;

        if (carry == 0)
          carry = result_high;
        else
          carry = result_high - 1;

        un[ii + j] = (uint32_t)result;
      }

    bool need_fixup = carry != 0;

/****************************************************************************/

#elif defined(MADDED_SUBFE)
    (void)p;
    (void)t;

    uint32_t carry = 0;
    uint32_t product[n + 1];

    /* VL = n + 1 */
    /* sv.madded product.v, vn.v, qhat.s, carry.s */

    for (int ii = 0; ii <= n; ii++)
      {
        uint32_t vn_v  = ii < n ? vn[ii] : 0;
        uint64_t value = (uint64_t)vn_v * (uint64_t)qhat + carry;
        carry          = (uint32_t)(value >> 32);
        product[ii]    = (uint32_t)value;
      }

    bool ca = true;

    /* VL = n + 1 */
    /* sv.subfe un_j.v, product.v, un_j.v */

    for (int ii = 0; ii <= n; ii++)
      {
        uint64_t value = (uint64_t) ~product[ii] + (uint64_t)un_j[ii] + ca;
        ca             = value >> 32 != 0;
        un_j[ii]       = (unsigned)value;
      }

    bool need_fixup = !ca;

/****************************************************************************/

#else
# error No algorithm selected
#endif

/****************************************************************************/

    q[j] = (unsigned)qhat;  /* Store quotient digit. */

    if (need_fixup)
      {                     /* If we subtracted too */
        q[j] = q[j] - 1;    /* much, add it back.   */

        (void)bigadd (un_j, vn, un_j, m, n, 0);
      }

  }  /* End j. */

  /*
   * If the caller wants the remainder,
   * unnormalize it and pass it back.
   */

  if (r != NULL)
    {
      for (i = 0; i < n - 1; i++)
        r[i] = (unsigned int)( (un[i] >> s) |
                 ( (unsigned long long)un[i + 1] << (32 - s) ) );

      r[n - 1] = un[n - 1] >> s;
    }

  return 0;
}

/****************************************************************************/

void
check (unsigned q[], unsigned r[], unsigned u[], unsigned v[], int m, int n,
       unsigned cq[], unsigned cr[]);

void
check (unsigned q[], unsigned r[], unsigned u[], unsigned v[], int m, int n,
       unsigned cq[], unsigned cr[])
{
  int i, szq;

  szq = kd_div_max (m - n + 1, 1);

  for (i = 0; i < szq; i++)
    {
      if (q[i] != cq[i])
        {
          dumpit ("FATAL ERROR: dividend u =", m, u);
          dumpit ("             divisor  v =", n, v);
          dumpit ("              remainder =", m - n + 1, q);
          dumpit ("              should be =", m - n + 1, cq);

          kd_div_errors++;

          return;
        }
    }

  for (i = 0; i < n; i++)
    {
      if (r[i] != cr[i])
        {
          dumpit ("FATAL ERROR: dividend u =", m, u);
          dumpit ("             divisor  v =", n, v);
          dumpit ("              remainder =", n, r);
          dumpit ("              should be =", n, cr);

          kd_div_errors++;

          return;
        }
    }

  return;
}

/****************************************************************************/

int
main(void);

int
main (void)
{
  static struct
  {
    int m;
    int n;
    uint32_t  u[10];
    uint32_t  v[10];
    uint32_t cq[10];
    uint32_t cr[10];
    bool error;
    char pad[3];
  } test[] = {

    { .m     =   3  ,
      .n     =   1  ,
      .u     = { 3 },
      .v     = { 0 },
      .cq    = { 1 },
      .cr    = { 1 },
      .error = true
    },

    { .m     =      1  ,
      .n     =      2  ,
      .u     = { 7    },
      .v     = { 1, 3 },
      .cq    = {    0 },
      .cr    = { 7, 0 },
      .error = true
    },

    { .m     =      2  ,
      .n     =      2  ,
      .u     = { 0, 0 },
      .v     = { 1, 0 },
      .cq    = {    0 },
      .cr    = { 0, 0 },
      .error = true
    },

    { .m     =   1  ,
      .n     =   1  ,
      .u     = { 3 },
      .v     = { 2 },
      .cq    = { 1 },
      .cr    = { 1 }
    },

    { .m     =   1  ,
      .n     =   1  ,
      .u     = { 3 },
      .v     = { 3 },
      .cq    = { 1 },
      .cr    = { 0 }
    },

    { .m     =   1  ,
      .n     =   1  ,
      .u     = { 3 },
      .v     = { 4 },
      .cq    = { 0 },
      .cr    = { 3 }
    },

    { .m     =            1  ,
      .n     =            1  ,
      .u     = {          0 },
      .v     = { 0xffffffff },
      .cq    = {          0 },
      .cr    = {          0 }
    },

    { .m     =            1  ,
      .n     =            1  ,
      .u     = { 0xffffffff },
      .v     = {          1 },
      .cq    = { 0xffffffff },
      .cr    = {          0 }
    },

    { .m     =            1  ,
      .n     =            1  ,
      .u     = { 0xffffffff },
      .v     = { 0xffffffff },
      .cq    = {          1 },
      .cr    = {          0 }
    },

    { .m     =            1  ,
      .n     =            1  ,
      .u     = { 0xffffffff },
      .v     = {          3 },
      .cq    = { 0x55555555 },
      .cr    = {          0 }
    },

    { .m     =                        2  ,
      .n     =                        1  ,
      .u     = { 0xffffffff, 0xffffffff },
      .v     = {                      1 },
      .cq    = { 0xffffffff, 0xffffffff },
      .cr    = {                      0 }
    },

    { .m     =                        2  ,
      .n     =                        1  ,
      .u     = { 0xffffffff, 0xffffffff },
      .v     = {             0xffffffff },
      .cq    = {          1,          1 },
      .cr    = {                      0 }
    },

    { .m     =                        2  ,
      .n     =                        1  ,
      .u     = { 0xffffffff, 0xfffffffe },
      .v     = {             0xffffffff },
      .cq    = { 0xffffffff,          0 },
      .cr    = {             0xfffffffe }
    },

    { .m     =                        2  ,
      .n     =                        1  ,
      .u     = { 0x00005678, 0x00001234 },
      .v     = {             0x00009abc },
      .cq    = { 0x1e1dba76,          0 },
      .cr    = {                 0x6bd0 }
    },

    { .m     =      2  ,
      .n     =      2  ,
      .u     = { 0, 0 },
      .v     = { 0, 1 },
      .cq    = {    0 },
      .cr    = { 0, 0 }
    },

    { .m     =      2  ,
      .n     =      2  ,
      .u     = { 0, 7 },
      .v     = { 0, 3 },
      .cq    = {    2 },
      .cr    = { 0, 1 }
    },

    { .m     =      2  ,
      .n     =      2  ,
      .u     = { 5, 7 },
      .v     = { 0, 3 },
      .cq    = {    2 },
      .cr    = { 5, 1 }
    },

    { .m     =      2  ,
      .n     =      2  ,
      .u     = { 0, 6 },
      .v     = { 0, 2 },
      .cq    = {    3 },
      .cr    = { 0, 0 }
    },

    { .m     =            1  ,
      .n     =            1  ,
      .u     = { 0x80000000 },
      .v     = { 0x40000001 },
      .cq    = { 0x00000001 },
      .cr    = { 0x3fffffff }
    },

    { .m     =                        2  ,
      .n     =                        1  ,
      .u     = { 0x00000000, 0x80000000 },
      .v     = {             0x40000001 },
      .cq    = { 0xfffffff8, 0x00000001 },
      .cr    = {             0x00000008 }
    },

    { .m     =                        2  ,
      .n     =                        2  ,
      .u     = { 0x00000000, 0x80000000 },
      .v     = { 0x00000001, 0x40000000 },
      .cq    = {             0x00000001 },
      .cr    = { 0xffffffff, 0x3fffffff }
    },

    { .m     =                        2  ,
      .n     =                        2  ,
      .u     = { 0x0000789a, 0x0000bcde },
      .v     = { 0x0000789a, 0x0000bcde },
      .cq    = {                      1 },
      .cr    = {          0,          0 }
    },

    { .m     =                        2  ,
      .n     =                        2  ,
      .u     = { 0x0000789b, 0x0000bcde },
      .v     = { 0x0000789a, 0x0000bcde },
      .cq    = {                      1 },
      .cr    = {          1,          0 }
    },

    { .m     =                        2  ,
      .n     =                        2  ,
      .u     = { 0x00007899, 0x0000bcde },
      .v     = { 0x0000789a, 0x0000bcde },
      .cq    = {                      0 },
      .cr    = { 0x00007899, 0x0000bcde }
    },

    { .m     =                        2  ,
      .n     =                        2  ,
      .u     = { 0x0000ffff, 0x0000ffff },
      .v     = { 0x0000ffff, 0x0000ffff },
      .cq    = {                      1 },
      .cr    = {          0,          0 }
    },

    { .m     =                        2  ,
      .n     =                        2  ,
      .u     = { 0x0000ffff, 0x0000ffff },
      .v     = { 0x00000000, 0x00000001 },
      .cq    = {             0x0000ffff },
      .cr    = { 0x0000ffff,          0 }
    },

    { .m     =                                    3  ,
      .n     =                                    2  ,
      .u     = { 0x000089ab, 0x00004567, 0x00000123 },
      .v     = { 0x00000000, 0x00000001             },
      .cq    = { 0x00004567, 0x00000123             },
      .cr    = {             0x000089ab,          0 }
    },

    { .m     =                                    3  ,
      .n     =                                    2  ,
      .u     = { 0x00000000, 0x0000fffe, 0x00008000 },
      .v     = {             0x0000ffff, 0x00008000 },
      .cq    = {             0xffffffff, 0x00000000 },
      .cr    = {             0x0000ffff, 0x00007fff }
    },

    { .m     =                                    3  ,
      .n     =                                    3  ,
      .u     = { 0x00000003, 0x00000000, 0x80000000 },
      .v     = { 0x00000001, 0x00000000, 0x20000000 },
      .cq    = {                         0x00000003 },
      .cr    = {          0,          0, 0x20000000 }
    },

    { .m     =                                    3  ,
      .n     =                                    3  ,
      .u     = { 0x00000003, 0x00000000, 0x00008000 },
      .v     = { 0x00000001, 0x00000000, 0x00002000 },
      .cq    = {                         0x00000003 },
      .cr    = {          0,          0, 0x00002000 }
    },

    { .m     =                                                4  ,
      .n     =                                                3  ,
      .u     = {          0,          0, 0x00008000, 0x00007fff },
      .v     = {                      1,          0, 0x00008000 },
      .cq    = {                         0xfffe0000,          0 },
      .cr    = {             0x00020000, 0xffffffff, 0x00007fff}
    },

    { .m     =                                                4  ,
      .n     =                                                3  ,
      .u     = {          0, 0x0000fffe,          0, 0x00008000 },
      .v     = {             0x0000ffff,          0, 0x00008000 },
      .cq    = {                         0xffffffff,          0 },
      .cr    = {             0x0000ffff, 0xffffffff, 0x00007fff }
    },

    { .m     =                                                4  ,
      .n     =                                                3  ,
      .u     = {          0, 0xfffffffe,          0, 0x80000000 },
      .v     = {             0x0000ffff,          0, 0x80000000 },
      .cq    = {                         0x00000000,          1 },
      .cr    = {             0x00000000, 0xfffeffff, 0x00000000 }
    },

    { .m     =                                                4  ,
      .n     =                                                3  ,
      .u     = {          0, 0xfffffffe,          0, 0x80000000 },
      .v     = {             0xffffffff,          0, 0x80000000 },
      .cq    = {                         0xffffffff,          0 },
      .cr    = {             0xffffffff, 0xffffffff, 0x7fffffff }
    },
  };

  int ferror = 0;
  unsigned q[10], r[10];
  const int ncases = sizeof (test) / sizeof (test[0]);
  const long loops = 12000000L;

  for (long l = 0L; l < loops; l++)
    for (int i = 0; i < ncases; i++)
      {
        int m        = test[i].m;
        int n        = test[i].n;
        uint32_t *u  = test[i].u;
        uint32_t *v  = test[i].v;
        uint32_t *cq = test[i].cq;
        uint32_t *cr = test[i].cr;

        int f = divmnu (q, r, u, v, m, n);

        if (f && !test[i].error)
          {
            if (ferror < 1)
              {
                dumpit ("FATAL: Unexpected error for dividend u =", m, u);
                dumpit ("                            divisor  v =", n, v);
              }

            if (ferror > 0)
              kd_div_errors++;

            ferror++;
          }

        else if (!f && test[i].error)
          {
            if (ferror < 1)
              {
                dumpit ("FATAL: Unexpected success for dividend u =", m, u);
                dumpit ("                              divisor  v =", n, v);
              }

            if (ferror > 0)
              kd_div_errors++;

            ferror++;
          }

        if (!f)
          check (q, r, u, v, m, n, cq, cr);
      }

  return (int)kd_div_errors;
}

/****************************************************************************/
