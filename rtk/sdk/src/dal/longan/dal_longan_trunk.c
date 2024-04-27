/*
 * Copyright(c) Realtek Semiconductor Corporation, 2008-2009
 * All rights reserved.
 *
 * $Revision$
 * $Date$
 *
 * Purpose :
 *
 * Feature :
 *
 */


/*
 * Include Files
 */
#include <common/rt_type.h>
#include <common/rt_error.h>
#include <common/util/rt_bitop.h>
#include <common/util/rt_util.h>
#include <common/debug/rt_log.h>
#include <osal/sem.h>
#include <osal/lib.h>
#include <osal/memory.h>
#include <hal/chipdef/allmem.h>
#include <hal/chipdef/allreg.h>
#include <hal/chipdef/longan/rtk_longan_table_struct.h>
#include <hal/chipdef/longan/rtk_longan_reg_struct.h>
#include <hal/mac/reg.h>
#include <hal/mac/mem.h>
#include <hal/common/halctrl.h>
#include <dal/longan/dal_longan_trunk.h>
#include <dal/longan/dal_longan_port.h>
#include <rtk/default.h>
#include <rtk/trunk.h>
#include <rtk/stack.h>


/*
 * Symbol Definition
 */
#define INVALID_LOCAL_IDX (0x3F)
#define INVALID_LOCAL_TRUNK_ID (0XFF)
#define RTK_MAX_NUM_OF_TRUNKMON_CB                   1
#define MAX_NUM_OF_PORT_PER_DEV_SUPPORT             (64)

/* trunk monitor control block */
typedef struct dal_trunkMon_cb_s {
    uint32              num_of_trunkMon_callback_f;
    rtk_trunk_mon_callback_t  trunkMon_callback_f[RTK_MAX_NUM_OF_TRUNKMON_CB];
} dal_trunkMon_cb_t;

/*
 * Data Declaration
 */
static uint32               trunk_init[RTK_MAX_NUM_OF_UNIT] = {INIT_NOT_COMPLETED};
static osal_mutex_t         trunk_sem[RTK_MAX_NUM_OF_UNIT];
static rtk_portmask_t       *pTrunkMemberSet[RTK_MAX_NUM_OF_UNIT];
static uint8 **trunk2localIdx;
static uint16 **localTrunkID;
static rtk_trunk_mode_t trunkMode;
static dal_trunkMon_cb_t     *pTrunkMon_cb;

/*
 * Macro Definition
 */
/* trunk semaphore handling */
#define TRUNK_SEM_LOCK(unit)    \
do {\
    if (osal_sem_mutex_take(trunk_sem[unit], OSAL_SEM_WAIT_FOREVER) != RT_ERR_OK)\
    {\
        RT_ERR(RT_ERR_SEM_LOCK_FAILED, (MOD_DAL|MOD_TRUNK), "semaphore lock failed");\
        return RT_ERR_SEM_LOCK_FAILED;\
    }\
} while(0)
#define TRUNK_SEM_UNLOCK(unit)   \
do {\
    if (osal_sem_mutex_give(trunk_sem[unit]) != RT_ERR_OK)\
    {\
        RT_ERR(RT_ERR_SEM_UNLOCK_FAILED, (MOD_DAL|MOD_TRUNK), "semaphore unlock failed");\
        return RT_ERR_SEM_UNLOCK_FAILED;\
    }\
} while(0)

/*
 * Function Declaration
 */
/* Function Name:
 *      dal_longan_trunk_tbl_refresh
 * Description:
 *      Sync local table with SRAM table.
 * Input:
 *      unit - unit id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 * Note:
 *      None
 */
int32   dal_longan_trunk_tbl_refresh(uint32 unit)
{
    int32   ret = RT_ERR_FAILED;
    uint32 doRefresh = 0;

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    TRUNK_SEM_LOCK(unit);

    doRefresh = 1;
    RT_ERR_HDL(reg_field_write(unit,
                      LONGAN_TRK_LOCAL_TBL_REFRESHr,
                      LONGAN_TRK_LOCAL_TBL_REFRESHf,
                      &doRefresh), err_unlock, ret);

    do
    {
        RT_ERR_HDL(reg_field_read(unit,
                          LONGAN_TRK_LOCAL_TBL_REFRESHr,
                          LONGAN_TRK_LOCAL_TBL_REFRESHf,
                          &doRefresh), err_unlock, ret);
    } while (0 != doRefresh);

    TRUNK_SEM_UNLOCK(unit);

    return ret;

err_unlock:

    TRUNK_SEM_UNLOCK(unit);
    RT_ERR(ret, (MOD_DAL|MOD_TRUNK), ""); \
    return ret; \

}


static int32 _dal_longan_trunk_init_config(uint32 unit);

/* Function Name:
 *      dal_longan_trunkMapper_init
 * Description:
 *      Hook trunk module of the specified device.
 * Input:
 *      pMapper - pointer of mapper
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 * Note:
 *      Must Hook trunk module before calling any trunk APIs.
 */
int32
dal_longan_trunkMapper_init(dal_mapper_t *pMapper)
{
    pMapper->trunk_init = dal_longan_trunk_init;
    pMapper->trunk_mode_get = dal_longan_trunk_mode_get;
    pMapper->trunk_mode_set = dal_longan_trunk_mode_set;
    pMapper->trunk_distributionAlgorithmTypeBind_get = dal_longan_trunk_distributionAlgorithmTypeBind_get;
    pMapper->trunk_distributionAlgorithmTypeBind_set = dal_longan_trunk_distributionAlgorithmTypeBind_set;
    pMapper->trunk_distributionAlgorithmTypeParam_get = dal_longan_trunk_distributionAlgorithmTypeParam_get;
    pMapper->trunk_distributionAlgorithmTypeParam_set = dal_longan_trunk_distributionAlgorithmTypeParam_set;
    pMapper->trunk_distributionAlgorithmShiftGbl_get = dal_longan_trunk_distributionAlgorithmShiftGbl_get;
    pMapper->trunk_distributionAlgorithmShiftGbl_set = dal_longan_trunk_distributionAlgorithmShiftGbl_set;
    pMapper->trunk_trafficSeparateEnable_get = dal_longan_trunk_trafficSeparateEnable_get;
    pMapper->trunk_trafficSeparateEnable_set = dal_longan_trunk_trafficSeparateEnable_set;
    pMapper->trunk_trafficSeparateDivision_get = dal_longan_trunk_trafficSeparateDivision_get;
    pMapper->trunk_trafficSeparateDivision_set = dal_longan_trunk_trafficSeparateDivision_set;
    pMapper->trunk_port_get = dal_longan_trunk_port_get;
    pMapper->trunk_port_set = dal_longan_trunk_port_set;
    pMapper->trunk_localPort_get = dal_longan_trunk_localPort_get;
    pMapper->trunk_localPort_set = dal_longan_trunk_localPort_set;
    pMapper->trunk_egrPort_get = dal_longan_trunk_egrPort_get;
    pMapper->trunk_egrPort_set = dal_longan_trunk_egrPort_set;
    pMapper->trunk_stkTrkPort_get = dal_longan_trunk_stkTrkPort_get;
    pMapper->trunk_stkTrkPort_set = dal_longan_trunk_stkTrkPort_set;
    pMapper->trunk_stkTrkHash_get = dal_longan_trunk_stkTrkHash_get;
    pMapper->trunk_stkTrkHash_set = dal_longan_trunk_stkTrkHash_set;
    pMapper->trunk_stkDistributionAlgorithmTypeBind_get = dal_longan_trunk_stkDistributionAlgorithmTypeBind_get;
    pMapper->trunk_stkDistributionAlgorithmTypeBind_set = dal_longan_trunk_stkDistributionAlgorithmTypeBind_set;
    pMapper->trunk_localFirst_get = dal_longan_trunk_localFirst_get;
    pMapper->trunk_localFirst_set = dal_longan_trunk_localFirst_set;
    pMapper->trunk_localFirstFailOver_get = dal_longan_trunk_localFirstFailOver_get;
    pMapper->trunk_localFirstFailOver_set = dal_longan_trunk_localFirstFailOver_set;
    pMapper->trunk_srcPortMap_get = dal_longan_trunk_srcPortMap_get;
    pMapper->trunk_srcPortMap_set = dal_longan_trunk_srcPortMap_set;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_longan_trunk_init
 * Description:
 *      Initialize trunk module of the specified device.
 * Input:
 *      unit - unit id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      Must initialize trunk module before calling any trunk APIs.
 */
int32
dal_longan_trunk_init(uint32 unit)
{
    int32   ret = RT_ERR_FAILED;
    uint32 trkId;
    uint32 unitId;

    /* parameter check */
    RT_INIT_REENTRY_CHK(trunk_init[unit]);
    trunk_init[unit] = INIT_NOT_COMPLETED;

    /* create semaphore */
    trunk_sem[unit] = osal_sem_mutex_create();
    if (0 == trunk_sem[unit])
    {
        RT_ERR(RT_ERR_FAILED, (MOD_DAL|MOD_TRUNK), "semaphore create failed");
        return RT_ERR_FAILED;
    }

    trunkMode = TRUNK_MODE_STANDALONE;

    /* allocate memory for control block */
    if (NULL == pTrunkMon_cb)
    {
        pTrunkMon_cb = osal_alloc(sizeof(dal_trunkMon_cb_t));
        if (NULL == pTrunkMon_cb)
            return ret;
        else
            osal_memset(pTrunkMon_cb, 0, sizeof(dal_trunkMon_cb_t));
    }


    /* allocate memory for trunk database for this unit */
    if (NULL == pTrunkMemberSet[unit])
    {
        pTrunkMemberSet[unit] = (rtk_portmask_t *)osal_alloc(HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit) * sizeof(rtk_portmask_t));
        if (NULL == pTrunkMemberSet[unit])
        {
            RT_ERR(RT_ERR_FAILED, (MOD_TRUNK|MOD_DAL), "memory allocate failed");
            goto err_free;
        }
        else
                /* reset trunk member */
            osal_memset(pTrunkMemberSet[unit], 0, HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit) * sizeof(rtk_portmask_t));
    }
    else
    {
        return RT_ERR_OK;
    }

    if (NULL == trunk2localIdx)
    {
        trunk2localIdx = (uint8 **)osal_alloc(RTK_MAX_NUM_OF_UNIT * sizeof(uint8*));
        if (NULL == trunk2localIdx)
        {
            RT_ERR(RT_ERR_FAILED, (MOD_TRUNK|MOD_DAL), "memory allocate failed");
            goto err_free;
        }
        else
            osal_memset(trunk2localIdx, 0, RTK_MAX_NUM_OF_UNIT * sizeof(uint8*));
    }

    if (NULL == trunk2localIdx[unit])
    {
        trunk2localIdx[unit] = (uint8 *)osal_alloc(HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit) * sizeof(uint8));
        if (NULL == trunk2localIdx[unit])
        {
            RT_ERR(RT_ERR_FAILED, (MOD_TRUNK|MOD_DAL), "memory allocate failed");
            goto err_free;
        }
        else
            osal_memset(trunk2localIdx[unit], 0, HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit) * sizeof(uint8));
    }
    else
    {
        return RT_ERR_OK;
    }


    if (NULL == localTrunkID)
    {
        localTrunkID = (uint16 **)osal_alloc(RTK_MAX_NUM_OF_UNIT * sizeof(uint16*));
        if (NULL == localTrunkID)
        {
            RT_ERR(RT_ERR_FAILED, (MOD_TRUNK|MOD_DAL), "memory allocate failed");
            goto err_free;
        }
        else
            osal_memset(localTrunkID, 0, RTK_MAX_NUM_OF_UNIT * sizeof(uint16*));
    }

    if (NULL == localTrunkID[unit])
    {
        localTrunkID[unit] = (uint16 *)osal_alloc(HAL_MAX_NUM_OF_LOCAL_TRUNK(unit) * sizeof(uint16));
        if (NULL == localTrunkID[unit])
        {
            RT_ERR(RT_ERR_FAILED, (MOD_TRUNK|MOD_DAL), "memory allocate failed");
            goto err_free;
        }
        else
            osal_memset(localTrunkID[unit], 0, HAL_MAX_NUM_OF_LOCAL_TRUNK(unit) * sizeof(uint16));
    }
    else
    {
        return RT_ERR_OK;
    }


    for(trkId = 0; trkId < HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit); trkId ++)
    {
        trunk2localIdx[unit][trkId] = INVALID_LOCAL_IDX;
    }

    for(trkId = 0; trkId < HAL_MAX_NUM_OF_LOCAL_TRUNK(unit); trkId ++)
    {
        localTrunkID[unit][trkId] = INVALID_LOCAL_TRUNK_ID;
    }


    /* set init flag to complete init */
    trunk_init[unit] = INIT_COMPLETED;

    /* initialize default configuration */
    if ((ret = _dal_longan_trunk_init_config(unit)) != RT_ERR_OK)
    {
        goto err_free;
    }

    return RT_ERR_OK;

err_free:

    for(unitId = 0; unitId < RTK_MAX_NUM_OF_UNIT; unitId ++)
    {
        trunk_init[unitId] = INIT_NOT_COMPLETED;

        if (NULL != pTrunkMemberSet[unitId])
            osal_free(pTrunkMemberSet[unitId]);
        else
            continue;
    }

    if (NULL != trunk2localIdx)
    {
        for(unitId = 0; unitId < RTK_MAX_NUM_OF_UNIT; unitId ++)
        {
            if (NULL != trunk2localIdx[unitId])
                osal_free(trunk2localIdx[unitId]);
            else
                continue;
        }
        osal_free(trunk2localIdx);
    }

    if (NULL != localTrunkID)
    {
        for(unitId = 0; unitId < RTK_MAX_NUM_OF_UNIT; unitId ++)
        {
            if (NULL != localTrunkID[unitId])
                osal_free(localTrunkID[unitId]);
            else
                continue;
        }
        osal_free(localTrunkID);
    }


    if (NULL != pTrunkMon_cb)
        osal_free(pTrunkMon_cb);

    RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "init default configuration failed");
    return ret;


}   /* end of dal_longan_trunk_init */

/* Function Name:
 *      dal_longan_trunk_mon_register
 * Description:
 *      Register callback function for trunk change notification
 * Input:
 *      unit  - unit id
 *      trunkMon_callback    - callback function for trunk change
 * Output:
 *      None
 * Return:
 *      RT_ERR_FAILED   - initialize fail
 *      RT_ERR_OK       - initialize success
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *
 */
int32
dal_longan_trunk_mon_register(uint32 unit, rtk_trunk_mon_callback_t trunkMon_callback)
{
    uint32  i, flag_find_available = 0, available_index = 0;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_PORT), "trunkMon_callback=%p",
           trunkMon_callback);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == trunkMon_callback), RT_ERR_NULL_POINTER);

    /* check the callback function available index and check CB already exist or not? */
    for (i = 0; i < RTK_MAX_NUM_OF_TRUNKMON_CB; i++)
    {
        if (pTrunkMon_cb->trunkMon_callback_f[i] == NULL && flag_find_available == 0)
        {
            flag_find_available = 1;
            available_index = i;
        }
        if ((pTrunkMon_cb->trunkMon_callback_f[i] != NULL) && (pTrunkMon_cb->trunkMon_callback_f[i] == trunkMon_callback))
            return RT_ERR_CB_FUNCTION_EXIST;
    }

    if (flag_find_available)
        pTrunkMon_cb->trunkMon_callback_f[available_index] = trunkMon_callback;
    else
        return RT_ERR_CB_FUNCTION_FULL;

    pTrunkMon_cb->num_of_trunkMon_callback_f++;

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_mon_register */


/* Function Name:
 *      dal_longan_trunk_mode_get
 * Description:
 *      Get the trunk mode from the specified device.
 * Input:
 *      unit  - unit id
 * Output:
 *      pMode - pointer buffer of trunk mode
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      The enum of the trunk mode as following
 *      - TRUNK_MODE_STACKING
 *      - TRUNK_MODE_STANDALONE
 */
int32
dal_longan_trunk_mode_get (uint32 unit, rtk_trunk_mode_t *pMode)
{
    int32   ret;
    uint32 val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d", unit);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pMode), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);

    /* get entry from CHIP*/
    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_TRK_STAND_ALONE_MODEf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    if(0 == val)
        *pMode = TRUNK_MODE_STACKING;
    else
        *pMode = TRUNK_MODE_STANDALONE;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "pMode=%x",
           *pMode);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_mode_get */


/* Function Name:
 *      dal_longan_trunk_mode_set
 * Description:
 *      Set the trunk mode to the specified device.
 * Input:
 *      unit - unit id
 *      mode - trunk mode
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT - The module is not initial
 *      RT_ERR_INPUT    - invalid input parameter
 * Note:
 *      The enum of the trunk mode as following
*       - TRUNK_MODE_STACKING
 *      - TRUNK_MODE_STANDALONE
 */
int32
dal_longan_trunk_mode_set(uint32 unit, rtk_trunk_mode_t mode)
{
    int32   ret;
    uint32 val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d,mode=%d", unit, mode);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((TRUNK_MODE_END <= mode), RT_ERR_INPUT);

    /* function body */
    TRUNK_SEM_LOCK(unit);

    if(TRUNK_MODE_STACKING == mode)
        val = 0;
    else
        val = 1;

    /* get entry from CHIP*/
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_TRK_STAND_ALONE_MODEf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    trunkMode = mode;

    TRUNK_SEM_UNLOCK(unit);

    RT_ERR_CHK(dal_longan_trunk_tbl_refresh(unit), ret);
    return RT_ERR_OK;
}   /* end of dal_longan_trunk_mode_set */

/*
 * Function Declaration
 *      dal_longan_trunk_distributionAlgorithmTypeBind_get
 * Description:
 *      Get the distribution algorithm ID binded for packet types for a trunk group from the specified device.
 * Input:
 *      unit      - unit id
 *      trk_gid   - trunk group id
 *      type - bind packet type
 * Output:
 *      pAlgo_idx - pointer buffer of the distribution algorithm ID
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_TRUNK_ID  - invalid trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      None
 */
int32
dal_longan_trunk_distributionAlgorithmTypeBind_get(uint32 unit, rtk_trk_t trk_gid, rtk_trunk_bindType_t type, uint32 *pAlgo_idx)
{
    int32 ret;
    lag_entry_t lag_entry;
    uint32 field;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, trk_gid=%d, type=%d",
           unit, trk_gid, type);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    RT_PARAM_CHK(type >= BIND_TYPE_END, RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pAlgo_idx), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);

    /* parameter check */
    if(TRUNK_MODE_STACKING == trunkMode)
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););
    else
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););

    /*** get entry from chip ***/

    if ((ret = table_read(unit, LONGAN_LAGt, trk_gid, (uint32 *) &lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    switch(type)
    {
        case BIND_TYPE_L2:
            field = LONGAN_LAG_L2_HASH_MSK_IDXtf;
            break;
        case BIND_TYPE_IPV4:
            field = LONGAN_LAG_IP4_HASH_MSK_IDXtf;
            break;
        case BIND_TYPE_IPV6:
            field = LONGAN_LAG_IP6_HASH_MSK_IDXtf;
            break;
        default:
            field = LONGAN_LAG_L2_HASH_MSK_IDXtf;
            break;
    }

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                field, pAlgo_idx, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "pAlgo_idx=%x",
           *pAlgo_idx);

    return RT_ERR_OK;
} /* end of dal_longan_trunk_distributionAlgorithmTypeBind_get */


/* Function Name:
 *      dal_longan_trunk_distributionAlgorithmTypeBind_set
 * Description:
 *      Set the distribution algorithm ID binded for packet types for a trunk group from the specified device.
 * Input:
 *      unit     - unit id
 *      trk_gid  - trunk group id
 *      type - bind packet type
 *      algo_idx - index the distribution algorithm
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID     - invalid unit id
 *      RT_ERR_NOT_INIT    - The module is not initial
 *      RT_ERR_LA_TRUNK_ID - invalid trunk ID
 *      RT_ERR_LA_ALGO_ID  - invalid trunk algorithm ID
 * Note:
 *      (1) The valid range of trk_gid is 0~127 in 9310.
 *      (2) The valid range of algo_idx is 0~1 in 9310.
 */
int32
dal_longan_trunk_distributionAlgorithmTypeBind_set(uint32 unit, rtk_trk_t trk_gid, rtk_trunk_bindType_t type, uint32 algo_idx)
{
    int32 ret;
    uint32 field;
    lag_entry_t lag_entry;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, trk_gid=%d, pAlgo_bitmask=%x",
           unit, trk_gid, algo_idx);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    RT_PARAM_CHK(type >= BIND_TYPE_END, RT_ERR_INPUT);
    RT_PARAM_CHK(algo_idx >= HAL_MAX_NUM_OF_TRUNK_ALGO(unit), RT_ERR_LA_ALGO_ID);

    TRUNK_SEM_LOCK(unit);

    /* parameter check */
    if(TRUNK_MODE_STACKING == trunkMode)
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););
    else
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););

    if ((ret = table_read(unit, LONGAN_LAGt, trk_gid, (uint32 *) &lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    switch(type)
    {
        case BIND_TYPE_L2:
            field = LONGAN_LAG_L2_HASH_MSK_IDXtf;
            break;
        case BIND_TYPE_IPV4:
            field = LONGAN_LAG_IP4_HASH_MSK_IDXtf;
            break;
        case BIND_TYPE_IPV6:
            field = LONGAN_LAG_IP6_HASH_MSK_IDXtf;
            break;
        default:
            field = LONGAN_LAG_L2_HASH_MSK_IDXtf;
            break;
    }
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                field, &algo_idx, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    if ((ret = table_write(unit, LONGAN_LAGt, trk_gid, (uint32 *) &lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    RT_ERR_CHK(dal_longan_trunk_tbl_refresh(unit), ret);

    return RT_ERR_OK;
} /* end of dal_longan_trunk_distributionAlgorithmTypeBind_set */


/* Function Name:
 *      dal_longan_trunk_distributionAlgorithmTypeParam_get
 * Description:
 *      Get the distribution algorithm by packet type and algorithm ID from the specified device.
 * Input:
 *      unit          - unit id
 *      type         - packet type
 *      algo_idx    - algorithm index
 * Output:
 *      pAlgo_bitmask - pointer buffer of bitmask of the distribution algorithm
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_ALGO_ID   - invalid trunk algorithm ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *
 */
int32
dal_longan_trunk_distributionAlgorithmTypeParam_get(uint32 unit, rtk_trunk_hashParamType_t type, uint32 algo_idx, uint32 *pAlgo_bitmask)
{
    int32 ret;
    uint32 field;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, algo_idx=%d, type=%d",
           unit, algo_idx, type);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(algo_idx >= HAL_MAX_NUM_OF_TRUNK_ALGO(unit), RT_ERR_LA_ALGO_ID);
    RT_PARAM_CHK(type >= PARAM_TYPE_END, RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pAlgo_bitmask), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);

    switch(type)
    {
        case PARAM_TYPE_L2:
            field = LONGAN_L2_HASH_MSKf;
            break;
        case PARAM_TYPE_L3:
            field = LONGAN_L3_HASH_MSKf;
            break;
        default:
            break;
    }

    /* get entry from CHIP*/
    if ((ret = reg_array_field_read(unit,
                          LONGAN_TRK_HASH_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          algo_idx,
                          field,
                          pAlgo_bitmask)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "pAlgo_bitmask=%x",
           *pAlgo_bitmask);

    return RT_ERR_OK;
} /* end of dal_longan_trunk_distributionAlgorithmTypeParam_get */


/* Function Name:
 *      dal_longan_trunk_distributionAlgorithmTypeParam_set
 * Description:
 *      Set the distribution algorithm by packet type and algorithm ID from the specified device.
 * Input:
 *      unit         - unit id
 *      type         - packet type
 *      algo_idx     - algorithm index
 *      algo_bitmask - bitmask of the distribution algorithm
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID     - invalid unit id
 *      RT_ERR_NOT_INIT    - The module is not initial
 *      RT_ERR_LA_ALGO_ID  - invalid trunk algorithm ID
 *      RT_ERR_LA_HASHMASK - invalid hash mask
 * Note:
 *      (1) The valid range of algo_idx is 0~1 in 9300 and 9310.
 *      (2) Following factors can be used in any combination to customize the L2 distribution algorithm.
 *          - TRUNK_DISTRIBUTION_ALGO_L2_SPA_BIT        (source port)
 *          - TRUNK_DISTRIBUTION_ALGO_L2_SMAC_BIT       (source mac)
 *          - TRUNK_DISTRIBUTION_ALGO_L2_DMAC_BIT       (destination mac)
 *          - TRUNK_DISTRIBUTION_ALGO_L2_VLAN_BIT        (vlan id)
 *      (3) Following factors can be used in any combination to customize the L3 distribution algorithm.
 *          - TRUNK_DISTRIBUTION_ALGO_L3_SPA_BIT        (source port)
 *          - TRUNK_DISTRIBUTION_ALGO_L3_SMAC_BIT       (source mac)
 *          - TRUNK_DISTRIBUTION_ALGO_L3_DMAC_BIT       (destination mac)
 *          - TRUNK_DISTRIBUTION_ALGO_L3_VLAN_BIT        (vlan id)
 *          - TRUNK_DISTRIBUTION_ALGO_L3_SIP_BIT        (source ip)
 *          - TRUNK_DISTRIBUTION_ALGO_L3_DIP_BIT        (destination ip)
 *          - TRUNK_DISTRIBUTION_ALGO_L3_SRC_L4PORT_BIT (source layer4 port)
 *          - TRUNK_DISTRIBUTION_ALGO_L3_DST_L4PORT_BIT (destination layer4 port)
 *          - TRUNK_DISTRIBUTION_ALGO_L3_PROTO_BIT (protocol)
 *          - TRUNK_DISTRIBUTION_ALGO_L3_FLOW_LABEL_BIT (ipv6 flow label)
 */
int32
dal_longan_trunk_distributionAlgorithmTypeParam_set(uint32 unit, rtk_trunk_hashParamType_t type, uint32 algo_idx, uint32 algo_bitmask)
{
    uint32 val;
    uint32 field = LONGAN_L2_HASH_MSKf;
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, algo_idx=%d, algo_bitmask=%x",
           unit, algo_idx, algo_bitmask);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(algo_idx >= HAL_MAX_NUM_OF_TRUNK_ALGO(unit), RT_ERR_LA_ALGO_ID);
    if(PARAM_TYPE_L2 == type)
        RT_PARAM_CHK((algo_bitmask > TRUNK_DISTRIBUTION_ALGO_L2_MASKALL), RT_ERR_LA_HASHMASK);
    if(PARAM_TYPE_L3 == type)
        RT_PARAM_CHK((algo_bitmask > TRUNK_DISTRIBUTION_ALGO_L3_MASKALL), RT_ERR_LA_HASHMASK);

    TRUNK_SEM_LOCK(unit);

    switch(type)
    {
        case PARAM_TYPE_L2:
            field = LONGAN_L2_HASH_MSKf;
            break;
        case PARAM_TYPE_L3:
            field = LONGAN_L3_HASH_MSKf;
            break;
        default:
            break;
    }

    val = algo_bitmask;
    if ((ret = reg_array_field_write(unit,
                          LONGAN_TRK_HASH_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          algo_idx,
                          field,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
} /* end of dal_longan_trunk_distributionAlgorithmTypeParam_set */




/* Function Name:
 *      dal_longan_trunk_distributionAlgorithmShift_get
 * Description:
 *      Get the shift bits of distribution algorithm parameters from the specified device.
 * Input:
 *      unit     - unit id
 *      algo_idx - algorithm index
 * Output:
 *      pShift   - pointer buffer of shift bits of the distribution algorithm
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_LA_ALGO_ID   - invalid trunk algorithm ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 */
int32
dal_longan_trunk_distributionAlgorithmShiftGbl_get(uint32 unit,  rtk_trunk_distAlgoShift_t *pShift)
{
    int32 ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d ", unit );

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pShift), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);

    /* get entry from CHIP*/
    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_FLOW_LABELf,
                          &pShift->flowLabel_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_PROTOf,
                          &pShift->proto_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }


    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_L4DPORTf,
                          &pShift->dport_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_L4SPORTf,
                          &pShift->sport_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_DIPf,
                          &pShift->dip_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_SIPf,
                          &pShift->sip_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_VLANf,
                          &pShift->vlan_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_DMACf,
                          &pShift->dmac_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_SMACf,
                          &pShift->smac_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_SPPf,
                          &pShift->spa_shift)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_distributionAlgorithmShiftGbl_get */

/* Function Name:
 *      dal_longan_trunk_distributionAlgorithmShift_set
 * Description:
 *      Set the shift bits of distribution algorithm parameters from the specified device.
 * Input:
 *      unit     - unit id
 *      algo_idx - algorithm index
 *      pShift   - shift bits of the distribution algorithm parameters
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_LA_ALGO_ID    - invalid trunk algorithm ID
 *      RT_ERR_LA_ALGO_SHIFT - invalid trunk algorithm shift
 *      RT_ERR_NULL_POINTER  - input parameter may be null pointer
 * Note:
 */
int32
dal_longan_trunk_distributionAlgorithmShiftGbl_set(uint32 unit, rtk_trunk_distAlgoShift_t *pShift)
{
    uint32  val;
    int32 ret;

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);
    /* parameter check */
    RT_PARAM_CHK((NULL == pShift), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK(pShift->dport_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);
    RT_PARAM_CHK(pShift->sport_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);
    RT_PARAM_CHK(pShift->dip_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);
    RT_PARAM_CHK(pShift->sip_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);
    RT_PARAM_CHK(pShift->proto_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);
    RT_PARAM_CHK(pShift->flowLabel_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);
    RT_PARAM_CHK(pShift->vlan_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);
    RT_PARAM_CHK(pShift->dmac_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);
    RT_PARAM_CHK(pShift->smac_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);
    RT_PARAM_CHK(pShift->spa_shift > HAL_TRUNK_ALGO_SHIFT_MAX(unit), RT_ERR_LA_ALGO_SHIFT);

    TRUNK_SEM_LOCK(unit);

    /* set to CHIP*/
    val = pShift->proto_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_PROTOf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }


    val = pShift->flowLabel_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_FLOW_LABELf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }



    val = pShift->dport_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_L4DPORTf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    val = pShift->sport_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_L4SPORTf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    val = pShift->dip_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_DIPf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    val = pShift->sip_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_SIPf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    val = pShift->vlan_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_VLANf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    val = pShift->dmac_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_DMACf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    val = pShift->smac_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_SMACf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    val = pShift->spa_shift;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_SHFT_CTRLr,
                          LONGAN_SHIFT_BITS_SPPf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_distributionAlgorithmShiftGbl_set */

/* Function Name:
 *      dal_longan_trunk_trafficSeparateEnable_get
 * Description:
 *      Get the traffic separation setting of a trunk group from the specified device.
 * Input:
 *      unit           - unit id
 *      trk_gid        - trunk group id
 * Output:
 *      pSeparateType      - pointer to traffic separation type
 * Return:
*      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_TRUNK_ID  - invalid trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) The configurable separation type includes SEPARATE_KNOWN_MULTI, SEPARATE_DLF, and SEPARATE_BCAST.
 *      (2) In 9310 stand-alone mode, the separated port is the link up egress member port with largest index.
 *      (3) In 9310 stacking mode, the separated port could be configured by rtk_trunk_trafficSeparatePort_set().
 */
int32
dal_longan_trunk_trafficSeparateEnable_get(uint32 unit, rtk_trk_t trk_gid, rtk_trunk_separateType_t separateType, rtk_enable_t *pEnable)
{
    int32 ret;
    uint32 sepTypeFieldName = LONGAN_LAG_SEP_KWN_MC_ENtf;
    lag_entry_t lag_entry;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, trk_gid=%d", unit, trk_gid);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);

    /* parameter check */
    if(TRUNK_MODE_STACKING == trunkMode)
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););
    else
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););

    /* get entry from CHIP*/
    if ((ret = table_read(unit, LONGAN_LAGt, trk_gid, (uint32 *) &lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    switch(separateType)
    {
        case SEPARATE_KNOWN_MULTI:
            sepTypeFieldName = LONGAN_LAG_SEP_KWN_MC_ENtf;
            break;
        case SEPARATE_FLOOD:
            sepTypeFieldName = LONGAN_LAG_SEP_DLF_BCAST_ENtf;
            break;
        default:
           TRUNK_SEM_UNLOCK(unit);
           return RT_ERR_INPUT;
    }

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                sepTypeFieldName, pEnable, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }


    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_trafficSeparateEnable_get */


/* Function Name:
 *      dal_longan_trunk_trafficSeparateEnable_set
 * Description:
 *      Set the traffic separation setting of a trunk group from the specified device.
 * Input:
 *      unit         - unit id
 *      trk_gid      - trunk group id
 *      separateType     - traffic separation setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_TRUNK_ID  - invalid trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) The configurable separation type includes SEPARATE_KNOWN_MULTI and SEPARATE_FLOOD.
 *      (2) The separated port is the link up egress member port with largest index.
 */
int32
dal_longan_trunk_trafficSeparateEnable_set(uint32 unit, rtk_trk_t trk_gid, rtk_trunk_separateType_t separateType, rtk_enable_t enable)
{
    int32 ret;
    uint32 sepTypeFieldName = LONGAN_LAG_SEP_KWN_MC_ENtf;
    lag_entry_t lag_entry;

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    TRUNK_SEM_LOCK(unit);

    /* parameter check */
    if(TRUNK_MODE_STACKING == trunkMode)
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););
    else
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););

    if ((ret = table_read(unit, LONGAN_LAGt, trk_gid, (uint32 *) &lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    switch(separateType)
    {
        case SEPARATE_KNOWN_MULTI:
            sepTypeFieldName = LONGAN_LAG_SEP_KWN_MC_ENtf;
            break;
        case SEPARATE_FLOOD:
            sepTypeFieldName = LONGAN_LAG_SEP_DLF_BCAST_ENtf;
            break;
        default:
           TRUNK_SEM_UNLOCK(unit);
           return RT_ERR_INPUT;
    }

    if ((ret = table_field_set(unit, LONGAN_LAGt,
                sepTypeFieldName, &enable, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    if ((ret = table_write(unit, LONGAN_LAGt, trk_gid, (uint32 *) &lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    RT_ERR_CHK(dal_longan_trunk_tbl_refresh(unit), ret);

    return RT_ERR_OK;
} /* end of dal_longan_trunk_trafficSeparate_set */

/* Function Name:
 *      dal_longan_trunk_trafficSeparateDivision_get
 * Description:
 *      Get the division control of known multicast to other separated packets from the specified device.
 * Input:
 *      unit          - unit id
 * Output:
 *      pEnable    - pointer buffer of known multicast packets separation divide control enable status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) This function only works when SEPARATE_KNOWN_MULTI and SEPARATE_FLOOD are both enabled.
 *           In that case, if the division function is enabled, separated DLF or broadcast packet would choose trunk member ports with
 *           largest index to forward; separated known multicast packets would choose second largest index to forward.
 *      (2) This function only works when at least 3 trunk member ports link up.
 */
int32
dal_longan_trunk_trafficSeparateDivision_get(uint32 unit, rtk_enable_t *pEnable)
{
    int32 ret;
    uint32  val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d", unit);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pEnable), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);

    /* get entry from CHIP*/
    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_SEP_PORT_SELf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    if(0 == val)
        *pEnable = DISABLED;
    else
        *pEnable = ENABLED;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_longan_trunk_trafficSeparateDivision_set
 * Description:
 *      Set the division control of known multicast to other separated packets to the specified device.
 * Input:
 *      unit        - unit id
 *      enable    - known multicast packets separation divide control enable status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_INPUT          - invalid input parameter
 * Note:
 *      (1) This function only works when SEPARATE_KNOWN_MULTI and SEPARATE_FLOOD are both enabled.
 *           In that case, if the division function is enabled, separated DLF or broadcast packet would choose trunk member ports with
 *           largest index to forward; separated known multicast packets would choose second largest index to forward.
 *      (2) This function only works when at least 3 trunk member ports link up.
 */
int32
dal_longan_trunk_trafficSeparateDivision_set(uint32 unit, rtk_enable_t enable)
{
    int32 ret;
    uint32  val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d", unit);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((enable != DISABLED && enable != ENABLED), RT_ERR_INPUT);

    TRUNK_SEM_LOCK(unit);

    if(DISABLED == enable)
        val = 0;
    else
        val = 1;

    /* get entry from CHIP*/
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_SEP_PORT_SELf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    RT_ERR_CHK(dal_longan_trunk_tbl_refresh(unit), ret);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_longan_trunk_port_get
 * Description:
 *      Get the members of the trunk id from the specified device.
 * Input:
 *      unit                    - unit id
 *      trk_gid                 - trunk group id
 * Output:
 *      pTrunk_member_portmask - pointer buffer of trunk member ports
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_LA_TRUNK_ID  - invalid trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 */
int32
dal_longan_trunk_port_get(uint32 unit, uint32 trk_gid, rtk_portmask_t *pTrunk_member_portmask)
{
    int32   ret;
    RT_PARAM_CHK((NULL == pTrunk_member_portmask), RT_ERR_NULL_POINTER);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, trk_gid=%d",
           unit, trk_gid);

    ret = dal_longan_trunk_localPort_get (unit, trk_gid, pTrunk_member_portmask);
    if(RT_ERR_OK != ret)
        return ret;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "pTrunk_member_portmask->bits= 0x%x",
           pTrunk_member_portmask->bits[0]);

    return RT_ERR_OK;
} /* end of dal_longan_trunk_port_get */


/* Function Name:
 *      dal_longan_trunk_port_set
 * Description:
 *      Set the members of the trunk id to the specified device.
 * Input:
 *      unit                    - unit id
 *      trk_gid                 - trunk group id
 *      pTrunk_member_portmask - trunk member ports
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_LA_TRUNK_ID       - invalid trunk ID
 *      RT_ERR_NULL_POINTER      - null pointer
 *      RT_ERR_LA_MEMBER_OVERLAP - the specified port mask is overlapped with other group
 *      RT_ERR_LA_PORTMASK       - error port mask
 *      RT_ERR_LA_PORTNUM_NORMAL - it can only aggregate at most eight ports when 802.1ad normal mode
 * Note:
 *      None.
 */
int32
dal_longan_trunk_port_set(uint32 unit, uint32 trk_gid, rtk_portmask_t *pTrunk_member_portmask)
{
    int32   ret;
    rtk_trk_egrPort_t trk_egr_ports;
    rtk_trk_egrPort_t egr_ports_temp;
    rtk_portmask_t origin_mbr;
    rtk_portmask_t portmask_to_remove;
    rtk_portmask_t portmask_to_add;
    uint32 portIdx;

    RT_PARAM_CHK((NULL == pTrunk_member_portmask), RT_ERR_NULL_POINTER);
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, trk_gid=%d pTrunk_member_portmask->bits= 0x%x", unit, trk_gid,
         pTrunk_member_portmask->bits[0]);

    RT_PARAM_CHK(!HWP_PMSK_EXIST(unit, pTrunk_member_portmask), RT_ERR_PORT_MASK);
    RT_PARAM_CHK(RTK_PORTMASK_IS_PORT_SET(*pTrunk_member_portmask, HWP_CPU_MACID(unit)), RT_ERR_PORT_MASK);
    RT_PARAM_CHK((RTK_PORTMASK_GET_PORT_COUNT(*pTrunk_member_portmask) > HAL_MAX_NUM_OF_TRUNKMEMBER(unit)), RT_ERR_LA_PORTNUM_NORMAL);

    RTK_PORTMASK_RESET(origin_mbr);
    RT_ERR_CHK(dal_longan_trunk_localPort_get (unit, trk_gid, &origin_mbr), ret);

    RTK_PORTMASK_RESET(portmask_to_add);
    RTK_PORTMASK_RESET(portmask_to_remove);

    RTK_PORTMASK_ASSIGN(portmask_to_add, *pTrunk_member_portmask);
    RTK_PORTMASK_REMOVE(portmask_to_add, origin_mbr);

    RTK_PORTMASK_ASSIGN(portmask_to_remove, origin_mbr);
    RTK_PORTMASK_REMOVE(portmask_to_remove, *pTrunk_member_portmask);

    /*do remove port first as add member could make number larger than 8*/
    osal_memset(&trk_egr_ports, 0, sizeof(rtk_trk_egrPort_t));
    osal_memset(&egr_ports_temp, 0, sizeof(rtk_trk_egrPort_t));
    RT_ERR_CHK(dal_longan_trunk_egrPort_get(unit, trk_gid, &trk_egr_ports), ret);
    for (portIdx = 0; portIdx < trk_egr_ports.num_ports; portIdx++)
    {
        if ((HAL_UNIT_TO_DEV_ID(unit) == trk_egr_ports.egr_port[portIdx].devID)
            && (RTK_PORTMASK_IS_PORT_SET(portmask_to_remove, trk_egr_ports.egr_port[portIdx].port)))
        {
            /* port need to be removed*/
            //trk_egr_ports.egr_port[portIdx].port = INVALID_TRUNK_MEMBER_PORT;
            continue;
        }
        else
        {
            egr_ports_temp.egr_port[egr_ports_temp.num_ports++] = trk_egr_ports.egr_port[portIdx];
        }

    }

    /*config egress*/
    RT_ERR_CHK(dal_longan_trunk_egrPort_set(unit, trk_gid, &egr_ports_temp), ret);

    /*config ingress*/
    for (portIdx = 0; portIdx < trk_egr_ports.num_ports; portIdx++)
    {
        if ((HAL_UNIT_TO_DEV_ID(unit) == trk_egr_ports.egr_port[portIdx].devID)
            && (RTK_PORTMASK_IS_PORT_SET(portmask_to_remove, trk_egr_ports.egr_port[portIdx].port)))
        {
            RT_ERR_CHK(dal_longan_trunk_srcPortMap_set(unit, trk_egr_ports.egr_port[portIdx], 0, 0), ret);
        }
    }


    /*add port*/
    RT_ERR_CHK(dal_longan_trunk_localPort_set (unit, trk_gid, pTrunk_member_portmask), ret);

    if (0 != RTK_PORTMASK_GET_PORT_COUNT(portmask_to_add))
    {
        for (portIdx = RTK_PORTMASK_GET_FIRST_PORT(portmask_to_add); portIdx <= RTK_PORTMASK_GET_LAST_PORT(portmask_to_add); portIdx++)
        {
            if (RTK_PORTMASK_IS_PORT_SET(portmask_to_add, portIdx))
            {
                egr_ports_temp.egr_port[egr_ports_temp.num_ports].devID = HAL_UNIT_TO_DEV_ID(unit);
                egr_ports_temp.egr_port[egr_ports_temp.num_ports].port = portIdx;
                RT_ERR_CHK(dal_longan_trunk_srcPortMap_set(unit, egr_ports_temp.egr_port[egr_ports_temp.num_ports], 1, trk_gid), ret);
                egr_ports_temp.num_ports++;
            }
        }
    }
    /*config egress*/
    RT_ERR_CHK(dal_longan_trunk_egrPort_set(unit, trk_gid, &egr_ports_temp), ret);


    return RT_ERR_OK;
} /* end of dal_longan_trunk_port_set */

/* Function Name:
 *      dal_longan_trunk_localPort_get
 * Description:
 *      Get the ingress member ports of a trunk id from the specified device.
 * Input:
 *      unit                   - unit id
 *      trk_gid               - trunk group id that contains local trunk member ports
 * Output:
 *      pTrk_local_ports        - pointer buffer of local trunk member ports
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_TRUNK_ID  - invalid trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) The valid range of trk_gid is 0~29 in 9300 stand-alone mode, 0~63 in 9300 stacking mode
 *      (2) The ingress trunk member portmask would be used for L2 learning, source port filtering, and non-unicast packet forwarding.
 *      (3) The ingress portmask are local portmask of the unit. For example, if trunk 10 contains member ports
 *         (unit 0, port 1), (unit 0, port 2) and (unit 3, port1). Local portmask of trunk 10 of unit 0 is port 1 and port 2.
 */
int32
dal_longan_trunk_localPort_get (uint32 unit, rtk_trk_t trk_gid, rtk_portmask_t *pTrk_local_ports)
{
    int32 ret;
    uint32  val;

    RT_PARAM_CHK((NULL == pTrk_local_ports), RT_ERR_NULL_POINTER);
    osal_memset(pTrk_local_ports, 0, sizeof(rtk_portmask_t));

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, trk_gid=%d",
           unit, trk_gid);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    TRUNK_SEM_LOCK(unit);

    /* parameter check */
    if(TRUNK_MODE_STACKING == trunkMode)
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););
    else
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););

    if(INVALID_LOCAL_IDX == trunk2localIdx[unit][trk_gid])
    {
        TRUNK_SEM_UNLOCK(unit);
        return RT_ERR_OK;
    }

    /* get entry from CHIP*/
    if ((ret = reg_array_field_read(unit,
                          LONGAN_TRK_MBR_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          trunk2localIdx[unit][trk_gid],
                          LONGAN_TRK_PPM_28_0f,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }
    pTrk_local_ports->bits[0] = val;

    TRUNK_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "pAlgo_bitmask->bits=0x%x",
           pTrk_local_ports->bits[0]);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_longan_trunk_localPort _set
 * Description:
 *      Set the local member ports of a trunk id from the specified device.
 * Input:
 *      unit                   - unit id
 *      trk_gid               - trunk group id that contains local trunk member ports
 *      pTrk_local_ports         - local trunk member ports
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_TRUNK_ID  - invalid trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 *      RT_ERR_LA_PORTMASK       - error port mask
 * Note:
 *      (1) The valid range of trk_gid is 0~29 in 9300 stand-alone mode, 0~63 in 9300 stacking mode.
 *      (2) The local trunk member portmask would be used for  source port filtering, and non-unicast packet forwarding.
 *      (3) The local portmask are trunk members of the unit. For example, if trunk 10 contains member ports
 *         (unit 0, port 1), (unit 0, port 2) and (unit 3, port1). Local portmask of trunk 10 of unit 0 is port 1 and port 2.
 */
int32
dal_longan_trunk_localPort_set (uint32 unit, rtk_trk_t trk_gid, rtk_portmask_t *pTrk_local_ports)
{
    uint32  val, isTrk, trkId;
    int32 ret;
    uint32  grp_num;
    rtk_port_t port;

    RT_PARAM_CHK((NULL == pTrk_local_ports), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK(!HWP_PMSK_EXIST(unit, pTrk_local_ports), RT_ERR_PORT_MASK);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, trk_gid=%d, pTrk_local_ports->bits=0x%x",
           unit, trk_gid, pTrk_local_ports->bits[0]);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    TRUNK_SEM_LOCK(unit);

    /* parameter check */
    if(TRUNK_MODE_STACKING == trunkMode)
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););
    else
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););

    RT_PARAM_CHK_EHDL((NULL == pTrk_local_ports), RT_ERR_NULL_POINTER,
                                        TRUNK_SEM_UNLOCK(unit););

    RT_PARAM_CHK_EHDL(RTK_PORTMASK_IS_PORT_SET(*pTrk_local_ports, HWP_CPU_MACID(unit)), RT_ERR_PORT_MASK, TRUNK_SEM_UNLOCK(unit););
    RT_PARAM_CHK_EHDL((RTK_PORTMASK_GET_PORT_COUNT(*pTrk_local_ports) > HAL_MAX_NUM_OF_TRUNKMEMBER(unit)), RT_ERR_LA_PORTNUM_NORMAL, TRUNK_SEM_UNLOCK(unit););

    if (RTK_PORTMASK_GET_PORT_COUNT(*pTrk_local_ports) != 0)
    {
        for (port = RTK_PORTMASK_GET_FIRST_PORT(*pTrk_local_ports); port <= RTK_PORTMASK_GET_LAST_PORT(*pTrk_local_ports); port++)
        {
            RT_PARAM_CHK_EHDL((RTK_PORTMASK_IS_PORT_SET(*pTrk_local_ports, port) && !HWP_PORT_EXIST(unit, port)), RT_ERR_LA_PORTMASK,
                                        TRUNK_SEM_UNLOCK(unit););
        }
    }

    /* Not allow different trunk overlap occurs */
    for (grp_num = 0; grp_num < HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit); grp_num++)
    {
        if (grp_num != trk_gid)
        {
            RT_PARAM_CHK_EHDL((pTrk_local_ports->bits[0] & pTrunkMemberSet[unit][grp_num].bits[0]), RT_ERR_LA_MEMBER_OVERLAP,
                                            TRUNK_SEM_UNLOCK(unit););
            RT_PARAM_CHK_EHDL((pTrk_local_ports->bits[1] & pTrunkMemberSet[unit][grp_num].bits[1]), RT_ERR_LA_MEMBER_OVERLAP,
                                            TRUNK_SEM_UNLOCK(unit););
        }
    }

    if(INVALID_LOCAL_IDX == trunk2localIdx[unit][trk_gid]) /*not an exist local trunk*/
    {
        /*clear a non-exist local trunk*/
        if (RTK_PORTMASK_IS_ALL_ZERO(*pTrk_local_ports))
        {
            TRUNK_SEM_UNLOCK(unit);
            return RT_ERR_OK;
        }

        for (grp_num = 0; grp_num < HAL_MAX_NUM_OF_LOCAL_TRUNK(unit); grp_num++)
        {
            if(INVALID_LOCAL_TRUNK_ID == localTrunkID[unit][grp_num]) /*find an empty entry*/
            {
                trunk2localIdx[unit][trk_gid] = grp_num;
                localTrunkID[unit][grp_num] = trk_gid;
                break;
            }
        }
    }

    if(INVALID_LOCAL_IDX == trunk2localIdx[unit][trk_gid]) /*no empty or exist entry. */
    {
        /*This return is not possible to happen*/
        {
            TRUNK_SEM_UNLOCK(unit);
            return RT_ERR_LA_TRUNK_ID;
        }
    }

    /*Set Local PPM*/
    if (RTK_PORTMASK_IS_ALL_ZERO(*pTrk_local_ports))
    {
        isTrk = 0;
    }
    else
    {
        isTrk = 1;
    }
    trkId = trk_gid;

    if ((ret = reg_array_field_write(unit,
                          LONGAN_TRK_ID_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          trunk2localIdx[unit][trk_gid],
                          LONGAN_TRK_VALIDf,
                          &isTrk)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if ((ret = reg_array_field_write(unit,
                          LONGAN_TRK_ID_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          trunk2localIdx[unit][trk_gid],
                          LONGAN_TRK_IDf,
                          &trkId)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    val = pTrk_local_ports->bits[0];
    if ((ret = reg_array_field_write(unit,
                          LONGAN_TRK_MBR_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          trunk2localIdx[unit][trk_gid],
                          LONGAN_TRK_PPM_28_0f,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    if (RTK_PORTMASK_IS_ALL_ZERO(*pTrk_local_ports))
    {
        localTrunkID[unit][trunk2localIdx[unit][trk_gid]] = INVALID_LOCAL_TRUNK_ID;
        trunk2localIdx[unit][trk_gid] = INVALID_LOCAL_IDX;
    }

    /*set source port to trunk ID mapping table*/
    ret = 0;

    HWP_PORT_TRAVS_EXCEPT_CPU(unit, port)
    {
        if (RTK_PORTMASK_IS_PORT_SET(pTrunkMemberSet[unit][trk_gid], port) ||
            RTK_PORTMASK_IS_PORT_SET(*pTrk_local_ports, port))
        {
            if (RTK_PORTMASK_IS_PORT_SET(*pTrk_local_ports, port))
            {
                isTrk = 1;
                trkId = trk_gid;
            }
            else
            {
                isTrk = 0;
                trkId = 0;
            }
            if ((ret = reg_array_field_write(unit,
                                  LONGAN_LOCAL_PORT_TRK_MAPr,
                                  port,
                                  REG_ARRAY_INDEX_NONE,
                                  LONGAN_IS_TRK_MBRf,
                                  &isTrk)) != RT_ERR_OK)
            {
                TRUNK_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
                return ret;
            }
            if ((ret = reg_array_field_write(unit,
                                  LONGAN_LOCAL_PORT_TRK_MAPr,
                                  port,
                                  REG_ARRAY_INDEX_NONE,
                                  LONGAN_TRK_IDf,
                                  &trkId)) != RT_ERR_OK)
            {
                TRUNK_SEM_UNLOCK(unit);
                RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
                return ret;
            }

        }
    }

    RTK_PORTMASK_ASSIGN(pTrunkMemberSet[unit][trk_gid], *pTrk_local_ports);

    TRUNK_SEM_UNLOCK(unit);
    RT_ERR_CHK(dal_longan_trunk_tbl_refresh(unit), ret);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_longan_trunk_egrPort_get
 * Description:
 *      Get the trunk egress candidate ports from the specified device.
 * Input:
 *      unit                   - unit id
 *      trk_gid                - trunk group id
 * Output:
 *      pTrk_egr_ports       - pointer buffer of trunk egress candidate ports
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_TRUNK_ID  - invalid trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) The valid range of trk_gid is 0~29 in 9300 stand-alone mode, 0~63 in 9300 stacking mode.
 *      (2) The traffic sent to trunk group would be hashed and an outgoing port would be picked from egress candidate ports.
 *      (3) Separated traffic or traffic ignoring trunk hash would not follow egress candidate ports to forward.
 */
int32
dal_longan_trunk_egrPort_get(uint32 unit, rtk_trk_t trk_gid, rtk_trk_egrPort_t *pTrk_egr_ports)
{
    uint32  val;
    int32 ret;
    lag_entry_t lag_entry;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, trk_gid=%d",
           unit, trk_gid);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    TRUNK_SEM_LOCK(unit);

    /* parameter check */
    if(TRUNK_MODE_STACKING == trunkMode)
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););
    else
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););

    if ((ret = table_read(unit, LONGAN_LAGt, trk_gid, (uint32 *) &lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_NUM_TX_CANDItf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->num_ports = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV0tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[0].devID = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT0tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[0].port = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV1tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[1].devID = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT1tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[1].port = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV2tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[2].devID = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT2tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[2].port = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV3tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[3].devID = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT3tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[3].port = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV4tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[4].devID = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT4tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[4].port = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV5tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[5].devID = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT5tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[5].port = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV6tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[6].devID = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT6tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[6].port = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV7tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[7].devID = val;

    if ((ret = table_field_get(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT7tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }
    pTrk_egr_ports->egr_port[7].port = val;


    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_longan_trunk_egrPort_set
 * Description:
 *      Set the trunk egress candidate ports to the specified device.
 * Input:
 *      unit                   - unit id
 *      trk_gid                - trunk group id
 *      pTrk_egr_ports         - trunk egress candidate ports
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_TRUNK_ID  - invalid trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) The valid range of trk_gid is 0~29 in 9300 stand-alone mode, 0~63 in 9300 stacking mode.
 *      (2) The traffic sent to trunk group would be hashed and an outgoing port would be picked from egress candidate ports.
 *      (3) Separated traffic or traffic ignoring trunk hash would not follow egress candidate ports to forward.
 */
int32
dal_longan_trunk_egrPort_set(uint32 unit, rtk_trk_t trk_gid, rtk_trk_egrPort_t *pTrk_egr_ports)
{
    uint32  val, i;
    int32 ret;
    lag_entry_t lag_entry;

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);
    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, trk_gid=%d",
           unit, trk_gid);
    RT_PARAM_CHK((NULL == pTrk_egr_ports), RT_ERR_NULL_POINTER);
    for (i = 0; i < pTrk_egr_ports->num_ports; i++)
    {
        RT_PARAM_CHK((HAL_UNIT_TO_DEV_ID(unit) == pTrk_egr_ports->egr_port[i].devID) && (!HWP_PORT_EXIST(unit, pTrk_egr_ports->egr_port[i].port)), RT_ERR_PORT_ID);
        RT_PARAM_CHK((HAL_UNIT_TO_DEV_ID(unit) == pTrk_egr_ports->egr_port[i].devID) && (HWP_IS_CPU_PORT(unit, pTrk_egr_ports->egr_port[i].port)), RT_ERR_PORT_ID);
    }

    TRUNK_SEM_LOCK(unit);

    /* parameter check */
    if(TRUNK_MODE_STACKING == trunkMode)
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););
    else
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););

    for (i = pTrk_egr_ports->num_ports; i < HAL_MAX_NUM_OF_TRUNKMEMBER(unit); i++)
    {
        pTrk_egr_ports->egr_port[i].devID = HAL_UNIT_TO_DEV_ID(unit);
        pTrk_egr_ports->egr_port[i].port = INVALID_TRUNK_MEMBER_PORT;
    }

    if ((ret = table_read(unit, LONGAN_LAGt, trk_gid, (uint32 *) &lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->num_ports;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_NUM_TX_CANDItf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[0].devID;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV0tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[0].port;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT0tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[1].devID;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV1tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[1].port;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT1tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[2].devID;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV2tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[2].port;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT2tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[3].devID;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV3tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[3].port;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT3tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[4].devID;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV4tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[4].port;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT4tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[5].devID;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV5tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[5].port;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT5tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[6].devID;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV6tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[6].port;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT6tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[7].devID;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_DEV7tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    val = pTrk_egr_ports->egr_port[7].port;
    if ((ret = table_field_set(unit, LONGAN_LAGt,
                LONGAN_LAG_TRK_PORT7tf, &val, (uint32 *)&lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    if ((ret = table_write(unit, LONGAN_LAGt, trk_gid, (uint32 *) &lag_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    for (i = 0; i < RTK_MAX_NUM_OF_TRUNKMON_CB; i++)
    {
        if (pTrunkMon_cb->trunkMon_callback_f[i] != NULL)
        {
            (pTrunkMon_cb->trunkMon_callback_f[i])(unit, trk_gid, pTrk_egr_ports);
        }
    }

    TRUNK_SEM_UNLOCK(unit);

    RT_ERR_CHK(dal_longan_trunk_tbl_refresh(unit), ret);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_longan_trunk_stkTrkPort_get
 * Description:
 *      Get the stacking portmask of designated trunk group from the specified device.
 * Input:
 *      unit          - unit id
 *      stk_trk_gid     - stacking trunk group id
 * Output:
 *      pStkPorts - pointer buffer of stacking portmask of trunk group
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_STACK_TRUNK_ID  - invalid stacking trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) The valid range of stk_trk_gid is from 0 ~ 1.
 *      (2) The stacking portmask indicates if stacking ports from port ID 24 to 27
 *           belong to the trunk group.
 */
int32
dal_longan_trunk_stkTrkPort_get(uint32 unit, rtk_stk_trk_t stk_trk_gid, rtk_portmask_t *pStkPorts)
{
    int32 ret;
    uint32 val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, stk_trk_gid=%d",
           unit, stk_trk_gid);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(stk_trk_gid >= HAL_MAX_NUM_OF_STACK_TRUNK(unit), RT_ERR_LA_STACK_TRUNK_ID);
    RT_PARAM_CHK((NULL == pStkPorts ), RT_ERR_NULL_POINTER);
    osal_memset(pStkPorts, 0, sizeof(rtk_portmask_t));

    TRUNK_SEM_LOCK(unit);
    /* get entry from CHIP*/
    if ((ret = reg_array_field_read(unit,
                          LONGAN_TRK_STK_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          stk_trk_gid,
                          LONGAN_STK_TRK_PPMf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    rt_util_upinkPort_reg2Mask(unit, &val, pStkPorts);

    TRUNK_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "pStkPorts bits=0x%x",
           pStkPorts->bits[0]);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_longan_trunk_stkTrkPort_set
 * Description:
 *      Get the stacking portmask of designated trunk group from the specified device.
 * Input:
 *      unit          - unit id
 *      stk_trk_gid     - stacking trunk group id
 *      pStkPorts  - pointer to stacking portmask of trunk group
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_STACK_TRUNK_ID  - invalid stacking trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) The valid range of stk_trk_gid is from 0 ~ 1.
 *      (2) The stacking portmask indicates if stacking ports from port ID 24 to 27
 *           belong to the trunk group.
 */
int32
dal_longan_trunk_stkTrkPort_set(uint32 unit, rtk_stk_trk_t stk_trk_gid, rtk_portmask_t *pStkPorts)
{
    int32 ret;
    uint32 val;

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(stk_trk_gid >= HAL_MAX_NUM_OF_STACK_TRUNK(unit), RT_ERR_LA_STACK_TRUNK_ID);
    RT_PARAM_CHK((NULL == pStkPorts ), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK(!HWP_PMSK_EXIST(unit, pStkPorts), RT_ERR_PORT_MASK);

    RT_PARAM_CHK((!HWP_PMSK_EXIST_ATTRI(unit, pStkPorts, HWP_CASCADE, HWP_OPERATION_OR) && !HAL_STACK_PORTMASK_CHK(unit, pStkPorts)), RT_ERR_PORT_MASK);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, stk_trk_gid=%d, pStkPorts= 0x%x",
           unit, stk_trk_gid, pStkPorts->bits[0]);

    TRUNK_SEM_LOCK(unit);

    /* get entry from CHIP*/
    val = 0;
    rt_util_upinkPort_mask2Reg(unit, pStkPorts, &val);

    if ((ret = reg_array_field_write(unit,
                          LONGAN_TRK_STK_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          stk_trk_gid,
                          LONGAN_STK_TRK_PPMf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_longan_trunk_stkTrkHash_get
 * Description:
 *      Get the option that if stacking trunk would follow the hash result in stacking header or re-calculate hash by its own distribution algorithm ID
 * Input:
 *      unit      - unit id
 * Output:
 *      pStkTrkHash - pointer buffer of the option that if stacking trunk would follow the hash result in stacking header or re-calculate hash by its own distribution algorithm ID
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) If STACK_TRK_HASH_KEEP is selected, the hash result in stacking header would be applied to select stacking trunk outgoing port.
 *      (2) If STACK_TRK_HASH_RECALCULATE is selected, use rtk_trunk_stkDistributionAlgorithmBind_set() to set the distribution algorithm ID.
 *           The recalculated hash result only decides the stacking trunk outgoing port and would not alter the hash result in stacking header.
 */
int32
dal_longan_trunk_stkTrkHash_get(uint32 unit, rtk_trunk_stkTrkHash_t *pStkTrkHash)
{
    int32 ret;
    uint32  val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d", unit);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pStkTrkHash), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);
    /* get entry from CHIP*/
    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_STK_HASH_CALf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    if(0 == val)
        *pStkTrkHash = STACK_TRK_HASH_KEEP;
    else
        *pStkTrkHash = STACK_TRK_HASH_RECALCULATE;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_longan_trunk_stkTrkHash_set
 * Description:
 *      Set the option that if stacking trunk would follow the hash result in stacking header or re-calculate hash by its own hash mask index
 * Input:
 *      unit     - unit id
 *      stkTrkHash - the option that if stacking trunk would follow the hash result in stacking header or re-calculate hash by its own hash mask index
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID     - invalid unit id
 *      RT_ERR_NOT_INIT    - The module is not initial
 *      RT_ERR_INPUT          - invalid input parameter
 * Note:
 *      (1) If STACK_TRK_HASH_KEEP is selected, the hash result in stacking header would be applied to select stacking trunk outgoing port.
 *      (2) If STACK_TRK_HASH_RECALCULATE is selected, use rtk_trunk_stkDistributionAlgorithmBind_set() to set the distribution algorithm ID.
 *           The recalculated hash result only decides the stacking trunk outgoing port and would not alter the hash result in stacking header.
 */
int32
dal_longan_trunk_stkTrkHash_set(uint32 unit, rtk_trunk_stkTrkHash_t stkTrkHash)
{
    int32 ret;
    uint32  val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d", unit);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((stkTrkHash > STACK_TRK_HASH_END), RT_ERR_INPUT);

    TRUNK_SEM_LOCK(unit);

    if(STACK_TRK_HASH_KEEP == stkTrkHash)
        val = 0;
    else
        val = 1;

    /* get entry from CHIP*/
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_STK_HASH_CALf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_longan_trunk_stkDistributionAlgorithmTypeBind_get
 * Description:
 *      Get the distribution algorithm ID binding of a stacking trunk group from the specified device.
 * Input:
 *      unit      - unit id
 *      stk_trk_gid     - trunk group id of stacking port
 * Output:
 *      pAlgo_idx - pointer buffer of the distribution algorithm ID
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_LA_STACK_TRUNK_ID  - invalid stacking port trunk ID
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      (1) The valid range fo stk_trk_gid is from 0 ~ 1.
 *      (2) The valid range of algo_idx is 0~3
 */
int32
dal_longan_trunk_stkDistributionAlgorithmTypeBind_get(uint32 unit, rtk_stk_trk_t stk_trk_gid, rtk_trunk_bindType_t type, uint32 *pAlgo_idx)
{
    int32 ret;
    uint32 val;
    uint32 field;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, type=%d, stk_trk_gid=%d",
           unit, type, stk_trk_gid);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(stk_trk_gid >= HAL_MAX_NUM_OF_STACK_TRUNK(unit), RT_ERR_LA_STACK_TRUNK_ID);
    RT_PARAM_CHK(type >= BIND_TYPE_END, RT_ERR_INPUT);
    RT_PARAM_CHK((NULL == pAlgo_idx ), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);

    switch(type)
    {
        case BIND_TYPE_L2:
            field = LONGAN_L2_HASH_MSK_IDXf;
            break;
        case BIND_TYPE_IPV4:
            field = LONGAN_IP4_HASH_MSK_IDXf;
            break;
        case BIND_TYPE_IPV6:
            field = LONGAN_IP6_HASH_MSK_IDXf;
            break;
        default:
            field = LONGAN_L2_HASH_MSK_IDXf;
            break;
    }

    /* get entry from CHIP*/
    if ((ret = reg_array_field_read(unit,
                          LONGAN_TRK_STK_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          stk_trk_gid,
                          field,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    *pAlgo_idx = val;

    TRUNK_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "*pAlgo_idx=%x", *pAlgo_idx);

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_longan_trunk_stkDistributionAlgorithmTypeBind_set
 * Description:
 *      Set the distribution algorithm ID binding of a stacking trunk group to the specified device.
 * Input:
 *      unit     - unit id
 *      stk_trk_gid   - trunk group id of stacking port
 *      algo_idx - index the distribution algorithm
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID     - invalid unit id
 *      RT_ERR_NOT_INIT    - The module is not initial
 *      RT_ERR_LA_STACK_TRUNK_ID  - invalid stacking port trunk ID
 *      RT_ERR_LA_ALGO_ID  - invalid trunk algorithm ID
 * Note:
 *      (1) The valid range fo stk_trk_gid is from 0 ~ 1.
 *      (2) The valid range of algo_idx is 0~3
 */
int32
dal_longan_trunk_stkDistributionAlgorithmTypeBind_set(uint32 unit, rtk_stk_trk_t stk_trk_gid, rtk_trunk_bindType_t type,uint32 algo_idx)
{
    int32 ret;
    uint32 val;
    uint32 field;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, stk_trk_gid=%d, algo_idx=%d",
           unit, stk_trk_gid, algo_idx);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK(stk_trk_gid >= HAL_MAX_NUM_OF_STACK_TRUNK(unit), RT_ERR_LA_STACK_TRUNK_ID);
    RT_PARAM_CHK(type >= BIND_TYPE_END, RT_ERR_INPUT);
    RT_PARAM_CHK(algo_idx >= HAL_MAX_NUM_OF_TRUNK_ALGO(unit), RT_ERR_LA_ALGO_ID);

    TRUNK_SEM_LOCK(unit);
    /* get entry from CHIP*/

    switch(type)
    {
        case BIND_TYPE_L2:
            field = LONGAN_L2_HASH_MSK_IDXf;
            break;
        case BIND_TYPE_IPV4:
            field = LONGAN_IP4_HASH_MSK_IDXf;
            break;
        case BIND_TYPE_IPV6:
            field = LONGAN_IP6_HASH_MSK_IDXf;
            break;
        default:
            field = LONGAN_L2_HASH_MSK_IDXf;
            break;
    }

    val = algo_idx;

    if ((ret = reg_array_field_write(unit,
                          LONGAN_TRK_STK_CTRLr,
                          REG_ARRAY_INDEX_NONE,
                          stk_trk_gid,
                          field,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    RT_ERR_CHK(dal_longan_trunk_tbl_refresh(unit), ret);

    return RT_ERR_OK;

}
/*
 * Function Declaration
 *      dal_longan_trunk_localFirst_get
 * Description:
 *      Get the local-first load-balacing enable status from the specified device.
 * Input:
 *      unit          - unit id
 * Output:
 *      pLocalFirst - pointer to local-first load-balancing enable status
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
dal_longan_trunk_localFirst_get(uint32 unit, rtk_enable_t *pLocalFirst)
{
    int32 ret;
    uint32 val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d", unit);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pLocalFirst), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);

    /* get entry from CHIP*/
     if ((ret = reg_field_read(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_LOCAL_FIRSTf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    *pLocalFirst = val;

    TRUNK_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "*pLocalFirst=%x", *pLocalFirst);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_localFirst_get */

/* Function Name:
 *      dal_longan_trunk_localFirst_set
 * Description:
 *      Set the local-first load-balacing enable status to the specified device.
 * Input:
 *      unit        - unit id
 *      localFirst - local-first load-balancing enable status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID     - invalid unit id
 *      RT_ERR_NOT_INIT    - The module is not initial
 *      RT_ERR_INPUT       - Invalid input parameter
 * Note:
 *      Local first load balancing only works in stacking mode
 */
int32
dal_longan_trunk_localFirst_set(uint32 unit, rtk_enable_t localFirst)
{
    int32 ret;
    uint32 val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, localFirst=%d", unit, localFirst);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((localFirst != DISABLED && localFirst != ENABLED), RT_ERR_INPUT);

    TRUNK_SEM_LOCK(unit);
    /* get entry from CHIP*/
    val = localFirst;

    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_LOCAL_FIRSTf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_localFirst_set */

/* Function Name:
 *      dal_longan_trunk_localFirstFailOver_get
 * Description:
 *      Get the local-first load balacing congest and link-fail avoidance enable status from the specified device.
 * Input:
 *      unit          - unit id
 * Output:
 *      pCongstAvoid - pointer to congest avoidance enable status
 *      pLinkFailAvoid - pointer to link fail avoidance enable status
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      the failover funtion only works when local-first load balancing is enabled
 */
int32
dal_longan_trunk_localFirstFailOver_get(uint32 unit, rtk_enable_t *pCongstAvoid, rtk_enable_t *pLinkFailAvoid)
{
    int32 ret;
    uint32 val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d", unit);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pCongstAvoid), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pLinkFailAvoid), RT_ERR_NULL_POINTER);

    TRUNK_SEM_LOCK(unit);

    /* get entry from CHIP*/
    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_CONGST_AVOIDf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    *pCongstAvoid = val;

    if ((ret = reg_field_read(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_LINK_DOWN_AVOIDf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    *pLinkFailAvoid = val;

    TRUNK_SEM_UNLOCK(unit);

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "*pCongstAvoid=%x *pLinkFailAvoid=%x", *pCongstAvoid, *pLinkFailAvoid);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_localFirstFailOver_get */

/* Function Name:
 *      dal_longan_trunk_localFirstFailOver_set
 * Description:
 *      Set the local-first load balacing congest and link-fail avoidance enable status to the specified device.
 * Input:
 *      unit          - unit id
 *      congstAvoid - congest avoidance enable status
 *      linkFailAvoid - link fail avoidance enable status
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_UNIT_ID      - invalid unit id
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *      the failover funtion only works when local-first load balancing is enabled
 */
int32
dal_longan_trunk_localFirstFailOver_set(uint32 unit, rtk_enable_t congstAvoid, rtk_enable_t linkFailAvoid)
{
    int32 ret;
    uint32 val;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d, congstAvoid=%d, linkFailAvoid=%d", unit, congstAvoid, linkFailAvoid);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((congstAvoid != DISABLED && congstAvoid != ENABLED), RT_ERR_INPUT);
    RT_PARAM_CHK((linkFailAvoid != DISABLED && linkFailAvoid != ENABLED), RT_ERR_INPUT);

    TRUNK_SEM_LOCK(unit);
    /* get entry from CHIP*/
    val = congstAvoid;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_CONGST_AVOIDf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    val = linkFailAvoid;
    if ((ret = reg_field_write(unit,
                          LONGAN_TRK_CTRLr,
                          LONGAN_LINK_DOWN_AVOIDf,
                          &val)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_DAL|MOD_TRUNK), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_localFirstFailOver_set */


/* Function Name:
 *      dal_longan_trunk_srcPortMap_get
 * Description:
 *      Get the info about whether DEV+Port belongs to some trunk and if yes, get its trunk ID.
 * Input:
 *      unit                   - unit id
 *      devPort             - device port
 * Output:
 *      pIsTrkMbr - pointer to get trunk or not
 *      pTrk_gid   - pointer to get trunk id.
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *
 */
int32
dal_longan_trunk_srcPortMap_get(uint32 unit,  rtk_dev_port_t devPort, uint32 *pIsTrkMbr, rtk_trk_t *pTrk_gid)
{
    int32   ret;
    uint32 tbl_idx;
    uint32 is_trk;
    uint32 trk_id;
    lag_src_map_entry_t lag_src_map_entry;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d,devPort=%d:%d", unit, devPort.devID, devPort.port);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);

    /* parameter check */
    RT_PARAM_CHK((NULL == pIsTrkMbr), RT_ERR_NULL_POINTER);
    RT_PARAM_CHK((NULL == pTrk_gid), RT_ERR_NULL_POINTER);

    /* function body */
    TRUNK_SEM_LOCK(unit);

    /*set source port to trunk ID mapping table*/
    tbl_idx = devPort.devID * MAX_NUM_OF_PORT_PER_DEV_SUPPORT + devPort.port;

    if ((ret = table_read(unit, LONGAN_SRC_TRK_MAPt, tbl_idx, (uint32 *) &lag_src_map_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    if ((ret = table_field_get(unit, LONGAN_SRC_TRK_MAPt,
                LONGAN_SRC_TRK_MAP_TRK_VALIDtf, &is_trk, (uint32 *)&lag_src_map_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    *pIsTrkMbr = (is_trk == 0) ? 0 : 0x1;

    if ((ret = table_field_get(unit, LONGAN_SRC_TRK_MAPt,
                LONGAN_SRC_TRK_MAP_TRK_IDtf, &trk_id, (uint32 *)&lag_src_map_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    *pTrk_gid = trk_id;

    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_srcPortMap_get */

/* Function Name:
 *      dal_longan_trunk_srcPortMap_set
 * Description:
 *      Set the info about whether DEV+Port belongs to some trunk and if yes, set its trunk ID.
 * Input:
 *      unit                   - unit id
 *      devPort             - device port
 *      isTrkMbr            - trunk or not
 *      trk_gid              -  trunk id.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT     - The module is not initial
 *      RT_ERR_NULL_POINTER - input parameter may be null pointer
 * Note:
 *
 */
int32
dal_longan_trunk_srcPortMap_set(uint32 unit, rtk_dev_port_t devPort, uint32 isTrkMbr, rtk_trk_t trk_gid)
{
    int32   ret;
    uint32 tbl_idx;
    uint32 is_trk;
    lag_src_map_entry_t lag_src_map_entry;

    RT_LOG(LOG_DEBUG, (MOD_DAL|MOD_TRUNK), "unit=%d,devPort=%d,isTrkMbr=%d,trk_gid=%d", unit, devPort, isTrkMbr, trk_gid);

    /* check Init status */
    RT_INIT_CHK(trunk_init[unit]);
    RT_PARAM_CHK((HAL_UNIT_TO_DEV_ID(unit) == devPort.devID) && (!HWP_PORT_EXIST(unit, devPort.port)), RT_ERR_PORT_ID);
    RT_PARAM_CHK((HAL_UNIT_TO_DEV_ID(unit) == devPort.devID) && (HWP_IS_CPU_PORT(unit, devPort.port)), RT_ERR_PORT_ID);

    TRUNK_SEM_LOCK(unit);

    /* parameter check */
    if(TRUNK_MODE_STACKING == trunkMode)
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK_IN_STACKING_MODE(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););
    else
        RT_PARAM_CHK_EHDL(trk_gid >= HAL_MAX_NUM_OF_TRUNK(unit), RT_ERR_LA_TRUNK_ID,
                                        TRUNK_SEM_UNLOCK(unit););

    /*set source port to trunk ID mapping table*/
    tbl_idx = devPort.devID * MAX_NUM_OF_PORT_PER_DEV_SUPPORT + devPort.port;

    is_trk = (isTrkMbr == 0) ? 0 : 0x1;

    if ((ret = table_read(unit, LONGAN_SRC_TRK_MAPt, tbl_idx, (uint32 *) &lag_src_map_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    if ((ret = table_field_set(unit, LONGAN_SRC_TRK_MAPt,
                LONGAN_SRC_TRK_MAP_TRK_VALIDtf, &is_trk, (uint32 *)&lag_src_map_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    if ((ret = table_field_set(unit, LONGAN_SRC_TRK_MAPt,
                LONGAN_SRC_TRK_MAP_TRK_IDtf, &trk_gid, (uint32 *)&lag_src_map_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    if ((ret = table_write(unit, LONGAN_SRC_TRK_MAPt, tbl_idx, (uint32 *) &lag_src_map_entry)) != RT_ERR_OK)
    {
        TRUNK_SEM_UNLOCK(unit);
        RT_ERR(ret, (MOD_TRUNK|MOD_DAL), "");
        return ret;
    }

    TRUNK_SEM_UNLOCK(unit);

    return RT_ERR_OK;
}   /* end of dal_longan_trunk_srcPortMap_set */





/* Function Name:
 *      _dal_longan_trunk_init_config
 * Description:
 *      Initialize default configuration for trunk module of the specified device.
 * Input:
 *      unit - unit id
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      Must initialize trunk module before calling this API
 */
static int32
_dal_longan_trunk_init_config(uint32 unit)
{
    //add code later if need
    return RT_ERR_OK;
} /* end of _dal_longan_trunk_init_config */

