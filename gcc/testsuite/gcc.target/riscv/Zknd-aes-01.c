/* { dg-do compile } */
/* { dg-options "-march=rv32gc_zknd -mabi=ilp32 -O2" } */

int foo1(int rs1, int rs2)
{
    return __builtin_riscv_aes32dsi(rs1, rs2, 1);
}

int foo2(int rs1, int rs2)
{
    return __builtin_riscv_aes32dsmi(rs1, rs2, 0);
}

/* { dg-final { scan-assembler-times "aes32dsi" 1 } } */
/* { dg-final { scan-assembler-times "aes32dsmi" 1 } } */
