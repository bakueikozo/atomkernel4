/****************************************************************
*****************************************************************/

#ifndef __JZM_JPEG_DEC_H__
#define __JZM_JPEG_DEC_H__
#include "helix.h"

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/printk.h>
#include <asm-generic/div64.h>
#define printf printk
#endif


#define JPGC_EN        (0x1)        /* JPGC enable signal */

#define YUV420P0C      (0x30)       /* component 0 configure information NBLK<<4| QT<<2| HA<<1| HD */
#define YUV420P1C      (0x03)       /* component 1 configure information NBLK<<4| QT<<2| HA<<1| HD */
#define YUV420P2C      (0x03)       /* component 2 configure information NBLK<<4| QT<<2| HA<<1| HD */
#define YUV420P3C      (0xf0)       /* component 3 configure information NBLK<<4| QT<<2| HA<<1| HD */

#define YUV420PVH      (0x0a<<16)   /* component vertical/horizontal size of MCU:P3H P3V P2H P2V P1H P1V P0H P0V */
#define JPGC_RSM       (0x1<<2)     /* JPGC rstart marker enable signal */
#define JPGC_SPEC      (0x0<<1)     /* YUV420 mode */
#define JPGC_UNIV      (0x1<<1)     /* YUV444 or YUV422 mode */
#define JPGC_DEC       (0x1<<3)     /* JPGC decode signal: 1 (decode); 0(encode) */
#define OPEN_CLOCK     (0x1)        /* open the core clock */
#define JPGC_NCOL      (0x2<<4)     /* color numbers of a MCU minus 1,it always 2 for YUV color space */
#define STAT_CLEAN     (0x0)        /* clean the STAT register */
#define CORE_RST       (0x1<<6)     /* JPGC core reset ,high active */
#define JPGC_EFE       (0x1<<8)     /* JPGC EFE source */
#define VRAM_RAWY_BA   (VPU_BASE | 0xF0000)
#define VRAM_RAWC_BA   (VRAM_RAWY_BA + 256)

#if 0
/* JPEG Decode quantization table select level */
typedef enum {
  LOW_QUALITY,
  MEDIUMS_QUALITY,
  HIGH_QUALITY
} QUANT_QUALITY;
#endif

#define HUFFMIN_LEN	16
#define HUFFBASE_LEN	64
#define HUFFSYMB_LEN	336
#define QMEM_LEN	256

/*
  _JPEGD_SliceInfo:
  JPEG Decoder Slice Level Information
 */
typedef struct{
    uint32_t *des_va, des_pa;
    uint32_t bsa;                       /* bitstream buffer address  */
    uint32_t p0a, p1a;                  /* componet 0-3 plane buffer address */
    uint8_t nrsm;                       /* Re-Sync-Marker gap number */
    uint32_t nmcu;                      /* number of MCU minus one */
    uint32_t width;                     /* yuv nv12 frame width, bit15: 1,NV12, 0,TILE .(1 << 15) | (mb_width - 1)*/
    int *huffmin  ;
    int *huffbase ;
    int *huffsymb ;
    int *qmem  ;
    uint32_t pxc[4];			/* component 0~3 config info */

}_JPEGD_SliceInfo;



void JPEGD_SliceInit(_JPEGD_SliceInfo *s);
#endif// __JZM_JPEG_ENC_H__
