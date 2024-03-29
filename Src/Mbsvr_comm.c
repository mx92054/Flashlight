/***************************************************
 * Copyright (C),2019 www.idsse.ac.cn
 * Written by chenming
 * Version 1.0
 * Data  2019-3-3
 * Description: modbus服务程序公用库实现文件
 * *************************************************/
#include "Mbsvr_comm.h"
#include "SysTick.h"
#include <string.h>
#include <stdlib.h>

uint8_t RxChar ;
/*********************************************************
 * @desc:  modbus data initial
 * @param: pblk-指向数据块的指针
 * @retval:None
 * ******************************************************/
void ModbusSvr_block_init(Modbus_block *pblk, UART_HandleTypeDef* pUSARTx)
{
    pblk->station = 1;
    pblk->baudrate = 38400;

    pblk->uCoilStartAdr = 0;
    pblk->uCoilLen = 10;
    pblk->uCoilEndAdr = pblk->uCoilStartAdr + pblk->uCoilLen ;
    pblk->ptrCoils = (short *)malloc(pblk->uCoilLen * sizeof(short));
    if (pblk->ptrCoils == NULL)
        pblk->uCoilLen = 9999;
		else
			memset(pblk->ptrCoils,0,pblk->uCoilLen * sizeof(short)) ;

    pblk->uRegStartAdr = 0;
    pblk->uRegLen = 100 ;
    pblk->uRegEndAdr = pblk->uRegStartAdr + pblk->uRegLen ;
    pblk->ptrRegs = (short *)malloc(pblk->uRegLen * sizeof(short));
    if (pblk->ptrRegs == NULL)
        pblk->uRegLen = 9999;
		else
			memset(pblk->ptrRegs,0, pblk->uRegLen * sizeof(short)) ;

    pblk->uRomStartAdr = 0;
    pblk->uRomLen = 10;
    pblk->uRomEndAdr = pblk->uRomStartAdr + pblk->uRomLen;
    pblk->ptrRoms = (short *)malloc(pblk->uRomLen * sizeof(short));
    if (pblk->ptrRoms == NULL)
        pblk->uRomLen = 9999;
		else
			memset(pblk->ptrRoms, 0, pblk->uRomLen * sizeof(short)) ;

    switch (pblk->baudrate)
    {
    case 9600:
        pblk->uFrameInterval = 15;
        break;
    case 19200:
        pblk->uFrameInterval = 10;
        break;
    case 38400:
        pblk->uFrameInterval = 5;
        break;
    case 57600:
        pblk->uFrameInterval = 5;
        break;
    case 115200:
        pblk->uFrameInterval = 5;
        break;
    default:
        pblk->uFrameInterval = 10;
        break;
    }

    pblk->pos_msg = 0;
    pblk->bFrameStart = 0;
    pblk->nMBInterval = 0;
    pblk->bSaved = 1;

    pblk->tsk_buf = pblk->buffer;
    pblk->isr_buf = pblk->buffer + 256;

    pblk->pUsart = pUSARTx ;
    HAL_UART_Receive_IT(pblk->pUsart, &RxChar, 1);
}

/*********************************************************
 * @desc:  正常应答
 * @param: pblk-指向数据块的指针
 *         pUSARTx-通信端口
 * @retval:None
 * ******************************************************/
static void ModbusSvr_normal_respose(Modbus_block *pblk)
{
    u16 uCrc1;

    //-----------Product CRC byte------------------------------------
    uCrc1 = CRC16(pblk->tsk_buf, pblk->trans_len - 2);
    pblk->tsk_buf[pblk->trans_len - 2] = uCrc1 & 0x00FF;
    pblk->tsk_buf[pblk->trans_len - 1] = uCrc1 >> 8;

    //-------------Transmitter frame---------------------------------
    Usart_SendBytes(pblk->pUsart, pblk->tsk_buf, pblk->trans_len);
}

/*********************************************************
 * @desc:  异常应答
 * @param: pblk-指向数据块的指针
 *         pUSARTx-通信端口
 * @retval:None
 * ******************************************************/
static void ModbusSvr_error_respose(Modbus_block *pblk)
{
    u16 uCrc1;

    pblk->tsk_buf[1] |= 0x80;
    pblk->tsk_buf[2] = pblk->errno;

    //-----------Product CRC byte------------------------------------
    uCrc1 = CRC16(pblk->tsk_buf, 3);
    pblk->tsk_buf[3] = uCrc1 & 0x00FF;
    pblk->tsk_buf[4] = uCrc1 >> 8;

    //-------------Transmitter frame---------------------------------
    Usart_SendBytes(pblk->pUsart, pblk->tsk_buf, 5);
}

/*********************************************************
 * @desc:  总线服务程序
 * @param: pblk-指向数据块的指针
 *         pUSARTx-通信端口
 * @retval:None
 * ******************************************************/
void ModbusSvr_task(Modbus_block *pblk)
{
    u8 *ptr;
    u32 tick;

    if (pblk->nMBInterval > pblk->uFrameInterval) //通信帧间隔时间已到，缓冲区域切换
    {
        ptr = pblk->tsk_buf;
        pblk->tsk_buf = pblk->isr_buf;
        pblk->isr_buf = ptr;
        pblk->frame_len = pblk->pos_msg;
        pblk->pos_msg = 0;
        pblk->bFrameStart = 1;

        if (pblk->frame_len >= 8 && pblk->tsk_buf[0] == pblk->station)
        {
					//HAL_GPIO_TogglePin(GPIOF,GPIO_PIN_0) ;
					
           tick = GetCurTick();
            pblk->errno = ModbusSvr_procotol_chain(pblk); //handle protocol
            if (pblk->errno)                              //occur error
                ModbusSvr_error_respose(pblk);
            else //occur finish
            {
                ModbusSvr_normal_respose(pblk);
            }
            pblk->ptrRegs[pblk->uRegLen - 1] = tick - pblk->uLTick;
            pblk->uLTick = tick;
        }
        pblk->nMBInterval = 0;
    }
}

/*********************************************************
 * @desc:  modbus协议解析处理程序
 * @param: pblk-指向数据块的指针
 *         pUSARTx-通信端口
 * @retval:None
 * ******************************************************/
u8 ModbusSvr_procotol_chain(Modbus_block *pblk)
{
    u16 reg_adr, uCrc1, uCrc2;
    u16 i, data_len, cur_adr;
    u8 cur_bit;
    u8 *ptr;
    short *ptrReg;

    u8 *tsk_buf = pblk->tsk_buf;

    if (tsk_buf[1] == 0 || (tsk_buf[1] > 6 && tsk_buf[1] < 15) || tsk_buf[1] > 16)
        return 1; //ILLEGAL FUNCTION

    if (pblk->frame_len > 250 || pblk->frame_len < 8)
        return 3; // ILLEGAL DATA VALUE

    uCrc1 = CRC16(tsk_buf, pblk->frame_len - 2);
    uCrc2 = tsk_buf[pblk->frame_len - 1] << 8 | tsk_buf[pblk->frame_len - 2];

    if (uCrc1 != uCrc2)
        return 3; // ILLEGAL DATA VALUE

    reg_adr = tsk_buf[2] << 8 | tsk_buf[3];
    data_len = tsk_buf[4] << 8 | tsk_buf[5];

    //----------Read Coils & Discrete inputs----------------------
    if (tsk_buf[1] == 1 || tsk_buf[1] == 2)
    {
        if (data_len > 960)
            return 3; // ILLEGAL DATA VALUE
        if (reg_adr < pblk->uCoilStartAdr)
            return 2; //ILLEGAL DATA ADDRESS
        if ((reg_adr + data_len) > pblk->uCoilEndAdr)
            return 2; //ILLEGAL DATA ADDRESS

        ptr = &tsk_buf[2];
        *ptr++ = BIT2BYTE(data_len);
        cur_adr = reg_adr - pblk->uCoilStartAdr;
        cur_bit = 0;
        for (i = 0; i < data_len; i++)
        {
            if (pblk->ptrCoils[cur_adr])
                *ptr |= 0x01 << cur_bit;
            else
                *ptr &= ~(0x01 << cur_bit);

            if (++cur_bit >= 8)
            {
                cur_bit = 0;
                ptr++;
            }
            cur_adr++;
        }
        pblk->trans_len = 5 + tsk_buf[2];
        return 0;
    }

    //----------Write single coil---------------------------------
    if (tsk_buf[1] == 5)
    {
        if (reg_adr < pblk->uCoilStartAdr)
            return 2; //ILLEGAL DATA ADDRESS
        if (reg_adr >= pblk->uCoilEndAdr)
            return 2; //ILLEGAL DATA ADDRESS

        if (data_len == 0xFF00)
            pblk->ptrCoils[reg_adr - pblk->uCoilStartAdr] = 1;
        else if (data_len == 0)
            pblk->ptrCoils[reg_adr - pblk->uCoilStartAdr] = 0;
        else
            return 3; //ILLEGAL DATA VALUE
        pblk->trans_len = 8;
        return 0;
    }

    //----------Write multiple coils---------------------------------
    if (tsk_buf[1] == 15)
    {
        if (reg_adr < pblk->uCoilStartAdr)
            return 2; //ILLEGAL DATA ADDRESS
        if ((reg_adr + data_len) > pblk->uCoilEndAdr)
            return 2; //ILLEGAL DATA ADDRESS

        cur_bit = 0;
        ptr = &tsk_buf[7];
        ptrReg = &pblk->ptrCoils[reg_adr - pblk->uCoilStartAdr];
        for (i = 0; i < data_len; i++)
        {
            *ptrReg++ = ((*ptr & (0x01 << cur_bit)) == 0) ? 0 : 1;
            if (++cur_bit >= 8)
            {
                cur_bit = 0;
                ptr++;
            }
        }
        pblk->trans_len = 8;
        return 0;
    }

    //----------Read Holding Register----------------------
    if (tsk_buf[1] == 3)
    {
        if (data_len > 125)
            return 3; // ILLEGAL DATA VALUE
        if (reg_adr < pblk->uRegStartAdr)
            return 2; //ILLEGAL DATA ADDRESS
        if ((reg_adr + data_len) > pblk->uRegEndAdr)
            return 2; //ILLEGAL DATA ADDRESS

        ptr = &tsk_buf[2];
        ptrReg = &pblk->ptrRegs[reg_adr - pblk->uRegStartAdr];
        *ptr++ = data_len << 1; //  Byte count
        for (i = 0; i < data_len; i++)
        {
            *ptr++ = *ptrReg >> 8;
            *ptr++ = *ptrReg & 0x00FF;
            ptrReg++;
        }
        pblk->trans_len = 5 + tsk_buf[2];
        return 0;
    }

    //----------Read Input Register----------------------
    if (tsk_buf[1] == 4)
    {
        if (data_len > 125)
            return 3; // ILLEGAL DATA VALUE
        if (reg_adr < pblk->uRomStartAdr)
            return 2; //ILLEGAL DATA ADDRESS
        if ((reg_adr + data_len) > pblk->uRomEndAdr)
            return 2; //ILLEGAL DATA ADDRESS

        ptr = &tsk_buf[2];
        ptrReg = &pblk->ptrRoms[reg_adr - pblk->uRomStartAdr];
        *ptr++ = data_len << 1; //  Byte count
        for (i = 0; i < data_len; i++)
        {
            *ptr++ = *ptrReg >> 8;
            *ptr++ = *ptrReg & 0x00FF;
            ptrReg++;
        }
        pblk->trans_len = 5 + tsk_buf[2];
        return 0;
    }

    //----------Write single holding register----------------------
    if (tsk_buf[1] == 6)
    {
        if (reg_adr < pblk->uRegStartAdr)
            return 2; //ILLEGAL DATA ADDRESS
        if (reg_adr >= pblk->uRegEndAdr)
            return 2; //ILLEGAL DATA ADDRESS

        pblk->ptrRegs[reg_adr - pblk->uRegStartAdr] = data_len;

        pblk->bSaved = 1;
        pblk->trans_len = 8;
        return 0;
    }

    //----------Write multiple holding register--------------------
    if (tsk_buf[1] == 16)
    {
        if (reg_adr < pblk->uRegStartAdr)
            return 2; //ILLEGAL DATA ADDRESS
        if ((reg_adr + data_len) > pblk->uRegEndAdr)
            return 2; //ILLEGAL DATA ADDRESS

        ptr = &tsk_buf[7];
        ptrReg = &pblk->ptrRegs[reg_adr - pblk->uRegStartAdr];
        for (i = 0; i < data_len; i++)
        {
            *ptrReg = ((*ptr++) << 8);
            *ptrReg |= (*ptr++);
            ptrReg++;
        }

        pblk->bSaved = 1;
        pblk->trans_len = 8;
        return 0;
    }

    return 1; //ILLEGAL_FUNCTION
}
/*********************************************************
 * @desc:  参数寄存器内容保存
 * @param: pblk-指向数据块的指针
 *         pUSARTx-通信端口
 * @retval:None
 * ******************************************************/
void ModbusSvr_save_para(Modbus_block *pblk)
{
    if (pblk->bSaved)
    {
        pblk->bSaved = 0;
    }
}

/*********************************************************
 * @desc:  中断处理程序
 * @param: pblk-指向数据块的指针
 *         pUSARTx-通信端口
 * @retval:None
 * ******************************************************/
void Modbus_UsartHandler(Modbus_block *pblk)
{
		/* Prevent unused argument(s) compilation warning */
		UNUSED(pblk->pUsart);
	
    if (pblk->bFrameStart)
    {
        if (RxChar != pblk->station && pblk->pos_msg == 0)
            pblk->bFrameStart = 0;

        pblk->isr_buf[pblk->pos_msg++] = RxChar;
    }
    pblk->nMBInterval = 0;
		HAL_UART_Receive_IT(pblk->pUsart, &RxChar, 1);
}

/*-------------------------------------------------------------------------------
 *	@brief	MODBUS CRC16计算程序
 *	@param	nData:需要计算的数据帧地址
 *					wLength: 需要计算的数据帧长度
 *	@retval	计算结果
 *----------------------------------------------------------------------------*/
static const uint16_t wCRCTable[] = {
    0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
    0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
    0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
    0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
    0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
    0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
    0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
    0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
    0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
    0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
    0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
    0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
    0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
    0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
    0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
    0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
    0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
    0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
    0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
    0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
    0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
    0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
    0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
    0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
    0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
    0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
    0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
    0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
    0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
    0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
    0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
    0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040};
u16 CRC16(const uint8_t *nData, uint8_t wLength)
{
    uint8_t nTemp;
    uint16_t wCRCWord = 0xFFFF;

    while (wLength--)
    {
        nTemp = *nData++ ^ wCRCWord;
        wCRCWord >>= 8;
        wCRCWord ^= wCRCTable[nTemp];
    }
    return wCRCWord;
}

//-------------------------------------------------------------------------------
//	@brief	发送一个字节
//	@param	pUSARTx:发送端口号
//					ch: 待发送的字节
//	@retval	None
//-------------------------------------------------------------------------------
void Usart_SendByte(UART_HandleTypeDef *pUSARTx, uint8_t ch)
{
    if (HAL_UART_Transmit(pUSARTx, (uint8_t*)&ch, 1, 0xFFFF) != HAL_OK)
    {
        Error_Handler();
    }
}

//-------------------------------------------------------------------------------
//	@brief	发送多个字节
//	@param	pUSARTx:发送端口号
//					ch: 待发送的字节
//	@retval	None
//-------------------------------------------------------------------------------
void Usart_SendBytes(UART_HandleTypeDef *pUSARTx, uint8_t *ptr, int n)
{
    if (HAL_UART_Transmit(pUSARTx, ptr, n, 0xFFFF) != HAL_OK)
    {
        Error_Handler();
    }
}

//-------------------------------------------------------------------------------
//	@brief	发送一个字符串
//	@param	pUSARTx:发送端口号
//					str: 待发送的字符串
//	@retval	None
//-------------------------------------------------------------------------------
void Usart_SendString(UART_HandleTypeDef *pUSARTx, char *str)
{
    if (HAL_UART_Transmit(pUSARTx, (uint8_t*)str, strlen(str), 0xFFFF) != HAL_OK)
    {
        Error_Handler();
    }
}

//-------------------------------------------------------------------------------
//	@brief	发送一个16位的字
//	@param	pUSARTx:发送端口号
//					ch: 待发送的字
//	@retval	None
//-------------------------------------------------------------------------------
void Usart_SendHalfWord(UART_HandleTypeDef *pUSARTx, uint16_t ch)
{
    uint8_t temp_h, temp_l;

    temp_h = (ch & 0XFF00) >> 8;
    temp_l = ch & 0XFF;
    HAL_UART_Transmit(pUSARTx,(uint8_t*)&temp_h,1,0xFFFF) ;
    HAL_UART_Transmit(pUSARTx,(uint8_t*)&temp_l,1,0xFFFF) ;
}

//-----------------end of file---------------------------------------------
