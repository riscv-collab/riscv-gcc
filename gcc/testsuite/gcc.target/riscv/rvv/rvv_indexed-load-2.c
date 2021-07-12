/* { dg-do compile } */

#include <riscv_vector.h>
#include <stddef.h>
#include "rvv-common.h"

/* Takes the scalar type STYPE, vector class VCLASS (int or float), and
   the e and m value.  */
#define VSLOADSTORE(STYPE, VCLASST, VCLASS, EM, MLEN, SEW, IEM, ISEW)                   \
  void vsloadstore##ISEW##_##VCLASS##EM(size_t n, long stride, STYPE *x,                \
                                        STYPE *y, STYPE z, uint##ISEW##_t *index, word_type vl) {     \
    v##VCLASST##EM##_t vx, vy, vz;                                                      \
    vuint##IEM##_t vindex;                                                              \
    vbool##MLEN##_t mask;                                                               \
    mask = MSET (MLEN);                                                             \
    vindex = VULOAD(ISEW, IEM, index);                                                       \
    vy = VLOAD(VCLASS, SEW, EM, y);                                                         \
    vx = vloxe##i##ISEW##_v_##VCLASS##EM##_m(mask, vy, x, vindex, vl);                       \
    vz = vx + vy;                                                                       \
    VSTORE(VCLASS, SEW, EM, x, vz);                                                         \
  }

RVV_INT_INDEX_TEST(VSLOADSTORE)
RVV_UINT_INDEX_TEST(VSLOADSTORE)
RVV_FLOAT_INDEX_TEST(VSLOADSTORE)

/* { dg-final { scan-assembler-times "vloxei8.v" 26 } } */
/* { dg-final { scan-assembler-times "vloxei16.v" 45 } } */
/* { dg-final { scan-assembler-times "vloxei32.v" 49 } } */
/* { dg-final { scan-assembler-times "vloxei64.v" 44 } } */
