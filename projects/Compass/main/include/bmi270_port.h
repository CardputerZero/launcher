/**
 * @file bmi270_port.h
 * @brief BMI270 Linux I2C 用户空间移植层 (M5CardputerZero)
 */

#ifndef BMI270_PORT_H_
#define BMI270_PORT_H_

#include "bmi2.h"
#include "bmi270.h"

/* I2C 配置 */
#define BMI270_I2C_BUS      "/dev/i2c-1"    /* M5CardputerZero 默认 I2C 总线 */
#define BMI270_I2C_ADDR     0x68            /* SDO=GND: 0x68, SDO=VDDIO: 0x69 */

#ifdef __cplusplus
extern "C" {
#endif

int8_t bmi270_port_init(struct bmi2_dev *dev);
void bmi270_port_deinit(struct bmi2_dev *dev);
void bmi270_print_result(int8_t rslt);

#ifdef __cplusplus
}
#endif

#endif /* BMI270_PORT_H_ */
