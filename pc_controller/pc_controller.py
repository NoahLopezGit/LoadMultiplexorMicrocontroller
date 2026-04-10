import can


def receive_can_messages(
    serial_port: str = "/dev/ttyUSB0",
    bitrate: int = 500000,
    timeout: float = 1.0,
) -> None:
    """
    Receive CAN messages from an SLCAN adapter.

    Args:
        serial_port: Serial device for the SLCAN adapter.
                     Examples:
                       Linux:   /dev/ttyUSB0
                       macOS:   /dev/tty.usbserial-0001
                       Windows: COM3
        bitrate: CAN bus bitrate in bits/sec.
        timeout: Seconds to wait for a message before printing a timeout notice.
    """
    try:
        # Create an SLCAN bus
        with can.Bus(
            interface="slcan",
            channel=serial_port,
            bitrate=bitrate,
        ) as bus:
            print(
                f"Listening on {serial_port} at {bitrate} bit/s... Press Ctrl+C to stop.")

            while True:
                msg = bus.recv(timeout=timeout)

                if msg is None:
                    print("No message received")
                    continue

                print(
                    f"Timestamp: {msg.timestamp:.6f} | "
                    f"ID: 0x{msg.arbitration_id:X} | "
                    f"Extended: {msg.is_extended_id} | "
                    f"DLC: {msg.dlc} | "
                    f"Data: {msg.data.hex(' ')}"
                )

    except KeyboardInterrupt:
        print("\nStopped by user.")
    except can.CanError as e:
        print(f"CAN error: {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")


if __name__ == "__main__":
    receive_can_messages(
        serial_port="/dev/ttyUSB0",  # Change this for your system
        bitrate=500000,
        timeout=1.0,
    )
