#include "button_map.h"
#include "usb_device_drivers.h"
#include "utils.h"
#include "wiimote.h"

#define DS_TOUCHPAD_W		1920
#define DS_TOUCHPAD_H		1080
#define DS_ACC_RES_PER_G	8192

struct ds_input_report {
    u8 report_id;
    u8 left_x;
    u8 left_y;
    u8 right_x;
    u8 right_y;
    u8 l_trigger;
    u8 r_trigger;

    u8 seq_number;

    u8 triangle : 1;
    u8 circle   : 1;
    u8 cross    : 1;
    u8 square   : 1;
    u8 dpad	    : 4;

    u8 r3      : 1;
    u8 l3      : 1;
    u8 options : 1;
    u8 create  : 1;
    u8 r2      : 1;
    u8 l2      : 1;
    u8 r1      : 1;
    u8 l1      : 1;

    u8 btn_pad1 : 5;
    u8 mic_mute : 1;
    u8 tpad     : 1;
    u8 ps       : 1;

    u8 btn_pad2;

    u8 reserved[4];

    union {
        s16 pitch;
        s16 gyro_x;
    };

    union {
        s16 yaw;
        s16 gyro_y;
    };

    union {
        s16 roll;
        s16 gyro_z;
    };

    s16 accel_x;
    s16 accel_y;
    s16 accel_z;

    s32 sensor_timestamp;

    u8 reserved2;

    u8 touch1_contact;
    u8 touch1_x_lo;
    u8 touch1_y_lo : 4;
    u8 touch1_x_hi : 4;
    u8 touch1_y_hi;

    u8 touch2_contact;
    u8 touch2_x_lo;
    u8 touch2_y_lo : 4;
    u8 touch2_x_hi : 4;
    u8 touch2_y_hi;

    u8 reserved3[12];

    u8 status;

    u8 reserved4[10];
} ATTRIBUTE_PACKED;

enum ds_buttons_e {
    DS_BUTTON_TRIANGLE,
    DS_BUTTON_CIRCLE,
    DS_BUTTON_CROSS,
    DS_BUTTON_SQUARE,
    DS_BUTTON_UP,
    DS_BUTTON_DOWN,
    DS_BUTTON_LEFT,
    DS_BUTTON_RIGHT,
    DS_BUTTON_R3,
    DS_BUTTON_L3,
DS_BUTTON_OPTIONS,
    DS_BUTTON_CREATE,
    DS_BUTTON_R2,
    DS_BUTTON_L2,
    DS_BUTTON_R1,
    DS_BUTTON_L1,
    DS_BUTTON_TOUCHPAD,
    DS_BUTTON_PS,
    DS_BUTTON_MIC_MUTE,
    DS_BUTTON__NUM
};

enum ds_analog_axis_e {
    DS_ANALOG_AXIS_LEFT_X,
    DS_ANALOG_AXIS_LEFT_Y,
    DS_ANALOG_AXIS_RIGHT_X,
    DS_ANALOG_AXIS_RIGHT_Y,
    DS_ANALOG_AXIS__NUM
};

struct ds_private_data_t {
    struct {
        u32 buttons;
        u8 analog_axis[DS_ANALOG_AXIS__NUM];
        s16 acc_x, acc_y, acc_z;
        struct {
            u16 x, y;
        } fingers[2];
        u8 num_fingers;
    } input;
    enum bm_ir_emulation_mode_e ir_emu_mode;
    struct bm_ir_emulation_state_t ir_emu_state;
    u8 mapping;
    u8 ir_emu_mode_idx;
    u8 leds;
    bool rumble_on;
    bool switch_mapping;
    bool switch_ir_emu_mode;
};
static_assert(sizeof(struct ds_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

#define SWITCH_MAPPING_COMBO		(BIT(DS_BUTTON_L1) | BIT(DS_BUTTON_L3))
#define SWITCH_IR_EMU_MODE_COMBO	(BIT(DS_BUTTON_R1) | BIT(DS_BUTTON_R3))

static const struct {
    enum wiimote_ext_e extension;
    u16 wiimote_button_map[DS_BUTTON__NUM];
    u8 nunchuk_button_map[DS_BUTTON__NUM];
    u8 nunchuk_analog_axis_map[DS_ANALOG_AXIS__NUM];
    u16 classic_button_map[DS_BUTTON__NUM];
    u8 classic_analog_axis_map[DS_ANALOG_AXIS__NUM];
} input_mappings[] = {
    {
        .extension = WIIMOTE_EXT_NUNCHUK,
        .wiimote_button_map = {
            [DS_BUTTON_TRIANGLE] = WIIMOTE_BUTTON_ONE,
            [DS_BUTTON_CIRCLE]   = WIIMOTE_BUTTON_B,
            [DS_BUTTON_CROSS]    = WIIMOTE_BUTTON_A,
            [DS_BUTTON_SQUARE]   = WIIMOTE_BUTTON_TWO,
            [DS_BUTTON_UP]       = WIIMOTE_BUTTON_UP,
            [DS_BUTTON_DOWN]     = WIIMOTE_BUTTON_DOWN,
            [DS_BUTTON_LEFT]     = WIIMOTE_BUTTON_LEFT,
            [DS_BUTTON_RIGHT]    = WIIMOTE_BUTTON_RIGHT,
            [DS_BUTTON_OPTIONS]  = WIIMOTE_BUTTON_PLUS,
            [DS_BUTTON_CREATE]   = WIIMOTE_BUTTON_MINUS,
            [DS_BUTTON_TOUCHPAD] = WIIMOTE_BUTTON_A,
            [DS_BUTTON_PS]       = WIIMOTE_BUTTON_HOME,
        },
        .nunchuk_button_map = {
            [DS_BUTTON_L1] = NUNCHUK_BUTTON_C,
            [DS_BUTTON_L2] = NUNCHUK_BUTTON_Z,
        },
        .nunchuk_analog_axis_map = {
            [DS_ANALOG_AXIS_LEFT_X] = BM_NUNCHUK_ANALOG_AXIS_X,
            [DS_ANALOG_AXIS_LEFT_Y] = BM_NUNCHUK_ANALOG_AXIS_Y,
        },
    },
    {
        .extension = WIIMOTE_EXT_CLASSIC,
        .classic_button_map = {
            [DS_BUTTON_TRIANGLE] = CLASSIC_CTRL_BUTTON_X,
            [DS_BUTTON_CIRCLE]   = CLASSIC_CTRL_BUTTON_A,
            [DS_BUTTON_CROSS]    = CLASSIC_CTRL_BUTTON_B,
            [DS_BUTTON_SQUARE]   = CLASSIC_CTRL_BUTTON_Y,
            [DS_BUTTON_UP]       = CLASSIC_CTRL_BUTTON_UP,
            [DS_BUTTON_DOWN]     = CLASSIC_CTRL_BUTTON_DOWN,
            [DS_BUTTON_LEFT]     = CLASSIC_CTRL_BUTTON_LEFT,
            [DS_BUTTON_RIGHT]    = CLASSIC_CTRL_BUTTON_RIGHT,
            [DS_BUTTON_OPTIONS]  = CLASSIC_CTRL_BUTTON_PLUS,
            [DS_BUTTON_CREATE]   = CLASSIC_CTRL_BUTTON_MINUS,
            [DS_BUTTON_R2]       = CLASSIC_CTRL_BUTTON_ZR,
            [DS_BUTTON_L2]       = CLASSIC_CTRL_BUTTON_ZL,
            [DS_BUTTON_R1]       = CLASSIC_CTRL_BUTTON_FULL_R,
            [DS_BUTTON_L1]       = CLASSIC_CTRL_BUTTON_FULL_L,
            [DS_BUTTON_TOUCHPAD] = CLASSIC_CTRL_BUTTON_A,
            [DS_BUTTON_PS]       = CLASSIC_CTRL_BUTTON_HOME,
        },
        .classic_analog_axis_map = {
            [DS_ANALOG_AXIS_LEFT_X]  = BM_CLASSIC_ANALOG_AXIS_LEFT_X,
            [DS_ANALOG_AXIS_LEFT_Y]  = BM_CLASSIC_ANALOG_AXIS_LEFT_Y,
            [DS_ANALOG_AXIS_RIGHT_X] = BM_CLASSIC_ANALOG_AXIS_RIGHT_X,
            [DS_ANALOG_AXIS_RIGHT_Y] = BM_CLASSIC_ANALOG_AXIS_RIGHT_Y,
        },
    },
};

static const u8 ir_analog_axis_map[DS_ANALOG_AXIS__NUM] = {
    [DS_ANALOG_AXIS_RIGHT_X] = BM_IR_AXIS_X,
    [DS_ANALOG_AXIS_RIGHT_Y] = BM_IR_AXIS_Y,
};

static const enum bm_ir_emulation_mode_e ir_emu_modes[] = {
    BM_IR_EMULATION_MODE_DIRECT,
    BM_IR_EMULATION_MODE_RELATIVE_ANALOG_AXIS,
    BM_IR_EMULATION_MODE_ABSOLUTE_ANALOG_AXIS,
};

static inline void ds_get_buttons(const struct ds_input_report *report, u32 *buttons)
{
    u32 mask = 0;

#define MAP(field, button) \
	if (report->field) \
		mask |= BIT(button);

    MAP(triangle, DS_BUTTON_TRIANGLE)
    MAP(circle, DS_BUTTON_CIRCLE)
    MAP(cross, DS_BUTTON_CROSS)
    MAP(square, DS_BUTTON_SQUARE)

    if (report->dpad == 0 || report->dpad == 1 || report->dpad == 7)
        mask |= BIT(DS_BUTTON_UP);
    else if (report->dpad == 3 || report->dpad == 4 || report->dpad == 5)
        mask |= BIT(DS_BUTTON_DOWN);
    if (report->dpad == 5 || report->dpad == 6 || report->dpad == 7)
        mask |= BIT(DS_BUTTON_LEFT);
    else if (report->dpad == 1 || report->dpad == 2 || report->dpad == 3)
        mask |= BIT(DS_BUTTON_RIGHT);

    MAP(r3, DS_BUTTON_R3)
    MAP(l3, DS_BUTTON_L3)
    MAP(options, DS_BUTTON_OPTIONS)
    MAP(create, DS_BUTTON_CREATE)
    MAP(r2, DS_BUTTON_R2)
    MAP(l2, DS_BUTTON_L2)
    MAP(r1, DS_BUTTON_R1)
    MAP(l1, DS_BUTTON_L1)
    MAP(tpad, DS_BUTTON_TOUCHPAD)
    MAP(ps, DS_BUTTON_PS)
    MAP(mic_mute, DS_BUTTON_MIC_MUTE)
#undef MAP

    *buttons = mask;
}

static inline void ds_get_analog_axis(const struct ds_input_report *report,
                                       u8 analog_axis[static DS_ANALOG_AXIS__NUM])
{
    analog_axis[DS_ANALOG_AXIS_LEFT_X] = report->left_x;
    analog_axis[DS_ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
    analog_axis[DS_ANALOG_AXIS_RIGHT_X] = report->right_x;
    analog_axis[DS_ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
}

static inline int ds_set_leds_rumble(usb_input_device_t *device, u8 r, u8 g, u8 b,
                                      u8 rumble_small, u8 rumble_large)
{
    u8 buf[] = {
        0x02, // Report ID
        0x03, 0x04, // Valid flags
        rumble_small, // Fast motor
        rumble_large, // Slow motor
        0x00, 0x00, 0x00, 0x00, // Reserved
        0x00, // Mute button LED
        0x00, // Power save control (Mic)
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, // Reserved
        0x01, // Valid flag (LED)
        0x00, 0x00, // Reserved
        0x00, // LED setup
        0x00, // LED brightness
        0x04, // Player LED, set to 1
        r, g, b, // RGB
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00 // Reserved
    };

    return usb_device_driver_issue_intr_transfer(device, 1, buf, sizeof(buf));
}

static inline int ds_request_data(usb_input_device_t *device)
{
    return usb_device_driver_issue_intr_transfer_async(device, 0, device->usb_async_resp,
                                                       sizeof(device->usb_async_resp));
}

static int ds_driver_update_leds_rumble(usb_input_device_t *device)
{
    struct ds_private_data_t *priv = (void *)device->private_data;
    u8 index;

    static const u8 colors[5][3] = {
        {  0,   0,   0},
        {  0,   0,  32},
        { 32,   0,   0},
        {  0,  32,   0},
        { 32,   0,  32},
    };

    index = priv->leds % ARRAY_SIZE(colors);

    u8 r = colors[index][0],
        g = colors[index][1],
        b = colors[index][2];

    return ds_set_leds_rumble(device, r, g, b, priv->rumble_on * 192, 0);
}

bool ds_driver_ops_probe(u16 vid, u16 pid)
{
    static const struct device_id_t compatible[] = {
        {SONY_VID, 0x05c4},
        {SONY_VID, 0x0ce6},
    };

    return usb_driver_is_comaptible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

int ds_driver_ops_init(usb_input_device_t *device, u16 vid, u16 pid)
{
    struct ds_private_data_t *priv = (void *)device->private_data;

    /* Init private state */
    priv->ir_emu_mode_idx = 0;
    bm_ir_emulation_state_reset(&priv->ir_emu_state);
    priv->mapping = 0;
    priv->leds = 0;
    priv->rumble_on = false;
    priv->switch_mapping = false;
    priv->switch_ir_emu_mode = false;

    /* Set initial extension */
    fake_wiimote_set_extension(device->wiimote, input_mappings[priv->mapping].extension);

    return ds_request_data(device);
}

int ds_driver_ops_disconnect(usb_input_device_t *device)
{
    struct ds_private_data_t *priv = (void *)device->private_data;

    priv->leds = 0;
    priv->rumble_on = false;

    return ds_driver_update_leds_rumble(device);
}

int ds_driver_ops_slot_changed(usb_input_device_t *device, u8 slot)
{
    struct ds_private_data_t *priv = (void *)device->private_data;

    priv->leds = slot;

    return ds_driver_update_leds_rumble(device);
}

int ds_driver_ops_set_rumble(usb_input_device_t *device, bool rumble_on)
{
    struct ds_private_data_t *priv = (void *)device->private_data;

    priv->rumble_on = rumble_on;

    return ds_driver_update_leds_rumble(device);
}

bool ds_report_input(usb_input_device_t *device)
{
    struct ds_private_data_t *priv = (void *)device->private_data;
    u16 wiimote_buttons = 0;
    u16 acc_x, acc_y, acc_z;
    union wiimote_extension_data_t extension_data;
    struct ir_dot_t ir_dots[IR_MAX_DOTS];
    enum bm_ir_emulation_mode_e ir_emu_mode;

    if (bm_check_switch_mapping(priv->input.buttons, &priv->switch_mapping, SWITCH_MAPPING_COMBO)) {
        priv->mapping = (priv->mapping + 1) % ARRAY_SIZE(input_mappings);
        fake_wiimote_set_extension(device->wiimote, input_mappings[priv->mapping].extension);
        return false;
    } else if (bm_check_switch_mapping(priv->input.buttons, &priv->switch_ir_emu_mode, SWITCH_IR_EMU_MODE_COMBO)) {
        priv->ir_emu_mode_idx = (priv->ir_emu_mode_idx + 1) % ARRAY_SIZE(ir_emu_modes);
        bm_ir_emulation_state_reset(&priv->ir_emu_state);
    }

    bm_map_wiimote(DS_BUTTON__NUM, priv->input.buttons,
                   input_mappings[priv->mapping].wiimote_button_map,
                   &wiimote_buttons);

    /* Normalize to accelerometer calibration configuration */
    acc_x = ACCEL_ZERO_G - ((s32)priv->input.acc_x * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS_ACC_RES_PER_G;
    acc_y = ACCEL_ZERO_G + ((s32)priv->input.acc_z * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS_ACC_RES_PER_G;
    acc_z = ACCEL_ZERO_G + ((s32)priv->input.acc_y * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS_ACC_RES_PER_G;

    fake_wiimote_report_accelerometer(device->wiimote, acc_x, acc_y, acc_z);

    ir_emu_mode = ir_emu_modes[priv->ir_emu_mode_idx];
    if (ir_emu_mode == BM_IR_EMULATION_MODE_NONE) {
        bm_ir_dots_set_out_of_screen(ir_dots);
    } else {
        if (ir_emu_mode == BM_IR_EMULATION_MODE_DIRECT) {
            bm_map_ir_direct(priv->input.num_fingers,
                             &priv->input.fingers[0].x, &priv->input.fingers[0].y,
                             DS_TOUCHPAD_W - 1, DS_TOUCHPAD_H - 1,
                             ir_dots);
        } else {
            bm_map_ir_analog_axis(ir_emu_mode, &priv->ir_emu_state,
                                  DS_ANALOG_AXIS__NUM, priv->input.analog_axis,
                                  ir_analog_axis_map, ir_dots);
        }
    }

    fake_wiimote_report_ir_dots(device->wiimote, ir_dots);

    if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NONE) {
        fake_wiimote_report_input(device->wiimote, wiimote_buttons);
    } else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NUNCHUK) {
        bm_map_nunchuk(DS_BUTTON__NUM, priv->input.buttons,
                       DS_ANALOG_AXIS__NUM, priv->input.analog_axis,
                       0, 0, 0,
                       input_mappings[priv->mapping].nunchuk_button_map,
                       input_mappings[priv->mapping].nunchuk_analog_axis_map,
                       &extension_data.nunchuk);
        fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
                                      &extension_data, sizeof(extension_data.nunchuk));
    } else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_CLASSIC) {
        bm_map_classic(DS_BUTTON__NUM, priv->input.buttons,
                       DS_ANALOG_AXIS__NUM, priv->input.analog_axis,
                       input_mappings[priv->mapping].classic_button_map,
                       input_mappings[priv->mapping].classic_analog_axis_map,
                       &extension_data.classic);
        fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
                                      &extension_data, sizeof(extension_data.classic));
    }

    return true;
}

int ds_driver_ops_usb_async_resp(usb_input_device_t *device)
{
    struct ds_private_data_t *priv = (void *)device->private_data;
    struct ds_input_report *report = (void *)device->usb_async_resp;

    if (report->report_id == 0x01) {
        ds_get_buttons(report, &priv->input.buttons);
        ds_get_analog_axis(report, priv->input.analog_axis);

        priv->input.acc_x = (s16)le16toh(report->accel_x);
        priv->input.acc_y = (s16)le16toh(report->accel_y);
        priv->input.acc_z = (s16)le16toh(report->accel_z);

        priv->input.num_fingers = 0;

        if (!report->touch1_contact) {
            priv->input.fingers[0].x = report->touch1_x_lo | ((u16)report->touch1_x_hi << 8);
            priv->input.fingers[0].y = report->touch1_y_lo | ((u16)report->touch1_y_hi << 4);
            priv->input.num_fingers++;
        }

        if (!report->touch1_contact) {
            priv->input.fingers[1].x = report->touch2_x_lo | ((u16)report->touch2_x_hi << 8);
            priv->input.fingers[1].y = report->touch2_y_lo | ((u16)report->touch2_y_hi << 4);
            priv->input.num_fingers++;
        }
    }

    return ds_request_data(device);
}

const usb_device_driver_t ds_usb_device_driver = {
    .probe		= ds_driver_ops_probe,
    .init		= ds_driver_ops_init,
    .disconnect	= ds_driver_ops_disconnect,
    .slot_changed	= ds_driver_ops_slot_changed,
    .set_rumble	= ds_driver_ops_set_rumble,
    .report_input	= ds_report_input,
    .usb_async_resp	= ds_driver_ops_usb_async_resp,
};
