#ifndef IMU_H
#define IMU_H

void imu_init(void);
void imu_close(void);
int imu_update(float* yaw, float* pitch, float* roll);

#endif
