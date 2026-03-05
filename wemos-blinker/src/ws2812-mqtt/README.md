# Home Assistant Controllable RGB Light

## Wiring:

- 5V -> 5V on board and on LED (RED wire)
- GND -> G on board and on LED (WHITE wire)
- CTRL -> D3 on board (GREEN wire)

## Home assistant config

```yaml
# mqtt.conf
  light:
  - name: "Wemos Matrix Light"
    state_topic: "lights/wemos/status"
    command_topic: "lights/wemos/switch"
    brightness_state_topic: "lights/wemos/brightness/status"
    brightness_command_topic: "lights/wemos/brightness/set"
    rgb_state_topic: "lights/wemos/rgb/status"
    rgb_command_topic: "lights/wemos/rgb/set"
    state_value_template: "{{ value_json.state }}"
    brightness_value_template: "{{ value_json.brightness }}"
    rgb_value_template: "{{ value_json.rgb | join(',') }}"
    qos: 0
    payload_on: "ON"
    payload_off: "OFF"
    optimistic: true
```