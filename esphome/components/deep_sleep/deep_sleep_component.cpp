#include "deep_sleep_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace deep_sleep {

static const char *TAG = "deep_sleep";

bool global_has_deep_sleep = false;

void DeepSleepComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Deep Sleep...");
  global_has_deep_sleep = true;

  if (this->run_duration_.has_value())
    this->set_timeout(*this->run_duration_, [this]() { this->begin_sleep(); });
}
void DeepSleepComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Setting up Deep Sleep...");
  if (this->sleep_duration_.has_value()) {
    uint32_t duration = *this->sleep_duration_ / 1000;
    ESP_LOGCONFIG(TAG, "  Sleep Duration: %u ms", duration);
  }
  if (this->run_duration_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Run Duration: %u ms", *this->run_duration_);
  }
#ifdef ARDUINO_ARCH_ESP32
  if (this->wakeup_pin_.has_value()) {
    LOG_PIN("  Wakeup Pin: ", *this->wakeup_pin_);
  }
#endif
}
void DeepSleepComponent::loop() {
  if (this->next_enter_deep_sleep_)
    this->begin_sleep();
}
float DeepSleepComponent::get_loop_priority() const {
  return -100.0f;  // run after everything else is ready
}
void DeepSleepComponent::set_sleep_duration(uint32_t time_ms) { this->sleep_duration_ = uint64_t(time_ms) * 1000; }
#ifdef ARDUINO_ARCH_ESP32
void DeepSleepComponent::set_wakeup_pin_mode(WakeupPinMode wakeup_pin_mode) {
  this->wakeup_pin_mode_ = wakeup_pin_mode;
}
void DeepSleepComponent::set_ext1_wakeup(Ext1Wakeup ext1_wakeup) { this->ext1_wakeup_ = ext1_wakeup; }


/*
  Handle an interrupt triggered when a pad is touched.
  Recognize what pad has been touched and save it in a table.
 */
static void DeepSleepComponent::tp_example_rtc_intr(void *arg)
{
    //clear interrupt
    ESP_LOGW(TAG, "Touch Pressed clearing interupt");
    touch_pad_clear_status();
}

void DeepSleepComponent::set_touch_wakeup(bool enable){
  if(enable == true){
    // If use interrupt trigger mode, should set touch sensor FSM mode at 'TOUCH_FSM_MODE_TIMER'.
    ESP_LOGW(TAG, "Set Touch fsm Mode");
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    // Register touch interrupt ISR
    ESP_LOGW(TAG, "Register TouchPad isr");
    touch_pad_isr_register(tp_example_rtc_intr, NULL);
    //interrupt mode, enable touch interrupt
    ESP_LOGW(TAG, "Enabled interupt for touch");
    touch_pad_intr_enable();
    esp_err_t sleepErrorInit = esp_sleep_enable_touchpad_wakeup();
    switch (sleepErrorInit)
    {
    case ESP_OK:
      ESP_LOGW(TAG, "ESP32 Touch wakeup has been set");
      break;
    case ESP_ERR_NOT_SUPPORTED:
      ESP_LOGW(TAG, "touch (CONFIG_ESP32_RTC_EXT_CRYST_ADDIT_CURRENT) is enabled.");
      break;
    case ESP_ERR_INVALID_STATE:
      ESP_LOGW(TAG, "wakeup triggers conflict for Touch Wakeup");
      break;
    default:
      break;
    }
  }
}
#endif
void DeepSleepComponent::set_run_duration(uint32_t time_ms) { this->run_duration_ = time_ms; }
void DeepSleepComponent::begin_sleep(bool manual) {
  if (this->prevent_ && !manual) {
    this->next_enter_deep_sleep_ = true;
    return;
  }
#ifdef ARDUINO_ARCH_ESP32
  if (this->wakeup_pin_mode_ == WAKEUP_PIN_MODE_KEEP_AWAKE && this->wakeup_pin_.has_value() &&
      !this->sleep_duration_.has_value() && (*this->wakeup_pin_)->digital_read()) {
    // Defer deep sleep until inactive
    if (!this->next_enter_deep_sleep_) {
      this->status_set_warning();
      ESP_LOGW(TAG, "Waiting for pin_ to switch state to enter deep sleep...");
    }
    this->next_enter_deep_sleep_ = true;
    return;
  }
#endif

  ESP_LOGI(TAG, "Beginning Deep Sleep");

  App.run_safe_shutdown_hooks();

#ifdef ARDUINO_ARCH_ESP32
  if (this->sleep_duration_.has_value())
    esp_sleep_enable_timer_wakeup(*this->sleep_duration_);
  if (this->wakeup_pin_.has_value()) {
    bool level = !(*this->wakeup_pin_)->is_inverted();
    if (this->wakeup_pin_mode_ == WAKEUP_PIN_MODE_INVERT_WAKEUP && (*this->wakeup_pin_)->digital_read())
      level = !level;
    esp_sleep_enable_ext0_wakeup(gpio_num_t((*this->wakeup_pin_)->get_pin()), level);
  }
  if (this->ext1_wakeup_.has_value()) {
    esp_sleep_enable_ext1_wakeup(this->ext1_wakeup_->mask, this->ext1_wakeup_->wakeup_mode);
  }
  esp_deep_sleep_start();
#endif

#ifdef ARDUINO_ARCH_ESP8266
  ESP.deepSleep(*this->sleep_duration_);
#endif
}
float DeepSleepComponent::get_setup_priority() const { return setup_priority::LATE; }
void DeepSleepComponent::prevent_deep_sleep() { this->prevent_ = true; }

}  // namespace deep_sleep
}  // namespace esphome
