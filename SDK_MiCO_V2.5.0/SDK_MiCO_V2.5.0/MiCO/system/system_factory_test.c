/**
 ******************************************************************************
 * @file    mico_factory_test.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide the factory test functions called by MiCO core
 *          before application's entrance: application_start().
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */


#include "time.h"
#include "mico.h"
#include "mico_config.h"
#include "platform.h"
#include "platform_config.h"
#include "StringUtils.h"
#include "CheckSumUtils.h"

#ifdef MICO_BLUETOOTH_ENABLE
#include "mico_bt.h"
#include "mico_bt_cfg.h"
#include "mico_bt_dev.h"
#include "mico_bt_smart_interface.h"
#include "mico_bt_smartbridge.h"
#include "mico_bt_smartbridge_gatt.h"
#endif

/* MFG test demo BEGIN */
extern int mfg_connect (char *ssid);
extern int mfg_scan (void);
extern void mfg_option (int use_udp, uint32_t remoteaddr);
extern char* system_lib_version (void);
extern void wlan_get_mac_address (char *mac);
extern void system_version(char *str, int len);
void app_crc (char *str,int len);
void serial_number (char *str, int len);

extern int mico_debug_enabled;

static char cmd_str[64];

static int _mfg_mode_ = 0;
static int test_for_app = 0;

int is_mfg_mode(void)
{
    return _mfg_mode_;
}

void init_mfg_mode(void)
{
    if (MicoShouldEnterMFGMode())
        _mfg_mode_ = 1;
    else
        _mfg_mode_ = 0;
}

void mf_printf(char *str)
{
  MicoUartSend( MFG_TEST, str, strlen(str));
}

static void mf_putc(char ch)
{
  MicoUartSend( MFG_TEST, &ch, 1);
}

static int get_line()
{
#define CNTLQ      0x11
#define CNTLS      0x13
#define DEL        0x7F
#define BACKSPACE  0x08
#define CR         0x0D
#define LF         0x0A
  
  char *p = cmd_str;
  int i = 0;
  char c;
  
  memset(cmd_str, 0, sizeof(cmd_str));
  while(1) {
    if( MicoUartRecv( MFG_TEST, p, 1, 100) != kNoErr)
      continue;
    
    mf_putc(*p);
    if (*p == BACKSPACE  ||  *p == DEL)  {
      if(i>0) {
        c = 0x20;
        mf_putc(c); 
        mf_putc(*p); 
        p--;
        i--; 
      }
      continue;
    }
    if(*p == CR || *p == LF) {
      *p = 0;
      return i;
    }
    
    p++;
    i++;
    if (i>sizeof(cmd_str))
      break;
  }
  
  return 0;
}


/**
* @brief  Display the Main Menu on HyperTerminal
* @param  None
* @retval None
*/
static char * ssid_get(void)
{
  char *cmd;
  uint32_t remote_addr = 0xFFFFFFFF;
  
  while (1)  {                                 /* loop forever                */
    mf_printf ("\r\nMXCHIP_MFMODE> ");
    get_line();
    cmd = cmd_str;
    if (strncmp(cmd, "tcp ", 4) == 0) {
      mf_printf ("\r\n");
      remote_addr = inet_addr(cmd+4);
      if (remote_addr == 0)
        remote_addr = 0xffffffff;
      sprintf(cmd, "Use TCP send packet to 0x%X\r\n", (unsigned int)remote_addr);
      mf_printf (cmd);
    } else if (strncmp(cmd, "udp ", 4) == 0) {
      mf_printf ("\r\n");
      remote_addr = inet_addr(cmd+4);
      if (remote_addr == 0)
        remote_addr = 0xffffffff;
      sprintf(cmd, "Use UDP send packet to 0x%X\r\n", (unsigned int)remote_addr);
      mf_printf (cmd);
    }  else if (strncmp(cmd, "ssid ", 5) == 0) {
      mf_printf ("\r\n");
      return cmd+5;
    } else {
      mf_printf ("Please input as \"ssid <ssid_string>\"");
      continue;
    }
  }
}

bool mfg_test_for_app (void)
{
  mico_uart_config_t uart_config;
  volatile ring_buffer_t  rx_buffer;
  volatile uint8_t *      rx_data;

  rx_data = malloc (50);
  require (rx_data, exit);

  /* Initialize UART interface */
  uart_config.baud_rate    = 115200;
  uart_config.data_width   = DATA_WIDTH_8BIT;
  uart_config.parity       = NO_PARITY;
  uart_config.stop_bits    = STOP_BITS_1;
  uart_config.flow_control = FLOW_CONTROL_DISABLED;
  uart_config.flags = UART_WAKEUP_DISABLE;

  ring_buffer_init ((ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, 50);
  MicoUartInitialize (MFG_TEST, &uart_config, (ring_buffer_t *)&rx_buffer);

  if (MicoUartRecv (MFG_TEST, cmd_str, 3, 100) == kNoErr) {
    if (cmd_str[0]=='#' && cmd_str[1]=='#' && cmd_str[2]=='#') {
      test_for_app = 1;
      return true;
    } else
      return false;
  }

exit:
  return false;
}


#ifdef MICO_BLUETOOTH_ENABLE
/* Scan complete handler. Scan complete event reported via this callback.
 * It runs on the MICO_NETWORKING_WORKER_THREAD context.
 */
static OSStatus scan_complete_handler( void *arg )
{
    UNUSED_PARAMETER(arg);
    OSStatus err = kNoErr;
    uint32_t count = 0;
    mico_bt_smart_scan_result_t *scan_result = NULL;


    mf_printf("BLE scan complete\r\n");
    err = mico_bt_smartbridge_get_scan_result_list( &scan_result, &count );
    require_noerr( err, exit );

    if( count == 0 )
    {
        mf_printf( "No ble device found\r\n" );
        err = kNotFoundErr;
        goto exit;
    }
    mf_printf("\r\n");
exit:
    /* Scan duration is complete */
    return err;
}

static OSStatus ble_scan_handler( const mico_bt_smart_advertising_report_t* result )
{
    OSStatus err = kNoErr;
    char*        bd_addr_str = NULL;
    char str[128];

    bd_addr_str = DataToHexStringWithColons( (uint8_t *)result->remote_device.address, 6 );
    snprintf( str, 128, "  ADDR: %s, RSSI: %d", bd_addr_str, result->signal_strength );
    mf_printf(str);
    free( bd_addr_str );
    mf_printf("\r\n");
    
    /* Scan duration is complete */
    return err;
}

extern mico_bt_cfg_settings_t mico_bt_cfg_settings;

void ble_scan(void)
{
  uint32_t count = 0;
  mico_bt_smart_scan_result_t *scan_result = NULL;
  /* Scan settings */
  mico_bt_smart_scan_settings_t scan_settings;

  scan_settings.type              = BT_SMART_PASSIVE_SCAN;
  scan_settings.filter_policy     = FILTER_POLICY_NONE;
  scan_settings.filter_duplicates = DUPLICATES_FILTER_ENABLED;
  scan_settings.interval          = MICO_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_INTERVAL;
  scan_settings.window            = MICO_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_WINDOW;
  scan_settings.duration_second   = 2;
  scan_settings.type = BT_SMART_PASSIVE_SCAN;

  /* Start scan */
  mico_bt_smartbridge_start_scan( &scan_settings, scan_complete_handler, ble_scan_handler );  

  mico_rtos_delay_seconds(2);

  mico_bt_smartbridge_stop_scan();

  mf_printf("BLE scan complete\r\n");
  mico_bt_smartbridge_get_scan_result_list( &scan_result, &count );

  if( count == 0 )
  {
    mf_printf( "No BLE device found\r\n" );
  }

  mf_printf("\r\n");
}

#endif


/* mxchip library manufacture test. */
void mxchip_mfg_test(void)
{
  char str[128];
  char mac[6];
  char *ssid;
  mico_uart_config_t uart_config;
  volatile ring_buffer_t  rx_buffer;
  volatile uint8_t *      rx_data;
  mico_debug_enabled = 0;
  
  rx_data = malloc(50);
  require(rx_data, exit);
  
  /* Initialize UART interface */
  uart_config.baud_rate    = 115200;
  uart_config.data_width   = DATA_WIDTH_8BIT;
  uart_config.parity       = NO_PARITY;
  uart_config.stop_bits    = STOP_BITS_1;
  uart_config.flow_control = FLOW_CONTROL_DISABLED;
  uart_config.flags = UART_WAKEUP_DISABLE;

  ring_buffer_init ((ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, 50);
  MicoUartInitialize (MFG_TEST, &uart_config, (ring_buffer_t *)&rx_buffer);

  mf_printf ("==== MXCHIP Manufacture Test ====\r\n");
  mf_printf ("Serial Number: ");
  mf_printf (SERIAL_NUMBER);
  mf_printf ("\r\n");

  mf_printf ("App CRC: ");
  memset (str, 0, sizeof (str));
  app_crc (str, sizeof (str));
  mf_printf (str);
  mf_printf ("\r\n");

  mf_printf ("Bootloader Version: ");
  mf_printf (mico_get_bootloader_ver());
  mf_printf ("\r\n");
  sprintf (str, "Library Version: %s\r\n", system_lib_version());
  mf_printf (str);
  mf_printf ("APP Version: ");
  memset (str, 0, sizeof (str));
  system_version (str, sizeof (str));
  mf_printf (str);
  mf_printf ("\r\n");
  memset (str, 0, sizeof (str));
  wlan_driver_version (str, sizeof (str));
  mf_printf ("Driver: ");
  mf_printf (str);
  mf_printf ("\r\n");

#ifdef MICO_BLUETOOTH_ENABLE
  /* Initialise MICO SmartBridge */
  mico_bt_init( MICO_BT_HCI_MODE, "SmartBridge Device", 0, 0 );  //Client + server connections
  mico_bt_smartbridge_init( 0 );
  mico_bt_dev_read_local_addr( (uint8_t *)mac );
  sprintf( str, "Local Bluetooth Address: %02X-%02X-%02X-%02X-%02X-%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
  mf_printf (str);
  ble_scan();
#endif

  wlan_get_mac_address (mac);
  sprintf (str, "MAC: %02X-%02X-%02X-%02X-%02X-%02X\r\n",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  mf_printf(str);
  mfg_scan();

  if (test_for_app==0) {
  ssid = ssid_get();
    mfg_connect (ssid);
  }

exit:
  mico_thread_sleep(MICO_NEVER_TIMEOUT);
}

void app_crc (char *str,int len)
{
  mico_logic_partition_t *partition_flash;
  CRC16_Context mfg_context;
  uint8_t *mfgbuf;
  uint16_t crc = 0;
  uint32_t flash_addr = 0x0;
  int flash_len,buf_len;
  mfgbuf = malloc (1024);
  partition_flash = MicoFlashGetInfo (MICO_PARTITION_APPLICATION);
  flash_len = partition_flash->partition_length;
  CRC16_Init (&mfg_context);

  while (flash_len > 0) {
    if (flash_len > 1024) {
      buf_len = 1024;
    }  else {
      buf_len = flash_len;
    }

    flash_len -= buf_len;
    MicoFlashRead (MICO_PARTITION_APPLICATION, &flash_addr, (uint8_t *)mfgbuf, buf_len);
    CRC16_Update (&mfg_context, (uint8_t *)mfgbuf, buf_len);
  }

  CRC16_Final (&mfg_context, &crc);

  snprintf (str, len, "%04X", crc);

}


#ifdef MFG_MODE_AUTO
static void uartRecvMfg_thread(void *inContext);
static size_t _uart_get_one_packet(uint8_t* inBuf, int inBufLen);

void mico_mfg_test(mico_Context_t *inContext)
{
  network_InitTypeDef_adv_st wNetConfig;
  int testCommandFd, scanFd;
  uint8_t *buf = NULL;
  int recvLength = -1;
  fd_set readfds;
  struct timeval_t t;
  struct sockaddr_t addr;
  socklen_t addrLen;
  mico_uart_config_t uart_config;
  volatile ring_buffer_t  rx_buffer;
  volatile uint8_t *       rx_data;
  OSStatus err;
  
  buf = malloc(1500);
  require_action(buf, exit, err = kNoMemoryErr);
  rx_data = malloc(2048);
  require_action(rx_data, exit, err = kNoMemoryErr);
  
  /* Connect to a predefined Wlan */
  memset( &wNetConfig, 0x0, sizeof(network_InitTypeDef_adv_st) );
  
  strncpy( (char*)wNetConfig.ap_info.ssid, "William Xu", maxSsidLen );
  wNetConfig.ap_info.security = SECURITY_TYPE_AUTO;
  memcpy( wNetConfig.key, "mx099555", maxKeyLen );
  wNetConfig.key_len = strlen( "mx099555" );
  wNetConfig.dhcpMode = DHCP_Client;
  
  wNetConfig.wifi_retry_interval = 100;
  micoWlanStartAdv(&wNetConfig);
  
  /* Initialize UART interface */
  uart_config.baud_rate    = 115200;
  uart_config.data_width   = DATA_WIDTH_8BIT;
  uart_config.parity       = NO_PARITY;
  uart_config.stop_bits    = STOP_BITS_1;
  uart_config.flow_control = FLOW_CONTROL_DISABLED;
  uart_config.flags = UART_WAKEUP_DISABLE;
  
  ring_buffer_init  ( (ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, 2048 );
  MicoUartInitialize( UART_FOR_APP, &uart_config, (ring_buffer_t *)&rx_buffer );
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "MFG UART Recv", uartRecvMfg_thread, 0x300, (void*)inContext );
  
  /* Initialize UDP interface */
  t.tv_sec = 5;
  t.tv_usec = 0;
  
  scanFd = socket(AF_INET, SOCK_DGRM, IPPROTO_UDP);
  require_action(IsValidSocket( scanFd ), exit, err = kNoResourcesErr );
  
  addr.s_port = 23230;
  addr.s_ip = INADDR_ANY;
  err = bind(scanFd, &addr, sizeof(addr));
  require_noerr(err, exit);
  
  testCommandFd = socket(AF_INET, SOCK_DGRM, IPPROTO_UDP);
  require_action(IsValidSocket( testCommandFd ), exit, err = kNoResourcesErr );
  
  addr.s_port = 23231;
  addr.s_ip = INADDR_ANY;
  err = bind(testCommandFd, &addr, sizeof(addr));
  require_noerr(err, exit);
  
  while(1) {
    /*Check status on erery sockets on bonjour query */
    FD_ZERO( &readfds );
    FD_SET( testCommandFd, &readfds );
    FD_SET( scanFd, &readfds );
    select( 1, &readfds, NULL, NULL, &t );
    
    /* Scan and return MAC address */ 
    if (FD_ISSET(scanFd, &readfds)) {
      recvLength = recvfrom(scanFd, buf, 1500, 0, &addr, &addrLen); 
      sendto(scanFd, inContext->micoStatus.mac, sizeof(inContext->micoStatus.mac), 0, &addr, addrLen);
    }
    
    /* Recv UDP data and send to COM */
    if (FD_ISSET(testCommandFd, &readfds)) {
      recvLength = recvfrom(testCommandFd, buf, 1500, 0, &addr, &addrLen); 
      MicoUartSend(UART_FOR_APP, buf, recvLength);
    }
  }
  
exit:
  if(buf) free(buf);  
}

void uartRecvMfg_thread(void *inContext)
{
  mico_Context_t *Context = inContext;
  int recvlen;
  uint8_t *inDataBuffer;
  
  inDataBuffer = malloc(500);
  require(inDataBuffer, exit);
  
  while(1) {
    recvlen = _uart_get_one_packet(inDataBuffer, 500);
    if (recvlen <= 0)
      continue; 
    else{
      /* if(......)   Should valid the UART input */
      Context->flashContentInRam.micoSystemConfig.configured = unConfigured;
      MICOUpdateConfiguration ( Context );
    }
  }
  
exit:
  if(inDataBuffer) free(inDataBuffer);
}


static size_t _uart_get_one_packet(uint8_t* inBuf, int inBufLen)
{
  
  int datalen;
  
  while(1) {
    if( MicoUartRecv( UART_FOR_APP, inBuf, inBufLen, 500) == kNoErr){
      return inBufLen;
    }
    else{
      datalen = MicoUartGetLengthInBuffer( UART_FOR_APP );
      if(datalen){
        MicoUartRecv(UART_FOR_APP, inBuf, datalen, 500);
        return datalen;
      }
    }
    
  }
}
#endif

/* MFG test demo END */














