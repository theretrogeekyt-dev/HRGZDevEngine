#ifndef __INPUT_INPUT_H__
#define __INPUT_INPUT_H__

#include <stdint.h>

struct controller_data_s
{
    int s1_x, s1_y;
    int s2_x, s2_y;
    int lt, rt;
    int a, b, x, y;
    int lb, rb;
    int start, back;
    int stick_lb, stick_rb;
    int up, down, left, right;
    int logo;
};

int get_controller_data(struct controller_data_s *ctrl, int port);

#endif
