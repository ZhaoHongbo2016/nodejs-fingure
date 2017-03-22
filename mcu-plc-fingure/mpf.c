#include "mpf.h"

void delay(uint num){
    while(num--);
}

//启动初始化
void initMain(){
    //初始化发送指令前7个byte
    for(ucharTemp = 0; ucharTemp < 7; ucharTemp ++){
        sendBuffer[ucharTemp] = sendPackageHeader[ucharTemp];
    }

    //将串口接受寄存器的数据长度置0
    receiveBufferLength = 0;
    
    //接受到了串口通知开关置0
    receiveCmdNotify = 0;
    //等待串口消息开关置0
    waitForReceive = 0;

    EA = 1;     //总中断开关打开
}

void main(){
	delay(65535);
    delay(65535);
    delay(65535);
    delay(65535);
    delay(65535);
    delay(65535);
    delay(65535);
    delay(65535);

    initUart();
    initMain();

    while( getAddressListFunction() ){
    	initUart();
    	initMain();

    	delay(65535);
	    delay(65535);
	    delay(65535);
	    delay(65535);
    }

    //初始化指令为:0x11，向指纹模块发送采集指纹用来验证的命令
    sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;

    while(1){
        /* 主循环中首先判断是否有串口响应，如果有，则本次循环用来处理消息响应 */
        if(receiveCmdNotify == 1){
            receiveCmdNotify = 0;

            receiveEventFunction();     //接住消息

            continue;
        }

        /*
         * 如果没有串口响应
         * 首先判断是否在等待下位机反馈，如果在等待，就开始延时计数
         */   
        if(waitForReceive == 1){
            waitForReceive = 0;
            if(waitTimes < 3)
                waitForReceiveFunction();
            else
                resetFingureFunction();
            continue;
        }

        /* 
         * 继续判断P2口有没有录入通知
         * 如果有录入通知，并且当前状态为验证状态，则发送录入命令
         * 如果没有录入通知，则发送验证命令
         */
        inputSignal = checkInputSignal();

        if(inputSignal == 0xFF){		//如果PLC返回信号在0xFF说明目前跟PLC在通讯的中途，为了加快通信，需要立即返回。
        	continue;
        }

        if((sendCmdStatus ==  ACTION_GET_IMAGE_FOR_CHECK) && inputSignal){
            switch(inputSignal){
	        	case 0x01:
		        	newFingureAddressIndex = getNewAddressIndexByPower(messageBuffer[3]);    //通过权限来构造一个空位置
		            noFingureTimesWhenInput = 0;                                        //将录入指纹时的重复次数归零
		            sendCmdStatus = ACTION_GET_IMAGE_FOR_INPUT1;
	        		break;
	        	case 0x09:
	        		break;
	        }
        }

        if(receiveCmdNotify == 0)
            sendCmdFunction();
        delay(2000);
    }
}

/**
 * 串口中断函数，用来接收下位机数据
 */
void serialInterruptCallback() interrupt 4 {
	if(RI){				//本次中断是接受中断
		RI = 0;			//接受完了清零

        //将接受到的字节追加到指令接受缓冲区
		receiveByte = SBUF;
		receiveBuffer[receiveBufferLength] = receiveByte;
        receiveBufferLength++;

        if(receiveBufferLength == 1 && receiveByte!=0xEF){
            receiveBufferLength = 0;
            return;
        }

        if(receiveBufferLength == 2 && receiveByte!=0x01){
            receiveBufferLength = 0;
            return;
        }

        if(receiveBufferLength == 3 && receiveByte!=0xFF){
            receiveBufferLength = 0;
            return;
        }

        if(receiveBufferLength == 4 && receiveByte!=0xFF){
            receiveBufferLength = 0;
            return;
        }

        if(receiveBufferLength == 5 && receiveByte!=0xFF){
            receiveBufferLength = 0;
            return;
        }

        if(receiveBufferLength == 6 && receiveByte!=0xFF){
            receiveBufferLength = 0;
            return;
        }

        if(receiveBufferLength == 9){
            //如果指令接受到9个byte，这时候包长度信息有了
            receivePackageLength = (receiveBuffer[7] << 8) + receiveBuffer[8];
        }else if (receiveBufferLength == 9 + receivePackageLength) {
            //结束一轮消息接受，清空buffer，解析命令
            
            //获取校验和
            receiveCheckSum = (receiveBuffer[7 + receivePackageLength] << 8) + receiveBuffer[8 + receivePackageLength];
            //获取确认码
            cfmCode = receiveBuffer[9];
            //获取参数
            for (ucit = 0; ucit < receivePackageLength - 3; ucit++) {
                receiveParams[ucit] = receiveBuffer[10 + ucit];
            }

            //验证校验和
            uiit = 0;
            for (ucit = 6; ucit < receiveBufferLength - 2; ucit++) {
                uiit += receiveBuffer[ucit];
            }

            if (uiit == receiveCheckSum) {
                //校验正确，发射消息响应
                receiveCmdNotify = 1;
                receiveEventStatus = sendCmdStatus - 100;    //事件类型根据发送类型对应
                waitForReceive = 0;                          //解除等待锁
                waitTimes = 0;                               //等待响应次数复位
            }
            //清空指令buffer
            receiveBufferLength = 0;
        }            
	}
}

/* 根据receiveEventStatus，解析串口响应 */
void receiveEventFunction(){
    switch(receiveEventStatus){
        case EVENT_GET_IMAGE_FOR_CHECK:
            if(cfmCode == 0){
                //传感器采集到指纹
                sendCmdStatus = ACTION_BUILD_CB1_FOR_CHECK;
            }else if(cfmCode == 2){ 
                //传感器上没有手指，延时后继续发送采集命令
                sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;
                delay(3000);
            }
            break;
        case EVENT_BUILD_CB1_FOR_CHECK:
            if(cfmCode == 0){
                //生成特征码成功
                sendCmdStatus = ACTION_SEARCH;
                delay(3000);
            }else{
                //生成特征码失败
                showWarning();
                sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;
                delay(3000);
            }
            break;
        case EVETN_SEARCH:
            if(cfmCode == 0){
                //@TODO:搜索到匹配指纹
                uintTemp = receiveParams[0];
                uintTemp = uintTemp<<8;
                uintTemp += receiveParams[1];

                //整理向PLC发送的数据
                bcdBuffer[3] = uintTemp % 10;
                bcdBuffer[1] = uintTemp / 100;
                bcdBuffer[2] = (uintTemp / 10) % 10;
                bcdBuffer[0] = uintTemp / 100 + 1;
				sendPLCMessage();

				//@TODO:在数码管上测试，需要删掉
                uintTemp = uintTemp / 100;
                P1 = display_code[uintTemp + 1];
                sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;
                delay(65535);
                delay(65535);
                delay(65535);
                delay(65535);
            }else{
                //搜索失败
                showWarning();
                P1 = 0x3F;
                sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;
                delay(3000);    
            }
            break;
        case EVENT_GET_IMAGE_FOR_INPUT1:
            if(cfmCode == 0){
                //传感器采集到指纹
                sendCmdStatus = ACTION_BUILD_CB1_FOR_INPUT;
                noFingureTimesWhenInput = 0;        //录入时没有指纹的次数归零
            }else if(cfmCode == 2){ 
                //传感器上没有手指，延时后继续发送采集命令，计数30次后重置为验证指纹
                noFingureTimesWhenInput++;
                if(noFingureTimesWhenInput > NO_FINGURE_WHEN_INPUT_MAX_TIME)
                    sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;
                else    
                    sendCmdStatus = ACTION_GET_IMAGE_FOR_INPUT1;
                delay(3000);
            }
            break;
        case EVENT_BUILD_CB1_FOR_INPUT:
            if(cfmCode == 0){
                //生成特征码成功
                sendCmdStatus = ACTION_GET_IMAGE_FOR_INPUT2;
                delay(3000);
            }else{
                //生成特征码失败
                showWarning();
                sendCmdStatus = ACTION_GET_IMAGE_FOR_INPUT1;
                delay(3000);
            }
            break;
        case EVENT_GET_IMAGE_FOR_INPUT2:
            if(cfmCode == 0){
                //传感器采集到指纹
                sendCmdStatus = ACTION_BUILD_CB2_FOR_INPUT;
                noFingureTimesWhenInput = 0;
            }else if(cfmCode == 2){ 
                //传感器上没有手指，延时后继续发送采集命令，计数30次后重置为验证指纹
                noFingureTimesWhenInput++;
                if(noFingureTimesWhenInput > NO_FINGURE_WHEN_INPUT_MAX_TIME)
                    sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;
                else    
                    sendCmdStatus = ACTION_GET_IMAGE_FOR_INPUT2;
                delay(3000);
            }
            break;
        case EVENT_BUILD_CB2_FOR_INPUT:
            if(cfmCode == 0){
                //生成特征码成功
                sendCmdStatus = ACTION_MEARGE_CODE;
                delay(3000);
            }else{
                //生成特征码失败
                showWarning();
                sendCmdStatus = ACTION_GET_IMAGE_FOR_INPUT2;
                delay(3000);
            }
            break;
        case EVENT_MEARGE_CODE:
            if(cfmCode == 0){
                //特征码合并成功
                sendCmdStatus = ACTION_SAVE_ADDRESS;
                delay(3000);
            }else{
                //特征码合并失败
                showWarning();
                sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;
                delay(3000);
            }
            break;
        case EVENT_SAVE_ADDRESS:
            if(cfmCode == 0){
                //指纹特征保存成功
                sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;
                delay(3000);
                updateFingureAddress(newFingureAddressIndex);
            }else{
                //特征码合并失败
                showWarning();
                sendCmdStatus = ACTION_GET_IMAGE_FOR_CHECK;
                delay(3000);
            }
            break;
    }
}

/* 交互反馈 */
void showWarning(){

}

/* 检查P2是否有请求录入指纹的信号 */
uchar checkInputSignal(){
	//读GPIO_PLCIO高4位
    ucharTemp = readPLCIOH4();

   	return processPLCInput(ucharTemp);
}



uchar readPLCIOH4(){
	return GPIO_PLCIO>>4;
}

void  writePLCIOL4(uchar dat){
	ucharTemp = GPIO_PLCIO & 0xF0;
	GPIO_PLCIO = ucharTemp | dat;
}

uchar sendPLCMessage(){
	uint times = 100000;

	//============E==================
	writePLCIOL4(0x0E);

	ucharTemp = readPLCIOH4();
	
	while(ucharTemp != 0x0E){
		ucharTemp = readPLCIOH4();
		times--;
		if(times == 0)
			return 1;
		delay(50);
	}

	//===========0===================
	writePLCIOL4(bcdBuffer[0]);

	ucharTemp = readPLCIOH4();
	
	while(ucharTemp != bcdBuffer[0]){
		ucharTemp = readPLCIOH4();
		times--;
		if(times == 0)
			return 2;
		delay(50);
	}

	//============E==================
	writePLCIOL4(0x0E);

	ucharTemp = readPLCIOH4();
	
	while(ucharTemp != 0x0E){
		ucharTemp = readPLCIOH4();
		times--;
		if(times == 0)
			return 3;
		delay(50);
	}

	//===========1==================
	writePLCIOL4(bcdBuffer[1]);

	ucharTemp = readPLCIOH4();
	
	while(ucharTemp != bcdBuffer[1]){
		ucharTemp = readPLCIOH4();
		times--;
		if(times == 0)
			return 4;
		delay(50);
	}

	//============E==================
	writePLCIOL4(0x0E);

	ucharTemp = readPLCIOH4();
	
	while(ucharTemp != 0x0E){
		ucharTemp = readPLCIOH4();
		times--;
		if(times == 0)
			return 5;
		delay(50);
	}

	//===========2==================
	writePLCIOL4(bcdBuffer[2]);

	ucharTemp = readPLCIOH4();
	
	while(ucharTemp != bcdBuffer[2]){
		ucharTemp = readPLCIOH4();
		times--;
		if(times == 0)
			return 6;
		delay(50);
	}

	//============E==================
	writePLCIOL4(0x0E);

	ucharTemp = readPLCIOH4();
	
	while(ucharTemp != 0x0E){
		ucharTemp = readPLCIOH4();
		times--;
		if(times == 0)
			return 7;
		delay(50);
	}

	//===========3==================
	writePLCIOL4(bcdBuffer[3]);

	ucharTemp = readPLCIOH4();
	
	while(ucharTemp != bcdBuffer[3]){
		ucharTemp = readPLCIOH4();
		times--;
		if(times == 0)
			return 8;
		delay(50);
	}

	//============B==================
	writePLCIOL4(0x0B);

	return 0;
}

//对输入进行响应
uchar processPLCInput(uchar dat){
	switch(dat){
		case 0x0B:
			writePLCIOL4(0x0B);					//反馈一个信号
			if(messageBufferLength != 0){
				messageBufferLength = 0;		//清空消息寄存器
				return messageBuffer[0];		//处理消息寄存器
			}
			return 0x00;
		case 0x0E:
			delay(50);							//延时，再读一次，确保稳定
			if(readPLCIOH4() == 0x0E){
				messageReadble = 1;				//打开接受数据开关，准备接受E后面的数据
				writePLCIOL4(0x0E);
				return 0xFF;	
			}
			return 0x00;						
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x09:
			delay(50);							//延时，再读一次，确保稳定
			if(messageReadble && dat == readPLCIOH4()){
				messageReadble = 0;
				messageBuffer[messageBufferLength] = dat;
				messageBufferLength++;
				writePLCIOL4(dat);
			}
			return 0xFF;
	}

	return 0x00;
}