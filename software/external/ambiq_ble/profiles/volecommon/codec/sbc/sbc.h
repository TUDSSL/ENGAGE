/*
 *
 *  Bluetooth low-complexity, subband codec (SBC) library
 *
 *  Copyright (C) 2008-2010  Nokia Corporation
 *  Copyright (C) 2012-2014  Intel Corporation
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2004-2005  Henryk Ploetz <henryk@ploetzli.ch>
 *  Copyright (C) 2005-2006  Brad Midgley <bmidgley@xmission.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __SBC_H
#define __SBC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
//#include <sys/types.h>
#include <limits.h>

/* sampling frequency */
#define SBC_FREQ_16000      0x00
#define SBC_FREQ_32000      0x01
#define SBC_FREQ_44100      0x02
#define SBC_FREQ_48000      0x03

/* blocks */
#define SBC_BLK_4       0x00
#define SBC_BLK_8       0x01
#define SBC_BLK_12      0x02
#define SBC_BLK_16      0x03

/* channel mode */
#define SBC_MODE_MONO       0x00
#define SBC_MODE_DUAL_CHANNEL   0x01
#define SBC_MODE_STEREO     0x02
#define SBC_MODE_JOINT_STEREO   0x03

/* allocation method */
#define SBC_AM_LOUDNESS     0x00
#define SBC_AM_SNR      0x01

/* subbands */
#define SBC_SB_4        0x00
#define SBC_SB_8        0x01

/* data endianess */
#define SBC_LE          0x00
#define SBC_BE          0x01

#ifndef _SSIZE_T_DEFINED
#ifdef  _WIN64
typedef __int64    ssize_t;
#else
typedef  int   ssize_t;
#endif
#define _SSIZE_T_DEFINED
#endif
#define SBC_EXPORT
#define SBC_X_BUFFER_SIZE 328
#define SCALE_OUT_BITS 15

//#define SBC_ALIGNED
#define SBC_ALWAYS_INLINE inline
#define SBCDEC_FIXED_EXTRA_BITS 2
#define SBC_ALIGN_BITS 4
#define SBC_ALIGN_MASK ((1 << (SBC_ALIGN_BITS)) - 1)

#define fabs(x) ((x) < 0 ? -(x) : (x))
/* C does not provide an explicit arithmetic shift right but this will
   always be correct and every compiler *should* generate optimal code */
#define ASR(val, bits) ((-2 >> 1 == -1) ? \
         ((int32_t)(val)) >> (bits) : ((int32_t) (val)) / (1 << (bits)))

#define SCALE_SPROTO4_TBL   12
#define SCALE_SPROTO8_TBL   14
#define SCALE_NPROTO4_TBL   11
#define SCALE_NPROTO8_TBL   11
#define SCALE4_STAGED1_BITS 15
#define SCALE4_STAGED2_BITS 16
#define SCALE8_STAGED1_BITS 15
#define SCALE8_STAGED2_BITS 16

typedef int32_t sbc_fixed_t;

#define SCALE4_STAGED1(src) ASR(src, SCALE4_STAGED1_BITS)
#define SCALE4_STAGED2(src) ASR(src, SCALE4_STAGED2_BITS)
#define SCALE8_STAGED1(src) ASR(src, SCALE8_STAGED1_BITS)
#define SCALE8_STAGED2(src) ASR(src, SCALE8_STAGED2_BITS)


#define MULA(a, b, res)  ((a) * (b) + (res))
#define MUL(a, b)        ((a) * (b))
#define SBC_FIXED_0(val) { val = 0; }

#define FIXED_A int32_t /* data type for fixed point accumulator */
#define FIXED_T int16_t /* data type for fixed point constants */
#define SBC_FIXED_EXTRA_BITS 0

#define SS4(val) ASR(val, SCALE_SPROTO4_TBL)
#define SS8(val) ASR(val, SCALE_SPROTO8_TBL)
#define SN4(val) ASR(val, SCALE_NPROTO4_TBL + 1 + SBCDEC_FIXED_EXTRA_BITS)
#define SN8(val) ASR(val, SCALE_NPROTO8_TBL + 1 + SBCDEC_FIXED_EXTRA_BITS)

#define SBC_PROTO_FIXED4_SCALE \
    ((sizeof(FIXED_T) * CHAR_BIT - 1) - SBC_FIXED_EXTRA_BITS + 1)
#define F_PROTO4(x) (FIXED_A) ((x * 2) * \
    ((FIXED_A) 1 << (sizeof(FIXED_T) * CHAR_BIT - 1)) + 0.5)
#define F(x) F_PROTO4(x)
static const FIXED_T _sbc_proto_fixed4[40] =
{
    F(0.00000000E+00),  F(5.36548976E-04),
    -F(1.49188357E-03),  F(2.73370904E-03),
    F(3.83720193E-03),  F(3.89205149E-03),
    F(1.86581691E-03),  F(3.06012286E-03),

    F(1.09137620E-02),  F(2.04385087E-02),
    -F(2.88757392E-02),  F(3.21939290E-02),
    F(2.58767811E-02),  F(6.13245186E-03),
    -F(2.88217274E-02),  F(7.76463494E-02),

    F(1.35593274E-01),  F(1.94987841E-01),
    -F(2.46636662E-01),  F(2.81828203E-01),
    F(2.94315332E-01),  F(2.81828203E-01),
    F(2.46636662E-01), -F(1.94987841E-01),

    -F(1.35593274E-01), -F(7.76463494E-02),
    F(2.88217274E-02),  F(6.13245186E-03),
    F(2.58767811E-02),  F(3.21939290E-02),
    F(2.88757392E-02), -F(2.04385087E-02),

    -F(1.09137620E-02), -F(3.06012286E-03),
    -F(1.86581691E-03),  F(3.89205149E-03),
    F(3.83720193E-03),  F(2.73370904E-03),
    F(1.49188357E-03), -F(5.36548976E-04),
};
#undef F

/*
 * To produce this cosine matrix in Octave:
 *
 * b = zeros(4, 8);
 * for i = 0:3
 * for j = 0:7 b(i+1, j+1) = cos((i + 0.5) * (j - 2) * (pi/4))
 * endfor
 * endfor;
 * printf("%.10f, ", b');
 *
 * Note: in each block of 8 numbers sign was changed for elements 2 and 7
 *
 * Change of sign for element 2 allows to replace constant 1.0 (not
 * representable in Q15 format) with -1.0 (fine with Q15).
 * Changed sign for element 7 allows to have more similar constants
 * and simplify subband filter function code.
 */
#define SBC_COS_TABLE_FIXED4_SCALE \
    ((sizeof(FIXED_T) * CHAR_BIT - 1) + SBC_FIXED_EXTRA_BITS)
#define F_COS4(x) (FIXED_A) ((x) * \
    ((FIXED_A) 1 << (sizeof(FIXED_T) * CHAR_BIT - 1)) + 0.5)
#define F(x) F_COS4(x)
static const FIXED_T cos_table_fixed_4[32] =
{
    F(0.7071067812),  F(0.9238795325), -F(1.0000000000),  F(0.9238795325),
    F(0.7071067812),  F(0.3826834324),  F(0.0000000000),  F(0.3826834324),

    -F(0.7071067812),  F(0.3826834324), -F(1.0000000000),  F(0.3826834324),
    -F(0.7071067812), -F(0.9238795325), -F(0.0000000000), -F(0.9238795325),

    -F(0.7071067812), -F(0.3826834324), -F(1.0000000000), -F(0.3826834324),
    -F(0.7071067812),  F(0.9238795325),  F(0.0000000000),  F(0.9238795325),

    F(0.7071067812), -F(0.9238795325), -F(1.0000000000), -F(0.9238795325),
    F(0.7071067812), -F(0.3826834324), -F(0.0000000000), -F(0.3826834324),
};
#undef F

/* A2DP specification: Section 12.8 Tables
 *
 * Original values are premultiplied by 4 for better precision (that is the
 * maximum which is possible without overflows)
 *
 * Note: in each block of 16 numbers sign was changed for elements 4, 13, 14, 15
 * in order to compensate the same change applied to cos_table_fixed_8
 */
#define SBC_PROTO_FIXED8_SCALE \
    ((sizeof(FIXED_T) * CHAR_BIT - 1) - SBC_FIXED_EXTRA_BITS + 1)
#define F_PROTO8(x) (FIXED_A) ((x * 2) * \
    ((FIXED_A) 1 << (sizeof(FIXED_T) * CHAR_BIT - 1)) + 0.5)
#define F(x) F_PROTO8(x)
static const FIXED_T _sbc_proto_fixed8[80] =
{
    F(0.00000000E+00),  F(1.56575398E-04),
    F(3.43256425E-04),  F(5.54620202E-04),
    -F(8.23919506E-04),  F(1.13992507E-03),
    F(1.47640169E-03),  F(1.78371725E-03),
    F(2.01182542E-03),  F(2.10371989E-03),
    F(1.99454554E-03),  F(1.61656283E-03),
    F(9.02154502E-04),  F(1.78805361E-04),
    F(1.64973098E-03),  F(3.49717454E-03),

    F(5.65949473E-03),  F(8.02941163E-03),
    F(1.04584443E-02),  F(1.27472335E-02),
    -F(1.46525263E-02),  F(1.59045603E-02),
    F(1.62208471E-02),  F(1.53184106E-02),
    F(1.29371806E-02),  F(8.85757540E-03),
    F(2.92408442E-03), -F(4.91578024E-03),
    -F(1.46404076E-02),  F(2.61098752E-02),
    F(3.90751381E-02),  F(5.31873032E-02),

    F(6.79989431E-02),  F(8.29847578E-02),
    F(9.75753918E-02),  F(1.11196689E-01),
    -F(1.23264548E-01),  F(1.33264415E-01),
    F(1.40753505E-01),  F(1.45389847E-01),
    F(1.46955068E-01),  F(1.45389847E-01),
    F(1.40753505E-01),  F(1.33264415E-01),
    F(1.23264548E-01), -F(1.11196689E-01),
    -F(9.75753918E-02), -F(8.29847578E-02),

    -F(6.79989431E-02), -F(5.31873032E-02),
    -F(3.90751381E-02), -F(2.61098752E-02),
    F(1.46404076E-02), -F(4.91578024E-03),
    F(2.92408442E-03),  F(8.85757540E-03),
    F(1.29371806E-02),  F(1.53184106E-02),
    F(1.62208471E-02),  F(1.59045603E-02),
    F(1.46525263E-02), -F(1.27472335E-02),
    -F(1.04584443E-02), -F(8.02941163E-03),

    -F(5.65949473E-03), -F(3.49717454E-03),
    -F(1.64973098E-03), -F(1.78805361E-04),
    -F(9.02154502E-04),  F(1.61656283E-03),
    F(1.99454554E-03),  F(2.10371989E-03),
    F(2.01182542E-03),  F(1.78371725E-03),
    F(1.47640169E-03),  F(1.13992507E-03),
    F(8.23919506E-04), -F(5.54620202E-04),
    -F(3.43256425E-04), -F(1.56575398E-04),
};
#undef F

/*
 * To produce this cosine matrix in Octave:
 *
 * b = zeros(8, 16);
 * for i = 0:7
 * for j = 0:15 b(i+1, j+1) = cos((i + 0.5) * (j - 4) * (pi/8))
 * endfor endfor;
 * printf("%.10f, ", b');
 *
 * Note: in each block of 16 numbers sign was changed for elements 4, 13, 14, 15
 *
 * Change of sign for element 4 allows to replace constant 1.0 (not
 * representable in Q15 format) with -1.0 (fine with Q15).
 * Changed signs for elements 13, 14, 15 allow to have more similar constants
 * and simplify subband filter function code.
 */
#define SBC_COS_TABLE_FIXED8_SCALE \
    ((sizeof(FIXED_T) * CHAR_BIT - 1) + SBC_FIXED_EXTRA_BITS)
#define F_COS8(x) (FIXED_A) ((x) * \
    ((FIXED_A) 1 << (sizeof(FIXED_T) * CHAR_BIT - 1)) + 0.5)
#define F(x) F_COS8(x)
static const FIXED_T cos_table_fixed_8[128] =
{
    F(0.7071067812),  F(0.8314696123),  F(0.9238795325),  F(0.9807852804),
    -F(1.0000000000),  F(0.9807852804),  F(0.9238795325),  F(0.8314696123),
    F(0.7071067812),  F(0.5555702330),  F(0.3826834324),  F(0.1950903220),
    F(0.0000000000),  F(0.1950903220),  F(0.3826834324),  F(0.5555702330),

    -F(0.7071067812), -F(0.1950903220),  F(0.3826834324),  F(0.8314696123),
    -F(1.0000000000),  F(0.8314696123),  F(0.3826834324), -F(0.1950903220),
    -F(0.7071067812), -F(0.9807852804), -F(0.9238795325), -F(0.5555702330),
    -F(0.0000000000), -F(0.5555702330), -F(0.9238795325), -F(0.9807852804),

    -F(0.7071067812), -F(0.9807852804), -F(0.3826834324),  F(0.5555702330),
    -F(1.0000000000),  F(0.5555702330), -F(0.3826834324), -F(0.9807852804),
    -F(0.7071067812),  F(0.1950903220),  F(0.9238795325),  F(0.8314696123),
    F(0.0000000000),  F(0.8314696123),  F(0.9238795325),  F(0.1950903220),

    F(0.7071067812), -F(0.5555702330), -F(0.9238795325),  F(0.1950903220),
    -F(1.0000000000),  F(0.1950903220), -F(0.9238795325), -F(0.5555702330),
    F(0.7071067812),  F(0.8314696123), -F(0.3826834324), -F(0.9807852804),
    -F(0.0000000000), -F(0.9807852804), -F(0.3826834324),  F(0.8314696123),

    F(0.7071067812),  F(0.5555702330), -F(0.9238795325), -F(0.1950903220),
    -F(1.0000000000), -F(0.1950903220), -F(0.9238795325),  F(0.5555702330),
    F(0.7071067812), -F(0.8314696123), -F(0.3826834324),  F(0.9807852804),
    F(0.0000000000),  F(0.9807852804), -F(0.3826834324), -F(0.8314696123),

    -F(0.7071067812),  F(0.9807852804), -F(0.3826834324), -F(0.5555702330),
    -F(1.0000000000), -F(0.5555702330), -F(0.3826834324),  F(0.9807852804),
    -F(0.7071067812), -F(0.1950903220),  F(0.9238795325), -F(0.8314696123),
    -F(0.0000000000), -F(0.8314696123),  F(0.9238795325), -F(0.1950903220),

    -F(0.7071067812),  F(0.1950903220),  F(0.3826834324), -F(0.8314696123),
    -F(1.0000000000), -F(0.8314696123),  F(0.3826834324),  F(0.1950903220),
    -F(0.7071067812),  F(0.9807852804), -F(0.9238795325),  F(0.5555702330),
    -F(0.0000000000),  F(0.5555702330), -F(0.9238795325),  F(0.9807852804),

    F(0.7071067812), -F(0.8314696123),  F(0.9238795325), -F(0.9807852804),
    -F(1.0000000000), -F(0.9807852804),  F(0.9238795325), -F(0.8314696123),
    F(0.7071067812), -F(0.5555702330),  F(0.3826834324), -F(0.1950903220),
    -F(0.0000000000), -F(0.1950903220),  F(0.3826834324), -F(0.5555702330),
};
#undef F

/*
 * Enforce 16 byte alignment for the data, which is supposed to be used
 * with SIMD optimized code.
 */

#define SBC_ALIGN_BITS 4
#define SBC_ALIGN_MASK ((1 << (SBC_ALIGN_BITS)) - 1)

#ifdef __GNUC__
#define SBC_ALIGNED __attribute__((aligned(1 << (SBC_ALIGN_BITS))))
#else
#define SBC_ALIGNED
#endif

/*
 * Constant tables for the use in SIMD optimized analysis filters
 * Each table consists of two parts:
 * 1. reordered "proto" table
 * 2. reordered "cos" table
 *
 * Due to non-symmetrical reordering, separate tables for "even"
 * and "odd" cases are needed
 */

static const FIXED_T SBC_ALIGNED analysis_consts_fixed4_simd_even[40 + 16] =
{
#define C0 1.0932568993
#define C1 1.3056875580
#define C2 1.3056875580
#define C3 1.6772280856

#define F(x) F_PROTO4(x)
     F(0.00000000E+00 * C0),  F(3.83720193E-03 * C0),
     F(5.36548976E-04 * C1),  F(2.73370904E-03 * C1),
     F(3.06012286E-03 * C2),  F(3.89205149E-03 * C2),
     F(0.00000000E+00 * C3), -F(1.49188357E-03 * C3),
     F(1.09137620E-02 * C0),  F(2.58767811E-02 * C0),
     F(2.04385087E-02 * C1),  F(3.21939290E-02 * C1),
     F(7.76463494E-02 * C2),  F(6.13245186E-03 * C2),
     F(0.00000000E+00 * C3), -F(2.88757392E-02 * C3),
     F(1.35593274E-01 * C0),  F(2.94315332E-01 * C0),
     F(1.94987841E-01 * C1),  F(2.81828203E-01 * C1),
    -F(1.94987841E-01 * C2),  F(2.81828203E-01 * C2),
     F(0.00000000E+00 * C3), -F(2.46636662E-01 * C3),
    -F(1.35593274E-01 * C0),  F(2.58767811E-02 * C0),
    -F(7.76463494E-02 * C1),  F(6.13245186E-03 * C1),
    -F(2.04385087E-02 * C2),  F(3.21939290E-02 * C2),
     F(0.00000000E+00 * C3),  F(2.88217274E-02 * C3),
    -F(1.09137620E-02 * C0),  F(3.83720193E-03 * C0),
    -F(3.06012286E-03 * C1),  F(3.89205149E-03 * C1),
    -F(5.36548976E-04 * C2),  F(2.73370904E-03 * C2),
     F(0.00000000E+00 * C3), -F(1.86581691E-03 * C3),
#undef F
#define F(x) F_COS4(x)
     F(0.7071067812 / C0),  F(0.9238795325 / C1),
    -F(0.7071067812 / C0),  F(0.3826834324 / C1),
    -F(0.7071067812 / C0), -F(0.3826834324 / C1),
     F(0.7071067812 / C0), -F(0.9238795325 / C1),
     F(0.3826834324 / C2), -F(1.0000000000 / C3),
    -F(0.9238795325 / C2), -F(1.0000000000 / C3),
     F(0.9238795325 / C2), -F(1.0000000000 / C3),
    -F(0.3826834324 / C2), -F(1.0000000000 / C3),
#undef F

#undef C0
#undef C1
#undef C2
#undef C3
};

static const FIXED_T SBC_ALIGNED analysis_consts_fixed4_simd_odd[40 + 16] =
{
#define C0 1.3056875580
#define C1 1.6772280856
#define C2 1.0932568993
#define C3 1.3056875580

#define F(x) F_PROTO4(x)
     F(2.73370904E-03 * C0),  F(5.36548976E-04 * C0),
    -F(1.49188357E-03 * C1),  F(0.00000000E+00 * C1),
     F(3.83720193E-03 * C2),  F(1.09137620E-02 * C2),
     F(3.89205149E-03 * C3),  F(3.06012286E-03 * C3),
     F(3.21939290E-02 * C0),  F(2.04385087E-02 * C0),
    -F(2.88757392E-02 * C1),  F(0.00000000E+00 * C1),
     F(2.58767811E-02 * C2),  F(1.35593274E-01 * C2),
     F(6.13245186E-03 * C3),  F(7.76463494E-02 * C3),
     F(2.81828203E-01 * C0),  F(1.94987841E-01 * C0),
    -F(2.46636662E-01 * C1),  F(0.00000000E+00 * C1),
     F(2.94315332E-01 * C2), -F(1.35593274E-01 * C2),
     F(2.81828203E-01 * C3), -F(1.94987841E-01 * C3),
     F(6.13245186E-03 * C0), -F(7.76463494E-02 * C0),
     F(2.88217274E-02 * C1),  F(0.00000000E+00 * C1),
     F(2.58767811E-02 * C2), -F(1.09137620E-02 * C2),
     F(3.21939290E-02 * C3), -F(2.04385087E-02 * C3),
     F(3.89205149E-03 * C0), -F(3.06012286E-03 * C0),
    -F(1.86581691E-03 * C1),  F(0.00000000E+00 * C1),
     F(3.83720193E-03 * C2),  F(0.00000000E+00 * C2),
     F(2.73370904E-03 * C3), -F(5.36548976E-04 * C3),
#undef F
#define F(x) F_COS4(x)
     F(0.9238795325 / C0), -F(1.0000000000 / C1),
     F(0.3826834324 / C0), -F(1.0000000000 / C1),
    -F(0.3826834324 / C0), -F(1.0000000000 / C1),
    -F(0.9238795325 / C0), -F(1.0000000000 / C1),
     F(0.7071067812 / C2),  F(0.3826834324 / C3),
    -F(0.7071067812 / C2), -F(0.9238795325 / C3),
    -F(0.7071067812 / C2),  F(0.9238795325 / C3),
     F(0.7071067812 / C2), -F(0.3826834324 / C3),
#undef F

#undef C0
#undef C1
#undef C2
#undef C3
};

static const FIXED_T SBC_ALIGNED analysis_consts_fixed8_simd_even[80 + 64] =
{
#define C0 2.7906148894
#define C1 2.4270044280
#define C2 2.8015616024
#define C3 3.1710363741
#define C4 2.5377944043
#define C5 2.4270044280
#define C6 2.8015616024
#define C7 3.1710363741

#define F(x) F_PROTO8(x)
     F(0.00000000E+00 * C0),  F(2.01182542E-03 * C0),
     F(1.56575398E-04 * C1),  F(1.78371725E-03 * C1),
     F(3.43256425E-04 * C2),  F(1.47640169E-03 * C2),
     F(5.54620202E-04 * C3),  F(1.13992507E-03 * C3),
    -F(8.23919506E-04 * C4),  F(0.00000000E+00 * C4),
     F(2.10371989E-03 * C5),  F(3.49717454E-03 * C5),
     F(1.99454554E-03 * C6),  F(1.64973098E-03 * C6),
     F(1.61656283E-03 * C7),  F(1.78805361E-04 * C7),
     F(5.65949473E-03 * C0),  F(1.29371806E-02 * C0),
     F(8.02941163E-03 * C1),  F(1.53184106E-02 * C1),
     F(1.04584443E-02 * C2),  F(1.62208471E-02 * C2),
     F(1.27472335E-02 * C3),  F(1.59045603E-02 * C3),
    -F(1.46525263E-02 * C4),  F(0.00000000E+00 * C4),
     F(8.85757540E-03 * C5),  F(5.31873032E-02 * C5),
     F(2.92408442E-03 * C6),  F(3.90751381E-02 * C6),
    -F(4.91578024E-03 * C7),  F(2.61098752E-02 * C7),
     F(6.79989431E-02 * C0),  F(1.46955068E-01 * C0),
     F(8.29847578E-02 * C1),  F(1.45389847E-01 * C1),
     F(9.75753918E-02 * C2),  F(1.40753505E-01 * C2),
     F(1.11196689E-01 * C3),  F(1.33264415E-01 * C3),
    -F(1.23264548E-01 * C4),  F(0.00000000E+00 * C4),
     F(1.45389847E-01 * C5), -F(8.29847578E-02 * C5),
     F(1.40753505E-01 * C6), -F(9.75753918E-02 * C6),
     F(1.33264415E-01 * C7), -F(1.11196689E-01 * C7),
    -F(6.79989431E-02 * C0),  F(1.29371806E-02 * C0),
    -F(5.31873032E-02 * C1),  F(8.85757540E-03 * C1),
    -F(3.90751381E-02 * C2),  F(2.92408442E-03 * C2),
    -F(2.61098752E-02 * C3), -F(4.91578024E-03 * C3),
     F(1.46404076E-02 * C4),  F(0.00000000E+00 * C4),
     F(1.53184106E-02 * C5), -F(8.02941163E-03 * C5),
     F(1.62208471E-02 * C6), -F(1.04584443E-02 * C6),
     F(1.59045603E-02 * C7), -F(1.27472335E-02 * C7),
    -F(5.65949473E-03 * C0),  F(2.01182542E-03 * C0),
    -F(3.49717454E-03 * C1),  F(2.10371989E-03 * C1),
    -F(1.64973098E-03 * C2),  F(1.99454554E-03 * C2),
    -F(1.78805361E-04 * C3),  F(1.61656283E-03 * C3),
    -F(9.02154502E-04 * C4),  F(0.00000000E+00 * C4),
     F(1.78371725E-03 * C5), -F(1.56575398E-04 * C5),
     F(1.47640169E-03 * C6), -F(3.43256425E-04 * C6),
     F(1.13992507E-03 * C7), -F(5.54620202E-04 * C7),
#undef F
#define F(x) F_COS8(x)
     F(0.7071067812 / C0),  F(0.8314696123 / C1),
    -F(0.7071067812 / C0), -F(0.1950903220 / C1),
    -F(0.7071067812 / C0), -F(0.9807852804 / C1),
     F(0.7071067812 / C0), -F(0.5555702330 / C1),
     F(0.7071067812 / C0),  F(0.5555702330 / C1),
    -F(0.7071067812 / C0),  F(0.9807852804 / C1),
    -F(0.7071067812 / C0),  F(0.1950903220 / C1),
     F(0.7071067812 / C0), -F(0.8314696123 / C1),
     F(0.9238795325 / C2),  F(0.9807852804 / C3),
     F(0.3826834324 / C2),  F(0.8314696123 / C3),
    -F(0.3826834324 / C2),  F(0.5555702330 / C3),
    -F(0.9238795325 / C2),  F(0.1950903220 / C3),
    -F(0.9238795325 / C2), -F(0.1950903220 / C3),
    -F(0.3826834324 / C2), -F(0.5555702330 / C3),
     F(0.3826834324 / C2), -F(0.8314696123 / C3),
     F(0.9238795325 / C2), -F(0.9807852804 / C3),
    -F(1.0000000000 / C4),  F(0.5555702330 / C5),
    -F(1.0000000000 / C4), -F(0.9807852804 / C5),
    -F(1.0000000000 / C4),  F(0.1950903220 / C5),
    -F(1.0000000000 / C4),  F(0.8314696123 / C5),
    -F(1.0000000000 / C4), -F(0.8314696123 / C5),
    -F(1.0000000000 / C4), -F(0.1950903220 / C5),
    -F(1.0000000000 / C4),  F(0.9807852804 / C5),
    -F(1.0000000000 / C4), -F(0.5555702330 / C5),
     F(0.3826834324 / C6),  F(0.1950903220 / C7),
    -F(0.9238795325 / C6), -F(0.5555702330 / C7),
     F(0.9238795325 / C6),  F(0.8314696123 / C7),
    -F(0.3826834324 / C6), -F(0.9807852804 / C7),
    -F(0.3826834324 / C6),  F(0.9807852804 / C7),
     F(0.9238795325 / C6), -F(0.8314696123 / C7),
    -F(0.9238795325 / C6),  F(0.5555702330 / C7),
     F(0.3826834324 / C6), -F(0.1950903220 / C7),
#undef F

#undef C0
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5
#undef C6
#undef C7
};

static const FIXED_T SBC_ALIGNED analysis_consts_fixed8_simd_odd[80 + 64] =
{
#define C0 2.5377944043
#define C1 2.4270044280
#define C2 2.8015616024
#define C3 3.1710363741
#define C4 2.7906148894
#define C5 2.4270044280
#define C6 2.8015616024
#define C7 3.1710363741

#define F(x) F_PROTO8(x)
     F(0.00000000E+00 * C0), -F(8.23919506E-04 * C0),
     F(1.56575398E-04 * C1),  F(1.78371725E-03 * C1),
     F(3.43256425E-04 * C2),  F(1.47640169E-03 * C2),
     F(5.54620202E-04 * C3),  F(1.13992507E-03 * C3),
     F(2.01182542E-03 * C4),  F(5.65949473E-03 * C4),
     F(2.10371989E-03 * C5),  F(3.49717454E-03 * C5),
     F(1.99454554E-03 * C6),  F(1.64973098E-03 * C6),
     F(1.61656283E-03 * C7),  F(1.78805361E-04 * C7),
     F(0.00000000E+00 * C0), -F(1.46525263E-02 * C0),
     F(8.02941163E-03 * C1),  F(1.53184106E-02 * C1),
     F(1.04584443E-02 * C2),  F(1.62208471E-02 * C2),
     F(1.27472335E-02 * C3),  F(1.59045603E-02 * C3),
     F(1.29371806E-02 * C4),  F(6.79989431E-02 * C4),
     F(8.85757540E-03 * C5),  F(5.31873032E-02 * C5),
     F(2.92408442E-03 * C6),  F(3.90751381E-02 * C6),
    -F(4.91578024E-03 * C7),  F(2.61098752E-02 * C7),
     F(0.00000000E+00 * C0), -F(1.23264548E-01 * C0),
     F(8.29847578E-02 * C1),  F(1.45389847E-01 * C1),
     F(9.75753918E-02 * C2),  F(1.40753505E-01 * C2),
     F(1.11196689E-01 * C3),  F(1.33264415E-01 * C3),
     F(1.46955068E-01 * C4), -F(6.79989431E-02 * C4),
     F(1.45389847E-01 * C5), -F(8.29847578E-02 * C5),
     F(1.40753505E-01 * C6), -F(9.75753918E-02 * C6),
     F(1.33264415E-01 * C7), -F(1.11196689E-01 * C7),
     F(0.00000000E+00 * C0),  F(1.46404076E-02 * C0),
    -F(5.31873032E-02 * C1),  F(8.85757540E-03 * C1),
    -F(3.90751381E-02 * C2),  F(2.92408442E-03 * C2),
    -F(2.61098752E-02 * C3), -F(4.91578024E-03 * C3),
     F(1.29371806E-02 * C4), -F(5.65949473E-03 * C4),
     F(1.53184106E-02 * C5), -F(8.02941163E-03 * C5),
     F(1.62208471E-02 * C6), -F(1.04584443E-02 * C6),
     F(1.59045603E-02 * C7), -F(1.27472335E-02 * C7),
     F(0.00000000E+00 * C0), -F(9.02154502E-04 * C0),
    -F(3.49717454E-03 * C1),  F(2.10371989E-03 * C1),
    -F(1.64973098E-03 * C2),  F(1.99454554E-03 * C2),
    -F(1.78805361E-04 * C3),  F(1.61656283E-03 * C3),
     F(2.01182542E-03 * C4),  F(0.00000000E+00 * C4),
     F(1.78371725E-03 * C5), -F(1.56575398E-04 * C5),
     F(1.47640169E-03 * C6), -F(3.43256425E-04 * C6),
     F(1.13992507E-03 * C7), -F(5.54620202E-04 * C7),
#undef F
#define F(x) F_COS8(x)
    -F(1.0000000000 / C0),  F(0.8314696123 / C1),
    -F(1.0000000000 / C0), -F(0.1950903220 / C1),
    -F(1.0000000000 / C0), -F(0.9807852804 / C1),
    -F(1.0000000000 / C0), -F(0.5555702330 / C1),
    -F(1.0000000000 / C0),  F(0.5555702330 / C1),
    -F(1.0000000000 / C0),  F(0.9807852804 / C1),
    -F(1.0000000000 / C0),  F(0.1950903220 / C1),
    -F(1.0000000000 / C0), -F(0.8314696123 / C1),
     F(0.9238795325 / C2),  F(0.9807852804 / C3),
     F(0.3826834324 / C2),  F(0.8314696123 / C3),
    -F(0.3826834324 / C2),  F(0.5555702330 / C3),
    -F(0.9238795325 / C2),  F(0.1950903220 / C3),
    -F(0.9238795325 / C2), -F(0.1950903220 / C3),
    -F(0.3826834324 / C2), -F(0.5555702330 / C3),
     F(0.3826834324 / C2), -F(0.8314696123 / C3),
     F(0.9238795325 / C2), -F(0.9807852804 / C3),
     F(0.7071067812 / C4),  F(0.5555702330 / C5),
    -F(0.7071067812 / C4), -F(0.9807852804 / C5),
    -F(0.7071067812 / C4),  F(0.1950903220 / C5),
     F(0.7071067812 / C4),  F(0.8314696123 / C5),
     F(0.7071067812 / C4), -F(0.8314696123 / C5),
    -F(0.7071067812 / C4), -F(0.1950903220 / C5),
    -F(0.7071067812 / C4),  F(0.9807852804 / C5),
     F(0.7071067812 / C4), -F(0.5555702330 / C5),
     F(0.3826834324 / C6),  F(0.1950903220 / C7),
    -F(0.9238795325 / C6), -F(0.5555702330 / C7),
     F(0.9238795325 / C6),  F(0.8314696123 / C7),
    -F(0.3826834324 / C6), -F(0.9807852804 / C7),
    -F(0.3826834324 / C6),  F(0.9807852804 / C7),
     F(0.9238795325 / C6), -F(0.8314696123 / C7),
    -F(0.9238795325 / C6),  F(0.5555702330 / C7),
     F(0.3826834324 / C6), -F(0.1950903220 / C7),
#undef F

#undef C0
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5
#undef C6
#undef C7
};

static const int32_t sbc_proto_4_40m0[] =
{
    SS4(0x00000000), SS4(0xffa6982f), SS4(0xfba93848), SS4(0x0456c7b8),
    SS4(0x005967d1), SS4(0xfffb9ac7), SS4(0xff589157), SS4(0xf9c2a8d8),
    SS4(0x027c1434), SS4(0x0019118b), SS4(0xfff3c74c), SS4(0xff137330),
    SS4(0xf81b8d70), SS4(0x00ec1b8b), SS4(0xfff0b71a), SS4(0xffe99b00),
    SS4(0xfef84470), SS4(0xf6fb4370), SS4(0xffcdc351), SS4(0xffe01dc7)
};

static const int32_t sbc_proto_4_40m1[] =
{
    SS4(0xffe090ce), SS4(0xff2c0475), SS4(0xf694f800), SS4(0xff2c0475),
    SS4(0xffe090ce), SS4(0xffe01dc7), SS4(0xffcdc351), SS4(0xf6fb4370),
    SS4(0xfef84470), SS4(0xffe99b00), SS4(0xfff0b71a), SS4(0x00ec1b8b),
    SS4(0xf81b8d70), SS4(0xff137330), SS4(0xfff3c74c), SS4(0x0019118b),
    SS4(0x027c1434), SS4(0xf9c2a8d8), SS4(0xff589157), SS4(0xfffb9ac7)
};

static const int32_t sbc_proto_8_80m0[] =
{
    SS8(0x00000000), SS8(0xfe8d1970), SS8(0xee979f00), SS8(0x11686100),
    SS8(0x0172e690), SS8(0xfff5bd1a), SS8(0xfdf1c8d4), SS8(0xeac182c0),
    SS8(0x0d9daee0), SS8(0x00e530da), SS8(0xffe9811d), SS8(0xfd52986c),
    SS8(0xe7054ca0), SS8(0x0a00d410), SS8(0x006c1de4), SS8(0xffdba705),
    SS8(0xfcbc98e8), SS8(0xe3889d20), SS8(0x06af2308), SS8(0x000bb7db),
    SS8(0xffca00ed), SS8(0xfc3fbb68), SS8(0xe071bc00), SS8(0x03bf7948),
    SS8(0xffc4e05c), SS8(0xffb54b3b), SS8(0xfbedadc0), SS8(0xdde26200),
    SS8(0x0142291c), SS8(0xff960e94), SS8(0xff9f3e17), SS8(0xfbd8f358),
    SS8(0xdbf79400), SS8(0xff405e01), SS8(0xff7d4914), SS8(0xff8b1a31),
    SS8(0xfc1417b8), SS8(0xdac7bb40), SS8(0xfdbb828c), SS8(0xff762170)
};

static const int32_t sbc_proto_8_80m1[] =
{
    SS8(0xff7c272c), SS8(0xfcb02620), SS8(0xda612700), SS8(0xfcb02620),
    SS8(0xff7c272c), SS8(0xff762170), SS8(0xfdbb828c), SS8(0xdac7bb40),
    SS8(0xfc1417b8), SS8(0xff8b1a31), SS8(0xff7d4914), SS8(0xff405e01),
    SS8(0xdbf79400), SS8(0xfbd8f358), SS8(0xff9f3e17), SS8(0xff960e94),
    SS8(0x0142291c), SS8(0xdde26200), SS8(0xfbedadc0), SS8(0xffb54b3b),
    SS8(0xffc4e05c), SS8(0x03bf7948), SS8(0xe071bc00), SS8(0xfc3fbb68),
    SS8(0xffca00ed), SS8(0x000bb7db), SS8(0x06af2308), SS8(0xe3889d20),
    SS8(0xfcbc98e8), SS8(0xffdba705), SS8(0x006c1de4), SS8(0x0a00d410),
    SS8(0xe7054ca0), SS8(0xfd52986c), SS8(0xffe9811d), SS8(0x00e530da),
    SS8(0x0d9daee0), SS8(0xeac182c0), SS8(0xfdf1c8d4), SS8(0xfff5bd1a)
};

static const int32_t synmatrix4[8][4] =
{
    { SN4(0x05a82798), SN4(0xfa57d868), SN4(0xfa57d868), SN4(0x05a82798) },
    { SN4(0x030fbc54), SN4(0xf89be510), SN4(0x07641af0), SN4(0xfcf043ac) },
    { SN4(0x00000000), SN4(0x00000000), SN4(0x00000000), SN4(0x00000000) },
    { SN4(0xfcf043ac), SN4(0x07641af0), SN4(0xf89be510), SN4(0x030fbc54) },
    { SN4(0xfa57d868), SN4(0x05a82798), SN4(0x05a82798), SN4(0xfa57d868) },
    { SN4(0xf89be510), SN4(0xfcf043ac), SN4(0x030fbc54), SN4(0x07641af0) },
    { SN4(0xf8000000), SN4(0xf8000000), SN4(0xf8000000), SN4(0xf8000000) },
    { SN4(0xf89be510), SN4(0xfcf043ac), SN4(0x030fbc54), SN4(0x07641af0) }
};

static const int32_t synmatrix8[16][8] =
{
    { SN8(0x05a82798), SN8(0xfa57d868), SN8(0xfa57d868), SN8(0x05a82798),
      SN8(0x05a82798), SN8(0xfa57d868), SN8(0xfa57d868), SN8(0x05a82798) },
    { SN8(0x0471ced0), SN8(0xf8275a10), SN8(0x018f8b84), SN8(0x06a6d988),
      SN8(0xf9592678), SN8(0xfe70747c), SN8(0x07d8a5f0), SN8(0xfb8e3130) },
    { SN8(0x030fbc54), SN8(0xf89be510), SN8(0x07641af0), SN8(0xfcf043ac),
      SN8(0xfcf043ac), SN8(0x07641af0), SN8(0xf89be510), SN8(0x030fbc54) },
    { SN8(0x018f8b84), SN8(0xfb8e3130), SN8(0x06a6d988), SN8(0xf8275a10),
      SN8(0x07d8a5f0), SN8(0xf9592678), SN8(0x0471ced0), SN8(0xfe70747c) },
    { SN8(0x00000000), SN8(0x00000000), SN8(0x00000000), SN8(0x00000000),
      SN8(0x00000000), SN8(0x00000000), SN8(0x00000000), SN8(0x00000000) },
    { SN8(0xfe70747c), SN8(0x0471ced0), SN8(0xf9592678), SN8(0x07d8a5f0),
      SN8(0xf8275a10), SN8(0x06a6d988), SN8(0xfb8e3130), SN8(0x018f8b84) },
    { SN8(0xfcf043ac), SN8(0x07641af0), SN8(0xf89be510), SN8(0x030fbc54),
      SN8(0x030fbc54), SN8(0xf89be510), SN8(0x07641af0), SN8(0xfcf043ac) },
    { SN8(0xfb8e3130), SN8(0x07d8a5f0), SN8(0xfe70747c), SN8(0xf9592678),
      SN8(0x06a6d988), SN8(0x018f8b84), SN8(0xf8275a10), SN8(0x0471ced0) },
    { SN8(0xfa57d868), SN8(0x05a82798), SN8(0x05a82798), SN8(0xfa57d868),
      SN8(0xfa57d868), SN8(0x05a82798), SN8(0x05a82798), SN8(0xfa57d868) },
    { SN8(0xf9592678), SN8(0x018f8b84), SN8(0x07d8a5f0), SN8(0x0471ced0),
      SN8(0xfb8e3130), SN8(0xf8275a10), SN8(0xfe70747c), SN8(0x06a6d988) },
    { SN8(0xf89be510), SN8(0xfcf043ac), SN8(0x030fbc54), SN8(0x07641af0),
      SN8(0x07641af0), SN8(0x030fbc54), SN8(0xfcf043ac), SN8(0xf89be510) },
    { SN8(0xf8275a10), SN8(0xf9592678), SN8(0xfb8e3130), SN8(0xfe70747c),
      SN8(0x018f8b84), SN8(0x0471ced0), SN8(0x06a6d988), SN8(0x07d8a5f0) },
    { SN8(0xf8000000), SN8(0xf8000000), SN8(0xf8000000), SN8(0xf8000000),
      SN8(0xf8000000), SN8(0xf8000000), SN8(0xf8000000), SN8(0xf8000000) },
    { SN8(0xf8275a10), SN8(0xf9592678), SN8(0xfb8e3130), SN8(0xfe70747c),
      SN8(0x018f8b84), SN8(0x0471ced0), SN8(0x06a6d988), SN8(0x07d8a5f0) },
    { SN8(0xf89be510), SN8(0xfcf043ac), SN8(0x030fbc54), SN8(0x07641af0),
      SN8(0x07641af0), SN8(0x030fbc54), SN8(0xfcf043ac), SN8(0xf89be510) },
    { SN8(0xf9592678), SN8(0x018f8b84), SN8(0x07d8a5f0), SN8(0x0471ced0),
      SN8(0xfb8e3130), SN8(0xf8275a10), SN8(0xfe70747c), SN8(0x06a6d988) }
};

struct sbc_encoder_state
{
    int position;
    /* Number of consecutive blocks handled by the encoder */
    uint8_t increment;
    int16_t SBC_ALIGNED X[2][SBC_X_BUFFER_SIZE];
    /* Polyphase analysis filter for 4 subbands configuration,
     * it handles "increment" blocks at once */
    void (*sbc_analyze_4s)(struct sbc_encoder_state *state,
            int16_t *x, int32_t *out, int out_stride);
    /* Polyphase analysis filter for 8 subbands configuration,
     * it handles "increment" blocks at once */
    void (*sbc_analyze_8s)(struct sbc_encoder_state *state,
            int16_t *x, int32_t *out, int out_stride);
    /* Process input data (deinterleave, endian conversion, reordering),
     * depending on the number of subbands and input data byte order */
    int (*sbc_enc_process_input_4s_le)(int position,
            const uint8_t *pcm, int16_t X[2][SBC_X_BUFFER_SIZE],
            int nsamples, int nchannels);
    int (*sbc_enc_process_input_4s_be)(int position,
            const uint8_t *pcm, int16_t X[2][SBC_X_BUFFER_SIZE],
            int nsamples, int nchannels);
    int (*sbc_enc_process_input_8s_le)(int position,
            const uint8_t *pcm, int16_t X[2][SBC_X_BUFFER_SIZE],
            int nsamples, int nchannels);
    int (*sbc_enc_process_input_8s_be)(int position,
            const uint8_t *pcm, int16_t X[2][SBC_X_BUFFER_SIZE],
            int nsamples, int nchannels);
    /* Scale factors calculation */
    void (*sbc_calc_scalefactors)(int32_t sb_sample_f[16][2][8],
            uint32_t scale_factor[2][8],
            int blocks, int channels, int subbands);
    /* Scale factors calculation with joint stereo support */
    int (*sbc_calc_scalefactors_j)(int32_t sb_sample_f[16][2][8],
            uint32_t scale_factor[2][8],
            int blocks, int subbands);
    const char *implementation_info;
};

/* A2DP specification: Appendix B, page 69 */
static const int sbc_offset4[4][4] =
{
    { -1, 0, 0, 0 },
    { -2, 0, 0, 1 },
    { -2, 0, 0, 1 },
    { -2, 0, 0, 1 }
};

/* A2DP specification: Appendix B, page 69 */
static const int sbc_offset8[4][8] =
{
    { -2, 0, 0, 0, 0, 0, 0, 1 },
    { -3, 0, 0, 0, 0, 0, 1, 2 },
    { -4, 0, 0, 0, 0, 0, 1, 2 },
    { -4, 0, 0, 0, 0, 0, 1, 2 }
};

struct sbc_struct
{
    unsigned long flags;

    uint8_t frequency;
    uint8_t blocks;
    uint8_t subbands;
    uint8_t mode;
    uint8_t allocation;
    uint8_t bitpool;
    uint8_t endian;

    void *priv;
    void *priv_alloc_base;
};

typedef struct sbc_struct sbc_t;

int sbc_init(sbc_t *sbc, unsigned long flags);
int sbc_reinit(sbc_t *sbc, unsigned long flags);
int sbc_init_msbc(sbc_t *sbc, unsigned long flags);
int sbc_init_a2dp(sbc_t *sbc, unsigned long flags,
                    const void *conf, size_t conf_len);
int sbc_reinit_a2dp(sbc_t *sbc, unsigned long flags,
                    const void *conf, size_t conf_len);

ssize_t sbc_parse(sbc_t *sbc, const void *input, size_t input_len);

/* Decodes ONE input block into ONE output block */
ssize_t sbc_decode(sbc_t *sbc, const void *input, size_t input_len,
            void *output, size_t output_len, size_t *written);

/* Encodes ONE input block into ONE output block */
ssize_t sbc_encode(sbc_t *sbc, const void *input, size_t input_len,
            void *output, size_t output_len, ssize_t *written);

/* Returns the output block size in bytes */
size_t sbc_get_frame_length(sbc_t *sbc);

/* Returns the time one input/output block takes to play in msec*/
unsigned sbc_get_frame_duration(sbc_t *sbc);

/* Returns the input block size in bytes */
size_t sbc_get_codesize(sbc_t *sbc);

const char *sbc_get_implementation_info(sbc_t *sbc);
void sbc_finish(sbc_t *sbc);
void pcm_to_sbc(char *filename, char *output, int msbc);
void sbc_to_pcm(char *filename, char *output, int msbc);
void sbc_init_primitives(struct sbc_encoder_state *state);
void sbc_decode_init(sbc_t *sbc, int msbc);
ssize_t sbc_decoder_decode(sbc_t *sbc, const void *input, size_t input_len,
            void *output, size_t output_len, size_t *written);
void sbc_decoder_uninit(sbc_t *sbc);
void sbc_encode_init(sbc_t *sbc, int msbc);
ssize_t sbc_encoder_encode(sbc_t *sbc, const void *input, size_t input_len,
            void *output, size_t output_len, ssize_t *written);
void sbc_encoder_uninit(sbc_t *sbc);

#ifdef __cplusplus
}
#endif

#endif /* __SBC_H */
