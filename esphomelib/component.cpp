//
// Created by Otto Winter on 25.11.17.
//

#include "component.h"

#include <utility>
#include <esp32-hal.h>

namespace esphomelib {

float Component::get_loop_priority() const {
  return 0.0f;
}

float Component::get_setup_priority() const {
  return 0.0f;
}

void Component::setup() {

}

void Component::loop() {

}

void Component::set_interval(const std::string &name, uint32_t interval, time_func_t f) {
  this->cancel_interval(name);
  struct TimeFunction function = {
      .name = name,
      .type = TimeFunction::INTERVAL,
      .interval = interval,
      .last_execution = millis(),
      .f = std::move(f)
  };
  this->time_functions_.push_back(function);
}

bool Component::cancel_interval(const std::string &name) {
  return this->cancel_time_function(name, TimeFunction::INTERVAL);
}

void Component::set_timeout(const std::string &name, uint32_t timeout, time_func_t f) {
  this->cancel_timeout(name);
  struct TimeFunction function = {
      .name = name,
      .type = TimeFunction::TIMEOUT,
      .interval = timeout,
      .last_execution = millis(),
      .f = std::move(f)
  };
  this->time_functions_.push_back(function);
}

bool Component::cancel_timeout(const std::string &name) {
  return this->cancel_time_function(name, TimeFunction::TIMEOUT);
}

void Component::loop_() {
  assert_setup(this);
  this->component_state_ = LOOP;

  for (int i = 0; i < this->time_functions_.size();) {
    TimeFunction *tf = &this->time_functions_[i];
    if (millis() - tf->last_execution > tf->interval) {
      tf->f();
      tf = &this->time_functions_[i]; // f() may have added new element, invalidating the pointer
      if (tf->type == TimeFunction::TIMEOUT) {
        this->time_functions_.erase(this->time_functions_.begin() + i);
      } else {
        uint32_t amount = (millis() - tf->last_execution) / tf->interval;
        tf->last_execution += amount * tf->interval;
        i++;
      }
    } else
      i++;
  }

  this->loop();
}

bool Component::cancel_time_function(const std::string &name, TimeFunction::Type type) {
  for (auto iter = this->time_functions_.begin(); iter != this->time_functions_.end(); iter++) {
    if (iter->name == name && iter->type == type) {
      this->time_functions_.erase(iter);
      return true;
    }
  }
  return false;
}
Component::Component() : component_state_(CONSTRUCTION) {

}
void Component::setup_() {
  assert_construction_state(this);
  this->component_state_ = SETUP;
  this->setup();
}
Component::ComponentState Component::get_component_state() const {
  return this->component_state_;
}

} // namespace esphomelib
