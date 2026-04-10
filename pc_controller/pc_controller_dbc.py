import can
import cantools

# Load your DBC file
db = cantools.database.load_file("DeviceStatus.dbc")

# Setup CAN bus (SLCAN)
bus = can.Bus(
    interface="slcan",
    # channel="/dev/ttyUSB0", # for linux
    channel="vcan0", # for windows
    bitrate=500000
)

print("Listening and decoding...")

while True:
    msg = bus.recv(timeout=1.0)

    if msg is None:
        continue

    try:
        # Decode message using DBC
        decoded = db.decode_message(msg.arbitration_id, msg.data)

        print(f"Message ID: 0x{msg.arbitration_id:X}")
        for signal, value in decoded.items():
            print(f"  {signal}: {value}")

    except KeyError:
        # Message not defined in DBC
        print(f"Unknown ID: 0x{msg.arbitration_id:X} Data: {msg.data.hex()}")