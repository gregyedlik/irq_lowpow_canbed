# Low-power example for Longan CANBed 2040
This an a sample project for low-power interrupt-driven operation on the Longan CANBed 2040, ideal if you need to run from a battery. It uses the Pico SDK.

To minimise power consumption, it uses:
* The low-power sleep functionality of the RP2040 chip, similar to the hello_sleep example in pico-examples.
* The low-power mode of the MCP2515 CAN controller.
* The low-power mode of the SN65HVD230 CAN transceiver.

To be able to use the low-power mode of the SN65HVD230, you need to wire the RS pin to a GPIO. I did it so:
![CANBed](https://github.com/user-attachments/assets/8c059be1-5641-4624-9739-e0473ab6bd9f)

## To run
```
./install.sh
cd build
cmake ..
./gregmake.sh
```
