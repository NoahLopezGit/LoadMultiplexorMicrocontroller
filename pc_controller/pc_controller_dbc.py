#!/usr/bin/env python3
"""
Live CAN DBC viewer.

Loads a .dbc file, listens on a CAN bus using python-can, decodes frames with
cantools, and displays the latest decoded values in a Tkinter GUI.

Example:
    python can_dbc_gui_viewer.py \
        --dbc CAN_schema.dbc \
        --channel can0 \
        --bustype socketcan

Other examples:
    python can_dbc_gui_viewer.py --bustype pcan --channel PCAN_USBBUS1 --bitrate 500000
    python can_dbc_gui_viewer.py --bustype virtual --channel vcan0

Dependencies:
    pip install cantools python-can
"""

from __future__ import annotations

import argparse
import queue
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk
from typing import Any

try:
    import cantools
except ImportError as exc:
    raise SystemExit(
        "Missing dependency: cantools\nInstall with: pip install cantools"
    ) from exc

try:
    import can
except ImportError as exc:
    raise SystemExit(
        "Missing dependency: python-can\nInstall with: pip install python-can"
    ) from exc


class CanReceiver(threading.Thread):
    """Background CAN receive thread."""

    def __init__(
        self,
        bus: can.BusABC,
        database: cantools.database.Database,
        out_queue: queue.Queue[dict[str, Any]],
        stop_event: threading.Event,
    ) -> None:
        super().__init__(daemon=True)
        self.bus = bus
        self.database = database
        self.out_queue = out_queue
        self.stop_event = stop_event

    def run(self) -> None:
        while not self.stop_event.is_set():
            try:
                msg = self.bus.recv(timeout=0.2)
            except Exception as exc:  # pragma: no cover - hardware/runtime dependent
                self.out_queue.put({"type": "error", "error": f"CAN receive error: {exc}"})
                time.sleep(0.5)
                continue

            if msg is None:
                continue

            timestamp = getattr(msg, "timestamp", time.time())
            raw_hex = " ".join(f"{b:02X}" for b in msg.data)

            try:
                dbc_msg = self.database.get_message_by_frame_id(msg.arbitration_id)
                decoded = dbc_msg.decode(msg.data)
                self.out_queue.put(
                    {
                        "type": "decoded",
                        "frame_id": msg.arbitration_id,
                        "message_name": dbc_msg.name,
                        "timestamp": timestamp,
                        "raw_hex": raw_hex,
                        "signals": decoded,
                    }
                )
            except KeyError:
                self.out_queue.put(
                    {
                        "type": "unknown",
                        "frame_id": msg.arbitration_id,
                        "timestamp": timestamp,
                        "raw_hex": raw_hex,
                    }
                )
            except Exception as exc:
                self.out_queue.put(
                    {
                        "type": "decode_error",
                        "frame_id": msg.arbitration_id,
                        "timestamp": timestamp,
                        "raw_hex": raw_hex,
                        "error": str(exc),
                    }
                )


class CanDbcViewer(tk.Tk):
    def __init__(
        self,
        database: cantools.database.Database,
        bus: can.BusABC,
        dbc_path: str,
    ) -> None:
        super().__init__()
        self.title("CAN DBC Viewer")
        self.geometry("1180x760")

        self.database = database
        self.bus = bus
        self.dbc_path = dbc_path

        self.rx_queue: queue.Queue[dict[str, Any]] = queue.Queue()
        self.stop_event = threading.Event()
        self.receiver = CanReceiver(bus, database, self.rx_queue, self.stop_event)

        self.message_rows: dict[str, dict[str, Any]] = {}
        self.signal_rows: dict[tuple[str, str], str] = {}
        self.status_var = tk.StringVar(value="Disconnected")
        self.dbc_var = tk.StringVar(value=f"DBC: {dbc_path}")
        self.stats_var = tk.StringVar(value="Decoded: 0 | Unknown: 0 | Errors: 0")
        self.decoded_count = 0
        self.unknown_count = 0
        self.error_count = 0

        self._build_ui()
        self._populate_expected_messages()

        self.protocol("WM_DELETE_WINDOW", self.on_close)
        self.after(100, self._drain_queue)

    def _build_ui(self) -> None:
        top = ttk.Frame(self, padding=10)
        top.pack(fill=tk.X)

        ttk.Label(top, textvariable=self.dbc_var).pack(side=tk.LEFT)
        ttk.Label(top, textvariable=self.status_var, foreground="green").pack(side=tk.LEFT, padx=(18, 0))
        ttk.Label(top, textvariable=self.stats_var).pack(side=tk.RIGHT)

        center = ttk.Panedwindow(self, orient=tk.VERTICAL)
        center.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))

        upper = ttk.Frame(center)
        lower = ttk.Frame(center)
        center.add(upper, weight=4)
        center.add(lower, weight=2)

        columns = ("frame_id", "message", "timestamp", "raw")
        self.msg_tree = ttk.Treeview(upper, columns=columns, show="tree headings", height=10)
        self.msg_tree.heading("frame_id", text="Frame ID")
        self.msg_tree.heading("message", text="Message")
        self.msg_tree.heading("timestamp", text="Last Seen")
        self.msg_tree.heading("raw", text="Raw Bytes")
        self.msg_tree.column("frame_id", width=110, anchor=tk.CENTER)
        self.msg_tree.column("message", width=240)
        self.msg_tree.column("timestamp", width=170, anchor=tk.CENTER)
        self.msg_tree.column("raw", width=300)
        self.msg_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        msg_scroll = ttk.Scrollbar(upper, orient=tk.VERTICAL, command=self.msg_tree.yview)
        msg_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.msg_tree.configure(yscrollcommand=msg_scroll.set)

        log_header = ttk.Frame(lower)
        log_header.pack(fill=tk.X)
        ttk.Label(log_header, text="Event Log").pack(side=tk.LEFT)
        ttk.Button(log_header, text="Clear Log", command=self._clear_log).pack(side=tk.RIGHT)

        self.log_text = tk.Text(lower, wrap="none", height=12)
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.log_text.configure(state="disabled")

        log_scroll = ttk.Scrollbar(lower, orient=tk.VERTICAL, command=self.log_text.yview)
        log_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.log_text.configure(yscrollcommand=log_scroll.set)

    def _populate_expected_messages(self) -> None:
        for message in sorted(self.database.messages, key=lambda m: m.frame_id):
            parent_id = self.msg_tree.insert(
                "",
                tk.END,
                text="",
                values=(f"0x{message.frame_id:03X}", message.name, "—", "—"),
                open=True,
            )
            self.message_rows[message.name] = {
                "item_id": parent_id,
                "frame_id": message.frame_id,
            }

            for signal in message.signals:
                child_id = self.msg_tree.insert(
                    parent_id,
                    tk.END,
                    text=f"{signal.name}",
                    values=("", "", "", "—"),
                )
                self.signal_rows[(message.name, signal.name)] = child_id

    def start(self) -> None:
        self.status_var.set("Connected")
        self.receiver.start()

    def _append_log(self, line: str) -> None:
        self.log_text.configure(state="normal")
        self.log_text.insert(tk.END, line + "\n")
        self.log_text.see(tk.END)
        self.log_text.configure(state="disabled")

    def _clear_log(self) -> None:
        self.log_text.configure(state="normal")
        self.log_text.delete("1.0", tk.END)
        self.log_text.configure(state="disabled")

    def _update_stats(self) -> None:
        self.stats_var.set(
            f"Decoded: {self.decoded_count} | Unknown: {self.unknown_count} | Errors: {self.error_count}"
        )

    @staticmethod
    def _fmt_timestamp(ts: float) -> str:
        return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(ts))

    @staticmethod
    def _fmt_signal_value(value: Any) -> str:
        if isinstance(value, float):
            return f"{value:.3f}".rstrip("0").rstrip(".")
        return str(value)

    def _drain_queue(self) -> None:
        while True:
            try:
                item = self.rx_queue.get_nowait()
            except queue.Empty:
                break

            item_type = item["type"]
            if item_type == "decoded":
                self.decoded_count += 1
                self._handle_decoded(item)
            elif item_type == "unknown":
                self.unknown_count += 1
                frame_id = item["frame_id"]
                self._append_log(
                    f"[{self._fmt_timestamp(item['timestamp'])}] UNKNOWN 0x{frame_id:03X}  {item['raw_hex']}"
                )
            else:
                self.error_count += 1
                frame_id = item.get("frame_id")
                prefix = f"0x{frame_id:03X}" if frame_id is not None else "GENERAL"
                self._append_log(
                    f"[{self._fmt_timestamp(item.get('timestamp', time.time()))}] ERROR {prefix}: {item['error']}"
                )

            self._update_stats()

        self.after(100, self._drain_queue)

    def _handle_decoded(self, item: dict[str, Any]) -> None:
        msg_name = item["message_name"]
        row = self.message_rows.get(msg_name)
        if row is None:
            return

        self.msg_tree.item(
            row["item_id"],
            values=(
                f"0x{item['frame_id']:03X}",
                msg_name,
                self._fmt_timestamp(item["timestamp"]),
                item["raw_hex"],
            ),
        )

        for signal_name, value in item["signals"].items():
            child_id = self.signal_rows.get((msg_name, signal_name))
            if child_id is None:
                continue
            self.msg_tree.item(
                child_id,
                values=("", signal_name, "", self._fmt_signal_value(value)),
            )

        signal_parts = ", ".join(
            f"{name}={self._fmt_signal_value(value)}"
            for name, value in item["signals"].items()
        )
        self._append_log(
            f"[{self._fmt_timestamp(item['timestamp'])}] {msg_name} (0x{item['frame_id']:03X})  {signal_parts}"
        )

    def on_close(self) -> None:
        self.stop_event.set()
        try:
            self.receiver.join(timeout=1.0)
        except RuntimeError:
            pass
        try:
            self.bus.shutdown()
        except Exception:
            pass
        self.destroy()


def load_database(dbc_path: str) -> cantools.database.Database:
    dbc_file = Path(dbc_path)
    if not dbc_file.exists():
        raise FileNotFoundError(f"DBC file not found: {dbc_file}")
    return cantools.database.load_file(str(dbc_file))


def create_bus(args: argparse.Namespace) -> can.BusABC:
    bus_kwargs: dict[str, Any] = {
        "interface": args.bustype,
        "channel": args.channel,
    }
    if args.bitrate is not None:
        bus_kwargs["bitrate"] = args.bitrate
    return can.Bus(**bus_kwargs)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Decode CAN traffic from a DBC and show it in a GUI.")
    parser.add_argument("--dbc", default="CAN_schema.dbc", help="Path to the DBC file.")
    parser.add_argument("--channel", default="can0", help="CAN channel, e.g. can0, vcan0, or PCAN_USBBUS1.")
    parser.add_argument(
        "--bustype",
        default="socketcan",
        help="python-can interface type, e.g. socketcan, pcan, kvaser, slcan, virtual.",
    )
    parser.add_argument("--bitrate", type=int, default=None, help="CAN bitrate if required by your interface.")
    return parser


def main() -> None:
    args = build_arg_parser().parse_args()

    try:
        database = load_database(args.dbc)
    except Exception as exc:
        raise SystemExit(f"Failed to load DBC: {exc}") from exc

    try:
        bus = create_bus(args)
    except Exception as exc:
        raise SystemExit(f"Failed to open CAN bus: {exc}") from exc

    app = CanDbcViewer(database=database, bus=bus, dbc_path=args.dbc)
    app.start()
    app.mainloop()


if __name__ == "__main__":
    main()
