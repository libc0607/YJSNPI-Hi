/******************************************************************************
  File Name     : yjsnpi-venc.c
  Version       : Initial Draft
  Author        : libc0607
  Created       : 2020
  Description   : 
  Usage: ./yjsnpi-venc conf.ini /var/tmp/mmcblock0
******************************************************************************/

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <utime.h>
#include <resolv.h>
#include <endian.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include "iniparser.h"
#include "sample_comm.h"

#define VB_MAX_NUM            10


typedef struct hiSAMPLE_VPSS_ATTR_S
{
    SIZE_S            stMaxSize;
    DYNAMIC_RANGE_E   enDynamicRange;
    PIXEL_FORMAT_E    enPixelFormat;
    COMPRESS_MODE_E   enCompressMode[VPSS_MAX_PHY_CHN_NUM];
    SIZE_S            stOutPutSize[VPSS_MAX_PHY_CHN_NUM];
    FRAME_RATE_CTRL_S stFrameRate[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL           bMirror[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL           bFlip[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL           bChnEnable[VPSS_MAX_PHY_CHN_NUM];

    SAMPLE_SNS_TYPE_E enSnsType;
    HI_U32            BigStreamId;
    HI_U32            SmallStreamId;
    VI_VPSS_MODE_E    ViVpssMode;
    HI_BOOL           bWrapEn;
    HI_U32            WrapBufLine;
} SAMPLE_VPSS_CHN_ATTR_S;

typedef struct hiSAMPLE_VB_ATTR_S
{
    HI_U32            validNum;
    HI_U64            blkSize[VB_MAX_NUM];
    HI_U32            blkCnt[VB_MAX_NUM];
    HI_U32            supplementConfig;
} SAMPLE_VB_ATTR_S;

/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_VENC_Usage(char* sPrgNm)
{
    printf("Usage : %s \n", sPrgNm);
	printf("./yjsnpi-venc conf.ini /var/tmp/mmcblock0\n");
    return;
}

/******************************************************************************
* function : get config from ini dict.
* just a quick & dirty hack, don't be serious
******************************************************************************/
HI_S32 SAMPLE_YJSNPI_GetConfigFromIni(YJSNPI_VENC_CONFIG_S *pconf, dictionary * ini)
{
	char buf[128];
	char * str;
	HI_S32 tmp;
	struct timeval stamp;
	
	// clear
	memset(pconf, 0, sizeof(YJSNPI_VENC_CONFIG_S));
	
	// global
	// 2轴dis 曾经想开 开了发现效果贼差 
	pconf->dis = HI_FALSE;
	
	// ch0 
	// ch0 enc
	tmp = iniparser_getint(ini, "venc:ch0_enc", 0);
	if (tmp == 265) {
		pconf->enc[0] = PT_H265;
	}
	else if (tmp == 264) {
		pconf->enc[0] = PT_H264;
	} 
	else {
		printf("YJSNPI VENC config: Warning: ch0_enc not set. Use default h265\n");
		pconf->enc[0] = PT_H265;
	} 

	// ch0 res
	// 不用改了 反正低了也不增加帧率……就默认让它最大
	pconf->res[0] = PIC_2304x1296;
	
	// ch0 rc
	// note that SAMPLE_RC_E[0]=CBR so default is CBR 
	pconf->rc[0] = iniparser_getint(ini, "venc:ch0_rc", 0);
	
	// ch0 kbps
	pconf->kbps[0] = iniparser_getint(ini, "venc:ch0_kbps", 0);
	if (pconf->kbps[0] > 20480 || pconf->kbps[0] < 512) {		// a little limit
		printf("YJSNPI VENC config: Warning: ch0_kbps out of range (512 ~ 20480). Use default 2048kbps \n");
		pconf->kbps[0] = 2048;
	}
	// ch0 gop
	tmp = iniparser_getint(ini, "venc:ch0_gop", 0);
	if (tmp == 0) {
		pconf->gop[0] = VENC_GOPMODE_NORMALP;
	}
	else if (tmp == 1) {
		pconf->gop[0] = VENC_GOPMODE_DUALP;
	} 
	else if (tmp == 2) {
		pconf->gop[0] = VENC_GOPMODE_SMARTP;
	}
	else {
		printf("YJSNPI VENC config: Warning: ch0_gop not set. Use default NORMAL\n");
		pconf->gop[0] = VENC_GOPMODE_NORMALP;
	}
	// ch0 savedir (by timestamp)
	// example: 
	// ch0_savedir=/var/tmp/mmcblock0/save
	// dest. video: /var/tmp/mmcblock0/save/0123456789.h26x
	memset(buf, 0, sizeof(buf));
	gettimeofday(&stamp, NULL);
	str = (char *)iniparser_getstring(ini, "venc:ch0_savedir", NULL);
	if (strlen(str) > 100) {
		printf("YJSNPI VENC config: Error: ch0_savedir tooooooo long.\n");
		return HI_FAILURE;
	}
	strncpy(buf, str, strlen(str));
	buf[strlen(str)] = '/';	// add seperate
	snprintf(buf+strlen(str)+1, 11, "%010ld", stamp.tv_sec);
	if (pconf->enc[0] == PT_H264) {
		strncpy(buf+strlen(str)+11, ".h264", 5);
	} else if (pconf->enc[0] == PT_H265) {
		strncpy(buf+strlen(str)+11, ".h265", 5);
	}
	strncpy(pconf->save, buf, strlen(buf));
	
	
	// ch1 
	// ch1 enc
	tmp = iniparser_getint(ini, "venc:ch1_enc", 0);
	if (tmp == 265) {
		pconf->enc[1] = PT_H265;
	}
	else if (tmp == 264) {
		pconf->enc[1] = PT_H264;
	} 
	else {
		printf("YJSNPI VENC config: Warning: ch1_enc not set. Use default h264\n");
		pconf->enc[1] = PT_H264;
	} 
	// ch1 res
	tmp = iniparser_getint(ini, "venc:ch1_res", 0);
	if (tmp == 360) {
		pconf->res[1] = PIC_360P;
	}
	else if (tmp == 240) {
		pconf->res[1] = PIC_CIF;
	} 
	else if (tmp == 480) {
		pconf->res[1] = PIC_D1_NTSC;
	} 
	else {
		printf("YJSNPI VENC config: Warning: ch1_res not set. Use default CIF\n");
		pconf->res[1] = PIC_CIF;
	} 

	// ch1 rc
	// note that SAMPLE_RC_E[0]=CBR so default is CBR 
	pconf->rc[1] = iniparser_getint(ini, "venc:ch1_rc", 0);
	// ch1 kbps
	pconf->kbps[1] = iniparser_getint(ini, "venc:ch1_kbps", 0);
	if (pconf->kbps[1] > 4096 || pconf->kbps[1] < 128) {		// a little limit
		printf("YJSNPI VENC config: Warning: ch1_kbps out of range (128 ~ 4096). Use default 512kbps \n");
		pconf->kbps[1] = 512;
	}
	// ch1 gop
	// default VENC_GOPMODE_NORMALP=0
	tmp = iniparser_getint(ini, "venc:ch1_gop", 0);
	if (tmp == 0) {
		pconf->gop[1] = VENC_GOPMODE_NORMALP;
	}
	else if (tmp == 1) {
		pconf->gop[1] = VENC_GOPMODE_DUALP;
	} 
	else if (tmp == 2) {
		pconf->gop[1] = VENC_GOPMODE_SMARTP;
	}
	else {
		printf("YJSNPI VENC config: Warning: ch1_gop not set. Use default NORMAL\n");
		pconf->gop[1] = VENC_GOPMODE_NORMALP;
	}
	
	// ch1 udp listen port
	pconf->lport = atoi(iniparser_getstring(ini, "venc:ch1_udp_bind_port", NULL));
	// ch1 udp dest. port
	pconf->dport = atoi(iniparser_getstring(ini, "venc:ch1_udp_send_port", NULL));
	// ch1 udp dest. ip
	str = (char *)iniparser_getstring(ini, "venc:ch1_udp_send_ip", NULL);
	strncpy(pconf->daddr, str, sizeof(pconf->daddr));
	 
	// encode profile 
	// default: h265=main, h264=main
	// 0-base(264)/main(265), 1-main(264)/main10(265), 2-high(264),3-svc-t(264)
	pconf->profile[0] = (pconf->enc[0] == PT_H265)? 0: 1;
	pconf->profile[1] = (pconf->enc[1] == PT_H265)? 0: 1;

	// print all info
	fprintf(stderr, "YJSNPI VENC settings: \n");
	fprintf(stderr, "DIS: %d \n", pconf->dis);
	fprintf(stderr, "CH0: enc %d, res %d, rc %d, kbps %d, gop %d, savedir %s \n",
				pconf->enc[0], pconf->res[0], pconf->rc[0], pconf->kbps[0], pconf->gop[0], pconf->save);
	fprintf(stderr, "CH1: enc %d, res %d, rc %d, kbps %d, gop %d, lport %d, dport %d, udpip %s \n",
				pconf->enc[1], pconf->res[1], pconf->rc[1], pconf->kbps[1], pconf->gop[1], 
				pconf->lport, pconf->dport, pconf->daddr);

	return HI_SUCCESS;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_VENC_HandleSig(HI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_VENC_StopSendQpmapFrame();
        SAMPLE_COMM_VENC_StopGetStream();
        SAMPLE_COMM_All_ISP_Stop();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

static HI_U32 GetFrameRateFromSensorType(SAMPLE_SNS_TYPE_E enSnsType)
{
    HI_U32 FrameRate;

    SAMPLE_COMM_VI_GetFrameRateBySensor(enSnsType, &FrameRate);

    return FrameRate;
}

static HI_U32 GetFullLinesStdFromSensorType(SAMPLE_SNS_TYPE_E enSnsType)
{
    HI_U32 FullLinesStd = 0;

    switch (enSnsType) {
        case SONY_IMX327_MIPI_2M_30FPS_12BIT:
        case SONY_IMX327_MIPI_2M_30FPS_12BIT_WDR2TO1:
            FullLinesStd = 1125;
            break;
        case SONY_IMX307_MIPI_2M_30FPS_12BIT:
        case SONY_IMX307_MIPI_2M_30FPS_12BIT_WDR2TO1:
        case SONY_IMX307_2L_MIPI_2M_30FPS_12BIT:
        case SONY_IMX307_2L_MIPI_2M_30FPS_12BIT_WDR2TO1:
            FullLinesStd = 1125;
            break;
        case SONY_IMX335_MIPI_5M_30FPS_12BIT:
        case SONY_IMX335_MIPI_5M_30FPS_10BIT_WDR2TO1:
            FullLinesStd = 1875;
            break;
        case SONY_IMX335_MIPI_4M_30FPS_12BIT:
        case SONY_IMX335_MIPI_4M_30FPS_10BIT_WDR2TO1:
	    case SONY_IMX335_MIPI_3M_30FPS_12BIT:	// wait to be tested: what does 'FullLinesStd' means?
            FullLinesStd = 1375;
            break;
        default:
            SAMPLE_PRT("Error: Not support this sensor now! ==> %d\n",enSnsType);
            break;
    }

    return FullLinesStd;
}

static HI_VOID GetSensorResolution(SAMPLE_SNS_TYPE_E enSnsType, SIZE_S *pSnsSize)
{
    HI_S32          ret;
    SIZE_S          SnsSize;
    PIC_SIZE_E      enSnsSize;

    ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return;
    }
    ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &SnsSize);
    if (HI_SUCCESS != ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return;
    }

    *pSnsSize = SnsSize;

    return;
}

static HI_VOID SAMPLE_VENC_GetDefaultVpssAttr(SAMPLE_SNS_TYPE_E enSnsType, HI_BOOL *pChanEnable, SIZE_S stEncSize[], SAMPLE_VPSS_CHN_ATTR_S *pVpssAttr)
{
    HI_S32 i;

    memset(pVpssAttr, 0, sizeof(SAMPLE_VPSS_CHN_ATTR_S));

    pVpssAttr->enDynamicRange = DYNAMIC_RANGE_SDR8;
    pVpssAttr->enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pVpssAttr->bWrapEn        = 0;
    pVpssAttr->enSnsType      = enSnsType;
    pVpssAttr->ViVpssMode     = VI_ONLINE_VPSS_ONLINE;
    //pVpssAttr->ViVpssMode     = VI_ONLINE_VPSS_OFFLINE;
    
    for (i = 0; i < VPSS_MAX_PHY_CHN_NUM; i++)
    {
        if (HI_TRUE == pChanEnable[i])
        {
            //pVpssAttr->enCompressMode[i]          = (i == 0)? COMPRESS_MODE_SEG : COMPRESS_MODE_NONE;
            pVpssAttr->enCompressMode[i]          = COMPRESS_MODE_NONE;
            pVpssAttr->stOutPutSize[i].u32Width   = stEncSize[i].u32Width;
            pVpssAttr->stOutPutSize[i].u32Height  = stEncSize[i].u32Height;
            pVpssAttr->stFrameRate[i].s32SrcFrameRate  = -1;
            pVpssAttr->stFrameRate[i].s32DstFrameRate  = -1;
            pVpssAttr->bMirror[i]                      = HI_FALSE;
            pVpssAttr->bFlip[i]                        = HI_FALSE;

            pVpssAttr->bChnEnable[i]                   = HI_TRUE;
        }
    }

    return;
}

HI_S32 SAMPLE_VENC_SYS_Init(SAMPLE_VB_ATTR_S *pCommVbAttr)
{
    HI_S32 i;
    HI_S32 s32Ret;
    VB_CONFIG_S stVbConf;

    if (pCommVbAttr->validNum > VB_MAX_COMM_POOLS)
    {
        SAMPLE_PRT("SAMPLE_VENC_SYS_Init validNum(%d) too large than VB_MAX_COMM_POOLS(%d)!\n", pCommVbAttr->validNum, VB_MAX_COMM_POOLS);
        return HI_FAILURE;
    }

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));

    for (i = 0; i < pCommVbAttr->validNum; i++)
    {
        stVbConf.astCommPool[i].u64BlkSize   = pCommVbAttr->blkSize[i];
        stVbConf.astCommPool[i].u32BlkCnt    = pCommVbAttr->blkCnt[i];
        //printf("%s,%d,stVbConf.astCommPool[%d].u64BlkSize = %lld, blkSize = %d\n",__func__,__LINE__,i,stVbConf.astCommPool[i].u64BlkSize,stVbConf.astCommPool[i].u32BlkCnt);
    }

    stVbConf.u32MaxPoolCnt = pCommVbAttr->validNum;

    if(pCommVbAttr->supplementConfig == 0)
    {
        s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    }
    else
    {
        s32Ret = SAMPLE_COMM_SYS_InitWithVbSupplement(&stVbConf,pCommVbAttr->supplementConfig);
    }

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_VENC_VI_Init( SAMPLE_VI_CONFIG_S *pstViConfig, VI_VPSS_MODE_E ViVpssMode)
{
    HI_S32              s32Ret;
    SAMPLE_SNS_TYPE_E   enSnsType;
    ISP_CTRL_PARAM_S    stIspCtrlParam;
    HI_U32              u32FrameRate;


    enSnsType = pstViConfig->astViInfo[0].stSnsInfo.enSnsType;

    pstViConfig->as32WorkingViId[0]                           = 0;

    pstViConfig->astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(pstViConfig->astViInfo[0].stSnsInfo.enSnsType, 0);
    pstViConfig->astViInfo[0].stSnsInfo.s32BusId           = 0;
    pstViConfig->astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;
    pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode    = ViVpssMode;

    //pstViConfig->astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe0;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[1]          = -1;

    //pstViConfig->astViInfo[0].stChnInfo.ViChn              = ViChn;
    //pstViConfig->astViInfo[0].stChnInfo.enPixFormat        = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    //pstViConfig->astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    pstViConfig->astViInfo[0].stChnInfo.enVideoFormat      = VIDEO_FORMAT_LINEAR;
    pstViConfig->astViInfo[0].stChnInfo.enCompressMode     = COMPRESS_MODE_NONE;
    s32Ret = SAMPLE_COMM_VI_SetParam(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam failed with %d!\n", s32Ret);
        return s32Ret;
    }

    SAMPLE_COMM_VI_GetFrameRateBySensor(enSnsType, &u32FrameRate);

    s32Ret = HI_MPI_ISP_GetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_GetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }
    stIspCtrlParam.u32StatIntvl  = u32FrameRate/30;
    if (stIspCtrlParam.u32StatIntvl == 0)
    {
        stIspCtrlParam.u32StatIntvl = 1;
    }

    s32Ret = HI_MPI_ISP_SetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_SetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_StartVi(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed with %d!\n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_VENC_VPSS_CreateGrp(VPSS_GRP VpssGrp, SAMPLE_VPSS_CHN_ATTR_S *pParam)
{
    HI_S32          s32Ret;
    PIC_SIZE_E      enSnsSize;
    SIZE_S          stSnsSize;
    VPSS_GRP_ATTR_S stVpssGrpAttr = {0};

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(pParam->enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    stVpssGrpAttr.enDynamicRange          = pParam->enDynamicRange;
    stVpssGrpAttr.enPixelFormat           = pParam->enPixelFormat;
    stVpssGrpAttr.u32MaxW                 = stSnsSize.u32Width;
    stVpssGrpAttr.u32MaxH                 = stSnsSize.u32Height;
    stVpssGrpAttr.bNrEn                   = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enNrType       = VPSS_NR_TYPE_VIDEO;
    stVpssGrpAttr.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;
    stVpssGrpAttr.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;

    s32Ret = HI_MPI_VPSS_CreateGrp(VpssGrp, &stVpssGrpAttr);

    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_CreateGrp(grp:%d) failed with %#x!\n", VpssGrp, s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

static HI_S32 SAMPLE_VENC_VPSS_DestoryGrp(VPSS_GRP VpssGrp)
{
    HI_S32          s32Ret;

    s32Ret = HI_MPI_VPSS_DestroyGrp(VpssGrp);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

static HI_S32 SAMPLE_VENC_VPSS_StartGrp(VPSS_GRP VpssGrp)
{
    HI_S32          s32Ret;

    s32Ret = HI_MPI_VPSS_StartGrp(VpssGrp);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_CreateGrp(grp:%d) failed with %#x!\n", VpssGrp, s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

static HI_S32 SAMPLE_VENC_VPSS_ChnEnable(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, SAMPLE_VPSS_CHN_ATTR_S *pParam, HI_BOOL bWrapEn)
{
    HI_S32 s32Ret;
    VPSS_CHN_ATTR_S     stVpssChnAttr;
    VPSS_CHN_BUF_WRAP_S stVpssChnBufWrap;
		
    memset(&stVpssChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));
    stVpssChnAttr.u32Width                     = pParam->stOutPutSize[VpssChn].u32Width;
    stVpssChnAttr.u32Height                    = pParam->stOutPutSize[VpssChn].u32Height;
    stVpssChnAttr.enChnMode                    = VPSS_CHN_MODE_USER;
    stVpssChnAttr.enCompressMode               = pParam->enCompressMode[VpssChn];
    stVpssChnAttr.enDynamicRange               = pParam->enDynamicRange;
    stVpssChnAttr.enPixelFormat                = pParam->enPixelFormat;
    stVpssChnAttr.stFrameRate.s32SrcFrameRate  = pParam->stFrameRate[VpssChn].s32SrcFrameRate;
    stVpssChnAttr.stFrameRate.s32DstFrameRate  = pParam->stFrameRate[VpssChn].s32DstFrameRate;
    stVpssChnAttr.u32Depth                     = 0;
    stVpssChnAttr.bMirror                      = pParam->bMirror[VpssChn];
    stVpssChnAttr.bFlip                        = pParam->bFlip[VpssChn];
    stVpssChnAttr.enVideoFormat                = VIDEO_FORMAT_LINEAR;
    stVpssChnAttr.stAspectRatio.enMode         = ASPECT_RATIO_NONE;

    s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_SetChnAttr chan %d failed with %#x\n", VpssChn, s32Ret);
        goto exit0;
    }
	
    if (bWrapEn)
    {
        if (VpssChn != 0)   //vpss limit! just vpss chan0 support wrap
        {
            SAMPLE_PRT("Error:Just vpss chan 0 support wrap! Current chan %d\n", VpssChn);
            goto exit0;
        }

        HI_U32 WrapBufLen = 0;
        VPSS_VENC_WRAP_PARAM_S WrapParam;

        memset(&WrapParam, 0, sizeof(VPSS_VENC_WRAP_PARAM_S));
        WrapParam.bAllOnline      = (pParam->ViVpssMode == VI_ONLINE_VPSS_ONLINE) ? 1 : 0;
        WrapParam.u32FrameRate    = GetFrameRateFromSensorType(pParam->enSnsType);
        WrapParam.u32FullLinesStd = GetFullLinesStdFromSensorType(pParam->enSnsType);
        WrapParam.stLargeStreamSize.u32Width = pParam->stOutPutSize[pParam->BigStreamId].u32Width;
        WrapParam.stLargeStreamSize.u32Height= pParam->stOutPutSize[pParam->BigStreamId].u32Height;
        WrapParam.stSmallStreamSize.u32Width = pParam->stOutPutSize[pParam->SmallStreamId].u32Width;
        WrapParam.stSmallStreamSize.u32Height= pParam->stOutPutSize[pParam->SmallStreamId].u32Height;

        if (HI_MPI_SYS_GetVPSSVENCWrapBufferLine(&WrapParam, &WrapBufLen) == HI_SUCCESS)
        {
            stVpssChnBufWrap.u32WrapBufferSize = VPSS_GetWrapBufferSize(WrapParam.stLargeStreamSize.u32Width,
                WrapParam.stLargeStreamSize.u32Height, WrapBufLen, pParam->enPixelFormat, DATA_BITWIDTH_8,
                COMPRESS_MODE_NONE, DEFAULT_ALIGN);
            stVpssChnBufWrap.bEnable = 1;
            stVpssChnBufWrap.u32BufLine = WrapBufLen;
            s32Ret = HI_MPI_VPSS_SetChnBufWrapAttr(VpssGrp, VpssChn, &stVpssChnBufWrap);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VPSS_SetChnBufWrapAttr Chn %d failed with %#x\n", VpssChn, s32Ret);
                goto exit0;
            }
        }
        else
        {
            SAMPLE_PRT("Current sensor type: %d, not support BigStream(%dx%d) and SmallStream(%dx%d) Ring!!\n",
                pParam->enSnsType,
                pParam->stOutPutSize[pParam->BigStreamId].u32Width, pParam->stOutPutSize[pParam->BigStreamId].u32Height,
                pParam->stOutPutSize[pParam->SmallStreamId].u32Width, pParam->stOutPutSize[pParam->SmallStreamId].u32Height);
        }
    }

    s32Ret = HI_MPI_VPSS_EnableChn(VpssGrp, VpssChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_EnableChn (%d) failed with %#x\n", VpssChn, s32Ret);
        goto exit0;
    }
exit0:
    return s32Ret;
}

static HI_S32 SAMPLE_VENC_VPSS_ChnDisable(VPSS_GRP VpssGrp, VPSS_CHN VpssChn)
{
    HI_S32 s32Ret;

    s32Ret = HI_MPI_VPSS_DisableChn(VpssGrp, VpssChn);

    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

static HI_S32 SAMPLE_VENC_VPSS_Init(VPSS_GRP VpssGrp, SAMPLE_VPSS_CHN_ATTR_S *pstParam, HI_BOOL bDisEn)
{
    HI_S32 i,j;
    HI_S32 s32Ret;
    HI_BOOL bWrapEn;

    s32Ret = SAMPLE_VENC_VPSS_CreateGrp(VpssGrp, pstParam);
    if (s32Ret != HI_SUCCESS)
    {
        goto exit0;
    }
	
/*	VPSS_CROP_INFO_S stCropInfo;
	stCropInfo.enCropCoordinate = VPSS_CROP_ABS_COOR;
	stCropInfo.stCropRect.s32X = (2304-1920)/2;
	stCropInfo.stCropRect.s32Y = (1296-1080)/2;		// emmmmm
	stCropInfo.bEnable = HI_TRUE;
	// if dis is enabled, cut 1296P -> 1080P here using VPSS & ISP DIS
	if (HI_TRUE == bDisEn) {
		stCropInfo.stCropRect.u32Width = 1920;
		stCropInfo.stCropRect.u32Height = 1080;
	} 
	else if (HI_FALSE == bDisEn) {
		printf("dis_d\n");
		stCropInfo.stCropRect.u32Width = 2304;
		stCropInfo.stCropRect.u32Height = 1296;
	}
	
	s32Ret = HI_MPI_VPSS_SetGrpCrop(VpssGrp, &stCropInfo);
	if(s32Ret != HI_SUCCESS) {	
		SAMPLE_PRT("HI_MPI_VPSS_SetGrpCrop failed with %#x\n", s32Ret);
	}
*/	
    for (i = 0; i < VPSS_MAX_PHY_CHN_NUM; i++)
    {
        if (pstParam->bChnEnable[i] == HI_TRUE)
        {
            bWrapEn = (i==0)? pstParam->bWrapEn : 0;

            s32Ret = SAMPLE_VENC_VPSS_ChnEnable(VpssGrp, i, pstParam, bWrapEn);
            if (s32Ret != HI_SUCCESS)
            {
                goto exit1;
            }
        }
    }

    i--; // for abnormal case 'exit1' prossess;


    s32Ret = SAMPLE_VENC_VPSS_StartGrp(VpssGrp);
    if (s32Ret != HI_SUCCESS)
    {
        goto exit1;
    }

    return s32Ret;

exit1:
    for (j = 0; j <= i; j++)
    {
        if (pstParam->bChnEnable[j] == HI_TRUE)
        {
            SAMPLE_VENC_VPSS_ChnDisable(VpssGrp, i);
        }
    }

    SAMPLE_VENC_VPSS_DestoryGrp(VpssGrp);
exit0:
    return s32Ret;
}

static HI_VOID SAMPLE_VENC_GetCommVbAttr(const SAMPLE_SNS_TYPE_E enSnsType, const SAMPLE_VPSS_CHN_ATTR_S *pstParam,
    HI_BOOL bSupportDcf, SAMPLE_VB_ATTR_S * pstcommVbAttr)
{
    if (pstParam->ViVpssMode != VI_ONLINE_VPSS_ONLINE)
    {
        SIZE_S snsSize = {0};
        GetSensorResolution(enSnsType, &snsSize);

        if (pstParam->ViVpssMode == VI_OFFLINE_VPSS_ONLINE || pstParam->ViVpssMode == VI_OFFLINE_VPSS_OFFLINE)
        {
            pstcommVbAttr->blkSize[pstcommVbAttr->validNum] = VI_GetRawBufferSize(snsSize.u32Width, snsSize.u32Height,
                                                                                  PIXEL_FORMAT_RGB_BAYER_12BPP,
                                                                                  COMPRESS_MODE_NONE,
                                                                                  DEFAULT_ALIGN);
            pstcommVbAttr->blkCnt[pstcommVbAttr->validNum]  = 3;
            pstcommVbAttr->validNum++;
        }

        if (pstParam->ViVpssMode == VI_OFFLINE_VPSS_OFFLINE)
        {
            pstcommVbAttr->blkSize[pstcommVbAttr->validNum] = COMMON_GetPicBufferSize(snsSize.u32Width, snsSize.u32Height,
                                                                                      PIXEL_FORMAT_YVU_SEMIPLANAR_420,
                                                                                      DATA_BITWIDTH_8,
                                                                                      COMPRESS_MODE_NONE,
                                                                                      DEFAULT_ALIGN);
            pstcommVbAttr->blkCnt[pstcommVbAttr->validNum]  = 2;
            pstcommVbAttr->validNum++;
        }

        if (pstParam->ViVpssMode == VI_ONLINE_VPSS_OFFLINE)
        {
            pstcommVbAttr->blkSize[pstcommVbAttr->validNum] = COMMON_GetPicBufferSize(snsSize.u32Width, snsSize.u32Height,
                                                                                      PIXEL_FORMAT_YVU_SEMIPLANAR_420,
                                                                                      DATA_BITWIDTH_8,
                                                                                      COMPRESS_MODE_NONE,
                                                                                      DEFAULT_ALIGN);
            pstcommVbAttr->blkCnt[pstcommVbAttr->validNum]  = 3;
            pstcommVbAttr->validNum++;

        }
    }
    if(HI_TRUE == pstParam->bWrapEn)
    {
        pstcommVbAttr->blkSize[pstcommVbAttr->validNum] = VPSS_GetWrapBufferSize(pstParam->stOutPutSize[pstParam->BigStreamId].u32Width,
                                                                                 pstParam->stOutPutSize[pstParam->BigStreamId].u32Height,
                                                                                 pstParam->WrapBufLine,
                                                                                 pstParam->enPixelFormat,DATA_BITWIDTH_8,COMPRESS_MODE_NONE,DEFAULT_ALIGN);
        pstcommVbAttr->blkCnt[pstcommVbAttr->validNum]  = 1;
        pstcommVbAttr->validNum++;
    }
    else
    {
        pstcommVbAttr->blkSize[pstcommVbAttr->validNum] = COMMON_GetPicBufferSize(pstParam->stOutPutSize[0].u32Width, pstParam->stOutPutSize[0].u32Height,
                                                                                  pstParam->enPixelFormat,
                                                                                  DATA_BITWIDTH_8,
                                                                                  pstParam->enCompressMode[0],
                                                                                  DEFAULT_ALIGN);

        if (pstParam->ViVpssMode == VI_ONLINE_VPSS_ONLINE)
        {
            pstcommVbAttr->blkCnt[pstcommVbAttr->validNum]  = 3;
        }
        else
        {
            pstcommVbAttr->blkCnt[pstcommVbAttr->validNum]  = 2;
        }

        pstcommVbAttr->validNum++;
    }



    pstcommVbAttr->blkSize[pstcommVbAttr->validNum] = COMMON_GetPicBufferSize(pstParam->stOutPutSize[1].u32Width, pstParam->stOutPutSize[1].u32Height,
                                                                              pstParam->enPixelFormat,
                                                                              DATA_BITWIDTH_8,
                                                                              pstParam->enCompressMode[1],
                                                                              DEFAULT_ALIGN);

    if (pstParam->ViVpssMode == VI_ONLINE_VPSS_ONLINE)
    {
        pstcommVbAttr->blkCnt[pstcommVbAttr->validNum]  = 3;
    }
    else
    {
        pstcommVbAttr->blkCnt[pstcommVbAttr->validNum]  = 2;
    }
    pstcommVbAttr->validNum++;


    //vgs dcf use
    if(HI_TRUE == bSupportDcf)
    {
        pstcommVbAttr->blkSize[pstcommVbAttr->validNum] = COMMON_GetPicBufferSize(160, 120,
                                                                                  pstParam->enPixelFormat,
                                                                                  DATA_BITWIDTH_8,
                                                                                  COMPRESS_MODE_NONE,
                                                                                  DEFAULT_ALIGN);
        pstcommVbAttr->blkCnt[pstcommVbAttr->validNum]  = 1;
        pstcommVbAttr->validNum++;
    }

}

HI_S32 SAMPLE_VENC_H265_H264(YJSNPI_VENC_CONFIG_S *pconf)
{
    HI_S32 i;
    HI_S32 s32Ret;
    HI_S32          s32ChnNum    	= 2;
    VENC_CHN        VencChn[2]    	= {0,1};
    HI_BOOL         bRcnRefShareBuf = HI_TRUE;
	VPSS_LOW_DELAY_INFO_S stLowDelayInfo;
	SAMPLE_VI_CONFIG_S stViConfig;
    VI_DEV          ViDev        = 0;
    VI_PIPE         ViPipe       = 0;
    VI_CHN          ViChn        = 0;
    VPSS_GRP        VpssGrp        = 0;
    VPSS_CHN        VpssChn[2]     = {0,1};
    HI_BOOL         abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {1,1,0};
    SAMPLE_VPSS_CHN_ATTR_S stParam;
    SAMPLE_VB_ATTR_S commVbAttr;
	SIZE_S          stSize[2];
	
	// 由 PIC_SIZE_E 定义转换为 SIZE_S 数字
    for (i=0; i<s32ChnNum; i++) {
        s32Ret = SAMPLE_COMM_SYS_GetPicSize(pconf->res[i], &stSize[i]);
        if (HI_SUCCESS != s32Ret) {
            SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
            return s32Ret;
        }
    }

	// 取得 Sensor 信息
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    if(SAMPLE_SNS_TYPE_BUTT == stViConfig.astViInfo[0].stSnsInfo.enSnsType) {
        SAMPLE_PRT("Not set SENSOR%d_TYPE !\n",0);
        return HI_FAILURE;
    }

	// 检查 Sensor 是否支持设置的分辨率 // 不用检查了，你们不要乱写就行

	// 搞一个默认的 VPSS 设置到 stParam 中
    SAMPLE_VENC_GetDefaultVpssAttr(stViConfig.astViInfo[0].stSnsInfo.enSnsType, abChnEnable, stSize, &stParam);
	
/*	// 如果开了防抖就改成VPSS离线 
	// For 3516ev300, DIS only works with VPSS Offline
	if (pconf->dis == HI_TRUE) {
		stParam.ViVpssMode     = VI_ONLINE_VPSS_OFFLINE;	
	}*/
    /******************************************
      step 1: init sys alloc common vb
    ******************************************/
    memset(&commVbAttr, 0, sizeof(commVbAttr));
    commVbAttr.supplementConfig = HI_FALSE;
    SAMPLE_VENC_GetCommVbAttr(stViConfig.astViInfo[0].stSnsInfo.enSnsType, &stParam, HI_FALSE, &commVbAttr);

	// 初始化啥？
    s32Ret = SAMPLE_VENC_SYS_Init(&commVbAttr);
    if(s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("Init SYS err for %#x!\n", s32Ret);
        return s32Ret;
    }

    stViConfig.s32WorkingViNum       = 1;
    stViConfig.astViInfo[0].stDevInfo.ViDev     = ViDev;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0] = ViPipe;
    stViConfig.astViInfo[0].stChnInfo.ViChn     = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    s32Ret = SAMPLE_VENC_VI_Init(&stViConfig, stParam.ViVpssMode);
    if(s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("Init VI err for %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_VENC_VPSS_Init(VpssGrp, &stParam, pconf->dis);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Init VPSS err for %#x!\n", s32Ret);
        goto EXIT_VI_STOP;
    }

    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if(s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("VI Bind VPSS err for %#x!\n", s32Ret);
        goto EXIT_VPSS_STOP;
    }
	
	// 2轴消抖 效果几乎没有……并且必须让VPSS为离线模式
	// 虽然内存是够的 但有待测试一下是否会增加延迟

	// 算了 效果实在是没有
	
	/*
	if (pconf->dis == HI_TRUE) {
		ISP_DIS_ATTR_S stDisAttr;
		stDisAttr.bEnable = HI_TRUE;
		s32Ret = HI_MPI_ISP_SetDISAttr(0, &stDisAttr);
		if (s32Ret != HI_SUCCESS) {
			SAMPLE_PRT("HI_MPI_ISP_SetDISAttr failed with %#x\n", s32Ret);
			return HI_FAILURE;
		}
	}
	*/

   /******************************************
    start stream venc
    ******************************************/
	// 设置两个通道的码率控制方式和帧预测方式
	// 通道0要存储，通道1用于回传

   /***encode ch0 **/
	s32Ret = SAMPLE_COMM_YJSNPIVENC_Start(VencChn[0], 0, pconf, bRcnRefShareBuf);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Venc Chn 0 Start failed for %#x!\n", s32Ret);
        goto EXIT_VI_VPSS_UNBIND;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn[0], VencChn[0]);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Venc Chn 0 bind Vpss failed for %#x!\n", s32Ret);
        goto EXIT_VENC_H265_STOP;
    }

    /***encode ch1 **/
	s32Ret = SAMPLE_COMM_YJSNPIVENC_Start(VencChn[1], 1, pconf, bRcnRefShareBuf);
	
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Venc Chn 1 Start failed for %#x!\n", s32Ret);
        goto EXIT_VENC_H265_UnBind;
    }
    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn[1], VencChn[1]);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Venc Chn 1 bind Vpss failed for %#x!\n", s32Ret);
        goto EXIT_VENC_H264_STOP;
    }

	// 开启通道1的低延迟模式
	s32Ret = HI_MPI_VPSS_GetLowDelayAttr(VpssGrp, VpssChn[1] ,&stLowDelayInfo);
	if (HI_SUCCESS != s32Ret) {
		SAMPLE_PRT("HI_MPI_VPSS_GetLowDelayAttr at Vpss chn 1 failed!\n");
		goto EXIT_VENC_H264_STOP;
	}
	stLowDelayInfo.bEnable = HI_TRUE;
	stLowDelayInfo.u32LineCnt = stSize[1].u32Height/2;
	s32Ret = HI_MPI_VPSS_SetLowDelayAttr(VpssGrp, VpssChn[1], &stLowDelayInfo);
	if (HI_SUCCESS != s32Ret) {
		SAMPLE_PRT("HI_MPI_VPSS_SetLowDelayAttr at Vpss chn 1 failed!\n");
		goto EXIT_VENC_H264_STOP;
	}

    /******************************************
     stream save process
    ******************************************/
    s32Ret = SAMPLE_COMM_YJSNPIVENC_StartGetStream(VencChn, s32ChnNum, pconf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto EXIT_VENC_H264_UnBind;
    }

    printf("please press twice ENTER to exit this sample\n");
    getchar();
    getchar();

    /******************************************
     exit process
    ******************************************/
    SAMPLE_COMM_VENC_StopGetStream();

EXIT_VENC_H264_UnBind:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp,VpssChn[1],VencChn[1]);
EXIT_VENC_H264_STOP:
    SAMPLE_COMM_VENC_Stop(VencChn[1]);
EXIT_VENC_H265_UnBind:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp,VpssChn[0],VencChn[0]);
EXIT_VENC_H265_STOP:
    SAMPLE_COMM_VENC_Stop(VencChn[0]);
EXIT_VI_VPSS_UNBIND:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe,ViChn,VpssGrp);
EXIT_VPSS_STOP:
    SAMPLE_COMM_VPSS_Stop(VpssGrp,abChnEnable);
EXIT_VI_STOP:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

/******************************************************************************
* function    : main()
* Description : video venc 
******************************************************************************/

int main (int argc, char *argv[])
{
    HI_S32 s32Ret;
	YJSNPI_VENC_CONFIG_S conf;
	dictionary * ini;
	char *ini_file;
	
    printf("============================================================================\n");
    printf("YJSNPI-Hi Video Encoder \n");
    printf("============================================================================\n");
	
	// set signal handler
    signal(SIGINT, SAMPLE_VENC_HandleSig);
    signal(SIGTERM, SAMPLE_VENC_HandleSig);
	
	// get config from ini
	ini_file = argv[1];
	ini = iniparser_load(ini_file);
	if (!ini) {
		fprintf(stderr,"main: iniparser_load: failed.\n");
		exit(1);
	}
	s32Ret = SAMPLE_YJSNPI_GetConfigFromIni(&conf, ini);
	iniparser_freedict(ini);
	if (HI_FAILURE == s32Ret) {
		printf("main: SAMPLE_YJSNPI_GetConfigFromIni failed.\n");
		exit(s32Ret);
	}
	
	// run
	s32Ret = SAMPLE_VENC_H265_H264(&conf);
	
	// exit
    if (HI_SUCCESS == s32Ret) { 
		printf("program exit normally!\n"); 
	} else { 
		printf("program exit abnormally!\n"); 
	}
	
	
    exit(s32Ret);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
