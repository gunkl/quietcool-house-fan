# Showing the fan timer's time remaining in Home Assistant

The QuietCool fan runs its own timer. This ESPHome device only **sends** the timer command (or
decodes a timer pressed on the physical remote); it does not count down or report time remaining,
on purpose. If you want a visual countdown, build it in Home Assistant with the recipe below. It
needs no changes to this device, and it keeps working across device firmware updates.

## How it works

| Piece | Role |
|---|---|
| `event.<device>_quietcool_remote` (from this device) | Fires `timer_1h` / `timer_2h` / `timer_4h` / `timer_8h` / `timer_12h` whenever a timer is set: pressed on the physical remote, **or** set from Home Assistant (the fan's timer presets). One listener therefore covers both sources. |
| `fan.<device>_quietcool_whole_house_fan` (from this device) | Going from `on` to `off`, by any means, ends the fan's timer, so the countdown is cancelled. |
| `timer.quietcool_fan_timer` (you create) | A standard HA timer helper. HA's own UI shows it counting down live, and it survives an HA restart (`restore: true`). |
| One automation (you create) | Starts or restarts the HA timer on a `timer_Nh` event; cancels it when the fan turns off. |

### Rules the automation follows

| What happens | Countdown |
|---|---|
| A `timer_Nh` event, from the remote or from HA | Starts at N hours. A new timer replaces a running one. |
| The fan entity goes `on` → `off` | Cancelled. |
| An `off` event from the remote | Cancelled (covers the case where the fan entity was already off in HA). |
| A speed change | No change. The fan keeps its timer through a speed change. |
| The countdown reaches zero | The timer helper goes idle. The fan stops itself at the same time. |
| HA restarts mid-timer | Keeps counting (`restore: true`). |
| The ESPHome device reboots or reconnects | No change. A stale event re-sent on reconnect is ignored (see the 30 s check below). |

## Sample configuration

Save this as a [package](https://www.home-assistant.io/docs/configuration/packages/), e.g.
`packages/quietcool_timer.yaml` with `homeassistant: packages: !include_dir_named packages` in your
`configuration.yaml`. You can also paste each section into the matching place in your own config.

**First, replace the two entity IDs** (marked `# <- your entity`) with yours. Find them under
*Settings → Devices & services → ESPHome → your fan device*. They are usually
`event.<device_name>_quietcool_remote` and `fan.<device_name>_quietcool_whole_house_fan`.

```yaml
timer:
  quietcool_fan_timer:
    name: "QuietCool Fan Timer"
    icon: mdi:timer-outline
    restore: true

automation:
  - id: quietcool_fan_timer_countdown
    alias: "QuietCool fan timer countdown"
    description: >-
      Mirrors the QuietCool fan's own timer into timer.quietcool_fan_timer for display.
      Display only: it never commands the fan.
    mode: queued
    triggers:
      - trigger: state
        entity_id: event.quietcool_esp32_quietcool_remote   # <- your entity
        not_from: ["unavailable", "unknown"]
        id: remote_event
      - trigger: state
        entity_id: fan.quietcool_esp32_quietcool_whole_house_fan   # <- your entity
        from: "on"
        to: "off"
        id: fan_off
    actions:
      - choose:
          # A timer was set (remote or HA): start/restart the countdown.
          - conditions:
              - condition: trigger
                id: remote_event
              - condition: template
                value_template: >-
                  {{ trigger.to_state.attributes.event_type is string
                     and trigger.to_state.attributes.event_type.startswith('timer_')
                     and (now() - as_datetime(trigger.to_state.state)).total_seconds() < 30 }}
            sequence:
              - action: timer.start
                target:
                  entity_id: timer.quietcool_fan_timer
                data:
                  duration:
                    hours: >-
                      {{ trigger.to_state.attributes.event_type
                         | replace('timer_', '') | replace('h', '') | int(0) }}
          # The remote reported Off: the fan's timer is gone.
          - conditions:
              - condition: trigger
                id: remote_event
              - condition: template
                value_template: "{{ trigger.to_state.attributes.event_type == 'off' }}"
            sequence:
              - action: timer.cancel
                target:
                  entity_id: timer.quietcool_fan_timer
          # The fan entity turned off (HA, remote, Climate Advisor, power reading, timer end).
          - conditions:
              - condition: trigger
                id: fan_off
            sequence:
              - action: timer.cancel
                target:
                  entity_id: timer.quietcool_fan_timer

# Optional: "ends at" and "minutes left" sensors, e.g. for a tile card or a notification.
template:
  - sensor:
      - name: "QuietCool Timer Ends"
        unique_id: quietcool_timer_ends
        device_class: timestamp
        state: >-
          {{ state_attr('timer.quietcool_fan_timer', 'finishes_at')
             if is_state('timer.quietcool_fan_timer', 'active') else none }}
      - name: "QuietCool Timer Minutes Left"
        unique_id: quietcool_timer_minutes_left
        unit_of_measurement: "min"
        icon: mdi:timer-sand
        # Templates using now() re-evaluate once a minute.
        state: >-
          {% set ends = state_attr('timer.quietcool_fan_timer', 'finishes_at') %}
          {% if is_state('timer.quietcool_fan_timer', 'active') and ends %}
            {{ [((as_datetime(ends) - now()).total_seconds() / 60) | round(0, 'ceil') | int, 0] | max }}
          {% else %}0{% endif %}
```

### Dashboard card

The timer helper counts down live in the standard cards; nothing else is needed:

```yaml
type: entities
title: Whole house fan
entities:
  - entity: fan.quietcool_esp32_quietcool_whole_house_fan   # <- your entity
  - entity: timer.quietcool_fan_timer
    name: Time remaining
```

## Known limitations

| Limitation | Effect |
|---|---|
| **"No timer" pressed on the remote**, or a timer otherwise cancelled on the fan while it keeps running | No event is sent for it, so the countdown keeps going until zero or until the fan turns off. The fan itself is correct; only the display is stale. |
| **HA was disconnected** from the device when the timer was set | The event was missed, so there is no countdown for that timer. |
| **Small offset** | The countdown starts when HA receives the event, within a second or two of the fan. |
| **Events older than 30 s are ignored** | This prevents a stale event, re-sent after a reconnect or HA restart, from restarting the countdown. |
| **Display only** | The automation never commands the fan and has no effect on Climate Advisor, which reads the same events independently. |
