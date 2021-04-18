/* { dg-do compile } */

#include <riscv_vector.h>
#include <stddef.h>
#include <stdint.h>

/*
https://github.com/riscv/riscv-v-spec/blob/master/v-spec.adoc

=== Example using vector mask instructions

The following is an example of vectorizing a data-dependent exit loop.

----
  # char* strcpy(char *dst, const char* src)
strcpy:
      mv a2, a0             # Copy dst
loop:
    vsetvli x0, x0, e8   # Max length vectors of bytes
    vlbuff.v v1, (a1)       # Get src bytes
      csrr t1, vl           # Get number of bytes fetched
    vmseq.vi v0, v1, 0      # Flag zero bytes
    vfirst.m a3, v0         # Zero found?
      add a1, a1, t1        # Bump pointer
    vmsif.m v0, v0          # Set mask up to and including zero byte.
    vsb.v v1, (a2), v0.t    # Write out bytes
      add a2, a2, t1        # Bump pointer
      bltz a3, loop         # Zero byte not found, so loop

      ret
*/

char *strcpy(char *dst, const char* src) {
  char *old_dst = dst;
  int zero_find = -1;
  while (zero_find < 0) {
    word_type old_vl = vsetvlmax_e8m1();
    word_type vl = 0;
    vuint8m1_t value;
    value = vle8ff_v_u8m1((uint8_t *)src, &vl, old_vl);
    vbool8_t cmp;
    cmp = vmseq_vx_u8m1_b8(value, 0, vl);
    zero_find = vfirst_m_b8(cmp, vl); // if no zero than return -1
    vbool8_t mask;
    mask = vmsif_m_b8(cmp, vl); // set mask up to and including zero byte
    vse8_v_u8m1_m(mask, (uint8_t *)dst, value, vl);
    src+=vl;
    dst+=vl;
  }
  return old_dst;
}
