//
// Created by Otto Winter on 26.11.17.
//

#include <esp_log.h>
#include "pca9685_output_component.h"

namespace esphomelib {

namespace output {

static const char *TAG = "output::pca9685";

PCA9685OutputComponent::PCA9685OutputComponent(float frequency, TwoWire &i2c_wire,
                                               PCA9685_PhaseBalancer phase_balancer,
                                               uint8_t mode)
    : frequency_(frequency), i2c_wire_(i2c_wire), phase_balancer_(phase_balancer), address_(0x00),
      mode_(mode), min_channel_(0xFF), max_channel_(0x00), update_(true) {
  for (uint16_t &pwm_amount : this->pwm_amounts_)
    pwm_amount = 0;
}

void PCA9685OutputComponent::setup() {
  ESP_LOGI(TAG, "Setting up PCA9685OutputComponent.");
  this->pwm_controller_ = PCA9685(this->i2c_wire_, this->phase_balancer_);
  ESP_LOGV(TAG, "    Resetting devices...");
  this->pwm_controller_.resetDevices();
  ESP_LOGV(TAG, "    Initializing...");
  this->pwm_controller_.init(this->address_, this->mode_);
  ESP_LOGV(TAG, "    Setting Frequency (%.0f)...", this->frequency_);
  this->pwm_controller_.setPWMFrequency(this->frequency_);
}

void PCA9685OutputComponent::loop() {
  if (this->min_channel_ == 0xFF || !this->update_)
    return;

  uint16_t *p = &this->pwm_amounts_[this->min_channel_];
  int len = this->max_channel_ - this->min_channel_ + 1;

  this->pwm_controller_.setChannelsPWM(this->min_channel_, len, p);
  this->update_ = false;
}

float PCA9685OutputComponent::get_setup_priority() const {
  return setup_priority::HARDWARE;
}

void PCA9685OutputComponent::set_channel_value(uint8_t channel, uint16_t value) {
  if (this->pwm_amounts_[channel] != value)
    this->update_ = true;
  this->pwm_amounts_[channel] = value;
}

PCA9685OutputComponent::Channel *PCA9685OutputComponent::create_channel(uint8_t channel,
                                                                        ATXComponent *atx,
                                                                        float max_power) {
  ESP_LOGV(TAG, "Getting channel %d...", channel);
  this->min_channel_ = std::min(this->min_channel_, channel);
  this->max_channel_ = std::max(this->max_channel_, channel);
  auto *c = new Channel(this, channel);
  c->set_atx(atx);
  c->set_max_power(max_power);
  return c;
}

float PCA9685OutputComponent::get_frequency() const {
  return this->frequency_;
}
void PCA9685OutputComponent::set_frequency(float frequency) {
  this->frequency_ = frequency;
}
TwoWire &PCA9685OutputComponent::get_i2c_wire() const {
  return this->i2c_wire_;
}
void PCA9685OutputComponent::set_i2c_wire(TwoWire &i2c_wire) {
  this->i2c_wire_ = i2c_wire;
}
PCA9685_PhaseBalancer PCA9685OutputComponent::get_phase_balancer() const {
  return this->phase_balancer_;
}
void PCA9685OutputComponent::set_phase_balancer(PCA9685_PhaseBalancer phase_balancer) {
  this->phase_balancer_ = phase_balancer;
}
uint8_t PCA9685OutputComponent::get_address() const {
  return this->address_;
}
void PCA9685OutputComponent::set_address(uint8_t address) {
  this->address_ = address;
}
uint8_t PCA9685OutputComponent::get_mode() const {
  return this->mode_;
}
void PCA9685OutputComponent::set_mode(uint8_t mode) {
  this->mode_ = mode;
}

void PCA9685OutputComponent::Channel::write_value_f(float adjusted_value) {
  uint16_t max_duty = 4096;
  auto duty = uint16_t(adjusted_value * max_duty);
  uint16_t duty_inverted = duty;

  if (this->is_inverted())
    duty_inverted = max_duty - duty;

  if (duty > 0)
    this->enable_atx();

  this->parent_->set_channel_value(this->channel_, duty_inverted);
}

PCA9685OutputComponent::Channel::Channel(PCA9685OutputComponent *parent, uint8_t channel)
    : FloatOutput(), HighPowerOutput(), parent_(parent), channel_(channel) {}

} // namespace output

} // namespace esphomelib
