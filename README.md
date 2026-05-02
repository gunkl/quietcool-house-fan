# QuietCool Whole House Attic Fan support in ESPHome and Home Assistant

This project aims to provide a way to add your QuietCool whole house attic fan to Home Assistant via ESP Home. Huge
thank you to Caleb Crome who reverse engineered an older model and released the code on
[his Github](https://github.com/ccrome/quiet-cool-rf-remote). My version of this works on the newer fans that use the
"glass" remote with touchscreen buttons rather than physical ones.

The QC fans require the remote to be paired with the fan so you have two options for how to connect. By default, this
tool will create a random ID based on your ESP32's chip ID. You can put your fan in pairing mode and then send the wake
command from this tool so that it will see your ESP32 as another remote.

Alternatively, you can read the ID from your current remote using the read feature. To do this, put the ESP32 in read
mode and then push any button on your fan's remote (just tapping the remote so that the lights turn on will also send
a wake command so that's usually enough). Sometimes it doesn't read properly so you need to make sure the command was
recognized.
