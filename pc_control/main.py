import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation
from collections import deque
import time

# Configuration initiale de la connexion série
ser = None

# Tampons circulaires pour stocker les données
buffer_size = 200
setpoints = deque(maxlen=buffer_size)
inputs = deque(maxlen=buffer_size)
times = deque(maxlen=buffer_size)
time_counter = 0  # Compteur pour simuler le temps

# Variable pour ignorer les messages de démarrage
startup_time = time.time()
startup_duration = 2  # Temps en secondes pour ignorer les messages inattendus

# Stockage des paramètres et widgets associés
parameters = {}
parameter_widgets = {}

# Création de l'interface graphique avec tkinter
root = tk.Tk()
root.title("Moniteur Série et Paramètres")

# Fonction pour obtenir les ports série disponibles
def get_serial_ports():
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

# Fonction pour se connecter au port série sélectionné
def connect_serial():
    global ser
    selected_port = port_dropdown.get()
    try:
        ser = serial.Serial(selected_port, 9600, timeout=1)
        ser.dtr = False
        ser.rts = False
        status_label.config(text=f"Connecté à {selected_port}", fg="green")
    except Exception as e:
        status_label.config(text=f"Erreur de connexion: {str(e)}", fg="red")

# Fonction pour se déconnecter du port série
def disconnect_serial():
    global ser
    if ser and ser.is_open:
        ser.dtr = False
        ser.close()
        status_label.config(text="Déconnecté", fg="red")

# Fonction pour quitter proprement l'application
def close_application():
    if ser and ser.is_open:
        ser.close()
    root.destroy()

# Fonction pour mettre à jour les valeurs des paramètres
def update_parameters(param, value):
    global ser
    try:
        # Envoi du paramètre modifié au microcontrôleur via le port série
        if ser and ser.is_open:
            command = f"{param}:{value}\n"
            ser.write(command.encode('utf-8'))
            status_label.config(text=f"Envoyé : {command.strip()}", fg="blue")
    except Exception as e:
        status_label.config(text=f"Erreur d'envoi : {str(e)}", fg="red")

# Initialisation de l'affichage avec matplotlib
fig, ax = plt.subplots(figsize=(10, 5))
line_setpoint, = ax.plot([], [], label="Setpoint", color='r')
line_input, = ax.plot([], [], label="Input", color='g')
ax.set_title("Setpoint et Input")
ax.set_xlabel("Temps (échantillons)")
ax.set_ylabel("Valeur")
ax.legend()

# Fonction pour mettre à jour le graphique et les paramètres
def update_plot(frame):
    global startup_time, time_counter, parameters, parameter_widgets
    if ser and ser.is_open:
        try:
            # Ignorer les messages pendant les premières secondes
            if time.time() - startup_time < startup_duration:
                ser.readline()  # Lire et ignorer
                return

            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if "Setpoint" in line and "Input" in line:
                # Analyser les données dans le format clé-valeur
                parts = line.split(",")
                data = {}
                for part in parts:
                    if ":" in part:
                        key, value = part.split(":")
                        key = key.strip()
                        value = value.strip()
                        if key not in ["Output", "Input"]:
                            data[key] = value

                # Mettre à jour les widgets pour chaque paramètre
                for key, value in data.items():
                    if key not in parameter_widgets:
                        # Créer les widgets pour les nouveaux paramètres
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

                        # Associer un événement pour l'envoi de la nouvelle valeur
                        input_entry.bind("<Return>", lambda event, k=key, e=input_entry: update_parameters(k, e.get()))

                        parameter_widgets[key] = {
                            "readonly": readonly_var,
                            "input": input_entry,
                        }
                    else:
                        # Mettre à jour les widgets existants
                        parameter_widgets[key]["readonly"].set(value)

                # Ajouter les données au graphique
                setpoint = float(data.get("Setpoint", 0))
                input_value = float(parts[1].split(":")[1].strip())  # Deuxième valeur correspond à Input
                time_counter += 1

                setpoints.append(setpoint)
                inputs.append(input_value)
                times.append(time_counter)

            # Ajuster les limites des axes dynamiquement
            if times:
                ax.set_xlim(times[0], times[-1])  # Gérer l'axe X
            if setpoints or inputs:
                min_y = min(min(setpoints, default=0), min(inputs, default=0))
                max_y = max(max(setpoints, default=0), max(inputs, default=0))
                padding = (max_y - min_y) * 0.1  # Ajout d'une marge de 10%
                ax.set_ylim(min_y - padding, max_y + padding)

            # Mise à jour du graphique
            line_setpoint.set_data(times, setpoints)
            line_input.set_data(times, inputs)
            canvas.draw()

        except (UnicodeDecodeError, ValueError) as e:
            status_label.config(text=f"Erreur de lecture : {str(e)}", fg="red")

def update_buffer_size(event=None):
    global buffer_size, setpoints, inputs, times
    try:
        # Lire la nouvelle taille du buffer entrée par l'utilisateur
        new_size = int(buffer_size_entry.get())
        if new_size > 0:  # Vérifier que la valeur est valide
            buffer_size = new_size
            # Réinitialiser les tampons avec la nouvelle taille
            setpoints = deque(setpoints, maxlen=buffer_size)
            inputs = deque(inputs, maxlen=buffer_size)
            times = deque(times, maxlen=buffer_size)
            status_label.config(text=f"Taille du buffer mise à jour : {buffer_size}", fg="blue")
        else:
            raise ValueError  # Lancer une exception si la taille n'est pas positive
    except ValueError:
        status_label.config(text="Entrée invalide pour la taille du buffer", fg="red")


# Fonction pour démarrer l'animation
def start_plotting():
    ani = FuncAnimation(fig, update_plot, interval=200, save_count=buffer_size)
    canvas.draw()

# Interface utilisateur avec tkinter
frame = tk.Frame(root)
frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

port_label = tk.Label(frame, text="Sélectionnez un port série:")
port_label.pack()

ports = get_serial_ports()
port_dropdown = ttk.Combobox(frame, values=ports)
port_dropdown.pack()

# Frame pour les boutons
button_frame = tk.Frame(frame)
button_frame.pack()

refresh_button = tk.Button(button_frame, text="Rafraîchir les ports", command=lambda: port_dropdown.config(values=get_serial_ports()))
refresh_button.pack(side=tk.LEFT)

connect_button = tk.Button(button_frame, text="Connecter", command=connect_serial)
connect_button.pack(side=tk.LEFT)

disconnect_button = tk.Button(button_frame, text="Déconnecter", command=disconnect_serial)
disconnect_button.pack(side=tk.LEFT)

close_button = tk.Button(button_frame, text="Fermer", command=close_application)
close_button.pack(side=tk.LEFT)

# Champ pour régler la taille du buffer
buffer_size_label = tk.Label(frame, text="Taille du buffer (Time Window):")
buffer_size_label.pack()

buffer_size_entry = tk.Entry(frame)
buffer_size_entry.insert(0, str(buffer_size))  # Valeur par défaut
buffer_size_entry.pack()
buffer_size_entry.bind("<Return>", update_buffer_size)  # Mise à jour sur Entrée

status_label = tk.Label(frame, text="Non connecté", fg="red")
status_label.pack()

# Frame pour les paramètres
params_frame = tk.Frame(root)
params_frame.pack(fill=tk.BOTH, expand=True)

# Champ de texte pour afficher les lignes non parsables
output_text = tk.Text(root, height=5, wrap=tk.WORD)
output_text.pack(fill=tk.BOTH, expand=True)

# Ajout du graphique à la fenêtre tkinter
canvas = FigureCanvasTkAgg(fig, master=root)
canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True)

# Lancement de l'animation
start_plotting()

# Lancement de l'interface graphique
root.mainloop()
