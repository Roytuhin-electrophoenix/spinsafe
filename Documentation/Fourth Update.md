- First the MPU6050 gyro sensor and DS18B2O are tasted together 
– The temperature sensor was heated keeping it close to a soldering iron 
- Test program was run into ESP32 while the sensor were connected
- The output was shown in serial monitor and serial plotter 
- From the serial plotter output it was evident that the temperature sensor responded to the heat and vibration sensor responded to the movements
- We had to do a troubleshooting. The Temperature sensor requires a pullup resistor (here 4.7kohms), which we forgot to connect within the data pin D4 & Vcc (3v3). As a result the temperature sensor was showing errorenous data (-127). After finding out this was immidiately resolved. 
- [Click here for demonstration](https://drive.google.com/drive/folders/1BR87xpz0EPwcEIekhED_AfU3oKXF51cQ?usp=drive_link)
