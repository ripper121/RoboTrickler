# RoboTrickler
 
If you like it, spend me a coffee :)
https://www.paypal.com/paypalme/ripper121


First off all, format the SD Card to FAT32!
Otherwise it will not be recognised by the RoboTrickler.

You can update the Firmware when you place a "update.bin" on the SD Card.


config.txt
```json
{
   "wifi":{       Wifi is only for Uploading new Firmware via HTTP or Arduino
      "ssid":"",		Your Wifi SSID only 2.4GHz, leave this empty to disable Wifi
      "psk":"" 			Your Wifi Password, leave this empty to disable Wifi
   },
   "scale":{
      "protocol":"SBI",		Your Scale Protocol. For now "SBI" (Satorius) and "GUG" (G&G) are supported
      "baud":9600		Baud Speed of your Scale
   },
   "mode":"trickler",		Software Mode. "tirckler" is for auto trickling. "logger" is for logging to csv file
   "log_measurements":10,	This is for the logger, how many measurements it will need bevor something is saved to csv
   "powder":"N135",		"POWDER-FILE-NAME" this is your Powder config file. Name them "POWDERNAME.txt"
   "beeper":"done"		This is when the buzzer should beep. "done" when something is done, "button" on button press, "both" for both
}
```

Use the TEST.txt to check how much Powder is trickled out with one Rotation (360 Steps).
Slower speed mostly means more powder with one rotation.
You can then calculate how much powder was trickled with one rotation. And apply this to your POWDER.txt
For example your first Powder step is 1 gramm and the trickler, trickles 0.250mg of powder with one rotation.
You will need 4 Rotations -> 4 x 360 Steps = 1440 Steps. But you want to be save and use only 3*360 Steps = 1080.
This will prevent an overtrickle. This is especially important when you reach the target weight.
You can also change the Speed of the motor in each trickle step.


POWDER.txt
```json
{
   "1":{			Trickler Step Number
      "weight":1.0,		Weight at which this applys
      "steps":1440,		Motor steps to trickle in degree (360 is one rotation)
      "speed":200,		Stepper Speed in RPM 10-300 
      "measurements":2		Measurments to do bevor next trickle action
   },
   "2":{
      "weight":0.5,
      "steps":720,
      "speed":200,
      "measurements":2
   },
   "3":{
      "weight":0.25,
      "steps":360,
      "speed":200,
      "measurements":10
   },
   .
   .
   .
   "N":{
      "weight":0.25,
      "steps":360,
      "speed":200,
      "measurements":20
   }
}
```

Youtube:

[![youtube video](https://img.youtube.com/vi/XJgkiJFMUQI/0.jpg)](https://www.youtube.com/watch?v=XJgkiJFMUQI)
