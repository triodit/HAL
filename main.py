import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import messagebox
import threading
import time

class RadarGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Radar Sensor GUI")

        self.serial_conn = None  
        self.connection_status = tk.StringVar(value="Status: Disconnected")
        self.motion_status = tk.StringVar(value="Unknown")
        self.distance_value = tk.StringVar(value="Distance: -- cm")
        self.signal_strength_value = tk.StringVar(value="Signal Strength: --%")

        # COM Port Selection
        self.com_label = tk.Label(root, text="Select COM Port:")
        self.com_label.pack()
        
        self.com_var = tk.StringVar(root)
        self.com_dropdown = tk.OptionMenu(root, self.com_var, *self.get_com_ports())
        self.com_dropdown.pack()

        self.connect_button = tk.Button(root, text="Connect", command=self.toggle_connection)
        self.connect_button.pack()

        self.status_label = tk.Label(root, textvariable=self.connection_status, fg="red")
        self.status_label.pack()

        self.motion_label = tk.Label(root, textvariable=self.motion_status, font=("Arial", 14), fg="black")
        self.motion_label.pack()

        self.distance_label = tk.Label(root, textvariable=self.distance_value, font=("Arial", 12))
        self.distance_label.pack()

        self.signal_strength_label = tk.Label(root, textvariable=self.signal_strength_value, font=("Arial", 12))
        self.signal_strength_label.pack()

        # Sensitivity Adjustment
        self.sensitivity_label = tk.Label(root, text="Sensitivity (0-9):")
        self.sensitivity_label.pack()
        self.sensitivity_var = tk.IntVar(value=5)
        self.sensitivity_slider = tk.Scale(root, from_=0, to=9, orient="horizontal", variable=self.sensitivity_var)
        self.sensitivity_slider.pack()
        self.set_sensitivity_button = tk.Button(root, text="Set Sensitivity", command=self.set_sensitivity)
        self.set_sensitivity_button.pack()

        # Distance Adjustment
        self.distance_label_text = tk.Label(root, text="Detection Distance (cm, max 500):")
        self.distance_label_text.pack()
        self.distance_var = tk.IntVar(value=300)
        self.distance_slider = tk.Scale(root, from_=10, to=500, orient="horizontal", variable=self.distance_var)
        self.distance_slider.pack()
        self.set_distance_button = tk.Button(root, text="Set Distance", command=self.set_distance)
        self.set_distance_button.pack()

        # Disconnect Button
        self.disconnect_button = tk.Button(root, text="Disconnect", command=self.disconnect, state=tk.DISABLED)
        self.disconnect_button.pack()

        self.ping_thread = threading.Thread(target=self.ping_arduino, daemon=True)
        self.ping_thread.start()

        self.root.after(100, self.read_serial)

    def get_com_ports(self):
        return [port.device for port in serial.tools.list_ports.comports()]

    def toggle_connection(self):
        """Connect to selected COM port"""
        selected_port = self.com_var.get()
        try:
            self.serial_conn = serial.Serial(selected_port, 115200, timeout=1)
            self.connection_status.set(f"Connected to {selected_port}")
            self.connect_button.config(state=tk.DISABLED)
            self.disconnect_button.config(state=tk.NORMAL)
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))

    def disconnect(self):
        """Disconnect from the serial port"""
        if self.serial_conn:
            self.serial_conn.close()
            self.serial_conn = None
            self.connection_status.set("Status: Disconnected")
            self.connect_button.config(state=tk.NORMAL)
            self.disconnect_button.config(state=tk.DISABLED)

    def read_serial(self):
        """Read and process data from the serial port"""
        if self.serial_conn and self.serial_conn.in_waiting:
            try:
                line = self.serial_conn.readline().decode("utf-8").strip()
                print(f"Received: {line}")  # Debugging output

                # Try to parse all messages
                parts = line.split(",")
                if len(parts) >= 2:
                    command = parts[0]

                    if command == "PONG":
                        self.connection_status.set("Connected (Arduino Responding)")
                    elif command == "ACK_SEN":
                        self.sensitivity_var.set(int(parts[1]))
                    elif command == "ACK_DIS":
                        self.distance_var.set(int(parts[1]))
                    elif command == "RADAR":
                        # Try to read motion, presence, distance, and strength
                        try:
                            motion = int(parts[1]) if len(parts) > 1 else 0
                            presence = int(parts[2]) if len(parts) > 2 else 0
                            distance = int(parts[3]) if len(parts) > 3 else 0
                            strength = int(parts[4]) if len(parts) > 4 else 0

                            # Update GUI
                            self.motion_status.set(f"Motion: {motion}, Presence: {presence}")
                            self.distance_value.set(f"Distance: {distance} cm")
                            self.signal_strength_value.set(f"Signal Strength: {strength}%")

                        except ValueError:
                            print(f"⚠️ Error parsing RADAR data: {line} (Skipping)")
                    
            except Exception as e:
                print(f"⚠️ Serial Read Error: {e}")

        self.root.after(100, self.read_serial)

    def set_sensitivity(self):
        """Send a sensitivity update command to the Arduino"""
        try:
            if self.serial_conn:
                self.serial_conn.write(f"SET_SEN,{self.sensitivity_var.get()}\n".encode())
        except Exception as e:
            print(f"⚠️ Error sending sensitivity command: {e}")

    def set_distance(self):
        """Send a distance update command to the Arduino"""
        try:
            if self.serial_conn:
                self.serial_conn.write(f"SET_DIS,{self.distance_var.get()}\n".encode())
        except Exception as e:
            print(f"⚠️ Error sending distance command: {e}")

    def ping_arduino(self):
        """Periodically ping the Arduino to check connection"""
        while True:
            try:
                if self.serial_conn:
                    self.serial_conn.write("PING\n".encode())
            except Exception as e:
                print(f"⚠️ Error sending PING command: {e}")
            time.sleep(5)

if __name__ == "__main__":
    root = tk.Tk()
    app = RadarGUI(root)
    root.mainloop()
