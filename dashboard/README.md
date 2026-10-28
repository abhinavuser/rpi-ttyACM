# tdr edge-ai diagnostic dashboard

real-time web dashboard visualizing time-domain reflectometry (tdr) telemetry and edge fault classification from the stm32f303k8 nucleo board.

## architecture

```text
[ stm32f303k8 ] -> uart 115200 8N1 -> [ raspberry pi / pc ]
                                              |
                                      (python app.py)
                                              |
                                  -------------------------
                                  |                       |
                           [ 1d-cnn edge ai ]      [ rest api / sse ]
                              classifier              port 5000
                                  |                       |
                                  -------------------------
                                              |
                                              v
                                      [ browser web ui ]
                                      (mobile or desktop)
```

## features

- real-time tdr waveform: 60 fps html5 canvas rendering showing pulse and reflected echo delay based on measured distance.
- edge-ai classification: predicts line condition with soft probabilities (healthy line, open circuit, short circuit, tapping anomaly) and inference latency.
- live telemetry cards: real-time voltage, physical fault distance (0.0 to 16.0 cm), and time of flight (0.0 to 1.6 ns).
- demo mode: includes a toggle to test and demonstrate all fault modes without needing physical hardware connected.

## how to run

### on linux / raspberry pi:

```bash
cd dashboard
chmod +x start.sh
./start.sh
```

### on windows:

```powershell
cd dashboard
pip install pyserial
python app.py
```

open your web browser at:

```text
http://localhost:5000
```

(or `http://<raspberry-pi-ip>:5000` from any device on your local network).
