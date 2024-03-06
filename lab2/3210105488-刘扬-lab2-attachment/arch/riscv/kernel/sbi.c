#include "types.h"
#include "sbi.h"


struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
			            uint64 arg1, uint64 arg2,
			            uint64 arg3, uint64 arg4,
			            uint64 arg5) 
{
	struct sbiret ret;
    uint64 a6 = (uint64)(fid);
    uint64 a7 = (uint64)(ext);
	__asm__ volatile (
        "mv a7, %[ext]\n"
        "mv a6, %[fid]\n"
		"mv a0, %[arg0]\n"
		"mv a1, %[arg1]\n"
		"mv a2, %[arg2]\n"
		"mv a3, %[arg3]\n"
		"mv a4, %[arg4]\n"
		"mv a5, %[arg5]\n"
		"ecall\n"
		"mv %[error], a0\n"
		"mv %[ret_val], a1"
        : [error] "=r" (ret.error), [ret_val] "=r" (ret.value)
        : [ext] "r" (a7), [fid] "r" (a6), 
		  [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2), [arg3] "r" (arg3), [arg4] "r" (arg4), [arg5] "r" (arg5) 
        : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
    );
	return ret;
}

void sbi_set_timer(uint64 time) {
    sbi_ecall(0x00, 0, time, 0, 0, 0, 0, 0);
}