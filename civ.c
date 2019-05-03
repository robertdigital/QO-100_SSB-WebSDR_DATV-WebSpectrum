/*
 * WSPR Toolkit for Linux
 * ======================
 * by DJ0ABR 
 * 
 * civ.c ... handles all ICOM CIV jobs
 * 
 * */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/file.h>
#include "civ.h"
#include "cat.h"

void civ_send_valu32(unsigned char cmd, unsigned int ulval);
void civ_send(unsigned char cmd, unsigned char subcmd, unsigned char sendsubcmd, unsigned char *dataarr, int arrlen);
void FreqToBCD(unsigned int freq, unsigned char *farr);
uint32_t bcdToint32(uint8_t *d, int mode);

unsigned char civRXdata[MAXCIVDATA];
unsigned int civ_freq = 0;
int civ_adr = 0xA2;

int readCIVmessage(int reti)
{
    // printf("ser.rx: %02X\n",reti);
    
    // handle the CIV data
   	// shift data back, get free byte at pos 0
	for(int i=(MAXCIVDATA-1); i>0; i--)
		civRXdata[i] = civRXdata[i-1];

	civRXdata[0] = (unsigned char)reti;
    
	if(civRXdata[0] == 0xfd)
	{
        // an ICOM frame was received, extract the CIV address
        if(civRXdata[3] == 0xe0 && civRXdata[4] == 0xfe && civRXdata[5] == 0xfe)
        {
            // response to PTT off
            // fe fe e0 [ad] fb fd
            //  5  4  3   2   1  0
            //civ_adr = civRXdata[2];
        }
        
		if(civRXdata[8] == 0xe0 && civRXdata[9] == 0xfe && civRXdata[10] == 0xfe)
		{
			// 5 byte Datensatz
			//civ_adr = civRXdata[7];
			civ_freq = bcdToint32(civRXdata+1,5);
		}
		if(civRXdata[8] == 0xe0 && civRXdata[8] == 0xfe && civRXdata[9] == 0xfe)
		{
			// 4 byte Datensatz
            //civ_adr = civRXdata[6];
			civ_freq = bcdToint32(civRXdata+1,4);
		}
		
		if(civ_freq < 0) civ_freq = 0;    // remove invalid values 
		
		//printf("CIV: frequency = %d\n",civ_freq);
	}
	return 1;
}

void civ_ptt(int onoff, unsigned char civad)
{
    printf("set PTT: %s",(onoff==1)?"TX":"RX");
    unsigned char tx[8] = {0xfe, 0xfe, 0x00, 0xe0, 0x1c,00, 00, 0xfd};
    
    tx[2] = civad;
    if(onoff) tx[6] = 1;

    write_port(tx, 8);
}

// mainsub: 0=main, 1=sub
void civ_selMainSub(int mainsub)
{
    printf("set VFO: %s",(mainsub==1)?"SUB":"MAIN");
    unsigned char tx[7] = {0xfe, 0xfe, 0x00, 0xe0,    0x07, 0xD0, 0xfd};

    tx[2] = civ_adr;
    if(mainsub == 1) tx[5] = 0xD1;

    write_port(tx, 7);
}

// query the Icom's Frequency
void civ_queryQRG()
{
    unsigned char tx[6] = {0xfe, 0xfe, 0x00, 0xe0,    0x03, 0xfd};

    // CI-V 1 ist die Default Adresse, da nicht alle TRX die 00 als Broadcast erkennen
    //tx[3] = 0xE1; // Controller Address, Default is 0xe0 but we can also use 0xe1 if we have multiple controllers
    tx[2] = civ_adr;

    write_port(tx, 6);
}

// set TRX to frequency
void civ_setQRG(int freq)
{
    if(freq > 0.1)
    {
        printf("set TRX to QRG: %d",freq);
        civ_send_valu32(5,freq);
    }
}

uint32_t bcdconv(uint8_t v, uint32_t mult)
{
uint32_t tmp,f;

	tmp = (v >> 4) & 0x0f;
	f = tmp * mult * 10;
	tmp = v & 0x0f;
	f += tmp * mult;

	return f;
}

// Wandle ICOM Frequenzangabe um
uint32_t bcdToint32(uint8_t *d, int mode)
{
uint32_t f=0;

	if(mode == 5)
	{
		f += bcdconv(d[0],100000000);
		f += bcdconv(d[1],1000000);
		f += bcdconv(d[2],10000);
		f += bcdconv(d[3],100);
		f += bcdconv(d[4],1);
	}

	if(mode == 4)
	{
		f += bcdconv(d[0],1000000);
		f += bcdconv(d[1],10000);
		f += bcdconv(d[2],100);
		f += bcdconv(d[3],1);
	}
	return f;
}

void FreqToBCD(unsigned int freq, unsigned char *farr)
{
    unsigned int _1000MHz = freq / 1000000000;
    freq -= (_1000MHz * 1000000000);
    
    unsigned int _100MHz = freq / 100000000;
    freq -= (_100MHz * 100000000);

    unsigned int _10MHz = freq / 10000000;
    freq -= (_10MHz * 10000000);

    unsigned int _1MHz = freq / 1000000;
    freq -= (_1MHz * 1000000);

    unsigned int _100kHz = freq / 100000;
    freq -= (_100kHz * 100000);

    unsigned int _10kHz = freq / 10000;
    freq -= (_10kHz * 10000);

    unsigned int _1kHz = freq / 1000;
    freq -= (_1kHz * 1000);

    unsigned int _100Hz = freq / 100;
    freq -= (_100Hz * 100);

    unsigned int _10Hz = freq / 10;
    freq -= (_10Hz * 10);

    unsigned int _1Hz = freq;

    farr[0] = (unsigned char)((unsigned char)(_10Hz << 4) + (unsigned char)_1Hz);
    farr[1] = (unsigned char)((unsigned char)(_1kHz << 4) + (unsigned char)_100Hz);
    farr[2] = (unsigned char)((unsigned char)(_100kHz << 4) + (unsigned char)_10kHz);
    farr[3] = (unsigned char)((unsigned char)(_10MHz << 4) + (unsigned char)_1MHz);
    farr[4] = (unsigned char)((unsigned char)(_1000MHz << 4) + (unsigned char)_100MHz);
}


void civ_send_valu32(unsigned char cmd, unsigned int ulval)
{
    unsigned char fa[5];
    FreqToBCD(ulval,fa);
    civ_send(cmd, 0, 0, fa, 5);
}
        
void civ_send(unsigned char cmd, unsigned char subcmd, unsigned char sendsubcmd, unsigned char *dataarr, int arrlen)
{
    int len = 7;
    if (!sendsubcmd) len--;
    if (dataarr != NULL) len += arrlen;

    int idx = 0;
    unsigned char txdata[100];
    txdata[idx++] = 0xfe;
    txdata[idx++] = 0xfe;
    txdata[idx++] = civ_adr;
    txdata[idx++] = 0xe0;
    txdata[idx++] = cmd;
    if (sendsubcmd) txdata[idx++] = subcmd;
    if (dataarr != NULL)
    {
        for (int i = 0; i < arrlen; i++)
            txdata[idx++] = dataarr[i];
    }
    txdata[idx++] = 0xfd;

    char s[256] = {0};
    for (int i = 0; i < idx; i++)
    {
        sprintf(s+strlen(s),"%02X ",txdata[i]);
    }
    printf("PC->Icom: %s\n",s);

    write_port(txdata, idx);
}