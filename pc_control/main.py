"""
Serial Monitor and Parameters Interface
Author: Gilles Charvin
Date: 01/12/2024
Version : 1.0

Description:
This program provides a graphical interface to monitor data from a serial connection, visualize it in real-time using Matplotlib, 
and dynamically interact with parameters sent to and received from a microcontroller or other serial devices.
"""

import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation
from collections import deque
import time

# Initial serial connection setup
ser = None

# Circular buffers to store data
buffer_size = 200
setpoints = deque(maxlen=buffer_size)
inputs = deque(maxlen=buffer_size)
times = deque(maxlen=buffer_size)

# Time tracking for real-time plotting
startup_time = time.time()  # Time at which the program started
startup_duration = 2  # Duration to ignore unexpected serial messages

# Storage for parameters and their associated widgets
parameters = {}
parameter_widgets = {}

# Create the GUI window using Tkinter
root = tk.Tk()
root.title("Temperature controller")

def close_application():
    """Close the application and release resources."""
    global ser
    if ser and ser.is_open:
        ser.close()  # Ensure the serial port is closed
    root.quit()  # Quit Tkinter main loop
    root.destroy()  # Destroy the Tkinter window


# Set the protocol to handle window close event
root.protocol("WM_DELETE_WINDOW", close_application)

def get_serial_ports():
    """Retrieve available serial ports on the system."""
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

def connect_serial():
    """Connect to the selected serial port."""
    global ser
    selected_port = port_dropdown.get()
    try:
        ser = serial.Serial(selected_port, 9600, timeout=1)
        ser.dtr = False
        ser.rts = False
        status_label.config(text=f"Connected to {selected_port}", fg="green")
    except Exception as e:
        status_label.config(text=f"Connection error: {str(e)}", fg="red")

def disconnect_serial():
    """Disconnect the serial port if connected."""
    global ser
    if ser and ser.is_open:
        ser.dtr = False
        ser.close()
        status_label.config(text="Disconnected", fg="red")



def update_buffer_size(event=None):
    """Update the size of the data buffer."""
    global buffer_size, setpoints, inputs, times
    try:
        new_size = int(buffer_size_entry.get())  # Get new buffer size from user input
        if new_size > 0:
            buffer_size = new_size
            # Resize the circular buffers
            setpoints = deque(setpoints, maxlen=buffer_size)
            inputs = deque(inputs, maxlen=buffer_size)
            times = deque(times, maxlen=buffer_size)
            status_label.config(text=f"Buffer size updated: {buffer_size}", fg="blue")
        else:
            raise ValueError  # Raise an error if size is invalid
    except ValueError:
        status_label.config(text="Invalid buffer size", fg="red")

def update_parameters(param, value):
    """Send updated parameter values to the microcontroller."""
    global ser
    try:
        if ser and ser.is_open:
            command = f"{param}:{value}\n"
            ser.write(command.encode('utf-8'))
            status_label.config(text=f"Sent: {command.strip()}", fg="blue")
    except Exception as e:
        status_label.config(text=f"Error sending parameter: {str(e)}", fg="red")

# Initialize Matplotlib for data visualization
fig, ax = plt.subplots(figsize=(10, 5))
line_setpoint, = ax.plot([], [], label="Setpoint", color='r')
line_input, = ax.plot([], [], label="Input", color='g')
ax.set_title("Setpoint and Input")
ax.set_xlabel("Time (seconds)")
ax.set_ylabel("Value")
ax.legend()

def send_parameter(param_name, value):
    """Send a specific parameter to the device."""
    global ser
    try:
        command = f"{param_name}:{value}\n"
        if ser and ser.is_open:
            ser.write(command.encode('utf-8'))
            status_label.config(text=f"Parameter sent: {command.strip()}", fg="blue")
        else:
            status_label.config(text="Error: Serial port not connected", fg="red")
    except Exception as e:
        status_label.config(text=f"Error sending parameter: {str(e)}", fg="red")

def update_plot(frame):
    """Fetch and plot data in real-time."""
    global startup_time, parameters, parameter_widgets
    if ser and ser.is_open:
        try:
            if time.time() - startup_time < startup_duration:  # Ignore early messages
                ser.readline()
                return

            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if "Setpoint" in line and "Input" in line:
                parts = line.split(",")
                data = {}
                for part in parts:
                    if ":" in part:
                        key, value = part.split(":")
                        data[key.strip()] = value.strip()

                # Update widgets for each parameter
                for key, value in data.items():
                    if key not in parameter_widgets:
                        frame = tk.Frame(params_frame)
                        frame.pack(fill=tk.X)

                        label = tk.Label(frame, text=key, width=15, anchor="w")
                        label.pack(side=tk.LEFT)

                        readonly_entry = tk.Entry(frame, state="readonly", width=10)
                        readonly_entry.pack(side=tk.LEFT, padx=5)
                        readonly_var = tk.StringVar(value=value)
                        readonly_entry.config(textvariable=readonly_var)

                        input_entry = tk.Entry(frame, width=10)
                        input_entry.pack(side=tk.LEFT, padx=5)
                        input_entry.bind("<Return>", lambda event, k=key, e=input_entry: send_parameter(k, e.get()))

                        parameter_widgets[key] = {
                            "readonly": readonly_var,
                            "input": input_entry,
                        }
                    else:
                        parameter_widgets[key]["readonly"].set(value)

                setpoint = float(data.get("Setpoint", 0))
                input_value = float(parts[1].split(":")[1].strip())
                current_time = time.time() - startup_time  # Elapsed time in seconds

                setpoints.append(setpoint)
                inputs.append(input_value)
                times.append(current_time)

                # Update plot data
                line_setpoint.set_data(times, setpoints)
                line_input.set_data(times, inputs)

                # Adjust axis limits dynamically
                ax.set_xlim(times[0], times[-1])
                if setpoints or inputs:
                    min_y = min(min(setpoints, default=0), min(inputs, default=0))
                    max_y = max(max(setpoints, default=0), max(inputs, default=0))
                    padding = (max_y - min_y) * 0.1
                    ax.set_ylim(min_y - padding, max_y + padding)

                canvas.draw()
            else:
                output_text.insert(tk.END, f"Unexpected line: {line}\n")
        except (UnicodeDecodeError, ValueError) as e:
            status_label.config(text=f"Error reading data: {str(e)}", fg="red")

def start_plotting():
    """Start the animation for real-time data visualization."""
    ani = FuncAnimation(fig, update_plot, interval=200, save_count=buffer_size)
    canvas.draw()

# Create GUI layout for user interaction
frame = tk.Frame(root)
frame.pack(side=tk.TOP, fill=tk.X)

port_label = tk.Label(frame, text="Serial Port:")
port_label.pack(side=tk.LEFT, padx=5)

ports = get_serial_ports()
port_dropdown = ttk.Combobox(frame, values=ports, width=15)
port_dropdown.pack(side=tk.LEFT, padx=5)

refresh_button = tk.Button(frame, text="Refresh Ports", command=lambda: port_dropdown.config(values=get_serial_ports()))
refresh_button.pack(side=tk.LEFT, padx=5)

connect_button = tk.Button(frame, text="Connect", command=connect_serial)
connect_button.pack(side=tk.LEFT, padx=5)

disconnect_button = tk.Button(frame, text="Disconnect", command=disconnect_serial)
disconnect_button.pack(side=tk.LEFT, padx=5)

close_button = tk.Button(frame, text="Close", command=close_application)
close_button.pack(side=tk.LEFT, padx=5)

buffer_size_label = tk.Label(frame, text="Buffer Size:")
buffer_size_label.pack(side=tk.LEFT, padx=5)

buffer_size_entry = tk.Entry(frame, width=5)
buffer_size_entry.insert(0, str(buffer_size))  # Default value
buffer_size_entry.pack(side=tk.LEFT, padx=5)
buffer_size_entry.bind("<Return>", update_buffer_size)

status_label = tk.Label(frame, text="Not connected", fg="red")
status_label.pack(side=tk.LEFT, padx=10)

params_frame = tk.Frame(root)
params_frame.pack(fill=tk.BOTH, expand=True)

output_text = tk.Text(root, height=5, wrap=tk.WORD)
output_text.pack(fill=tk.BOTH, expand=True)

canvas = FigureCanvasTkAgg(fig, master=root)
canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True)

start_plotting()

root.mainloop()
