#ifndef __DISP_DRV_H__
#define __DISP_DRV_H__

#include <mach/mt_typedefs.h>

#include "disp_drv_log.h"
#include "ddp_path.h"
#include "lcm_drv.h"
#include "disp_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------

#define DISP_CHECK_RET(expr)                                                \
    do {                                                                    \
        DISP_STATUS ret = (expr);                                           \
        if (DISP_STATUS_OK != ret) {                                        \
            DISP_LOG_PRINT(ANDROID_LOG_ERROR, "COMMON", "[ERROR][mtkfb] DISP API return error code: 0x%x\n"      \
                   "  file : %s, line : %d\n"                               \
                   "  expr : %s\n", ret, __FILE__, __LINE__, #expr);        \
        }                                                                   \
    } while (0)


// ---------------------------------------------------------------------------

#define ASSERT_LAYER    (DDP_OVL_LAYER_MUN-1)
extern unsigned int FB_LAYER;    // default LCD layer
#define DISP_DEFAULT_UI_LAYER_ID (DDP_OVL_LAYER_MUN-1)
#define DISP_CHANGED_UI_LAYER_ID (DDP_OVL_LAYER_MUN-2)

typedef struct{
    unsigned int id;
    unsigned int curr_en;
    unsigned int next_en;
    unsigned int hw_en;
    int curr_idx;
    int next_idx;
    int hw_idx;
    int curr_identity;
    int next_identity;
    int hw_identity;
    int curr_conn_type;
    int next_conn_type;
    int hw_conn_type;
}DISP_LAYER_INFO;

#if 0
typedef enum
{
	HS_PRPR = 0,
	HS_ZERO = 1,
	HS_TRAIL= 2,
	TA_GO= 3,
	TA_SURE= 4,
	TA_GET= 5,
	DA_HS_EXIT= 6,
	CLK_ZERO= 7,
	CLK_TRAIL= 8,
	CONT_DET= 9,
	CLK_HS_PRPR= 10,
	CLK_HS_POST= 11,
	CLK_HS_EXIT= 12,
	MAX= 0XFF,	
} MIPI_SETTING_TYPE;

typedef struct MIPI_TIMING{     
    MIPI_SETTING_TYPE type;
	unsigned char value;
}MIPI_TIMING;
#endif

// ---------------------------------------------------------------------------
// Public Functions
// ---------------------------------------------------------------------------

DISP_STATUS DISP_Init(UINT32 fbVA, UINT32 fbPA, BOOL isLcmInited);
DISP_STATUS DISP_Deinit(void);
DISP_STATUS DISP_PowerEnable(BOOL enable);
DISP_STATUS DISP_LCMEnable(BOOL enable);
DISP_STATUS DISP_PanelEnable(BOOL enable);
DISP_STATUS DISP_SetFrameBufferAddr(UINT32 fbPhysAddr);
DISP_STATUS DISP_EnterOverlayMode(void);
DISP_STATUS DISP_LeaveOverlayMode(void);
DISP_STATUS DISP_EnableDirectLinkMode(UINT32 layer);
DISP_STATUS DISP_DisableDirectLinkMode(UINT32 layer);
DISP_STATUS DISP_UpdateScreen(UINT32 x, UINT32 y, UINT32 width, UINT32 height);
DISP_STATUS DISP_WaitForLCDNotBusy(void);
DISP_STATUS DISP_PrepareSuspend(void);
DISP_STATUS DISP_GetLayerInfo(DISP_LAYER_INFO *pLayer);

//Register extra trigger source 
typedef int (*DISP_EXTRA_CHECKUPDATE_PTR)(int);
typedef int (*DISP_EXTRA_CONFIG_PTR)(int);
int DISP_RegisterExTriggerSource(DISP_EXTRA_CHECKUPDATE_PTR pCheckUpdateFunc , DISP_EXTRA_CONFIG_PTR pConfFunc);
void DISP_UnRegisterExTriggerSource(int u4ID);
void GetUpdateMutex(void);
void ReleaseUpdateMutex(void);

DISP_STATUS DISP_ConfigDither(int lrs, int lgs, int lbs, int dbr, int dbg, int dbb);


// Retrieve Information
BOOL   DISP_IsVideoMode(void);
UINT32 DISP_GetScreenWidth(void);
UINT32 DISP_GetScreenHeight(void);
UINT32 DISP_GetActiveWidth(void);
UINT32 DISP_GetActiveHeight(void);
UINT32 DISP_GetScreenBpp(void);
UINT32 DISP_GetPages(void);
DISP_STATUS DISP_SetScreenBpp(UINT32);   ///config how many bits for each pixel of framebuffer
DISP_STATUS DISP_SetPages(UINT32);         ///config how many framebuffer will be used
///above information is used to determine the vRAM size

BOOL   DISP_IsDirectLinkMode(void);
BOOL   DISP_IsInOverlayMode(void);
UINT32 DISP_GetFBRamSize(void);         ///get FB buffer size
UINT32 DISP_GetVRamSize(void);          /// get total RAM size (FB+working buffer+DAL buffer)
PANEL_COLOR_FORMAT DISP_GetPanelColorFormat(void);
UINT32 DISP_GetPanelBPP(void);
BOOL DISP_IsLcmFound(void);
BOOL DISP_IsImmediateUpdate(void);
DISP_STATUS DISP_ConfigImmediateUpdate(BOOL enable);

DISP_STATUS DISP_SetBacklight(UINT32 level);
DISP_STATUS DISP_SetPWM(UINT32 divider);
DISP_STATUS DISP_GetPWM(UINT32 divider, unsigned int *freq);
DISP_STATUS DISP_SetBacklight_mode(UINT32 mode);

DISP_STATUS DISP_Set3DPWM(BOOL enable, BOOL landscape);

// FM De-sense
DISP_STATUS DISP_FMDesense_Query(void);
DISP_STATUS DISP_FM_Desense(unsigned long);
DISP_STATUS DISP_Reset_Update(void);
DISP_STATUS DISP_Get_Default_UpdateSpeed(unsigned int *speed);
DISP_STATUS DISP_Get_Current_UpdateSpeed(unsigned int *speed);
DISP_STATUS DISP_Change_Update(unsigned int);
///////////////

// ---------------------------------------------------------------------------
// Private Functions
// ---------------------------------------------------------------------------

BOOL DISP_SelectDevice(const char* lcm_name);
BOOL DISP_DetectDevice(void);
BOOL DISP_SelectDeviceBoot(const char* lcm_name);
UINT32 DISP_GetVRamSizeBoot(char *cmdline);
DISP_STATUS DISP_Capture_Framebuffer(unsigned int pvbuf, unsigned int bpp, unsigned int is_early_suspended);
BOOL DISP_IsContextInited(void);

DISP_STATUS DISP_Capture_Videobuffer(unsigned int pvbuf, unsigned int bpp, unsigned int video_rotation);
UINT32 DISP_GetOutputBPPforDithering(void);
const char* DISP_GetLCMId(void);

BOOL DISP_EsdRecoverCapbility(void);
BOOL DISP_EsdCheck(void);
BOOL DISP_EsdRecover(void);
void DISP_WaitVSYNC(void);
void DISP_InitVSYNC(unsigned int vsync_interval);
DISP_STATUS DISP_PauseVsync(BOOL enable);
DISP_STATUS DISP_Config_Overlay_to_Memory(unsigned int mva, int enable);
void DISP_StartConfigUpdate(void);

unsigned long DISP_GetLCMIndex(void);
unsigned int DISP_AutoTest(void);
unsigned int DISP_BLS_Query(void);
void DISP_BLS_Enable(BOOL enable);
// ---------------------------------------------------------------------------
void DISP_Change_LCM_Resolution(unsigned int width, unsigned int height);
BOOL fbconfig_dsi_vdo_prepare(void);
void fbconfig_disp_set_mipi_clk(unsigned int clk);
void fbconfig_disp_set_mipi_lane_num(unsigned int lane_num);



#ifdef __cplusplus
}
#endif

#endif // __DISP_DRV_H__
