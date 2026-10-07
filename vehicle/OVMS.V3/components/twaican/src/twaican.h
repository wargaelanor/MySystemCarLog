/*
;    Project:       Open Vehicle Monitor System
;    Date:          7th October 2026
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#ifndef __TWAICAN_H__
#define __TWAICAN_H__

#include <stdint.h>
#include "can.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "ovms_mutex.h"

class twaican : public canbus
  {
  public:
    twaican(const char* name, int txpin, int rxpin);
    ~twaican();

  public:
    esp_err_t Start(CAN_mode_t mode, CAN_speed_t speed);
    esp_err_t Stop();

  public:
    esp_err_t Write(const CAN_frame_t* p_frame, TickType_t maxqueuewait=0);
    bool AsynchronousInterruptHandler(CAN_frame_t* frame, uint32_t* framesReceived);
    void TxCallback(CAN_frame_t* p_frame, bool success);

  protected:
    esp_err_t WriteFrame(const CAN_frame_t* p_frame);
    esp_err_t Deactivate();
    void ProcessAlerts(uint32_t alerts);

  public:
    void SetPowerMode(PowerMode powermode) override;
    bool GetErrorFlagsDesc(std::string &buffer, uint32_t error_flags) override;
    bool CheckRxStalled() override;

  public:
    static void AlertTask(void *pvParameters);

  public:
    gpio_num_t m_txpin;
    gpio_num_t m_rxpin;
    OvmsMutex m_write_mutex;

  protected:
    volatile bool m_started;
    volatile bool m_installed;
    volatile bool m_task_idle;
    bool m_tx_pending;
    uint32_t m_pending_alerts;
    TaskHandle_t m_task;
  };

#endif //#ifndef __TWAICAN_H__
