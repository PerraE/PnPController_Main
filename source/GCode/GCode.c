/*
 * GCode.c
 *
 *  Created on: 7 sep. 2020
 *      Author: perra
 */

#include "PnPContoller_Main.h"


/*
	G0	X Y Z1 Z2 C1 C2 C3 C4	Rapid Move
	G1	X Y Z1 Z2 C1 C2 C3 C4	Linear Move
	G90							Absolute distance Mode
	G91							Incremental distance mode. Not implemented
	G94							Units per minute. G93 not supported
	M0							Pause program
	M2							End program
	G20							Inches for length unit. Not implemented
	G21							mm for length unit
	G28							Home all axes
	F							Feedrate
	M62 P						Turn digital output on
	M63 P						Turn digital output off
	M66 PELQ					Wait for digital input
	M67 PELQ					Wait for analog input
	M400						Wait for move to complete

 *
 */


// NOTE: Max line number is defined by the g-code standard to be 99999. It seems to be an
// arbitrary value, and some GUIs may require more. So we increased it based on a max safe
// value when converting a float (7.2 digit precision)s to an integer.
#define MAX_LINE_NUMBER 10000000


#define FAIL(status) return(status);

parser_state_t gc_state;
typedef enum {
    AxisCommand_None = 0,
    AxisCommand_NonModal,
    AxisCommand_MotionMode,
    AxisCommand_ToolLengthOffset,
    AxisCommand_Scaling
} axis_command_t;

static scale_factor_t scale_factor;
static gc_thread_data thread;
static output_command_t *output_commands = NULL; // Linked list

// Simple hypotenuse computation function.
inline static float hypot_f (float x, float y)
{
    return sqrtf(x*x + y*y);
}

static void set_scaling (float factor)
{
    uint_fast8_t idx = N_AXIS;
//    axes_signals_t state = gc_get_g51_state();

    do {
        scale_factor.ijk[--idx] = factor;

    } while(idx);

    gc_state.modal.scaling_active = factor != 1.0f;

//    if(state.value != gc_get_g51_state().value)
//        sys.report.scaling = On;
}

float *gc_get_scaling (void)
{
    return scale_factor.ijk;
}

axes_signals_t gc_get_g51_state ()
{
    uint_fast8_t idx = N_AXIS;
    axes_signals_t scaled = {0};

    do {
        scaled.value <<= 1;
        if(scale_factor.ijk[--idx] != 1.0f)
            scaled.value |= 0x01;

    } while(idx);

    return scaled;
}

float gc_get_offset (uint_fast8_t idx)
{
    return gc_state.modal.coord_system.xyz[idx] + gc_state.g92_coord_offset[idx] + gc_state.tool_length_offset[idx];
}

inline static float gc_get_block_offset (parser_block_t *gc_block, uint_fast8_t idx)
{
    return gc_block->modal.coord_system.xyz[idx] + gc_state.g92_coord_offset[idx] + gc_state.tool_length_offset[idx];
}

void gc_init(bool cold_start)
{

#if COMPATIBILITY_LEVEL > 1
    cold_start = true;
#endif

    if(cold_start) {
        memset(&gc_state, 0, sizeof(parser_state_t));
      #ifdef N_TOOLS
        gc_state.tool = &tool_table[0];
      #else
        memset(&tool_table, 0, sizeof(tool_table));
        gc_state.tool = &tool_table;
      #endif
    } else {
        memset(&gc_state, 0, offsetof(parser_state_t, g92_coord_offset));
        gc_state.tool_pending = gc_state.tool->tool;
        // TODO: restore offsets, tool offset mode?
    }

    // Clear any pending output commands
    while(output_commands) {
        output_command_t *next = output_commands->next;
        free(output_commands);
        output_commands = next;
    }

    // Load default override status
//    gc_state.modal.override_ctrl = sys.override.control;
//    gc_state.spindle.css.max_rpm = settings.spindle.rpm_max; // default max speed for CSS mode

    set_scaling(1.0f);

    // Load default G54 coordinate system.
//    if (!settings_read_coord_data(gc_state.modal.coord_system.idx, &gc_state.modal.coord_system.xyz))
//        hal.report.status_message(Status_SettingReadFail);

#if COMPATIBILITY_LEVEL <= 1
//    if (cold_start && !settings_read_coord_data(SETTING_INDEX_G92, &gc_state.g92_coord_offset))
//        hal.report.status_message(Status_SettingReadFail);
#endif

//    if(settings.flags.lathe_mode)
//        gc_state.modal.plane_select = PlaneSelect_ZX;
}
// Add output command to linked list
static bool add_output_command (output_command_t *command)
{
    output_command_t *add_cmd;

    if((add_cmd = malloc(sizeof(output_command_t)))) {

        memcpy(add_cmd, command, sizeof(output_command_t));

        if(output_commands == NULL)
            output_commands = add_cmd;
        else {
            output_command_t *cmd = output_commands;
            while(cmd->next)
                cmd = cmd->next;
            cmd->next = add_cmd;
        }
    }

    return add_cmd != NULL;
}

//****************************************************************************
/**
 * compressInstring
 *
 * @param dataptr pointer to the application buffer that contains the data to send
 * @param size size of the application data to send
 * @param apiflags combination of following flags :
 * - NETCONN_COPY: data will be copied into memory belonging to the stack
 * - NETCONN_MORE: for TCP connection, PSH flag will be set on last segment sent
 * - NETCONN_DONTBLOCK: only write the data if all data can be written at once
 * @param bytes_written pointer to a location that receives the number of written bytes
 * @return ERR_OK if data was sent, any other err_t on error
 */
void compressInstring(char *dataptr, u16_t *len, char *outbuff)
{
	u16_t i,j;
	uint8_t comment, addch;

	comment = 0;
	j = 0;

	for (i=0; i<*len; i++)
	{
		addch = 1;
		// Remove blanks and special characters
		if(*dataptr == '\n' || *dataptr == '\r' || *dataptr == ' '  || *dataptr == '\0' )
		{
			addch=0;
		}
		// Remove comments
		if(*dataptr == ';')
		{
			i=*len;
			addch=0;
		}
		if(*dataptr == '(')
		{
			comment=1;
			addch=0;
		}
		// Make Upper case
		if((*dataptr >= 'a') && (*dataptr <= 'z'))
		{
			*dataptr &= 0b01011111;
		}
		// Add the character
		if(addch && !comment)
		{
			*outbuff = *dataptr;
			outbuff ++;
			j++;
		}

		// Remove comment flag
		if(*dataptr == ')')
		{
			comment=0;
		}
		dataptr ++;
	}
	*outbuff = '\0';
	(*len) = j;
	return;

}

// From grblHAL
void parseBlock(char *block, char *message)
{

    static parser_block_t gc_block;

    // Determine if the line is a program start/end marker.
    // Old comment from protocol.c:
    // NOTE: This maybe installed to tell Grbl when a program is running vs manual input,
    // where, during a program, the system auto-cycle start will continue to execute
    // everything until the next '%' sign. This will help fix resuming issues with certain
    // functions that empty the planner buffer to execute its task on-time.
    if (block[0] == CMD_PROGRAM_DEMARCATION && block[1] == '\0') {
        gc_state.file_run = !gc_state.file_run;
        return Status_OK;
    }

    /* -------------------------------------------------------------------------------------
        STEP 1: Initialize parser block struct and copy current g-code state modes. The parser
        updates these modes and commands as the block line is parsed and will only be used and
        executed after successful error-checking. The parser block struct also contains a block
        values struct, word tracking variables, and a non-modal commands tracker for the new
        block. This struct contains all of the necessary information to execute the block. */

       memset(&gc_block, 0, sizeof(gc_block));                           // Initialize the parser block struct.
       memcpy(&gc_block.modal, &gc_state.modal, sizeof(gc_state.modal)); // Copy current modes

       bool set_tool = false;
       axis_command_t axis_command = AxisCommand_None;
       uint_fast8_t port_command = 0;
       plane_t plane;

       // Initialize bitflag tracking variables for axis indices compatible operations.
       uint8_t axis_words = 0; // XYZ tracking
//       uint8_t ijk_words = 0; // IJK tracking

       // Initialize command and value words and parser flags variables.
       uint32_t command_words = 0; // Bitfield for tracking G and M command words. Also used for modal group violations.
       uint32_t value_words = 0;   // Bitfield for tracking value words.
       gc_parser_flags_t gc_parser_flags = {0};

       // Determine if the line is a jogging motion or a normal g-code block.
       if (block[0] == '$') { // NOTE: `$J=` already parsed when passed to this function.
           // Set G1 and G94 enforced modes to ensure accurate error checks.
           gc_parser_flags.jog_motion = On;
           gc_block.modal.motion = MotionMode_Linear;
           gc_block.modal.feed_mode = FeedMode_UnitsPerMin;
           gc_block.modal.spindle_rpm_mode = SpindleSpeedMode_RPM;
           gc_block.values.n = JOG_LINE_NUMBER; // Initialize default line number reported during jog.
       }


       /* -------------------------------------------------------------------------------------
          STEP 2: Import all g-code words in the block. A g-code word is a letter followed by
          a number, which can either be a 'G'/'M' command or sets/assigns a command value. Also,
          perform initial error-checks for command word modal group violations, for any repeated
          words, and for negative values set for the value words F, N, P, T, and S. */

         word_bit_t word_bit; // Bit-value for assigning tracking variables
         uint_fast8_t char_counter = gc_parser_flags.jog_motion ? 3 /* Start parsing after `$J=` */ : 0;
         char letter;
         float value;
         uint_fast16_t int_value = 0;
         uint_fast16_t mantissa = 0;

         while ((letter = block[char_counter++]) != '\0') { // Loop until no more g-code words in block.

             // Import the next g-code word, expecting a letter followed by a value. Otherwise, error out.
             if((letter < 'A') || (letter > 'Z'))
                 FAIL(Status_ExpectedCommandLetter); // [Expected word letter]

             if (!read_float(block, &char_counter, &value))
                 FAIL(Status_BadNumberFormat); // [Expected word value]

             // Convert values to smaller uint8 significand and mantissa values for parsing this word.
             // NOTE: Mantissa is multiplied by 100 to catch non-integer command values. This is more
             // accurate than the NIST gcode requirement of x10 when used for commands, but not quite
             // accurate enough for value words that require integers to within 0.0001. This should be
             // a good enough comprimise and catch most all non-integer errors. To make it compliant,
             // we would simply need to change the mantissa to int16, but this add compiled flash space.
             // Maybe update this later.
             int_value = (uint_fast16_t)truncf(value);
             mantissa = (uint_fast16_t)roundf(100.0f * (value - int_value)); // Compute mantissa for Gxx.x commands.
             // NOTE: Rounding must be used to catch small floating point errors.

             // Check if the g-code word is supported or errors due to modal group violations or has
              // been repeated in the g-code block. If ok, update the command or record its value.
              switch(letter) {

                /* 'G' and 'M' Command Words: Parse commands and check for modal group violations.
                   NOTE: Modal group numbers are defined in Table 4 of NIST RS274-NGC v3, pg.20 */

                  case 'G':
                  // Determine 'G' command and its modal group
                      switch(int_value) {

                          case 10: case 28: case 30: case 92:
                              // Check for G10/28/30/92 being called with G0/1/2/3/38 on same block.
                              // * G43.1 is also an axis command but is not explicitly defined this way.
                              if (mantissa == 0) { // Ignore G28.1, G30.1, and G92.1
                                  if (axis_command)
                                      FAIL(Status_GcodeAxisCommandConflict); // [Axis word/command conflict]
                                  axis_command = AxisCommand_NonModal;
                              }
                              // No break. Continues to next line.

                          case 4: case 53:
                              word_bit.group = ModalGroup_G0;
                              gc_block.non_modal_command = (non_modal_t)int_value;
                              if ((int_value == 28) || (int_value == 30)) {
                                  if (!((mantissa == 0) || (mantissa == 10)))
                                      FAIL(Status_GcodeUnsupportedCommand);
                                  gc_block.non_modal_command += mantissa;
                                  mantissa = 0; // Set to zero to indicate valid non-integer G command.
                              } else if (int_value == 92) {
                                  if (!((mantissa == 0) || (mantissa == 10) || (mantissa == 20) || (mantissa == 30)))
                                      FAIL(Status_GcodeUnsupportedCommand);
                                  gc_block.non_modal_command += mantissa;
                                  mantissa = 0; // Set to zero to indicate valid non-integer G command.
                              }
                              break;


                          case 0: case 1: case 2: case 3: case 5:
                              // Check for G0/1/2/3/38 being called with G10/28/30/92 on same block.
                              // * G43.1 is also an axis command but is not explicitly defined this way.
                              if (axis_command)
                                  FAIL(Status_GcodeAxisCommandConflict); // [Axis word/command conflict]
                              axis_command = AxisCommand_MotionMode;
                              // No break. Continues to next line.

                          case 80:
                              word_bit.group = ModalGroup_G1;
                              gc_block.modal.motion = (motion_mode_t)int_value;
                              gc_block.modal.canned_cycle_active = false;
                              break;

                          case 73: case 81: case 82: case 83: case 85: case 86: case 89:
                              if (axis_command)
                                  FAIL(Status_GcodeAxisCommandConflict); // [Axis word/command conflict]
                              axis_command = AxisCommand_MotionMode;
                              word_bit.group = ModalGroup_G1;
                              gc_block.modal.canned_cycle_active = true;
                              gc_block.modal.motion = (motion_mode_t)int_value;
                              gc_parser_flags.canned_cycle_change = gc_block.modal.motion != gc_state.modal.motion;
                              break;

                          case 17: case 18: case 19:
                              word_bit.group = ModalGroup_G2;
                              gc_block.modal.plane_select = (plane_select_t)(int_value - 17);
                              break;

                          case 90: case 91:
                              if (mantissa == 0) {
                                  word_bit.group = ModalGroup_G3;
                                  gc_block.modal.distance_incremental = int_value == 91;
                              } else {
                                  word_bit.group = ModalGroup_G4;
                                  if ((mantissa != 10) || (int_value == 90))
                                      FAIL(Status_GcodeUnsupportedCommand); // [G90.1 not supported]
                                  mantissa = 0; // Set to zero to indicate valid non-integer G command.
                                  // Otherwise, arc IJK incremental mode is default. G91.1 does nothing.
                              }
                              break;

                          case 93: case 94:
                              word_bit.group = ModalGroup_G5;
                              gc_block.modal.feed_mode = (feed_mode_t)(94 - int_value);
                              break;

                          case 20: case 21:
                              word_bit.group = ModalGroup_G6;
                              gc_block.modal.units_imperial = int_value == 20;
                              break;

                          case 40:
                              word_bit.group = ModalGroup_G7;
                              // NOTE: Not required since cutter radius compensation is always disabled. Only here
                              // to support G40 commands that often appear in g-code program headers to setup defaults.
                              // gc_block.modal.cutter_comp = CUTTER_COMP_DISABLE; // G40
                              break;

                          case 43: case 49:
                              word_bit.group = ModalGroup_G8;
                              // NOTE: The NIST g-code standard vaguely states that when a tool length offset is changed,
                              // there cannot be any axis motion or coordinate offsets updated. Meaning G43, G43.1, and G49
                              // all are explicit axis commands, regardless if they require axis words or not.

                              if (axis_command)
                                  FAIL(Status_GcodeAxisCommandConflict); // [Axis word/command conflict] }

                              axis_command = AxisCommand_ToolLengthOffset;
                              if (int_value == 49) // G49
                                  gc_block.modal.tool_offset_mode = ToolLengthOffset_Cancel;
      #ifdef N_TOOLS
                              else if (mantissa == 0) // G43
                                  gc_block.modal.tool_offset_mode = ToolLengthOffset_Enable;
                              else if (mantissa == 20) // G43.2
                                  gc_block.modal.tool_offset_mode = ToolLengthOffset_ApplyAdditional;
      #endif
                              else if (mantissa == 10) // G43.1
                                  gc_block.modal.tool_offset_mode = ToolLengthOffset_EnableDynamic;
                              else
                                  FAIL(Status_GcodeUnsupportedCommand); // [Unsupported G43.x command]
                              mantissa = 0; // Set to zero to indicate valid non-integer G command.
                              break;

                          case 54: case 55: case 56: case 57: case 58: case 59:
                              word_bit.group = ModalGroup_G12;
                              gc_block.modal.coord_system.idx = int_value - 54; // Shift to array indexing.
      #if N_COORDINATE_SYSTEM > 6
                              if(int_value == 59) {
                                  gc_block.modal.coord_system.idx += mantissa / 10;
                                  mantissa = 0;
                              }
      #endif
                              break;

                          case 61:
                              word_bit.group = ModalGroup_G13;
                              if (mantissa != 0) // [G61.1 not supported]
                                  FAIL(Status_GcodeUnsupportedCommand);
                              // gc_block.modal.control = CONTROL_MODE_EXACT_PATH; // G61
                              break;

                          case 98: case 99:
                              word_bit.group = ModalGroup_G10;
                              gc_block.modal.retract_mode = (cc_retract_mode_t)(int_value - 98);
                              break;

                          case 50: case 51:
                              axis_command = AxisCommand_Scaling;
                              word_bit.group = ModalGroup_G11;
                              gc_block.modal.scaling_active = int_value == 51;
                              break;

                          default: FAIL(Status_GcodeUnsupportedCommand); // [Unsupported G command]
                      } // end G-value switch

                      if (mantissa > 0)
                          FAIL(Status_GcodeCommandValueNotInteger); // [Unsupported or invalid Gxx.x command]
                      // Check for more than one command per modal group violations in the current block
                      // NOTE: Variable 'word_bit' is always assigned, if the command is valid.
                      if (bit_istrue(command_words, bit(word_bit.group)))
                          FAIL(Status_GcodeModalGroupViolation);
                      command_words |= bit(word_bit.group);
                      break;

                  case 'M': // Determine 'M' command and its modal group

                      if (mantissa > 0)
                          FAIL(Status_GcodeCommandValueNotInteger); // [No Mxx.x commands]

                      switch(int_value) {

                          case 0: case 1: case 2: case 30:
                              word_bit.group = ModalGroup_M4;
                              switch(int_value) {

                                  case 0: // M0 - program pause
                                      gc_block.modal.program_flow = ProgramFlow_Paused;
                                      break;

                                  default: // M2, M30 - program end and reset
                                      gc_block.modal.program_flow = (program_flow_t)int_value;
                              }
                              break;

                          case 49: case 50: case 51: case 53:
                              word_bit.group = ModalGroup_M9;
                              gc_block.override_command = (override_mode_t)int_value;
                              break;

                          case 61:
                              set_tool = true;
                              word_bit.group = ModalGroup_M6; //??
                              break;

                          case 62:
                          case 63:
                              word_bit.group = ModalGroup_M10;
                              port_command = int_value;
                              break;

                          case 66:
                              word_bit.group = ModalGroup_M10;
                              port_command = int_value;
                              break;

                          case 67:
                          case 68:
                             word_bit.group = ModalGroup_M10;
                              port_command = int_value;
                              break;

                          default:
                                  FAIL(Status_GcodeUnsupportedCommand); // [Unsupported M command]
                      } // end M-value switch

                      // Check for more than one command per modal group violations in the current block
                      // NOTE: Variable 'word_bit' is always assigned, if the command is valid.
                      if (bit_istrue(command_words, bit(word_bit.group)))
                          FAIL(Status_GcodeModalGroupViolation);
                      command_words |= bit(word_bit.group);
                      break;

                  // NOTE: All remaining letters assign values.
                  default:

                      /* Non-Command Words: This initial parsing phase only checks for repeats of the remaining
                      legal g-code words and stores their value. Error-checking is performed later since some
                      words (I,J,K,L,P,R) have multiple connotations and/or depend on the issued commands. */

                      switch(letter) {

                          case 'A':
                              word_bit.parameter = Word_A;
                              gc_block.values.xyz[A_AXIS] = value;
                              bit_true(axis_words, bit(A_AXIS));
                              break;


                          case 'B':
                              word_bit.parameter = Word_B;
                              gc_block.values.xyz[B_AXIS] = value;
                              bit_true(axis_words, bit(B_AXIS));
                              break;

                          case 'C':
                              word_bit.parameter = Word_C;
                              gc_block.values.xyz[C_AXIS] = value;
                              bit_true(axis_words, bit(C_AXIS));
                              break;

                          case 'D':
                              word_bit.parameter = Word_D;
                              gc_block.values.xyz[D_AXIS] = value;
                              bit_true(axis_words, bit(D_AXIS));
                              break;

                          case 'E':
                              word_bit.parameter = Word_E;
                              gc_block.values.e = value;
                              break;

                          case 'F':
                              word_bit.parameter = Word_F;
                              gc_block.values.f = value;
                              break;

                          case 'H':
                              if (mantissa > 0)
                                  FAIL(Status_GcodeCommandValueNotInteger);
                              word_bit.parameter = Word_H;
                              gc_block.values.h = int_value;
                              break;

                          case 'L':
                              if (mantissa > 0)
                                  FAIL(Status_GcodeCommandValueNotInteger);
                              word_bit.parameter = Word_L;
                              gc_block.values.l = int_value;
                              break;

                          case 'N':
                              word_bit.parameter = Word_N;
                              gc_block.values.n = (int32_t)truncf(value);
                              break;

                          case 'P': // NOTE: For certain commands, P value must be an integer, but none of these commands are supported.
                              word_bit.parameter = Word_P;
                              gc_block.values.p = value;
                              break;

                          case 'Q': // may be used for user defined mcodes or G61,G76
                              word_bit.parameter = Word_Q;
                              gc_block.values.q = value;
                              break;

                          case 'R':
                              word_bit.parameter = Word_R;
                              gc_block.values.r = value;
                              break;

                          case 'S':
                              word_bit.parameter = Word_S;
                              gc_block.values.s = value;
                              break;

                          case 'U':
                              word_bit.parameter = Word_U;
                              gc_block.values.xyz[U_AXIS] = value;
                              bit_true(axis_words, bit(U_AXIS));
                              break;

                          case 'X':
                              word_bit.parameter = Word_X;
                              gc_block.values.xyz[X_AXIS] = value;
                              bit_true(axis_words, bit(X_AXIS));
                              break;

                          case 'Y':
                              word_bit.parameter = Word_Y;
                              gc_block.values.xyz[Y_AXIS] = value;
                              bit_true(axis_words, bit(Y_AXIS));
                              break;

                          case 'Z':
                              word_bit.parameter = Word_Z;
                              gc_block.values.xyz[Z_AXIS] = value;
                              bit_true(axis_words, bit(Z_AXIS));
                              break;

                          default: FAIL(Status_GcodeUnsupportedCommand);

                      } // end parameter letter switch

                      // NOTE: Variable 'word_bit' is always assigned, if the non-command letter is valid.
                      if (bit_istrue(value_words, bit(word_bit.parameter)))
                          FAIL(Status_GcodeWordRepeated); // [Word repeated]

                      // Check for invalid negative values for words F, H, N, P, T, and S.
                      // NOTE: Negative value check is done here simply for code-efficiency.
                      if ((bit(word_bit.parameter) & (bit(Word_D)|bit(Word_F)|bit(Word_H)|bit(Word_N)|bit(Word_T)|bit(Word_S))) && value < 0.0f)
                          FAIL(Status_NegativeValue); // [Word value cannot be negative]

                      value_words |= bit(word_bit.parameter); // Flag to indicate parameter assigned.

              } // end main letter switch
          }

          // Parsing complete!
         /* -------------------------------------------------------------------------------------
            STEP 3: Error-check all commands and values passed in this block. This step ensures all of
            the commands are valid for execution and follows the NIST standard as closely as possible.
            If an error is found, all commands and values in this block are dumped and will not update
            the active system g-code modes. If the block is ok, the active system g-code modes will be
            updated based on the commands of this block, and signal for it to be executed.

            Also, we have to pre-convert all of the values passed based on the modes set by the parsed
            block. There are a number of error-checks that require target information that can only be
            accurately calculated if we convert these values in conjunction with the error-checking.
            This relegates the next execution step as only updating the system g-code modes and
            performing the programmed actions in order. The execution step should not require any
            conversion calculations and would only require minimal checks necessary to execute.
         */

         /* NOTE: At this point, the g-code block has been parsed and the block line can be freed.
            NOTE: It's also possible, at some future point, to break up STEP 2, to allow piece-wise
            parsing of the block on a per-word basis, rather than the entire block. This could remove
            the need for maintaining a large string variable for the entire block and free up some memory.
            To do this, this would simply need to retain all of the data in STEP 1, such as the new block
            data struct, the modal group and value bitflag tracking variables, and axis array indices
            compatible variables. This data contains all of the information necessary to error-check the
            new g-code block when the EOL character is received. However, this would break Grbl's startup
            lines in how it currently works and would require some refactoring to make it compatible.
         */

        /*
         * Order of execution as per RS274-NGC_3 table 8:
         *
         *      1. comment (includes message)
         *      2. set feed rate mode (G93, G94 - inverse time or per minute)
         *      3. set feed rate (F)
         *      4. set spindle speed (S)
         *      5. select tool (T)
         *      6. change tool (M6)
         *      7. spindle on or off (M3, M4, M5)
         *      8. coolant on or off (M7, M8, M9)
         *      9. enable or disable overrides (M48, M49, M50, M51, M53)
         *      10. dwell (G4)
         *      11. set active plane (G17, G18, G19)
         *      12. set length units (G20, G21)
         *      13. cutter radius compensation on or off (G40, G41, G42)
         *      14. cutter length compensation on or off (G43, G49)
         *      15. coordinate system selection (G54, G55, G56, G57, G58, G59, G59.1, G59.2, G59.3)
         *      16. set path control mode (G61, G61.1, G64)
         *      17. set distance mode (G90, G91)
         *      18. set retract mode (G98, G99)
         *      19. home (G28, G30) or
         *              change coordinate system data (G10) or
         *              set axis offsets (G92, G92.1, G92.2, G94).
         *      20. perform motion (G0 to G3, G33, G80 to G89) as modified (possibly) by G53
         *      21. stop and end (M0, M1, M2, M30, M60)
         */

         // [0. Non-specific/common error-checks and miscellaneous setup]:

           // Determine implicit axis command conditions. Axis words have been passed, but no explicit axis
           // command has been sent. If so, set axis command to current motion mode.
           if (axis_words && !axis_command)
               axis_command = AxisCommand_MotionMode; // Assign implicit motion-mode

           if(gc_state.tool_change && axis_command == AxisCommand_MotionMode && !gc_parser_flags.jog_motion)
               FAIL(Status_GcodeToolChangePending); // [Motions (except jogging) not allowed when changing tool]

           // Check for valid line number N value.
           // Line number value cannot be less than zero (done) or greater than max line number.
           if (bit_istrue(value_words, bit(Word_N)) && gc_block.values.n > MAX_LINE_NUMBER)
               FAIL(Status_GcodeInvalidLineNumber); // [Exceeds max line number]
           // bit_false(value_words,bit(Word_N)); // NOTE: Single-meaning value word. Set at end of error-checking.

           // Track for unused words at the end of error-checking.
           // NOTE: Single-meaning value words are removed all at once at the end of error-checking, because
           // they are always used when present. This was done to save a few bytes of flash. For clarity, the
           // single-meaning value words may be removed as they are used. Also, axis words are treated in the
           // same way. If there is an explicit/implicit axis command, XYZ words are always used and are
           // are removed at the end of error-checking.

           // [1. Comments ]: MSG's may be supported by driver layer. Comment handling performed by protocol.

           // [2. Set feed rate mode ]: G93 F word missing with G1,G2/3 active, implicitly or explicitly. Feed rate
           //   is not defined after switching between G93, G94 and G95.
           // NOTE: For jogging, ignore prior feed rate mode. Enforce G94 and check for required F word.
           if (gc_parser_flags.jog_motion) {

               if(bit_isfalse(value_words, bit(Word_F)))
                   FAIL(Status_GcodeUndefinedFeedRate);

               if (gc_block.modal.units_imperial)
                   gc_block.values.f *= MM_PER_INCH;

           } else if (gc_block.modal.feed_mode == FeedMode_InverseTime) { // = G93
               // NOTE: G38 can also operate in inverse time, but is undefined as an error. Missing F word check added here.
               if (axis_command == AxisCommand_MotionMode) {
                   if (!(gc_block.modal.motion == MotionMode_None || gc_block.modal.motion == MotionMode_Seek)) {
                       if (bit_isfalse(value_words, bit(Word_F)))
                           FAIL(Status_GcodeUndefinedFeedRate); // [F word missing]
                   }
               }
               // NOTE: It seems redundant to check for an F word to be passed after switching from G94 to G93. We would
               // accomplish the exact same thing if the feed rate value is always reset to zero and undefined after each
               // inverse time block, since the commands that use this value already perform undefined checks. This would
               // also allow other commands, following this switch, to execute and not error out needlessly. This code is
               // combined with the above feed rate mode and the below set feed rate error-checking.

               // [3. Set feed rate ]: F is negative (done.)
               // - In inverse time mode: Always implicitly zero the feed rate value before and after block completion.
               // NOTE: If in G93 mode or switched into it from G94, just keep F value as initialized zero or passed F word
               // value in the block. If no F word is passed with a motion command that requires a feed rate, this will error
               // out in the motion modes error-checking. However, if no F word is passed with NO motion command that requires
               // a feed rate, we simply move on and the state feed rate value gets updated to zero and remains undefined.

           } else if (gc_block.modal.feed_mode == FeedMode_UnitsPerMin || gc_block.modal.feed_mode == FeedMode_UnitsPerRev) {
                 // if F word passed, ensure value is in mm/min or mm/rev depending on mode, otherwise push last state value.
               if (bit_isfalse(value_words, bit(Word_F))) {
                   if(gc_block.modal.feed_mode == gc_state.modal.feed_mode)
                       gc_block.values.f = gc_state.feed_rate; // Push last state feed rate
               } else if (gc_block.modal.units_imperial)
                   gc_block.values.f *= MM_PER_INCH;
           } // else, switching to G94 from G93, so don't push last state feed rate. Its undefined or the passed F word value.

           // bit_false(value_words,bit(Word_F)); // NOTE: Single-meaning value word. Set at end of error-checking.

           if(bit_istrue(command_words, bit(ModalGroup_M10)) && port_command) {

               switch(port_command) {

                   case 62:
                   case 63:
                   case 64:
                   case 65:
                       if(bit_isfalse(value_words, bit(Word_P)))
                           FAIL(Status_GcodeValueWordMissing);
                       if(gc_block.values.p < 0.0f)
                           FAIL(Status_NegativeValue);
//                       if((uint32_t)gc_block.values.p + 1 > hal.port.num_digital)
//                           FAIL(Status_GcodeValueOutOfRange);
                       gc_block.output_command.is_digital = true;
                       gc_block.output_command.port = (uint8_t)gc_block.values.p;
                       gc_block.output_command.value = port_command == 62 || port_command == 64 ? 1.0f : 0.0f;
                       bit_false(value_words, bit(Word_P));
                       break;

                   case 66:
                       if(bit_isfalse(value_words, bit(Word_L)|bit(Word_Q)))
                           FAIL(Status_GcodeValueWordMissing);

                       if(bit_istrue(value_words, bit(Word_P)) && bit_istrue(value_words, bit(Word_E)))
                           FAIL(Status_ValueWordConflict);

                       if(gc_block.values.l >= (uint8_t)WaitMode_Max)
                           FAIL(Status_GcodeValueOutOfRange);

                       if((wait_mode_t)gc_block.values.l != WaitMode_Immediate && gc_block.values.q == 0.0f)
                           FAIL(Status_GcodeValueOutOfRange);

                       if(bit_istrue(value_words, bit(Word_P))) {
                           if(gc_block.values.p < 0.0f)
                               FAIL(Status_NegativeValue);
//                           if((uint32_t)gc_block.values.p + 1 > hal.port.num_digital)
//                               FAIL(Status_GcodeValueOutOfRange);

                           gc_block.output_command.is_digital = true;
                           gc_block.output_command.port = (uint8_t)gc_block.values.p;
                       }

                       if(bit_istrue(value_words, bit(Word_E))) {
//                           if((uint32_t)gc_block.values.e + 1 > hal.port.num_analog)
//                               FAIL(Status_GcodeValueOutOfRange);
                           if((wait_mode_t)gc_block.values.l != WaitMode_Immediate)
                               FAIL(Status_GcodeValueOutOfRange);

                           gc_block.output_command.is_digital = false;
                           gc_block.output_command.port = (uint8_t)gc_block.values.e;
                       }

                       bit_false(value_words, bit(Word_E)|bit(Word_L)|bit(Word_P)|bit(Word_Q));
                       break;

                   case 67:
                   case 68:
                       if(bit_isfalse(value_words, bit(Word_E)|bit(Word_Q)))
                           FAIL(Status_GcodeValueWordMissing);
//                       if((uint32_t)gc_block.values.e + 1 > hal.port.num_analog)
//                           FAIL(Status_GcodeRPMOutOfRange);
                       gc_block.output_command.is_digital = false;
                       gc_block.output_command.port = (uint8_t)gc_block.values.e;
                       gc_block.output_command.value = gc_block.values.q;
                       bit_false(value_words, bit(Word_E)|bit(Word_Q));
                   break;
               }
           }
           // [9a. User defined M commands ]:
           if (bit_istrue(command_words, bit(ModalGroup_M10)) && gc_block.user_mcode) {
//               if((int_value = (uint_fast16_t)hal.user_mcode_validate(&gc_block, &value_words)))
//                   FAIL((status_code_t)int_value);
               axis_words = 0;
           }

           // [10. Dwell ]: P value missing. NOTE: See below.
           if (gc_block.non_modal_command == NonModal_Dwell) {
               if (bit_isfalse(value_words, bit(Word_P)))
                   FAIL(Status_GcodeValueWordMissing); // [P word missing]
               if(gc_block.values.p < 0.0f)
                   FAIL(Status_NegativeValue);
               bit_false(value_words, bit(Word_P));
           }
           // [12. Set length units ]: N/A
           // Pre-convert XYZ coordinate values to millimeters, if applicable.
           uint_fast8_t idx = N_AXIS;
//           if (gc_block.modal.units_imperial) do { // Axes indices are consistent, so loop may be used.
//               if (bit_istrue(axis_words, bit(--idx)))
//                   gc_block.values.xyz[idx] *= MM_PER_INCH;
//           } while(idx);
//
//           if (bit_istrue(command_words, bit(ModalGroup_G15))) {
//               sys.report.xmode |= gc_state.modal.diameter_mode != gc_block.modal.diameter_mode;
//               gc_state.modal.diameter_mode = gc_block.modal.diameter_mode;
//           }
//
//           if(gc_state.modal.diameter_mode && bit_istrue(axis_words, bit(X_AXIS)))
//               gc_block.values.xyz[X_AXIS] /= 2.0f;
//
//           // Scale axis words if commanded
//           if(axis_command == AxisCommand_Scaling) {
//
//               if(gc_block.modal.scaling_active) {
//
//                   // TODO: precheck for 0.0f and fail if found?
//
//                   gc_block.modal.scaling_active = false;
//
//                   if (!(value_words & bit(Word_P) ))
//                       FAIL(Status_GcodeNoAxisWords); // [No axis words]
//
//                   idx = N_AXIS;
//                   do {
//                       if(bit_istrue(axis_words, bit(--idx)))
//                           scale_factor.xyz[idx] = gc_block.values.xyz[idx];
//                       else
//                           scale_factor.xyz[idx] = gc_state.position[idx];
//                   } while(idx);
//
//                   bit_false(value_words, AXIS_WORDS_MASK); // Remove axis words.
//
//                   idx = 3;
//                   do {
//                       idx--;
//                       if(value_words & bit(Word_P)) {
////                           sys.report.scaling = sys.report.scaling || scale_factor.ijk[idx] != gc_block.values.p;
//                           scale_factor.ijk[idx] = gc_block.values.p;
//                       } else if(bit_istrue(ijk_words, bit(idx))) {
////                           sys.report.scaling = sys.report.scaling || scale_factor.ijk[idx] != gc_block.values.ijk[idx];
//                           scale_factor.ijk[idx] = gc_block.values.ijk[idx];
//                       }
//                       gc_block.modal.scaling_active = gc_block.modal.scaling_active || (scale_factor.ijk[idx] != 1.0f);
//                   } while(idx);
//
//                   if(value_words & bit(Word_P))
//                       bit_false(value_words, bit(Word_P));
//                   else
//                       bit_false(value_words, bit(Word_I)|bit(Word_J)|bit(Word_K));
////                   sys.report.scaling = sys.report.scaling || gc_state.modal.scaling_active != gc_block.modal.scaling_active;
//                   gc_state.modal.scaling_active = gc_block.modal.scaling_active;
//
//               } //else
////                   set_scaling(1.0f);
//           }
//           // Scale axis words if scaling active
//           if(gc_state.modal.scaling_active) {
//               idx = N_AXIS;
//               do {
//                   if(bit_istrue(axis_words, bit(--idx))) {
//                       if(gc_block.modal.distance_incremental)
//                            gc_block.values.xyz[idx] *= scale_factor.ijk[idx];
//                       else
//                            gc_block.values.xyz[idx] = (gc_block.values.xyz[idx] - scale_factor.xyz[idx]) * scale_factor.ijk[idx] + scale_factor.xyz[idx];
//                   }
//               } while(idx);
//           }
           // [15. Coordinate system selection ]: *N/A. Error, if cutter radius comp is active.
           // TODO: An EEPROM read of the coordinate data may require a buffer sync when the cycle
           // is active. The read pauses the processor temporarily and may cause a rare crash. For
           // future versions on processors with enough memory, all coordinate data should be stored
           // in memory and written to EEPROM only when there is not a cycle active.
           // NOTE: If EEPROM emulation is active then EEPROM reads/writes are buffered and updates
           // delayed until no cycle is active.

//           if (bit_istrue(command_words, bit(ModalGroup_G12))) { // Check if called in block
//               if (gc_block.modal.coord_system.idx > N_COORDINATE_SYSTEM)
//                   FAIL(Status_GcodeUnsupportedCoordSys); // [Greater than N sys]
//               if (gc_state.modal.coord_system.idx != gc_block.modal.coord_system.idx && !settings_read_coord_data(gc_block.modal.coord_system.idx, &gc_block.modal.coord_system.xyz))
//                   FAIL(Status_SettingReadFail);
//           }

           // [16. Set path control mode ]: N/A. Only G61. G61.1 and G64 NOT SUPPORTED.
           // [17. Set distance mode ]: N/A. Only G91.1. G90.1 NOT SUPPORTED.
           // [18. Set retract mode ]: N/A.

           // [19. Remaining non-modal actions ]: Check go to predefined position, set G10, or set axis offsets.
           // NOTE: We need to separate the non-modal commands that are axis word-using (G10/G28/G30/G92), as these
           // commands all treat axis words differently. G10 as absolute offsets or computes current position as
           // the axis value, G92 similarly to G10 L20, and G28/30 as an intermediate target position that observes
           // all the current coordinate system and G92 offsets.
           switch (gc_block.non_modal_command) {

                case NonModal_SetCoordinateData:

                    // [G10 Errors]: L missing and is not 2 or 20. P word missing. (Negative P value done.)
                    // [G10 L2 Errors]: R word NOT SUPPORTED. P value not 0 to N_COORDINATE_SYSTEM (max 9). Axis words missing.
                    // [G10 L20 Errors]: P must be 0 to N_COORDINATE_SYSTEM (max 9). Axis words missing.
                    // [G10 L1, L10, L11 Errors]: P must be 0 to MAX_TOOL_NUMBER (max 9). Axis words or R word missing.

                    if (!(axis_words || (gc_block.values.l != 20 && bit_istrue(value_words, bit(Word_R)))))
                        FAIL(Status_GcodeNoAxisWords); // [No axis words (or R word for tool offsets)]

                    if (bit_isfalse(value_words, bit(Word_P)|bit(Word_L)))
                        FAIL(Status_GcodeValueWordMissing); // [P/L word missing]

                    if(gc_block.values.p < 0.0f)
                        FAIL(Status_NegativeValue);

                    uint8_t p_value;

                    p_value = (uint8_t)truncf(gc_block.values.p); // Convert p value to int.

                    switch(gc_block.values.l) {

                        case 2:
                            if (bit_istrue(value_words, bit(Word_R)))
                                FAIL(Status_GcodeUnsupportedCommand); // [G10 L2 R not supported]
                            // no break

                        case 20:
//                            if (p_value > N_COORDINATE_SYSTEM)
//                                FAIL(Status_GcodeUnsupportedCoordSys); // [Greater than N sys]
//                            // Determine coordinate system to change and try to load from EEPROM.
//                            gc_block.values.coord_data.idx = p_value == 0
//                                                              ? gc_block.modal.coord_system.idx // Index P0 as the active coordinate system
//                                                              : (p_value - 1); // else adjust index to EEPROM coordinate data indexing.
//
//                            if (!settings_read_coord_data(gc_block.values.coord_data.idx, &gc_block.values.coord_data.xyz))
//                                FAIL(Status_SettingReadFail); // [EEPROM read fail]
//
//                            // Pre-calculate the coordinate data changes.
//                            idx = N_AXIS;
//                            do { // Axes indices are consistent, so loop may be used.
//                                // Update axes defined only in block. Always in machine coordinates. Can change non-active system.
//                                if (bit_istrue(axis_words, bit(--idx))) {
//                                    if (gc_block.values.l == 20)
//                                        // L20: Update coordinate system axis at current position (with modifiers) with programmed value
//                                        // WPos = MPos - WCS - G92 - TLO  ->  WCS = MPos - G92 - TLO - WPos
//                                        gc_block.values.coord_data.xyz[idx] = gc_state.position[idx] - gc_block.values.xyz[idx] - gc_state.g92_coord_offset[idx] - gc_state.tool_length_offset[idx];
//                                    else // L2: Update coordinate system axis to programmed value.
//                                        gc_block.values.coord_data.xyz[idx] = gc_block.values.xyz[idx];
//                                } // else, keep current stored value.
//                            } while(idx);
                            break;


                        default:
                            FAIL(Status_GcodeUnsupportedCommand); // [Unsupported L]
                    }
                    bit_false(value_words, bit(Word_L)|bit(Word_P));
                    break;

                case NonModal_SetCoordinateOffset:

                    // [G92 Errors]: No axis words.
                    if (!axis_words)
                        FAIL(Status_GcodeNoAxisWords); // [No axis words]

                    // Update axes defined only in block. Offsets current system to defined value. Does not update when
                    // active coordinate system is selected, but is still active unless G92.1 disables it.
                    idx = N_AXIS;
                    do { // Axes indices are consistent, so loop may be used.
                        if (bit_istrue(axis_words, bit(--idx))) {
                    // WPos = MPos - WCS - G92 - TLO  ->  G92 = MPos - WCS - TLO - WPos
                            gc_block.values.xyz[idx] = gc_state.position[idx] - gc_block.modal.coord_system.xyz[idx] - gc_block.values.xyz[idx] - gc_state.tool_length_offset[idx];
                        } else
                            gc_block.values.xyz[idx] = gc_state.g92_coord_offset[idx];
                    } while(idx);
                    break;

                default:

                    // At this point, the rest of the explicit axis commands treat the axis values as the traditional
                    // target position with the coordinate system offsets, G92 offsets, absolute override, and distance
                    // modes applied. This includes the motion mode commands. We can now pre-compute the target position.
                    // NOTE: Tool offsets may be appended to these conversions when/if this feature is added.
                    if (axis_words && axis_command != AxisCommand_ToolLengthOffset) { // TLO block any axis command.
                        idx = N_AXIS;
                        do { // Axes indices are consistent, so loop may be used to save flash space.
                            if (bit_isfalse(axis_words, bit(--idx)))
                                gc_block.values.xyz[idx] = gc_state.position[idx]; // No axis word in block. Keep same axis position.
                            else if (gc_block.non_modal_command != NonModal_AbsoluteOverride) {
                                // Update specified value according to distance mode or ignore if absolute override is active.
                                // NOTE: G53 is never active with G28/30 since they are in the same modal group.
                                // Apply coordinate offsets based on distance mode.
                                if (gc_block.modal.distance_incremental)
                                    gc_block.values.xyz[idx] += gc_state.position[idx];
                                else  // Absolute mode
                                    gc_block.values.xyz[idx] += gc_get_block_offset(&gc_block, idx);
                            }
                        } while(idx);
                    }

                    // Check remaining non-modal commands for errors.
                    switch (gc_block.non_modal_command) {

                        case NonModal_GoHome_0: // G28
                        case NonModal_GoHome_1: // G30
                            // [G28/30 Errors]: Cutter compensation is enabled.
                            // Retreive G28/30 go-home position data (in machine coordinates) from EEPROM

//                            if (!settings_read_coord_data(gc_block.non_modal_command == NonModal_GoHome_0 ? SETTING_INDEX_G28 : SETTING_INDEX_G30, &gc_block.values.coord_data.xyz))
//                                FAIL(Status_SettingReadFail);

                            if (axis_words) {
                                // Move only the axes specified in secondary move.
                                idx = N_AXIS;
                                do {
                                    if (bit_isfalse(axis_words, bit(--idx)))
                                        gc_block.values.coord_data.xyz[idx] = gc_state.position[idx];
                                } while(idx);
                            } else
                                axis_command = AxisCommand_None; // Set to none if no intermediate motion.
                            break;

                        case NonModal_SetHome_0: // G28.1
                        case NonModal_SetHome_1: // G30.1
                            // [G28.1/30.1 Errors]: Cutter compensation is enabled.
                            // NOTE: If axis words are passed here, they are interpreted as an implicit motion mode.
                            break;

                        case NonModal_ResetCoordinateOffset:
                            // NOTE: If axis words are passed here, they are interpreted as an implicit motion mode.
                            break;

                        case NonModal_AbsoluteOverride:
                            // [G53 Errors]: G0 and G1 are not active. Cutter compensation is enabled.
                            // NOTE: All explicit axis word commands are in this modal group. So no implicit check necessary.
                            if (!(gc_block.modal.motion == MotionMode_Seek || gc_block.modal.motion == MotionMode_Linear))
                                FAIL(Status_GcodeG53InvalidMotionMode); // [G53 G0/1 not active]
                            break;

                        default:
                            break;
                    }
            } // end gc_block.non_modal_command


           // [20. Motion modes ]:
           if (gc_block.modal.motion == MotionMode_None) {

               // [G80 Errors]: Axis word are programmed while G80 is active.
               // NOTE: Even non-modal commands or TLO that use axis words will throw this strict error.
               if (axis_words) // [No axis words allowed]
                   FAIL(Status_GcodeAxisWordsExist);

               gc_block.modal.retract_mode = CCRetractMode_Previous;

           // Check remaining motion modes, if axis word are implicit (exist and not used by G10/28/30/92), or
           // was explicitly commanded in the g-code block.
           } else if (axis_command == AxisCommand_MotionMode) {

               gc_parser_flags.motion_mode_changed = gc_block.modal.motion != gc_state.modal.motion;

               if (gc_block.modal.motion == MotionMode_Seek) {
                   // [G0 Errors]: Axis letter not configured or without real value (done.)
                   // Axis words are optional. If missing, set axis command flag to ignore execution.
                   if (!axis_words)
                       axis_command = AxisCommand_None;

               // All remaining motion modes (all but G0 and G80), require a valid feed rate value. In units per mm mode,
               // the value must be positive. In inverse time mode, a positive value must be passed with each block.
               } else {

                   if(!gc_block.modal.canned_cycle_active)
                       gc_block.modal.retract_mode = CCRetractMode_Previous;

                   // Initial(?) check for spindle running for moves in G96 mode
//                   if(gc_block.modal.spindle_rpm_mode == SpindleSpeedMode_CSS && (!gc_block.modal.spindle.on || gc_block.values.s == 0.0f))
//                        FAIL(Status_GcodeSpindleNotRunning);

                   // Check if feed rate is defined for the motion modes that require it.
                   if (gc_block.modal.motion == MotionMode_SpindleSynchronized) {

                       if(gc_block.values.k == 0.0f)
                           FAIL(Status_GcodeValueOutOfRange); // [No distance (pitch) given]

                       // Ensure spindle speed is at 100% - any override will be disabled on execute.
                       gc_parser_flags.spindle_force_sync = On;

                   }


                    else if (gc_block.values.f == 0.0f)
                       FAIL(Status_GcodeUndefinedFeedRate); // [Feed rate undefined]


                   switch (gc_block.modal.motion) {

                       case MotionMode_Linear:
                           // [G1 Errors]: Feed rate undefined. Axis letter not configured or without real value.
                           // Axis words are optional. If missing, set axis command flag to ignore execution.
                           if (!axis_words)
                               axis_command = AxisCommand_None;
                           break;


                       case MotionMode_ProbeTowardNoError:
                       case MotionMode_ProbeAwayNoError:
                           gc_parser_flags.probe_is_no_error = On;
                           // No break intentional.

                       case MotionMode_ProbeToward:
                       case MotionMode_ProbeAway:
                           if(gc_block.modal.motion == MotionMode_ProbeAway || gc_block.modal.motion == MotionMode_ProbeAwayNoError)
                               gc_parser_flags.probe_is_away = On;
                           // [G38 Errors]: Target is same current. No axis words. Cutter compensation is enabled. Feed rate
                           //   is undefined. Probe is triggered. NOTE: Probe check moved to probe cycle. Instead of returning
                           //   an error, it issues an alarm to prevent further motion to the probe. It's also done there to
                           //   allow the planner buffer to empty and move off the probe trigger before another probing cycle.
                           if (!axis_words)
                               FAIL(Status_GcodeNoAxisWords); // [No axis words]
                           if (isequal_position_vector(gc_state.position, gc_block.values.xyz))
                               FAIL(Status_GcodeInvalidTarget); // [Invalid target]
                           break;

                       default:
                           break;

                   } // end switch gc_block.modal.motion
               }
           }
           // [21. Program flow ]: No error checks required.

           // [0. Non-specific error-checks]: Complete unused value words check, i.e. IJK used when in arc
           // radius mode, or axis words that aren't used in the block.
           if (gc_parser_flags.jog_motion) // Jogging only uses the F feed rate and XYZ value words. N is valid, but S and T are invalid.
               bit_false(value_words, bit(Word_N)|bit(Word_F));
           else
               bit_false(value_words, bit(Word_N)|bit(Word_F)|bit(Word_S)|bit(Word_T)); // Remove single-meaning value words.

           if (axis_command)
               bit_false(value_words, AXIS_WORDS_MASK); // Remove axis words.

           if (value_words)
               FAIL(Status_GcodeUnusedWords); // [Unused words]



           /* -------------------------------------------------------------------------------------
            STEP 4: EXECUTE!!
            Assumes that all error-checking has been completed and no failure modes exist. We just
            need to update the state and execute the block according to the order-of-execution.
           */
           // Initialize planner data struct for motion blocks.
           // TODO New plandata fpr RTOS
//           plan_line_data_t plan_data;
//           memset(&plan_data, 0, sizeof(plan_line_data_t)); // Zero plan_data struct

           // Intercept jog commands and complete error checking for valid jog commands and execute.
           // NOTE: G-code parser state is not updated, except the position to ensure sequential jog
           // targets are computed correctly. The final parser position after a jog is updated in
           // protocol_execute_realtime() when jogging completes or is canceled.
           if (gc_parser_flags.jog_motion) {

               // Only distance and unit modal commands and G53 absolute override command are allowed.
               // NOTE: Feed rate word and axis word checks have already been performed in STEP 3.
               if (command_words & ~(bit(ModalGroup_G3)|bit(ModalGroup_G6)|bit(ModalGroup_G0)))
                   FAIL(Status_InvalidJogCommand);

               if (!(gc_block.non_modal_command == NonModal_AbsoluteOverride || gc_block.non_modal_command == NonModal_NoAction))
                   FAIL(Status_InvalidJogCommand);


//               if ((status_code_t)(int_value = (uint_fast16_t)mc_jog_execute(&plan_data, &gc_block)) == Status_OK)
//                   memcpy(gc_state.position, gc_block.values.xyz, sizeof(gc_state.position));

               return (status_code_t)int_value;
           }
           // [0. Non-specific/common error-checks and miscellaneous setup]:
           // NOTE: If no line number is present, the value is zero.
           gc_state.line_number = gc_block.values.n;
//           plan_data.line_number = gc_state.line_number; // Record data for planner use.

           // [1. Comments feedback ]: Extracted in protocol.c if HAL entry point provided
//           if(message && (plan_data.message = malloc(strlen(message) + 1)))
//               strcpy(plan_data.message, message);

           // [2. Set feed rate mode ]:
           gc_state.modal.feed_mode = gc_block.modal.feed_mode;
//           if (gc_state.modal.feed_mode == FeedMode_InverseTime)
//               plan_data.condition.inverse_time = On; // Set condition flag for planner use.

           // [3. Set feed rate ]:
           gc_state.feed_rate = gc_block.values.f; // Always copy this value. See feed rate error-checking.
//           plan_data.feed_rate = gc_state.feed_rate; // Record data for planner use.

           // [4. Set spindle speed ]:

           // [5. Select tool ]: Only tracks tool value if ATC or manual tool change is not possible.

           // [5a. HAL pin I/O ]: M62 - M68. (Modal group M10)

           if(port_command) {

               switch(port_command) {

                   case 62:
                   case 63:
                       add_output_command(&gc_block.output_command);
                       break;

                   case 64:
                   case 65:
//                       hal.port.digital_out(gc_block.output_command.port, gc_block.output_command.value != 0.0f);
                       break;

                   case 66:
//                       hal.port.wait_on_input(gc_block.output_command.is_digital, gc_block.output_command.port, (wait_mode_t)gc_block.values.l, gc_block.values.q);
                       break;

                   case 67:
                       add_output_command(&gc_block.output_command);
                       break;

                   case 68:
//                       hal.port.analog_out(gc_block.output_command.port, gc_block.output_command.value);
                       break;
               }
           }

           // [6. Change tool ]: Delegated to (possible) driver implementation
           // [7. Spindle control ]:

           // [8. Coolant control ]:

           // [9. Override control ]:

           // [9a. User defined M commands ]:
//           if(gc_block.user_mcode && sys.state != STATE_CHECK_MODE) {
//
//               if(gc_block.user_mcode_sync)
//                   protocol_buffer_synchronize(); // Ensure user defined mcode is executed when specified in program.
//
//               hal.user_mcode_execute(sys.state, &gc_block);
//           }

           // [10. Dwell ]:
//           if (gc_block.non_modal_command == NonModal_Dwell)
//               mc_dwell(gc_block.values.p);

           // [11. Set active plane ]:
           gc_state.modal.plane_select = gc_block.modal.plane_select;

           // [12. Set length units ]:
           gc_state.modal.units_imperial = gc_block.modal.units_imperial;

           // [13. Cutter radius compensation ]: G41/42 NOT SUPPORTED
           // gc_state.modal.cutter_comp = gc_block.modal.cutter_comp; // NOTE: Not needed since always disabled.

           // [14. Tool length compensation ]: G43, G43.1 and G49 supported. G43 supported when N_TOOLS defined.
           // NOTE: If G43 were supported, its operation wouldn't be any different from G43.1 in terms
           // of execution. The error-checking step would simply load the offset value into the correct
           // axis of the block XYZ value array.


           // [15. Coordinate system selection ]:


           // [16. Set path control mode ]: G61.1/G64 NOT SUPPORTED


           // [17. Set distance mode ]:
           gc_state.modal.distance_incremental = gc_block.modal.distance_incremental;

           // [18. Set retract mode ]:
           gc_state.modal.retract_mode = gc_block.modal.retract_mode;

           // [19. Go to predefined position, Set G10, or Set axis offsets ]:
           switch(gc_block.non_modal_command) {

               case NonModal_SetCoordinateData:
//                   settings_write_coord_data(gc_block.values.coord_data.idx, &gc_block.values.coord_data.xyz);
//                   // Update system coordinate system if currently active.
//                   if (gc_state.modal.coord_system.idx == gc_block.values.coord_data.idx) {
//                       memcpy(gc_state.modal.coord_system.xyz, gc_block.values.coord_data.xyz, sizeof(gc_state.modal.coord_system.xyz));
//                       system_flag_wco_change();
//                   }
                   break;

               case NonModal_GoHome_0:
               case NonModal_GoHome_1:
                   // Move to intermediate position before going home. Obeys current coordinate system and offsets
                   // and absolute and incremental modes.
//                   plan_data.condition.rapid_motion = On; // Set rapid motion condition flag.
//                   if (axis_command)
//                       mc_line(gc_block.values.xyz, &plan_data);
//                   mc_line(gc_block.values.coord_data.xyz, &plan_data);
//                   memcpy(gc_state.position, gc_block.values.coord_data.xyz, sizeof(gc_state.position));
//                   set_scaling(1.0f);
            	   printf("GoHome\r\n");
                   break;

               case NonModal_SetHome_0:
//                   settings_write_coord_data(SETTING_INDEX_G28, &gc_state.position);
                   break;

               case NonModal_SetHome_1:
//                   settings_write_coord_data(SETTING_INDEX_G30, &gc_state.position);
                   break;

               case NonModal_SetCoordinateOffset: // G92
                   memcpy(gc_state.g92_coord_offset, gc_block.values.xyz, sizeof(gc_state.g92_coord_offset));
//       #if COMPATIBILITY_LEVEL <= 1
//                   settings_write_coord_data(SETTING_INDEX_G92, &gc_state.g92_coord_offset); // Save G92 offsets to EEPROM
//       #endif
//                   system_flag_wco_change();
                   break;

               case NonModal_ResetCoordinateOffset: // G92.1
                   clear_vector(gc_state.g92_coord_offset); // Disable G92 offsets by zeroing offset vector.
//                   settings_write_coord_data(SETTING_INDEX_G92, &gc_state.g92_coord_offset); // Save G92 offsets to EEPROM
//                   system_flag_wco_change();
                   break;

               case NonModal_ClearCoordinateOffset: // G92.2
                   clear_vector(gc_state.g92_coord_offset); // Disable G92 offsets by zeroing offset vector.
//                   system_flag_wco_change();
                   break;

               case NonModal_RestoreCoordinateOffset: // G92.3
//                   settings_read_coord_data(SETTING_INDEX_G92, &gc_state.g92_coord_offset); // Restore G92 offsets from EEPROM
//                   system_flag_wco_change();
                   break;

               default:
                   break;
           }

           // [20. Motion modes ]:
           // NOTE: Commands G10,G28,G30,G92 lock out and prevent axis words from use in motion modes.
           // Enter motion modes only if there are axis words or a motion mode command word in the block.
           gc_state.modal.motion = gc_block.modal.motion;

           if (gc_state.modal.motion != MotionMode_None && axis_command == AxisCommand_MotionMode) {

//               plan_data.output_commands = output_commands;
               output_commands = NULL;

               pos_update_t gc_update_pos = GCUpdatePos_Target;

               switch(gc_state.modal.motion) {

                   case MotionMode_Linear:

//                       //??    gc_state.distance_per_rev = plan_data.feed_rate;
//                           // check initial feed rate - fail if zero?
//                       }
//                       mc_line(gc_block.values.xyz, &plan_data);
                	   printf("MotionMode_Linear\r\n");
                       break;

                   case MotionMode_Seek:
//                       plan_data.condition.rapid_motion = On; // Set rapid motion condition flag.
//                       mc_line(gc_block.values.xyz, &plan_data);
                	   mc_line(&gc_block);
                	   printf("MotionMode_Seek\r\n");

                       break;

                   case MotionMode_CwArc:
                   case MotionMode_CcwArc:
//                       // fail if spindle synchronized motion?
//                       mc_arc(gc_block.values.xyz, &plan_data, gc_state.position, gc_block.values.ijk, gc_block.values.r,
//                               plane, gc_parser_flags.arc_is_clockwise);
                       break;

                   case MotionMode_CubicSpline:
//                       mc_cubic_b_spline(gc_block.values.xyz, &plan_data, gc_state.position, gc_block.values.ijk, gc_state.modal.spline_pq);
                       break;

                   case MotionMode_SpindleSynchronized:
//                       {
//                           protocol_buffer_synchronize(); // Wait until any previous moves are finished.
//
//                           gc_override_flags_t overrides = sys.override.control; // Save current override disable status.
//
//                           status_code_t status = init_sync_motion(&plan_data, gc_block.values.k);
//                           if(status != Status_OK)
//                               FAIL(status);
//
////                           plan_data.condition.spindle.synchronized = On;
//
//                           mc_line(gc_block.values.xyz, &plan_data);
//
//                           protocol_buffer_synchronize();    // Wait until synchronized move is finished,
//                           sys.override.control = overrides; // then restore previous override disable status.
//                       }
                       break;

                   case MotionMode_Threading:
//                       {
//                           protocol_buffer_synchronize(); // Wait until any previous moves are finished.
//
//                           gc_override_flags_t overrides = sys.override.control; // Save current override disable status.
//
//                           status_code_t status = init_sync_motion(&plan_data, thread.pitch);
//                           if(status != Status_OK)
//                               FAIL(status);
//
//                           mc_thread(&plan_data, gc_state.position, &thread, overrides.feed_hold_disable);
//
//                           sys.override.control = overrides; // then restore previous override disable status.
//                       }
                       break;

                   case MotionMode_DrillChipBreak:
                   case MotionMode_CannedCycle81:
                   case MotionMode_CannedCycle82:
                   case MotionMode_CannedCycle83:;

                   case MotionMode_ProbeToward:
                   case MotionMode_ProbeTowardNoError:
                   case MotionMode_ProbeAway:
                   case MotionMode_ProbeAwayNoError:

                       break;

                   default:
                       break;
               }

               // Do not update position on cancel (already done in protocol_exec_rt_system)
//               if(sys.cancel)
//                   gc_update_pos = GCUpdatePos_None;

               //  Clean out any remaining output commands (may linger on error)
               //TODO hanging in ininit loop
//               while(plan_data.output_commands) {
//                   output_command_t *next = plan_data.output_commands;
//                   free(plan_data.output_commands);
//                   plan_data.output_commands = next;
//               }

               // As far as the parser is concerned, the position is now == target. In reality the
               // motion control system might still be processing the action and the real tool position
               // in any intermediate location.
               if (gc_update_pos == GCUpdatePos_Target)
                   memcpy(gc_state.position, gc_block.values.xyz, sizeof(gc_state.position)); // gc_state.position[] = gc_block.values.xyz[]
//               else if (gc_update_pos == GCUpdatePos_System)
//                   gc_sync_position(); // gc_state.position[] = sys_position
               // == GCUpdatePos_None
           }

//           if(plan_data.message)
//               protocol_message(plan_data.message);

           // [21. Program flow ]:
           // M0,M1,M2,M30: Perform non-running program flow actions. During a program pause, the buffer may
           // refill and can only be resumed by the cycle start run-time command.
           gc_state.modal.program_flow = gc_block.modal.program_flow;

           if (gc_state.modal.program_flow) {

//               protocol_buffer_synchronize(); // Sync and finish all remaining buffered motions before moving on.

               if (gc_state.modal.program_flow == ProgramFlow_Paused || gc_block.modal.program_flow == ProgramFlow_OptionalStop) {
//                   if (sys.state != STATE_CHECK_MODE)
            	   {
//                       system_set_exec_state_flag(EXEC_FEED_HOLD); // Use feed hold for program pause.
//                       protocol_execute_realtime(); // Execute suspend.
                   }
               } else { // == ProgramFlow_Completed
                   // Upon program complete, only a subset of g-codes reset to certain defaults, according to
                   // LinuxCNC's program end descriptions and testing. Only modal groups [G-code 1,2,3,5,7,12]
                   // and [M-code 7,8,9] reset to [G1,G17,G90,G94,G40,G54,M5,M9,M48]. The remaining modal groups
                   // [G-code 4,6,8,10,13,14,15] and [M-code 4,5,6] and the modal words [F,S,T,H] do not reset.
                   gc_state.file_run = false;
                   gc_state.modal.motion = MotionMode_Linear;
                   gc_block.modal.canned_cycle_active = false;
                   gc_state.modal.plane_select = PlaneSelect_XY;
       //            gc_state.modal.plane_select = settings.flags.lathe_mode ? PlaneSelect_ZX : PlaneSelect_XY;
                   gc_state.modal.spindle_rpm_mode = SpindleSpeedMode_RPM; // NOTE: not compliant with linuxcnc (?)
                   gc_state.modal.distance_incremental = false;
                   gc_state.modal.feed_mode = FeedMode_UnitsPerMin;
       // TODO: check           gc_state.distance_per_rev = 0.0f;
                   // gc_state.modal.cutter_comp = CUTTER_COMP_DISABLE; // Not supported.
                   gc_state.modal.coord_system.idx = 0; // G54
//                   gc_state.modal.spindle = (spindle_state_t){0};
//                   gc_state.modal.coolant = (coolant_state_t){0};
                   gc_state.modal.override_ctrl.feed_rate_disable = Off;
                   gc_state.modal.override_ctrl.spindle_rpm_disable = Off;
//                   if(settings.parking.flags.enabled)
//                       gc_state.modal.override_ctrl.parking_disable = settings.parking.flags.enable_override_control &&
//                                                                       settings.parking.flags.deactivate_upon_init;
//                   sys.override.control = gc_state.modal.override_ctrl;

//                   if(settings.flags.restore_overrides) {
//                       sys.override.feed_rate = DEFAULT_FEED_OVERRIDE;
//                       sys.override.rapid_rate = DEFAULT_RAPID_OVERRIDE;
//                       sys.override.spindle_rpm = DEFAULT_SPINDLE_RPM_OVERRIDE;
//                   }

                   // Execute coordinate change and spindle/coolant stop.
//                   if (sys.state != STATE_CHECK_MODE)
//                   {
//
//                       float g92_offset_stored[N_AXIS];
//                       if(settings_read_coord_data(SETTING_INDEX_G92, &g92_offset_stored) && !isequal_position_vector(g92_offset_stored, gc_state.g92_coord_offset))
//                           settings_write_coord_data(SETTING_INDEX_G92, &gc_state.g92_coord_offset); // Save G92 offsets to EEPROM
//
//                       if (!(settings_read_coord_data(gc_state.modal.coord_system.idx, &gc_state.modal.coord_system.xyz)))
//                           FAIL(Status_SettingReadFail);
//                       system_flag_wco_change(); // Set to refresh immediately just in case something altered.
////                       hal.spindle_set_state(gc_state.modal.spindle, 0.0f);
////                       hal.coolant_set_state(gc_state.modal.coolant);
//                       sys.report.spindle = On; // Set to report change immediately
//                       sys.report.coolant = On; // ...
//                   }

                   // Clear any pending output commands
                   while(output_commands) {
                       output_command_t *next = output_commands->next;
                       free(output_commands);
                       output_commands = next;
                   }

//                   hal.report.feedback_message(Message_ProgramEnd);
               }
               gc_state.modal.program_flow = ProgramFlow_Running; // Reset program flow.
           }

           // TODO: % to denote start of program.

           return Status_OK;
}
/*-----------------------------------------------------------------------------------*/
static void
gcode_thread(void *arg)
{
  extern QueueHandle_t xInQueue;

  char inbuff[50];
  char message[50];
  LWIP_UNUSED_ARG(arg);

  xInQueue = xQueueCreate(10, 50);
  if( xInQueue == NULL )
  {
  	PRINTF("Could not create InQueue");
  }
  vTaskDelay(1000);
  if( xInQueue != NULL )
  {
	 for( ;; )
	 {
		// Get new GCode line from queue
		if (xQueueReceive(xInQueue, &inbuff, portMAX_DELAY ) == pdPASS)
		{
//			printf("%s", inbuff);
//			printf("\r\n");
			parseBlock(&inbuff, &message);

		}
	}
     vTaskDelay(1);
  }

}
/*-----------------------------------------------------------------------------------*/
void
gcode_init(void)
{
  sys_thread_new("gcode_thread", gcode_thread, NULL, 500, 10);
}
/*-----------------------------------------------------------------------------------*/

