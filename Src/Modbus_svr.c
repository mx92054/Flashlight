/***************************************************
 * Copyright (C),2019 www.idsse.ac.cn
 * Written by chenming
 * Version 1.0
 * Data  2019-3-3
 * Description: modbus接口实现文件
 * *************************************************/

#include <stdio.h>
#include "Modbus_svr.h"

Modbus_block mblock1;

extern UART_HandleTypeDef huart1;

//-------------------------------------------------------------------------------
//	@brief	协议栈初始化
//	@param	None
//	@retval	None
//-------------------------------------------------------------------------------
void Modbus_init(void)
{
	char msg[100];

	ModbusSvr_block_init(&mblock1, &huart1);
/*
	sprintf(msg, "\r\nStation No: %d, Baudrate: %d", mblock1.station, mblock1.baudrate);
	Usart_SendString(&huart1, msg);
	sprintf(msg, "\r\nCoil Start adr: %4d, Len: %4d", mblock1.uCoilStartAdr, mblock1.uCoilLen);
	Usart_SendString(&huart1, msg);
	sprintf(msg, "\r\nReg  Start adr: %4d, Len: %4d", mblock1.uRegStartAdr, mblock1.uRegLen);
	Usart_SendString(&huart1, msg);
	sprintf(msg, "\r\nRom  Start adr: %4d, Len: %4d", mblock1.uRomStartAdr, mblock1.uRomLen);
	Usart_SendString(&huart1, msg);*/
}

//-------------------------------------------------------------------------------
//	@brief	协议任务调度
//	@param	None
//	@retval	None
//-------------------------------------------------------------------------------
void Modbus_task(void)
{
	ModbusSvr_task(&mblock1);
}

//-------------------------------------------------------------------------------
//	@brief	modbus recieve counter
//	@param	None
//	@retval	None
//-------------------------------------------------------------------------------
void ModbusTimer(void)
{
	mblock1.nMBInterval++;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if ( huart == &huart1)
		Modbus_UsartHandler(&mblock1) ;
}


void Modbus_SavePara(void)
{
	ModbusSvr_save_para(&mblock1) ;
}


//-----------------end of file---------------------------------------------
