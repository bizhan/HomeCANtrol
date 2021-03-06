/***************************************************************************
 *            led_0.c
 *
 *  Tue Mar 11 21:11:55 2008
 *  Copyright  2008  sharan
 *  <sharan@bastard>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#include <avr/io.h>

#include "config.h"

#if defined(LED)

#include "led_core.h"
#include "led_1.h"

void LED_1_init( void )
{
#if defined(__AVR_ATmega2561__) || defined(__AVR_ATmega644P__)
	LED_1_DDR |= ( 1<<LED_1_PIN );
	LED_Register ( LED_1_ON, LED_1_OFF, LED_1_TOGGLE );
	LED_1_OFF ();
#endif
}

void LED_1_ON ( void )
{
#ifdef __AVR_ATmega2561__
	LED_1_PORT &= ~( 1<<LED_1_PIN );
#endif
#ifdef __AVR_ATmega644P__
	LED_1_PORT |= ( 1<<LED_1_PIN );
#endif
}

void LED_1_OFF ( void )
{
#ifdef __AVR_ATmega2561__
	LED_1_PORT |= ( 1<<LED_1_PIN );
#endif
#ifdef __AVR_ATmega644P__
	LED_1_PORT &= ~( 1<<LED_1_PIN );
#endif
}

void LED_1_TOGGLE ( void )
{
#if defined(__AVR_ATmega2561__) || defined(__AVR_ATmega644P__)
	if ( ( LED_1_PORT & ( 1<<LED_1_PIN )) != 0 ) 
	{
		LED_1_PORT &= ~( 1<<LED_1_PIN );
	}
	else 
	{
		LED_1_PORT |= ( 1<<LED_1_PIN );
	}
#endif
}
#endif
