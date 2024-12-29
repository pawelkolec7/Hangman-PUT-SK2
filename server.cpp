#include <iostream>
#include <arpa/inet.h>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <algorithm>
#include <cstring>
#include <vector>
#include <queue>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

std::unordered_map<int, std::string> clients; // socket -> nick
std::unordered_map<std::string, int> nicknames; // nick -> socket
std::vector<int> game_lobby; 
std::queue<int> waiting_room;
std::unordered_map<int, int> player_lives; // Player -> remaining lives
std::unordered_set<int> eliminated_players;
std::unordered_map<int, int> player_points; // socket -> points

bool game_in_progress = false;
bool send_list = false;
std::vector<std::string> word_pool = {"tee","taee","teea","teae","teee"}; //to tylko przykładowe do testów
std::unordered_map<int, std::string> player_word; // socket -> current word
std::unordered_map<int, std::string> player_state; // socket -> current state
std::string round_winner;
void start_game();

void reset_game_for_player(int client_fd) {
    player_word.erase(client_fd);
    player_state.erase(client_fd);
    player_lives.erase(client_fd);
    eliminated_players.erase(client_fd);
}

void send_player_list() {
    std::string player_list = "Player List:\n";
    for (int client_fd : game_lobby) {
        player_list += clients[client_fd] + ": Lives=" + std::to_string(player_lives[client_fd]) +
                       ", Points=" + std::to_string(player_points[client_fd]) + "\n";
    }
    player_list += "-----------\n";
    std::cout << "Sending player list..." << std::endl;  
    std::cout << player_list << std::endl;  
    for (int client_fd : game_lobby) {
        send(client_fd, player_list.c_str(), player_list.size(), 0);
    }
}

void end_round() {
    game_in_progress = false;
    send_list = false;
    

    if (!round_winner.empty()) {
        int winner_fd = nicknames[round_winner];
        player_points[winner_fd] += 1; // Dodaj punkt zwycięzcy
        for (int client_fd : game_lobby) {
            std::string message = "Round over! " + round_winner + " wins this round!\n";
            send(client_fd, message.c_str(), message.size(), 0);
        }
    } else {
        for (int client_fd : game_lobby) {
            std::string message = "Round over! No winner this time.\n";
            send(client_fd, message.c_str(), message.size(), 0);
        }
    }
    //send_player_list();
    round_winner.clear();

    // Przenieś graczy do poczekalni, jeśli gra nie może być kontynuowana
    if (game_lobby.size() < 2) {
        std::cout << "Not enough players for the next round.\n";
        player_points.clear();
        while (!game_lobby.empty()) {
            int player_fd = game_lobby.back();
            game_lobby.pop_back();
            waiting_room.push(player_fd);
            send(player_fd, "Moving to the waiting room.\n", 28, 0);
        }

        // Przenieś graczy z poczekalni, jeśli jest ich wystarczająco dużo
        while (!waiting_room.empty() && game_lobby.size() < 2) {
            int player_fd = waiting_room.front();
            waiting_room.pop();
            game_lobby.push_back(player_fd);
            send(player_fd, "You have been moved back to the game lobby.\n", 44, 0);
        }

        if (game_lobby.size() < 2) {
            std::cout << "Still not enough players for the next round. Waiting...\n";
            return;
        }
    }

    // Automatyczne wznowienie gry po 3 sekundach
    std::cout << "Next round will start in 3 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(5)); // Czekaj 3 sekundy

    if (game_lobby.size() >= 2) {
        std::cout << "Starting the next round.\n";
        start_game();
    } else {
        std::cout << "Not enough players for the next round.\n";
    }
}


void start_game() {
    // Przenieś graczy z poczekalni, jeśli jest ich wystarczająco dużo
    while (!waiting_room.empty()) {
        int player_fd = waiting_room.front();
        waiting_room.pop();
        game_lobby.push_back(player_fd);
        send(player_fd, "You have been moved back to the game lobby.\n", 44, 0);
    }

    if (game_lobby.size() >= 2) {
        game_in_progress = true;
        eliminated_players.clear();

        srand(time(nullptr));
        std::string word = word_pool[rand() % word_pool.size()];
        for (int client_fd : game_lobby) {
            player_word[client_fd] = word;
            player_state[client_fd] = std::string(word.size(), '_');
            player_lives[client_fd] = 10;

            std::string response = "Game is starting! Your word: " + player_state[client_fd] + "\n";
            std::string response2 = "Password: " + player_state[client_fd] + "\n\n";
            send(client_fd, response.c_str(), response.size(), 0);
            send(client_fd, response2.c_str(), response2.size(), 0);
        }
        std::cout << "Game started with " << game_lobby.size() << " players.\n";
        send_player_list();
        //Timer dla rundy
        std::thread([]() {
            int remaining_time = 20; // Czas trwania rundy w sekundach - 20 sekund do testów, docelowo będzie minuta

            while (remaining_time > 0 && game_in_progress) {
                std::cout << "Time remaining: " << remaining_time << " seconds.\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                remaining_time--;
            }

            if (game_in_progress) {
                std::cout << "Time's up! Ending round.\n";
                end_round();
            }
        }).detach();
    }
}

void handle_guess(int client_fd, char guess) {
    if (eliminated_players.count(client_fd)) {
        std::string response = "You have been eliminated. Wait for the next round.\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    std::string &word = player_word[client_fd];
    std::string &state = player_state[client_fd];
    bool correct = false;

    for (size_t i = 0; i < word.size(); ++i) {
        if (tolower(word[i]) == tolower(guess) && state[i] == '_') {
            state[i] = word[i];
            correct = true;
        }
    }

    if (correct) {
        send_player_list();
        std::string response = "Correct! Your current word:\n Password: " + state + "\n";
        send(client_fd, response.c_str(), response.size(), 0);
        std::cout << "Player " << clients[client_fd] << " guessed correctly: " << guess << "\n";

        // Check if the player has guessed their word
        if (state == word) {
            round_winner = clients[client_fd];
            end_round();
            return;
        }
    } else {
        player_lives[client_fd]--;
        send_player_list();
        std::string response = "Wrong guess! You have " + std::to_string(player_lives[client_fd]) + " lives left.\n";
        send(client_fd, response.c_str(), response.size(), 0);
        std::cout << "Player " << clients[client_fd] << " guessed incorrectly: " << guess << "\n";

        if (player_lives[client_fd] <= 0) {
            eliminated_players.insert(client_fd);
            std::string elimination_message = "You have been eliminated. Wait for the next round.\n";
            send(client_fd, elimination_message.c_str(), elimination_message.size(), 0);
            std::cout << "Player " << clients[client_fd] << " has been eliminated.\n";

            //Sprawdzamy czy wszyscy zostali wyeliminowani
            bool all_eliminated = true;
            for (int fd : game_lobby) {
                if (!eliminated_players.count(fd)) {
                    all_eliminated = false;
                    break;
                }
            }

            if (all_eliminated) {
                std::cout << "All players eliminated. Ending round.\n";
                end_round();
            }
        }
    }
}

void handle_client_message(int client_fd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        // Disconnect
        if (clients.count(client_fd)) {
            std::string nick = clients[client_fd];
            nicknames.erase(nick);
            clients.erase(client_fd);
            std::cout << "Client " << nick << " disconnected.\n";

            if (player_points.count(client_fd)) {
                player_points[client_fd] = 0;
            }
    
            // Usuniecie z lobby
            game_lobby.erase(std::remove(game_lobby.begin(), game_lobby.end(), client_fd), game_lobby.end());
            std::queue<int> temp_queue;
            while (!waiting_room.empty()) {
                int fd = waiting_room.front();
                waiting_room.pop();
                if (fd != client_fd) {
                    temp_queue.push(fd);
                }
            }
            waiting_room = temp_queue;
        }
        close(client_fd);
        return;
    }

    std::string message(buffer);
    message.erase(message.find_last_not_of("\r\n") + 1); 

    if (clients.find(client_fd) == clients.end()) {
        // Nick gracza
        if (nicknames.count(message)) {
            std::string response = "Nickname already taken. Choose another:\n";
            send(client_fd, response.c_str(), response.size(), 0);
        } else {
            clients[client_fd] = message;
            nicknames[message] = client_fd;
            std::string response = "Welcome to the lobby, " + message + ".\n";
            send(client_fd, response.c_str(), response.size(), 0);
            std::cout << "Client " << message << " connected.\n";
        }
    } else {
        if (message == "join") {
            if (game_in_progress) {
                waiting_room.push(client_fd);
                std::string response = "Game is in progress. You will join the next round.\n";
                send(client_fd, response.c_str(), response.size(), 0);
                std::cout << "Client " << clients[client_fd] << " is waiting for the next round.\n";
            } else {
                game_lobby.push_back(client_fd);
                std::string response = "You joined the game lobby. Waiting for more players...\n";
                send(client_fd, response.c_str(), response.size(), 0);
                std::cout << "Client " << clients[client_fd] << " joined the game lobby.\n";

                if (game_lobby.size() >= 2) {
                    start_game();
                }
            }
        } else if (message == "exit") {
            std::string nick = clients[client_fd];
            nicknames.erase(nick);
            clients.erase(client_fd);
            game_lobby.erase(std::remove(game_lobby.begin(), game_lobby.end(), client_fd), game_lobby.end());

            std::queue<int> temp_queue;
            while (!waiting_room.empty()) {
                int fd = waiting_room.front();
                waiting_room.pop();
                if (fd != client_fd) {
                    temp_queue.push(fd);
                }
            }
            waiting_room = temp_queue;

            std::string response = "Goodbye!\n";
            send(client_fd, response.c_str(), response.size(), 0);
            close(client_fd);
            std::cout << "Client " << nick << " exited.\n";
        } else if (message.size() == 1 && isalpha(message[0])) {
            if (game_lobby.size() < 2 && game_in_progress == false) {
                std::string response = "You cannot play alone. Please wait for more players to join.\n";
                send(client_fd, response.c_str(), response.size(), 0);
                std::cout << "Client " << clients[client_fd] << " tried to guess a letter but is alone in the lobby.\n";
            } else {
                handle_guess(client_fd, message[0]);
            }
        } else {
            std::string response = "Unknown command. Use 'join', 'exit', or guess a letter.\n";
            send(client_fd, response.c_str(), response.size(), 0);
        }
    }
}

int main(int argc,char**argv) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(server_fd);
        return 1;
    }

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl");
        close(server_fd);
        close(epoll_fd);
        return 1;
    }

    epoll_event events[MAX_EVENTS];

    while (true) {
        int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (event_count == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < event_count; ++i) {
            if (events[i].data.fd == server_fd) {
                // Nowe połaczenie
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }

                event.events = EPOLLIN;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll_ctl");
                    close(client_fd);
                    continue;
                }

                std::string welcome = "Welcome! Please enter your nickname:\n";
                send(client_fd, welcome.c_str(), welcome.size(), 0);
            } else {
                handle_client_message(events[i].data.fd);
            }
        }

        // Sprawdzamy czy gra sie zakonczyla
        if (game_in_progress && game_lobby.empty()) {
            game_in_progress = false;
            std::cout << "Game ended. Moving players from waiting room to game lobby.\n";
            while (!waiting_room.empty() && game_lobby.size() < 2) {
                int client_fd = waiting_room.front();
                waiting_room.pop();
                game_lobby.push_back(client_fd);
                std::string response = "You joined the game lobby. Waiting for more players...\n";
                send(client_fd, response.c_str(), response.size(), 0);
            }
            if (game_lobby.size() >= 2) {
                start_game();
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}
