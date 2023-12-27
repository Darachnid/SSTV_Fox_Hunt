
## SSTV Wildlife Camera
This project uses an ESP32 microcontroller and a modified camera module to create an SSTV (Slow Scan Television) camera trap for capturing images of urban coyotes. The camera is equipped for night vision by removing the IR filter, and it interfaces with a Baofeng UV-5R for image transmission.

### Features
- PIR sensors, external to the camera's housing are connected via hookup wire to the main device.
- When the PIR sensors are triggered, indicating an approaching animal, the camera device powers on as well as a distance meter.
- When the distance meter detects an approaching animal within 1 meter, it take an image.
- If the animal is moving away from the camera, the sensor will not trigger to avoid taking photos of coyote bums.
- Future versions could include two units for taking images on both sides. 
