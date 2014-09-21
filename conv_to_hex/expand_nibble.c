

/*
   Operations:
    - 6 shifts
    - 4 bit-and
    - 3 bit-or
*/
uint32_t nibble_expand_naive(uint32_t x) {
    assert(x <= 0xffff);

    const uint32_t n0 = x & 0x0f;
    const uint32_t n1 = (x >>  4) & 0x0f;
    const uint32_t n2 = (x >>  8) & 0x0f;
    const uint32_t n3 = (x >> 12) & 0x0f;

    return n3 | (n2 << 8) | (n1 << 16) | (n0 << 24);
}


/*
    Operations:
    - 2 multiplications
    - 4 bit-and
    - 1 bit-or
    - 1 bswap
*/
uint32_t nibble_expand_mul(uint32_t x) {
    assert(x <= 0xffff);

    // 1. isolate nibble 0 and 2
    const uint32_t n02 = x & 0x0f0f;

    // 2. isolate nibble 1 and 3
    const uint32_t n13 = x & 0xf0f0;

    // 3. move nibble 2 to 3rd byte             // n02  = 0x00000a0b
    const uint32_t tmp1 = n02  * 0x00000101;    // tmp1 = 0x000a???b
    const uint32_t tmp2 = tmp1 & 0x00ff00ff;    // tmp2 = 0x000a000b

    // 4. move nibble 1 to 1st byte and 3 to the 4th byte
    const uint32_t tmp3 = n13  * 0x00001010;    // tmp3 = 0x0c???f00
    const uint32_t tmp4 = tmp3 & 0xff00ff00;    // tmp4 = 0x0c000f00

    return bswap(tmp2 | tmp4);
}



/*
    Operations:
    - 1 interleave (kind of shift)
    - 1 shift
    - 2 bit-and
*/

#define SIMD_ALIGN __attribute__((aligned(16)))
#define packed_byte(x)   {x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x}

uint8_t LSB_mask[16] SIMD_ALIGN = packed_byte(0x0f);

uint32_t nibble_expand_simd(uint32_t x) {
    assert(x <= 0xffff);

    uint32_t result;

    __asm__ __volatile__ (
        "movd    %0,        %%xmm0    \n"
        "movdqa  %%xmm0,    %%xmm1    \n"

        // 1. isolate even nibbles
        "movdqa  LSB_mask,  %%xmm2    \n"
        "pand    %%xmm2,    %%xmm0    \n"

        // 2. isolate odd nibbles
        "psrlw   $4,        %%xmm1    \n"
        "pand    %%xmm2,    %%xmm1    \n"

        // 3. intereleave nibble
        "punpcklbw %%xmm1, %%xmm0     \n"

        // store
        "movd    %%xmm0, %0           \n"
        "bswap   %0                   \n"
        : "=r" (result)
        :  "0" (x)
    );

    return result;
}


// --- PDEP version -------------------------------------------------------

uint32_t nibble_expand_pdep(uint32_t x) {
    assert(x <= 0xffff);

    return bswap(pdep(0x0f0f0f0f, x));
}

