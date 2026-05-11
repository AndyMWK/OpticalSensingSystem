from __future__ import annotations

import queue
import re
import threading
import time
import tkinter as tk
from collections import deque
from dataclasses import dataclass
from tkinter import messagebox, ttk

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - shown in the GUI at runtime
    serial = None
    list_ports = None


BAUD_RATE = 9600
MAX_POINTS = 120
READ_TIMEOUT_S = 0.2

SAMPLE_RE = re.compile(
    r"PD(?P<pd0_id>\d+)\s+D:(?P<pd0_distance>\d+(?:\.\d+)?)\s+T:(?P<pd0_time>\d+)s\s+"
    r"PD(?P<pd1_id>\d+)\s+D:(?P<pd1_distance>\d+(?:\.\d+)?)\s+T:(?P<pd1_time>\d+)s",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class IrSample:
    received_at: float
    pd0_distance_m: float
    pd1_distance_m: float
    pd0_timestamp_s: int
    pd1_timestamp_s: int
    raw_line: str


class SerialReader:
    def __init__(self, rx_queue: queue.Queue[tuple[str, object]]) -> None:
        self._rx_queue = rx_queue
        self._serial = None
        self._thread: threading.Thread | None = None
        self._stop_event = threading.Event()

    @property
    def is_running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def start(self, port_name: str, baud_rate: int = BAUD_RATE) -> None:
        if serial is None:
            raise RuntimeError("pyserial is not installed. Run: python -m pip install -r requirements.txt")

        self.stop()
        self._stop_event.clear()
        self._serial = serial.Serial(port_name, baud_rate, timeout=READ_TIMEOUT_S)
        self._thread = threading.Thread(target=self._read_loop, name="serial-reader", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=1.0)
        self._thread = None

        if self._serial is not None:
            try:
                self._serial.close()
            finally:
                self._serial = None

    def _read_loop(self) -> None:
        while not self._stop_event.is_set():
            try:
                if self._serial is None:
                    break
                line = self._serial.readline()
            except Exception as exc:
                self._rx_queue.put(("error", str(exc)))
                break

            if not line:
                continue

            text = line.decode("utf-8", errors="replace").strip()
            if not text:
                continue

            self._rx_queue.put(("line", text))


class IrSensorGui(tk.Tk):
    def __init__(self) -> None:
        super().__init__()

        self.title("IR Sensor UART Monitor")
        self.geometry("980x680")
        self.minsize(820, 560)

        self.rx_queue: queue.Queue[tuple[str, object]] = queue.Queue()
        self.reader = SerialReader(self.rx_queue)

        self.sample_index = 0
        self.x_data: deque[int] = deque(maxlen=MAX_POINTS)
        self.pd0_data: deque[float] = deque(maxlen=MAX_POINTS)
        self.pd1_data: deque[float] = deque(maxlen=MAX_POINTS)

        self.status_var = tk.StringVar(value="Disconnected")
        self.port_var = tk.StringVar()
        self.latest_pd0_var = tk.StringVar(value="--")
        self.latest_pd1_var = tk.StringVar(value="--")
        self.latest_time_var = tk.StringVar(value="--")

        self._build_layout()
        self.refresh_ports()
        self.after(100, self._poll_serial_queue)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

        if serial is None:
            self._set_status("pyserial missing")
            messagebox.showwarning(
                "Missing dependency",
                "pyserial is required for serial ports.\nRun: python -m pip install -r requirements.txt",
            )
        else:
            self.after(250, self.connect_first_available)

    def _build_layout(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)

        controls = ttk.Frame(self, padding=(12, 10))
        controls.grid(row=0, column=0, sticky="ew")
        controls.columnconfigure(1, weight=1)

        ttk.Label(controls, text="Port").grid(row=0, column=0, padx=(0, 6), sticky="w")
        self.port_combo = ttk.Combobox(controls, textvariable=self.port_var, state="readonly", width=36)
        self.port_combo.grid(row=0, column=1, sticky="ew")

        ttk.Button(controls, text="Refresh", command=self.refresh_ports).grid(row=0, column=2, padx=(8, 0))
        ttk.Button(controls, text="Connect", command=self.connect_selected).grid(row=0, column=3, padx=(8, 0))
        ttk.Button(controls, text="Disconnect", command=self.disconnect).grid(row=0, column=4, padx=(8, 0))
        ttk.Label(controls, textvariable=self.status_var).grid(row=0, column=5, padx=(14, 0), sticky="e")

        main = ttk.Frame(self, padding=(12, 0, 12, 12))
        main.grid(row=1, column=0, sticky="nsew")
        main.columnconfigure(0, weight=3)
        main.columnconfigure(1, weight=1)
        main.rowconfigure(0, weight=1)

        plot_frame = ttk.Frame(main)
        plot_frame.grid(row=0, column=0, sticky="nsew", padx=(0, 12))
        plot_frame.rowconfigure(0, weight=1)
        plot_frame.columnconfigure(0, weight=1)

        self.figure = Figure(figsize=(7.0, 4.8), dpi=100)
        self.ax = self.figure.add_subplot(111)
        self.ax.set_title("IR Sensor Distance")
        self.ax.set_xlabel("Sample")
        self.ax.set_ylabel("Distance (m)")
        self.ax.grid(True, alpha=0.3)
        (self.pd0_line,) = self.ax.plot([], [], label="PD0", color="#1f77b4", linewidth=2)
        (self.pd1_line,) = self.ax.plot([], [], label="PD1", color="#d62728", linewidth=2)
        self.ax.legend(loc="upper right")

        self.canvas = FigureCanvasTkAgg(self.figure, master=plot_frame)
        self.canvas.get_tk_widget().grid(row=0, column=0, sticky="nsew")

        side = ttk.Frame(main)
        side.grid(row=0, column=1, sticky="nsew")
        side.columnconfigure(0, weight=1)
        side.rowconfigure(4, weight=1)

        ttk.Label(side, text="Latest Readings", font=("Segoe UI", 12, "bold")).grid(row=0, column=0, sticky="w")
        self._reading_row(side, 1, "PD0 distance", self.latest_pd0_var)
        self._reading_row(side, 2, "PD1 distance", self.latest_pd1_var)
        self._reading_row(side, 3, "Firmware time", self.latest_time_var)

        ttk.Label(side, text="UART Data", font=("Segoe UI", 12, "bold")).grid(row=4, column=0, sticky="sw", pady=(18, 4))
        self.raw_text = tk.Text(side, height=14, wrap="none", state="disabled")
        self.raw_text.grid(row=5, column=0, sticky="nsew")

    def _reading_row(self, parent: ttk.Frame, row: int, label: str, variable: tk.StringVar) -> None:
        frame = ttk.Frame(parent, padding=(0, 8, 0, 0))
        frame.grid(row=row, column=0, sticky="ew")
        frame.columnconfigure(1, weight=1)
        ttk.Label(frame, text=label).grid(row=0, column=0, sticky="w")
        ttk.Label(frame, textvariable=variable, font=("Segoe UI", 14, "bold")).grid(row=0, column=1, sticky="e")

    def refresh_ports(self) -> None:
        ports = self._available_ports()
        self.port_combo["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])
        elif not ports:
            self.port_var.set("")
        self._set_status("Disconnected" if not self.reader.is_running else f"Connected: {self.port_var.get()}")

    def connect_first_available(self) -> None:
        if self.reader.is_running:
            return
        self.refresh_ports()
        if self.port_var.get():
            self.connect_selected()

    def connect_selected(self) -> None:
        port_name = self.port_var.get()
        if not port_name:
            self._set_status("No serial ports found")
            return

        try:
            self.reader.start(port_name, BAUD_RATE)
        except Exception as exc:
            self._set_status("Connect failed")
            messagebox.showerror("Serial connection failed", str(exc))
            return

        self._set_status(f"Connected: {port_name} @ {BAUD_RATE}")

    def disconnect(self) -> None:
        self.reader.stop()
        self._set_status("Disconnected")

    def _available_ports(self) -> list[str]:
        if list_ports is None:
            return []
        return [port.device for port in list_ports.comports()]

    def _poll_serial_queue(self) -> None:
        try:
            while True:
                kind, payload = self.rx_queue.get_nowait()
                if kind == "line":
                    self._handle_line(str(payload))
                elif kind == "error":
                    self.disconnect()
                    self._set_status(f"Serial error: {payload}")
        except queue.Empty:
            pass

        self.after(100, self._poll_serial_queue)

    def _handle_line(self, line: str) -> None:
        self._append_raw_line(line)

        sample = self._parse_sample(line)
        if sample is None:
            return

        self.sample_index += 1
        self.x_data.append(self.sample_index)
        self.pd0_data.append(sample.pd0_distance_m)
        self.pd1_data.append(sample.pd1_distance_m)

        self.latest_pd0_var.set(f"{sample.pd0_distance_m:.2f} m")
        self.latest_pd1_var.set(f"{sample.pd1_distance_m:.2f} m")
        self.latest_time_var.set(f"PD0 {sample.pd0_timestamp_s}s / PD1 {sample.pd1_timestamp_s}s")
        self._redraw_plot()

    def _parse_sample(self, line: str) -> IrSample | None:
        match = SAMPLE_RE.search(line)
        if match is None:
            return None

        return IrSample(
            received_at=time.time(),
            pd0_distance_m=float(match.group("pd0_distance")),
            pd1_distance_m=float(match.group("pd1_distance")),
            pd0_timestamp_s=int(match.group("pd0_time")),
            pd1_timestamp_s=int(match.group("pd1_time")),
            raw_line=line,
        )

    def _redraw_plot(self) -> None:
        self.pd0_line.set_data(self.x_data, self.pd0_data)
        self.pd1_line.set_data(self.x_data, self.pd1_data)

        if self.x_data:
            self.ax.set_xlim(max(1, self.x_data[0]), max(MAX_POINTS, self.x_data[-1]))
        if self.pd0_data or self.pd1_data:
            y_values = list(self.pd0_data) + list(self.pd1_data)
            y_min = min(y_values)
            y_max = max(y_values)
            padding = max((y_max - y_min) * 0.15, 0.05)
            self.ax.set_ylim(max(0.0, y_min - padding), y_max + padding)

        self.canvas.draw_idle()

    def _append_raw_line(self, line: str) -> None:
        self.raw_text.configure(state="normal")
        self.raw_text.insert("end", line + "\n")
        self.raw_text.see("end")
        self.raw_text.configure(state="disabled")

    def _set_status(self, text: str) -> None:
        self.status_var.set(text)

    def _on_close(self) -> None:
        self.reader.stop()
        self.destroy()


if __name__ == "__main__":
    app = IrSensorGui()
    app.mainloop()
