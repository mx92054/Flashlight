/***************************************************
 * Copyright (C),2019 www.idsse.ac.cn
 * Written by chenming
 * Version 1.0
 * Data  2019-3-3
 * Description: modbus接口头文件
 * *************************************************/
#ifndef __MODBUS_COM__
#define __MODBUS_COM__

#include "Mbsvr_comm.h"

extern Modbus_block mblock1 ;
//------------------Function Define ----------------------------------

void Modbus_init(void);
void Modbus_task(void);
void ModbusTimer(void);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

void Modbus_SavePara(void);
static void MODBUS_Config(u32 baud);

#endif
//------------end fo file----------------------------------
