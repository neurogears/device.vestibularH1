#include "hwbp_core.h"
#include "hwbp_core_regs.h"
#include "hwbp_core_types.h"

#include "app.h"
#include "app_funcs.h"
#include "app_ios_and_regs.h"

#include "spi.h"
#include "PAA5100JE.h"
#include "PMW3360.h"

/************************************************************************/
/* Declare application registers                                        */
/************************************************************************/
extern AppRegs app_regs;
extern uint8_t app_regs_type[];
extern uint16_t app_regs_n_elements[];
extern uint8_t *app_regs_pointer[];
extern void (*app_func_rd_pointer[])(void);
extern bool (*app_func_wr_pointer[])(void*);

/************************************************************************/
/* Initialize app                                                       */
/************************************************************************/
static const uint8_t default_device_name[] = "VestibularH1";

void hwbp_app_initialize(void)
{
    /* Define versions */
    uint8_t hwH = 1;
    uint8_t hwL = 0;
    uint8_t fwH = 1;
    uint8_t fwL = 1;
    uint8_t ass = 0;
    
   	/* Start core */
    core_func_start_core(
        1224,
        hwH, hwL,
        fwH, fwL,
        ass,
        (uint8_t*)(&app_regs),
        APP_NBYTES_OF_REG_BANK,
        APP_REGS_ADD_MAX - APP_REGS_ADD_MIN + 1,
        default_device_name,
        true,	// The device is able to repeat the harp timestamp clock
        false,	// The device is not able to generate the harp timestamp clock
        0		// Default timestamp offset
    );
}

/************************************************************************/
/* Handle if a catastrophic error occur                                 */
/************************************************************************/
void core_callback_catastrophic_error_detected(void)
{
	
}

/************************************************************************/
/* User functions                                                       */
/************************************************************************/
/* Add your functions here or load external functions if needed */

/************************************************************************/
/* Initialization Callbacks                                             */
/************************************************************************/
#define FLOW_USED_NONE 0
#define FLOW_USED_PAA5100JE 1
#define FLOW_USED_PMW3360 2

uint8_t flow0_used = FLOW_USED_NONE;
uint8_t flow1_used = FLOW_USED_NONE;

void core_callback_define_clock_default(void)
{
	/* By default, the device will be used as a clock repeater */
	core_device_to_clock_repeater();
}
uint16_t cpi_;

void core_callback_initialize_hardware(void)
{
	/* Initialize IOs */
	/* Don't delete this function!!! */
	init_ios();
	
	/* Check if PAA5100JE is connected */
	spi_mode0_initialize_flow0();
	spi_mode0_initialize_flow1();
	if (optical_tracking_initialize_flow0() == true) flow0_used = FLOW_USED_PAA5100JE;
	if (optical_tracking_initialize_flow1() == true) flow1_used = FLOW_USED_PAA5100JE;
	
	/* Check if PMW3360 is connected */
	if (flow0_used == FLOW_USED_NONE) spi_mode3_initialize_flow0();
	if (flow1_used == FLOW_USED_NONE) spi_mode3_initialize_flow1();
	
	if (flow0_used == FLOW_USED_NONE) if (optical_tracking_initialize_pwm3360_0() == true) flow0_used = FLOW_USED_PMW3360;
	if (flow1_used == FLOW_USED_NONE) if (optical_tracking_initialize_pwm3360_1() == true) flow1_used = FLOW_USED_PMW3360;
	
	/* If PMW3360 is connected to port 0 */
	//set_cpi_pmw3360_0(12000); // Default is 5000
	
	/* If PMW3360 is connected to port 1 */
	//set_cpi_pmw3360_1(12000); // Default it 5000
	
	/* Initialize serial with 100 KHz */
	uint16_t BSEL = 19;
	int8_t BSCALE = 0;
		
	USARTD0_CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;
	USARTD0_BAUDCTRLA = *((uint8_t*)&BSEL);
	USARTD0_BAUDCTRLB = (*(1+(uint8_t*)&BSEL) & 0x0F) | ((BSCALE<<4) & 0xF0);
	USARTD0_CTRLB = USART_TXEN_bm;
}

void core_callback_reset_registers(void)
{
	/* Initialize registers */
	app_regs.REG_CAM0_TRIGGER_FREQUENCY = 120;		// 120 Hz
	app_regs.REG_CAM0_TRIGGER_DURATION_US = 1000;	// 1 mS
	
	app_regs.REG_CAM1_TRIGGER_FREQUENCY = 120;		// 120 Hz
	app_regs.REG_CAM1_TRIGGER_DURATION_US = 1000;	// 1 mS
		
	app_regs.REG_VALVE0_PULSE = 10; // 10 ms
	app_regs.REG_VALVE1_PULSE = 10; // 10 ms
	
	app_regs.REG_OUT_SET = 0;
	app_regs.REG_OUT_CLEAR = 0;
	app_regs.REG_OUT_TOGGLE = 0;
	app_regs.REG_OUT_WRITE = 0;
}

void core_callback_registers_were_reinitialized(void)
{
	/* Update registers if needed */
	app_read_REG_IN_STATE();
	
	app_write_REG_CAM0_TRIGGER_FREQUENCY(&app_regs.REG_CAM0_TRIGGER_FREQUENCY);
	app_write_REG_CAM0_TRIGGER_DURATION_US(&app_regs.REG_CAM0_TRIGGER_DURATION_US);
	
	app_write_REG_CAM1_TRIGGER_FREQUENCY(&app_regs.REG_CAM1_TRIGGER_FREQUENCY);
	app_write_REG_CAM1_TRIGGER_DURATION_US(&app_regs.REG_CAM1_TRIGGER_DURATION_US);
	
}

/************************************************************************/
/* Callbacks: Visualization                                             */
/************************************************************************/
void core_callback_visualen_to_on(void)
{
	/* Update visual indicators */
	
}

void core_callback_visualen_to_off(void)
{
	/* Clear all the enabled indicators */
	
}

/************************************************************************/
/* Callbacks: Change on the operation mode                              */
/************************************************************************/
void core_callback_device_to_standby(void) {}
void core_callback_device_to_active(void) {}
void core_callback_device_to_enchanced_active(void) {}
void core_callback_device_to_speed(void) {}

/************************************************************************/
/* Callbacks: 1 ms timer                                                */
/************************************************************************/
uint8_t pulse_countdown_valve0 = 0;
uint8_t pulse_countdown_valve1 = 0;

uint16_t optical_counter = 0;
uint16_t optical_counter_divider = 10; // 200Hz

extern bool cam0_start_request;
extern bool cam1_start_request;
extern bool cam0_stop_request;
extern bool cam1_stop_request;

extern bool cam0_acquiring;
extern bool cam1_acquiring;

extern uint8_t cam0_freq_prescaler;
extern uint8_t cam1_freq_prescaler;
extern uint16_t cam0_freq_target_count;
extern uint16_t cam1_freq_target_count;
extern uint16_t cam0_freq_dutycyle;
extern uint16_t cam1_freq_dutycyle;

bool stop_cam0_when_possible = false;
bool stop_cam1_when_possible = false;

void core_callback_t_before_exec(void) {}
void core_callback_t_after_exec(void) {}
void core_callback_t_new_second(void)
{	
	optical_counter = optical_counter_divider-1;
	
	if (cam0_start_request)
	{
		cam0_start_request = false;
		cam0_acquiring = true;
		
		timer_type0_pwm(&TCC0, cam0_freq_prescaler, cam0_freq_target_count, cam0_freq_dutycyle, INT_LEVEL_LOW, INT_LEVEL_LOW);
	}

	if (cam1_start_request)
	{
		cam1_start_request = false;
		cam1_acquiring = true;
		
		timer_type0_pwm(&TCD0, cam1_freq_prescaler, cam1_freq_target_count, cam1_freq_dutycyle, INT_LEVEL_LOW, INT_LEVEL_LOW);
	}
}
void core_callback_t_500us(void)
{
	if (pulse_countdown_valve0 > 0)
		if (--pulse_countdown_valve0 == 0)
			clr_VALVE0;
	
	if (pulse_countdown_valve1 > 0)
		if (--pulse_countdown_valve1 == 0)
			clr_VALVE1;
}

int16_t motor_pulse_interval;

void core_callback_t_1ms(void)
{
	if (++optical_counter == optical_counter_divider)
	{		
		Motion optical_motion_flow0;
		Motion optical_motion_flow1;
		
		if (flow0_used == FLOW_USED_PAA5100JE)
		{
			optical_tracking_read_motion_optimized(&optical_motion_flow0, &optical_motion_flow1);
		}
		if (flow0_used == FLOW_USED_PMW3360)
		{
			optical_tracking_read_motion_optimized_pmw3360(&optical_motion_flow0, &optical_motion_flow1);
		}		
		
		memcpy(app_regs.REG_REG_OPTICAL_TRACKING_READ+0, ((uint8_t*)(&optical_motion_flow0))+2,5);
		memcpy(app_regs.REG_REG_OPTICAL_TRACKING_READ+3, ((uint8_t*)(&optical_motion_flow1))+2,5);
		*(((uint8_t*)(&app_regs.REG_REG_OPTICAL_TRACKING_READ[2]))+1) = 0;	// Clear high byte of [2]
		*(((uint8_t*)(&app_regs.REG_REG_OPTICAL_TRACKING_READ[5]))+1) = 0;	// Clear high byte of [2]
				
		if (app_regs.REG_MCA_SIGNAL_SELECT != MSK_MCA_OFF)
		{
			int16_t input;
			
			switch (app_regs.REG_MCA_SIGNAL_SELECT)
			{
				case MSK_MCA_FLOW0_X: input = app_regs.REG_REG_OPTICAL_TRACKING_READ[0]; break;
				case MSK_MCA_FLOW0_Y: input = app_regs.REG_REG_OPTICAL_TRACKING_READ[1]; break;
				case MSK_MCA_FLOW1_X: input = app_regs.REG_REG_OPTICAL_TRACKING_READ[3]; break;
				case MSK_MCA_FLOW1_Y: input = app_regs.REG_REG_OPTICAL_TRACKING_READ[4]; break;
				default:              input = app_regs.REG_REG_OPTICAL_TRACKING_READ[0];
			}
			
			bool signal_is_positive = (input >= 0) ? true : false;
			
			float signal = input * app_regs.REG_MCA_SIGNAL_GAIN * (signal_is_positive ? 1 : -1);
						
			if (signal < app_regs.REG_MCA_ZERO_THRESHOLD)
			{
				motor_pulse_interval = 0;
								
				USARTD0_DATA = motor_pulse_interval & 0x00FF;
				timer_type1_enable(&TCD1, TIMER_PRESCALER_DIV64, 50, INT_LEVEL_LOW);	// 100 us
			}
			else
			{
				motor_pulse_interval = 1000000.0 / signal;
				
				if (motor_pulse_interval > app_regs.REG_MCA_MAX_PULSE_INTERVAL) motor_pulse_interval = app_regs.REG_MCA_MAX_PULSE_INTERVAL;
				if (motor_pulse_interval < app_regs.REG_MCA_MIN_PULSE_INTERVAL) motor_pulse_interval = app_regs.REG_MCA_MIN_PULSE_INTERVAL;
				
				motor_pulse_interval = motor_pulse_interval * (signal_is_positive ? 1 : -1);
				
				USARTD0_DATA = motor_pulse_interval & 0x00FF;
				timer_type1_enable(&TCD1, TIMER_PRESCALER_DIV64, 50, INT_LEVEL_LOW);	// 100 us
			}
		}
		
		core_func_send_event(ADD_REG_REG_OPTICAL_TRACKING_READ, true);
		
		optical_counter = 0;
	}
	
	
	if (cam0_stop_request)
	{
		cam0_stop_request = false;
		stop_cam0_when_possible = true;
	}

	if (cam1_stop_request)
	{
		cam1_stop_request = false;
		stop_cam1_when_possible = true;
	}
}

/************************************************************************/
/* Callbacks: clock control                                             */
/************************************************************************/
void core_callback_clock_to_repeater(void) {}
void core_callback_clock_to_generator(void) {}
void core_callback_clock_to_unlock(void) {}
void core_callback_clock_to_lock(void) {}

/************************************************************************/
/* Callbacks: uart control                                              */
/************************************************************************/
void core_callback_uart_rx_before_exec(void) {}
void core_callback_uart_rx_after_exec(void) {}
void core_callback_uart_tx_before_exec(void) {}
void core_callback_uart_tx_after_exec(void) {}
void core_callback_uart_cts_before_exec(void) {}
void core_callback_uart_cts_after_exec(void) {}

/************************************************************************/
/* Callbacks: Read app register                                         */
/************************************************************************/
bool core_read_app_register(uint8_t add, uint8_t type)
{
	/* Check if it will not access forbidden memory */
	if (add < APP_REGS_ADD_MIN || add > APP_REGS_ADD_MAX)
		return false;
	
	/* Check if type matches */
	if (app_regs_type[add-APP_REGS_ADD_MIN] != type)
		return false;
	
	/* Receive data */
	(*app_func_rd_pointer[add-APP_REGS_ADD_MIN])();	

	/* Return success */
	return true;
}

/************************************************************************/
/* Callbacks: Write app register                                        */
/************************************************************************/
bool core_write_app_register(uint8_t add, uint8_t type, uint8_t * content, uint16_t n_elements)
{
	/* Check if it will not access forbidden memory */
	if (add < APP_REGS_ADD_MIN || add > APP_REGS_ADD_MAX)
		return false;
	
	/* Check if type matches */
	if (app_regs_type[add-APP_REGS_ADD_MIN] != type)
		return false;

	/* Check if the number of elements matches */
	if (app_regs_n_elements[add-APP_REGS_ADD_MIN] != n_elements)
		return false;

	/* Process data and return false if write is not allowed or contains errors */
	return (*app_func_wr_pointer[add-APP_REGS_ADD_MIN])(content);
}