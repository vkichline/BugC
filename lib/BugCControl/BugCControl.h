#pragma once
#include <stdint.h>

// Adapted from github.com/m5stack/M5StickC/blob/master/examples/Hat/BUGC by Sorzn
// By Van Kichline, in the year of the plague
//
// The motors of the BugC are arrainged like:
//   1      3
//   0      2
// ...where left is the front of the BugC
// Speed varies from -100 to 100.


#define BUGC_ADDR 0x38

#define BUGC_FRONT_LEFT_MOTOR   0
#define BUGC_FRONT_RIGHT_MOTOR  1
#define BUGC_REAR_LEFT_MOTOR    2
#define BUGC_REAR_RIGHT_MOTOR   3
#define BUGC_LEFT_LIGHT         0
#define BUGC_RIGHT_LIGHT        1
#define BUGC_NUM_MOTORS         4
#define BUGC_NUM_LIGHTS         2


class BugCControl {
  public:
    static void     set_speed(uint8_t pos, int8_t speed);
    static void     set_all_speeds(int8_t speed_0, int8_t speed_1, int8_t speed_2, int8_t speed_3);
    static void     come_to_halt();
    static void     set_lights(uint32_t color_left, uint32_t color_right);
    static int8_t   get_speed(uint8_t pos);
    static int32_t  get_color(uint8_t pos);
    static void     display_speed(uint8_t motor, int8_t speed);
  protected:
    static uint8_t  speeds[4];
    static uint32_t lights[2];
};
