import streamlit as st
import paramiko
import matplotlib.pyplot as plt
import time

# Configuración
HOST = "localhost"
PORT = 50022
USER = "pi"
PASSWORD = "1234"
DEVICE = "/dev/signal_dev_TP5"

# Conexión SSH persistente
@st.cache_resource
def get_ssh_connection():
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, port=PORT, username=USER, password=PASSWORD)
    return ssh

def set_signal(ssh, index):
    ssh.exec_command(f"echo {index} > {DEVICE}")

def read_signal(ssh):
    stdin, stdout, stderr = ssh.exec_command(f"cat {DEVICE}")
    value = stdout.read().decode().strip()
    return int(value) if value in ("0", "1") else 0

# UI
st.title("📈 Visualización de Señal desde Raspberry Pi")
signal_index = st.radio("Seleccionar señal:", [0, 1], horizontal=True)
ssh = get_ssh_connection()
set_signal(ssh, signal_index)

# Variables de estado
if "signal_times" not in st.session_state:
    st.session_state.signal_times = []
    st.session_state.signal_values = []
    st.session_state.start_time = time.time()

# Botón para reiniciar gráfico
if st.button("🔄 Reiniciar gráfico"):
    st.session_state.signal_times = []
    st.session_state.signal_values = []
    st.session_state.start_time = time.time()
    set_signal(ssh, signal_index)

# Actualización continua
placeholder = st.empty()
while True:
    elapsed = int(time.time() - st.session_state.start_time)
    val = read_signal(ssh)

    st.session_state.signal_times.append(elapsed)
    st.session_state.signal_values.append(val)

    print(f"[{elapsed:02d}s] Señal {signal_index} = {val}")

    # Graficar
    with placeholder.container():
        st.subheader(f"Señal {signal_index} en tiempo real")
        fig, ax = plt.subplots()
        ax.plot(st.session_state.signal_times, st.session_state.signal_values, marker='o')
        ax.set_xlabel("Tiempo [s]")
        ax.set_ylabel("Valor")
        ax.grid(True)
        st.pyplot(fig)

    time.sleep(5)