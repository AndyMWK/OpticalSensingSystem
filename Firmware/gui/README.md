# IR Sensor UART GUI

Basic Python GUI for the `ir_sensor_mock` UART logger.

The mock firmware emits lines like:

```text
PD0 D:1.23 T:45s PD1 D:1.20 T:45s
```

The GUI opens the first available serial port at `9600` baud, shows incoming UART text, and plots the PD0/PD1 distance values.

## Run

```powershell
python -m pip install -r requirements.txt
python ir_sensor_gui.py
```

Use **Refresh** if you connect the board after opening the app.
