/**
 * @file bmm150_aux_port.c
 * @brief BMM150 通过 BMI270 AUX 接口访问的适配层实现
 */

#include "bmm150_aux_port.h"
#include <unistd.h>

/* 全局引用 BMI270 实例，供 AUX 读写回调使用 */
static struct bmi2_dev *g_bmi270 = NULL;

/*! AUX 读回调：将 BMM150 的寄存器读请求转发给 BMI270 AUX 手动模式 */
static BMM150_INTF_RET_TYPE bmm150_aux_read(uint8_t reg_addr, uint8_t *reg_data,
                                             uint32_t len, void *intf_ptr)
{
    (void)intf_ptr;

    if (!g_bmi270) {
        return BMM150_E_NULL_PTR;
    }

    int8_t rslt = bmi2_read_aux_man_mode(reg_addr, reg_data, (uint16_t)len, g_bmi270);
    return (rslt == BMI2_OK) ? BMM150_INTF_RET_SUCCESS : BMM150_E_COM_FAIL;
}

/*! AUX 写回调：将 BMM150 的寄存器写请求转发给 BMI270 AUX 手动模式 */
static BMM150_INTF_RET_TYPE bmm150_aux_write(uint8_t reg_addr, const uint8_t *reg_data,
                                              uint32_t len, void *intf_ptr)
{
    (void)intf_ptr;

    if (!g_bmi270) {
        return BMM150_E_NULL_PTR;
    }

    int8_t rslt = bmi2_write_aux_man_mode(reg_addr, reg_data, (uint16_t)len, g_bmi270);
    return (rslt == BMI2_OK) ? BMM150_INTF_RET_SUCCESS : BMM150_E_COM_FAIL;
}

/*! 延时回调 */
static void bmm150_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    usleep(period);
}

/***************************************************************************/

int8_t bmm150_aux_port_init(struct bmm150_dev *dev, struct bmi2_dev *bmi)
{
    if (!dev || !bmi) {
        return BMM150_E_NULL_PTR;
    }

    g_bmi270 = bmi;

    dev->read     = bmm150_aux_read;
    dev->write    = bmm150_aux_write;
    dev->delay_us = bmm150_delay_us;
    dev->intf     = BMM150_I2C_INTF;
    dev->intf_ptr = (void *)0x01;  /* dummy non-NULL, not used by callbacks */

    return bmm150_init(dev);
}

void bmm150_aux_port_deinit(struct bmm150_dev *dev)
{
    (void)dev;
    g_bmi270 = NULL;
}
