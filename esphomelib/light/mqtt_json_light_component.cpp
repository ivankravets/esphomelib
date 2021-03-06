//
// Created by Otto Winter on 28.11.17.
//

#include <esp_log.h>
#include "mqtt_json_light_component.h"

namespace esphomelib {

namespace light {

static const char *TAG = "light::mqtt_json_light";

std::string MQTTJSONLightComponent::component_type() const {
  return "light";
}

void MQTTJSONLightComponent::setup() {
  assert_not_nullptr(this->state_);
  ESP_LOGD(TAG, "Setting up MQTT light...");

  this->send_discovery([&](JsonBuffer &buffer, JsonObject &root) {
    root["brightness"] = this->state_->get_traits().supports_brightness();
    root["rgb"] = this->state_->get_traits().supports_rgb();
    root["flash"] = true;
    root["xy"] = this->state_->get_traits().supports_rgb();
    root["white_value"] = this->state_->get_traits().has_rgb_white_value();
    if (this->state_->supports_effects()) {
      root["effect"] = true;
      JsonArray &effect_list = root.createNestedArray("effect_list");
      for (const LightEffect::Entry &entry : light_effect_entries) {
        if (this->state_->get_traits().supports_traits(entry.requirements))
          continue;
        effect_list.add(buffer.strdup(entry.name.c_str()));
      }
    }
  }, true, true, "mqtt_json");

  this->preferences_.begin(this->get_friendly_name().c_str());
  LightColorValues recovered_values;
  recovered_values.load_from_preferences(&this->preferences_);
  this->state_->set_immediately(recovered_values);

  this->subscribe_json(this->get_command_topic(), [&](JsonObject &root) {
    this->parse_light_json(root);
  });

  this->state_->set_send_callback([&]() {
    this->next_send_ = true;
  });
}

void MQTTJSONLightComponent::parse_light_json(const JsonObject &root) {
  assert_setup(this);
  ESP_LOGV(TAG, "Interpreting light JSON.");
  LightColorValues v = this->state_->get_remote_values(); // use remote values for fallback
  v.parse_json(root);
  v.normalize_color(this->state_->get_traits());
  ESP_LOGV(TAG, "New Color: %s", v.to_string().c_str());

  if (root.containsKey("flash")) {
    auto length = uint32_t(float(root["flash"]) * 1000);
    ESP_LOGD(TAG, "Starting flash with length=%u ms", length);
    this->state_->start_flash(v, length);
  } else if (root.containsKey("transition")) {
    auto length = uint32_t(float(root["transition"]) * 1000);
    ESP_LOGD(TAG, "Starting transition with length=%u ms", length);
    this->state_->start_transition(v, length);
  } else if (root.containsKey("effect")) {
    const char *effect = root["effect"];
    ESP_LOGD(TAG, "Starting effect '%s'", effect);
    this->state_->start_effect(effect);
  } else {
    uint32_t length = this->default_transition_length_;
    ESP_LOGD(TAG, "Starting default transition with length=%u ms", length);
    this->state_->start_transition(v, length);
  }
}

MQTTJSONLightComponent::MQTTJSONLightComponent(std::string friendly_name)
    : MQTTComponent(std::move(friendly_name)), state_(nullptr), default_transition_length_(1000),
      next_send_(true) {

}

void MQTTJSONLightComponent::send_light_values() {
  assert_setup(this);
  LightColorValues remote_values = this->state_->get_remote_values();
  remote_values.save_to_preferences(&this->preferences_);
  this->send_json_message(this->get_state_topic(), [&](JsonBuffer &buffer, JsonObject &root) {
    assert_not_nullptr(this->state_);
    if (this->state_->supports_effects())
      root["effect"] = buffer.strdup(this->state_->get_effect_name().c_str());
    remote_values.dump_json(root, this->state_->get_traits());
  });
}

void MQTTJSONLightComponent::set_state(LightState *state) {
  this->state_ = state;
}

void MQTTJSONLightComponent::set_default_transition_length(uint32_t default_transition_length) {
  this->default_transition_length_ = default_transition_length;
}
uint32_t MQTTJSONLightComponent::get_default_transition_length() const {
  return this->default_transition_length_;
}
LightState *MQTTJSONLightComponent::get_state() const {
  return this->state_;
}
const Preferences &MQTTJSONLightComponent::get_preferences() const {
  return this->preferences_;
}
void MQTTJSONLightComponent::loop() {
  if (this->next_send_) {
    this->next_send_ = false;
    this->send_light_values();
  }
}

} // namespace light

} // namespace esphomelib
