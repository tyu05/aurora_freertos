/*
*   Copyright 2018 NXP
*/


#ifndef __CPUDEFS_H__
#define __CPUDEFS_H__

#ifdef __cplusplus
extern "C"
{
#endif

    #define LED0                        0U
    #define LED1                        1U
    #define LED2                        2U

    #define HWRGM_FES_DEFAULT           0x0000
    #define CGM_SC_DC0_3_DEFAULT        0x80010000

    #define CPU0_ENABLE                 0x01000000 /* CPU0 is enabled                   */
    #define MPC574xx_ID                 0x005A0000 /* RCHW boot ID for MPC574xx devices */

    #define RCHWDATA_Init()  \
    RCHWDATA const unsigned int RCHW2=(const unsigned int)__start; /*/ entry point /*/\
    RCHWDATA const unsigned int RCHW1=RCHW_VAL;

    #define HWINTC_INTERRUPTS    (755U)

    /* INTC has some unimplemented inputs (the registers PSR, PRC_SEL are not
       implemented for these inputs). Following array contains bit set where
       each bit corresponds to an INTC input with number of the bit.
       If the value of bit is 1 - INTC input is implemented, 0 - INTC input is
       not implemented */
    #define AVAL_INTC_TABLE { \
        0xff, 0xff, 0x00, 0x00, 0xf7, 0xff, 0xf0, 0xff, /*   0.. 63 */ \
        0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, /*  64..127 */ \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, /* 128..191 */ \
        0x00, 0x00, 0x00, 0x1f, 0xfc, 0x03, 0x78, 0xf8, /* 192..255 */ \
        0x00, 0xf0, 0xe1, 0x03, 0x00, 0x00, 0x00, 0x00, /* 256..319 */ \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, /* 320..383 */ \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, /* 384..447 */ \
        0xe0, 0x7f, 0x00, 0x60, 0x00, 0x03, 0x77, 0x00, /* 448..511 */ \
        0x00, 0xff, 0xff, 0xff, 0xfc, 0x7f, 0x00, 0x00, /* 512..575 */ \
        0x00, 0x00, 0x3c, 0xfc, 0xfb, 0xef, 0x1f, 0xff, /* 576..639 */ \
        0x09, 0x80, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf0, /* 640..703 */ \
        0xff, 0x07, 0x00, 0xc0, 0x0f, 0xff, 0x03        /* 704..754 */ \
    }

    /* Definition is used in FreeRTOSConfig.h */
    #define configCPU_CLOCK_HZ                         ((unsigned portLONG ) 4000000 )
    /* RaceRuner Ultra specific: pit channel to use 0-3 */
    #define configUSE_PIT_CHANNEL                     3
    #define MAX_PIT_CHANNEL                           3

#ifdef __cplusplus
}
#endif

#endif/*__CPUDEFS_H__*/