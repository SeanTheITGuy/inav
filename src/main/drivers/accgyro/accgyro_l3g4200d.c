/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include <platform.h>

#include "drivers/system.h"
#include "drivers/time.h"
#include "drivers/bus_i2c.h"

#include "common/maths.h"
#include "common/axis.h"

#include "drivers/sensor.h"
#include "drivers/accgyro/accgyro.h"
#include "drivers/accgyro/accgyro_l3g4200d.h"

// L3G4200D, Standard address 0x68
#define L3G4200D_ADDRESS         0x68
#define L3G4200D_ID              0xD3
#define L3G4200D_AUTOINCR        0x80

// Registers
#define L3G4200D_WHO_AM_I        0x0F
#define L3G4200D_CTRL_REG1       0x20
#define L3G4200D_CTRL_REG2       0x21
#define L3G4200D_CTRL_REG3       0x22
#define L3G4200D_CTRL_REG4       0x23
#define L3G4200D_CTRL_REG5       0x24
#define L3G4200D_REFERENCE       0x25
#define L3G4200D_STATUS_REG      0x27
#define L3G4200D_GYRO_OUT        0x28

// Bits
#define L3G4200D_POWER_ON        0x0F
#define L3G4200D_FS_SEL_2000DPS  0xF0
#define L3G4200D_DLPF_32HZ       0x00
#define L3G4200D_DLPF_54HZ       0x40
#define L3G4200D_DLPF_78HZ       0x80
#define L3G4200D_DLPF_93HZ       0xC0

static void l3g4200dInit(gyroDev_t *gyro)
{
    bool ack;

    uint8_t mpuLowPassFilter = L3G4200D_DLPF_32HZ;

    switch (gyro->lpf) {
        default:
        case 3:     // BITS_DLPF_CFG_42HZ
            mpuLowPassFilter = L3G4200D_DLPF_32HZ;
            break;
        case 2:     // BITS_DLPF_CFG_98HZ
            mpuLowPassFilter = L3G4200D_DLPF_54HZ;
            break;
        case 1:     // BITS_DLPF_CFG_188HZ
            mpuLowPassFilter = L3G4200D_DLPF_78HZ;
            break;
        case 0:     // BITS_DLPF_CFG_256HZ
            mpuLowPassFilter = L3G4200D_DLPF_93HZ;
            break;
    }

    delay(100);
    ack = busWrite(gyro->busDev, L3G4200D_CTRL_REG4, L3G4200D_FS_SEL_2000DPS);
    if (!ack) {
        failureMode(FAILURE_ACC_INIT);
    }

    delay(5);

    busWrite(gyro->busDev, L3G4200D_CTRL_REG1, L3G4200D_POWER_ON | mpuLowPassFilter);
}

// Read 3 gyro values into user-provided buffer. No overrun checking is done.
static bool l3g4200dRead(gyroDev_t *gyro)
{
    uint8_t buf[6];

    if (!busReadBuf(gyro->busDev, L3G4200D_AUTOINCR | L3G4200D_GYRO_OUT, buf, 6)) {
        return false;
    }

    gyro->gyroADCRaw[X] = (int16_t)((buf[0] << 8) | buf[1]);
    gyro->gyroADCRaw[Y] = (int16_t)((buf[2] << 8) | buf[3]);
    gyro->gyroADCRaw[Z] = (int16_t)((buf[4] << 8) | buf[5]);

    return true;
}


static bool deviceDetect(busDevice_t * busDev)
{
    busSetSpeed(busDev, BUS_SPEED_INITIALIZATION);

    for (int retry = 0; retry < 5; retry++) {
        uint8_t deviceid;

        delay(150);

        bool ack = busRead(busDev, L3G4200D_WHO_AM_I, &deviceid);
        if (ack && deviceid == L3G4200D_ID) {
            return true;
        }
    };

    return false;
}

bool l3g4200dDetect(gyroDev_t *gyro)
{
    gyro->busDev = busDeviceInit(BUSTYPE_ANY, DEVHW_L3G4200, gyro->imuSensorToUse, OWNER_MPU);
    if (gyro->busDev == NULL) {
        return false;
    }

    if (!deviceDetect(gyro->busDev)) {
        busDeviceDeInit(gyro->busDev);
        return false;
    }

    gyro->initFn = l3g4200dInit;
    gyro->readFn = l3g4200dRead;
    gyro->scale = 1.0f / 14.2857f;      // 14.2857dps/lsb scalefactor
    gyro->gyroAlign = gyro->busDev->param;

    return true;
}
