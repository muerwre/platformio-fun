# Experiments with Wemos Mini D1

## Common problems:

- How to fix Wemos Mini D1 not waking from sleep (aka 10k Resistor way): https://forum.arduino.cc/t/wemos-d1-mini-pro-deepsleep-fail/1394898/44?page=2
- Standart baud rate for all minis is 74880 to read messages at boot time.
- If i2c is not working, do in setup(): `Wire.begin(D2, D1);`