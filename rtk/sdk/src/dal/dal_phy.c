/*
 * Copyright(c) Realtek Semiconductor Corporation, 2008
 * All rights reserved.
 *
 * $Revision$
 * $Date$
 *
 * Purpose : PHY API for DAL
 *
 * Feature :
 *
 *
 */

/*
 * Include Files
 */
#include <common/rt_type.h>
#include <common/rt_error.h>
#include <common/debug/rt_log.h>
#include <common/util/rt_util.h>
#include <osal/sem.h>
#include <osal/time.h>
#include <osal/memory.h>
#include <dal/dal_mgmt.h>
#include <dal/dal_phy.h>
#include <dal/dal_waMon.h>
#include <hal/common/miim.h>
#include <hal/common/halctrl.h>
#include <rtk/default.h>
#include <hal/mac/serdes.h>
#include <soc/type.h>

#include <dal/dal_linkMon.h>

#if defined(CONFIG_SDK_RTL8295R) && defined(CONFIG_SDK_RTL8390)
  #include <hal/mac/reg.h>
  #include <hal/chipdef/cypress/rtk_cypress_reg_struct.h>
#endif/* end defined(CONFIG_SDK_RTL8295R) && defined(CONFIG_SDK_RTL8390) */

#ifdef CONFIG_SDK_RTL8380
#include <hal/mac/reg.h>
#include <hal/chipdef/maple/rtk_maple_reg_struct.h>
#endif  /* CONFIG_SDK_RTL8380 */

#if defined(CONFIG_SDK_RTL8390)
  #include <hal/phy/phy_rtl8390.h>
#endif
#if defined(CONFIG_SDK_RTL9300)
  #include <hal/phy/phy_rtl9300.h>
#endif
#if defined(CONFIG_SDK_RTL9310)
  #include <hal/phy/phy_rtl9310.h>
#endif
#if defined(CONFIG_SDK_RTL8295R) || defined(CONFIG_SDK_RTL8214QF) || defined(CONFIG_SDK_RTL8224QF)
  #include <hal/phy/phy_rtl8295.h>
#endif
#include <hal/common/miim_debug.h>
#include <hal/mac/miim_common_drv.h>

/*
 * Symbol Definition
 */

typedef struct dal_phy_info_s {
    uint8   force_mode_speed[RTK_MAX_NUM_OF_PORTS];
    uint8   force_fiber_mode_speed[RTK_MAX_NUM_OF_PORTS];
    uint8   force_mode_duplex[RTK_MAX_NUM_OF_PORTS];
    uint8   force_mode_flowControl[RTK_MAX_NUM_OF_PORTS];
    uint8   force_mode_rxPause[RTK_MAX_NUM_OF_PORTS];
    uint8   force_mode_txPause[RTK_MAX_NUM_OF_PORTS];
    uint8   auto_mode_pause[RTK_MAX_NUM_OF_PORTS];
    uint8   auto_mode_asy_pause[RTK_MAX_NUM_OF_PORTS];
    uint8   cross_over_mode[RTK_MAX_NUM_OF_PORTS];
} dal_phy_info_t;


/*
 * Data Declaration
 */
static uint32           phy_init[RTK_MAX_NUM_OF_UNIT] = {INIT_NOT_COMPLETED};
static osal_mutex_t     phy_sem[RTK_MAX_NUM_OF_UNIT];
static dal_phy_info_t   *pPhy_info[RTK_MAX_NUM_OF_UNIT];

static rtk_phy_batch_entry_t *batch_profile = NULL;
static init_state_t          batch_profile_state = INIT_NOT_COMPLETED;
static uint32                batch_size = 100;
static uint32                batch_next_index = 0;
static init_state_t          batch_port_state = INIT_NOT_COMPLETED;
static rtk_portmask_t        batch_portmask;
static uint32                batch_output = 0;
static uint32                batch_err_cnt = 0;
#if defined(CONFIG_SDK_WA_FIBER_RX_WATCHDOG)
extern uint32 fiber_rx_watchdog_cnt[RTK_MAX_NUM_OF_UNIT];
#endif
#if defined(CONFIG_SDK_WA_PHY_WATCHDOG)
extern uint32 phy_watchdog_cnt[RTK_MAX_NUM_OF_UNIT];
extern uint32 phySerdes_watchdog_cnt[RTK_MAX_NUM_OF_UNIT];
static  uint32  phySerdes_rst_flag[RTK_MAX_NUM_OF_UNIT][RTK_MAX_PHY_PER_UNIT] = {{0}};
static uint8    fiberRstSts[RTK_MAX_NUM_OF_UNIT][RTK_MAX_PORT_PER_UNIT];
#endif  /* CONFIG_SDK_WA_PHY_WATCHDOG */

/*
 * Macro Definition
 */
#define PHY_SEM_LOCK(unit)    \
do {\
    if (osal_sem_mutex_take(phy_sem[unit], OSAL_SEM_WAIT_FOREVER) != RT_ERR_OK)\
    {\
        RT_ERR(RT_ERR_SEM_LOCK_FAILED, (MOD_DAL|MOD_PHY), "semaphore lock failed");\
        return RT_ERR_SEM_LOCK_FAILED;\
    }\
} while(0)

#define PHY_SEM_UNLOCK(unit)   \
do {\
    if (osal_sem_mutex_give(phy_sem[unit]) != RT_ERR_OK)\
    {\
        RT_ERR(RT_ERR_SEM_UNLOCK_FAILED, (MOD_DAL|MOD_PHY), "semaphore unlock failed");\
        return RT_ERR_SEM_UNLOCK_FAILED;\
    }\
} while(0)

#define BATCH_PRT(mode, cur_mode, fmt, args...)  \
do {\
    if (mode == cur_mode)\
    {\
        osal_printf(fmt, ##args);\
    }\
} while (0)

/*
 * Function Declaration
 */
int32
dal_phy_portAutoNegoEnablePortmask_set(uint32 unit, rtk_portmask_t *pPortMask,
    rtk_enable_t enable);


/* Module Name : */

/* Function Name:
 *      _dal_phy_portForceModePause_get
 * Description:
 *      Converting tx-pause & rx-pause to phy's symmetric PAUSE & asymmetric PAUSE ability
 * Input:
 *      unit        - unit id
 *      port        - port id
 * Output:
 *      pAbility    - FC and AsyFC
 * Return:
 *      None
 * Note:
 *      None
 */
void
_dal_phy_portForceModePause_get(uint32 unit, rtk_port_t port, rtk_port_phy_ability_t *pAbility)
{
    if ((pPhy_info[unit]->force_mode_txPause[port] == DISABLED) && (pPhy_info[unit]->force_mode_rxPause[port] == DISABLED))
    {
        pAbility->FC = 0;
        pAbility->AsyFC = 0;
    }
    else if ((pPhy_info[unit]->force_mode_txPause[port] == ENABLED) && (pPhy_info[unit]->force_mode_rxPause[port] == ENABLED))
    {
        pAbility->FC = 1;
        pAbility->AsyFC = 1; /* this bit is "don't care" when partner pause is 1. */
    }
    else if ((pPhy_info[unit]->force_mode_txPause[port] == ENABLED) && (pPhy_info[unit]->force_mode_rxPause[port] == DISABLED))
    {
        pAbility->FC = 0;
        pAbility->AsyFC = 1;
    }
    else /* ((pPhy_info[unit]->force_mode_txPause[port] == DISABLED) && (pPhy_info[unit]->force_mode_rxPause[port] == ENABLED)) */
    {
        pAbility->FC = 1;
        pAbility->AsyFC = 1;
    }
}



/* Module Name    : Diag */
/* Sub-module Name: RTCT */

/* Function Name:
 *      dal_phy_portRtctResult_get
 * Description:
 *      Get test result of RTCT.
 * Input:
 *      unit        - unit id
 *      port        - the port for retriving RTCT test result
 * Output:
 *      pRtctResult - RTCT result
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_PHY_RTCT_NOT_FINISH   - RTCT not finish. Need to wait a while.
 *      RT_ERR_TIMEOUT      - RTCT test timeout in this port.
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      If linkType is PORT_SPEED_1000M, test result will be stored in ge_result.
 *      If linkType is PORT_SPEED_10M or PORT_SPEED_100M, test result will be stored in fe_result.
 */
int32
dal_phy_portRtctResult_get(uint32 unit, rtk_port_t port, rtk_rtctResult_t *pRtctResult)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d",
           unit, port);

    /* parameter check */
    RT_PARAM_CHK(!HWP_ETHER_PORT(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pRtctResult), RT_ERR_NULL_POINTER);

    osal_memset(pRtctResult, 0, sizeof(rtk_rtctResult_t));

    PHY_SEM_LOCK(unit);

    /* Get RTCT Result */
    if ((ret = phy_rtctResult_get(unit, port, pRtctResult)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "ret=0x%x", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /*end of dal_phy_portRtctResult_get*/

/* Function Name:
 *      dal_phy_rtctEnable_set
 * Description:
 *      Start RTCT for ports.
 *      When enable RTCT, the port won't transmit and receive normal traffic.
 * Input:
 *      unit      - unit id
 *      pPortmask - the ports for RTCT test
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_rtctEnable_set(uint32 unit, rtk_portmask_t *pPortmask)
{
    rtk_port_t  port;
    int32       ret = RT_ERR_FAILED;

    RT_PARAM_CHK((NULL == pPortmask), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK(!HWP_PMSK_EXIST(unit, pPortmask), RT_ERR_PORT_MASK);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, *pPortmask=0x%x",
           unit, *pPortmask);

    PHY_SEM_LOCK(unit);
    HWP_PORT_TRAVS(unit, port)
    {
        if (RTK_PORTMASK_IS_PORT_SET(*pPortmask, port))
        {
            if ((ret = phy_rtct_start(unit, port)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "ret=0x%x", ret);
                return ret;
            }
        }
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_rtctEnable_set */

/* Function Name:
 *      dal_phyMapper_init
 * Description:
 *      Hook phy module of the specified device.
 * Input:
 *      pMapper - pointer of mapper
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 * Note:
 *      Must Hook phy module before calling any phy APIs.
 */
int32
dal_phyMapper_init(dal_mapper_t *pMapper)
{
    pMapper->phy_init = dal_phy_init;
    pMapper->port_phyAutoNegoEnable_get = dal_phy_portAutoNegoEnable_get;
    pMapper->port_phyMasterSlave_get = dal_phy_portMasterSlave_get;
    pMapper->port_phyMasterSlave_set = dal_phy_portMasterSlave_set;
    pMapper->port_gigaLiteEnable_get = dal_phy_portGigaLiteEnable_get;
    pMapper->port_gigaLiteEnable_set = dal_phy_portGigaLiteEnable_set;
    pMapper->port_phy2pt5gLiteEnable_get = dal_phy_port2pt5gLiteEnable_get;
    pMapper->port_phy2pt5gLiteEnable_set = dal_phy_port2pt5gLiteEnable_set;
    pMapper->port_phyReg_get = dal_phy_portReg_get;
    pMapper->port_phyReg_set = dal_phy_portReg_set;
    pMapper->port_phyExtParkPageReg_get = dal_phy_portExtParkPageReg_get;
    pMapper->port_phyExtParkPageReg_set = dal_phy_portExtParkPageReg_set;
    pMapper->port_phymaskExtParkPageReg_set = dal_phy_portmaskExtParkPageReg_set;
    pMapper->port_phyMmdReg_get = dal_phy_portMmdReg_get;
    pMapper->port_phyMmdReg_set = dal_phy_portMmdReg_set;
    pMapper->port_phymaskMmdReg_set = dal_phy_portmaskMmdReg_set;
    pMapper->port_phyComboPortMedia_get = dal_phy_portComboPortMedia_get;
    pMapper->port_phyComboPortMedia_set = dal_phy_portComboPortMedia_set;
    pMapper->port_greenEnable_get = dal_phy_portGreenEnable_get;
    pMapper->port_greenEnable_set = dal_phy_portGreenEnable_set;
    pMapper->port_phyCrossOverMode_get = dal_phy_portCrossOverMode_get;
    pMapper->port_phyCrossOverMode_set = dal_phy_portCrossOverMode_set;
    pMapper->port_phyCrossOverStatus_get = dal_phy_portCrossOverStatus_get;
    pMapper->port_phyComboPortFiberMedia_get = dal_phy_portComboPortFiberMedia_get;
    pMapper->port_phyComboPortFiberMedia_set = dal_phy_portComboPortFiberMedia_set;
    pMapper->port_linkDownPowerSavingEnable_get = dal_phy_portLinkDownPowerSavingEnable_get;
    pMapper->port_linkDownPowerSavingEnable_set = dal_phy_portLinkDownPowerSavingEnable_set;
    pMapper->port_downSpeedEnable_get = dal_phy_portDownSpeedEnable_get;
    pMapper->port_downSpeedEnable_set = dal_phy_portDownSpeedEnable_set;
    pMapper->port_downSpeedStatus_get = dal_phy_portDownSpeedStatus_get;
    pMapper->port_fiberDownSpeedEnable_get = dal_phy_portFiberDownSpeedEnable_get;
    pMapper->port_fiberDownSpeedEnable_set = dal_phy_portFiberDownSpeedEnable_set;
    pMapper->port_fiberNwayForceLinkEnable_get = dal_phy_portFiberNwayForceLinkEnable_get;
    pMapper->port_fiberNwayForceLinkEnable_set = dal_phy_portFiberNwayForceLinkEnable_set;
    pMapper->port_phyLoopBackEnable_get = dal_phy_portLoopBack_get;
    pMapper->port_phyLoopBackEnable_set = dal_phy_portLoopBack_set;
    pMapper->port_phyFiberTxDis_set = dal_phy_portFiberTxDis_set;
    pMapper->port_phyFiberTxDisPin_set = dal_phy_portFiberTxDisPin_set;
    pMapper->diag_rtctEnable_set = dal_phy_rtctEnable_set;
    pMapper->diag_portRtctResult_get = dal_phy_portRtctResult_get;
    pMapper->port_phyForceModeAbility_get = dal_phy_portForceModeAbility_get;
    pMapper->port_phyForceModeAbility_set = dal_phy_portForceModeAbility_set;
    pMapper->port_phyForceFlowctrlMode_get = dal_phy_portForceFlowctrlMode_get;
    pMapper->port_phyForceFlowctrlMode_set = dal_phy_portForceFlowctrlMode_set;
    pMapper->port_phyAutoNegoEnable_set = dal_phy_portAutoNegoEnable_set;
    pMapper->port_phyAutoNegoAbilityLocal_get = dal_phy_portAutoNegoAbilityLocal_get;
    pMapper->port_phyAutoNegoAbility_get = dal_phy_portAutoNegoAbility_get;
    pMapper->port_phyAutoNegoAbility_set = dal_phy_portAutoNegoAbility_set;
    pMapper->port_fiberRxEnable_get = dal_phy_portFiberRxEnable_get;
    pMapper->port_fiberRxEnable_set = dal_phy_portFiberRxEnable_set;
    pMapper->port_10gMedia_get      = dal_phy_port10gMedia_get;
    pMapper->port_10gMedia_set      = dal_phy_port10gMedia_set;
    pMapper->port_phyIeeeTestMode_set = dal_phy_portIeeeTestMode_set;
    pMapper->port_phyPolar_get = dal_phy_polar_get;
    pMapper->port_phyPolar_set = dal_phy_polar_set;
    pMapper->port_phyEyeMonitor_start = dal_phy_portEyeMonitor_start;
    pMapper->port_phyEyeMonitorInfo_get = dal_phy_portEyeMonitorInfo_get;
    pMapper->port_imageFlash_load = dal_phy_portFlashImage_load;
    pMapper->port_phySds_get = dal_phy_portSds_get;
    pMapper->port_phySds_set = dal_phy_portSds_set;
    pMapper->port_phySdsRxCaliStatus_get = dal_phy_sdsRxCaliStatus_get;
    pMapper->port_phySdsReg_get = dal_phy_portSdsReg_get;
    pMapper->port_phySdsReg_set = dal_phy_portSdsReg_set;

    pMapper->time_portPtpEnable_get = dal_phy_portPtpEnable_get;
    pMapper->time_portPtpEnable_set = dal_phy_portPtpEnable_set;
    pMapper->time_portPtpRxTimestamp_get = dal_phy_portPtpRxTimestamp_get;
    pMapper->time_portPtpTxTimestamp_get = dal_phy_portPtpTxTimestamp_get;
    pMapper->time_portRefTime_get = dal_phy_portRefTime_get;
    pMapper->time_portRefTime_set = dal_phy_portRefTime_set;
    pMapper->time_portRefTimeAdjust_set = dal_phy_portRefTimeAdjust_set;
    pMapper->time_portRefTimeEnable_get = dal_phy_portRefTimeEnable_get;
    pMapper->time_portRefTimeEnable_set = dal_phy_portRefTimeEnable_set;
    pMapper->time_portRefTimeFreq_get = dal_phy_portRefTimeFreq_get;
    pMapper->time_portRefTimeFreq_set = dal_phy_portRefTimeFreq_set;
    pMapper->time_portPtpMacAddr_get = dal_phy_portSwitchMacAddr_get;
    pMapper->time_portPtpMacAddr_set = dal_phy_portSwitchMacAddr_set;
    pMapper->time_portPtpMacAddrRange_get = dal_phy_portPtpMacAddrRange_get;
    pMapper->time_portPtpMacAddrRange_set = dal_phy_portPtpMacAddrRange_set;
    pMapper->time_portPtpVlanTpid_get = dal_phy_portPtpVlanTpid_get;
    pMapper->time_portPtpVlanTpid_set = dal_phy_portPtpVlanTpid_set;
    pMapper->time_portPtpOper_get = dal_phy_portPtpOper_get;
    pMapper->time_portPtpOper_set = dal_phy_portPtpOper_set;
    pMapper->time_portPtpLatchTime_get = dal_phy_portPtpLatchTime_get;
    pMapper->time_portPtpRefTimeFreqCfg_get = dal_phy_portPtpRefTimeFreqCfg_get;
    pMapper->time_portPtpRefTimeFreqCfg_set = dal_phy_portPtpRefTimeFreqCfg_set;
    pMapper->time_portPtpTxInterruptStatus_get = dal_phy_portPtpTxInterruptStatus_get;
    pMapper->time_portPtpInterruptEnable_get = dal_phy_portPtpInterruptEnable_get;
    pMapper->time_portPtpInterruptEnable_set = dal_phy_portPtpInterruptEnable_set;
    pMapper->time_portPtpTxTimestampFifo_get = dal_phy_portPtpTxTimestampFifo_get;
    pMapper->time_portPtp1PPSOutput_get = dal_phy_portPtp1PPSOutput_get;
    pMapper->time_portPtp1PPSOutput_set = dal_phy_portPtp1PPSOutput_set;
    pMapper->time_portPtpClockOutput_get = dal_phy_portPtpClockOutput_get;
    pMapper->time_portPtpClockOutput_set = dal_phy_portPtpClockOutput_set;
    pMapper->time_portPtpOutputSigSel_get = dal_phy_portPtpOutputSigSel_get;
    pMapper->time_portPtpOutputSigSel_set = dal_phy_portPtpOutputSigSel_set;
    pMapper->time_portPtpTransEnable_get = dal_phy_portPtpTransEnable_get;
    pMapper->time_portPtpTransEnable_set = dal_phy_portPtpTransEnable_set;
    pMapper->time_portPtpLinkDelay_get = dal_phy_portPtpLinkDelay_get;
    pMapper->time_portPtpLinkDelay_set = dal_phy_portPtpLinkDelay_set;
    pMapper->port_phyReset_set = dal_phy_portReset_set;
    pMapper->port_phyLinkStatus_get = dal_phy_portLinkStatus_get;
    pMapper->port_phyPeerAutoNegoAbility_get = dal_phy_portPeerAutoNegoAbility_get;
    pMapper->port_phyMacIntfSerdesMode_get = dal_phy_portMacIntfSerdesMode_get;
    pMapper->port_phyLedMode_set = dal_phy_portLedMode_set;
    pMapper->port_phyLedCtrl_get = dal_phy_portLedCtrl_get;
    pMapper->port_phyLedCtrl_set = dal_phy_portLedCtrl_set;
    pMapper->port_phyMacIntfSerdesLinkStatus_get = dal_phy_portMacIntfSerdesLinkStatus_get;
    pMapper->port_phySdsEyeParam_get = dal_phy_portSdsEyeParam_get;
    pMapper->port_phySdsEyeParam_set = dal_phy_portSdsEyeParam_set;
    pMapper->port_phyMdiLoopbackEnable_get = dal_phy_portMdiLoopbackEnable_get;
    pMapper->port_phyMdiLoopbackEnable_set = dal_phy_portMdiLoopbackEnable_set;
    pMapper->port_phyIntr_init = dal_phy_portIntr_init;
    pMapper->port_phyIntrEnable_get = dal_phy_portIntrEnable_get;
    pMapper->port_phyIntrEnable_set = dal_phy_portIntrEnable_set;
    pMapper->port_phyIntrStatus_get = dal_phy_portIntrStatus_get;
    pMapper->port_phyIntrMask_get = dal_phy_portIntrMask_get;
    pMapper->port_phyIntrMask_set = dal_phy_portIntrMask_set;
    pMapper->port_phySdsTestMode_set = dal_phy_portPhySdsTestMode_set;
    pMapper->port_phySdsTestModeCnt_get = dal_phy_portPhySdsTestModeCnt_get;
    pMapper->port_phySdsLeq_get = dal_phy_portSdsLeq_get;
    pMapper->port_phySdsLeq_set = dal_phy_portSdsLeq_set;
    pMapper->port_phyCtrl_get = dal_phy_portCtrl_get;
    pMapper->port_phyCtrl_set = dal_phy_portCtrl_set;
    pMapper->port_phyLiteEnable_get = dal_phy_portLiteEnable_get;
    pMapper->port_phyLiteEnable_set = dal_phy_portLiteEnable_set;
    pMapper->port_phyDbgCounter_get = dal_phy_portDbgCounter_get;
    pMapper->eee_portEnable_get = dal_phy_portEeeEnable_get;
    pMapper->eee_portEnable_set = dal_phy_portEeeEnable_set;

    pMapper->port_adminEnable_set = dal_phy_portEnable_set;
    pMapper->port_adminEnable_get = dal_phy_portEnable_get;
    pMapper->port_speedDuplex_get = dal_phy_portSpeedDuplex_get;
    pMapper->port_linkMedia_get = dal_phy_portLinkMedia_get;

    pMapper->debug_phy_get = dal_phy_debug_get;
    pMapper->debug_phy_batch_port_set = dal_phy_debug_batch_port_set;
    pMapper->debug_phy_batch_op_set = dal_phy_debug_batch_op_set;

    pMapper->port_macsecReg_get = dal_phy_port_macsecReg_get;
    pMapper->port_macsecReg_set = dal_phy_port_macsecReg_set;
    pMapper->macsec_port_cfg_set = dal_phy_macsec_port_cfg_set;
    pMapper->macsec_port_cfg_get = dal_phy_macsec_port_cfg_get;
    pMapper->macsec_sc_create = dal_phy_macsec_sc_create;
    pMapper->macsec_sc_get = dal_phy_macsec_sc_get;
    pMapper->macsec_sc_del = dal_phy_macsec_sc_del;
    pMapper->macsec_sc_status_get = dal_phy_macsec_sc_status_get;
    pMapper->macsec_sa_create = dal_phy_macsec_sa_create;
    pMapper->macsec_sa_get = dal_phy_macsec_sa_get;
    pMapper->macsec_sa_del = dal_phy_macsec_sa_del;
    pMapper->macsec_sa_activate = dal_phy_macsec_sa_activate;
    pMapper->macsec_rxsa_disable = dal_phy_macsec_rxsa_disable;
    pMapper->macsec_txsa_disable = dal_phy_macsec_txsa_disable;
    pMapper->macsec_stat_clear = dal_phy_macsec_stat_clear;
    pMapper->macsec_stat_port_get = dal_phy_macsec_stat_port_get;
    pMapper->macsec_stat_txsa_get = dal_phy_macsec_stat_txsa_get;
    pMapper->macsec_stat_rxsa_get = dal_phy_macsec_stat_rxsa_get;
    pMapper->macsec_intr_status_get = dal_phy_macsec_intr_status_get;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_phy_init
 * Description:
 *      Initialize PHY module of the specified device.
 * Input:
 *      unit          - unit id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      1. Module must be initialized before using all of APIs in this module
 */
int32
dal_phy_init(uint32 unit)
{
    rtk_port_t              port;
    rtk_port_phy_ability_t  phy_ability;
    rtk_portmask_t          portmask;
#ifndef  __BOOTLOADER__
    int32                   ret;
#endif
    uint32                  sds;

    RT_INIT_REENTRY_CHK(phy_init[unit]);
    phy_init[unit] = INIT_NOT_COMPLETED;

    /* create semaphore */
    phy_sem[unit] = osal_sem_mutex_create();
    if (0 == phy_sem[unit])
    {
        RT_ERR(RT_ERR_FAILED, (MOD_DAL|MOD_PHY), "semaphore create failed");
        return RT_ERR_FAILED;
    }

    #if defined(CONFIG_SDK_WA_PHY_WATCHDOG)
    osal_memset(fiberRstSts[unit], 0, sizeof(uint8) * RTK_MAX_PORT_PER_UNIT);
    #endif  /* CONFIG_SDK_WA_PHY_WATCHDOG */

    pPhy_info[unit] = (dal_phy_info_t *)osal_alloc(sizeof(dal_phy_info_t));
    if (NULL == pPhy_info[unit])
    {
        RT_ERR(RT_ERR_FAILED, (MOD_DAL|MOD_PHY), "memory allocate failed");
        return RT_ERR_FAILED;
    }

    osal_memset(pPhy_info[unit], 0, sizeof(dal_phy_info_t));

    phy_init[unit] = INIT_COMPLETED;

    RTK_PORTMASK_RESET(portmask);

    HWP_PORT_TRAVS_EXCEPT_CPU(unit, port)
    {
        if (!HWP_PHY_EXIST(unit, port) && !HWP_SERDES_PORT(unit, port))
            continue;

        pPhy_info[unit]->auto_mode_pause[port] = RTK_DEFAULT_PORT_PAUSE_CAPABILITY;
        pPhy_info[unit]->auto_mode_asy_pause[port] = RTK_DEFAULT_PORT_ASYPAUSE_CAPABILITY;
        pPhy_info[unit]->force_mode_duplex[port] = PORT_FULL_DUPLEX;
        pPhy_info[unit]->force_mode_rxPause[port] = ENABLED;
        pPhy_info[unit]->force_mode_txPause[port] = ENABLED;

        osal_memset(&phy_ability, 0, sizeof(rtk_port_phy_ability_t));
        phy_ability.FC = RTK_DEFAULT_PORT_PAUSE_CAPABILITY;
        phy_ability.AsyFC = RTK_DEFAULT_PORT_ASYPAUSE_CAPABILITY;

        if (HWP_FE_PORT(unit,port))
        {
            pPhy_info[unit]->force_mode_speed[port] = PORT_SPEED_100M;
            pPhy_info[unit]->force_fiber_mode_speed[port] = PORT_SPEED_100M;

            /* auto-nego ability */
            if (HWP_COPPER_PORT(unit,port))
            {
                phy_ability.Half_100 = RTK_DEFAULT_PORT_100HALF_CAPABLE;
                phy_ability.Full_100 = RTK_DEFAULT_PORT_100FULL_CAPABLE;
                phy_ability.Half_10 = RTK_DEFAULT_PORT_10HALF_CAPABLE;
                phy_ability.Full_10 = RTK_DEFAULT_PORT_10FULL_CAPABLE;
            }
            else
            {
                continue;
            }

        }
        else if (HWP_GE_PORT(unit,port))
        {
            pPhy_info[unit]->force_mode_speed[port] = PORT_SPEED_1000M;
            pPhy_info[unit]->force_fiber_mode_speed[port] = PORT_SPEED_1000M;

            /* auto-nego ability */
            phy_ability.Half_1000 = RTK_DEFAULT_PORT_1000HALF_CAPABLE;
            phy_ability.Full_1000 = RTK_DEFAULT_PORT_1000FULL_CAPABLE;
            if (HWP_COPPER_PORT(unit,port) || HWP_COMBO_PORT(unit,port))
            {
                phy_ability.Half_100 = RTK_DEFAULT_PORT_100HALF_CAPABLE;
                phy_ability.Full_100 = RTK_DEFAULT_PORT_100FULL_CAPABLE;
                phy_ability.Half_10 = RTK_DEFAULT_PORT_10HALF_CAPABLE;
                phy_ability.Full_10 = RTK_DEFAULT_PORT_10FULL_CAPABLE;
            }
        }
        else if (HWP_2_5GE_PORT(unit,port))
        {
            /* for 2.5G Copper */
            pPhy_info[unit]->force_mode_speed[port] = PORT_SPEED_2_5G;

            /* auto-nego ability */
            phy_ability.adv_2_5G  = RTK_DEFAULT_PORT_2_5G_CAPABLE;
            phy_ability.Full_1000 = RTK_DEFAULT_PORT_1000FULL_CAPABLE;
            phy_ability.Half_100 = RTK_DEFAULT_PORT_100HALF_CAPABLE;
            phy_ability.Full_100 = RTK_DEFAULT_PORT_100FULL_CAPABLE;
            phy_ability.Half_10 = RTK_DEFAULT_PORT_10HALF_CAPABLE;
            phy_ability.Full_10 = RTK_DEFAULT_PORT_10FULL_CAPABLE;
        }
        else if (HWP_5GE_PORT(unit,port))
        {
            /* for 5G Copper */
            pPhy_info[unit]->force_mode_speed[port] = PORT_SPEED_5G;

            /* auto-nego ability */
            phy_ability.adv_5G = RTK_DEFAULT_PORT_5G_CAPABLE;
            phy_ability.adv_2_5G  = RTK_DEFAULT_PORT_2_5G_CAPABLE;
            phy_ability.Full_1000 = RTK_DEFAULT_PORT_1000FULL_CAPABLE;
            phy_ability.Half_100 = RTK_DEFAULT_PORT_100HALF_CAPABLE;
            phy_ability.Full_100 = RTK_DEFAULT_PORT_100FULL_CAPABLE;
        }
        else if (HWP_10GE_PORT(unit,port))
        {
            pPhy_info[unit]->force_mode_speed[port] = PORT_SPEED_10G;
            pPhy_info[unit]->force_fiber_mode_speed[port] = PORT_SPEED_10G;

            if (HWP_SERDES_PORT(unit, port))
            {
                sds = HWP_PORT_SDSID(unit, port);
                if (RTK_MII_10GR == HWP_SDS_MODE(unit, sds))
                {
                    phy_ability.FC = DISABLED;
                    phy_ability.AsyFC = DISABLED;
                    pPhy_info[unit]->force_mode_flowControl[port] = DISABLED;
                }
                else
                {
                    phy_ability.Half_1000 = RTK_DEFAULT_PORT_1000HALF_CAPABLE;
                    phy_ability.Full_1000 = RTK_DEFAULT_PORT_1000FULL_CAPABLE;
                }
            }
            /* auto-nego ability */
            else if (HWP_FIBER_PORT(unit,port))
            {
                phy_ability.Half_1000 = RTK_DEFAULT_PORT_1000HALF_CAPABLE;
                phy_ability.Full_1000 = RTK_DEFAULT_PORT_1000FULL_CAPABLE;
            }
            else if (HWP_COPPER_PORT(unit,port) || HWP_COMBO_PORT(unit,port))
            {
                phy_ability.Half_100 = RTK_DEFAULT_PORT_100HALF_CAPABLE;
                phy_ability.Full_100 = RTK_DEFAULT_PORT_100FULL_CAPABLE;
                phy_ability.Half_1000 = RTK_DEFAULT_PORT_1000HALF_CAPABLE;
                phy_ability.Full_1000 = RTK_DEFAULT_PORT_1000FULL_CAPABLE;
#if (defined(CONFIG_SDK_RTL8390) || defined(CONFIG_SDK_RTL9300))
                phy_ability.Half_10G = RTK_DEFAULT_PORT_10GHALF_CAPABLE;
                phy_ability.Full_10G = RTK_DEFAULT_PORT_10GFULL_CAPABLE;
#endif
                phy_ability.adv_2_5G = RTK_DEFAULT_PORT_2_5G_CAPABLE;
                phy_ability.adv_5G = RTK_DEFAULT_PORT_5G_CAPABLE;
                phy_ability.adv_10GBase_T = RTK_DEFAULT_PORT_10GBASET_CAPABLE;
            }
            else
            {
                continue;
            }

        }
        else
        {
            continue;
        }

        /* for PHYs only support force-mode on 100M, set default force_mode_speed to 100M*/
        if ((HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_RTL8261B)
            || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_RTL8264B))
        {
            pPhy_info[unit]->force_mode_speed[port] = PORT_SPEED_100M;
        }

#if defined(CONFIG_SDK_PHY_CUST1) || defined(CONFIG_SDK_PHY_CUST2) || defined(CONFIG_SDK_PHY_CUST3) || defined(CONFIG_SDK_PHY_CUST4) || defined(CONFIG_SDK_PHY_CUST5)
        /* do not configure to CUST PHY to let it remains the default configure */
        if ((HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_CUST1)
            || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_CUST2)
            || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_CUST3)
            || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_CUST4)
            || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_CUST5))
        {
            continue;
        }
#endif

#if !defined (__MODEL_USER__)
  #ifndef __BOOTLOADER__
        if (HWP_UNIT_VALID_LOCAL(unit))
        {
            if ((ret = dal_phy_portAutoNegoAbility_set(unit, port, &phy_ability)) != RT_ERR_OK)
            {
                /* skip unsupported ports */
                if ((ret == RT_ERR_CHIP_NOT_SUPPORTED) || (ret == RT_ERR_PORT_NOT_SUPPORTED))
                {
                    continue;
                }

                /* continue to next, don't return here */
                RT_ERR(ret, (MOD_DAL|MOD_PHY), "Port%u init set autonegotiation ability failed", port);
                continue;
            }

            if ((ret = dal_phy_portEeeEnable_set(unit, port, RTK_DEFAULT_EEE_PORT_ENABLE_DEFAULT)) != RT_ERR_OK)
            {
                /* skip unsupported ports */
                if ((ret == RT_ERR_CHIP_NOT_SUPPORTED) || (ret == RT_ERR_PORT_NOT_SUPPORTED))
                {
                    continue;
                }

                /* continue to next, don't return here */
                RT_ERR(ret, (MOD_DAL|MOD_PHY), "Port%u init set EEE %s failed", port, (RTK_DEFAULT_EEE_PORT_ENABLE_DEFAULT == DISABLED)? "disable":"enable");
                continue;
            }
        }
  #endif
        if (!HWP_10GE_PORT(unit, port))
        {
            if (HWP_PHY_MODEL_BY_PORT(unit, port) != RTK_PHYTYPE_RTL8224QF) /* 8224QF 2.5G Fiber not support auto-nego */
                RTK_PORTMASK_PORT_SET(portmask, port);
        }
        else if (HWP_COPPER_PORT(unit, port))
        {
            RTK_PORTMASK_PORT_SET(portmask, port);
        }
#endif
    }   /* end of HWP_PORT_TRAVS_EXCEPT_CPU */

#if !defined (__MODEL_USER__)
  #ifndef  __BOOTLOADER__
    if (HWP_UNIT_VALID_LOCAL(unit))
    {
        if ((ret = dal_phy_portAutoNegoEnablePortmask_set(unit, &portmask, RTK_DEFAULT_PORT_AUTONEGO_ENABLE)) != RT_ERR_OK)
        {
            RT_ERR(ret, (MOD_DAL|MOD_PHY), "Port init enable PHY autonegotiation failed");
            return ret;
        }
    }
  #endif
#endif

    return RT_ERR_OK;
}/* end of dal_phy_init */

/* Function Name:
 *      dal_phy_portAutoNegoEnable_get
 * Description:
 *      Get PHY ability of the specific port
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pEnable - pointer to PHY auto negotiation status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portAutoNegoEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get value from CHIP*/
    if ((ret = phy_autoNegoEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_PHY), "port %d", port);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pEnable=%d", *pEnable);

    return RT_ERR_OK;
} /* end of dal_phy_portAutoNegoEnable_get */

/* Function Name:
 *      dal_phy_portMasterSlave_get
 * Description:
 *      Get PHY configuration of master/slave mode of the specific port
 * Input:
 *      unit                - unit id
 *      port                - port id
 * Output:
 *      pMasterSlaveCfg     - pointer to the PHY master slave configuration
 *      pMasterSlaveActual  - pointer to the PHY master slave actual link status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID       - invalid port id
 *      RT_ERR_NULL_POINTER  - input parameter may be null pointer
 * Note:
 *      This function only works on giga port to get its master/slave mode configuration.
 */
int32
dal_phy_portMasterSlave_get(
    uint32              unit,
    rtk_port_t          port,
    rtk_port_masterSlave_t   *pMasterSlaveCfg,
    rtk_port_masterSlave_t   *pMasterSlaveActual)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pMasterSlaveCfg), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pMasterSlaveActual), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    ret = phy_masterSlave_get(unit, port, pMasterSlaveCfg, pMasterSlaveActual);

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "*pMasterSlaveCfg=%d, *pMasterSlaveActual=%d", *pMasterSlaveCfg, *pMasterSlaveActual);

    return ret;
}/* end of dal_phy_portMasterSlave_get */

/* Function Name:
 *      dal_phy_portMasterSlave_set
 * Description:
 *      Set PHY configuration of master/slave mode of the specific port
 * Input:
 *      unit                - unit id
 *      port                - port id
 *      masterSlave         - PHY master slave configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID       - invalid port id
 *      RT_ERR_INPUT         - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portMasterSlave_set(
    uint32              unit,
    rtk_port_t          port,
    rtk_port_masterSlave_t   masterSlave)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, masterSlave=%d", unit, port, masterSlave);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK(masterSlave >= PORT_MASTER_SLAVE_END, RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    ret = phy_masterSlave_set(unit, port, masterSlave);

    PHY_SEM_UNLOCK(unit);

    return ret;
}/* end of dal_phy_portMasterSlave_set */

/* Function Name:
 *      dal_phy_portReg_get
 * Description:
 *      Get PHY register data of the specific port
 * Input:
 *      unit  - unit id
 *      port  - port id
 *      page  - page id
 *      reg   - reg id
 * Output:
 *      pData - pointer to the PHY reg data
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_PHY_PAGE_ID  - invalid page id
 *      RT_ERR_PHY_REG_ID   - invalid reg id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portReg_get(
    uint32              unit,
    rtk_port_t          port,
    uint32              page,
    rtk_port_phy_reg_t  reg,
    uint32              *pData)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, page=0x%x, reg=0x%x",
           unit, port, page, reg);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    //RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pData), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_reg_get(unit, port, page, reg, pData)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pData=0x%x", *pData);

    return ret;

} /* end of dal_phy_portReg_get */

/* Function Name:
 *      dal_phy_portReg_set
 * Description:
 *      Set PHY register data of the specific port
 * Input:
 *      unit - unit id
 *      port - port id
 *      page - page id
 *      reg  - reg id
 *      data - reg data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT    - The module is not initial
 *      RT_ERR_PORT_ID     - invalid port id
 *      RT_ERR_PHY_PAGE_ID - invalid page id
 *      RT_ERR_PHY_REG_ID  - invalid reg id
 * Note:
 *      None
 */
int32
dal_phy_portReg_set(
    uint32              unit,
    rtk_port_t          port,
    uint32              page,
    rtk_port_phy_reg_t  reg,
    uint32              data)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, page=0x%x, reg=0x%x \
           data=0x%x", unit, port, page, reg, data);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    //RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_reg_set(unit, port, page, reg, data)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return ret;
} /* end of dal_phy_portReg_set */

/* Function Name:
 *      dal_phy_portMmdReg_get
 * Description:
 *      Get PHY MMD register data of the specific port
 * Input:
 *      unit                - unit id
 *      port                - port id
 *      mmdAddr             - mmd device address
 *      mmdReg              - mmd reg id
 * Output:
 *      pData              - pointer to the PHY reg data
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID       - invalid port id
 *      RT_ERR_NULL_POINTER  - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portMmdReg_get(
    uint32              unit,
    rtk_port_t          port,
    uint32              mmdAddr,
    uint32              mmdReg,
    uint32              *pData)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, mmdAddr=0x%x, mmdReg=0x%x, reg=0x%x",
           unit, port, mmdAddr, mmdReg);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    //RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pData), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_reg_mmd_get(unit, port, mmdAddr, mmdReg, pData)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pData=0x%x", *pData);

    return ret;
}    /* end of dal_phy_portMmdReg_get */

/* Function Name:
 *      dal_phy_portMmdReg_set
 * Description:
 *      Set PHY MMD register data of the specific port
 * Input:
 *      unit               - unit id
 *      port               - port id
 *      mmdAddr            - mmd device address
 *      mmdReg             - mmd reg id
 *      data               - reg data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portMmdReg_set(
    uint32              unit,
    rtk_port_t          port,
    uint32              mmdAddr,
    uint32              mmdReg,
    uint32              data)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, mmdAddr=0x%x, mmdReg=0x%x \
           data=0x%x", unit, port, mmdAddr, mmdReg, data);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    //RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_reg_mmd_set(unit, port, mmdAddr, mmdReg, data)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return ret;
}    /* end of dal_phy_portMmdReg_set */

/* Function Name:
 *      dal_phy_portmaskMmdReg_set
 * Description:
 *      Set PHY MMD register data of the specific portmask
 * Input:
 *      unit               - unit id
 *      pPortmask          - pointer to the portmask
 *      mmdAddr            - mmd device address
 *      mmdReg             - mmd reg id
 *      data               - reg data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NULL_POINTER  - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portmaskMmdReg_set(
    uint32              unit,
    rtk_portmask_t      *pPortmask,
    uint32              mmdAddr,
    uint32              mmdReg,
    uint32              data)
{
    int32   ret;

    RT_PARAM_CHK((NULL == pPortmask), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK(!HWP_PMSK_EXIST(unit, pPortmask), RT_ERR_PORT_MASK);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, portmask="PMSK_FMT", mmdAddr=0x%x, mmdReg=0x%x \
           data=0x%x", unit, PMSK_ARG(*pPortmask), mmdAddr, mmdReg, data);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pPortmask), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_reg_mmd_portmask_set(unit, *pPortmask, mmdAddr, mmdReg, data)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return ret;
}    /* end of dal_phy_portmaskMmdReg_set */

/* Function Name:
 *      dal_phy_portComboPortMedia_get
 * Description:
 *      Get PHY port media of the specific port
 * Input:
 *      unit   - unit id
 *      port   - port id
 * Output:
 *      pMedia - pointer to the port media
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portComboPortMedia_get(uint32 unit, rtk_port_t port, rtk_port_media_t *pMedia)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pMedia), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get value from CHIP*/
    if ((ret = phy_media_get(unit, port, pMedia)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portComboPortMedia_get */

/* Function Name:
 *      dal_phy_portComboPortMedia_set
 * Description:
 *      Set PHY port media of the specific port
 * Input:
 *      unit  - unit id
 *      port  - port id
 *      media - port media
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      1) You can set these port media which mode PHY currently stays on
 */
int32
dal_phy_portComboPortMedia_set(uint32 unit, rtk_port_t port, rtk_port_media_t media)
{
    int32           ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, media=%d",
           unit, port, media);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((media >= PORT_MEDIA_END), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    /* set value into CHIP*/
    if ((ret = phy_media_set(unit, port, media)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portComboPortMedia_set */

/* Function Name:
 *      dal_phy_portComboPortFiberMedia_get
 * Description:
 *      Get PHY port fiber media of the specific port
 * Input:
 *      unit   - unit id
 *      port   - port id
 * Output:
 *      pMedia - pointer to the port fiber media
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      1. fiber media type value is as following:
 *      - PORT_FIBER_MEDIA_1000
 *      - PORT_FIBER_MEDIA_100
 *      - PORT_FIBER_MEDIA_AUTO
 */
int32
dal_phy_portComboPortFiberMedia_get(uint32 unit, rtk_port_t port, rtk_port_fiber_media_t *pMedia)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d",
           unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pMedia), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get value from CHIP*/
    if ((ret = phy_fiber_media_get(unit, port, pMedia)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pMedia=%d", *pMedia);

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portComboPortFiberMedia_get */

/* Function Name:
 *      dal_phy_portComboPortFiberMedia_set
 * Description:
 *      Set PHY port fiber media of the specific port
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      pMedia - pointer to the port fiber media
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      1. fiber media type value is as following:
 *      - PORT_FIBER_MEDIA_1000
 *      - PORT_FIBER_MEDIA_100
 *      - PORT_FIBER_MEDIA_AUTO
 */
int32
dal_phy_portComboPortFiberMedia_set(uint32 unit, rtk_port_t port, rtk_port_fiber_media_t media)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, media=%d",
           unit, port, media);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((media >= PORT_FIBER_MEDIA_END), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    /* set value from CHIP*/
    if ((ret = phy_fiber_media_set(unit, port, media)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return ret;
} /* end of dal_phy_portComboPortFiberMedia_set */

/* Function Name:
 *      dal_phy_portLinkDownPowerSavingEnable_get
 * Description:
 *      Get the statue of link-down power saving of the specific port in the specific unit
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pEnable - pointer to status of link-down power saving
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portLinkDownPowerSavingEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_linkDownPowerSavingEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portLinkDownPowerSavingEnable_get */

/* Function Name:
 *      dal_phy_portLinkDownPowerSavingEnable_set
 * Description:
 *      Set the statue of link-down power saving of the specific port in the specific unit
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - status of link-down power saving
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portLinkDownPowerSavingEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    /* Configure if PHY supported green feature */
    PHY_SEM_LOCK(unit);
    if ((ret = phy_linkDownPowerSavingEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portLinkDownPowerSavingEnable_set */

/* Function Name:
 *      dal_phy_portGigaLiteEnable_get
 * Description:
 *      Get the statue of Giga Lite the specific port in the specific unit
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pEnable - pointer to status of Giga Lite
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portGigaLiteEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_gigaLiteEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portGigaLiteEnable_get */

/* Function Name:
 *      dal_phy_portGigaLiteEnable_set
 * Description:
 *      Set the statue of Giga Lite of the specific port in the specific unit
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - status of Giga Lite
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portGigaLiteEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    /* Configure if PHY supported giga-lite feature */
    PHY_SEM_LOCK(unit);

    /* Set PHY enable GigaLite ability */
    if ((ret = phy_gigaLiteEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }


    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portGigaLiteEnable_set */


/* Function Name:
 *      dal_phy_port2pt5gLiteEnable_get
 * Description:
 *      Get the statue of 2.5G Lite the specific port in the specific unit
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pEnable - pointer to status of 2.5G Lite
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_port2pt5gLiteEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_2pt5gLiteEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_port2pt5gLiteEnable_set
 * Description:
 *      Set the statue of 2.5G Lite of the specific port in the specific unit
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - status of 2.5G Lite
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_port2pt5gLiteEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    /* Configure if PHY supported giga-lite feature */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_2pt5gLiteEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_phy_portGreenEnable_get
 * Description:
 *      Get the statue of green feature of the specific port in the specific unit
 * Input:
 *      unit - unit id
 *      port - port id
 * Output:
 *      pEnable - pointer to status of green feature
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portGreenEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_greenEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portGreenEnable_get */

/* Function Name:
 *      dal_phy_portGreenEnable_set
 * Description:
 *      Set the status of green feature of the specific port in the specific unit
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - status of green feature
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portGreenEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    /* Configure if PHY supported green feature */
    if ((ret = phy_greenEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portGreenEnable_set */

/* Function Name:
 *      dal_phy_portEeeEnable_get
 * Description:
 *      Get the statue of eee feature of the specific port in the specific unit
 * Input:
 *      unit - unit id
 *      port - port id
 * Output:
 *      pEnable - pointer to status of green feature
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portEeeEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_eeeEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portEeeEnable_get */

/* Function Name:
 *      dal_phy_portEeeEnable_set
 * Description:
 *      Set the status of eee feature of the specific port in the specific unit
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - status of green feature
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portEeeEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    /* Configure if PHY supported eee feature */
    if ((ret = phy_eeeEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portEeeEnable_set */

/* Function Name:
 *      dal_phy_portEeepEnable_get
 * Description:
 *      Get the statue of eeep feature of the specific port in the specific unit
 * Input:
 *      unit - unit id
 *      port - port id
 * Output:
 *      pEnable - pointer to status of green feature
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portEeepEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_eeepEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portEeepEnable_get */

/* Function Name:
 *      dal_phy_portEeepEnable_set
 * Description:
 *      Set the status of eeep feature of the specific port in the specific unit
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - status of green feature
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portEeepEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    /* Configure if PHY supported eeep feature */
    if ((ret = phy_eeepEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portEeepEnable_set */


/* Function Name:
 *      dal_phy_portCrossOverMode_get
 * Description:
 *      Get cross over mode in the specified port.
 * Input:
 *      unit  - unit id
 *      port  - port id
 * Output:
 *      pMode - pointer to cross over mode
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      Following value is valid
 *      - PORT_CROSSOVER_MODE_AUTO
 *      - PORT_CROSSOVER_MODE_MDI
 *      - PORT_CROSSOVER_MODE_MDIX
 */
int32
dal_phy_portCrossOverMode_get(uint32 unit, rtk_port_t port, rtk_port_crossOver_mode_t *pMode)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pMode), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get value from CHIP*/
    if ((ret = phy_crossOverMode_get(unit, port, pMode)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pMode=%d", *pMode);

    return RT_ERR_OK;
}/* end of dal_phy_portCrossOverMode_get */

/* Function Name:
 *      dal_phy_portCrossOverMode_set
 * Description:
 *      Set cross over mode in the specified port.
 * Input:
 *      unit - unit id
 *      port - port id
 *      mode - cross over mode
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID - invalid unit id
 *      RT_ERR_PORT_ID - invalid port id
 *      RT_ERR_INPUT   - invalid input parameter
 * Note:
 *      Following value is valid
 *      - PORT_CROSSOVER_MODE_AUTO
 *      - PORT_CROSSOVER_MODE_MDI
 *      - PORT_CROSSOVER_MODE_MDIX
 */
int32
dal_phy_portCrossOverMode_set(uint32 unit, rtk_port_t port, rtk_port_crossOver_mode_t mode)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, mode=%d",
           unit, port, mode);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK(mode >= PORT_CROSSOVER_MODE_END, RT_ERR_INPUT);


    PHY_SEM_LOCK(unit);

    /* set value from CHIP*/
    if ((ret = phy_crossOverMode_set(unit, port, mode)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}/* end of dal_phy_portCrossOverMode_set */

/* Function Name:
 *      dal_phy_portCrossOverStatus_get
 * Description:
 *      Get cross over status in the specified port.
 * Input:
 *      unit  - unit id
 *      port  - port id
 * Output:
 *      pStatus - pointer to cross over mode status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 *      RT_ERR_PHY_FIBER_LINKUP - This feature is not supported in this mode
 * Note:
 *      Following value is valid
 *      - PORT_CROSSOVER_STATUS_MDI
 *      - PORT_CROSSOVER_STATUS_MDIX
 */
int32
dal_phy_portCrossOverStatus_get(uint32 unit, rtk_port_t port, rtk_port_crossOver_status_t *pStatus)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pStatus), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get value from CHIP*/
    if ((ret = phy_crossOverStatus_get(unit, port, pStatus)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pStatus=%d", *pStatus);

    return RT_ERR_OK;
}/* end of dal_phy_portCrossOverStatus_get */

/* Function Name:
 *      dal_phy_portDownSpeedEnable_get
 * Description:
 *      Get down speed status of the specific port
 * Input:
 *      unit   - unit id
 *      port   - port id
 * Output:
 *      pEnable - down speed status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portDownSpeedEnable_get(uint32 unit, rtk_port_t port,
    rtk_enable_t *pEnable)
{
    int32           ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d",unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    /* get value from CHIP*/
    if ((ret = phy_downSpeedEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "port %d Error Code: 0x%X", port, ret);
        return ret;
    }

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pEnable=%d", *pEnable);

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portDownSpeedEnable_get */

/* Function Name:
 *      dal_phy_portDownSpeedEnable_set
 * Description:
 *      Set down speed status of the specific port
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - down speed status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portDownSpeedEnable_set(uint32 unit, rtk_port_t port,
    rtk_enable_t enable)
{
    int32           ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,enable=%d",unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_ENABLE_END <= enable), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);

    /* set value from CHIP*/
    if ((ret = phy_downSpeedEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "port %d Error Code: 0x%X", port, ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portDownSpeedEnable_set */

/* Function Name:
 *      dal_phy_portDownSpeedStatus_get
 * Description:
 *      Get the statue of down speed status the specific port in the specific unit
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pDownSpeedStatus - pointer to status of down speed.
 *                         TRUE: link is up due to down speed; FALSE: down speed is not performed.
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portDownSpeedStatus_get(uint32 unit, rtk_port_t port, uint32 *pDownSpeedStatus)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pDownSpeedStatus), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_downSpeedStatus_get(unit, port, pDownSpeedStatus)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "port %d Error Code: 0x%X", port, ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portFiberDownSpeedEnable_get
 * Description:
 *      Get fiber down speed status of the specific port
 * Input:
 *      unit   - unit id
 *      port   - port id
 * Output:
 *      pEnable - fiber down speed status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portFiberDownSpeedEnable_get(uint32 unit, rtk_port_t port,
    rtk_enable_t *pEnable)
{
    int32           ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d",unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    /* get value from CHIP*/
    if ((ret = phy_fiberDownSpeedEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "port %d Error Code: 0x%X", port, ret);
        return ret;
    }

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pEnable=%d", *pEnable);

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portFiberDownSpeedEnable_get */

/* Function Name:
 *      dal_phy_portFiberDownSpeedEnable_set
 * Description:
 *      Set fiber down speed status of the specific port
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - fiber down speed status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portFiberDownSpeedEnable_set(uint32 unit, rtk_port_t port,
    rtk_enable_t enable)
{
    int32           ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,enable=%d",unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_ENABLE_END <= enable), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);

    /* set value from CHIP*/
    if ((ret = phy_fiberDownSpeedEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "port %d Error Code: 0x%X", port, ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portFiberDownSpeedEnable_set */


/* Function Name:
 *      dal_phy_portLoopBack_get
 * Description:
 *      Get PHY Loopback featrue
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      pEnable  - ENABLED: Enable loopback;
 *                DISABLED: Disable loopback. PHY back to normal operation.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portLoopBack_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK(!(HWP_PHY_EXIST(unit, port) || HWP_SERDES_PORT(unit, port)), RT_ERR_CHIP_NOT_FOUND);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* set value*/
    if ((ret = phy_loopbackEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit %d port %d Error Code: 0x%X", unit, port, ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pEnable=%d", *pEnable);

    return RT_ERR_OK;
}   /* end of dal_phy_portLoopBack_get */

/* Function Name:
 *      dal_phy_portLoopBack_set
 * Description:
 *      Set PHY Loopback featrue
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable  - ENABLED: Enable loopback;
 *                DISABLED: Disable loopback. PHY back to normal operation.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portLoopBack_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,enable=%d", unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK(!(HWP_PHY_EXIST(unit, port) || HWP_SERDES_PORT(unit, port)), RT_ERR_CHIP_NOT_FOUND);
    RT_PARAM_CHK((RTK_ENABLE_END <= enable), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    /* set value*/
    if ((ret = phy_loopbackEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit %d port %d Error Code: 0x%X", unit, port, ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

#ifndef  __BOOTLOADER__
#if defined(CONFIG_SDK_RTL8295R)
    dal_waMon_8295rClkRecovChk_set(unit, port);
#endif
#endif

    return RT_ERR_OK;
}   /* end of dal_phy_portLoopBack_set */

/* Function Name:
 *      dal_phy_portFiberTxDis_set
 * Description:
 *      Set PHY fiber Tx disable signal
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      enable  - ENABLED: Enable Tx disable signal;
 *                DISABLED: Disable Tx disable signal.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portFiberTxDis_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,enable=%d", unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_ENABLE_END <= enable), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_fiberTxDis_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portFiberTxDis_set */

/* Function Name:
 *      dal_phy_portFiberTxDisPin_set
 * Description:
 *      Set PHY fiber Tx disable signal GPO output
 * Input:
 *      unit        - unit id
 *      port        - port id
 *      data      - GPO pin value
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portFiberTxDisPin_set(uint32 unit, rtk_port_t port, uint32 data)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,data=%d", unit, port, data);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_fiberTxDisPin_set(unit, port, data)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portFiberTxDisPin_set */

/* Function Name:
 *      dal_phy_portExtParkPageReg_get
 * Description:
 *      Get PHY register data of the specific port with extension page and parking page parameters
 * Input:
 *      unit                - unit id
 *      port                - port id
 *      mainPage            - main page id
 *      extPage             - extension page id
 *      parkPage            - parking page id
 *      reg                 - reg id
 * Output:
 *      pData              - pointer to the PHY reg data
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID       - invalid port id
 *      RT_ERR_PHY_PAGE_ID   - invalid page id
 *      RT_ERR_PHY_REG_ID    - invalid reg id
 *      RT_ERR_NULL_POINTER  - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portExtParkPageReg_get(
    uint32              unit,
    rtk_port_t          port,
    uint32              mainPage,
    uint32              extPage,
    uint32              parkPage,
    rtk_port_phy_reg_t  reg,
    uint32              *pData)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, mainPage=0x%x, extPage=0x%x, parkPage=0x%x, reg=0x%x",
           unit, port, mainPage, extPage, parkPage, reg);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pData), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get phy register */
    if ((ret = phy_reg_extParkPage_get(unit, port, mainPage, extPage, parkPage, reg, pData)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pData=0x%x", *pData);

    return ret;
}    /* end of dal_phy_portExtParkPageReg_get */

/* Function Name:
 *      dal_phy_portExtParkPageReg_set
 * Description:
 *      Set PHY register data of the specific port with extension page and parking page parameters
 * Input:
 *      unit               - unit id
 *      port               - port id
 *      mainPage           - main page id
 *      extPage            - extension page id
 *      parkPage           - parking page id
 *      reg                - reg id
 *      data               - reg data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_PHY_PAGE_ID  - invalid page id
 *      RT_ERR_PHY_REG_ID   - invalid reg id
 * Note:
 *      None
 */
int32
dal_phy_portExtParkPageReg_set(
    uint32              unit,
    rtk_port_t          port,
    uint32              mainPage,
    uint32              extPage,
    uint32              parkPage,
    rtk_port_phy_reg_t  reg,
    uint32              data)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, mainPage=0x%x, extPage=0x%x, parkPage=0x%x, reg=0x%x \
           data=0x%x", unit, port, mainPage, extPage, parkPage, reg, data);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_reg_extParkPage_set(unit, port, mainPage, extPage, parkPage, reg, data)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);


    return ret;
}    /* end of dal_phy_portExtParkPageReg_set */

/* Function Name:
 *      dal_phy_portmaskExtParkPageReg_set
 * Description:
 *      Set PHY register data of the specific portmask with extension page and parking page parameters
 * Input:
 *      unit               - unit id
 *      pPortmask          - pointer to the portmask
 *      mainPage           - main page id
 *      extPage            - extension page id
 *      parkPage           - parking page id
 *      reg                - reg id
 *      data               - reg data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_PHY_PAGE_ID  - invalid page id
 *      RT_ERR_PHY_REG_ID   - invalid reg id
 * Note:
 *      None
 */
int32
dal_phy_portmaskExtParkPageReg_set(
    uint32              unit,
    rtk_portmask_t      *pPortmask,
    uint32              mainPage,
    uint32              extPage,
    uint32              parkPage,
    rtk_port_phy_reg_t  reg,
    uint32              data)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pPortmask), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK(!HWP_PMSK_EXIST(unit, pPortmask), RT_ERR_PORT_MASK);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, portmask="PMSK_FMT", mainPage=0x%x, extPage=0x%x, parkPage=0x%x, reg=0x%x \
           data=0x%x", unit, PMSK_ARG(*pPortmask), mainPage, extPage, parkPage, reg, data);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_reg_extParkPage_portmask_set(unit, *pPortmask, mainPage, extPage, parkPage, reg, data)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return ret;
}    /* end of dal_phy_portmaskExtParkPageReg_set */

/* Function Name:
 *      dal_phy_portFiberNwayForceLinkEnable_get
 * Description:
 *      When fiber port is configured N-way,
 *      which can link with link partner is configured force mode.
 * Input:
 *      unit   - unit id
 *      port   - port id
 * Output:
 *      pEnable - fiber Nway force links status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portFiberNwayForceLinkEnable_get(uint32 unit, rtk_port_t port,
    rtk_enable_t *pEnable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d",unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_fiberNwayForceLinkEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_PHY), "port %u", port);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portFiberNwayForceLinkEnable_get */

/* Function Name:
 *      dal_phy_portFiberNwayForceLinkEnable_set
 * Description:
 *      When fiber port is configured N-way,
 *      which can link with link partner is configured force mode.
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - fiber Nway force links status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portFiberNwayForceLinkEnable_set(uint32 unit, rtk_port_t port,
    rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,enable=%d",unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_ENABLE_END <= enable), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_fiberNwayForceLinkEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_PHY), "port %u", port);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portFiberNwayForceLinkEnable_set */

/* Function Name:
 *      dal_phy_portForceModeAbility_get
 * Description:
 *      Get PHY ability status of the specific port
 * Input:
 *      unit                 - unit id
 *      port                 - port id
 * Output:
 *      pSpeed              - pointer to the port speed
 *      pDuplex             - pointer to the port duplex
 *      pFlowControl        - pointer to the flow control enable status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID        - invalid port id
 *      RT_ERR_NULL_POINTER   - input parameter may be null pointer
 *      RT_ERR_TYPE           - invalid media mode of port
 * Note:
 *      None
 */
int32
dal_phy_portForceModeAbility_get(
    uint32              unit,
    rtk_port_t          port,
    rtk_port_speed_t    *pSpeed,
    rtk_port_duplex_t   *pDuplex,
    rtk_enable_t        *pFlowControl)
{
    int32   ret;
    rtk_enable_t    enable;
    rtk_port_phy_ability_t  ability;
    rtk_port_media_t port_mode_sts;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pSpeed), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pDuplex), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pFlowControl), RT_ERR_NULL_POINTER);

    if ((ret = dal_phy_portAutoNegoEnable_get(unit, port, &enable)) != RT_ERR_OK)
    {
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_LOCK(unit);

    if (ENABLED == enable)
    {
        if ((ret = phy_media_get(unit, port, &port_mode_sts)) != RT_ERR_OK)
        {
            PHY_SEM_UNLOCK(unit);
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
            return ret;
        }

        switch (port_mode_sts)
        {
            case PORT_MEDIA_FIBER:
                *pSpeed = pPhy_info[unit]->force_fiber_mode_speed[port];
                break;
            case PORT_MEDIA_COPPER:
                *pSpeed = pPhy_info[unit]->force_mode_speed[port];
                break;
            default:
                PHY_SEM_UNLOCK(unit);
                ret = RT_ERR_TYPE;
                RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
                return ret;
        }
        *pDuplex = pPhy_info[unit]->force_mode_duplex[port];
        *pFlowControl = pPhy_info[unit]->force_mode_flowControl[port];
    }
    else
    {
        if ((ret = phy_speed_get(unit, port, pSpeed)) != RT_ERR_OK)
        {
            PHY_SEM_UNLOCK(unit);
            RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
            return ret;
        }

        if ((ret = phy_duplex_get(unit, port, pDuplex)) != RT_ERR_OK)
        {
            PHY_SEM_UNLOCK(unit);
            RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
            return ret;
        }

        if ((ret = phy_autoNegoAbility_get(unit, port, &ability)) != RT_ERR_OK)
        {
            PHY_SEM_UNLOCK(unit);
            RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
            return ret;
        }

        *pFlowControl = pPhy_info[unit]->force_mode_flowControl[port];
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pSpeed=%d, pDuplex=%d \
           pFlowControl=%d", *pSpeed, *pDuplex, *pFlowControl);

    return RT_ERR_OK;
}/* end of dal_phy_portForceModeAbility_get */

/* Function Name:
 *      dal_phy_portForceModeAbility_set
 * Description:
 *      Set the port speed/duplex mode/pause/asy_pause in the PHY force mode
 * Input:
 *      unit                  - unit id
 *      port                  - port id
 *      speed                 - port speed
 *      duplex                - port duplex mode
 *      flowControl           - enable flow control
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID         - invalid port id
 *      RT_ERR_PHY_SPEED       - invalid PHY speed setting
 *      RT_ERR_PHY_DUPLEX      - invalid PHY duplex setting
 *      RT_ERR_INPUT           - invalid input parameter
 * Note:
 *      1. You can set these abilities no matter which mode PHY currently stays on
 *
 *      2. The speed type of the port is as following:
 *         - PORT_SPEED_10M
 *         - PORT_SPEED_100M
 *
 *      3. The duplex mode of the port is as following:
 *         - HALF_DUPLEX
 *         - FULL_DUPLEX
 */
int32
dal_phy_portForceModeAbility_set(
    uint32              unit,
    rtk_port_t          port,
    rtk_port_speed_t    speed,
    rtk_port_duplex_t   duplex,
    rtk_enable_t        flowControl)
{
    int32   ret;
    rtk_enable_t    enable;
    rtk_port_phy_ability_t ability;
    rtk_port_media_t port_mode_sts;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, speed=%d, duplex=%d \
           flowControl=%d", unit, port, speed, duplex, flowControl);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK(speed >= PORT_SPEED_END, RT_ERR_PHY_SPEED);
    RT_PARAM_CHK((HWP_FE_PORT(unit, port)) && speed == PORT_SPEED_1000M, RT_ERR_CHIP_NOT_SUPPORTED);
    RT_PARAM_CHK((duplex == PORT_HALF_DUPLEX) && (speed == PORT_SPEED_1000M), RT_ERR_CHIP_NOT_SUPPORTED);
    RT_PARAM_CHK(duplex >= PORT_DUPLEX_END, RT_ERR_PHY_DUPLEX);
    RT_PARAM_CHK(flowControl >= RTK_ENABLE_END, RT_ERR_INPUT);

    if ((ret = dal_phy_portAutoNegoEnable_get(unit, port, &enable)) != RT_ERR_OK)
    {
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_LOCK(unit);

    if ((ret = phy_media_get(unit, port, &port_mode_sts)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    if (port_mode_sts == PORT_MEDIA_FIBER)
        pPhy_info[unit]->force_fiber_mode_speed[port] = speed;
    if (port_mode_sts == PORT_MEDIA_COPPER)
        pPhy_info[unit]->force_mode_speed[port] = speed;
    pPhy_info[unit]->force_mode_duplex[port] = duplex;
    pPhy_info[unit]->force_mode_flowControl[port] = flowControl;
    if (DISABLED == enable)
    {
        /*SS-316, forbid 1000H*/
        if(duplex == PORT_HALF_DUPLEX)
        {
            /* set value to CHIP*/
            if ((ret = phy_speed_set(unit, port, speed)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_PORT), "");
                return ret;
            }

            if ((ret = phy_duplex_set(unit, port, duplex)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_PORT), "");
                return ret;
            }
        }
        else
        {
            if ((ret = phy_duplex_set(unit, port, duplex)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_PORT), "");
                return ret;
            }

            /* set value to CHIP*/
            if ((ret = phy_speed_set(unit, port, speed)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_PORT), "");
                return ret;
            }
        }

        if ((ret = phy_autoNegoAbility_get(unit, port, &ability)) != RT_ERR_OK)
        {
            PHY_SEM_UNLOCK(unit);
            RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
            return ret;
        }

        if (ENABLED == flowControl)
        {
            _dal_phy_portForceModePause_get(unit, port, &ability);
        }
        else
        {
            ability.FC = DISABLED;
            ability.AsyFC = DISABLED;
        }

        if ((ret = phy_autoNegoAbility_set(unit, port, &ability)) != RT_ERR_OK)
        {
            PHY_SEM_UNLOCK(unit);
            RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
            return ret;
        }
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}/* end of dal_phy_portForceModeAbility_set */


/* Function Name:
 *      dal_phy_portForceFlowctrlMode_get
 * Description:
 *      Get the port flow control mode in the PHY force mode
 * Input:
 *      unit     - unit id
 *      port     - port id
 * Output:
 *      pMode     - pointer to the port flow control mode
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portForceFlowctrlMode_get(
    uint32                      unit,
    rtk_port_t                  port,
    rtk_port_flowctrl_mode_t    *pMode)
{
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "unit=%d, port=%d",
           unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pMode), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    pMode->rx_pause = pPhy_info[unit]->force_mode_rxPause[port];
    pMode->tx_pause = pPhy_info[unit]->force_mode_txPause[port];

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "rx_pause=%d, tx_pause=%d", pMode->rx_pause, pMode->rx_pause);

    return RT_ERR_OK;
}



/* Function Name:
 *      dal_phy_portForceFlowctrlMode_set
 * Description:
 *      Set the port flow control mode in the PHY force mode
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      pMode     - pointer to the port flow control mode
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *
 */
int32
dal_phy_portForceFlowctrlMode_set(
    uint32                      unit,
    rtk_port_t                  port,
    rtk_port_flowctrl_mode_t    *pMode)
{
    /* Display debug message */
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "unit=%d, port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pMode), RT_ERR_NULL_POINTER);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "rx_pause=%d, tx_pause=%d",
           pMode->rx_pause, pMode->rx_pause);

    PHY_SEM_LOCK(unit);

    pPhy_info[unit]->force_mode_rxPause[port] = pMode->rx_pause;
    pPhy_info[unit]->force_mode_txPause[port] = pMode->tx_pause;

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_phy_portAutoNegoEnablePortmask_set
 * Description:
 *      Set PHY ability of the specific port(s)
 * Input:
 *      unit        - unit id
 *      pPortMask   - list of ports
 *      enable      - enable PHY auto negotiation
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      1) ENABLED : switch to PHY auto negotiation mode
 *         DISABLED: switch to PHY force mode
 *      2) Once the abilities of both auto-nego and force mode are set,
 *         you can freely swtich the mode without calling ability setting API again
 */
int32
dal_phy_portAutoNegoEnablePortmask_set(uint32 unit, rtk_portmask_t *pPortMask,
    rtk_enable_t enable)
{
    rtk_port_phy_ability_t  ability;
    rtk_port_t              port;
    rtk_port_media_t        port_mode_sts;
    int32                   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, enable=%d",
           unit, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pPortMask), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK(!HWP_PMSK_EXIST(unit, pPortMask), RT_ERR_PORT_MASK);
    RT_PARAM_CHK(enable >= RTK_ENABLE_END, RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    HWP_PORT_TRAVS_EXCEPT_CPU(unit, port)
    {
        if (!RTK_PORTMASK_IS_PORT_SET(*pPortMask, port))
            continue;

        if ((ret = phy_autoNegoEnable_set(unit, port, enable)) != RT_ERR_OK)
        {
            PHY_SEM_UNLOCK(unit);
            RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
            return ret;
        }
    }

    /*Delay 100ms*/
    osal_time_usleep(100 * 1000);

    if (ENABLED == enable)
    {
        HWP_PORT_TRAVS_EXCEPT_CPU(unit, port)
        {
            if (!RTK_PORTMASK_IS_PORT_SET(*pPortMask, port))
                continue;

            if ((ret = phy_autoNegoAbility_get(unit, port, &ability)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
                return ret;
            }

            ability.FC = pPhy_info[unit]->auto_mode_pause[port];
            ability.AsyFC = pPhy_info[unit]->auto_mode_asy_pause[port];

            if ((ret = phy_autoNegoAbility_set(unit, port, &ability)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
                return ret;
            }
        }
    }

    if (DISABLED == enable)
    {
        HWP_PORT_TRAVS_EXCEPT_CPU(unit, port)
        {
            if (!RTK_PORTMASK_IS_PORT_SET(*pPortMask,port))
                continue;

            /*SS-316, forbid 1000H*/
            if(pPhy_info[unit]->force_mode_duplex[port] == PORT_FULL_DUPLEX)
            {
                if ((ret = phy_duplex_set(unit, port, pPhy_info[unit]->force_mode_duplex[port])) != RT_ERR_OK)
                {
                    PHY_SEM_UNLOCK(unit);
                    RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
                    return ret;
                }
            }

            if ((ret = phy_media_get(unit, port, &port_mode_sts)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
                return ret;
            }

            if (port_mode_sts == PORT_MEDIA_FIBER)
            {
                if (!(HWP_10GE_PORT(unit, port) && HWP_SERDES_PORT(unit, port)))
                {
                    if ((ret = phy_speed_set(unit, port, pPhy_info[unit]->force_fiber_mode_speed[port])) != RT_ERR_OK)
                    {
                        PHY_SEM_UNLOCK(unit);
                        RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
                        return ret;
                    }
                }
            }

            if (port_mode_sts == PORT_MEDIA_COPPER)
            {
                if ((ret = phy_speed_set(unit, port, pPhy_info[unit]->force_mode_speed[port])) != RT_ERR_OK)
                {
                    PHY_SEM_UNLOCK(unit);
                    RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
                    return ret;
                }
            }

            /*SS-316, forbid 1000H*/
            if(pPhy_info[unit]->force_mode_duplex[port] == PORT_HALF_DUPLEX)
            {
                if ((ret = phy_duplex_set(unit, port, pPhy_info[unit]->force_mode_duplex[port])) != RT_ERR_OK)
                {
                    PHY_SEM_UNLOCK(unit);
                    RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
                    return ret;
                }
            }

            if ((ret = phy_autoNegoAbility_get(unit, port, &ability)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
                return ret;
            }

            if (pPhy_info[unit]->force_mode_flowControl[port] == ENABLED)
            {
                _dal_phy_portForceModePause_get(unit, port, &ability);
            }
            else
            {
                ability.FC = DISABLED;
                ability.AsyFC = DISABLED;
            }

            if ((ret = phy_autoNegoAbility_set(unit, port, &ability)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_PHY), "");
                return ret;
            }
        }
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portAutoNegoEnablePortmask_set */

/* Function Name:
 *      dal_phy_portAutoNegoEnable_set
 * Description:
 *      Set PHY ability of the specific port
 * Input:
 *      unit                 - unit id
 *      port                 - port id
 *      enable               - enable PHY auto negotiation
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID        - invalid port id
 *      RT_ERR_INPUT          - input parameter out of range
 * Note:
 *      1. ENABLED : switch to PHY auto negotiation mode
 *         DISABLED: switch to PHY force mode
 *      2. Once the abilities of both auto-nego and force mode are set,
 *         you can freely swtich the mode without calling ability setting API again
 */
int32
dal_phy_portAutoNegoEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    rtk_portmask_t myPortMask;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    RTK_PORTMASK_RESET(myPortMask);
    RTK_PORTMASK_PORT_SET(myPortMask, port);

    return dal_phy_portAutoNegoEnablePortmask_set(unit, &myPortMask, enable);
} /* end of dal_phy_portAutoNegoEnable_set */

/* Function Name:
 *      dal_phy_portAutoNegoAbilityLocal_get
 * Description:
 *      Get complete abilities for auto negotiation of the specific port
 * Input:
 *      unit    - unit id
 *      port    - port id
 *
 * Output:
 *      pAbility - pointer to PHY auto negotiation ability
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portAutoNegoAbilityLocal_get(uint32 unit, rtk_port_t port, rtk_port_phy_ability_t *pAbility)
{
    int32           ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d",
           unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pAbility), RT_ERR_NULL_POINTER);

    osal_memset(pAbility, 0, sizeof(rtk_port_phy_ability_t));

    PHY_SEM_LOCK(unit);
    if ((ret = phy_autoNegoAbilityLocal_get(unit, port, pAbility)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Half_10=%d, Full_10=%d, \
           Half_100=%d, Full_100=%d, Half_1000=%d, Full_1000=%d, \
           2.5G=%d, 5G=%d, 10G=%d, FC=%d, AsyFC=%d",
           pAbility->Half_10, pAbility->Full_10, pAbility->Half_100, pAbility->Full_100,
           pAbility->Half_1000, pAbility->Full_1000,
           pAbility->adv_2_5G, pAbility->adv_5G, pAbility->adv_10GBase_T,
           pAbility->FC, pAbility->AsyFC);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portAutoNegoAbility_get
 * Description:
 *      Get PHY auto negotiation ability of the specific port
 * Input:
 *      unit                 - unit id
 *      port                 - port id
 * Output:
 *      pAbility            - pointer to the PHY ability
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID        - invalid port id
 *      RT_ERR_NULL_POINTER   - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portAutoNegoAbility_get(
    uint32                  unit,
    rtk_port_t              port,
    rtk_port_phy_ability_t  *pAbility)
{
    rtk_enable_t    enable;
    int32           ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d",
           unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pAbility), RT_ERR_NULL_POINTER);

    if ((ret = dal_phy_portAutoNegoEnable_get(unit, port, &enable)) != RT_ERR_OK)
    {
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    osal_memset(pAbility, 0, sizeof(rtk_port_phy_ability_t));

    PHY_SEM_LOCK(unit);

    if ((ret = phy_autoNegoAbility_get(unit, port, pAbility)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    if (DISABLED == enable && !HWP_SERDES_PORT(unit, port))
    {
        pAbility->FC    = pPhy_info[unit]->auto_mode_pause[port];
        pAbility->AsyFC = pPhy_info[unit]->auto_mode_asy_pause[port];
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Half_10=%d, Full_10=%d, \
           Half_100=%d, Full_100=%d, Half_1000=%d, Full_1000=%d, FC=%d, AsyFC=%d",
           pAbility->Half_10, pAbility->Full_10, pAbility->Half_100, pAbility->Full_100,
           pAbility->Half_1000, pAbility->Full_1000, pAbility->FC, pAbility->AsyFC);

    return RT_ERR_OK;
}/* end of dal_phy_portAutoNegoAbility_get */

/* Function Name:
 *      dal_phy_portAutoNegoAbility_set
 * Description:
 *      Set PHY auto negotiation ability of the specific port
 * Input:
 *      unit               - unit id
 *      port               - port id
 *      pAbility          - pointer to the PHY ability
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      1. You can set these abilities no matter which mode PHY currently stays on
 */
int32
dal_phy_portAutoNegoAbility_set(
    uint32                  unit,
    rtk_port_t              port,
    rtk_port_phy_ability_t  *pAbility)
{
    int32   ret;
    rtk_enable_t    enable;

    /* Display debug message */
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((HWP_FE_PORT(unit, port)) && (pAbility->Full_1000 == ABILITY_BIT_ON), RT_ERR_CHIP_NOT_SUPPORTED);
    RT_PARAM_CHK((NULL == pAbility), RT_ERR_NULL_POINTER);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Half_10=%d, Full_10=%d, Half_100=%d, Full_100=%d, \
           Half_1000=%d, Full_1000=%d, FC=%d, AsyFC=%d",
           pAbility->Half_10, pAbility->Full_10, pAbility->Half_100, pAbility->Full_100,
           pAbility->Half_1000, pAbility->Full_1000, pAbility->FC, pAbility->AsyFC);

    if ((ret = dal_phy_portAutoNegoEnable_get(unit, port, &enable)) != RT_ERR_OK)
    {
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "dal_phy_portAutoNegoEnable_get(unit=%u, port=%u) failed!!",\
               unit, port);
        return ret;
    }

    if (DISABLED == enable && !HWP_SERDES_PORT(unit, port))
    {
        pPhy_info[unit]->auto_mode_pause[port] = pAbility->FC;
        pPhy_info[unit]->auto_mode_asy_pause[port] = pAbility->AsyFC;

        if (pPhy_info[unit]->force_mode_flowControl[port] == ENABLED)
        {
            _dal_phy_portForceModePause_get(unit, port, pAbility);
        }
        else
        {
            pAbility->FC = DISABLED;
            pAbility->AsyFC = DISABLED;
        }
    }

    PHY_SEM_LOCK(unit);

    if ((ret = phy_autoNegoAbility_set(unit, port, pAbility)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    if (ENABLED == enable)
    {
        pPhy_info[unit]->auto_mode_pause[port] = pAbility->FC;
        pPhy_info[unit]->auto_mode_asy_pause[port] = pAbility->AsyFC;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}/* end of dal_phy_portAutoNegoAbility_set */

/* Function Name:
 *      dal_phy_portFiberRxEnable_get
 * Description:
 *      Get fiber Rx enable status of the specific port
 * Input:
 *      unit   - unit id
 *      port   - port id
 * Output:
 *      pEnable - fiber Rx enable status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portFiberRxEnable_get(uint32 unit, rtk_port_t port,
    rtk_enable_t *pEnable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_fiberRxEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portFiberRxEnable_get */

/* Function Name:
 *      dal_phy_portFiberRxEnable_set
 * Description:
 *      Set fiber Rx enable status of the specific port
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - fiber Rx enable status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portFiberRxEnable_set(uint32 unit, rtk_port_t port,
    rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,enable=%d", unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_ENABLE_END <= enable), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_fiberRxEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portFiberRxEnable_set */



/* Function Name:
 *      dal_phy_port10gMedia_get
 * Description:
 *      Get 10G port media of the specific PHY port
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pMedia  - pointer to the media type
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_port10gMedia_get(uint32 unit, rtk_port_t port, rtk_port_10gMedia_t *pMedia)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%u,port=%u", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pMedia), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_10gMedia_get(unit, port, pMedia)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_port10gMedia_set
 * Description:
 *      Set 10G port media of the specific PHY port
 * Input:
 *      unit  - unit id
 *      port  - port id
 *      media - port media
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_port10gMedia_set(uint32 unit, rtk_port_t port, rtk_port_10gMedia_t media)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%u,port=%u,media=%u", unit, port, media);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((media >= PORT_10GMEDIA_END), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_10gMedia_set(unit, port, media)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

#ifndef  __BOOTLOADER__
#if defined(CONFIG_SDK_RTL8295R)
    dal_waMon_8295rClkRecovChk_set(unit, port);
#endif
#endif

    return RT_ERR_OK;

}


/* Function Name:
 *      dal_phy_portIeeeTestMode_set
 * Description:
 *      Set test mode for Giga PHY transmitter test
 * Input:
 *      unit  - unit id
 *      port  - port id
 *      pTestMode->mode  - test mode 1 ~ 4 which is defined in IEEE 40.6.1.1.2
 *      pTestMode->channel  - Channel A, B, C, D, or none
 *      pTestMode->flags -
 *          RTK_PORT_PHYTESTMODE_FLAG_ALL_PHY_PORTS -
 *              apply the test on all ports of the PHY.
 *              To use this feature, the "port" parameter shall set to the first port of the PHY.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_NOT_ALLOWED - The operation is not allowed
 *      RT_ERR_PORT_NOT_SUPPORTED - test mode is not supported
 * Note:
 *      None
 */
int32
dal_phy_portIeeeTestMode_set(uint32 unit, rtk_port_t port, rtk_port_phyTestMode_t *pTestMode)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%u, port=%u, mode=%u channel=%d flags=%x", unit, port, pTestMode->mode, pTestMode->channel, pTestMode->flags);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    RT_PARAM_CHK((port > RTK_MAX_NUM_OF_PORTS), RT_ERR_PORT_ID);
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((pTestMode->channel >= PHY_TESTMODE_CHANNEL_END), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);

    ret = phy_ieeeTestMode_set(unit, port, pTestMode);

    PHY_SEM_UNLOCK(unit);

    return ret;

}

/* Function Name:
 *      dal_phy_polar_get
 * Description:
 *      Get 10GE port polarity configure
 * Input:
 *      unit   - unit id
 *      port   - port id
 * Output:
 *      pPolarCtrl - polarity configuration
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_polar_get(uint32 unit, rtk_port_t port, rtk_port_phyPolarCtrl_t *pPolarCtrl)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    RT_PARAM_CHK((port > RTK_MAX_NUM_OF_PORTS), RT_ERR_PORT_ID);
    RT_PARAM_CHK(!(HWP_PHY_EXIST(unit, port) || HWP_SERDES_PORT(unit, port)), RT_ERR_CHIP_NOT_FOUND);


    /* function body */
    PHY_SEM_LOCK(unit);

    ret = phy_polar_get(unit, port, pPolarCtrl);

    PHY_SEM_UNLOCK(unit);

    return ret;

}

/* Function Name:
 *      dal_phy_polar_set
 * Description:
 *      Configure 10GE port polarity
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      pPolarCtrl - polarity configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_polar_set(uint32 unit, rtk_port_t port, rtk_port_phyPolarCtrl_t *pPolarCtrl)
{

    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%u, port=%u, polar tx:%d rx:%d", unit, port, pPolarCtrl->phy_polar_tx, pPolarCtrl->phy_polar_rx);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    RT_PARAM_CHK((port > RTK_MAX_NUM_OF_PORTS), RT_ERR_PORT_ID);
    RT_PARAM_CHK(!(HWP_PHY_EXIST(unit, port) || HWP_SERDES_PORT(unit, port)), RT_ERR_CHIP_NOT_FOUND);
    RT_PARAM_CHK((pPolarCtrl->phy_polar_tx >= PHY_POLARITY_END), RT_ERR_INPUT);
    RT_PARAM_CHK((pPolarCtrl->phy_polar_rx >= PHY_POLARITY_END), RT_ERR_INPUT);


    /* function body */
    PHY_SEM_LOCK(unit);

    ret = phy_polar_set(unit, port, pPolarCtrl);

    PHY_SEM_UNLOCK(unit);

    return ret;

}

#if defined(CONFIG_SDK_WA_REMOTE_FAULT)
/* Function Name:
 *      dal_phy_fiberRemoteFault_watchdog
 * Description:
 *      Fiber remote fault handle
 * Input:
 *      None.
 * Output:
 *      None
 * Return:
 *      RT_ERR_FAILED - initialize fail
 *      RT_ERR_OK     - initialize success
 * Note:
 *
 */
int32 dal_phy_fiberRemoteFault_watchdog(uint32 unit)
{
    uint32  port_index, sdsID;

    HWP_SDS_TRAVS(unit, sdsID)
    {
        port_index = HWP_SDS_ID2MACID(unit, sdsID);

        if(HWP_NONE == port_index)
            continue;

        if(HWP_10GE_SERDES_PORT(unit, port_index))
        {
            phy_fiberRemoteFault_handle(unit, port_index);
        }
    }

    return RT_ERR_OK;
}

#endif /* CONFIG_SDK_WA_REMOTE_FAULT */

#if defined(CONFIG_SDK_WA_FIBER_RX_WATCHDOG)
/* Function Name:
 *      dal_phy_fiberRx_watchdog
 * Description:
 *      check fiber RX is normal or not
 * Input:
 *      None.
 * Output:
 *      None
 * Return:
 *      RT_ERR_FAILED - initialize fail
 *      RT_ERR_OK     - initialize success
 * Note:
 *
 */
uint32 fiberRxOrder[RTK_MAX_NUM_OF_UNIT] = {0};
uint32 fiberRxOrder10GPort[RTK_MAX_NUM_OF_UNIT] = {0};
uint8 fiberRxWDSts[RTK_MAX_NUM_OF_UNIT][RTK_MAX_PORT_PER_UNIT];
#define FIBER_RX_WATCHDOG_CHK_PORT_NUM  1

int32 dal_phy_fiberRx_watchdog(uint32 unit)
{
    uint32  port_num = 0;
    uint32  port_index;
    int32   phyModel;
    uint32  chk_error, reg_data;
    rtk_port_media_t portMedia;
    uint32  clear_flag, reset_flag;
    int32   chk_flag;

    clear_flag = 1;
    reset_flag = 0;

    chk_flag = 0;

    port_num = RTK_MAX_PORT_PER_UNIT;

    if (fiberRxOrder10GPort[unit] >= port_num)
        fiberRxOrder10GPort[unit] = 0;

    for(port_index = fiberRxOrder10GPort[unit]; (port_index < port_num); port_index++)
    {
        if (!HWP_PORT_EXIST(unit, port_index))
            continue;

        if(HWP_10GE_SERDES_PORT(unit, port_index))
        {
            ++chk_flag;
            chk_error = 0;
            phy_serdesFiberRx_check(unit, port_index, &chk_error);
            if(1 == chk_error)
                reset_flag = 1;
            if (2 == chk_error)
            {
                if (1 == fiberRxWDSts[unit][port_index])
                {
                    reset_flag = 1;
                }
                else
                {
                    fiberRxWDSts[unit][port_index] = 1;
                    clear_flag = 0;
                }
            }

            if (1 == reset_flag)
            {
                fiber_rx_watchdog_cnt[unit]++;
                phy_serdesFiberRx_reset(unit, port_index);
                reset_flag = 0;
            }

            if ((1 == clear_flag) || (1 == reset_flag))
            {
                fiberRxWDSts[unit][port_index] = 0;
            }
            continue;
        }

#if defined(CONFIG_SDK_RTL8295R)
        if ((HWP_PHY_MODEL_BY_PORT(unit, port_index) == RTK_PHYTYPE_RTL8295R) || (HWP_PHY_MODEL_BY_PORT(unit, port_index) == RTK_PHYTYPE_RTL8295R_C22))
        {
            ++chk_flag;
            phy_fiberRx_check(unit, port_index, &chk_error);
            if(1 == chk_error)
            {
                reset_flag = 1;
            }

            if (1 == reset_flag)
            {
                fiber_rx_watchdog_cnt[unit]++;
                phy_fiberRx_reset(unit, port_index);
                reset_flag = 0;
                port_index++;
                break;
            }

            continue;
        }
#endif
    }

    /*Back up last time 10G port_index*/
    fiberRxOrder10GPort[unit] = port_index;

    chk_flag = 0;

    if (fiberRxOrder[unit] >= port_num)
        fiberRxOrder[unit] = 0;

    /* Scan from last fiber port index, per-time chk_flag ports number */
    for(port_index = fiberRxOrder[unit]; (port_index < port_num) && (chk_flag < FIBER_RX_WATCHDOG_CHK_PORT_NUM); port_index++)
    {
        if (!HWP_PORT_EXIST(unit, port_index))
            continue;

        clear_flag = 1;
        reset_flag = 0;
        chk_error = 0;

        if ((HWP_COMBO_PORT(unit, port_index) || HWP_FIBER_PORT(unit, port_index)))
        {
            phyModel = HWP_PHY_MODEL_BY_PORT(unit, port_index);
            if ((phyModel == RTK_PHYTYPE_RTL8214FC) ||
                    (phyModel == RTK_PHYTYPE_RTL8218FB) ||
                    (phyModel == RTK_PHYTYPE_RTL8214QF) || (phyModel == RTK_PHYTYPE_RTL8214QF_NC5) ||
                    (phyModel == RTK_PHYTYPE_RTL8224QF))
            {
                ++chk_flag;

                phy_media_get(unit, port_index, &portMedia);
                if(portMedia == PORT_MEDIA_FIBER) /*Checking Media is Fiber*/
                {
                    /* only work for giga */
                    phy_reg_get(unit, port_index, 0, 0, &reg_data);
                    if ((0 == ((reg_data >> 13) & 0x1)) && (1 == ((reg_data >> 6) & 0x1)))
                    {
                        /*Checking Port is Link Down*/
                        phy_reg_get(unit, port_index, 0, 1, &reg_data);
                        phy_reg_get(unit, port_index, 0, 1, &reg_data);
                        if((reg_data & 0x4) == 0)
                        {
                            phy_fiberRx_check(unit, port_index, &chk_error);
                            if(1 == chk_error)
                                reset_flag = 1;
                            if (2 == chk_error)
                            {
                                if (1 == fiberRxWDSts[unit][port_index])
                                {
                                    reset_flag = 1;
                                }
                                else
                                {
                                    fiberRxWDSts[unit][port_index] = 1;
                                    clear_flag = 0;
                                }
                            }
                        }
                    }
                }

               /*Reset RX*/
               if (1 == reset_flag)
               {
                   fiber_rx_watchdog_cnt[unit]++;
                   phy_fiberRx_reset(unit, port_index);
               }

               if ((1 == clear_flag) || (1 == reset_flag))
               {
                   fiberRxWDSts[unit][port_index] = 0;
               }

            }
        }

        if(HWP_SERDES_PORT(unit, port_index))
        {
            if(HWP_10GE_PORT(unit, port_index))
                continue;

            ++chk_flag;
            chk_error = 0;
            phy_serdesFiberRx_check(unit, port_index, &chk_error);
            if(1 == chk_error)
                reset_flag = 1;
            if (2 == chk_error)
            {
                if (1 == fiberRxWDSts[unit][port_index])
                {
                    reset_flag = 1;
                }
                else
                {
                    fiberRxWDSts[unit][port_index] = 1;
                    clear_flag = 0;
                }
            }

            if (1 == reset_flag)
            {
                fiber_rx_watchdog_cnt[unit]++;
                phy_serdesFiberRx_reset(unit, port_index);
            }

            if ((1 == clear_flag) || (1 == reset_flag))
            {
                fiberRxWDSts[unit][port_index] = 0;
            }
        }
    }

    /*Back up last time port_index*/
    fiberRxOrder[unit] = port_index;

    return RT_ERR_OK;
}

#endif /* CONFIG_SDK_WA_FIBER_RX_WATCHDOG */

#if defined(CONFIG_SDK_WA_PHY_WATCHDOG)
/* Function Name:
 *      _dal_phy_serdesLinkDown_handler
 * Description:
 *      Monitor phy serdes link status.
 * Input:
 *      unit - unit id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID - invalid unit id
 * Note:
 *       The API is monitor serdes link down and recover it.
 */
int32
_dal_phy_serdesLinkDown_handler(uint32 unit)
{
    uint32              scanPhyIdx;
    phy_type_t       phyModel;
    uint32              link_down_flag;
    rtk_port_t         port;
    int32                ret;

    HWP_PHY_TRAVS(unit,scanPhyIdx)
    {
        phyModel = HWP_PHY_MODEL_BY_PHY(unit, scanPhyIdx);
        if((RTK_PHYTYPE_RTL8218D != phyModel) && (RTK_PHYTYPE_RTL8218D_NMP != phyModel) && (RTK_PHYTYPE_RTL8214FC != phyModel) && (RTK_PHYTYPE_RTL8218E != phyModel))
        {
            continue;
        }

        link_down_flag = 0;

        port = HWP_PHY_BASE_MACID_BY_IDX(unit, scanPhyIdx);
        if((ret = phy_serdes_linkdown_check(unit, port, &link_down_flag)) != RT_ERR_OK)
        {
            return ret;
        }

        if (0x1 == link_down_flag)
        {
            phySerdes_rst_flag[unit][scanPhyIdx]++;

            if(5 == phySerdes_rst_flag[unit][scanPhyIdx])
            {
                phySerdes_watchdog_cnt[unit]++;
                if(phySerdes_watchdog_cnt[unit] >= 64)
                    phySerdes_watchdog_cnt[unit] = 0;

                phy_serdes_rst(unit,port);

                phySerdes_rst_flag[unit][scanPhyIdx] = 0;
            }
        }
        else
        {
            phySerdes_rst_flag[unit][scanPhyIdx] = 0;
        }
    }
    return RT_ERR_OK;
}

static uint8    fiberRstSts[RTK_MAX_NUM_OF_UNIT][RTK_MAX_PORT_PER_UNIT];

/* Function Name:
 *      _dal_phy_fiberLinkUp_handler1
 * Description:
 *      Handle fiber link up
 * Input:
 *      unit - unit id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID - invalid unit id
 * Note:
 *      To do fiber handle when port link up. [SS-47]
 */
static int32 _dal_phy_fiberLinkUp_handler1(uint32 unit)
{
    rtk_port_t          port;
    rtk_port_media_t    portMedia;
    uint32              data = 0, speed;
    #ifdef CONFIG_SDK_RTL8380
    uint32              val;
    #endif
    uint32              phyModel;
    int32               ret;

    HWP_PORT_TRAVS_EXCEPT_CPU(unit, port)
    {
        /* Port check */
        if (!HWP_PORT_EXIST(unit, port))
            continue;

        if (!HWP_PHY_EXIST(unit, port) && !HWP_SERDES_PORT(unit, port))
            continue;

        if (fiberRstSts[unit][port] != 0)
            continue;

        phyModel = HWP_PHY_MODEL_BY_PORT(unit, port);
        if ((phyModel == RTK_PHYTYPE_RTL8214FC) ||
                (phyModel == RTK_PHYTYPE_RTL8218FB))
        {
            PHY_SEM_LOCK(unit);

            if ((ret = phy_media_get(unit, port, &portMedia)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Port:%d failed!\n", port);
                continue;
            }

            if (PORT_MEDIA_FIBER != portMedia)
            {
                PHY_SEM_UNLOCK(unit);
                RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Port:%d failed!\n", port);
                continue;
            }

            if ((ret = phy_reg_get(unit, port, PHY_PAGE_0, PHY_STATUS_REG, &data)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Port:%d failed!\n", port);
                continue;
            }

            if ((ret = phy_reg_get(unit, port, PHY_PAGE_0, PHY_STATUS_REG, &data)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Port:%d failed!\n", port);
                continue;
            }

            data &= LinkStatus_MASK;
            if (0 != data && fiberRstSts[unit][port] != data)
            {
                if ((ret = phy_speed_get(unit, port, &speed)) != RT_ERR_OK)
                {
                    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Port:%d speed failed!\n", port);
                }

                if (PORT_SPEED_100M == speed)
                {
                    phy_fiberLinkUp_handler1(unit, port);
                    phy_watchdog_cnt[unit]++;
                    fiberRstSts[unit][port] = data;
                }
                else
                {
                    fiberRstSts[unit][port] = 0;
                }
            }
            else
            {
                fiberRstSts[unit][port] = data;
            }

            PHY_SEM_UNLOCK(unit);
        }
        #ifdef CONFIG_SDK_RTL8380
        else if (HWP_8380_30_FAMILY(unit) && HWP_SERDES_PORT(unit, port))
        {
            uint32  reg_addr_link;

            PHY_SEM_LOCK(unit);

            /*Get current media type*/
            phy_media_get(unit, port, &portMedia);

            if (PORT_MEDIA_FIBER == portMedia)
            {
                /*Get link status */
                if(port == 24)
                {
                    reg_addr_link = MAPLE_SDS4_FIB_REG1r;
                }
                else if(port == 26)
                {
                    reg_addr_link = MAPLE_SDS5_FIB_REG1r;
                }
                else
                {
                    reg_addr_link = 0;
                }

                reg_read(unit, reg_addr_link, &data);
                reg_read(unit, reg_addr_link, &data);

                data &= 0x4;
                if (0 != data && fiberRstSts[unit][port] != data)
                {
                    if ((ret = reg_array_field_read(unit, MAPLE_MAC_LINK_SPD_STSr,
                              port, REG_ARRAY_INDEX_NONE, MAPLE_SPD_STS_27_0f,
                              &val)) != RT_ERR_OK)
                    {
                        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Port:%d speed failed!\n", port);
                    }

                    if (1 == val)
                    {
                        phy_fiberLinkUp_handler1(unit, port);
                        phy_watchdog_cnt[unit]++;
                        fiberRstSts[unit][port] = data;
                    }
                    else
                    {
                        fiberRstSts[unit][port] = 0;
                    }
                }
                else
                {
                    fiberRstSts[unit][port] = data;
                }
            }

            PHY_SEM_UNLOCK(unit);
        }
        #endif  /* CONFIG_SDK_RTL8380 */
    }

    return RT_ERR_OK;
}   /* end of _dal_phy_fiberLinkUp_handler1 */

/* Function Name:
 *      dal_phy_watchdog
 * Description:
 *      Monitor for phy problem.
 * Input:
 *      None.
 * Output:
 *      None
 * Return:
 *      RT_ERR_FAILED - initialize fail
 *      RT_ERR_OK     - initialize success
 * Note:
 *      The API is monitor for phy problem and patch it.
 */
int32 dal_phy_handler(uint32 unit)
{
    int32   ret;

    RT_ERR_CHK(_dal_phy_fiberLinkUp_handler1(unit), ret);

    RT_ERR_CHK(_dal_phy_serdesLinkDown_handler(unit), ret);

    RT_ERR_CHK(dal_phy_cableESD_watchdog(unit), ret);

    return RT_ERR_OK;
}


static uint32   esdPhyLast[RTK_MAX_NUM_OF_UNIT] = { -1 };
static uint8    esdPhyNumPerScan = 1;

/* Function Name:
 *      dal_phy_cableESDPhyIdNext_get
 * Description:
 *      Get PHY that need to be processed for Cable ESD problem
 * Input:
 *      unit - unit id
 *      phyIdx - current PHY index. Set it to -1 would get from first PHY index.
 *      wrapAround - TRUE of FALSE. whether to return first PHY index or not when last PHY index is reached.
 * Output:
 *      nextPhyIdx - next PHY index that need to be processed for Cable ESD problem
 * Return:
 *      RT_ERR_OK - nextPhyIdx is available
 *      RT_ERR_ENTRY_NOTFOUND - nextPhyIdx is not available
 * Note:
 *      None.
 */
int32
dal_phy_cableESDPhyIdNext_get(uint32 unit, int32 phyIdx, int32 *nextPhyIdx, uint32 wrapAround)
{
    int32       idx, i;
    uint32      phyModel;
    uint32      port;
    int32       phyCnt;

    if ((phyCnt = HWP_PHY_COUNT(unit)) == 0)
    {
        return RT_ERR_ENTRY_NOTFOUND;
    }

    for (i = 0; i < phyCnt; i++)
    {
        idx = (phyIdx + 1 + i) % phyCnt;
        phyModel = HWP_PHY_MODEL_BY_PHY(unit, idx);
        if (!((phyModel == RTK_PHYTYPE_RTL8218B) ||
              (phyModel == RTK_PHYTYPE_RTL8218D) || (phyModel == RTK_PHYTYPE_RTL8218D_NMP) ||
              (phyModel == RTK_PHYTYPE_RTL8214FC) ||
              (phyModel == RTK_PHYTYPE_RTL8214C) ||
              (phyModel == RTK_PHYTYPE_RTL8218FB) ||
              (phyModel == RTK_PHYTYPE_RTL8218E) ))
        {
            continue;
        }

        for(port = HWP_PHY_BASE_MACID_BY_IDX(unit, idx);
            port < (port + HWP_PHY_BASE_PHY_MAX(unit, port));
            port++)
        {
            if ((HWP_COPPER_PORT(unit, port)) || (HWP_COMBO_PORT(unit, port)))
            {
                if ((wrapAround == FALSE) && (idx <= phyIdx))
                {
                    return RT_ERR_ENTRY_NOTFOUND;
                }

                *nextPhyIdx = idx;
                return RT_ERR_OK;
            }
        }/* end for */
    }/* end for */

    return RT_ERR_ENTRY_NOTFOUND;
}


/* Function Name:
 *      dal_phy_cableESD_watchdog
 * Description:
 *      Cable ESD problem.
 * Input:
 *      unit - unit id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID - invalid unit id
 * Note:
 *      The API is monitor for Cable ESD probem and patch it.
 *      Protect PHY page by port semaphore.
 */
int32
dal_phy_cableESD_watchdog(uint32 unit)
{
    int32           ret = RT_ERR_OK;
    int32           scanPhyIdx, scanCnt = 0, startIdx = -1;
    uint32          phy_rst_flag;
    uint32          port_rst_flag = 0;
    rtk_portmask_t  rst_portmask;
    rtk_port_t      port,maxPort;
    uint32          sdsId,maxSdsId;

    scanPhyIdx = esdPhyLast[unit];
    while (scanCnt < esdPhyNumPerScan)
    {
        if (dal_phy_cableESDPhyIdNext_get(unit, scanPhyIdx, &scanPhyIdx, TRUE) != RT_ERR_OK)
        {
            /* if there is no chips need to do cableESD, return ok */
            return RT_ERR_OK;
        }
        /* check duplicates */
        if (startIdx == -1)
        {
            startIdx = scanPhyIdx;
        }
        else if (startIdx == scanPhyIdx)
        {
            /* when the chip number that need to do cableESD is less than esdPhyNumPerScan, don't duplicate scan the chip in this loop. */
            break;
        }

        /* setup scaned phy index */
        esdPhyLast[unit] = scanPhyIdx;
        scanCnt++;

        /* cableESD process */
        phy_rst_flag = 0;
        port_rst_flag = 0;
        osal_memset(&rst_portmask, 0, sizeof(rst_portmask));

        port = HWP_PHY_BASE_MACID_BY_IDX(unit, scanPhyIdx);
        ret = phy_cableESD_recover(unit, port, &phy_rst_flag, &port_rst_flag, &rst_portmask);
        if (ret != RT_ERR_OK)
        {
            return ret;
        }

        if ((phy_rst_flag != 0x0) || (port_rst_flag != 0x0))
        {
            phy_watchdog_cnt[unit]++;
            if(phy_watchdog_cnt[unit] >= 64)
                phy_watchdog_cnt[unit] = 0;

            osal_time_udelay(1000 * 1000);

            if (0x1 == phy_rst_flag)
            {
                sdsId = HWP_PORT_SDSID(unit,port);

                maxPort = port + HWP_PHY_MAX_PORT_NUM_BY_IDX(unit,scanPhyIdx) - 1;
                maxSdsId = HWP_PORT_SDSID(unit,maxPort);

                hal_mac_serdes_rst(unit, sdsId);

                if (sdsId  != maxSdsId)
                {
                    hal_mac_serdes_rst(unit, maxSdsId);
                }

                dal_waMon_phyReconfig_portMaskSet(unit, port);
                dal_waMon_phyEsdRstEvn_set(unit, port);
            }
            else if (0x2 == phy_rst_flag)
            {
                HWP_SDS_TRAVS(unit, sdsId)
                {
                    if((RTK_PHYTYPE_NONE != HWP_SDS_ID2PHYMODEL(unit, sdsId)) &&
                        (RTK_PHYTYPE_SERDES != HWP_SDS_ID2PHYMODEL(unit, sdsId)))
                    {
                        hal_mac_serdes_rst(unit, sdsId);
                        port = HWP_PHY_BASE_MACID_BY_IDX(unit, scanPhyIdx);
                        dal_waMon_phyReconfig_portMaskSet(unit, port);
                        dal_waMon_phyEsdRstEvn_set(unit, port);
                    }
                }
            }
            else if (port_rst_flag)
            {
                dal_waMon_phyReconfig_perPort_portMaskSet(unit, port, &rst_portmask);
            }
        }
    }/* end while */

    return RT_ERR_OK;
}

#endif  /* CONFIG_SDK_WA_PHY_WATCHDOG */

/* Function Name:
 *      dal_phy_portLinkChange_process
 * Description:
 *      Link change process of PHY
 * Input:
 *      unit - unit id
 *      port - port id
 *      linkSts - new link state
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_portLinkChange_process(int32 unit, rtk_port_t port, rtk_port_linkStatus_t linkSts)
{
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%u link:%s", unit, port, (linkSts==PORT_LINKUP)?"Up":"Down");

    PHY_SEM_LOCK(unit);

#if defined(CONFIG_SDK_RTL8295R)
    if ((HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_RTL8295R) || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_RTL8295R_C22))
    {
      #if defined(CONFIG_SDK_RTL8390)
        if (HWP_8390_50_FAMILY(unit))
        {
            int32       ret;
            uint32      mac_force_en, link_en = 0;
            /* for 95R disabled link pass through */
            if ((ret = reg_array_field_write(unit, CYPRESS_MAC_FORCE_MODE_CTRLr, port, REG_ARRAY_INDEX_NONE, CYPRESS_FORCE_LINK_ENf, &link_en)) != RT_ERR_OK)
            {
                RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%u link:%s FRC_LINK 0 fail", unit, port, (linkSts==PORT_LINKUP)?"Up":"Down");
            }

            mac_force_en = (linkSts == PORT_LINKDOWN) ? 1 : 0; /* when link down, enable mac force mode and set force link as down;
                                                                * when link up, disable mac force mode, the force link register will not be referenced, MAC will auto detect link. */
            if ((ret = reg_array_field_write(unit, CYPRESS_MAC_FORCE_MODE_CTRLr, port, REG_ARRAY_INDEX_NONE, CYPRESS_MAC_FORCE_ENf, &mac_force_en)) != RT_ERR_OK)
            {
                RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%u link:%s FRC_EN %u fail", unit, port, (linkSts==PORT_LINKUP)?"Up":"Down", mac_force_en);
            }
        }
      #endif /* defined(CONFIG_SDK_RTL8390) */

        /* 95R shall be processed before MAC link change process */
        phy_8295r_linkChange_process(unit, port, linkSts);

        PHY_SEM_UNLOCK(unit);
        /* port is processed, return */
        return RT_ERR_OK;
    }
#endif /* CONFIG_SDK_RTL8295R */

#if defined(CONFIG_SDK_RTL8390)
    if (HWP_8390_FAMILY_ID(unit))
    {
        phy_8390_linkChange_process(unit, port, linkSts);
    }
#endif/* CONFIG_SDK_RTL8390 */

#if defined(CONFIG_SDK_RTL9300)
    if(HWP_9300_FAMILY_ID(unit))
    {
        phy_9300_linkChange_process(unit, port, linkSts);
    }
#endif/* CONFIG_SDK_RTL9300 */

#if defined(CONFIG_SDK_RTL9310)
    if(HWP_9310_FAMILY_ID(unit))
    {
        phy_9310_linkChange_process(unit, port, linkSts);
    }
#endif/* CONFIG_SDK_RTL9310 */

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;

}

/* Function Name:
 *      dal_phy_portEyeMonitor_start
 * Description:
 *      Trigger eye monitor function
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      sdsId   - serdes id or 0 for serdes port or the PHY has no serdes id.
 *      frameNum- frame number
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portEyeMonitor_start(uint32 unit, rtk_port_t port, uint32 sdsId, uint32 frameNum)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,sdsId=%d,frameNum=%d", unit, port, sdsId, frameNum);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_portEyeMonitor_start(unit, port, sdsId, frameNum)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portEyeMonitor_start */

/* Function Name:
 *      dal_phy_portEyeMonitorInfo_get
 * Description:
 *      Get the statue of eye monitor height and width the specific port in the specific unit
 * Input:
 *      unit    - unit id
 *      port    - base port id of the PHY
 *      sds     - SerDes id of the PHY
 *      frameNum- frame number
 * Output:
 *      pInfo - pointer to the information of eye monitor height and width
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_PORT_NOT_SUPPORTED   - This function is not supported by the PHY of this port
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 *      RT_ERR_BUSYWAIT_TIMEOUT - Read information timeout
 * Note:
 *      None
 */
int32
dal_phy_portEyeMonitorInfo_get(uint32 unit, rtk_port_t port, uint32 sds, uint32 frameNum, rtk_sds_eyeMonInfo_t *pInfo)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,sdsId=%d,frameNum=%d", unit, port, sds, frameNum);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_eyeMonitorInfo_get(unit, port, sds, frameNum, pInfo)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portImageFlash_load
 * Description:
 *      load image into flash
 * Input:
 *      unit - unit id
 *      port - port id
 *      size - image size
 *      image - image
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_portFlashImage_load(uint32 unit, rtk_port_t port, uint32 size, uint8 *image)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, size=%d", unit, port, size);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_portFlashImage_load(unit, port, size, image)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portEyeMonitor_start */

/* Function Name:
 *      dal_phy_portSds_get
 * Description:
 *      Get PHY SerDes
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      sdsCfg  - SerDes configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portSds_get(uint32 unit, rtk_port_t port, rtk_sdsCfg_t *sdsCfg)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == sdsCfg), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_sds_get(unit, port, sdsCfg)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portSds_get */

/* Function Name:
 *      dal_phy_portSds_set
 * Description:
 *      Configure PHY SerDes
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      sdsCfg  - SerDes configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portSds_set(uint32 unit, rtk_port_t port, rtk_sdsCfg_t *sdsCfg)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == sdsCfg), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_sds_set(unit, port, sdsCfg)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portSds_set */


/* Function Name:
 *      dal_phy_sdsRxCaliStatus_get
 * Description:
 *      Get PHY SerDes rx-calibration status
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      sdsId   - serdes ID
 * Output:
 *      rtk_port_phySdsRxCaliStatus_t   - rx-calibration status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_UNIT_ID - invalid unit id
 *      RT_ERR_OUT_OF_RANGE - invalid serdes id
 * Note:
 *      None
 */
int32
dal_phy_sdsRxCaliStatus_get(uint32 unit, rtk_port_t port, uint32 sdsId, rtk_port_phySdsRxCaliStatus_t *pStatus)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pStatus), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_sdsRxCaliStatus_get(unit, port, sdsId, pStatus)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d phy_sdsRxCaliStatus_get ret 0x%x", unit, port, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portEnable_get
 * Description:
 *      Get PHY interface status of the specific port
 * Input:
 *      unit          - unit id
 *      port          - port id
 * Output:
 *      pEnable       - pointer to admin configuration of PHY interface
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int32
dal_phy_portEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_enable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portEnable_set
 * Description:
 *      Set PHY interface status of the specific port
 * Input:
 *      unit          - unit id
 *      port          - port id
 *      enable        - admin configuration of PHY interface
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int32
dal_phy_portEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,enable=%d", unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    PHY_SEM_LOCK(unit);

    ret = phy_enable_set(unit, port, enable);

    PHY_SEM_UNLOCK(unit);

#ifndef  __BOOTLOADER__
#if defined(CONFIG_SDK_RTL8295R)
    dal_waMon_8295rClkRecovChk_set(unit, port);
#endif
#endif

    return ret;
}

/* Function Name:
 *      dal_phy_portSpeed_get
 * Description:
 *      Get link speed status of the specific port
 * Input:
 *      unit - unit id
 *      port - port id
 * Output:
 *      pSpeed - pointer to PHY link speed
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int32
dal_phy_portSpeed_get(uint32 unit, rtk_port_t port, rtk_port_speed_t *pSpeed)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    PHY_SEM_LOCK(unit);

    ret = phy_speed_get(unit, port, pSpeed);

    PHY_SEM_UNLOCK(unit);

    return ret;
}

/* Function Name:
 *      dal_phy_portSpeed_set
 * Description:
 *      Set speed mode status of the specific port
 * Input:
 *      unit          - unit id
 *      port          - port id
 *      speed         - link speed status 10/100/1000
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int32
dal_phy_portSpeed_set(uint32 unit, rtk_port_t port, rtk_port_speed_t speed)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, speed=%d", unit, port, speed);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    PHY_SEM_LOCK(unit);

    ret = phy_speed_set(unit, port, speed);

    PHY_SEM_UNLOCK(unit);

    return ret;
}


/* Function Name:
 *      dal_phy_portDuplex_get
 * Description:
 *      Get duplex mode status of the specific port
 * Input:
 *      unit - unit id
 *      port - port id
 * Output:
 *      pDuplex - pointer to PHY duplex mode status
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int32
dal_phy_portDuplex_get(uint32 unit, rtk_port_t port, rtk_port_duplex_t *pDuplex)
{
    int32           ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    PHY_SEM_LOCK(unit);

    ret = phy_duplex_get(unit, port, pDuplex);

    PHY_SEM_UNLOCK(unit);

    return ret;
} /* end of phy_duplex_get */

/* Function Name:
 *      dal_phy_portDuplex_set
 * Description:
 *      Set duplex mode status of the specific port
 * Input:
 *      unit          - unit id
 *      port          - port id
 *      duplex        - duplex mode of the port, full or half
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int32
dal_phy_portDuplex_set(uint32 unit, rtk_port_t port, rtk_port_duplex_t duplex)
{
    int32           ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, duplex=%d", unit, port, duplex);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    PHY_SEM_LOCK(unit);

    ret = phy_duplex_set(unit, port, duplex);

    PHY_SEM_UNLOCK(unit);

    return ret;

}

/* Function Name:
 *      dal_phy_portFiberOAMLoopBackEnable_set
 * Description:
 *      Set fiber port OAM Loopback featrue enable or not
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      enable  - status of fiber down speed
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portFiberOAMLoopBackEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d enable=%d", unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    PHY_SEM_LOCK(unit);

    ret = phy_fiberOAMLoopBackEnable_set(unit, port, enable);

    PHY_SEM_UNLOCK(unit);

    return ret;
}

/* Function Name:
 *      dal_phy_portPtpEnable_get
 * Description:
 *      Get TIME status of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pEnable - status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID     - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portPtpEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portPtpEnable_get */


/* Function Name:
 *      dal_phy_portPtpEnable_set
 * Description:
 *      Set TIME status of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      enable  - status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID     - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portPtpEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,enable=%d", unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portPtpEnable_set */

/* Function Name:
 *      dal_phy_portPtpRxTimestamp_get
 * Description:
 *      Get TIME timstamp of the TIME identifier of the dedicated port from the specified device.
 * Input:
 *      unit       - unit id
 *      port       - port id
 *      identifier - indentifier of TIME packet
 * Output:
 *      pTimeStamp - pointer buffer of TIME timestamp
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpRxTimestamp_get(
    uint32 unit,
    rtk_port_t port,
    rtk_time_ptpIdentifier_t identifier,
    rtk_time_timeStamp_t *pTimeStamp)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pTimeStamp), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpRxTimestamp_get(unit, port, identifier, pTimeStamp)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_mango_time_portPtpRxTimestamp_get */

/* Function Name:
 *      dal_phy_portPtpTxTimestamp_get
 * Description:
 *      Get TIME Tx timstamp of the TIME identifier of the dedicated port from the specified device.
 * Input:
 *      unit       - unit id
 *      port       - port id
 *      identifier - indentifier of TIME packet
 * Output:
 *      pTimeStamp - pointer buffer of TIME timestamp
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpTxTimestamp_get(
    uint32 unit,
    rtk_port_t port,
    rtk_time_ptpIdentifier_t identifier,
    rtk_time_timeStamp_t *pTimeStamp)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, msgType=%d, sequenceId=0x%x",
            unit, port, identifier.msgType, identifier.sequenceId);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pTimeStamp), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpTxTimestamp_get(unit, port, identifier, pTimeStamp)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portPtpTxTimestamp_get */


/* Function Name:
 *      dal_phy_portRefTime_get
 * Description:
 *      Get the reference time of TIME of the specified port.
 * Input:
 *      unit  - unit id
 *      port    - port id, it should be base port of PHY
 * Output:
 *      pTimeStamp - pointer buffer of TIME reference time
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID     - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portRefTime_get(uint32 unit, rtk_port_t port, rtk_time_timeStamp_t *pTimeStamp)
{
int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pTimeStamp), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpRefTime_get(unit, port, pTimeStamp)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portRefTime_get */


/* Function Name:
 *      dal_phy_portRefTime_set
 * Description:
 *      Set the reference time of TIME of the specified port.
 * Input:
 *      unit      - unit id
 *      portmask - portmask, it should be base ports of PHYs
 *      timeStamp - reference timestamp value
 *      exec        - 0 : do not execute, 1: execute
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portRefTime_set(uint32 unit, rtk_portmask_t portmask, rtk_time_timeStamp_t timeStamp, uint32 exec)
{
    int32   ret;
    rtk_port_t port;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,portmask=0x%x 0x%x,timeStamp=%d", unit, portmask.bits[1], portmask.bits[0], timeStamp);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((timeStamp.nsec > RTK_TIME_NSEC_MAX), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);

    HWP_PORT_TRAVS(unit, port)
    {
        if (RTK_PORTMASK_IS_PORT_SET(portmask, port))
        {
            if ((ret = phy_ptpRefTime_set(unit, port, timeStamp, exec)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
                return ret;
            }
        }
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portRefTime_set */


/* Function Name:
 *      dal_phy_portTimeAdjust_set
 * Description:
 *      Adjust TIME reference time.
 * Input:
 *      unit      - unit id
 *      portmask - portmask, it should be base ports of PHYs
 *      sign      - significant
 *      timeStamp - reference timestamp value
 *      exec        - 0 : do not execute, 1: execute
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 *      RT_ERR_INPUT        - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portRefTimeAdjust_set(uint32 unit, rtk_portmask_t portmask, uint32 sign, rtk_time_timeStamp_t timeStamp, uint32 exec)
{
    int32   ret;
    rtk_port_t port;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,portmask=0x%x 0x%x,timeStamp=%d", unit, portmask.bits[1], portmask.bits[0], timeStamp);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((timeStamp.nsec > RTK_TIME_NSEC_MAX), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);

    HWP_PORT_TRAVS(unit, port)
    {
        if (RTK_PORTMASK_IS_PORT_SET(portmask, port))
        {
            if ((ret = phy_ptpRefTimeAdjust_set(unit, port, sign, timeStamp, exec)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
                return ret;
            }
        }
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portRefTimeAdjust_set */

/* Function Name:
 *      dal_phy_portRefTimeEnable_get
 * Description:
 *      Get the enable state of reference time of the specified device.
 * Input:
 *      unit    - unit id
 *      port    - port id, it should be base port of PHY
 * Output:
 *      pEnable - status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portRefTimeEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpRefTimeEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_phy_portRefTimeEnable_get */

/* Function Name:
 *      dal_phy_portRefTimeEnable_set
 * Description:
 *      Set the enable state of reference time of the specified port.
 * Input:
 *      unit   - unit id
 *      portmask - portmask, it should be base ports of PHYs
 *      enable - status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portRefTimeEnable_set(uint32 unit, rtk_portmask_t portmask, rtk_enable_t enable)
{
    int32   ret;
    rtk_port_t port;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,portmask=0x%x 0x%x,enable=%d", unit, portmask.bits[1], portmask.bits[0], enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */

    /* function body */
    PHY_SEM_LOCK(unit);

    HWP_PORT_TRAVS(unit, port)
    {
        if (RTK_PORTMASK_IS_PORT_SET(portmask, port))
        {
            if ((ret = phy_ptpRefTimeEnable_set(unit, port, enable)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
                return ret;
            }
        }
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_mango_time_refTimeEnable_set */

/*
 * Function Declaration
 *      dal_phy_portRefTimeFreq_get
 * Description:
 *      Get the frequency of PTP reference time of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id, it should be base port of PHY
 * Output:
 *      pFreq  - pointer to reference time frequency
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      The frequency configuration decides the reference time tick frequency.
 *      The default value is 0x80000000.
 *      If it is configured to 0x40000000, the tick frequency would be half of default.
 *      If it is configured to 0xC0000000, the tick frequency would be one and half times of default.
 *
 */
int32
dal_phy_portRefTimeFreq_get(uint32 unit, rtk_port_t port, uint32 *pFreq)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pFreq), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpRefTimeFreq_get(unit, port, pFreq)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portRefTimeFreq_get */


/* Function Name:
 *      dal_phy_portRefTimeFreq_set
 * Description:
 *      Set the frequency of PTP reference time of the specified port.
 * Input:
 *      unit   - unit id
 *      portmask - portmask, it should be base ports of PHYs
 *      freq   - reference time frequency
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      The frequency configuration decides the reference time tick frequency.
 *      The default value is 0x80000000.
 *      If it is configured to 0x40000000, the tick frequence would be half of default.
 *      If it is configured to 0xC0000000, the tick frequence would be one and half times of default.
 */
int32
dal_phy_portRefTimeFreq_set(uint32 unit, rtk_portmask_t portmask, uint32 freq)
{
    int32   ret;
    rtk_port_t port;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,portmask=0x%x 0x%x,freq=%d", unit, portmask.bits[1], portmask.bits[0], freq);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */

    /* function body */
    PHY_SEM_LOCK(unit);

    HWP_PORT_TRAVS(unit, port)
    {
        if (RTK_PORTMASK_IS_PORT_SET(portmask, port))
        {
            if ((ret = phy_ptpRefTimeFreq_set(unit, port, freq)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
                return ret;
            }
        }
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portRefTimeFreq_set */

/* Function Name:
 *      dal_phy_portSwitchMacAddr_get
 * Description:
 *      Get the Switch MAC address setting of PHY of the specified port.
 * Input:
 *      unit           - unit id
 *      port           - port id
 *      pSwitchMacAddr - point to the Switch MAC Address
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portSwitchMacAddr_get(uint32 unit, rtk_port_t port, rtk_mac_t *pSwitchMacAddr)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pSwitchMacAddr), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpSwitchMacAddr_get(unit, port, pSwitchMacAddr)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portSwitchMacAddr_set
 * Description:
 *      Set the Switch MAC address setting of PHY of the specified port.
 * Input:
 *      unit           - unit id
 *      port           - port id
 *      pSwitchMacAddr - point to the Switch MAC Address
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portSwitchMacAddr_set(uint32 unit, rtk_port_t port, rtk_mac_t *pSwitchMacAddr)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port %d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpSwitchMacAddr_set(unit, port, pSwitchMacAddr)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpMacAddrRange_get
 * Description:
 *      Get the PTP MAC address of specific port.
 * Input:
 *      unit    - unit id
 *      port   - port ID
 * Output:
 *      pMacRange  - pointer to MAC adress range
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpMacAddrRange_get(uint32 unit, rtk_port_t port, uint32 *pRange)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pRange), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpSwitchMacRange_get(unit, port, pRange)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpMacAddrRange_set
 * Description:
 *      Set the PTP MAC address range of the specified port.
 * Input:
 *      unit   - unit id
 *      port   - port ID
 *      range  - pointer to MAC adress range
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portPtpMacAddrRange_set(uint32 unit, rtk_port_t port, uint32 range)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port %d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpSwitchMacRange_set(unit, port, range)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpVlanTpid_get
 * Description:
 *      Get the VLAN TPID of specific port.
 * Input:
 *      unit    - unit id
 *      port   - port ID
 *      type   - outer or inner VLAN
 *      idx    - TPID entry index
 * Output:
 *      pTpid  - pointer to TPID
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpVlanTpid_get(uint32 unit, rtk_port_t port, rtk_vlanType_t type, uint32 idx, uint32 *pTpid)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pTpid), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpIgrTpid_get(unit, port, type, idx, pTpid)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) && (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpVlanTpid_set
 * Description:
 *      Set the VLAN TPID of specific port.
 * Input:
 *      unit    - unit id
 *      port   - port ID
 *      type   - outer or inner VLAN
 *      idx     - TPID entry index
 *      tpid    - VLAN TPID
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portPtpVlanTpid_set(uint32 unit, rtk_port_t port, rtk_vlanType_t type, uint32 idx, uint32 tpid)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port %d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpIgrTpid_set(unit, port, type, idx, tpid)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpOper_get
 * Description:
 *      Get the PTP time operation configuration of specific port.
 * Input:
 *      unit    - unit id
 *      port   - port ID
 * Output:
 *      pOperCfg  - pointer to PTP time operation configuraton
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpOper_get(uint32 unit, rtk_port_t port, rtk_time_operCfg_t *pOperCfg)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pOperCfg), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpOper_get(unit, port, pOperCfg)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpOper_set
 * Description:
 *      Set the PTP time operation configuration of specific port.
 * Input:
 *      unit    - unit id
 *      port   - port ID
 *      pOperCfg  - pointer to PTP time operation configuraton
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpOper_set(uint32 unit, rtk_port_t port, rtk_time_operCfg_t *pOperCfg)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port %d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpOper_set(unit, port, pOperCfg)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpLatchTime_get
 * Description:
 *      Get the PTP latched time of specific port.
 * Input:
 *      unit    - unit id
 *      port   - port ID
 * Output:
 *      pOperCfg  - pointer to PTP time operation configuraton
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpLatchTime_get(uint32 unit, rtk_port_t port, rtk_time_timeStamp_t *pLatchTime)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pLatchTime), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_ptpLatchTime_get(unit, port, pLatchTime)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        if ((ret != RT_ERR_CHIP_NOT_SUPPORTED) || (ret != RT_ERR_PORT_NOT_SUPPORTED))
        {
            RT_ERR(ret, (MOD_DAL | MOD_PHY), "unit=%d,port=%d %s ret 0x%x", unit, port, __FUNCTION__, ret);
        }
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpRefTimeFreqCfg_get
 * Description:
 *      Get the frequency of reference time of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 * Output:
 *      pFreqCfg    - pointer to configured reference time frequency
 *      pFreqCur    - pointer to current reference time frequency
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpRefTimeFreqCfg_get(uint32 unit, rtk_port_t port, uint32 *pFreqCfg, uint32 *pFreqCur)
{
    int32   ret = RT_ERR_OK;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pFreqCfg), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pFreqCur), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpRefTimeFreqCfg_get(unit, port, pFreqCfg, pFreqCur)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpRefTimeFreqCfg_set
 * Description:
 *      Set the frequency of reference time of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 *      freq        - reference time frequency
 *      apply       - if the frequency is applied immediately
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portPtpRefTimeFreqCfg_set(uint32 unit, rtk_port_t port, uint32 freq, uint32 apply)
{
    int32   ret = RT_ERR_OK;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, freq=%d, apply=%d",
           unit, port, freq, apply);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpRefTimeFreqCfg_set(unit, port, freq, apply)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpTxInterruptStatus_get
 * Description:
 *      Get the TX timestamp FIFO non-empty interrupt status of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 * Output:
 *      pIntrSts    - interrupt status of RX/TX PTP frame types
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpTxInterruptStatus_get(uint32 unit, rtk_port_t port, uint32 *pIntrSts)
{
    int32   ret = RT_ERR_OK;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pIntrSts), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpTxInterruptStatus_get(unit, port, pIntrSts)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpInterruptEnable_get
 * Description:
 *      Get the PTP TX timestamp FIFO non-empty interrupt enable status of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pEnable - status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpInterruptEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret = RT_ERR_OK;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpInterruptEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpInterruptEnable_set
 * Description:
 *      Set the PTP TX timestamp FIFO non-empty interrupt enable status of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      enable  - status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portPtpInterruptEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret = RT_ERR_OK;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpInterruptEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpTxTimestampFifo_get
 * Description:
 *      Get the top entry from PTP Tx timstamp FIFO on the dedicated port from the specified device. of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 * Output:
 *      pTimeEntry  - pointer buffer of TIME timestamp entry
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpTxTimestampFifo_get(uint32 unit, rtk_port_t port, rtk_time_txTimeEntry_t *pTimeEntry)
{
    int32   ret = RT_ERR_OK;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pTimeEntry), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpTxTimestampFifo_get(unit, port, pTimeEntry)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtp1PPSOutput_get
 * Description:
 *      Get the 1 PPS output configuration of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 * Output:
 *      pPulseWidth - pointer to 1 PPS pulse width
 *      pEnable     - pointer to 1 PPS output enable status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtp1PPSOutput_get(uint32 unit, rtk_port_t port, uint32 *pPulseWidth, rtk_enable_t *pEnable)
{
    int32   ret = RT_ERR_OK;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pPulseWidth), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptp1PPSOutput_get(unit, port, pPulseWidth, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtp1PPSOutput_set
 * Description:
 *      Set the 1 PPS output configuration of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 *      pulseWidth  - pointer to 1 PPS pulse width
 *      enable      - enable 1 PPS output
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portPtp1PPSOutput_set(uint32 unit, rtk_port_t port, uint32 pulseWidth, rtk_enable_t enable)
{
    int32   ret = RT_ERR_OK;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptp1PPSOutput_set(unit, port, pulseWidth, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpClockOutput_get
 * Description:
 *      Get the clock output configuration of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 * Output:
 *      pClkOutput  - pointer to clock output configuration
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpClockOutput_get(uint32 unit, rtk_port_t port, rtk_time_clkOutput_t *pClkOutput)
{
    int32   ret = RT_ERR_OK;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pClkOutput), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpClockOutput_get(unit, port, pClkOutput)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpClockOutput_set
 * Description:
 *      Set the clock output configuration of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 *      pClkOutput  - pointer to clock output configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portPtpClockOutput_set(uint32 unit, rtk_port_t port, rtk_time_clkOutput_t *pClkOutput)
{
    int32   ret = RT_ERR_OK;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d",
           unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pClkOutput), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpClockOutput_set(unit, port, pClkOutput)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpOutputSigSel_get
 * Description:
 *      Get the output pin signal selection configuration of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 * Output:
 *      pOutSigSel  - pointer to output pin signal selection configuration
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpOutputSigSel_get(uint32 unit, rtk_port_t port, rtk_time_outSigSel_t *pOutSigSel)
{
    int32   ret = RT_ERR_OK;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pOutSigSel), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpOutputSigSel_get(unit, port, pOutSigSel)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpOutputSigSel_set
 * Description:
 *      Set the output pin signal selection configuration of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 *      outSigSel   - output pin signal selection configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portPtpOutputSigSel_set(uint32 unit, rtk_port_t port, rtk_time_outSigSel_t outSigSel)
{
    int32   ret = RT_ERR_OK;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, outSigSel=%d",
           unit, port, outSigSel);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((outSigSel >= PTP_OUT_SIG_SEL_END), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpOutputSigSel_set(unit, port, outSigSel)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpTransEnable_get
 * Description:
 *      Get the enable status for PTP transparent clock of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 * Output:
 *      pEnable     - pointer to PTP transparent clock enable status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpTransEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret = RT_ERR_OK;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpTransEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpTransEnable_set
 * Description:
 *      Set the enable status for PTP transparent clock of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      enable  - PTP transparent clock enable status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portPtpTransEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret = RT_ERR_OK;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpTransEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpLinkDelay_get
 * Description:
 *      Get the link delay for PTP p2p transparent clock of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 * Output:
 *      pLinkDelay  - pointer to link delay (unit: nsec)
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portPtpLinkDelay_get(uint32 unit, rtk_port_t port, uint32 *pLinkDelay)
{
    int32   ret = RT_ERR_OK;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pLinkDelay), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpLinkDelay_get(unit, port, pLinkDelay)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPtpLinkDelay_set
 * Description:
 *      Set the link delay for PTP p2p transparent clock of the specified port.
 * Input:
 *      unit        - unit id
 *      port        - port id
 *      linkDelay   - link delay (unit: nsec)
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portPtpLinkDelay_set(uint32 unit, rtk_port_t port, uint32 linkDelay)
{
    int32   ret = RT_ERR_OK;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, linkDelay=%d",
           unit, port, linkDelay);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ptpLinkDelay_set(unit, port, linkDelay)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_phy_fiberUnidirEnable_set
 * Description:
 *      Set fiber unidirection enable status of the specific port
 * Input:
 *      unit           - unit id
 *      port           - port id
 *      enable         - enable status of fiber unidirection
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_fiberUnidirEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=0x%x ,enable=%d", unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = phy_fiberUnidirEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_fiberUnidirEnable_set */

/* Function Name:
 *      dal_phy_portReset_set
 * Description:
 *      Set PHY standard register Reset bit (0.15).
 * Input:
 *      unit                - unit id
 *      port                - port id
 *      enable              - ENABLED  : Set Reset bit; Reset PHY;
 *                            DISABLED : Clear Reset bit; PHY back to normal operation..
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_portReset_set(uint32 unit, rtk_port_t port)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = phy_reset_set(unit, port)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portReset_set */

/* Function Name:
 *      dal_phy_portLinkStatus_get
 * Description:
 *      Get PHY link status from standard register (1.2).
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pStatus - pointer to the link status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      The Link Status bit (Status Register 1.2) has LL (Latching Low) attribute
 *      for link failure. Please refer IEEE 802.3 for detailed.
 */
int32
dal_phy_portLinkStatus_get(uint32 unit, rtk_port_t port,
    rtk_port_linkStatus_t *pStatus)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pStatus), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = phy_linkStatus_get(unit, port, pStatus)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portLinkStatus_get */

/* Function Name:
 *      dal_phy_portPeerAutoNegoAbility_get
 * Description:
 *      Get ability from link partner advertisement auto negotiation of the specific port
 * Input:
 *      unit - unit id
 *      port - port id
 * Output:
 *      pAbility - pointer to PHY auto negotiation ability
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int32
dal_phy_portPeerAutoNegoAbility_get(uint32 unit, rtk_port_t port,
    rtk_port_phy_ability_t *pAbility)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pAbility), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = phy_peerAutoNegoAbility_get(unit, port, pAbility)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_portPeerAutoNegoAbility_get */

/* Function Name:
 *      _dal_phy_portSwMacPollPhyStatus_get
 * Description:
 *      Get PHY status of a specific port
 * Input:
 *      unit    - unit ID
 *      port    - port ID
 * Output:
 *      pphyStatus  - PHY status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
_dal_phy_portSwMacPollPhyStatus_get(uint32 unit, rtk_port_t port, rtk_port_swMacPollPhyStatus_t *pphyStatus)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    PHY_SEM_LOCK(unit);

    ret = _phy_swMacPollPhyStatus_get(unit, port, pphyStatus);

    PHY_SEM_UNLOCK(unit);

    return ret;
}

/* Function Name:
 *      dal_phy_portMacIntfSerdesMode_get
 * Description:
 *      Get PHY MAC side serdes mode
 * Input:
 *      unit    - unit ID
 *      port    - port ID
 * Output:
 *      pserdesMode  - PHY serdes mode
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_portMacIntfSerdesMode_get(uint32 unit, rtk_port_t port, rt_serdesMode_t *pserdesMode)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    PHY_SEM_LOCK(unit);

    ret = phy_macIntfSerdesMode_get(unit, port, pserdesMode);

    PHY_SEM_UNLOCK(unit);

    return ret;
}

/* Function Name:
 *      dal_phy_portLedMode_set
 * Description:
 *      Set LED mode for PHY control LED
 * Input:
 *      unit    - unit ID
 *      port    - port ID
 *      pLedMode - LED mode select
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_portLedMode_set(uint32 unit, rtk_port_t port, rtk_phy_ledMode_t *pLedMode)
{
    int32   ret;
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "unit=%d,port=%d\n", unit, port);
    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */

    /* function body */
    PHY_SEM_LOCK(unit);
    ret = phy_ledMode_set(unit, port, pLedMode);
    PHY_SEM_UNLOCK(unit);

    return ret;
}   /* end of dal_phy_portLedMode_set */

/* Function Name:
 *      dal_phy_portLedCtrl_get
 * Description:
 *      Get configuration of LED for PHY control LED
 * Input:
 *      unit    - unit ID
 *      port    - port ID
 * Output:
 *      pLedCtrl - LED control
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_portLedCtrl_get(uint32 unit, rtk_port_t port, rtk_phy_ledCtrl_t *pLedCtrl)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);
    ret = phy_ledCtrl_get(unit, port, pLedCtrl);
    PHY_SEM_UNLOCK(unit);

    return ret;
}   /* end of dal_phy_portLedCtrl_get */

/* Function Name:
 *      dal_phy_portLedCtrl_set
 * Description:
 *      Configure LED for PHY control LED
 * Input:
 *      unit    - unit ID
 *      port    - port ID
 *      pLedCtrl - LED control
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_portLedCtrl_set(uint32 unit, rtk_port_t port, rtk_phy_ledCtrl_t *pLedCtrl)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "unit=%d,port=%d,ledCtrl blink_rate=%u burst_cycle=%u clock_cycle=%u active_low=%u", unit, port,
            pLedCtrl->blink_rate, pLedCtrl->burst_cycle, pLedCtrl->clock_cycle, pLedCtrl->active_low);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);
    ret = phy_ledCtrl_set(unit, port, pLedCtrl);
    PHY_SEM_UNLOCK(unit);

    return ret;
}   /* end of dal_phy_portLedCtrl_set */

/* Function Name:
 *      dal_phy_portMacIntfSerdesLinkStatus_get
 * Description:
 *      Get PHY's MAC side interface serdes link status
 * Input:
 *      unit    - unit ID
 *      port    - port ID
 * Output:
 *      pStatus - link status of the SerDes
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_NOT_SUPPORTED   - This function is not supported by the PHY of this port
 * Note:
 *      None
 */
int32
dal_phy_portMacIntfSerdesLinkStatus_get(uint32 unit, rtk_port_t port,
    rtk_phy_macIntfSdsLinkStatus_t *pStatus)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pStatus), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    ret = phy_macIntfSerdesLinkStatus_get(unit, port, pStatus);
    PHY_SEM_UNLOCK(unit);

    return ret;
}   /* end of dal_phy_portMacIntfSerdesLinkStatus_get */

/* Function Name:
 *      _dal_phy_macIntfSerdes_reset
 * Description:
 *      Reset PHY's MAC interface SerDes
 * Input:
 *      unit     - unit id
 *      basePort - base port id of the PHY
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
_dal_phy_macIntfSerdes_reset(uint32 unit, rtk_port_t basePort)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    PHY_SEM_LOCK(unit);

    ret = phy_serdes_rst(unit, basePort);

    PHY_SEM_UNLOCK(unit);

    return ret;
}


/* Function Name:
 *      dal_phy_portSdsEyeParam_get
 * Description:
 *      Get SerDes eye parameters
 * Input:
 *      unit    - unit ID
 *      port    - Base mac ID of the PHY
 * Output:
 *      pEyeParam - eye parameter
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_portSdsEyeParam_get(uint32 unit, rtk_port_t port, uint32 sdsId, rtk_sds_eyeParam_t *pEyeParam)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);
    ret = phy_sdsEyeParam_get(unit, port, sdsId, pEyeParam);
    PHY_SEM_UNLOCK(unit);

    return ret;
}   /* end of dal_phy_portLedCtrl_get */

/* Function Name:
 *      dal_phy_portSdsEyeParam_set
 * Description:
 *      Set SerDes eye parameters
 * Input:
 *      unit    - unit ID
 *      port    - Base mac ID of the PHY
 *      pEyeParam - eye parameter
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_portSdsEyeParam_set(uint32 unit, rtk_port_t port, uint32 sdsId, rtk_sds_eyeParam_t *pEyeParam)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "unit=%d,port=%d,sdsId=%u,imp=%u,preAmp=%u,mainAmp=%u,postAmp=%u,preEn=%d,postEn=%d", unit, port, sdsId,
        pEyeParam->impedance, pEyeParam->pre_amp, pEyeParam->main_amp, pEyeParam->post_amp, pEyeParam->pre_en, pEyeParam->post_en);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);
    ret = phy_sdsEyeParam_set(unit, port, sdsId, pEyeParam);
    PHY_SEM_UNLOCK(unit);

    return ret;
}   /* end of dal_phy_portLedCtrl_set */


/* Function Name:
 *      dal_phy_portMdiLoopbackEnable_get
 * Description:
 *      Enable port MDI loopback for connecting with RJ45 loopback connector
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pEnable - pointer to status of MDI loopback
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portMdiLoopbackEnable_get(uint32 unit, rtk_port_t port, rtk_enable_t *pEnable)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_mdiLoopbackEnable_get(unit, port, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portMdiLoopbackEnable_set
 * Description:
 *      Enable port MDI loopback for connecting with RJ45 loopback connector
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - status of MDI loopback
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portMdiLoopbackEnable_set(uint32 unit, rtk_port_t port, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_mdiLoopbackEnable_set(unit, port, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_phy_portIntr_init
 * Description:
 *      Initialize the type of PHY interrupt function of the specified PHY chip.
 * Input:
 *      unit    - unit id
 *      port    - base mac ID number of the PHY
 *      phyIntr - PHY interrupt type
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT     - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portIntr_init(uint32 unit, rtk_port_t port, rtk_phy_intr_t phyIntr)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, phyIntr=%d", unit, port, phyIntr);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((phyIntr >= RTK_PHY_INTR_END), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_intr_init(unit, port, phyIntr)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portIntrEnable_get
 * Description:
 *      Get the type of PHY interrupt enable status of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      phyIntr - PHY interrupt type
 * Output:
 *      pEnable - pointer to status of interrupt enable
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT     - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portIntrEnable_get(uint32 unit, rtk_port_t port, rtk_phy_intr_status_t phyIntr, rtk_enable_t *pEnable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, phyIntr=%d", unit, port, phyIntr);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((phyIntr >= RTK_PHY_INTR_STATUS_END), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get value from CHIP */
    if ((ret = phy_intrEnable_get(unit, port, phyIntr, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_PHY), "port %d", port);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pEnable=%d", *pEnable);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portIntrEnable_set
 * Description:
 *      Set the type of PHY interrupt enable status of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      phyIntr - PHY interrupt type
 *      enable  - status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT     - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portIntrEnable_set(uint32 unit, rtk_port_t port, rtk_phy_intr_status_t phyIntr, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, phyIntr=%d, enable=%d", unit, port, phyIntr, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((phyIntr >= RTK_PHY_INTR_STATUS_END), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_intrEnable_set(unit, port, phyIntr, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPhyIntrStatus_get
 * Description:
 *      Get specified PHY interrupt status.
 * Input:
 *      unit    - unit id
 *      port    - port id or base mac ID number of the PHY
 *      phyIntr - PHY interrupt type
 * Output:
 *      pStatus - interrupt triggered status for specified PHY interrupt
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT     - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portIntrStatus_get(uint32 unit, rtk_port_t port, rtk_phy_intr_t phyIntr, rtk_phy_intrStatusVal_t *pStatus)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, phyIntr=%d", unit, port, phyIntr);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((phyIntr >= RTK_PHY_INTR_END), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pStatus), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get value from CHIP */
    if ((ret = phy_intrStatus_get(unit, port, phyIntr, pStatus)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_PHY), "port %d", port);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pStatus=%d", *pStatus);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portIntrMask_get
 * Description:
 *      Get PHY interrupt mask status of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      phyIntr - PHY interrupt type
 * Output:
 *      pMask   - pointer to status of PHY interrupt mask
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT     - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portIntrMask_get(uint32 unit, rtk_port_t port, rtk_phy_intr_t phyIntr, uint32 *pMask)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, phyIntr=%d", unit, port, phyIntr);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((phyIntr >= RTK_PHY_INTR_END), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pMask), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get value from CHIP */
    if ((ret = phy_intrMask_get(unit, port, phyIntr, pMask)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_PHY), "port %d", port);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pMask=%d", *pMask);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portIntrMask_set
 * Description:
 *      Set PHY interrupt mask status of the specified port.
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      phyIntr - PHY interrupt type
 *      mask    - mask
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID  - invalid unit id
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT     - invalid port id
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      None
 */
int32
dal_phy_portIntrMask_set(uint32 unit, rtk_port_t port, rtk_phy_intr_t phyIntr, uint32 mask)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, phyIntr=%d, mask=%d", unit, port, phyIntr, mask);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((phyIntr >= RTK_PHY_INTR_END), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_intrMask_set(unit, port, phyIntr, mask)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_phy_portPhySdsTestMode_set
 * Description:
 *      Set SerDes test mode.
 * Input:
 *      unit        - unit id
 *      port        - base mac ID number of the PHY
 *      sdsId       - SerDes id
 *      testMode    - test mode
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT         - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_portPhySdsTestMode_set(uint32 unit, rtk_port_t port, uint32 sdsId, rtk_sds_testMode_t testMode)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, sdsId=%d, testMode=%d", unit, port, sdsId, testMode);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_sdsTestMode_set(unit, port, sdsId, testMode)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL | MOD_PHY), "");
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portPhySdsTestModeCnt_get
 * Description:
 *      Get SerDes test mode test pattern error counter.
 * Input:
 *      unit        - unit id
 *      port        - base mac ID number of the PHY
 *      sdsId       - SerDes id
 * Output:
 *      pCnt        - test pattern error counter
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT         - invalid port id
 * Note:
 *      The test pattern error counter register is read-clear.
 */
int32
dal_phy_portPhySdsTestModeCnt_get(uint32 unit, rtk_port_t port, uint32 sdsId, uint32 *pCnt)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, sdsId=%d", unit, port, sdsId);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pCnt), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    /* get value from CHIP */
    if ((ret = phy_sdsTestModeCnt_get(unit, port, sdsId, pCnt)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_PHY), "port %d", port);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pCnt=%u", *pCnt);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_phy_portSdsLeq_get
 * Description:
 *      Get the statue of LEQ of the specific PHY's SerDes in the specific unit
 * Input:
 *      unit    - unit id
 *      port    - base port id of the PHY
 *      sdsId   - SerDes of the PHY
 * Output:
 *      pManual_en - pointer to manual LEQ config satus
 *      pLeq_val   - pointer to current LEQ value
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID - invalid port id
 *      RT_ERR_PORT_NOT_SUPPORTED   - This function is not supported by the PHY of this port
 * Note:
 *      None
 */
int32
dal_phy_portSdsLeq_get(uint32 unit, rtk_port_t port, uint32 sdsId, rtk_enable_t *pManual_en, uint32 *pLeq_val)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pManual_en), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pLeq_val), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_sdsLeq_get(unit, port, sdsId, pManual_en, pLeq_val)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portSdsLeq_set
 * Description:
 *      Get the statue of LEQ of the specific PHY's SerDes in the specific unit
 * Input:
 *      unit    - unit id
 *      port    - base port id of the PHY
 *      sdsId   - SerDes of the PHY
 *      manual_en - ENABLED: LEQ in manual-mode; DISABLED: LEQ is auto-mode.
 *      leq_val - Fixed LEQ value when manual_en is set to ENABLED;
 *                this field is not used in driver when manual_en set to DISABLED, just keep it set to 0.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID - invalid port id
 *      RT_ERR_PORT_NOT_SUPPORTED   - This function is not supported by the PHY of this port
 * Note:
 *      None
 */
int32
dal_phy_portSdsLeq_set(uint32 unit, rtk_port_t port, uint32 sdsId, rtk_enable_t manual_en, uint32 leq_val)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d leq=leq_val",
           unit, port, sdsId, manual_en, leq_val);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((manual_en != DISABLED && manual_en != ENABLED), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_sdsLeq_set(unit, port, sdsId, manual_en, leq_val)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portCtrl_get
 * Description:
 *      Get Port/PHY specific settings
 * Input:
 *      unit      - unit id
 *      port      - port id
 *      ctrl_type - setting type
 * Output:
 *      pValue    - pointer to setting value
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portCtrl_get(uint32 unit, rtk_port_t port, rtk_phy_ctrl_t ctrl_type, uint32 *pValue)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pValue), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ctrl_get(unit, port, ctrl_type, pValue)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portCtrl_set
 * Description:
 *      Set the statue of Control of the specific port in the specific unit
 * Input:
 *      unit      - unit id
 *      port      - port id
 *      ctrl_type - setting type
 *      value     - setting value
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portCtrl_set(uint32 unit, rtk_port_t port, rtk_phy_ctrl_t ctrl_type, uint32 value)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, ctrl_type=%d, value=%d",
           unit, port, ctrl_type, value);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_ctrl_set(unit, port, ctrl_type, value)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portLiteEnable_get
 * Description:
 *      Get the status of Lite setting
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      mode    - Lite speed mode
 * Output:
 *      pEnable - pointer to status of Lite
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portLiteEnable_get(uint32 unit, rtk_port_t port, rtk_port_lite_mode_t mode, rtk_enable_t *pEnable)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_liteEnable_get(unit, port, mode, pEnable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portLiteEnable_set
 * Description:
 *      Set the status of Lite setting
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      mode    - Lite speed mode
 *      enable  - status of Lite
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portLiteEnable_set(uint32 unit, rtk_port_t port, rtk_port_lite_mode_t mode, rtk_enable_t enable)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, enable=%d",
           unit, port, enable);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_liteEnable_set(unit, port, mode, enable)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_phy_portSpeedDuplex_get
 * Description:
 *      Get the negotiated port speed and duplex status of the specific port
 * Input:
 *      unit    - unit id
 *      port    - port id
 * Output:
 *      pSpeed  - pointer to the port speed
 *      pDuplex - pointer to the port duplex
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID       - invalid unit id
 *      RT_ERR_NOT_INIT      - The module is not initial
 *      RT_ERR_PORT_ID       - invalid port id
 *      RT_ERR_NULL_POINTER  - input parameter may be null pointer
 *      RT_ERR_PORT_LINKDOWN - link down port status
 * Note:
 *      None
 */
int32
dal_phy_portSpeedDuplex_get( uint32 unit, rtk_port_t port, rtk_port_speed_t *pSpeed, rtk_port_duplex_t *pDuplex)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pSpeed), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pDuplex), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_speedDuplexStatus_get(unit, port, pSpeed, pDuplex)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL | MOD_PHY), "Get speedDuplexStatus Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portLinkMedia_get
 * Description:
 *      Get link status and media
 * Input:
 *      unit   - unit id
 *      port   - port id
 *      enable - status of link-down power saving
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 *      RT_ERR_INPUT    - input parameter out of range
 * Note:
 *      None
 */
int32
dal_phy_portLinkMedia_get(uint32 unit, rtk_port_t port, rtk_port_linkStatus_t *pStatus, rtk_port_media_t *pMedia)
{

    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pStatus), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pMedia), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_linkStatus_get(unit, port, pStatus))  != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Get linkStatus Error Code: 0x%X", ret);
        return ret;
    }

    if ((ret = phy_media_get(unit, port, pMedia)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Get media Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_portDbgCounter_get
 * Description:
 *      Get the status of debug counter
 * Input:
 *      unit - unit id
 *      port - port id
 *      type - counter type
 * Output:
 *      pCnt - pointer to counter value
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portDbgCounter_get(uint32 unit, rtk_port_t port, rtk_port_phy_dbg_cnt_t type, uint64 *pCnt)
{
    int32   ret;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pCnt), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    if ((ret = phy_dbgCounter_get(unit, port, type, pCnt)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_debug_get
 * Description:
 *      Dump RealTek PHY debug information
 * Input:
 *      unit  - unit id
 *      pDbg  - pointer to the parameter structure
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 * Note:
 *      None
 */
int32
dal_phy_debug_get(uint32 unit, rtk_phy_debug_t *pDbg)
{
    int32       ret = RT_ERR_FAILED;
    uint32      i = 0;
    uint32      j = 0;
    uint32      index = 0;
    rtk_port_t  port;

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pDbg), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);
    switch (pDbg->phyDbgType)
    {
        case RTK_PHY_DEBUG_COUPLING:
            if ((ret = phy_ctrl_set(unit, pDbg->port, RTK_PHY_CTRL_DEBUG_DUMP_COUPLING, pDbg->parameter.coupling.channel_bitmap)) != RT_ERR_OK)
            {
                PHY_SEM_UNLOCK(unit);
                RT_LOG(LOG_DEBUG, (MOD_DAL | MOD_PHY), "ret=0x%x", ret);
                return ret;
            }
            break;

        case RTK_PHY_DEBUG_DSP:
            osal_printf("\nu,p,item,cnt,ch,str,idx,data\n");

            for (j = 0; j < 64; j++)
            {
                if (pDbg->parameter.dsp.itemBitmap & (1ULL << j))
                {
                    index = j + 1;

                    if ((ret = phy_ctrl_set(unit, pDbg->port, RTK_PHY_CTRL_DEBUG_DUMP_DSP_INIT, index)) != RT_ERR_OK)
                    {
                        PHY_SEM_UNLOCK(unit);
                        RT_LOG(LOG_DEBUG, (MOD_DAL | MOD_PHY), "ret=0x%x", ret);
                        return ret;
                    }
                }
            }

            for (i = 0; i < pDbg->parameter.dsp.cnt; i++)
            {
                for (j = 0; j < 64; j++)
                {
                    if (pDbg->parameter.dsp.itemBitmap & (1ULL << j))
                    {
                        index = j + 1;

                        if ((ret = phy_ctrl_set(unit, pDbg->port, RTK_PHY_CTRL_DEBUG_DUMP_DSP, RTK_PHY_CTRL_DEBUG_DUMP_ITEM(i, index))) != RT_ERR_OK)
                        {
                            PHY_SEM_UNLOCK(unit);
                            RT_LOG(LOG_DEBUG, (MOD_DAL | MOD_PHY), "ret=0x%x", ret);
                            return ret;
                        }
                    }
                }
            }
            break;

        case RTK_PHY_DEBUG_DSP_MULTIPORT:
            osal_printf("\nu,p,item,cnt,ch,str,idx,data\n");


            HWP_PORT_TRAVS(unit, port)
            {
                if (RTK_PORTMASK_IS_PORT_SET(pDbg->parameter.dsp_multiport.portmask, port))
                {
                    for (j = 0; j < 64; j++)
                    {
                        if (pDbg->parameter.dsp_multiport.itemBitmap & (1ULL << j))
                        {
                            index = j + 1;
                            if ((ret = phy_ctrl_set(unit, port, RTK_PHY_CTRL_DEBUG_DUMP_DSP_INIT, index)) != RT_ERR_OK)
                            {
                                PHY_SEM_UNLOCK(unit);
                                RT_LOG(LOG_DEBUG, (MOD_DAL | MOD_PHY), "item %u init ret=0x%x", index, ret);
                                return ret;
                            }
                        }
                    }
                }
            }

            for (i = 0; i < pDbg->parameter.dsp_multiport.cnt; i++)
            {

                for (j = 0; j < 64; j++)
                {
                    if (pDbg->parameter.dsp_multiport.itemBitmap & (1ULL << j))
                    {
                        index = j + 1;
                        HWP_PORT_TRAVS(unit, port)
                        {
                            if (RTK_PORTMASK_IS_PORT_SET(pDbg->parameter.dsp_multiport.portmask, port))
                            {

                                if ((ret = phy_ctrl_set(unit, port, RTK_PHY_CTRL_DEBUG_DUMP_DSP, RTK_PHY_CTRL_DEBUG_DUMP_ITEM(i, index))) != RT_ERR_OK)
                                {
                                    PHY_SEM_UNLOCK(unit);
                                    RT_LOG(LOG_DEBUG, (MOD_DAL | MOD_PHY), "item %u ret=0x%x", index, ret);
                                    return ret;
                                }
                            }
                        }
                    }
                }
            }
            break;

        default:
            PHY_SEM_UNLOCK(unit);
            return RT_ERR_INPUT;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      _dal_phy_debugCmd_set
 * Description:
 *      debug command set
 * Input:
 *      unit     - unit id
 *      port -  mac id
 *      mdxMacId - mdio mac id
 *      sds - serdes id
 *      cmd - command string
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 * Note:
 *      None
 */
int32
_dal_phy_debugCmd_set(uint32 unit, rtk_port_t port, rtk_port_t mdxMacId, uint32 sds, uint8 *cmd)
{
    int32       ret = RT_ERR_PORT_NOT_SUPPORTED;

    PHY_SEM_LOCK(unit);

#if defined(CONFIG_SDK_RTL8295R) || defined(CONFIG_SDK_RTL8214QF) || defined(CONFIG_SDK_RTL8224QF)
    if ((HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_RTL8295R)
     || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_RTL8295R_C22)
     || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_RTL8214QF)
     || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_RTL8214QF_NC5)
     || (HWP_PHY_MODEL_BY_PORT(unit, port) == RTK_PHYTYPE_RTL8224QF))
    {
        ret = phy_8295_diag_set(unit, port, mdxMacId, sds, cmd);
    }
#endif

    PHY_SEM_UNLOCK(unit);
    return ret;
}


/* Function Name:
 *      _dal_phy_debug_cmd
 * Description:
 *      debug command
 * Input:
 *      unit     - unit id
 *      port -  mac id
 *      mdxMacId - mdio mac id
 *      sds - serdes id
 *      cmd - command string
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 * Note:
 *      None
 */
int32
_dal_phy_debug_cmd(uint32 unit, char *cmd_str, rtk_portmask_t *portmask, uint32 param1, uint32 param2, uint32 param3, uint32 param4, uint32 param5)
{

    return _phy_debug_cmd(unit, cmd_str, portmask, param1, param2, param3, param4, param5);
}

int32
_dal_phy_debug_batch_init(uint32 entry_num)
{
    if (batch_profile != NULL)
    {
        osal_free(batch_profile);
        batch_profile = NULL;
        batch_profile_state = INIT_NOT_COMPLETED;
        batch_next_index = 0;
    }

    if (entry_num == 0) //deinit
    {
        return RT_ERR_OK;
    }

    batch_profile = (rtk_phy_batch_entry_t *)osal_alloc(entry_num * sizeof(rtk_phy_batch_entry_t));
    if (batch_profile == NULL)
    {
        osal_printf("Error: memory allocate failed!\n");
        return RT_ERR_FAILED;
    }
    osal_memset(batch_profile, 0x0, (entry_num * sizeof(rtk_phy_batch_entry_t)));
    osal_printf("init batch profile as %u entries\n",entry_num);
    batch_profile_state = INIT_COMPLETED;
    batch_next_index = 0;
    return RT_ERR_OK;
}

void
_dal_phy_debug_batch_entry_print(uint32 idx, rtk_phy_batch_entry_t *entry)
{
    uint32 i = 0;
    switch (entry->batch_op)
    {
        case RTK_PHY_BATCH_OP_EMPTY:
            osal_printf("[%4u] EMPTY ",idx);
            break;
        case RTK_PHY_BATCH_OP_LOOPSTART:
            osal_printf("[%4u] loopstart ",idx);
            break;
        case RTK_PHY_BATCH_OP_LOOPEND:
            osal_printf("[%4u] loopend ",idx);
            break;
        case RTK_PHY_BATCH_OP_W_C45:
            osal_printf("[%4u] w ",idx);
            break;
        case RTK_PHY_BATCH_OP_R_C45:
            osal_printf("[%4u] r ",idx);
            break;
        case RTK_PHY_BATCH_OP_C_C45:
            osal_printf("[%4u] c ",idx);
            break;
        case RTK_PHY_BATCH_OP_W_C22:
            osal_printf("[%4u] 22w ",idx);
            break;
        case RTK_PHY_BATCH_OP_R_C22:
            osal_printf("[%4u] 22r ",idx);
            break;
        case RTK_PHY_BATCH_OP_C_C22:
            osal_printf("[%4u] 22c ",idx);
            break;
        case RTK_PHY_BATCH_OP_WEQ_C45:
            osal_printf("[%4u] weq ",idx);
            break;
        case RTK_PHY_BATCH_OP_WNEQ_C45:
            osal_printf("[%4u] wneq ",idx);
            break;
        default:
            osal_printf("[%4u]unknown(%u) ", idx, entry->batch_op);
            break;
    }
    for (i = 0; i < entry->para_num; i++)
    {
        if (i == 1 || i == 4 || (entry->para_num == 3 && i == 2))
            osal_printf("0x%X ", entry->para[i]);
        else
            osal_printf("%u ", entry->para[i]);
    }
    osal_printf("\n");
}

int32
_dal_phy_debug_batch_set(rtk_phy_batch_op_t op, rtk_phy_batch_para_t *pPara)
{
    rtk_phy_batch_entry_t entry;
    osal_memset(&entry, 0x0, sizeof(rtk_phy_batch_entry_t));

    if (batch_next_index >= batch_size)
    {
        osal_printf("Error: batch profile entry full!\n");
        return RT_ERR_FAILED;
    }
    batch_profile[batch_next_index].batch_op = op;
    batch_profile[batch_next_index].para_num = pPara->para_num;
    batch_profile[batch_next_index].para[0] = pPara->para1;
    batch_profile[batch_next_index].para[1] = pPara->para2;
    batch_profile[batch_next_index].para[2] = pPara->para3;
    batch_profile[batch_next_index].para[3] = pPara->para4;
    batch_profile[batch_next_index].para[4] = pPara->para5;
    batch_next_index++;

    return RT_ERR_OK;
}

int32
_dal_phy_debug_batch_run_op(uint32 unit, rtk_port_t port, rtk_phy_batch_entry_t **profile, uint32 index)
{
    int32 ret = RT_ERR_OK;
    uint32 data = 0;
    uint32 mask = 0;
    uint32 i = 0;
    rtk_phy_batch_entry_t *p = (*profile);
    uint32 wait_count = 0;

    if (batch_output == 1)
    {
        _dal_phy_debug_batch_entry_print(index, &p[index]);
    }

    switch (p[index].batch_op)
    {
        case RTK_PHY_BATCH_OP_W_C45:
            if (p[index].para_num == 5)
            {
                if ((ret = hal_miim_mmd_read(unit, port, p[index].para[0], p[index].para[1], &data)) != RT_ERR_OK)
                    return ret;

                for (i = p[index].para[3]; i <= p[index].para[2]; i++)
                {
                    mask = mask | (0x1 << i);
                }

                data = REG32_FIELD_SET(data, p[index].para[4], p[index].para[3], mask);
                if((ret = hal_miim_mmd_write(unit, port, p[index].para[0], p[index].para[1], data)) != RT_ERR_OK)
                    return ret;

            }
            else
            {
                if((ret = hal_miim_mmd_write(unit, port, p[index].para[0], p[index].para[1], p[index].para[2])) != RT_ERR_OK)
                    return ret;
            }
            break;
        case RTK_PHY_BATCH_OP_R_C45:
            if((ret = hal_miim_mmd_read(unit, port, p[index].para[0], p[index].para[1], &data)) != RT_ERR_OK)
                return ret;
            if (p[index].para_num == 4)
            {
                for (i = p[index].para[3]; i <= p[index].para[2]; i++)
                {
                    mask = mask | (0x1 << i);
                }

                data = (data & mask) >> p[index].para[3];
            }
            BATCH_PRT(0, batch_output,"0x%04X\n", data);
            BATCH_PRT(1, batch_output,"read: 0x%04X\n", data);
            break;
        case RTK_PHY_BATCH_OP_C_C45:
            if((ret = hal_miim_mmd_read(unit, port, p[index].para[0], p[index].para[1], &data)) != RT_ERR_OK)
                return ret;
            if (p[index].para_num == 5)
            {
                for (i = p[index].para[3]; i <= p[index].para[2]; i++)
                {
                    mask = mask | (0x1 << i);
                }
                data = (data & mask) >> p[index].para[3];

                if (data != p[index].para[4])
                {
                    BATCH_PRT(0, batch_output,"[%u:%u]0x%04X != 0x%04X\n", p[index].para[2], p[index].para[3], data, p[index].para[4]);
                    BATCH_PRT(1, batch_output,"compare failed: 0x%04X != 0x%04X\n", data, p[index].para[4]);
                    BATCH_PRT(2, batch_output,"%u.0x%X[%u:%u]:0x%04X != 0x%04X\n", p[index].para[0], p[index].para[1], p[index].para[2], p[index].para[3],
                              data, p[index].para[4]);
                    batch_err_cnt++;
                }
            }
            else
            {
                if (data != p[index].para[2])
                {
                    BATCH_PRT(0, batch_output,"0x%04X != 0x%04X\n", data, p[index].para[2]);
                    BATCH_PRT(1, batch_output,"compare failed: 0x%04X != 0x%04X\n", data, p[index].para[2]);
                    BATCH_PRT(2, batch_output,"%u.0x%X:0x%04X != 0x%04X\n", p[index].para[0], p[index].para[1],
                              data, p[index].para[2]);
                    batch_err_cnt++;
                }
            }
            break;
        case RTK_PHY_BATCH_OP_WEQ_C45:
            do
            {
                if ((ret = hal_miim_mmd_read(unit, port, p[index].para[0], p[index].para[1], &data)) != RT_ERR_OK)
                    return ret;
                if (p[index].para_num == 5)
                {
                    for (i = p[index].para[3]; i <= p[index].para[2]; i++)
                    {
                        mask = mask | (0x1 << i);
                    }
                    data = (data & mask) >> p[index].para[3];
                }
                osal_time_udelay(10);
                if (++wait_count > 10000)
                {
                    osal_printf("wait time out, 0x%04X != 0x%04X\n", data, (p[index].para_num == 5)? (p[index].para[4]):(p[index].para[2]));
                    break;
                }
            } while (data != p[index].para[4]);
            break;
        case RTK_PHY_BATCH_OP_WNEQ_C45:
            do
            {
                if ((ret = hal_miim_mmd_read(unit, port, p[index].para[0], p[index].para[1], &data)) != RT_ERR_OK)
                    return ret;
                if (p[index].para_num == 5)
                {
                    for (i = p[index].para[3]; i <= p[index].para[2]; i++)
                    {
                        mask = mask | (0x1 << i);
                    }
                    data = (data & mask) >> p[index].para[3];
                }
                osal_time_udelay(10);
                if (++wait_count > 10000)
                {
                    osal_printf("wait time out, 0x%04X == 0x%04X\n", data, (p[index].para_num == 5)? (p[index].para[4]):(p[index].para[2]));
                    break;
                }
            } while (data == p[index].para[4]);
            break;
        case RTK_PHY_BATCH_OP_W_C22:
            if (p[index].para_num == 5)
            {
                if ((ret = hal_miim_read(unit, port, p[index].para[0], p[index].para[1], &data)) != RT_ERR_OK)
                    return ret;

                for (i = p[index].para[3]; i <= p[index].para[2]; i++)
                {
                    mask = mask | (0x1 << i);
                }

                data = REG32_FIELD_SET(data, p[index].para[4], p[index].para[3], mask);
                if((ret = hal_miim_write(unit, port, p[index].para[0], p[index].para[1], data)) != RT_ERR_OK)
                    return ret;

            }
            else
            {
                if((ret = hal_miim_write(unit, port, p[index].para[0], p[index].para[1], p[index].para[2])) != RT_ERR_OK)
                    return ret;
            }
            break;
        case RTK_PHY_BATCH_OP_R_C22:
            if((ret = hal_miim_read(unit, port, p[index].para[0], p[index].para[1], &data)) != RT_ERR_OK)
                return ret;
            if (p[index].para_num == 4)
            {
                for (i = p[index].para[3]; i <= p[index].para[2]; i++)
                {
                    mask = mask | (0x1 << i);
                }

                data = (data & mask) >> p[index].para[3];
            }
            BATCH_PRT(0, batch_output,"0x%04X\n", data);
            BATCH_PRT(1, batch_output,"read: 0x%04X\n", data);
            break;
        case RTK_PHY_BATCH_OP_C_C22:
            if((ret = hal_miim_read(unit, port, p[index].para[0], p[index].para[1], &data)) != RT_ERR_OK)
                return ret;
            if (p[index].para_num == 5)
            {
                for (i = p[index].para[3]; i <= p[index].para[2]; i++)
                {
                    mask = mask | (0x1 << i);
                }
                data = (data & mask) >> p[index].para[3];

                if (data != p[index].para[4])
                {
                    BATCH_PRT(0, batch_output,"[%u:%u]0x%04X != 0x%04X\n", p[index].para[2], p[index].para[3], data, p[index].para[4]);
                    BATCH_PRT(1, batch_output,"compare failed: 0x%04X != 0x%04X\n", data, p[index].para[4]);
                    BATCH_PRT(2, batch_output,"%u.0x%X[%u:%u]:0x%04X != 0x%04X\n", p[index].para[0], p[index].para[1], p[index].para[2], p[index].para[3],
                              data, p[index].para[4]);
                    batch_err_cnt++;
                }
            }
            else
            {
                if (data != p[index].para[2])
                {
                    BATCH_PRT(0, batch_output,"0x%04X != 0x%04X\n", data, p[index].para[2]);
                    BATCH_PRT(1, batch_output,"compare failed: 0x%04X != 0x%04X\n", data, p[index].para[2]);
                    BATCH_PRT(2, batch_output,"%u.0x%X:0x%04X != 0x%04X\n", p[index].para[0], p[index].para[1],
                              data, p[index].para[2]);
                    batch_err_cnt++;
                }
            }
            break;
        default:
            osal_printf("Error batch op type: %u\n", p[index].batch_op);
            break;
    }
    return RT_ERR_OK;
}

int32
_dal_phy_debug_batch_loop(uint32 unit, rtk_port_t port, rtk_phy_batch_entry_t **profile, uint32 end_index, uint32 *loopIdx)
{
    int32 ret = RT_ERR_OK;
    uint32 j = 0;
    uint32 loopstart = 0, loopend = end_index-1;
    uint32 loop = 0, loopcnt = 0;
    if (profile == NULL)
    {
        osal_printf("Error: NULL profile!\n");
        return RT_ERR_FAILED;
    }

    loopcnt = (*profile)[*loopIdx].para[0];
    loopstart = (*loopIdx) + 1;

    for (loop = 0; loop < loopcnt; loop++)
    {
        for (j = loopstart; j < end_index; j++)
        {
            if ((*profile)[j].batch_op == RTK_PHY_BATCH_OP_LOOPSTART)
            {
                osal_printf("Error: not support loop in loop!\n");
                return RT_ERR_FAILED;
            }
            else if ((*profile)[j].batch_op == RTK_PHY_BATCH_OP_LOOPEND)
            {
                loopend = j;
                break;
            }
            if ((ret = _dal_phy_debug_batch_run_op(unit, port, profile, j)) != RT_ERR_OK)
                return ret;
        }
    }

    *loopIdx = loopend;
    return ret;
}

int32
_dal_phy_debug_batch_run(uint32 unit, rtk_phy_batch_entry_t **profile, uint32 end_index)
{
    int32 ret = RT_ERR_OK;
    uint32 i = 0;
    rtk_port_t  port;

    PHY_SEM_LOCK(unit);

    batch_err_cnt = 0;
    HWP_PORT_TRAVS(unit, port)
    {
        if (RTK_PORTMASK_IS_PORT_SET(batch_portmask, port))
        {
            osal_printf("batch port %u:\n", port);
            for (i = 0; i < end_index; i++)
            {
                if ((*profile)[i].batch_op == RTK_PHY_BATCH_OP_LOOPSTART)
                {

                    if ((ret = _dal_phy_debug_batch_loop(unit, port, profile, end_index, &i)) != RT_ERR_OK)
                    {
                        break;
                    }
                }
                else
                {
                    if ((ret = _dal_phy_debug_batch_run_op(unit, port, profile, i)) != RT_ERR_OK)
                    {
                        break;
                    }
                }
            }

            if (ret != RT_ERR_OK)
            {
                break;
            }
        }
    }

    if (batch_err_cnt != 0)
    {
        osal_printf("Compare Error Count: %u\n", batch_err_cnt);
    }
    PHY_SEM_UNLOCK(unit);
    return ret;
}

/* Function Name:
 *      dal_phy_debug_batch_op_set
 * Description:
 *      debug batch command
 * Input:
 *      pPara     - input parameter
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 * Note:
 *      None
 */
int32
dal_phy_debug_batch_op_set(uint32 unit, rtk_phy_batch_para_t *pPara)
{

    int32 ret = RT_ERR_OK;
    uint32 idx = 0;
    //osal_printf("op: [%u]\n", pPara->batch_op);

    if ((pPara->batch_op != RTK_PHY_BATCH_OP_INIT) && (batch_profile_state != INIT_COMPLETED || batch_profile == NULL) )
    {
            osal_printf("batch profile is not initializated!(run \"debug batch init\")\n");
            return RT_ERR_FAILED;
    }

    switch (pPara->batch_op)
    {
        case RTK_PHY_BATCH_OP_INIT:
            if (pPara->para_num == 0)
            {
                if ((ret = _dal_phy_debug_batch_init(batch_size)) != RT_ERR_OK)
                    return ret;
            }
            else if (pPara->para_num == 1)
            {
                batch_size = pPara->para1;
                if ((ret = _dal_phy_debug_batch_init(batch_size)) != RT_ERR_OK)
                    return ret;
            }
            else
            {
                osal_printf("Error parameter!\n");
                return RT_ERR_FAILED;
            }
            break;

        case RTK_PHY_BATCH_OP_RUN:
            //run
            if (batch_port_state != INIT_COMPLETED)
            {
                osal_printf("batch port is not initializated!(run \"debug batch port <port list>\")\n");
                return RT_ERR_FAILED;
            }
            if (pPara->para_num == 0)
            {
                ret = _dal_phy_debug_batch_run(unit, &batch_profile, batch_next_index);
            }
            else
            {
                batch_output = pPara->para1;
                ret = _dal_phy_debug_batch_run(unit, &batch_profile, batch_next_index);
            }
            break;

        case RTK_PHY_BATCH_OP_DUMP:
            osal_printf("dump batch profile:\n");
            for (idx = 0; idx < batch_next_index; idx++)
            {
                if (batch_profile->batch_op == RTK_PHY_BATCH_OP_EMPTY)
                {
                    break;
                }
                _dal_phy_debug_batch_entry_print(idx, &batch_profile[idx]);
            }
            break;

        case RTK_PHY_BATCH_OP_LOOPSTART:
            if (pPara->para_num != 1 )
            {
                osal_printf("Error parameter!\n");
                return RT_ERR_FAILED;
            }
            if (pPara->para1 < 1)
            {
                osal_printf("Error parameter! (<num>:%u < 1)\n", pPara->para1);
                return RT_ERR_FAILED;
            }
            ret = _dal_phy_debug_batch_set(pPara->batch_op, pPara);
            break;

        case RTK_PHY_BATCH_OP_LOOPEND:
            if (pPara->para_num != 0)
            {
                osal_printf("Error parameter!\n");
                return RT_ERR_FAILED;
            }
            ret = _dal_phy_debug_batch_set(pPara->batch_op, pPara);
            break;

        case RTK_PHY_BATCH_OP_W_C45:
        case RTK_PHY_BATCH_OP_W_C22:
        case RTK_PHY_BATCH_OP_C_C45:
        case RTK_PHY_BATCH_OP_C_C22:
        case RTK_PHY_BATCH_OP_WEQ_C45:
        case RTK_PHY_BATCH_OP_WNEQ_C45:
            if ((pPara->para_num != 3) && (pPara->para_num != 5))
            {
                osal_printf("Error parameter!\n");
                return RT_ERR_FAILED;
            }
            ret = _dal_phy_debug_batch_set(pPara->batch_op, pPara);
            break;

        case RTK_PHY_BATCH_OP_R_C45:
        case RTK_PHY_BATCH_OP_R_C22:
            if ((pPara->para_num != 2) && (pPara->para_num != 4))
            {
                osal_printf("Error parameter!\n");
                return RT_ERR_FAILED;
            }
            ret = _dal_phy_debug_batch_set(pPara->batch_op, pPara);
            break;

        default:
            osal_printf("Unknow batch command!\n");
            return RT_ERR_FAILED;
    }

    return ret;
}

/* Function Name:
 *      dal_phy_debug_batch_port_set
 * Description:
 *      Set target port list for debug batch tool
 * Input:
 *      unit      - unit
 *      pPortmask - the ports for debug batch
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 * Note:
 *      None
 */
int32
dal_phy_debug_batch_port_set(uint32 unit, rtk_portmask_t *pPortmask)
{
    RT_PARAM_CHK((NULL == pPortmask), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK(!HWP_PMSK_EXIST(unit, pPortmask), RT_ERR_PORT_MASK);

    osal_memcpy(&batch_portmask, pPortmask, sizeof(rtk_portmask_t));

    batch_port_state = INIT_COMPLETED;
    return RT_ERR_OK;
}

/* Function Name:
 *      dal_phy_port_macsecReg_get
 * Description:
 *      Get PHY MACSec register data of the specific port
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      type    - macsec reg type
 *      reg     - register address
 * Output:
 *      pData   - pointer to the reg data
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_PORT_ID      - invalid port id
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_port_macsecReg_get(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir, uint32 reg,
    uint32 *pData)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d,reg=0x%x", unit, port, dir, reg);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pData), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = miim_phy_macsec_reg_get(unit, port, dir, reg, pData)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pData=0x%x", *pData);

    return RT_ERR_OK;
}   /* end of dal_phy_port_macsecReg_get */

/* Function Name:
 *      dal_phy_port_macsecReg_set
 * Description:
 *      Set PHY MACSec register data of the specific port
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      type    - macsec reg type
 *      reg     - register address
 *      data    - value write to register
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_PORT_ID  - invalid port id
 * Note:
 *      None
 */
int32
dal_phy_port_macsecReg_set(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir, uint32 reg,
    uint32 data)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d,reg=0x%x,data=0x%x", unit, port, dir, reg, data);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);

    if ((ret = miim_phy_macsec_reg_set(unit, port, dir, reg, data)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_port_macsecReg_set */


/* Function Name:
 *      dal_phy_macsec_port_cfg_set
 * Description:
 *      Set per-port configurations for MACsec
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      pPortcfg - pointer to macsec port configuration structure
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_port_cfg_set(uint32 unit, rtk_port_t port,
    rtk_macsec_port_cfg_t *pPortcfg)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pPortcfg), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_port_cfg_set(unit, port, pPortcfg)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_port_cfg_set */

/* Function Name:
 *      dal_phy_macsec_port_cfg_get
 * Description:
 *      Get per-port configurations for MACsec
 * Input:
 *      unit     - unit id
 *      port     - port id
 * Output:
 *      pPortcfg - pointer to macsec port configuration structure
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_port_cfg_get(uint32 unit, rtk_port_t port,
    rtk_macsec_port_cfg_t *pPortcfg)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pPortcfg), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_port_cfg_get(unit, port, pPortcfg)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_port_cfg_get */

/* Function Name:
 *      dal_phy_macsec_sc_create
 * Description:
 *      Create a MACsec Secure Channel
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      dir      - ingress or egress
 *      pSc      - pointer to macsec sc configuration structure
 * Output:
 *      pSc_id   - pointer to the created SC id
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_sc_create(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir,
    rtk_macsec_sc_t *pSc, uint32 *pSc_id)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d", unit, port, dir);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MACSEC_DIR_END <= dir), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pSc), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pSc_id), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_sc_create(unit, port, dir, pSc, pSc_id)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_sc_create */

/* Function Name:
 *      dal_phy_macsec_sc_get
 * Description:
 *      Get configuration info for a created Secure Channel
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      dir      - ingress or egress
 *      sc_id    - SC id
 * Output:
 *      pSc_index - pointer to the created SC id
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_sc_get(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_sc_t *pSc)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d,sc_id=%d", unit, port, dir, sc_id);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MACSEC_DIR_END <= dir), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= sc_id), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pSc), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_sc_get(unit, port, dir, sc_id, pSc)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_sc_get */

/* Function Name:
 *      dal_phy_macsec_sc_del
 * Description:
 *      Delete a Secure Channel
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      dir      - ingress or egress
 *      sc_id    - SC id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_sc_del(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir,
    uint32 sc_id)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d,sc_id=%d", unit, port, dir, sc_id);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MACSEC_DIR_END <= dir), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= sc_id), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_sc_del(unit, port, dir, sc_id)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_sc_del */

/* Function Name:
 *      dal_phy_macsec_sc_status_get
 * Description:
 *      Get hardware status for a Secure Channel
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      dir      - ingress or egress
 *      sc_id    - SC id
 * Output:
 *      pSc_status - pointer to macsec SC status structure
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_sc_status_get(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_sc_status_t *pSc_status)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d,sc_id=%d", unit, port, dir, sc_id);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MACSEC_DIR_END <= dir), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= sc_id), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pSc_status), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_sc_status_get(unit, port, dir, sc_id, pSc_status)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_sc_status_get */

/* Function Name:
 *      dal_phy_macsec_sa_create
 * Description:
 *      Create a MACsec Secure Association
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      dir      - ingress or egress
 *      sc_id    - SC id
 *      an       - Secure Association Number
 *      pSa      - pointer to macsec SA configuration structure
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_sa_create(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_an_t an, rtk_macsec_sa_t *pSa)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d,sc_id=%d,an=%d", unit, port, dir, sc_id, an);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MACSEC_DIR_END <= dir), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= sc_id), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MACSEC_AN_MAX <= an), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pSa), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_sa_create(unit, port, dir, sc_id, an, pSa)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_sa_create */

/* Function Name:
 *      dal_phy_macsec_sa_get
 * Description:
 *      Get configuration info for a Secure Association
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      dir      - ingress or egress
 *      sc_id    - SC id
 *      an       - Secure Association Number
 * Output:
 *      pSa      - pointer to macsec SA configuration structure
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_sa_get(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_an_t an, rtk_macsec_sa_t *pSa)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d,sc_id=%d,an=%d", unit, port, dir, sc_id, an);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MACSEC_DIR_END <= dir), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= sc_id), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MACSEC_AN_MAX <= an), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pSa), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_sa_get(unit, port, dir, sc_id, an, pSa)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_sa_get */

/* Function Name:
 *      dal_phy_macsec_sa_del
 * Description:
 *      Delete a MACsec Secure Association
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      dir      - ingress or egress
 *      sc_id    - SC id
 *      an       - Secure Association Number
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_sa_del(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_an_t an)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d,sc_id=%d,an=%d", unit, port, dir, sc_id, an);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MACSEC_DIR_END <= dir), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= sc_id), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MACSEC_AN_MAX <= an), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_sa_del(unit, port, dir, sc_id, an)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_sa_del */

/* Function Name:
 *      dal_phy_macsec_sa_activate
 * Description:
 *      Activate a MACsec Secure Association
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      dir      - ingress or egress
 *      sc_id    - Secure Channel id
 *      an       - Secure Association Number
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      For egress, this function will change running SA.
 */
int32
dal_phy_macsec_sa_activate(uint32 unit, rtk_port_t port, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_an_t an)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,dir=%d,sc_id=%d,an=%d", unit, port, dir, sc_id, an);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MACSEC_DIR_END <= dir), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= sc_id), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MACSEC_AN_MAX <= an), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_sa_activate(unit, port, dir, sc_id, an)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_sa_activate */

/* Function Name:
 *      dal_phy_macsec_rxsa_disable
 * Description:
 *      Disable a ingress MACsec Secure Association
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      rxsc_id  - ingress SC id
 *      an       - Secure Association Number
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_rxsa_disable(uint32 unit, rtk_port_t port, uint32 rxsc_id,
    rtk_macsec_an_t an)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,rxsc_id=%d,an=%d", unit, port, rxsc_id, an);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= rxsc_id), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MACSEC_AN_MAX <= an), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_rxsa_disable(unit, port, rxsc_id, an)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_rxsa_disable */

/* Function Name:
 *      dal_phy_macsec_txsa_disable
 * Description:
 *      Disable the running egress MACsec Secure Association
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      txsc_id  - egress SC id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_txsa_disable(uint32 unit, rtk_port_t port, uint32 txsc_id)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,txsc_id=%d", unit, port, txsc_id);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= txsc_id), RT_ERR_INPUT);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_txsa_disable(unit, port, txsc_id)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_txsa_disable */

/* Function Name:
 *      dal_phy_macsec_stat_clear
 * Description:
 *      Clear all statistics counter
 * Input:
 *      unit     - unit id
 *      port     - port id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_stat_clear(uint32 unit, rtk_port_t port)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_stat_clear(unit, port)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_stat_clear */

/* Function Name:
 *      dal_phy_macsec_stat_port_get
 * Description:
 *      get per-port statistics counter
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      stat     - statistics type
 * Output:
 *      pCnt     - pointer to counter value
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      the counter value will read clear, customer software should collecting and accumulating the statistics.
 */
int32
dal_phy_macsec_stat_port_get(uint32 unit, rtk_port_t port, rtk_macsec_stat_t stat,
    uint64 *pCnt)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,stat=%d", unit, port, stat);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MACSEC_STAT_MAX <= stat), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pCnt), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_stat_port_get(unit, port, stat, pCnt)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_stat_port_get */

/* Function Name:
 *      dal_phy_macsec_stat_txsa_get
 * Description:
 *      get per-egress-SA statistics counter
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      txsc_id  - egress SC id
 *      an       - Secure Association Number
 *      stat     - statistics type
 * Output:
 *      pCnt     - pointer to counter value
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      the counter value will read clear, customer software should collecting and accumulating the statistics.
 */
int32
dal_phy_macsec_stat_txsa_get(uint32 unit, rtk_port_t port, uint32 txsc_id,
    rtk_macsec_an_t an, rtk_macsec_txsa_stat_t stat, uint64 *pCnt)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,txsc_id=%d,an=%d,stat=%d", unit, port, txsc_id, an, stat);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= txsc_id), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MACSEC_AN_MAX <= an), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MACSEC_TXSA_STAT_MAX <= stat), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pCnt), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_stat_txsa_get(unit, port, txsc_id, an, stat, pCnt)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_stat_txsa_get */

/* Function Name:
 *      dal_phy_macsec_stat_rxsa_get
 * Description:
 *      get per-egress-SA statistics counter
 * Input:
 *      unit     - unit id
 *      port     - port id
 *      rxsc_id  - ingress SC id
 *      an       - Secure Association Number
 *      stat     - statistics type
 * Output:
 *      pCnt     - pointer to counter value
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      the counter value will read clear, customer software should collecting and accumulating the statistics.
 */
int32
dal_phy_macsec_stat_rxsa_get(uint32 unit, rtk_port_t port, uint32 rxsc_id,
    rtk_macsec_an_t an, rtk_macsec_rxsa_stat_t stat, uint64 *pCnt)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d,rxsc_id=%d,an=%d,stat=%d", unit, port, rxsc_id, an, stat);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((RTK_MAX_MACSEC_SC_PER_PORT <= rxsc_id), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MACSEC_AN_MAX <= an), RT_ERR_INPUT);
    RT_PARAM_CHK((RTK_MACSEC_RXSA_STAT_MAX <= stat), RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pCnt), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_stat_rxsa_get(unit, port, rxsc_id, an, stat, pCnt)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_stat_rxsa_get */

/* Function Name:
 *      dal_phy_macsec_intr_status_get
 * Description:
 *      Get status information for MACsec interrupt
 * Input:
 *      unit     - unit id
 *      port     - port id
 * Output:
 *      pIntr_status - interrupt status structure
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      None
 */
int32
dal_phy_macsec_intr_status_get(uint32 unit, rtk_port_t port,
    rtk_macsec_intr_status_t *pIntr_status)
{
    int32   ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d,port=%d", unit, port);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(!HWP_PORT_EXIST(unit, port), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pIntr_status), RT_ERR_NULL_POINTER);

    /* function body */
    PHY_SEM_LOCK(unit);
    if ((ret = miim_phy_macsec_intr_status_get(unit, port, pIntr_status)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }
    PHY_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_phy_macsec_intr_status_get */

/* Function Name:
 *      dal_phy_portSdsReg_set
 * Description:
 *      Set PHY SerDes register data of the specific port
 * Input:
 *      unit    - unit id
 *      port    - port id
 *      sdsPage - page id
 *      sdsReg  - reg id
 *      data - reg data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT    - The module is not initial
 *      RT_ERR_PORT_ID     - invalid port id
 *      RT_ERR_PHY_PAGE_ID - invalid page id
 *      RT_ERR_PHY_REG_ID  - invalid reg id
 * Note:
 *      None
 */
int32
dal_phy_portSdsReg_set(
    uint32              unit,
    rtk_port_t          port,
    uint32              sdsPage,
    uint32              sdsReg,
    uint32              data)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, sdsPage=0x%x, sdsReg=0x%x",
           unit, port, sdsPage, sdsReg);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    //RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_sdsReg_set(unit, port, sdsPage, sdsReg, data)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);

    return ret;
} /* end of dal_phy_portReg_set */

/* Function Name:
 *      dal_phy_portSdsReg_get
 * Description:
 *      Get PHY MMD register data of the specific port
 * Input:
 *      unit                - unit id
 *      port                - port id
 *      sdsPage             - page id
 *      sdsReg              - reg id
 * Output:
 *      pData               - pointer to the PHY SerDes reg data
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID       - invalid port id
 *      RT_ERR_NULL_POINTER  - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_phy_portSdsReg_get(
    uint32              unit,
    rtk_port_t          port,
    uint32              sdsPage,
    uint32              sdsReg,
    uint32              *pData)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "unit=%d, port=%d, sdsPage=0x%x, sdsReg=0x%x",
           unit, port, sdsPage, sdsReg);

    /* check Init status */
    RT_INIT_CHK(phy_init[unit]);

    /* parameter check */
    //RT_PARAM_CHK((!HWP_PORT_EXIST(unit, port) || HWP_IS_CPU_PORT(unit, port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((NULL == pData), RT_ERR_NULL_POINTER);

    PHY_SEM_LOCK(unit);

    if ((ret = phy_sdsReg_get(unit, port, sdsPage, sdsReg, pData)) != RT_ERR_OK)
    {
        PHY_SEM_UNLOCK(unit);
        RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "Error Code: 0x%X", ret);
        return ret;
    }

    PHY_SEM_UNLOCK(unit);
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PHY), "pData=0x%x", *pData);

    return ret;
}    /* end of dal_phy_portMmdReg_get */

