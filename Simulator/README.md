# Pump System Simulator

The simulator creates a virtual serial port on Linux and responds to Monitor's
registration and heartbeat requests. It requires Python 3.8 or later.

Run the simulator from the project root.

```bash
python3 Simulator/simulator.py
```

Copy the printed `/dev/pts/N` path into Monitor's port field and click
**Подключить** (Connect). You can also pass the path to Monitor with `--port`.
Connect manually because automatic discovery scans USB serial ports.

By default, pumps 101, 102, and 103 are online with their limit switches
released. To use different pump IDs, pass them on the command line, for example
`python3 Simulator/simulator.py 105 110`.

| Simulator console command | Action |
| --- | --- |
| `pump 101 offline` / `pump 101 online` | Disconnect pump 101 or bring it back online. |
| `pump 101 forward` / `pump 101 reverse` / `pump 101 clear` | Trigger the forward or reverse limit switch, or release it. |
| `master off` | Stop UART replies. Monitor detects the lost connection when the heartbeat times out. |
| `master on` | Enable Master. After Monitor detects the lost connection, click **Подключить** (Connect) again using the same port path. |
| `status` / `help` | Show device states or list the commands. |
| `quit` | Close the virtual port and exit the simulator. |

The simulator retains the limit switch state while a pump is offline. On
reconnection, Monitor receives the current status of every pump. The simulator
logs incoming rotation commands as UART frames in its console.
