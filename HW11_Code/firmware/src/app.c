
#include "app.h"
#include"I2C2_Commands.h"


#include<math.h>


// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************

#define IMU_Address 0b1101011 // Address for the IMU
#define reg_Accel 0x10
#define reg_Gyro 0x11
#define reg_Contr 0x12
#define reg_OUT_TEMP_L 0x20

#define maxTime 4

int startTime = 0;
unsigned char data[14];

/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
 */


void IMU_init(void) {
    // Initializes the IMU
    unsigned char sendbyte;

    i2c_master_start();
    sendbyte = (IMU_Address << 1) | (0b00000000); // Writing    
    i2c_master_send(sendbyte); // Send Device ID with Write Byte
    i2c_master_send(reg_Accel); // CTRL1_XL Register
    i2c_master_send(0b10000010); //1.66 kHz [1000], 2g [00], 100 Hz Filter [10]
    i2c_master_stop();

    i2c_master_start();
    sendbyte = (IMU_Address << 1) | (0b00000000); // Writing    
    i2c_master_send(sendbyte); // Send Device ID with Write Byte
    i2c_master_send(reg_Gyro); // CTRL2_G Register
    i2c_master_send(0b10001000); // 1.66kHz [1000], 1000 dps sense [10], [00]
    i2c_master_stop();

    i2c_master_start();
    sendbyte = (IMU_Address << 1) | (0b00000000); // Writing    
    i2c_master_send(sendbyte); // Send Device ID with Write Byte
    i2c_master_send(reg_Contr); // CTRL3_C Register
    i2c_master_send(0b00000100); // [00000100]
    i2c_master_stop();
}

void I2C_read_multiple(unsigned char address, unsigned char regRead, unsigned char * data, int length) {
    unsigned char sendbyte;

    // Read from registers
    i2c_master_start();
    sendbyte = (IMU_Address << 1) | (0b00000000); // Writing    
    i2c_master_send(sendbyte); // Send Device ID with Write Byte
    i2c_master_send(regRead); // The Register to read
    i2c_master_restart();
    sendbyte = (IMU_Address << 1) | (0b00000001); // Reading    
    i2c_master_send(sendbyte); // Send Device ID with Read Byte

    int i = 0;
    // Read all data
    for (i = 0; i < length; i++) {
        data[i] = i2c_master_recv(); // Receive current register
        if (i == length - 1) {
            i2c_master_ack(1);
        } else {
            i2c_master_ack(0);
        }
    }

    i2c_master_stop(); // End Comms

}

void debugLED(void) {
    // Configure AN4 as output
    ANSELA = 0;
    TRISAbits.TRISA4 = 0;
    // Turn on the LED
    LATAbits.LATA4 = 1;
}


APP_DATA appData;

/* Mouse Report */
MOUSE_REPORT mouseReport APP_MAKE_BUFFER_DMA_READY;
MOUSE_REPORT mouseReportPrevious APP_MAKE_BUFFER_DMA_READY;


// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

void APP_USBDeviceHIDEventHandler(USB_DEVICE_HID_INDEX hidInstance,
        USB_DEVICE_HID_EVENT event, void * eventData, uintptr_t userData) {
    APP_DATA * appData = (APP_DATA *) userData;

    switch (event) {
        case USB_DEVICE_HID_EVENT_REPORT_SENT:

            /* This means the mouse report was sent.
             We are free to send another report */

            appData->isMouseReportSendBusy = false;
            break;

        case USB_DEVICE_HID_EVENT_REPORT_RECEIVED:

            /* Dont care for other event in this demo */
            break;

        case USB_DEVICE_HID_EVENT_SET_IDLE:

            /* Acknowledge the Control Write Transfer */
            USB_DEVICE_ControlStatus(appData->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);

            /* save Idle rate received from Host */
            appData->idleRate = ((USB_DEVICE_HID_EVENT_DATA_SET_IDLE*) eventData)->duration;
            break;

        case USB_DEVICE_HID_EVENT_GET_IDLE:

            /* Host is requesting for Idle rate. Now send the Idle rate */
            USB_DEVICE_ControlSend(appData->deviceHandle, &(appData->idleRate), 1);

            /* On successfully receiving Idle rate, the Host would acknowledge back with a
               Zero Length packet. The HID function driver returns an event
               USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT to the application upon
               receiving this Zero Length packet from Host.
               USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT event indicates this control transfer
               event is complete */

            break;

        case USB_DEVICE_HID_EVENT_SET_PROTOCOL:
            /* Host is trying set protocol. Now receive the protocol and save */
            appData->activeProtocol = *(USB_HID_PROTOCOL_CODE *) eventData;

            /* Acknowledge the Control Write Transfer */
            USB_DEVICE_ControlStatus(appData->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);
            break;

        case USB_DEVICE_HID_EVENT_GET_PROTOCOL:

            /* Host is requesting for Current Protocol. Now send the Idle rate */
            USB_DEVICE_ControlSend(appData->deviceHandle, &(appData->activeProtocol), 1);

            /* On successfully receiving Idle rate, the Host would acknowledge
              back with a Zero Length packet. The HID function driver returns
              an event USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT to the
              application upon receiving this Zero Length packet from Host.
              USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT event indicates
              this control transfer event is complete */
            break;

        case USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT:
            break;

        default:
            break;
    }
}

/*******************************************************************************
  Function:
    void APP_USBDeviceEventHandler (USB_DEVICE_EVENT event,
        USB_DEVICE_EVENT_DATA * eventData)

  Summary:
    Event callback generated by USB device layer.

  Description:
    This event handler will handle all device layer events.

  Parameters:
    None.

  Returns:
    None.
 */

void APP_USBDeviceEventHandler(USB_DEVICE_EVENT event, void * eventData, uintptr_t context) {
    USB_DEVICE_EVENT_DATA_CONFIGURED * configurationValue;
    switch (event) {
        case USB_DEVICE_EVENT_SOF:
            /* This event is used for switch debounce. This flag is reset
             * by the switch process routine. */
            appData.sofEventHasOccurred = true;
            appData.setIdleTimer++;
            break;
        case USB_DEVICE_EVENT_RESET:
        case USB_DEVICE_EVENT_DECONFIGURED:

            /* Device got deconfigured */

            appData.isConfigured = false;
            appData.isMouseReportSendBusy = false;
            appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            //appData.emulateMouse = true;
            //BSP_LEDOn ( APP_USB_LED_1 );
            //BSP_LEDOn ( APP_USB_LED_2 );
            //BSP_LEDOff ( APP_USB_LED_3 );

            break;

        case USB_DEVICE_EVENT_CONFIGURED:

            /* Device is configured */
            configurationValue = (USB_DEVICE_EVENT_DATA_CONFIGURED *) eventData;
            if (configurationValue->configurationValue == 1) {
                appData.isConfigured = true;

                //BSP_LEDOff ( APP_USB_LED_1 );
                //BSP_LEDOff ( APP_USB_LED_2 );
                //BSP_LEDOn ( APP_USB_LED_3 );

                /* Register the Application HID Event Handler. */

                USB_DEVICE_HID_EventHandlerSet(appData.hidInstance,
                        APP_USBDeviceHIDEventHandler, (uintptr_t) & appData);
            }
            break;

        case USB_DEVICE_EVENT_POWER_DETECTED:

            /* VBUS was detected. We can attach the device */
            USB_DEVICE_Attach(appData.deviceHandle);
            break;

        case USB_DEVICE_EVENT_POWER_REMOVED:

            /* VBUS is not available any more. Detach the device. */
            USB_DEVICE_Detach(appData.deviceHandle);
            break;

        case USB_DEVICE_EVENT_SUSPENDED:
            //BSP_LEDOff ( APP_USB_LED_1 );
            //BSP_LEDOn ( APP_USB_LED_2 );
            //BSP_LEDOn ( APP_USB_LED_3 );
            break;

        case USB_DEVICE_EVENT_RESUMED:
        case USB_DEVICE_EVENT_ERROR:
        default:
            break;

    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void) {
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;
    appData.deviceHandle = USB_DEVICE_HANDLE_INVALID;
    appData.isConfigured = false;
    //appData.emulateMouse = true;
    appData.hidInstance = 0;
    appData.isMouseReportSendBusy = false;
    
    
    startTime = _CP0_GET_COUNT();
    
    
    // Configure Bits
    ANSELA = 0;
    ANSELB = 0;
    
    // Initialize communications
    i2c_master_setup();
    // Initialize the imu
    IMU_init();
}

/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks(void) {
    static int8_t vector = 0;
    static uint8_t movement_length = 0;
    int8_t dir_table[] = {-4, -4, -4, 0, 4, 4, 4, 0};
    
    static char ComI = 0;

    /* Check the application's current state. */
    switch (appData.state) {
            /* Application's initial state. */
        case APP_STATE_INIT:
        {
            /* Open the device layer */
            appData.deviceHandle = USB_DEVICE_Open(USB_DEVICE_INDEX_0,
                    DRV_IO_INTENT_READWRITE);

            if (appData.deviceHandle != USB_DEVICE_HANDLE_INVALID) {
                /* Register a callback with device layer to get event notification (for end point 0) */
                USB_DEVICE_EventHandlerSet(appData.deviceHandle,
                        APP_USBDeviceEventHandler, 0);

                appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            } else {
                /* The Device Layer is not ready to be opened. We should try
                 * again later. */
            }
            break;
        }

        case APP_STATE_WAIT_FOR_CONFIGURATION:

            /* Check if the device is configured. The 
             * isConfigured flag is updated in the
             * Device Event Handler */

            if (appData.isConfigured) {
                appData.state = APP_STATE_MOUSE_EMULATE;
            }
            break;

        case APP_STATE_MOUSE_EMULATE:

            // Read the accelerometer
            //unsigned char data[14];
            I2C_read_multiple(IMU_Address, reg_OUT_TEMP_L, data, 14);
            // Shift Data into variables to get raw data
            short temperature = (data[1] << 8) | data[0];
            short gyroX = (data[3] << 8) | data[2];
            short gyroY = (data[5] << 8) | data[4];
            short gyroZ = (data[7] << 8) | data[6];
            short accelX = (data[9] << 8) | data[8];
            short accelY = (data[11] << 8) | data[10];
            short accelZ = (data[13] << 8) | data[12];

            ComI++;
            if (ComI >= maxTime)
            {
                ComI = 0;
                appData.mouseButton[0] = MOUSE_BUTTON_STATE_RELEASED;
                appData.mouseButton[1] = MOUSE_BUTTON_STATE_RELEASED;
                appData.xCoordinate = accelX/256;
                appData.yCoordinate = accelY/256;
            }
            else
            {
                appData.mouseButton[0] = MOUSE_BUTTON_STATE_RELEASED;
                appData.mouseButton[1] = MOUSE_BUTTON_STATE_RELEASED;
                appData.xCoordinate = 0;
                appData.yCoordinate = 0;
            }
            /*
            // every 50th loop, or 20 times per second
            if (movement_length > 50) {
                appData.mouseButton[0] = MOUSE_BUTTON_STATE_RELEASED;
                appData.mouseButton[1] = MOUSE_BUTTON_STATE_RELEASED;
                appData.xCoordinate = (int8_t) dir_table[vector & 0x07];
                appData.yCoordinate = (int8_t) dir_table[(vector + 2) & 0x07];
                vector++;
                movement_length = 0;
            }*/

            if (!appData.isMouseReportSendBusy) {
                /* This means we can send the mouse report. The
                   isMouseReportBusy flag is updated in the HID Event Handler. */

                appData.isMouseReportSendBusy = true;

                /* Create the mouse report */

                MOUSE_ReportCreate(appData.xCoordinate, appData.yCoordinate,
                        appData.mouseButton, &mouseReport);

                if (memcmp((const void *) &mouseReportPrevious, (const void *) &mouseReport,
                        (size_t)sizeof (mouseReport)) == 0) {
                    /* Reports are same as previous report. However mouse reports
                     * can be same as previous report as the coordinate positions are relative.
                     * In that case it needs to be sent */
                    if ((appData.xCoordinate == 0) && (appData.yCoordinate == 0)) {
                        /* If the coordinate positions are 0, that means there
                         * is no relative change */
                        if (appData.idleRate == 0) {
                            appData.isMouseReportSendBusy = false;
                        } else {
                            /* Check the idle rate here. If idle rate time elapsed
                             * then the data will be sent. Idle rate resolution is
                             * 4 msec as per HID specification; possible range is
                             * between 4msec >= idlerate <= 1020 msec.
                             */
                            if (appData.setIdleTimer
                                    >= appData.idleRate * 4) {
                                /* Send REPORT as idle time has elapsed */
                                appData.isMouseReportSendBusy = true;
                            } else {
                                /* Do not send REPORT as idle time has not elapsed */
                                appData.isMouseReportSendBusy = false;
                            }
                        }
                    }

                }
                if (appData.isMouseReportSendBusy == true) {
                    /* Copy the report sent to previous */
                    memcpy((void *) &mouseReportPrevious, (const void *) &mouseReport,
                            (size_t)sizeof (mouseReport));

                    /* Send the mouse report. */
                    USB_DEVICE_HID_ReportSend(appData.hidInstance,
                            &appData.reportTransferHandle, (uint8_t*) & mouseReport,
                            sizeof (MOUSE_REPORT));
                    appData.setIdleTimer = 0;
                }
                movement_length++;
            }

            break;

        case APP_STATE_ERROR:

            break;

            /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */

