/*
 * hal.h
 *
 *  Created on: 12 sep. 2020
 *      Author: perra
 */

#ifndef GCODE_HAL_H_
#define GCODE_HAL_H_

#include "GCode.h"

// driver capabilities, to be set by driver in driver_init(), flags may be cleared after to switch off option
typedef union {
    uint32_t value;
    struct {
        uint32_t mist_control,
                 variable_spindle,
                 safety_door,
                 spindle_dir,
                 software_debounce,
                 step_pulse_delay,
                 limits_pull_up,
                 control_pull_up,
                 probe_pull_up,
                 amass_level, // 0...3
                 program_stop,
                 block_delete,
                 e_stop,
                 spindle_at_speed,
                 laser_ppi_mode,
                 spindle_sync,
                 sd_card,
                 bluetooth,
                 ethernet,
                 wifi,
                 spindle_pwm_invert,
                 spindle_pid,
                 axis_ganged_x,
                 axis_ganged_y,
                 axis_ganged_z,
                 mpg_mode,
                 spindle_pwm_linearization,
                 probe_connected,
                 unassigned;
    };
} driver_cap_t;

typedef void (*stream_write_ptr)(const char *s);
typedef axes_signals_t (*limits_get_state_ptr)(void);
typedef void (*driver_reset_ptr)(void);

/* TODO: add to HAL so that a different formatting (xml, json etc) of reports may be implemented by driver? */
//typedef struct {
//    status_code_t (*report_status_message)(status_code_t status_code);
//    alarm_code_t (*report_alarm_message)(alarm_code_t alarm_code);
//    message_code_t (*report_feedback_message)(message_code_t message_code);
//    void (*report_init_message)(void);
//    void (*report_grbl_help)(void);
//    void (*report_grbl_settings)(void);
//    void (*report_echo_line_received)(char *line);
//    void (*report_realtime_status)(void);
//    void (*report_probe_parameters)(void);
//    void (*report_ngc_parameters)(void);
//    void (*report_gcode_modes)(void);
//    void (*report_startup_line)(uint8_t n, char *line);
//    void (*report_execute_startup_message)(char *line, status_code_t status_code);
//} HAL_report_t;

//typedef struct {
//    status_code_t (*status_message)(status_code_t status_code);
//    message_code_t (*feedback_message)(message_code_t message_code);
//} report_t;

//typedef struct {
//    stream_type_t type;
//    uint16_t (*get_rx_buffer_available)(void);
////    bool (*stream_write)(char c);
//    stream_write_ptr write; // write to current I/O stream only
//    stream_write_ptr write_all; // write to all active output streams
//    int16_t (*read)(void);
//    void (*reset_read_buffer)(void);
//    void (*cancel_read_buffer)(void);
//    bool (*suspend_read)(bool await);
//    bool (*enqueue_realtime_command)(char data); // NOTE: set by grbl at startup
//} io_stream_t;

typedef struct {
    uint8_t num_digital;
    uint8_t num_analog;
    void (*digital_out)(uint8_t port, bool on);
    bool (*analog_out)(uint8_t port, float value);
    int32_t (*wait_on_input)(bool digital, uint8_t port, wait_mode_t wait_mode, float timeout);
} io_port_t;

typedef struct HAL {
    uint32_t version;
    char *info;
//    char *driver_version;
//    char *driver_options;
//    char *board;
//    uint32_t f_step_timer;
//    uint32_t rx_buffer_size;

//    bool (*driver_setup)(settings_t *settings);

//    void (*limits_enable)(bool on, bool homing);
    limits_get_state_ptr limits_get_state;
//    void (*coolant_set_state)(coolant_state_t mode);
//    coolant_state_t (*coolant_get_state)(void);
//    void (*delay_ms)(uint32_t ms, void (*callback)(void));

//    void (*spindle_set_state)(spindle_state_t state, float rpm);
//    spindle_state_t (*spindle_get_state)(void);
//#ifdef SPINDLE_PWM_DIRECT
//    uint_fast16_t (*spindle_get_pwm)(float rpm);
//    void (*spindle_update_pwm)(uint_fast16_t pwm);
//#else
//    void (*spindle_update_rpm)(float rpm);
//#endif
//    control_signals_t (*system_control_get_state)(void);

//    void (*stepper_wake_up)(void);
//    void (*stepper_go_idle)(bool clear_signals);
//    void (*stepper_enable)(axes_signals_t enable);
//    void (*stepper_disable_motors)(axes_signals_t axes, squaring_mode_t mode);
//    void (*stepper_cycles_per_tick)(uint32_t cycles_per_tick);
//    void (*stepper_pulse_start)(stepper_t *stepper);
//
//    io_stream_t stream; // pointers to current I/O stream handlers
//
//    void (*set_bits_atomic)(volatile uint_fast16_t *value, uint_fast16_t bits);
//    uint_fast16_t (*clear_bits_atomic)(volatile uint_fast16_t *value, uint_fast16_t bits);
//    uint_fast16_t (*set_value_atomic)(volatile uint_fast16_t *value, uint_fast16_t bits);

//    void (*settings_changed)(settings_t *settings);

    // optional entry points, may be unassigned (null)
//    bool (*driver_release)(void);
//    probe_state_t (*probe_get_state)(void);
//    void (*probe_configure_invert_mask)(bool is_probe_away);
//    void (*execute_realtime)(uint_fast16_t state);
//    user_mcode_t (*user_mcode_check)(user_mcode_t mcode);
//    status_code_t (*user_mcode_validate)(parser_block_t *gc_block, uint32_t *value_words);
//    void (*user_mcode_execute)(uint_fast16_t state, parser_block_t *gc_block);
//    status_code_t (*user_command_execute)(char *line);
//    void (*driver_rt_command_execute)(uint8_t cmd);
////    void (*driver_rt_report)(stream_write_ptr stream_write, report_tracking_flags_t report);
//    void (*driver_feedback_message)(stream_write_ptr stream_write);
//    status_code_t (*driver_sys_command_execute)(uint_fast16_t state, char *line, char *lcline); // return Status_Unhandled
//    bool (*get_position)(int32_t (*position)[N_AXIS]);
//    void (*tool_select)(tool_data_t *tool, bool next);
//    status_code_t (*tool_change)(parser_state_t *gc_state);
//    void (*show_message)(const char *msg);
//    void (*report_options)(void);
//    driver_reset_ptr driver_reset;
//    status_code_t (*driver_setting)(setting_type_t setting, float value, char *svalue);
//    void (*driver_settings_restore)(void);
//    void (*driver_settings_report)(setting_type_t setting_type);
//    void (*driver_axis_settings_report)(axis_setting_type_t setting_type, uint8_t axis_idx);
//    spindle_data_t (*spindle_get_data)(spindle_data_request_t request);
//    void (*spindle_reset_data)(void);
//    void (*state_change_requested)(uint_fast16_t state);
//    void (*encoder_state_changed)(encoder_t *encoder);
//#ifdef DEBUGOUT
//    void (*debug_out)(bool on);
//#endif

//    eeprom_io_t eeprom;

//    io_port_t port;

    // entry points set by grbl at reset
//    report_t report;

    // callbacks - set up by grbl before MCU init
//    bool (*protocol_enqueue_gcode)(char *data);
//    bool (*stream_blocking_callback)(void);
//    void (*stepper_interrupt_callback)(void);
//    void (*limit_interrupt_callback)(axes_signals_t state);
//    void (*control_interrupt_callback)(control_signals_t signals);
//    void (*spindle_index_callback)(spindle_data_t *rpm);

    driver_cap_t driver_cap;
} HAL;

extern HAL hal;
extern bool driver_init (void);
#endif /* GCODE_HAL_H_ */
