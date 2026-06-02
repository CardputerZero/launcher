/**
 * @file bmi270_port.c
 * @brief BMI270 Linux I2C 用户空间移植层实现 (M5CardputerZero)
 * @note  使用 /dev/i2c-x + ioctl(I2C_RDWR) 实现带 Repeated Start 的连续读写
 */

#include "bmi270_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

/* 静态文件句柄，生命周期与进程相同 */
static int i2c_fd = -1;

/* I2C 读回调：I2C_RDWR 保证写寄存器地址和读数据之间是 Repeated Start */
static int8_t bmi270_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    (void)intf_ptr;

    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data msgset;

    msgs[0].addr  = BMI270_I2C_ADDR;
    msgs[0].flags = 0;              /* 写方向 */
    msgs[0].len   = 1;
    msgs[0].buf   = &reg_addr;

    msgs[1].addr  = BMI270_I2C_ADDR;
    msgs[1].flags = I2C_M_RD;       /* 读方向 */
    msgs[1].len   = len;
    msgs[1].buf   = reg_data;

    msgset.msgs  = msgs;
    msgset.nmsgs = 2;

    if (ioctl(i2c_fd, I2C_RDWR, &msgset) < 0) {
        perror("[BMI270] i2c read ioctl");
        return BMI2_E_COM_FAIL;
    }
    return BMI2_OK;
}

/* I2C 写回调 */
static int8_t bmi270_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    (void)intf_ptr;

    uint8_t *buf = malloc(len + 1);
    if (!buf) return BMI2_E_COM_FAIL;

    buf[0] = reg_addr;
    memcpy(buf + 1, reg_data, len);

    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data msgset;

    msg.addr  = BMI270_I2C_ADDR;
    msg.flags = 0;
    msg.len   = len + 1;
    msg.buf   = buf;

    msgset.msgs  = &msg;
    msgset.nmsgs = 1;

    int8_t rslt = BMI2_OK;
    if (ioctl(i2c_fd, I2C_RDWR, &msgset) < 0) {
        perror("[BMI270] i2c write ioctl");
        rslt = BMI2_E_COM_FAIL;
    }

    free(buf);
    return rslt;
}

/* 微秒延时回调 */
static void bmi270_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    usleep(period);
}

int8_t bmi270_port_init(struct bmi2_dev *dev)
{
    if (!dev) return BMI2_E_NULL_PTR;

    i2c_fd = open(BMI270_I2C_BUS, O_RDWR);
    if (i2c_fd < 0) {
        perror("[BMI270] open i2c bus");
        return BMI2_E_COM_FAIL;
    }

    /* 上电等待：至少 100~200ms */
    usleep(200000);

    dev->intf           = BMI2_I2C_INTF;
    dev->read           = bmi270_i2c_read;
    dev->write          = bmi270_i2c_write;
    dev->delay_us       = bmi270_delay_us;
    dev->intf_ptr       = &i2c_fd;
    dev->read_write_len = 46;

    return BMI2_OK;
}

void bmi270_port_deinit(struct bmi2_dev *dev)
{
    (void)dev;
    if (i2c_fd >= 0) {
        close(i2c_fd);
        i2c_fd = -1;
    }
}

void bmi270_print_result(int8_t rslt)
{
    switch (rslt) {
        case BMI2_OK: break;
        case BMI2_E_NULL_PTR:      printf("[BMI270] Error: Null pointer\n"); break;
        case BMI2_E_COM_FAIL:      printf("[BMI270] Error: Communication failure\n"); break;
        case BMI2_E_DEV_NOT_FOUND: printf("[BMI270] Error: Device not found\n"); break;
        case BMI2_E_INVALID_SENSOR:printf("[BMI270] Error: Invalid sensor\n"); break;
        case BMI2_E_CONFIG_LOAD:   printf("[BMI270] Error: Config load error\n"); break;
        default:                   printf("[BMI270] Error: Unknown code %d\n", rslt); break;
    }
}
