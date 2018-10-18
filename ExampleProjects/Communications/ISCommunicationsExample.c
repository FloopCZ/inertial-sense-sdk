/*
MIT LICENSE

Copyright 2014 Inertial Sense, LLC - http://inertialsense.com

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>

// STEP 1: Add Includes
// Change these include paths to the correct paths for your project
#include "../../src/ISComm.h"
#include "../../src/serialPortPlatform.h"

static int running = 1;

static void handleInsMessage(ins_1_t* ins)
{
	printf("INS TimeOfWeek: %.3fs, LLA: %3.7f,%3.7f,%5.2f, Euler: %5.1f,%5.1f,%5.1f\r\n", 
		ins->timeOfWeek,
		ins->lla[0], ins->lla[1], ins->lla[2], 
		ins->theta[0] * C_RAD2DEG_F, ins->theta[1] * C_RAD2DEG_F, ins->theta[2] * C_RAD2DEG_F );
}

static void handleGpsMessage(gps_pos_t* pos)
{
	printf("GPS TimeOfWeek: %dms, LLA: %3.7f,%3.7f,%5.2f\r\n", pos->timeOfWeekMs, pos->lla[0], pos->lla[1], pos->lla[2]);
}

static void handleImuMessage(dual_imu_t* imu)
{
	printf("IMU Time: %.3fs, PQR: %5.1f,%5.1f,%5.1f, ACC: %5.1f,%5.1f,%5.1f,\r\n", 
		imu->time, 
		imu->I[0].pqr[0], imu->I[0].pqr[1], imu->I[0].pqr[2],
		imu->I[0].acc[0], imu->I[0].acc[1], imu->I[0].acc[2]);
}


int set_configuration(serial_port_t *serialPort, is_comm_instance_t *comm)
{
	// Set INS output Euler rotation in radians to 90 degrees roll for mounting
	float rotation[3] = { 90.0f*C_DEG2RAD_F, 0.0f, 0.0f };
	int messageSize = is_comm_set_data(comm, _DID_FLASH_CONFIG, offsetof(nvm_flash_cfg_t, insRotation), sizeof(float) * 3, rotation);
	if (messageSize != serialPortWrite(serialPort, comm->buffer, messageSize))
	{
		printf("Failed to encode and write set INS rotation\r\n");
		return -3;
	}

	return 0;
}


int stop_message_broadcasting(serial_port_t *serialPort, is_comm_instance_t *comm)
{
	// Stop all broadcasts on the device
	int messageSize = is_comm_stop_broadcasts(comm);
	if (messageSize != serialPortWrite(serialPort, comm->buffer, messageSize))
	{
		printf("Failed to encode and write stop broadcasts message\r\n");
		return -3;
	}
	return 0;
}


int enable_message_broadcasting_RMC(serial_port_t *serialPort, is_comm_instance_t *comm)
{
	// Enable broadcasts using RMC: DID_INS_1 @ 20Hz and DID_GPS_NAV @ 5Hz
	rmc_t rmc;
	rmc.bits = RMC_BITS_INS1 | RMC_BITS_GPS1_POS;
	rmc.options = 0;
	rmc.insPeriodMs = 50;	// INS @ 20Hz

	int messageSize = is_comm_set_data(comm, _DID_RMC, 0, sizeof(rmc_t), &rmc);
	if (messageSize != serialPortWrite(serialPort, comm->buffer, messageSize))
	{
		printf("Failed to encode and write RMC message\r\n");
		return -3;
	}
	return 0;
}


int enable_message_broadcasting_get_data(serial_port_t *serialPort, is_comm_instance_t *comm)
{
	// Ask for INS message 20 times a second (period of 50 milliseconds).  Max rate is 500 times a second (2ms period).
	int messageSize = is_comm_get_data(comm, _DID_INS_LLA_EULER_NED, 0, 0, 50);
	if (messageSize != serialPortWrite(serialPort, comm->buffer, messageSize))
	{
		printf("Failed to encode and write get INS message\r\n");
		return -4;
	}

#if 1
	// Ask for gps message 5 times a second (period of 200 milliseconds) - offset and size can be left at 0 unless you want to just pull a specific field from a data set
	messageSize = is_comm_get_data(comm, _DID_GPS1_POS, 0, 0, 200);
	if (messageSize != serialPortWrite(serialPort, comm->buffer, messageSize))
	{
		printf("Failed to encode and write get GPS message\r\n");
		return -5;
	}
#endif

#if 0
	// Ask for IMU data 10 times a second - this could be as high as 1000 times a second (a period of 1)
	messageSize = is_comm_get_data(comm, _DID_IMU_DUAL, 0, 0, 100);
	if (messageSize != serialPortWrite(serialPort, comm->buffer, messageSize))
	{
		printf("Failed to encode and write get IMU message\r\n");
		return -6;
	}
#endif
	return 0;
}


int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		printf("Please pass the com port as the only argument\r\n");
		// In Visual Studio IDE, this can be done through "Project Properties -> Debugging -> Command Arguments: COM3" 
		return -1;
	}


	// STEP 2: Init comm instance
	is_comm_instance_t comm;
	uint8_t buffer[2048];

	// Make sure to assign a valid buffer and buffer size to the comm instance
	comm.buffer = buffer;
	comm.bufferSize = sizeof(buffer);

	// Initialize the comm instance, sets up state tracking, packet parsing, etc.
	is_comm_init(&comm);


	// STEP 3: Initialize and open serial port
	serial_port_t serialPort;

	// Initialize the serial port (Windows, MAC or Linux) - if using an embedded system like Arduino,
	//  you will need to handle the serial port creation, open and reads yourself. In this
	//  case, you do not need to include serialPort.h/.c and serialPortPlatform.h/.c in your project.
	serialPortPlatformInit(&serialPort);

	// Open serial, last parameter is a 1 which means a blocking read, you can set as 0 for non-blocking
	// you can change the baudrate to a supported baud rate (IS_BAUDRATE_*), make sure to reboot the uINS
	//  if you are changing baud rates, you only need to do this when you are changing baud rates.
	if (!serialPortOpen(&serialPort, argv[1], IS_BAUDRATE_921600, 1))
	{
		printf("Failed to open serial port on com port %s\r\n", argv[1]);
		return -2;
	}


	int error;

	// STEP 4: Stop any message broadcasting
	if (error = stop_message_broadcasting(&serialPort, &comm))
	{
		return error;
	}


#if 0	// STEP 5: Set configuration
	if (error = set_configuration(&serialPort, &comm))
	{
		return error; 
	}
#endif

	
#if 1	// STEP 6: Enable message broadcasting
	if (error = enable_message_broadcasting_RMC(&serialPort, &comm))
#else
	if (error = enable_message_broadcasting_get_data(&serialPort, &comm))
#endif
	{
		return error;
	}


	// STEP 7: Handle received data
	int count;
	uint8_t inByte;

	// You can set running to false with some other piece of code to break out of the loop and end the program
	while (running)
	{
		// Read one byte with a 20 millisecond timeout
		while ((count = serialPortReadCharTimeout(&serialPort, &inByte, 20)) > 0)
		{
			switch (is_comm_parse(&comm, inByte))
			{
			case _DID_INS_LLA_EULER_NED:
				handleInsMessage((ins_1_t*)buffer);
				break;

			case _DID_GPS1_POS:
				handleGpsMessage((gps_pos_t*)buffer);
				break;

			case _DID_IMU_DUAL:
				handleImuMessage((dual_imu_t*)buffer);
				break;

				// TODO: add other cases for other data ids that you care about
			}
		}
	}
}
