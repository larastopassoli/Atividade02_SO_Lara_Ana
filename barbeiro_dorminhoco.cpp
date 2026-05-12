#include <iostream>
#include <pthread.h>
#include <queue>
#include <vector>
#include <unistd.h>
#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>

using namespace std;

int cadeiras;
int chegadaMin, chegadaMax;
int atendimentoMin, atendimentoMax;
int duracaoSimulacao;

bool simulando = true;

int clientesAtendidos = 0;
int clientesDesistentes = 0;
int proximoCliente = 1;

string estadoBarbeiro = "DORME";
int clienteAtual = -1;

queue<int> filaClientes;
vector<pthread_t> threadsClientes;

pthread_mutex_t mutexFila = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clienteChegou = PTHREAD_COND_INITIALIZER;

chrono::steady_clock::time_point inicioSimulacao;

int numeroAleatorio(int min, int max) {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

string tempoAtual() {
    auto agora = chrono::steady_clock::now();
    auto diff = chrono::duration_cast<chrono::milliseconds>(agora - inicioSimulacao).count();

    int horas = diff / 3600000;
    diff %= 3600000;

    int minutos = diff / 60000;
    diff %= 60000;

    int segundos = diff / 1000;
    int milissegundos = diff % 1000;

    stringstream ss;
    ss << "["
       << setfill('0') << setw(2) << horas << ":"
       << setfill('0') << setw(2) << minutos << ":"
       << setfill('0') << setw(2) << segundos << "."
       << setfill('0') << setw(3) << milissegundos
       << "]";

    return ss.str();
}

void imprimirFila() {
    queue<int> copia = filaClientes;

    cout << "Fila: [";

    int ocupadas = filaClientes.size();

    for (int i = 0; i < cadeiras; i++) {
        if (i < ocupadas) {
            cout << "#";
        } else {
            cout << ".";
        }
    }

    cout << "] (" << ocupadas << "/" << cadeiras << ") -> ";

    while (!copia.empty()) {
        cout << "C" << copia.front() << " ";
        copia.pop();
    }

    cout << endl;
}

void imprimirEvento(const string& evento) {
    cout << tempoAtual() << " " << evento << endl;

    if (estadoBarbeiro == "ATENDE") {
        cout << "Barbeiro: ATENDE C" << clienteAtual << endl;
    } else {
        cout << "Barbeiro: DORME" << endl;
    }

    imprimirFila();

    cout << "Contadores: atendidos=" << clientesAtendidos
         << " | desistentes=" << clientesDesistentes
         << " | em_espera=" << filaClientes.size()
         << endl;

    cout << "------------------------------------------------------------" << endl;
}

void* funcaoCliente(void* arg) {
    int id = *((int*)arg);
    delete (int*)arg;

    pthread_mutex_lock(&mutexFila);

    if ((int)filaClientes.size() >= cadeiras) {
        clientesDesistentes++;

        imprimirEvento("Cliente C" + to_string(id) + " chegou, mas desistiu por falta de cadeira");
    } else {
        filaClientes.push(id);

        imprimirEvento("Cliente C" + to_string(id) + " chegou e entrou na fila");

        pthread_cond_signal(&clienteChegou);
    }

    pthread_mutex_unlock(&mutexFila);

    return NULL;
}

void* funcaoBarbeiro(void* arg) {
    while (true) {
        pthread_mutex_lock(&mutexFila);

        while (filaClientes.empty() && simulando) {
            estadoBarbeiro = "DORME";
            clienteAtual = -1;

            imprimirEvento("Barbeiro está dormindo, pois não há clientes");

            pthread_cond_wait(&clienteChegou, &mutexFila);
        }

        if (!simulando && filaClientes.empty()) {
            pthread_mutex_unlock(&mutexFila);
            break;
        }

        int cliente = filaClientes.front();
        filaClientes.pop();

        estadoBarbeiro = "ATENDE";
        clienteAtual = cliente;

        imprimirEvento("Barbeiro iniciou atendimento do cliente C" + to_string(cliente));

        pthread_mutex_unlock(&mutexFila);

        int tempoAtendimento = numeroAleatorio(atendimentoMin, atendimentoMax);
        usleep(tempoAtendimento * 1000);

        pthread_mutex_lock(&mutexFila);

        clientesAtendidos++;
        estadoBarbeiro = "DORME";
        clienteAtual = -1;

        imprimirEvento("Barbeiro concluiu atendimento do cliente C" + to_string(cliente));

        pthread_mutex_unlock(&mutexFila);
    }

    return NULL;
}

int main() {
    cout << "Digite o numero de cadeiras de espera: ";
    cin >> cadeiras;

    cout << "Digite o tempo minimo entre chegadas de clientes em ms: ";
    cin >> chegadaMin;

    cout << "Digite o tempo maximo entre chegadas de clientes em ms: ";
    cin >> chegadaMax;

    cout << "Digite o tempo minimo de atendimento em ms: ";
    cin >> atendimentoMin;

    cout << "Digite o tempo maximo de atendimento em ms: ";
    cin >> atendimentoMax;

    cout << "Digite a duracao total da simulacao em segundos: ";
    cin >> duracaoSimulacao;

    inicioSimulacao = chrono::steady_clock::now();

    pthread_t threadBarbeiro;
    pthread_create(&threadBarbeiro, NULL, funcaoBarbeiro, NULL);

    while (true) {
        auto agora = chrono::steady_clock::now();
        auto tempoPassado = chrono::duration_cast<chrono::seconds>(agora - inicioSimulacao).count();

        if (tempoPassado >= duracaoSimulacao) {
            break;
        }

        pthread_t threadCliente;

        int* idCliente = new int;
        *idCliente = proximoCliente++;

        pthread_create(&threadCliente, NULL, funcaoCliente, idCliente);

        threadsClientes.push_back(threadCliente);

        int tempoChegada = numeroAleatorio(chegadaMin, chegadaMax);
        usleep(tempoChegada * 1000);
    }

    pthread_mutex_lock(&mutexFila);
    simulando = false;
    pthread_cond_signal(&clienteChegou);
    pthread_mutex_unlock(&mutexFila);

    for (pthread_t t : threadsClientes) {
        pthread_join(t, NULL);
    }

    pthread_join(threadBarbeiro, NULL);

    cout << endl;
    cout << "================ RESUMO FINAL ================" << endl;
    cout << "Clientes atendidos: " << clientesAtendidos << endl;
    cout << "Clientes desistentes: " << clientesDesistentes << endl;
    cout << "Clientes restantes na fila: " << filaClientes.size() << endl;
    cout << "Total de clientes gerados: " << proximoCliente - 1 << endl;
    cout << "==============================================" << endl;

    pthread_mutex_destroy(&mutexFila);
    pthread_cond_destroy(&clienteChegou);

    return 0;
}