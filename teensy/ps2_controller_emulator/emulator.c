/*
 * PS2 Controller Emulator v2.0 for Teensy 2.0
 *
 * Copyright (C) 2015 Aaron Clovsky <pelvicthrustman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  Special thanks goes to the following projects for making this possible
 *    Teensy PS2 Controller Sim Firmware v2 by Johnny Chung Lee
 *      <http://procrastineering.blogspot.com/2010/12/simulated-ps2-controller-for.html>
 *    Playstation Controller Extender by Micah Elizabeth Scott
 *      <http://scanlime.org/2007/08/playstation-controller-extender/>
 *    Interfacing a PS2 Controller @ curiousinventor.com
 *      <http://store.curiousinventor.com/guides/PS2/>
 *    SNES to PSX: SNES controller to Playstation adapter by Raphaël Assénat
 *      <http://www.raphnet.net/electronique/snes_to_psx/index_en.php>
 *    LilyPad pcsx2 plugin by ChickenLiver
 *      <https://github.com/PCSX2/pcsx2/tree/master/plugins/LilyPad>
 *    PadSSSPSX pcsxr plugin by Nagisa
 *      <https://github.com/tetious/pcsxr/tree/master/win32/plugins/PadSSSPSX>
 */

/*******************************************************************************
 Headers
*******************************************************************************/
#include <avr/io.h>        /* Device-specific I/O */
#include <avr/interrupt.h> /* Enable/Disable interrupts */
#include "usb_serial.h"    /* USB serial support */

/*******************************************************************************
 Firmware Configuration
*******************************************************************************/
/*#define INVERT_SPI_MISO*/              /* Perform bitwise-not on SPI output */
/*#define ACK_SIMULATED_OPEN_COLLECTOR*/ /* Use alternate Ack signaling */
/*#define USE_8MHZ*/                     /* Run CPU at 8MHz instead of 16MHz */

/*******************************************************************************
 Hardware Configuration
*******************************************************************************/
/* CPU */
#define CPU_PRESCALE(n)  (CLKPR = 0x80, CLKPR = (n))
#define CPU_16MHz        0x00
#define CPU_8MHz         0x01

/* LED */
unsigned char global_led_state; /* Reflects current LED state */

#define LED_CONFIG()     ((DDRD  |=  (1<<6)), global_led_state = 0)
#define LED_OFF()        ((PORTD &= ~(1<<6)), global_led_state = 0)
#define LED_ON()         ((PORTD |=  (1<<6)), global_led_state = 1)
#define LED_TOGGLE()     (global_led_state ? (LED_OFF()) : (LED_ON()))

/* Timer - set timer1 prescaler to 1024 and start the timer */
#define TIMER_CONFIG()   (TCCR1B |= (1 << CS12) | (1 << CS10)) 
#define TIMER_READ()     ((unsigned int)(TCNT1))

/* Attn */
#define ATT_PIN          ((PINB & 1) == 1)

/* Clk */
#define CLK_PIN          ((PINB & 2)  > 0)

/* Ack */
#define ACK_INPUT()      (DDRB  &= ~(1<<7))
#define ACK_OUTPUT()     (DDRB  |=  (1<<7))
#define ACK_LOW()        (PORTB &= ~(1<<7))
#define ACK_HIGH()       (PORTB |=  (1<<7))

#ifdef ACK_SIMULATED_OPEN_COLLECTOR
    #define ACK_CONFIG() ACK_INPUT(); ACK_LOW()
#else
    #define ACK_CONFIG() ACK_OUTPUT(); ACK_HIGH()
#endif

/* SPI */
#define SPI_CONFIG()     (DDRB  |=  (1<<3))

/*******************************************************************************
 Timing
*******************************************************************************/
/* Timer constants
    - Calculated based on 1024 prescale set by TIMER_CONFIG()
*/
#ifdef USE_8MHZ
    #define TIMER_ONE_SECOND    7813
    #define TIMER_TWO_SECONDS   15625
    #define TIMER_THREE_SECONDS 23438
#else
    #define TIMER_ONE_SECOND    15625
    #define TIMER_TWO_SECONDS   31250
    #define TIMER_THREE_SECONDS 46875
#endif

/* Delay macro functions
    - Spam NOPs for high quality timing
    - Each NOP is ~62.5 nanoseconds @ 16MHz */
#define DELAY_8_CYCLES() \
{                        \
    __asm__("nop\n\t");  \
    __asm__("nop\n\t");  \
    __asm__("nop\n\t");  \
    __asm__("nop\n\t");  \
    __asm__("nop\n\t");  \
    __asm__("nop\n\t");  \
    __asm__("nop\n\t");  \
    __asm__("nop\n\t");  \
}

#ifdef USE_8MHZ
    #define DELAY_1_US()  \
    {                     \
        DELAY_8_CYCLES(); \
    }
#else
    #define DELAY_1_US()  \
    {                     \
        DELAY_8_CYCLES(); \
        DELAY_8_CYCLES(); \
    }
#endif

#define DELAY_2_US() \
{                    \
    DELAY_1_US();    \
    DELAY_1_US();    \
}

/* Below are not high accuracy */
#define DELAY_1_MS()             \
{                                \
    int us;                      \
    for (us = 0; us < 100; us++) \
    {                            \
        DELAY_2_US();            \
        DELAY_2_US();            \
        DELAY_2_US();            \
        DELAY_2_US();            \
        DELAY_2_US();            \
    }                            \
}

#define DELAY_N_MS(n)            \
{                                \
    int ms;                      \
    for (ms = 0; ms < (n); ms++) \
    {                            \
        DELAY_1_MS();            \
    }                            \
}

/*******************************************************************************
 SPI
*******************************************************************************/
/* SPI communication macro functions
    - Functions are send and receive (simultaneous TX/RX)
    - Waits on (SPSR & 0x80) as completion condition
    - If at any point ATT_PIN goes high all transmissions reset
    - ACK signal requires minimum 2us pulse */
#ifdef ACK_SIMULATED_OPEN_COLLECTOR
    #define SPI_ACK()                                     \
    {                                                     \
        ACK_OUTPUT();                                     \
        DELAY_2_US();                                     \
        ACK_INPUT();                                      \
    }
#else
    #define SPI_ACK()                                     \
    {                                                     \
        ACK_LOW();                                        \
        DELAY_2_US();                                     \
        ACK_HIGH();                                       \
    }
#endif

#ifdef INVERT_SPI_MISO
    #define SPI_OUT_OPERATOR ~
#else
    #define SPI_OUT_OPERATOR
#endif

#define SPI_TXRX(out)                                     \
{                                                         \
    SPDR = SPI_OUT_OPERATOR(out);                         \
    while (!(SPSR & 0x80)) if (ATT_PIN) goto device_loop; \
}

#define SPI_TXRX_IN(out, in)                              \
{                                                         \
    SPI_TXRX(out)                                         \
    in = SPDR;                                            \
}

#define SPI_TXRX_PRE_ACK(out)                             \
{                                                         \
    SPI_ACK();                                            \
    SPI_TXRX(out)                                         \
}

#define SPI_TXRX_IN_PRE_ACK(out, in)                      \
{                                                         \
    SPI_ACK();                                            \
    SPI_TXRX_IN(out, in)                                  \
}

/*******************************************************************************
 Controller
*******************************************************************************/
enum button_bitmasks /* Button bits are zero when pressed */
{
    /* buttons[0] */
    BUTTON_SELECT     = 0x01,
    BUTTON_L3         = 0x02,
    BUTTON_R3         = 0x04,
    BUTTON_START      = 0x08,
    BUTTON_UP         = 0x10,
    BUTTON_RIGHT      = 0x20,
    BUTTON_DOWN       = 0x40,
    BUTTON_LEFT       = 0x80,
    
    /* buttons[1] */
    BUTTON_L2         = 0x01,
    BUTTON_R2         = 0x02,
    BUTTON_L1         = 0x04,
    BUTTON_R1         = 0x08,
    BUTTON_TRIANGLE   = 0x10,
    BUTTON_CIRCLE     = 0x20,
    BUTTON_CROSS      = 0x40,
    BUTTON_SQUARE     = 0x80,
};

enum commands
{
    cmd_init_pressure              = 0x00,
    cmd_get_available_poll_results = 0x01,
    cmd_poll                       = 0x02,
    cmd_escape                     = 0x03,
    cmd_set_major_mode             = 0x04,
    cmd_read_ext_status            = 0x05,
    cmd_read_const_1               = 0x06,
    cmd_read_const_2               = 0x07,
    cmd_unsupported_8h             = 0x08,
    cmd_unsupported_9h             = 0x09,
    cmd_unsupported_ah             = 0x0A,
    cmd_unsupported_bh             = 0x0B,
    cmd_read_const_3               = 0x0C,
    cmd_set_poll_cmd_format        = 0x0D,
    cmd_unsupported_eh             = 0x0E,
    cmd_set_poll_result_format     = 0x0F,
};

struct
{
    /* Digital button states */
    unsigned char buttons[2];
    
    /* Analog stick axes */
    unsigned char joystick_rx;
    unsigned char joystick_ry;
    unsigned char joystick_lx;
    unsigned char joystick_ly;
    
    /* Analog button pressures */
    unsigned char pressure_right;
    unsigned char pressure_left;
    unsigned char pressure_up;
    unsigned char pressure_down;
    unsigned char pressure_triangle;
    unsigned char pressure_circle;
    unsigned char pressure_cross;
    unsigned char pressure_square;
    unsigned char pressure_l1;
    unsigned char pressure_r1;
    unsigned char pressure_l2;
    unsigned char pressure_r2;
    
    /* Motor states */
    unsigned char small_motor;
    unsigned char large_motor;
    
    /* Internal controller state */
    unsigned char active;           /* Non-zero when console activity has been observed since last reset */
    unsigned char control_mode;     /* Current mode of the controller */
    unsigned char config_mode;      /* Current mode of the controller when control_mode is 0xF3: config mode */
    unsigned char mode_lock;        /* Non-zero if the mode button has no effect when pressed (locked) */
    unsigned char mode_request;     /* When the mode button is pressed and the controller is in config mode the
                                       mode change is deferred until exiting config mode by setting this
                                       to the current mode number (it will otherwise be zero) */
    unsigned char motor_map[6];     /* Current mapping of the controller's motors */
    unsigned char response_mask[2]; /* 16 bits set by command 0x4F and read by 0x41, may exist to mask controller data
                                       but no examples of this appear to exist so emulation is limited to read/write */
    unsigned char connected;        /* Non-zero when controller is connected - not effected by reset_controller() */
} controller;

void reset_controller()
{
    controller.buttons[0] = 0xFF;
    controller.buttons[1] = 0xFF;
    
    controller.joystick_rx = 0x80;
    controller.joystick_ry = 0x80;
    controller.joystick_lx = 0x80;
    controller.joystick_ly = 0x80;
    
    controller.pressure_right    = 0x00;
    controller.pressure_left     = 0x00;
    controller.pressure_up       = 0x00;
    controller.pressure_down     = 0x00;
    controller.pressure_triangle = 0x00;
    controller.pressure_circle   = 0x00;
    controller.pressure_cross    = 0x00;
    controller.pressure_square   = 0x00;
    controller.pressure_l1       = 0x00;
    controller.pressure_r1       = 0x00;
    controller.pressure_l2       = 0x00;
    controller.pressure_r2       = 0x00;
    
    controller.small_motor  = 0x00;
    controller.large_motor  = 0x00;
    
    controller.config_mode  = 0x41;
    controller.control_mode = 0x41;
    
    controller.mode_lock    = 0;
    controller.mode_request = 0;
    
    controller.motor_map[0] = 0xFF;
    controller.motor_map[1] = 0xFF;
    controller.motor_map[2] = 0xFF;
    controller.motor_map[3] = 0xFF;
    controller.motor_map[4] = 0xFF;
    controller.motor_map[5] = 0xFF;
    
    controller.response_mask[0] = 0xFF;
    controller.response_mask[1] = 0xFF;
    
    controller.active = 0;
}

/*******************************************************************************
 Setup
*******************************************************************************/
void setup()
{
    /* Confgure CPU Clock */
    #ifdef USE_8MHZ
        CPU_PRESCALE(CPU_8MHz);
    #else
        CPU_PRESCALE(CPU_16MHz);
    #endif
    
    /* Confgure LED */
    LED_CONFIG();
    LED_ON();
    
    /* Configure 16-bit timer */
    TIMER_CONFIG();
    
    /* Confgure USB */
    usb_init();
    DELAY_N_MS(1000);
    usb_serial_flush_input();
    
    /* Confgure SPI */
    SPI_CONFIG();
    
	/* Enable SPI */
	SPCR = 0; 
	SPCR |= (1 << SPE);
	SPCR |= (1 << DORD);
	SPCR |= (1 << CPOL);
	SPCR |= (1 << CPHA);
	
	/* Clear SPI Registers */
	{
		char clr;
		clr = SPSR;
		clr = SPDR;
	}
    
    /* Confgure Ack */
    ACK_CONFIG();

    /* Initialize controller connection state */
    controller.connected = 0;
	
    /* Initialize controller data */
    reset_controller();
    
    /* Disable interrupts */
    cli();
    
    /* Setup complete - toggle LED */
    LED_TOGGLE();
}

/*******************************************************************************
 main()
*******************************************************************************/
void main()
{
    unsigned int  idle_timer;        /* Tracks time since attention line activity (idle time) */
    unsigned int  disconnect_timer;  /* Tracks time since last USB packet */
    unsigned char rx_buffer[18];     /* Buffers bytes of a USB packet */
    unsigned int  sequence = 0;      /* Tracks bytes of a USB packet received so far */
    unsigned int  ignore_packet = 0; /* Ignores SPI transfers until end of controller packet */
	unsigned char cmd;               /* Controller packet command byte */
    
    setup();
    
    idle_timer = TIMER_READ();
    
    device_loop:
    {
        /* If the attention line is inactive (high) then perform idle tasks */
        if (ATT_PIN)
        {
            /* Clear ignore packet flag between packets */
            ignore_packet = 0;
            
            /* Check idle timer
                - After approximately one second of inactivity
                  reset the controller's internal state */
            if ((TIMER_READ() - idle_timer) >= TIMER_ONE_SECOND)
            {
                if (controller.active)
                {
                    reset_controller();
                }
                
                LED_TOGGLE();
                
                idle_timer = TIMER_READ();
            }
            
            /* Check disconnect timer
                - After approximately one second without a state
                  update the controller is considered disconnected */
            if (controller.connected && ((TIMER_READ() - disconnect_timer) >= TIMER_ONE_SECOND))
            {
                reset_controller();
				
                controller.connected = 0;
            }
            
            /* Handle USB controller updates */
            {
                sei(); /* Enable interrupts */
                DELAY_1_US();
                
                if (usb_serial_available())
                {
                    int result = usb_serial_getchar();
                    
                    if (result == -1)
                    {
                        sequence = 0;
                    }
                    else
                    {
                        unsigned char byte = (unsigned char)result;
                        
                        switch (sequence)
                        {
                            case 0:
                            {
                                /* Valid packets begin with 0x5A */
                                if (byte != 0x5A)
                                {
                                    break;
                                }
                                
                                sequence++;
                                
                                break;
                            }
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                            case 6:
                            case 7:
                            case 8:
                            case 9:
                            case 10:
                            case 11:
                            case 12:
                            case 13:
                            case 14:
                            case 15:
                            case 16:
                            case 17:
                            case 18:
                            {
                                rx_buffer[sequence++ - 1] = byte;
                                
                                break;
                            }
                            case 19:
                            {
                                if (byte == 0x55 || byte == 0xAA) /* Valid packets end with 0x55, 0xAA or 0x5A */
                                {
                                    controller.buttons[0]  = rx_buffer[0];
                                    controller.buttons[1]  = rx_buffer[1];
                                    
                                    controller.joystick_rx = rx_buffer[2];
                                    controller.joystick_ry = rx_buffer[3];
                                    controller.joystick_lx = rx_buffer[4];
                                    controller.joystick_ly = rx_buffer[5];
                                    
                                    controller.pressure_right    = rx_buffer[6];
                                    controller.pressure_left     = rx_buffer[7];
                                    controller.pressure_up       = rx_buffer[8];
                                    controller.pressure_down     = rx_buffer[9];
                                    controller.pressure_triangle = rx_buffer[10];
                                    controller.pressure_circle   = rx_buffer[11];
                                    controller.pressure_cross    = rx_buffer[12];
                                    controller.pressure_square   = rx_buffer[13];
                                    controller.pressure_l1       = rx_buffer[14];
                                    controller.pressure_r1       = rx_buffer[15];
                                    controller.pressure_l2       = rx_buffer[16];
                                    controller.pressure_r2       = rx_buffer[17];
                                    
                                    /* If the mode button was pressed and the mode is not locked */
                                    if (byte == 0xAA && !controller.mode_lock)
                                    {
                                        /* Defer mode button action if currently in config mode */
                                        if (controller.control_mode == 0xF3)
                                        {
                                            controller.mode_request = controller.config_mode;
                                        }
                                        /* Otherwise toggle the mode */
                                        else
                                        {
                                            controller.control_mode = (controller.control_mode == 0x41) ? 0x73 : 0x41;
                                        }
                                    }
                                    
                                    /* Compose response packet */
                                    usb_serial_putchar_nowait(0x5A); /* Header */
                                    
                                    usb_serial_putchar_nowait(controller.small_motor); /* Motors */
                                    usb_serial_putchar_nowait(controller.large_motor);
                                    
                                    /* Footer - 0x55 for mode LED off - 0xAA for mode LED on */
                                    if (controller.control_mode == 0x41
                                    || (controller.control_mode == 0xF3 && controller.config_mode == 0x41))
                                    {
                                        usb_serial_putchar_nowait(0x55);
                                    }
                                    else
                                    {
                                        usb_serial_putchar_nowait(0xAA);
                                    }
                                    
                                    controller.connected = 1;
                                    disconnect_timer = TIMER_READ();
                                }
                                else if (byte == 0x5A) /* Packets ending with 0x5A disconnect the controller */
                                {
                                    /* Compose response packet */
                                    usb_serial_putchar_nowait(0x5A); /* Header */
                                    usb_serial_putchar_nowait(0x00); /* Small motor */
                                    usb_serial_putchar_nowait(0x00); /* Large motor */
                                    usb_serial_putchar_nowait(0x55); /* Footer */
                                    
									reset_controller();
									
                                    controller.connected = 0;
                                }
                                
                                sequence = 0;
                                
                                break;
                            }
                        }
                    }
                }
                
                DELAY_1_US();
                cli(); /* Disable interrupts */
            }
            
            goto device_loop;
        }
        
        /* LED reflects attention line activity */
        LED_ON();
        
        /* First byte is port number
            - Ignore packets not for port #1
            - Ignore packet if not connected  */
        SPI_TXRX_IN(0xFF, cmd);
        
        if (cmd != 0x01 || ignore_packet || !controller.connected) 
        {
            ignore_packet = 1;
            goto device_loop;
        }
        
        controller.active = 1; /* Controller is active */        
        
        idle_timer = TIMER_READ(); /* Reset idle timer */
        
        /* Second byte is command
            - Valid commands begin with 0x4 as the high nibble
            - Only 0x42 (poll) and 0x43 (enter or exit config mode) are valid in normal mode */
        SPI_TXRX_IN_PRE_ACK(controller.control_mode, cmd);
        
        if ((cmd & 0xF0) != 0x40)
        {
            goto device_loop;
        }
        
        cmd &= 0x0F; /* Only the low nibble of the command is used */
        
        if (controller.control_mode != 0xF3 && cmd != cmd_poll && cmd != cmd_escape)
        {
            goto device_loop;
        }
        
        /* Third byte is padding */
        SPI_TXRX_PRE_ACK(0x5A);
        
        switch (cmd)
        {    
            /* 0x40 */
            case cmd_init_pressure:
            {
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x02);
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x5A);
                
                goto device_loop;
            }
            
            /* 0x41 */
            case cmd_get_available_poll_results:
            {
                const unsigned char response[2][4] =
                {
                    { 0x00, 0x00, 0x00, 0x00 },
                    { 0x03, 0x00, 0x00, 0x5A }
                };
                
                unsigned char mode = (controller.config_mode & 0x10) >> 4;
                unsigned char mask = ~mode + 1;
                
                SPI_TXRX_PRE_ACK(controller.response_mask[0] & mask);
                SPI_TXRX_PRE_ACK(controller.response_mask[1] & mask);
                SPI_TXRX_PRE_ACK(response[mode][0]);
                SPI_TXRX_PRE_ACK(response[mode][1]);
                SPI_TXRX_PRE_ACK(response[mode][2]);
                SPI_TXRX_PRE_ACK(response[mode][3]);
                
                goto device_loop;
            }
            
            /* 0x43 */
            case cmd_escape:
            {
                if (controller.control_mode == 0xF3) /* If in config mode */
                {
                    unsigned char param;
                    
                    SPI_TXRX_IN_PRE_ACK(0x00, param);
                    SPI_TXRX_PRE_ACK(0x00);
                    SPI_TXRX_PRE_ACK(0x00);
                    SPI_TXRX_PRE_ACK(0x00);
                    SPI_TXRX_PRE_ACK(0x00);
                    SPI_TXRX_PRE_ACK(0x00);
                    
                    if ((param & 0x01) == 0) /* If the parameter is zero then exit config mode */
                    {
                        controller.control_mode = controller.config_mode;
                        
                        /* If the mode button was pressed and its action deferred
                           and the mode is not locked
                           and the mode has not changed since the mode button was pressed
                           then toggle the mode
                           
                           Note: The mode button's deferred action logic might not be neccessary 
                                 but it might resolve potential behavioral inconsistencies
                        */
                        if (controller.mode_request
                        && !controller.mode_lock
                        &&  controller.mode_request == controller.control_mode)
                        {
                            controller.control_mode = (controller.control_mode == 0x41) ? 0x73 : 0x41;
                            
                            controller.mode_request = 0; /* Request has been handled */
                        }
                    }
                    
                    goto device_loop;
                }
                
                /* If not in config mode then 0x43 is substantially 0x42 */
            }
            
            /* 0x42 */
            case cmd_poll:
            {
                unsigned char param[6];
                
                switch (controller.control_mode)
                {
                    case 0x41: /* digital mode */
                    {
                        SPI_TXRX_IN_PRE_ACK(controller.buttons[0] | BUTTON_L3 | BUTTON_R3, param[0]);
                        SPI_TXRX_IN_PRE_ACK(controller.buttons[1], param[1]);
                        param[2] = 0x00;
                        param[3] = 0x00;
                        param[4] = 0x00;
                        param[5] = 0x00;
                        
                        break;
                    }
                    case 0xF3: /* config mode */
                    case 0x73: /* digital buttons, analog joysticks */
                    {
                        SPI_TXRX_IN_PRE_ACK(controller.buttons[0], param[0]);
                        SPI_TXRX_IN_PRE_ACK(controller.buttons[1], param[1]);
                        SPI_TXRX_IN_PRE_ACK(controller.joystick_rx, param[2]);
                        SPI_TXRX_IN_PRE_ACK(controller.joystick_ry, param[3]);
                        SPI_TXRX_IN_PRE_ACK(controller.joystick_lx, param[4]);
                        SPI_TXRX_IN_PRE_ACK(controller.joystick_ly, param[5]);
                        
                        break;
                    }
                    case 0x79: /* analog joysticks, analog buttons */
                    {
                        SPI_TXRX_IN_PRE_ACK(controller.buttons[0], param[0]);
                        SPI_TXRX_IN_PRE_ACK(controller.buttons[1], param[1]);
                        SPI_TXRX_IN_PRE_ACK(controller.joystick_rx, param[2]);
                        SPI_TXRX_IN_PRE_ACK(controller.joystick_ry, param[3]);
                        SPI_TXRX_IN_PRE_ACK(controller.joystick_lx, param[4]);
                        SPI_TXRX_IN_PRE_ACK(controller.joystick_ly, param[5]);
                        SPI_TXRX_PRE_ACK(controller.pressure_right);
                        SPI_TXRX_PRE_ACK(controller.pressure_left);
                        SPI_TXRX_PRE_ACK(controller.pressure_up);
                        SPI_TXRX_PRE_ACK(controller.pressure_down);
                        SPI_TXRX_PRE_ACK(controller.pressure_triangle);
                        SPI_TXRX_PRE_ACK(controller.pressure_circle);
                        SPI_TXRX_PRE_ACK(controller.pressure_cross);
                        SPI_TXRX_PRE_ACK(controller.pressure_square);
                        SPI_TXRX_PRE_ACK(controller.pressure_l1);
                        SPI_TXRX_PRE_ACK(controller.pressure_r1);
                        SPI_TXRX_PRE_ACK(controller.pressure_l2);
                        SPI_TXRX_PRE_ACK(controller.pressure_r2);
                        
                        break;
                    }
                }
                
                if (cmd == cmd_poll) /* Update motor states */
                {
                    int i;
                    
                    for (i = 0; i < 6; i++)
                    {
                        if (controller.motor_map[i] == 0x00)
                        {
                            controller.small_motor = param[i];
                        }
                        else if (controller.motor_map[i] == 0x01)
                        {
                            controller.large_motor = param[i];
                        }
                    }
                }
                else if ((param[0] & 0x01) == 0x01) /* Enter configuration mode */
                {
                    controller.config_mode = controller.control_mode;
                    controller.control_mode = 0xF3;
                }
                
                goto device_loop;
            }
            
            /* 0x44 */
            case cmd_set_major_mode:
            {
                unsigned char param[2];
                
                SPI_TXRX_IN_PRE_ACK(0x00, param[0]);
                SPI_TXRX_IN_PRE_ACK(0x00, param[1]);
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x00);
                
                controller.mode_lock = (param[1] == 0x03); /* Only 0x03 locks the mode */
                
                controller.config_mode = ((param[0] & 0x01) == 0x00) ? 0x41 : 0x73;
                
                goto device_loop;
            }
            
            /* 0x45 */
            case cmd_read_ext_status:
            {
                unsigned char led = (controller.config_mode & 0x10) >> 4;
                
                SPI_TXRX_PRE_ACK(0x03); /* This is 0x01 for DS1 and GH guitar */
                SPI_TXRX_PRE_ACK(0x02);
                SPI_TXRX_PRE_ACK(led);
                SPI_TXRX_PRE_ACK(0x02);
                SPI_TXRX_PRE_ACK(0x01);
                SPI_TXRX_PRE_ACK(0x00);
                
                goto device_loop;
            }
            
            /* 0x46 */
            /* 0x47 */
            /* 0x4C */
            case cmd_read_const_1:
            case cmd_read_const_2:
            case cmd_read_const_3:
            {
                const unsigned char offset[16] =
                {
                    0, 0, 0, 0, 0, 0, 0, 1,
                    0, 0, 0, 0, 2, 0, 0, 0
                };
                
                const unsigned char data[3][2][4] =
                {
                    {
                        { 0x01, 0x02, 0x00, 0x0A },
                        { 0x01, 0x01, 0x01, 0x14 }
                    },
                    {
                        { 0x02, 0x00, 0x01, 0x00 },
                        { 0x02, 0x00, 0x01, 0x00 }
                    },
                    {
                        { 0x00, 0x04, 0x00, 0x00 },
                        { 0x00, 0x07, 0x00, 0x00 }
                    }
                };
                
                unsigned char param;
                
                SPI_TXRX_IN_PRE_ACK(0x00, param);
                param &= 0x01;
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(data[offset[cmd]][param][0]);
                SPI_TXRX_PRE_ACK(data[offset[cmd]][param][1]);
                SPI_TXRX_PRE_ACK(data[offset[cmd]][param][2]);
                SPI_TXRX_PRE_ACK(data[offset[cmd]][param][3]);
                
                goto device_loop;
            }
            
            /* 0x4D */
            case cmd_set_poll_cmd_format:
            {
                SPI_TXRX_IN_PRE_ACK(controller.motor_map[0], controller.motor_map[0]);
                SPI_TXRX_IN_PRE_ACK(controller.motor_map[1], controller.motor_map[1]);
                SPI_TXRX_IN_PRE_ACK(controller.motor_map[2], controller.motor_map[2]);
                SPI_TXRX_IN_PRE_ACK(controller.motor_map[3], controller.motor_map[3]);
                SPI_TXRX_IN_PRE_ACK(controller.motor_map[4], controller.motor_map[4]);
                SPI_TXRX_IN_PRE_ACK(controller.motor_map[5], controller.motor_map[5]);
                
                goto device_loop;
            }
            
            /* 0x4F */
            case cmd_set_poll_result_format:
            {
                SPI_TXRX_IN_PRE_ACK(0x00, controller.response_mask[0]);
                SPI_TXRX_IN_PRE_ACK(0x00, controller.response_mask[1]);
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x00);
                SPI_TXRX_PRE_ACK(0x5A);
                
                if (controller.config_mode == 0x73)
                {
                    controller.config_mode = 0x79;
                }
                
                goto device_loop;
            }
            
            default:
            {
                goto device_loop;
            }
        }
        
        goto device_loop; /* This should be unreachable */
    }
}
