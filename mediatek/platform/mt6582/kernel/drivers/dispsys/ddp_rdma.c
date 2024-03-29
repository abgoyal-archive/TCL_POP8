#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/aee.h>

#include <linux/xlog.h>

#include <asm/io.h>
#include <mach/mt_typedefs.h>
#include "ddp_matrix_para.h"

#include "ddp_reg.h"
#include "ddp_rdma.h"
#include "ddp_hal.h"

#ifndef ASSERT
//#define ASSERT(expr) do {printk("ASSERT error func=%s, line=%d\n", __FUNC__, __LINE__);} while (!(expr))
#define ASSERT(expr) do {printk("ASSERT error \n");} while (!(expr))
#endif

#define DISP_INDEX_OFFSET 0  // 82 has only one rdma1, base addr do not have to add offset according to module index



int RDMAStart(unsigned idx) {
    ASSERT(idx <= 2);

    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_INT_ENABLE, 0x3F);
    DISP_REG_SET_FIELD(GLOBAL_CON_FLD_ENGINE_EN, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON, 1);

    return 0;
}

int RDMAStop(unsigned idx) {
    ASSERT(idx <= 2);

    DISP_REG_SET_FIELD(GLOBAL_CON_FLD_ENGINE_EN, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON, 0);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_INT_ENABLE, 0);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_INT_STATUS, 0);
    return 0;
}

int RDMAReset(unsigned idx) {
    unsigned int delay_cnt=0;
    //static unsigned int cnt=0;

    // printk("[DDP] RDMAReset called %d \n", cnt++);

    ASSERT(idx <= 2);

    DISP_REG_SET_FIELD(GLOBAL_CON_FLD_SOFT_RESET, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON, 1);
    while((DISP_REG_GET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON)&0x700)==0x100)
    {
        delay_cnt++;
        if(delay_cnt>10000)
        {
            printk("[DDP] error, RDMAReset(%d) timeout, stage 1! DISP_REG_RDMA_GLOBAL_CON=0x%x \n", idx, DISP_REG_GET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON));
            break;
        }
    }
    DISP_REG_SET_FIELD(GLOBAL_CON_FLD_SOFT_RESET, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON, 0);
    // printk("[DDP] start reset! \n");
    while((DISP_REG_GET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON)&0x700)!=0x100)
    {
        delay_cnt++;
        if(delay_cnt>10000)
        {
            printk("[DDP] error, RDMAReset(%d) timeout, stage 2! DISP_REG_RDMA_GLOBAL_CON=0x%x \n", idx, DISP_REG_GET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON));
            break;
        }
    }
    // printk("[DDP] end reset! \n");

    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON     , 0x00);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0     , 0x00);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_1     , 0x00);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_MEM_CON         , 0x00);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_MEM_START_ADDR , 0x00);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_MEM_SRC_PITCH     , 0x00);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_MEM_GMC_SETTING_1 , 0x20);     ///TODO: need check
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_FIFO_CON , 0x80f00008);        ///TODO: need check

    return 0;
}

int RDMAConfig(unsigned idx,
                    enum RDMA_MODE mode,
                    DpColorFormat inFormat,
                    unsigned address,
                    enum RDMA_OUTPUT_FORMAT outputFormat,
                    unsigned pitch,
                    unsigned width,
                    unsigned height,
                    bool isByteSwap, // input setting
                    bool isRGBSwap)  // ourput setting
{
    unsigned bpp;

    unsigned int input_is_yuv = 0;
    unsigned int output_is_yuv = 0;

    enum RDMA_INPUT_FORMAT inputFormat = rdma_fmt_convert(inFormat);
    ASSERT(idx <= 2);
    if((width > RDMA_MAX_WIDTH) || (height > RDMA_MAX_HEIGHT))
    {
    	  printk("DDP error, RDMA input overflow, w=%d, h=%d, max_w=%d, max_h=%d\n", width, height, RDMA_MAX_WIDTH, RDMA_MAX_HEIGHT);
    }

    switch(inputFormat) {
    case RDMA_INPUT_FORMAT_YUYV:
    case RDMA_INPUT_FORMAT_UYVY:
    case RDMA_INPUT_FORMAT_YVYU:
    case RDMA_INPUT_FORMAT_VYUY:
    case RDMA_INPUT_FORMAT_RGB565:
        bpp = 2;
        break;
    case RDMA_INPUT_FORMAT_RGB888:
        bpp = 3;
        break;
    case RDMA_INPUT_FORMAT_ARGB:
        bpp = 4;
        break;
    default:
        printk("DDP error, unknown RDMA input format = %d\n", inputFormat);
        ASSERT(0);
    }

    // printk("RDMA: w=%d, h=%d, addr=%x, pitch=%d, mode=%d \n", width, height, address, width*bpp, mode);

    switch(inputFormat) {
    case RDMA_INPUT_FORMAT_YUYV:
    case RDMA_INPUT_FORMAT_UYVY:
    case RDMA_INPUT_FORMAT_YVYU:
    case RDMA_INPUT_FORMAT_VYUY:
        input_is_yuv = 1;
        break;

    case RDMA_INPUT_FORMAT_RGB565:
    case RDMA_INPUT_FORMAT_RGB888:
    case RDMA_INPUT_FORMAT_ARGB:
        input_is_yuv = 0;
        break;

    default:
    	  printk("DDP error, unknow input format is %d\n", inputFormat);
        ASSERT(0);
    }

    if(outputFormat==RDMA_OUTPUT_FORMAT_ARGB)
    {
        output_is_yuv = 0;
    }
    else
    {
        output_is_yuv = 1;
    }

    if(input_is_yuv==1 && output_is_yuv==0)
    {
	    // set color conversion matrix
	    DISP_REG_SET(DISP_REG_RDMA_CF_00, coef_rdma_601_y2r[0][0] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_01, coef_rdma_601_y2r[0][1] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_02, coef_rdma_601_y2r[0][2] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_10, coef_rdma_601_y2r[1][0] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_11, coef_rdma_601_y2r[1][1] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_12, coef_rdma_601_y2r[1][2] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_20, coef_rdma_601_y2r[2][0] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_21, coef_rdma_601_y2r[2][1] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_22, coef_rdma_601_y2r[2][2] );

	    DISP_REG_SET(DISP_REG_RDMA_CF_PRE_ADD0, coef_rdma_601_y2r[3][0]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_PRE_ADD1, coef_rdma_601_y2r[3][1]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_PRE_ADD2, coef_rdma_601_y2r[3][2]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_POST_ADD0, coef_rdma_601_y2r[4][0]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_POST_ADD1, coef_rdma_601_y2r[4][1]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_POST_ADD2, coef_rdma_601_y2r[4][2]);
    }
    else if(input_is_yuv==0 && output_is_yuv==1)
    {
	    // set color conversion matrix
	    DISP_REG_SET(DISP_REG_RDMA_CF_00, coef_rdma_601_r2y[0][0] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_01, coef_rdma_601_r2y[0][1] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_02, coef_rdma_601_r2y[0][2] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_10, coef_rdma_601_r2y[1][0] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_11, coef_rdma_601_r2y[1][1] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_12, coef_rdma_601_r2y[1][2] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_20, coef_rdma_601_r2y[2][0] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_21, coef_rdma_601_r2y[2][1] );
	    DISP_REG_SET(DISP_REG_RDMA_CF_22, coef_rdma_601_r2y[2][2] );

	    DISP_REG_SET(DISP_REG_RDMA_CF_PRE_ADD0, coef_rdma_601_r2y[3][0]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_PRE_ADD1, coef_rdma_601_r2y[3][1]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_PRE_ADD2, coef_rdma_601_r2y[3][2]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_POST_ADD0, coef_rdma_601_r2y[4][0]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_POST_ADD1, coef_rdma_601_r2y[4][1]);
	    DISP_REG_SET(DISP_REG_RDMA_CF_POST_ADD2, coef_rdma_601_r2y[4][2]);
    }

    DISP_REG_SET_FIELD(GLOBAL_CON_FLD_MODE_SEL, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON, mode);
    DISP_REG_SET_FIELD(MEM_CON_FLD_MEM_MODE_INPUT_FORMAT, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_MEM_CON, inputFormat);

    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_MEM_START_ADDR, address);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_MEM_SRC_PITCH, pitch);
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_INT_ENABLE, 0x1F);

    DISP_REG_SET_FIELD(SIZE_CON_0_FLD_INPUT_BYTE_SWAP, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, isByteSwap);
    DISP_REG_SET_FIELD(SIZE_CON_0_FLD_OUTPUT_FORMAT, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, outputFormat);
    DISP_REG_SET_FIELD(SIZE_CON_0_FLD_OUTPUT_FRAME_WIDTH, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, width);
    DISP_REG_SET_FIELD(SIZE_CON_0_FLD_OUTPUT_RGB_SWAP, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, isRGBSwap);
    DISP_REG_SET_FIELD(SIZE_CON_1_FLD_OUTPUT_FRAME_HEIGHT, idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_1, height);

    return 0;
}

void RDMAWait(unsigned idx)
{
    // polling interrupt status
    while((DISP_REG_GET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_INT_STATUS) & 0x1) != 0x1) ;
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_INT_STATUS , 0x0);
}

void RDMASetTargetLine(unsigned int idx, unsigned int line)
{
    DISP_REG_SET(idx * DISP_INDEX_OFFSET + DISP_REG_RDMA_TARGET_LINE, line);
}

enum RDMA_INPUT_FORMAT rdma_fmt_convert(DpColorFormat fmt)
{
   enum RDMA_INPUT_FORMAT rdma_fmt = RDMA_INPUT_FORMAT_RGB565;
   switch(fmt)
   {
       case eRGB565           :
         rdma_fmt = RDMA_INPUT_FORMAT_RGB565   ;  break;
       case eRGB888           :
         rdma_fmt = RDMA_INPUT_FORMAT_RGB888   ;  break;
       case eARGB8888         :
         rdma_fmt = RDMA_INPUT_FORMAT_ARGB     ;  break;
       case eYUY2             :
           rdma_fmt = RDMA_INPUT_FORMAT_YUYV     ;  break;
       case eUYVY             :
           rdma_fmt = RDMA_INPUT_FORMAT_UYVY     ;  break;
       case eYVYU             :
           rdma_fmt = RDMA_INPUT_FORMAT_YVYU     ;  break;
       case eVYUY             :
           rdma_fmt = RDMA_INPUT_FORMAT_VYUY     ;  break;
       default:
           printk("error: rdma_fmt_convert fmt=%d, rdma_fmt=%d \n", fmt, rdma_fmt);
   }

   return rdma_fmt;
}

enum RDMA_OUTPUT_FORMAT rdma_output_fmt_convert(DpColorFormat fmt)
{
   enum RDMA_OUTPUT_FORMAT rdma_fmt = RDMA_OUTPUT_FORMAT_ARGB;
   switch(fmt)
   {
       case eRGB565           :
         rdma_fmt = RDMA_OUTPUT_FORMAT_ARGB   ;  break;
       //case eYUV_444_1P           :
       //  rdma_fmt = RDMA_OUTPUT_FORMAT_YUV444   ;  break;
       default:
           printk("error: rdma_fmt_convert fmt=%d, rdma_fmt=%d \n", fmt, rdma_fmt);
   }

   return rdma_fmt;
}

