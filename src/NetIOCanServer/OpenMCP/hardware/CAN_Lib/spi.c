// ----------------------------------------------------------------------------
/*
 * Copyright (c) 2008 Florian Kristen, Fabian Greif
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: spi.c 6802 2008-11-12 10:25:05Z fabian $
 */
// ----------------------------------------------------------------------------

#include "mcp2515_private.h"

#include "spi.h"

#ifdef	SPI_PRESCALER
	#if (SPI_PRESCALER == 2) || (SPI_PRESCALER == 8) || (SPI_PRESCALER == 32) || (SPI_PRESCALER == 64)
		#define	R_SPSR	(1<<SPI2X)
		#define SPI_PRESCALER_ 	(SPI_PRESCALER * 2)
	#else
		#define	R_SPSR	0
		#define	SPI_PRESCALER_	SPI_PRESCALER
	#endif
	
	#define	SPI_CLOCK_RATE_BIT0		(1<<SPR0)
	#define	SPI_CLOCK_RATE_BIT1		(1<<SPR1)
	
	#if (SPI_PRESCALER_ == 4)
		#define	R_SPCR	0
	#elif (SPI_PRESCALER_ == 16)
		#define	R_SPCR	SPI_CLOCK_RATE_BIT0
	#elif (SPI_PRESCALER_ == 64)
		#define	R_SPCR	SPI_CLOCK_RATE_BIT1
	#elif (SPI_PRESCALER_ == 128)
		#define	R_SPCR	SPI_CLOCK_RATE_BIT1 | SPI_CLOCK_RATE_BIT0
	#else
		#error	 SPI_PRESCALER must be one of the values of 2^n with n = 1..7!
	#endif
#else
	#error	SPI_PRESCALER not defined!
#endif

	extern void usart_write_P (const char *Buffer,...);
	#define usart_write(format, args...)   usart_write_P(PSTR(format) , ## args)

// ----------------------------------------------------------------------------
void mcp2515_spi_init(void)
{
}

// ----------------------------------------------------------------------------
// Schreibt/liest ein Byte ueber den Hardware SPI Bus

uint8_t spi_putc(uint8_t IF,uint8_t data)
{

	uint8_t data_in = 0;
	uint8_t data_b ;
	
	data_b = data ;
	
	if (IF==0) {
		RESET(P_SCK_1);
		for (uint8_t i=0;i<8;i++)
		{
			data_in <<= 1;
		
			if (data & 0x80)
				SET(P_MOSI_1);
			else
				RESET(P_MOSI_1);
				
//			_delay_us(1);
			SET(P_SCK_1);
//			_delay_us(1);

			if (IS_SET(P_MISO_1))
			data_in |= 1;
				
			data <<= 1;
			
//			_delay_us(1);
			RESET(P_SCK_1);
//			_delay_us(1);
		}
	
	} else {
		RESET(P_SCK_2);
		for (uint8_t i=0;i<8;i++)
		{
			data_in <<= 1;
		
			if (data & 0x80)
				SET(P_MOSI_2);
			else
				RESET(P_MOSI_2);
			
//			_delay_us(1);
			SET(P_SCK_2);
//			_delay_us(1);
			
			if (IS_SET(P_MISO_2))
				data_in |= 1;
		
			data <<= 1;
			
//			_delay_us(1);
			RESET(P_SCK_2);
//			_delay_us(1);
		}
	
	} ;
//	usart_write("%i: 0x%x -> 0x%x\r\n",IF,data_b,data_in) ;
	
//	_delay_ms(50) ;

	return data_in;

}
