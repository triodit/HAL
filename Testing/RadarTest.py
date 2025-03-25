import tkinter as tk
from tkinter import ttk
import serial
import threading
import time
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from collections import deque

# ---- Serial Config ----
PORT = 'COM3'
BAUD = 115200

# ---- GUI App ----
class RadarApp:
    def __init__(self, root):
        self.root = root
        self.root.title("HLK-LD1125H Radar Monitor")
        self.ser = serial.Serial(PORT, BAUD, timeout=1)

        # Data Buffers
        self.dist_data = deque(maxlen=100)
        self.time_data = deque(maxlen=100)
        self.start_time = time.time()
        self.status = tk.StringVar(value="No Data")

        # GUI Layout
        self.create_widgets()

        # Serial Thread
        self.running = True
        threading.Thread(target=self.read_serial, daemon=True).start()

        # Update plot regularly
        self.update_plot()

    def create_widgets(self):
        frame_top = ttk.Frame(self.root)
        frame_top.pack(pady=10)

        ttk.Label(frame_top, text="Status:").grid(row=0, column=0)
        ttk.Label(frame_top, textvariable=self.status, foreground="blue").grid(row=0, column=1)

        self.rmax_var = tk.StringVar(value="6")
        self.mth1_var = tk.StringVar(value="60")
        self.mth2_var = tk.StringVar(value="30")
        self.mth3_var = tk.StringVar(value="20")
        self.test_mode_var = tk.StringVar(value="0")

        config_frame = ttk.LabelFrame(self.root, text="Configuration")
        config_frame.pack(pady=10)

        for i, (label, var) in enumerate([
            ("Max Distance (rmax):", self.rmax_var),
            ("Sensitivity mth1 (0–2.8m):", self.mth1_var),
            ("Sensitivity mth2 (2.8–8m):", self.mth2_var),
            ("Sensitivity mth3 (>8m):", self.mth3_var),
            ("Test Mode (0/1):", self.test_mode_var)
        ]):
            ttk.Label(config_frame, text=label).grid(row=i, column=0, sticky="w")
            ttk.Entry(config_frame, textvariable=var, width=10).grid(row=i, column=1)

        ttk.Button(config_frame, text="Apply Settings", command=self.send_settings).grid(row=5, column=0, columnspan=2, pady=5)
        ttk.Button(config_frame, text="Save", command=lambda: self.send_cmd("save")).grid(row=6, column=0)
        ttk.Button(config_frame, text="Get All", command=lambda: self.send_cmd("get_all")).grid(row=6, column=1)

        # Matplotlib Figure
        self.fig, self.ax = plt.subplots()
        self.line, = self.ax.plot([], [], label="Distance (m)")
        self.ax.set_ylim(0, 8)
        self.ax.set_ylabel("Distance (m)")
        self.ax.set_xlabel("Time (s)")
        self.ax.legend()

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.root)
        self.canvas.get_tk_widget().pack()

    def read_serial(self):
        while self.running:
            try:
                line = self.ser.readline().decode().strip()
                if not line:
                    continue
                print("RX:", line)  # Debug log

                current_time = time.time() - self.start_time

                if line.startswith("mov"):
                    self.status.set("MOVING")
                    dis = float(line.split("=")[-1])
                    self.dist_data.append(dis)
                    self.time_data.append(current_time)
                elif line.startswith("occ"):
                    self.status.set("PRESENT")
                    dis = float(line.split("=")[-1])
                    self.dist_data.append(dis)
                    self.time_data.append(current_time)
                elif line.startswith("str"):
                    pass  # Optional: display signal strength
            except Exception as e:
                print("Error:", e)

    def update_plot(self):
        self.line.set_data(self.time_data, self.dist_data)
        self.ax.relim()
        self.ax.autoscale_view()
        self.canvas.draw()
        self.root.after(500, self.update_plot)

    def send_cmd(self, cmd):
        if self.ser.is_open:
            self.ser.write((cmd + "\r\n").encode())

    def send_settings(self):
        self.send_cmd(f"rmax={self.rmax_var.get()}")
        self.send_cmd(f"mth1={self.mth1_var.get()}")
        self.send_cmd(f"mth2={self.mth2_var.get()}")
        self.send_cmd(f"mth3={self.mth3_var.get()}")
        self.send_cmd(f"test_mode={self.test_mode_var.get()}")

    def close(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()

if __name__ == "__main__":
    root = tk.Tk()
    app = RadarApp(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.close(), root.destroy()))
    root.mainloop()
