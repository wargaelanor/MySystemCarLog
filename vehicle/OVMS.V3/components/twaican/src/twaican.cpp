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

#include "ovms_log.h"
static const char *TAG = "twaican";

#include <string.h>
#include <sstream>
#include "twaican.h"

#define TWAICAN_ALERTS \
  (TWAI_ALERT_RX_DATA | TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | \
   TWAI_ALERT_ABOVE_ERR_WARN | TWAI_ALERT_BELOW_ERR_WARN | TWAI_ALERT_ERR_ACTIVE | \
   TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED | \
   TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_RX_FIFO_OVERRUN | \
   TWAI_ALERT_PERIPH_RESET)

#define TWAICAN_ERROR_ALERTS \
  (TWAI_ALERT_TX_FAILED | TWAI_ALERT_ABOVE_ERR_WARN | TWAI_ALERT_ERR_PASS | \
   TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL | \
   TWAI_ALERT_RX_FIFO_OVERRUN | TWAI_ALERT_PERIPH_RESET)

#define TWAICAN_RX_DRAIN_LIMIT 16

static bool TwaicanTimingConfig(CAN_speed_t speed, twai_timing_config_t* t_config)
  {
  switch (speed)
    {
    case CAN_SPEED_33KBPS:
      *t_config = twai_timing_config_t{ .clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 500000,
        .brp = 0, .tseg_1 = 12, .tseg_2 = 2, .sjw = 2, .triple_sampling = false };
      break;
    case CAN_SPEED_83KBPS:
      *t_config = twai_timing_config_t{ .clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 1250000,
        .brp = 0, .tseg_1 = 12, .tseg_2 = 2, .sjw = 2, .triple_sampling = false };
      break;
    case CAN_SPEED_50KBPS:
      *t_config = TWAI_TIMING_CONFIG_50KBITS();
      break;
    case CAN_SPEED_100KBPS:
      *t_config = TWAI_TIMING_CONFIG_100KBITS();
      break;
    case CAN_SPEED_125KBPS:
      *t_config = TWAI_TIMING_CONFIG_125KBITS();
      break;
    case CAN_SPEED_250KBPS:
      *t_config = TWAI_TIMING_CONFIG_250KBITS();
      break;
    case CAN_SPEED_500KBPS:
      *t_config = TWAI_TIMING_CONFIG_500KBITS();
      break;
    case CAN_SPEED_1000KBPS:
      *t_config = TWAI_TIMING_CONFIG_1MBITS();
      break;
    default:
      return false;
    }
  return true;
  }

twaican::twaican(const char* name, int txpin, int rxpin)
  : canbus(name)
  {
  m_txpin = (gpio_num_t)txpin;
  m_rxpin = (gpio_num_t)rxpin;
  m_started = false;
  m_installed = false;
  m_task_idle = true;
  m_tx_pending = false;
  m_pending_alerts = 0;
  m_task = NULL;

  if (xTaskCreatePinnedToCore(AlertTask, "twai alert", 3072, this, 20, &m_task, CORE(0)) != pdPASS)
    {
    m_task = NULL;
    ESP_LOGE(TAG, "%s: failed to create alert task", this->GetName());
    }

  m_powermode = Off;
  SetPowerMode(Off);
  }

twaican::~twaican()
  {
  Deactivate();
  if (m_task)
    {
    vTaskDelete(m_task);
    m_task = NULL;
    }
  }

esp_err_t twaican::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  if (!m_task)
    {
    ESP_LOGE(TAG, "%s: Start requested but alert task is unavailable", this->GetName());
    return ESP_FAIL;
    }

  if (m_mode != CAN_MODE_OFF)
    {
    Stop();
    }

  canbus::Start(mode, speed);

  m_mode = mode;
  m_speed = speed;

  twai_timing_config_t t_config;
  if (!TwaicanTimingConfig(speed, &t_config))
    {
    ESP_LOGE(TAG, "%s: unsupported speed %d", this->GetName(), MAP_CAN_SPEED(speed));
    m_mode = CAN_MODE_OFF;
    return ESP_FAIL;
    }

  if (Deactivate() != ESP_OK)
    {
    m_mode = CAN_MODE_OFF;
    return ESP_FAIL;
    }

  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(m_txpin, m_rxpin,
    (mode == CAN_MODE_LISTEN) ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL);
  g_config.tx_queue_len = 1;
  g_config.rx_queue_len = 16;
  g_config.alerts_enabled = TWAICAN_ALERTS;

  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t res = twai_driver_install(&g_config, &t_config, &f_config);
  if (res != ESP_OK)
    {
    ESP_LOGE(TAG, "%s: twai_driver_install failed: %s", this->GetName(), esp_err_to_name(res));
    m_mode = CAN_MODE_OFF;
    return ESP_FAIL;
    }
  m_installed = true;

  res = twai_start();
  if (res != ESP_OK)
    {
    ESP_LOGE(TAG, "%s: twai_start failed: %s", this->GetName(), esp_err_to_name(res));
    Deactivate();
    m_mode = CAN_MODE_OFF;
    return ESP_FAIL;
    }

  m_started = true;

  {
  OvmsMutexLock lock(&m_write_mutex);
  if (!m_tx_pending && uxQueueMessagesWaiting(m_txqueue))
    {
    CAN_frame_t frame;
    if (xQueueReceive(m_txqueue, (void*)&frame, 0) == pdTRUE)
      {
      if (WriteFrame(&frame) == ESP_OK)
        {
        m_tx_pending = true;
        canbus::Write(&frame, 0);
        }
      else
        {
        CAN_queue_msg_t msg;
        msg.type = CAN_txfailedcallback;
        msg.body.frame = frame;
        msg.body.bus = this;
        xQueueSend(MyCan.m_rxqueue, &msg, 0);
        }
      }
    }
  }

  ESP_LOGI(TAG, "%s: started in %s mode at %dbps", this->GetName(),
    (mode == CAN_MODE_LISTEN) ? "listen" : "active", MAP_CAN_SPEED(speed));
  pcp::SetPowerMode(On);
  return ESP_OK;
  }

esp_err_t twaican::Stop()
  {
  if (m_mode == CAN_MODE_OFF)
    {
    pcp::SetPowerMode(Off);
    return ESP_OK;
    }

  canbus::Stop();

  m_started = false;

  if (m_installed)
    {
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK && status.state == TWAI_STATE_RUNNING)
      {
      twai_stop();
      }
    }

  {
  OvmsMutexLock lock(&m_write_mutex);
  m_tx_pending = false;
  m_pending_alerts = 0;
  }

  pcp::SetPowerMode(Off);
  m_mode = CAN_MODE_OFF;
  return ESP_OK;
  }

esp_err_t twaican::Deactivate()
  {
  m_started = false;

  for (int i = 0; i < 200 && !m_task_idle; i++)
    {
    vTaskDelay(pdMS_TO_TICKS(10));
    }
  if (!m_task_idle)
    {
    ESP_LOGE(TAG, "%s: alert task did not go idle", this->GetName());
    return ESP_FAIL;
    }

  {
  OvmsMutexLock lock(&m_write_mutex);
  m_tx_pending = false;
  m_pending_alerts = 0;
  }

  if (!m_installed)
    {
    return ESP_OK;
    }

  esp_err_t res = ESP_FAIL;
  for (int i = 0; i < 100; i++)
    {
    twai_status_info_t status;
    if (twai_get_status_info(&status) != ESP_OK)
      {
      break;
      }
    if (status.state == TWAI_STATE_RUNNING)
      {
      twai_stop();
      continue;
      }
    if (status.state == TWAI_STATE_RECOVERING)
      {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
      }
    res = twai_driver_uninstall();
    if (res == ESP_OK)
      {
      break;
      }
    vTaskDelay(pdMS_TO_TICKS(10));
    }

  if (res != ESP_OK)
    {
    ESP_LOGE(TAG, "%s: failed to uninstall TWAI driver: %s", this->GetName(), esp_err_to_name(res));
    return res;
    }

  m_installed = false;
  return ESP_OK;
  }

esp_err_t twaican::WriteFrame(const CAN_frame_t* p_frame)
  {
  twai_message_t msg = {};
  msg.extd = (p_frame->FIR.B.FF == CAN_frame_ext);
  msg.rtr = (p_frame->FIR.B.RTR == CAN_RTR);
  msg.identifier = p_frame->MsgID;
  msg.data_length_code = p_frame->FIR.B.DLC;
  memcpy(msg.data, p_frame->data.u8, sizeof(msg.data));

  return twai_transmit(&msg, 0);
  }

esp_err_t twaican::Write(const CAN_frame_t* p_frame, TickType_t maxqueuewait /*=0*/)
  {
  OvmsMutexLock lock(&m_write_mutex);

  if (m_mode != CAN_MODE_ACTIVE)
    {
    ESP_LOGW(TAG,"Cannot write %s when not in ACTIVE mode",m_name);
    return ESP_FAIL;
    }

  if (m_tx_pending || uxQueueMessagesWaiting(m_txqueue))
    {
    return QueueWrite(p_frame, maxqueuewait);
    }

  if (!m_started || WriteFrame(p_frame) != ESP_OK)
    {
    return QueueWrite(p_frame, maxqueuewait);
    }

  canbus::Write(p_frame, maxqueuewait);
  m_tx_pending = true;

  return ESP_OK;
  }

void twaican::TxCallback(CAN_frame_t* p_frame, bool success)
  {
  canbus::TxCallback(p_frame, success);

  OvmsMutexLock lock(&m_write_mutex);

  if (m_mode == CAN_MODE_OFF || !m_started || m_tx_pending)
    {
    return;
    }

  CAN_frame_t frame;
  while (xQueueReceive(m_txqueue, (void*)&frame, 0) == pdTRUE)
    {
    if (WriteFrame(&frame) == ESP_OK)
      {
      m_tx_pending = true;
      canbus::Write(&frame, 0);
      break;
      }
    CAN_queue_msg_t msg;
    msg.type = CAN_txfailedcallback;
    msg.body.frame = frame;
    msg.body.bus = this;
    xQueueSend(MyCan.m_rxqueue, &msg, 0);
    }
  }

void twaican::ProcessAlerts(uint32_t alerts)
  {
  m_status.error_flags |= (alerts & TWAICAN_ERROR_ALERTS);

  if (alerts & TWAI_ALERT_BELOW_ERR_WARN)
    {
    m_status.error_flags &= ~((uint32_t)TWAI_ALERT_ABOVE_ERR_WARN);
    }
  if (alerts & TWAI_ALERT_ERR_ACTIVE)
    {
    m_status.error_flags &= ~((uint32_t)(TWAI_ALERT_ABOVE_ERR_WARN | TWAI_ALERT_ERR_PASS));
    }

  CAN_log_type_t log_status = CAN_LogNone;

  if (alerts & (TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED))
    {
    bool success = (alerts & TWAI_ALERT_TX_SUCCESS) != 0;
    OvmsMutexLock lock(&m_write_mutex);
    if (m_tx_pending)
      {
      m_tx_pending = false;
      CAN_queue_msg_t msg;
      msg.type = success ? CAN_txcallback : CAN_txfailedcallback;
      msg.body.frame = m_tx_frame;
      msg.body.bus = this;
      xQueueSend(MyCan.m_rxqueue, &msg, 0);
      }
    }

  if (alerts & (TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_RX_FIFO_OVERRUN))
    {
    m_status.rxbuf_overflow++;
    log_status = CAN_LogStatus_Error;
    }

  twai_status_info_t status;
  if (twai_get_status_info(&status) == ESP_OK)
    {
    m_status.errors_tx = status.tx_error_counter;
    m_status.errors_rx = status.rx_error_counter;
    if (status.state == TWAI_STATE_BUS_OFF)
      {
      m_status.errors_tx |= 0x100;
      m_status.errors_rx |= 0x100;
      }
    }

  if (alerts & TWAI_ALERT_BUS_OFF)
    {
    ESP_LOGW(TAG, "%s: bus-off condition, initiating recovery", this->GetName());
    {
    OvmsMutexLock lock(&m_write_mutex);
    if (m_tx_pending)
      {
      m_tx_pending = false;
      CAN_queue_msg_t msg;
      msg.type = CAN_txfailedcallback;
      msg.body.frame = m_tx_frame;
      msg.body.bus = this;
      xQueueSend(MyCan.m_rxqueue, &msg, 0);
      }
    }
    if (m_mode != CAN_MODE_OFF)
      {
      esp_err_t res = twai_initiate_recovery();
      if (res != ESP_OK && res != ESP_ERR_INVALID_STATE)
        {
        ESP_LOGE(TAG, "%s: twai_initiate_recovery failed: %s", this->GetName(), esp_err_to_name(res));
        }
      }
    log_status = CAN_LogStatus_Error;
    }

  if (alerts & TWAI_ALERT_BUS_RECOVERED)
    {
    m_status.error_flags &= ~((uint32_t)TWAI_ALERT_BUS_OFF);
    if (m_mode != CAN_MODE_OFF && m_started)
      {
      esp_err_t res = twai_start();
      if (res == ESP_OK)
        {
        ESP_LOGI(TAG, "%s: bus recovery completed, controller restarted", this->GetName());
        log_status = CAN_LogStatus_Statistics;
        }
      else
        {
        ESP_LOGE(TAG, "%s: twai_start after bus recovery failed: %s", this->GetName(), esp_err_to_name(res));
        log_status = CAN_LogStatus_Error;
        }
      }
    }

  if (alerts & (TWAI_ALERT_ABOVE_ERR_WARN | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR |
      TWAI_ALERT_TX_FAILED | TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_RX_FIFO_OVERRUN))
    {
    log_status = CAN_LogStatus_Error;
    }
  else if (alerts & (TWAI_ALERT_BELOW_ERR_WARN | TWAI_ALERT_ERR_ACTIVE))
    {
    log_status = CAN_LogStatus_Statistics;
    }

  if (log_status != CAN_LogNone)
    {
    LogStatus(log_status);
    }
  }

bool twaican::AsynchronousInterruptHandler(CAN_frame_t* frame, uint32_t* framesReceived)
  {
  *framesReceived = 0;

  if (m_mode == CAN_MODE_OFF || !m_started)
    {
    return false;
    }

  uint32_t alerts;
  {
  OvmsMutexLock lock(&m_write_mutex);
  alerts = m_pending_alerts;
  m_pending_alerts = 0;
  }

  if (alerts)
    {
    ProcessAlerts(alerts);
    }

  uint32_t drained = 0;
  while (drained < TWAICAN_RX_DRAIN_LIMIT)
    {
    twai_message_t msg;
    if (twai_receive(&msg, 0) != ESP_OK)
      {
      break;
      }

    memset(frame, 0, sizeof(*frame));
    frame->origin = this;
    frame->FIR.B.DLC = (msg.data_length_code > 8) ? 8 : msg.data_length_code;
    frame->FIR.B.FF = msg.extd ? CAN_frame_ext : CAN_frame_std;
    frame->FIR.B.RTR = msg.rtr ? CAN_RTR : CAN_no_RTR;
    frame->MsgID = msg.identifier;
    memcpy(frame->data.u8, msg.data, sizeof(frame->data.u8));
    (*framesReceived)++;
    drained++;
    MyCan.IncomingFrame(frame);
    }

  return drained >= TWAICAN_RX_DRAIN_LIMIT;
  }

void twaican::AlertTask(void *pvParameters)
  {
  twaican *me = (twaican*)pvParameters;
  uint32_t alerts;

  while (1)
    {
    if (!me->m_started)
      {
      me->m_task_idle = true;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
      }

    me->m_task_idle = false;

    if (twai_read_alerts(&alerts, pdMS_TO_TICKS(50)) == ESP_OK)
      {
      if (!me->m_started)
        {
        continue;
        }
      me->m_status.interrupts++;

      OvmsMutexLock lock(&me->m_write_mutex);
      me->m_pending_alerts |= alerts;
      CAN_queue_msg_t msg;
      msg.type = CAN_asyncinterrupthandler;
      msg.body.bus = me;
      if (xQueueSend(MyCan.m_rxqueue, &msg, 0) != pdTRUE)
        {
        me->m_status.isr_queue_overrun++;
        }
      }
    else
      {
      OvmsMutexLock lock(&me->m_write_mutex);
      if (me->m_pending_alerts != 0)
        {
        CAN_queue_msg_t msg;
        msg.type = CAN_asyncinterrupthandler;
        msg.body.bus = me;
        if (xQueueSend(MyCan.m_rxqueue, &msg, 0) != pdTRUE)
          {
          me->m_status.isr_queue_overrun++;
          }
        }
      }
    }
  }

void twaican::SetPowerMode(PowerMode powermode)
  {
  switch (powermode)
    {
    case On:
      ESP_LOGI(TAG, "%s: SetPowerMode on", this->GetName());
      if (m_mode != CAN_MODE_OFF)
        {
        Start(m_mode, m_speed);
        }
      break;
    case Sleep:
    case DeepSleep:
    case Off:
      pcp::SetPowerMode(powermode);
      ESP_LOGI(TAG, "%s: SetPowerMode off", this->GetName());
      if (m_mode != CAN_MODE_OFF)
        {
        Stop();
        }
      break;
    default:
      pcp::SetPowerMode(powermode);
      break;
    }
  }

bool twaican::CheckRxStalled()
  {
  if (!m_started)
    {
    return false;
    }

  twai_status_info_t status;
  if (twai_get_status_info(&status) != ESP_OK)
    {
    return false;
    }
  return status.msgs_to_rx > 0;
  }

bool twaican::GetErrorFlagsDesc(std::string &buffer, uint32_t error_flags)
  {
  std::ostringstream ss;

  if (error_flags & TWAI_ALERT_TX_FAILED)       ss << " | " << "TX failed";
  if (error_flags & TWAI_ALERT_ABOVE_ERR_WARN)  ss << " | " << "Error warning limit exceeded";
  if (error_flags & TWAI_ALERT_ERR_PASS)        ss << " | " << "Error passive";
  if (error_flags & TWAI_ALERT_BUS_ERROR)       ss << " | " << "Bus error";
  if (error_flags & TWAI_ALERT_BUS_OFF)         ss << " | " << "Bus-off";
  if (error_flags & TWAI_ALERT_RX_QUEUE_FULL)   ss << " | " << "RX queue overflow (frame lost)";
  if (error_flags & TWAI_ALERT_RX_FIFO_OVERRUN) ss << " | " << "RX FIFO overrun (frame lost)";
  if (error_flags & TWAI_ALERT_PERIPH_RESET)    ss << " | " << "Controller reset";

  buffer = ss.str();
  return true;
  }
