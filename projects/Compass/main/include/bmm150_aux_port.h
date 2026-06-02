/**
 * @file bmm150_aux_port.h
 * @brief BMM150 通过 BMI270 AUX 接口访问的适配层
 *
 * 将 BMM150 SensorAPI 的 read/write 回调映射到 BMI270 的 AUX 手动模式 API，
 * 使 BMM150 驱动无需修改即可通过 BMI270 间接访问。
 */

#ifndef BMM150_AUX_PORT_H
#define BMM150_AUX_PORT_H

#include <stdint.h>
#include "bmi2.h"
#include "bmm150.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 BMM150（通过 BMI270 AUX 接口）
 *
 * @param[out] dev   BMM150 设备结构体
 * @param[in]  bmi   已初始化完成的 BMI270 设备结构体
 *
 * @return BMM150_OK (0) 成功，负值失败
 */
int8_t bmm150_aux_port_init(struct bmm150_dev *dev, struct bmi2_dev *bmi);

/**
 * @brief 反初始化 BMM150 AUX 适配层
 */
void bmm150_aux_port_deinit(struct bmm150_dev *dev);

#ifdef __cplusplus
}
#endif

#endif /* BMM150_AUX_PORT_H */
