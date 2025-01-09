import socket
import threading
import tkinter as tk
from tkinter import scrolledtext
from tkinter import simpledialog
from PIL import Image, ImageTk
import os
import re
import sys

def restart_application():
    """Restartuje aplikację."""
    python = sys.executable  # Ścieżka do interpretera Pythona
    os.execl(python, python, *sys.argv)  # Uruchamia ponownie bieżący skrypt

def update_image(image_label, lives, image_folder):
    """Aktualizuje obraz w GUI w zależności od liczby żyć."""
    image_path = os.path.join(image_folder, f"{lives}.png")
    if os.path.exists(image_path):
        img = Image.open(image_path)
        img = img.resize((200, 200))  # Dostosowanie rozmiaru obrazka
        photo = ImageTk.PhotoImage(img)
        image_label.config(image=photo)
        image_label.image = photo
    else:
        print(f"Image {image_path} not found!")

def update_text_area(label, new_message):
    updated_text = new_message[1:]
    label.config(text=updated_text)

def parse_lives_from_message(message):
    """Wyciąga liczbę żyć z wiadomości serwera."""
    match = re.search(r"You have (\d+) lives left", message)
    if match:
        return int(match.group(1))
    return None

def add_spacing(text):
    return " ".join(text)

def send_message(sock, message_entry):
    """Funkcja obsługująca wysyłanie wiadomości do serwera."""
    message = message_entry.get()
    if message:
        sock.sendall(message.encode('utf-8'))
        message_entry.delete(0, tk.END)

def send_letter(sock, button):
    """Wysyła wybraną literę do serwera i zmienia kolor przycisku."""
    letter = button['text']
    sock.sendall(letter.encode('utf-8'))
    button.config(state=tk.DISABLED) 

def send_join(sock, join_button):
    """Funkcja wysyłająca komendę JOIN."""
    sock.sendall("join".encode('utf-8'))
    join_button.config(state=tk.DISABLED)  # Dezaktywuj przycisk po kliknięciu

def receive_messages(sock, text_area, player_list_area, join_button, image_label, image_folder, pass_label,message_entry,player_name_area,time_area,keyboard_frame):
    """Funkcja obsługująca odbieranie wiadomości od serwera."""
    while True:
        try:
            message = sock.recv(1024).decode('utf-8')
            if message:
                if "$" in message:
                    lines = message.split("\n")
                    start_index = None
                    end_index = None
                    for i, line in enumerate(lines):
                        if "$" in line:
                            start_index = i
                        if line.strip() == "-----------" and start_index is not None:
                            end_index = i
                            break

                    if start_index is not None and end_index is not None:
                        player_list = "\n".join(lines[start_index:end_index + 1])
                        remaining_message = "\n".join(lines[:start_index] + lines[end_index + 1:])
                    else:
                        player_list = "\n".join(lines[start_index:])
                        remaining_message = "\n".join(lines[:start_index])

                    player_list_area.config(state=tk.NORMAL)
                    player_list_area.delete(1.0, tk.END)
                    player_list_area.insert(tk.END, player_list + "\n")
                    player_list_area.config(state=tk.DISABLED)
                    
                    if remaining_message.strip():
                        update_text_area(text_area, remaining_message)
                else:
                    if "#Welcome to the lobby," in message:
                        player_name = message.split(",")[1].strip().strip(".")  # Wyodrębnij nazwę gracza
                        player_name_area.config(text=f"Player: {player_name}")
                    if not "#Time" in message:
                        update_text_area(text_area, message)
                if "#Correct! Your current word:" in message:
                    update_text_area(text_area, "#Correct")
                if "#Game is starting!" in message:
                            update_text_area(text_area, "#Game is starting")
                            for button in keyboard_frame.winfo_children():
                                button.config(state=tk.NORMAL)    

                if "@" in message:
                    lines = message.split("\n")
                    num_line = None
                    for i, line in enumerate(lines):
                        if "@" in line:
                            num_line = i
                            break
                    if num_line is not None:
                        spaced_text = add_spacing(lines[num_line])
                        pass_label.config(text=spaced_text[1:])

                if "#Time" in message:
                    lines = message.split('\n')
                    new_message = ''.join(lines[:1])
                    new2_message = ''.join(lines[1:])
                    update_text_area(time_area,new_message)
                    if new2_message != '':
                        update_text_area(text_area,new2_message)

                lives = parse_lives_from_message(message)
                if lives is not None:
                    update_image(image_label, lives + 1, image_folder)

                if "#Round over" in message:
                    update_image(image_label, 0, image_folder)
                    for button in keyboard_frame.winfo_children():
                        button.config(state=tk.NORMAL)

                if "#Welcome to the lobby" in message:
                    join_button.config(state=tk.NORMAL)
                    message_entry.grid_remove()
                    update_image(image_label, 0, image_folder)
            else:
                break
        except Exception as e:
            text_area.config(state=tk.NORMAL)
            text_area.insert(tk.END, f"Connection error: {e}\n")
            text_area.config(state=tk.DISABLED)
            text_area.see(tk.END)
            break

def main():
    root = tk.Tk()
    root.title("Hangman Client")

    adres = simpledialog.askstring("Adres IP", "Podaj adres IP serwera:", initialvalue="127.0.0.1")
    if not adres:
        print("Adres IP nie został podany.")
        return
    port = simpledialog.askinteger("Port", "Podaj numer portu serwera:", initialvalue=1235)
    if not port:
        print("Numer portu nie został podany.")
        return
    server_address = (adres, port)
    try:
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect(server_address)
    except Exception as e:
        print(f"Could not connect to server: {e}")
        return

    image_folder = "wisielec-obrazki"
    if not os.path.exists(image_folder):
        print(f"Folder {image_folder} not found!")
        return

    text_area = tk.Label(root, text="", wraplength=400, anchor="center", width=50, height=15)
    text_area.grid(row=1, column=0, padx=10, pady=10, sticky="nw")
    text_area.config(fg="black", font=("Helvetica", 12, "bold"))

    player_name_area = tk.Label(root,text="Player")
    player_name_area.grid(row=0, column=0, columnspan=2, pady=10)
    player_name_area.config(fg="black", font=("Helvetica", 24, "bold"))

    time_area = tk.Label(root,text="Time")
    time_area.grid(row=0, column=1, columnspan=2, pady=10)
    time_area.config(fg="black", font=("Helvetica", 24, "bold"))

    player_list_area = scrolledtext.ScrolledText(root, state=tk.DISABLED, wrap=tk.WORD, height=10, width=40)
    player_list_area.grid(row=1, column=1, padx=10, pady=10)
    
    # Tworzymy ramkę dla pola tekstowego
    player_list_frame = tk.Frame(root, bg="#000000", bd=2, relief="groove")
    player_list_frame.grid(row=1, column=1, padx=10, pady=10, sticky="nsew")

    # Pole tekstowe z dostosowanym stylem
    player_list_area = scrolledtext.ScrolledText(
        player_list_frame,
        state=tk.DISABLED,
        wrap=tk.WORD,
        height=10,
        width=30,
        font=("Helvetica", 12),
        bg="#CCCCCC",  # Kolor tła
        fg="#000000",  # Kolor tekstu
        relief="flat",  # Bez widocznych krawędzi
        highlightthickness=0,  # Wyłącz podświetlenie
    )
    player_list_area.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)


    pass_label = tk.Label(root)
    pass_label.grid(row=2, column=0, columnspan=2, pady=10)
    pass_label.config(fg="black", font=("Helvetica", 24, "bold"))

    image_label = tk.Label(root)
    image_label.grid(row=4, column=0, columnspan=1, pady=10)

    message_entry = tk.Entry(root, width=40)
    message_entry.grid(row=3, column=0, padx=10, pady=10)

    send_button = tk.Button(root, text="SEND", command=lambda: send_message(client_socket, message_entry))
    send_button.grid(row=3, column=1, padx=10, pady=10)

    exit_button = tk.Button(root,text="SWITCH SERVER",command=restart_application)
    exit_button.grid(row=3, column=2, padx=10, pady=10)

    exit_button2 = tk.Button(root,text="EXIT",command=root.destroy)
    exit_button2.grid(row=4, column=2, padx=10, pady=10)

    message_entry.bind("<Return>", lambda event: send_message(client_socket, message_entry))

    join_button = tk.Button(root, text="JOIN GAME", state=tk.DISABLED, command=lambda: send_join(client_socket, join_button))
    join_button.grid(row=4, column=1, columnspan=2, pady=10)

    keyboard_frame = tk.Frame(root)
    keyboard_frame.grid(row=5, column=0, columnspan=2, pady=10)

    for i, letter in enumerate("QWERTYUIOPASDFGHJKLZXCVBNM"):
        if i < 10:
            button = tk.Button(keyboard_frame, text=letter, width=4)
            button.grid(row=0, column=i, padx=2, pady=2)
            button.config(command=lambda b=button: send_letter(client_socket, b))
        elif i < 19:
            button = tk.Button(keyboard_frame, text=letter, width=4)
            button.grid(row=1, column=i - 10, padx=2, pady=2)
            button.config(command=lambda b=button: send_letter(client_socket, b))
        else:
            button = tk.Button(keyboard_frame, text=letter, width=4)
            button.grid(row=2, column=i - 19, padx=2, pady=2)
            button.config(command=lambda b=button: send_letter(client_socket, b))

    threading.Thread(
        target=receive_messages,
        args=(client_socket, text_area, player_list_area, join_button, image_label, image_folder, pass_label,message_entry,player_name_area,time_area,keyboard_frame),
        daemon=True
    ).start()

    def on_close():
        client_socket.close()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()

if __name__ == "__main__":
    main()
