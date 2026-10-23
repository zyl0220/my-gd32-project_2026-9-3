/* 电流/电压坐标变换与 SVPWM。Q15 的 32767 表示标幺值 1；
 * 角度 65536 对应一整圈。查表插值避免在 20 kHz ISR 调用 sinf/cosf。 */
#include "motor/foc_math.h"
#include "motor/motor_param.h"

/* 65 entries cover a quarter turn; interpolate between adjacent entries. */
static const int16_t sine_quarter[65] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602,
    6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767
};

static int16_t saturate_q15(int32_t value)
{
    if (value > 32767) return 32767;
    if (value < -32767) return -32767;
    return (int16_t)value;
}

static int16_t sin_q15(uint16_t angle)
{
    uint16_t quarter = angle & 0x3FFFU;
    uint16_t quadrant = angle >> 14;
    uint16_t position = (quadrant & 1U) ? (uint16_t)(0x4000U - quarter) : quarter;
    uint16_t index = position >> 8;
    uint16_t fraction = position & 0xFFU;
    int32_t value = sine_quarter[index];
    if (index < 64U) {
        value += ((int32_t)(sine_quarter[index + 1U] - sine_quarter[index]) *
                  fraction + 128) >> 8;
    }
    return (quadrant >= 2U) ? (int16_t)-value : (int16_t)value;
}

void motor_sincos(uint16_t angle, int16_t *sin_out, int16_t *cos_out)
{
    *sin_out = sin_q15(angle);
    *cos_out = sin_q15((uint16_t)(angle + 16384U));
}

motor_ab_t motor_clarke(int16_t ia, int16_t ib)
{
    /* 两电阻采样：Ic = -(Ia + Ib)，β = (Ia + 2Ib)/sqrt(3)。 */
    motor_ab_t result;
    result.alpha = ia;
    result.beta = saturate_q15(((int32_t)ia + 2 * (int32_t)ib) * 18919 / 32768);
    return result;
}

motor_dq_t motor_park(motor_ab_t ab, uint16_t angle)
{
    int16_t s, c;
    motor_dq_t result;
    motor_sincos(angle, &s, &c);
    result.d = saturate_q15(((int32_t)ab.alpha * c + (int32_t)ab.beta * s) >> 15);
    result.q = saturate_q15(((-(int32_t)ab.alpha * s) + (int32_t)ab.beta * c) >> 15);
    return result;
}

motor_ab_t motor_inv_park(motor_dq_t dq, uint16_t angle)
{
    int16_t s, c;
    motor_ab_t result;
    motor_sincos(angle, &s, &c);
    result.alpha = saturate_q15(((int32_t)dq.d * c - (int32_t)dq.q * s) >> 15);
    result.beta = saturate_q15(((int32_t)dq.d * s + (int32_t)dq.q * c) >> 15);
    return result;
}

static uint16_t duty_permille(int32_t phase)
{
    int32_t duty = (16384 + phase) * 1000 / 32768;
    if (duty < (int32_t)MOTOR_DUTY_MIN_PERMILLE)
        return MOTOR_DUTY_MIN_PERMILLE;
    if (duty > (int32_t)MOTOR_DUTY_MAX_PERMILLE)
        return MOTOR_DUTY_MAX_PERMILLE;
    return (uint16_t)duty;
}

motor_duty_t motor_svpwm(motor_ab_t voltage)
{
    /* 零序注入：从三相参考电压减去 (最大值+最小值)/2，
     * 再映射到以 50% 为零电压的三相占空比。 */
    int32_t a = voltage.alpha;
    int32_t b = (-a + ((int32_t)voltage.beta * 56756 >> 15)) / 2;
    int32_t c = (-a - ((int32_t)voltage.beta * 56756 >> 15)) / 2;
    int32_t max = a > b ? a : b;
    int32_t min = a < b ? a : b;
    int32_t offset;
    motor_duty_t result;
    if (c > max) max = c;
    if (c < min) min = c;
    offset = (max + min) / 2;
    result.u = duty_permille(a - offset);
    result.v = duty_permille(b - offset);
    result.w = duty_permille(c - offset);
    return result;
}

/* 整数平方根，避免20kHz路径引入浮点库；返回floor(sqrt(value))。 */
static uint32_t sqrt_u64(uint64_t value)
{
    uint64_t result = 0U;
    uint64_t bit = (uint64_t)1U << 62;
    while (bit > value) bit >>= 2;
    while (bit != 0U) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)result;
}

motor_dq_t motor_voltage_limit(int32_t d, int32_t q, int16_t limit_q15)
{
    uint64_t magnitude2 = (uint64_t)((int64_t)d * d) +
                          (uint64_t)((int64_t)q * q);
    uint32_t magnitude;
    motor_dq_t result = {0, 0};
    if (limit_q15 <= 0) return result;
    if (magnitude2 <= (uint64_t)((int32_t)limit_q15 * limit_q15)) {
        result.d = (int16_t)d;
        result.q = (int16_t)q;
        return result;
    }
    magnitude = sqrt_u64(magnitude2);
    /* 向上取模长，保证除法舍入后的矢量在电压圆内。 */
    if ((uint64_t)magnitude * magnitude < magnitude2) ++magnitude;
    result.d = (int16_t)((int64_t)d * limit_q15 / magnitude);
    result.q = (int16_t)((int64_t)q * limit_q15 / magnitude);
    return result;
}
