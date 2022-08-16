/*
Copyright 2022 aki27

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "quantum.h"
#include "cocot46plus.h"
#include "wait.h"
#include "debug.h"


// Invert vertical scroll direction
#ifndef COCOT_SCROLL_INV_DEFAULT
#    define COCOT_SCROLL_INV_DEFAULT 1
#endif

#ifndef COCOT_CPI_OPTIONS
#    define COCOT_CPI_OPTIONS { 250, 500, 750, 1000, 1250 }
#    ifndef COCOT_CPI_DEFAULT
#       define COCOT_CPI_DEFAULT 4
#    endif
#endif
#ifndef COCOT_CPI_DEFAULT
#    define COCOT_CPI_DEFAULT 4
#endif

#ifndef COCOT_SCROLL_DIVIDERS
#    define COCOT_SCROLL_DIVIDERS { 1, 2, 3, 4, 5, 6 }
#    ifndef COCOT_SCROLL_DIV_DEFAULT
#       define COCOT_SCROLL_DIV_DEFAULT 4
#    endif
#endif
#ifndef COCOT_SCROLL_DIV_DEFAULT
#    define COCOT_SCROLL_DIV_DEFAULT 4
#endif


#ifndef COCOT_ROTATION_ANGLE
#    define COCOT_ROTATION_ANGLE { -60, -45, -30, -15, 0, 15, 30, 45, 60 }
#    ifndef COCOT_ROTATION_DEFAULT
#       define COCOT_ROTATION_DEFAULT 2
#    endif
#endif
#ifndef COCOT_ROTATION_DEFAULT
#    define COCOT_ROTATION_DEFAULT 2
#endif

#define TIMES (1000)
#define DEGS 90
#define m_sin( x ) (sin(x * (M_PI / 180) * -1) * TIMES)

const int32_t sin_table[] = {
    m_sin(0),m_sin(1),m_sin(2),m_sin(3),m_sin(4),m_sin(5),m_sin(6),m_sin(7),m_sin(8),m_sin(9),m_sin(10),
    m_sin(11),m_sin(12),m_sin(13),m_sin(14),m_sin(15),m_sin(16),m_sin(17),m_sin(18),m_sin(19),m_sin(20),
    m_sin(21),m_sin(22),m_sin(23),m_sin(24),m_sin(25),m_sin(26),m_sin(27),m_sin(28),m_sin(29),m_sin(30),
    m_sin(31),m_sin(32),m_sin(33),m_sin(34),m_sin(35),m_sin(36),m_sin(37),m_sin(38),m_sin(39),m_sin(40),
    m_sin(41),m_sin(42),m_sin(43),m_sin(44),m_sin(45),m_sin(46),m_sin(47),m_sin(48),m_sin(49),m_sin(50),
    m_sin(51),m_sin(52),m_sin(53),m_sin(54),m_sin(55),m_sin(56),m_sin(57),m_sin(58),m_sin(59),m_sin(60),
    m_sin(61),m_sin(62),m_sin(63),m_sin(64),m_sin(65),m_sin(66),m_sin(67),m_sin(68),m_sin(69),m_sin(70),
    m_sin(71),m_sin(72),m_sin(73),m_sin(74),m_sin(75),m_sin(76),m_sin(77),m_sin(78),m_sin(79),m_sin(80),
    m_sin(81),m_sin(82),m_sin(83),m_sin(84),m_sin(85),m_sin(86),m_sin(87),m_sin(88),m_sin(89),m_sin(90)
};

#define _SIN(x) pickup_sin_table((x<0)*2, x)
#define _COS(x) pickup_sin_table(3, x)

cocot_config_t cocot_config;
uint16_t cpi_array[] = COCOT_CPI_OPTIONS;
uint16_t scrl_div_array[] = COCOT_SCROLL_DIVIDERS;
uint16_t angle_array[] = COCOT_ROTATION_ANGLE;
#define CPI_OPTION_SIZE (sizeof(cpi_array) / sizeof(uint16_t))
#define SCRL_DIV_SIZE (sizeof(scrl_div_array) / sizeof(uint16_t))
#define ANGLE_SIZE (sizeof(angle_array) / sizeof(int16_t))


// Trackball State
bool     BurstState        = false;  // init burst state for Trackball module
uint16_t MotionStart       = 0;      // Timer for accel, 0 is resting state

// Scroll Accumulation
static int16_t h_acm       = 0;
static int16_t v_acm       = 0;


int32_t pickup_sin_table(int16_t add, int16_t deg) {
    uint16_t degree = abs(deg);
    uint16_t deg90 = degree % DEGS;
    int32_t ret;
    switch ( (degree / DEGS + add) % 4 ) {
    case 0:
        ret = sin_table[deg90]; break;
    case 1:
        ret = sin_table[DEGS-deg90]; break;
    case 2:
        ret = -sin_table[deg90]; break;
    case 3:
        ret = -sin_table[DEGS-deg90]; break;
    }
    return ret;
}

void pointing_device_init_kb(void) {
    // set the CPI.
    pointing_device_set_cpi(cpi_array[cocot_config.cpi_idx]);
}


report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {

    int16_t angle = angle_array[cocot_config.rotation_angle];
    int32_t sin = _SIN(angle);
    int32_t cos = _COS(angle);
    int8_t x_rev = (+ mouse_report.x * cos - mouse_report.y * sin)/TIMES;
    int8_t y_rev = (+ mouse_report.x * sin + mouse_report.y * cos)/TIMES;

    if (cocot_get_scroll_mode()) {
        // accumulate scroll
        h_acm += x_rev * cocot_config.scrl_inv;
        v_acm += y_rev * cocot_config.scrl_inv * -1;

        int8_t h_rev = h_acm >> scrl_div_array[cocot_config.scrl_div];
        int8_t v_rev = v_acm >> scrl_div_array[cocot_config.scrl_div];

        // clear accumulated scroll on assignment

        if (h_rev != 0) {
            if (mouse_report.h + h_rev > 127) {
                h_rev = 127  - mouse_report.h;
            } else if (mouse_report.h + h_rev < -127) {
                h_rev = -127 - mouse_report.h;
            }
            mouse_report.h += h_rev;
            h_acm -= h_rev << scrl_div_array[cocot_config.scrl_div];
        }
        if (v_rev != 0) {
            if (mouse_report.v + v_rev > 127) {
                v_rev = 127 - mouse_report.v;
            } else if (mouse_report.v + v_rev < -127) {
                v_rev = -127 - mouse_report.v;
            }
            mouse_report.v += v_rev;
            v_acm -= v_rev << scrl_div_array[cocot_config.scrl_div];
        }

        mouse_report.x = 0;
        mouse_report.y = 0;
    } else {
        mouse_report.x = x_rev;
        mouse_report.y = y_rev;
    }

    return pointing_device_task_user(mouse_report);
}



bool process_record_kb(uint16_t keycode, keyrecord_t* record) {
    // xprintf("KL: kc: %u, col: %u, row: %u, pressed: %u\n", keycode, record->event.key.col, record->event.key.row, record->event.pressed);
    
    if (!process_record_user(keycode, record)) return false;

    switch (keycode) {
#ifndef MOUSEKEY_ENABLE
        // process KC_MS_BTN1~8 by myself
        // See process_action() in quantum/action.c for details.
        case KC_MS_BTN1 ... KC_MS_BTN8: {
            extern void register_button(bool, enum mouse_buttons);
            register_button(record->event.pressed, MOUSE_BTN_MASK(keycode - KC_MS_BTN1));
            return false;
        }
#endif

    }

    if (keycode == CPI_SW && record->event.pressed) {
        cocot_config.cpi_idx = (cocot_config.cpi_idx + 1) % CPI_OPTION_SIZE;
        eeconfig_update_kb(cocot_config.raw);
        pointing_device_set_cpi(cpi_array[cocot_config.cpi_idx]);
    }

    if (keycode == SCRL_SW && record->event.pressed) {
        cocot_config.scrl_div = (cocot_config.scrl_div + 1) % SCRL_DIV_SIZE;
        eeconfig_update_kb(cocot_config.raw);
    }
    
    if (keycode == ROT_R15 && record->event.pressed) {
        cocot_config.rotation_angle = (cocot_config.rotation_angle + 1) % ANGLE_SIZE;
        eeconfig_update_kb(cocot_config.raw);
    }

    if (keycode == ROT_L15 && record->event.pressed) {
        cocot_config.rotation_angle = (ANGLE_SIZE + cocot_config.rotation_angle - 1) % ANGLE_SIZE;
        eeconfig_update_kb(cocot_config.raw);
    }

    if (keycode == SCRL_IN && record->event.pressed) {
        cocot_config.scrl_inv = - cocot_config.scrl_inv;
        eeconfig_update_kb(cocot_config.raw);
    }

    if (keycode == SCRL_TO && record->event.pressed) {
        { cocot_config.scrl_mode ^= 1; }
    }

    if (keycode == SCRL_MO) {
        { cocot_config.scrl_mode ^= 1; }
    }

    return true;
}


void eeconfig_init_kb(void) {
    cocot_config.cpi_idx = COCOT_CPI_DEFAULT;
    cocot_config.scrl_div = COCOT_SCROLL_DIV_DEFAULT;
    cocot_config.rotation_angle = COCOT_ROTATION_DEFAULT;
    cocot_config.scrl_inv = COCOT_SCROLL_INV_DEFAULT;
    cocot_config.scrl_mode = false;
    eeconfig_update_kb(cocot_config.raw);
    eeconfig_init_user();
}


void matrix_init_kb(void) {
    // is safe to just read CPI setting since matrix init
    // comes before pointing device init.
    cocot_config.raw = eeconfig_read_kb();
    if (cocot_config.cpi_idx > CPI_OPTION_SIZE) // || cocot_config.scrl_div > SCRL_DIV_SIZE || cocot_config.rotation_angle > ANGLE_SIZE)
    {
        eeconfig_init_kb();
    }
    matrix_init_user();
}


bool cocot_get_scroll_mode(void) {
    return cocot_config.scrl_mode;
}

void cocot_set_scroll_mode(bool mode) {
    cocot_config.scrl_mode = mode;
}



// OLED utility
#ifdef OLED_ENABLE
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return OLED_ROTATION_0;
}
#endif
