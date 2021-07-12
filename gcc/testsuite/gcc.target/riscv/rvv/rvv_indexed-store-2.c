/* { dg-do compile } */

#include <riscv_vector.h>
#include <stddef.h>
#include "rvv-common.h"

/* Takes the scalar type STYPE, vector class VCLASS (int or float), and
   the e and m value.  */
#define VSLOADSTORE(STYPE, VCLASST, VCLASS, EM, MLEN, SEW, IEM, ISEW)                              \
  void vsloadstore##ISEW##VCLASS##EM(size_t n, long stride, STYPE *x,                \
                               STYPE *y, STYPE z, uint##ISEW##_t *index, word_type vl) {           \
    v##VCLASST##EM##_t vx, vy, vz;                                            \
    vuint##IEM##_t vindex;                                              \
    vbool##MLEN##_t mask;                                                    \
    mask = MSET (MLEN);                                             \
    vindex = VULOAD(ISEW, IEM, index);                                      \
    vx = VLOAD(VCLASS, SEW, EM, x);                                               \
    vy = VLOAD(VCLASS, SEW, EM, y);                                               \
    vz = vx + vy;                                                              \
    vsoxe##i##ISEW##_v_##VCLASS##EM##_m(mask, x, vindex, vz, vl);                                       \
  }									\
  void vusloadstore##ISEW##VCLASS##EM(size_t n, long stride, STYPE *x,                \
                               STYPE *y, STYPE z, uint##ISEW##_t *index, word_type vl) {           \
    v##VCLASST##EM##_t vx, vy, vz;                                            \
    vuint##IEM##_t vindex;                                              \
    vbool##MLEN##_t mask;                                                    \
    mask = MSET (MLEN);                                             \
    vindex = VULOAD(ISEW, IEM, index);                                      \
    vx = VLOAD(VCLASS, SEW, EM, x);                                               \
    vy = VLOAD(VCLASS, SEW, EM, y);                                               \
    vz = vx + vy;                                                              \
    vsuxe##i##ISEW##_v_##VCLASS##EM##_m(mask, x, vindex, vz, vl);                                       \
  }

RVV_INT_INDEX_TEST(VSLOADSTORE)
RVV_UINT_INDEX_TEST(VSLOADSTORE)
RVV_FLOAT_INDEX_TEST(VSLOADSTORE)

/* { dg-final { scan-assembler-times "vsoxei8.v" 26 } } */
/* { dg-final { scan-assembler-times "vsoxei16.v" 45 } } */
/* { dg-final { scan-assembler-times "vsoxei32.v" 49 } } */
/* { dg-final { scan-assembler-times "vsoxei64.v" 44 } } */

/* { dg-final { scan-assembler-times "vsuxei8.v" 26 } } */
/* { dg-final { scan-assembler-times "vsuxei16.v" 45 } } */
/* { dg-final { scan-assembler-times "vsuxei32.v" 49 } } */
/* { dg-final { scan-assembler-times "vsuxei64.v" 44 } } */
