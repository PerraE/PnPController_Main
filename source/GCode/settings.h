/*
 * settings.h
 *
 *  Created on: 12 sep. 2020
 *      Author: perra
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

//// Define persistent storage address indexing for coordinate parameters
//#if COMPATIBILITY_LEVEL <= 1
//#define N_COORDINATE_SYSTEM 9  // Number of supported work coordinate systems (from index 1)
//#else
//#define N_COORDINATE_SYSTEM 6  // Number of supported work coordinate systems (from index 1)
//#endif
//#define SETTING_INDEX_NCOORD (N_COORDINATE_SYSTEM + 2)  // Total number of coordinate system stored (from index 0)
//// NOTE: Work coordinate indices are (0=G54, 1=G55, ... , 6=G59)
//#define SETTING_INDEX_G28    N_COORDINATE_SYSTEM        // Home position 1
//#define SETTING_INDEX_G30    (N_COORDINATE_SYSTEM + 1)  // Home position 2
//#if N_COORDINATE_SYSTEM >= 9
//#define SETTING_INDEX_G59_3  (N_COORDINATE_SYSTEM - 1)  // G59.3 position
//#endif
//#define SETTING_INDEX_G92    (N_COORDINATE_SYSTEM + 2)  // Coordinate offset

//typedef enum {
//    SettingIndex_G54 = 0,
//    SettingIndex_G55,
//    SettingIndex_G56,
//    SettingIndex_G57,
//    SettingIndex_G58,
//    SettingIndex_G59,
//#if N_COORDINATE_SYSTEM >= 9
//    SettingIndex_G59_1,
//    SettingIndex_G59_2,
//    SettingIndex_G59_3,
//#endif
//    SettingIndex_G28,   // Home position 1
//    SettingIndex_G30,   // Home position 2
//    SettingIndex_G92,   // Coordinate offset
//    SettingIndex_NCoord
//} setting_coord_system_t;


//#define N_COORDINATE_SYSTEMS (SettingIndex_NCoord - 3)  // Number of supported work coordinate systems (from index 1)


typedef union {
    uint8_t value;
    struct {
        uint8_t hard_enabled,
                soft_enabled,
                check_at_init,
                jog_soft_limited,
                unassigned;
    };
} limit_settings_flags_t;

typedef struct {
    limit_settings_flags_t flags;
    axes_signals_t invert;
    axes_signals_t disable_pullup;
} limit_settings_t;

// Global persistent settings (Stored from byte persistent storage_ADDR_GLOBAL onwards)
typedef struct {
	limit_settings_t limits;
} settings_t;


#endif /* SETTINGS_H_ */
