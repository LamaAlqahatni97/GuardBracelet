# GuardBracelet
Electronic bracelet connected to a mobile application that will help parents to check on their children all the time in the absence of them.
The bracelet made by a microcontroller called Node, and it consists of a heartbeat sensor that will analyze the child's heart rate and classify 
it to determine his status either the child's heart rate is normal or abnormal. Also, a tracking sensor that will enable the parent to track 
their children's location. Attached the Arduino Code.


Connection:
NodeMCU        GPS NEO-6 module
GND                 GND
3V                  Vcc
D1                  RX
D2                  TX


NodeMCU        Heart Rate Pulse Sensor
GND                   GND
3V                     +
A0                     S
