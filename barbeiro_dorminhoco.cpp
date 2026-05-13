#include <iostream>
#include <pthread.h>
#include <queue>
#include <vector>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <ctime>

using namespace std;

int cadeiras;
int chegadaMin, chegadaMax;
int atendimentoMin, atendimentoMax;
int duracao;

queue<int> filaClientes;

int clientesAtendidos = 0;
int clientesDesistentes = 0;
int totalClientesGerados = 0;

bool simulacaoAtiva = true;
bool barbeiroAtendendo = false;
int clienteAtual = -1;

pthread_mutex_t mutexBarbearia = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condCliente = PTHREAD_COND_INITIALIZER;

pthread_t threadBarbeiro;
pthread_t threadGerador;

chrono::steady_clock::time_point inicio;

// Verifica se o tempo da simulação acabou
bool tempoEsgotado() {
    auto agora = chrono::steady_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(agora - inicio).count();
    return ms >= duracao * 1000;
}

// Formata o tempo no padrão HH:MM:SS.mmm
string tempoAtual() {
    auto agora = chrono::steady_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(agora - inicio).count();

    if (ms > duracao * 1000) {
        ms = duracao * 1000;
    }

    int horas = ms / 3600000;
    ms %= 3600000;
    int minutos = ms / 60000;
    ms %= 60000;
    int segundos = ms / 1000;
    int milissegundos = ms % 1000;

    stringstream ss;
    ss << "[" << setfill('0') << setw(2) << horas << ":"
       << setw(2) << minutos << ":"
       << setw(2) << segundos << "."
       << setw(3) << milissegundos << "]";

    return ss.str();
}

// Gera número aleatório entre mínimo e máximo
int aleatorioEntre(int minimo, int maximo, unsigned int &seed) {
    return minimo + rand_r(&seed) % (maximo - minimo + 1);
}

// Espera em pequenos intervalos para conseguir parar quando o tempo acabar
void esperarComControle(int tempoMs) {
    int tempoPassado = 0;

    while (simulacaoAtiva && !tempoEsgotado() && tempoPassado < tempoMs) {
        int pedaco = 50;

        if (tempoMs - tempoPassado < pedaco) {
            pedaco = tempoMs - tempoPassado;
        }

        usleep(pedaco * 1000);
        tempoPassado += pedaco;
    }
}

// Imprime a situação atual da fila
void imprimirFila() {
    queue<int> copia = filaClientes;

    cout << "Fila: [";

    for (int i = 0; i < cadeiras; i++) {
        if (i < (int)filaClientes.size()) {
            cout << "#";
        } else {
            cout << ".";
        }
    }

    cout << "] (" << filaClientes.size() << "/" << cadeiras << ") -> ";

    while (!copia.empty()) {
        cout << "C" << copia.front() << " ";
        copia.pop();
    }

    cout << endl;
}

// Imprime cada evento importante da simulação
void imprimirEvento(string evento) {
    if (tempoEsgotado()) {
        return;
    }

    cout << tempoAtual() << " " << evento << endl;

    if (barbeiroAtendendo) {
        cout << "Barbeiro: ATENDE C" << clienteAtual << endl;
    } else {
        cout << "Barbeiro: DORME" << endl;
    }

    imprimirFila();

    cout << "Contadores: atendidos=" << clientesAtendidos
         << " | desistentes=" << clientesDesistentes
         << " | em_espera=" << filaClientes.size() << endl;

    cout << "------------------------------------------------------------" << endl;
}

// Thread do barbeiro
void* rotinaBarbeiro(void* arg) {
    unsigned int seed = time(nullptr) + 200;

    while (simulacaoAtiva && !tempoEsgotado()) {
        pthread_mutex_lock(&mutexBarbearia);

        while (filaClientes.empty() && simulacaoAtiva && !tempoEsgotado()) {
            barbeiroAtendendo = false;
            clienteAtual = -1;
            imprimirEvento("Barbeiro está dormindo, pois não há clientes");
            pthread_cond_wait(&condCliente, &mutexBarbearia);
        }

        if (!simulacaoAtiva || tempoEsgotado()) {
            pthread_mutex_unlock(&mutexBarbearia);
            break;
        }

        clienteAtual = filaClientes.front();
        filaClientes.pop();
        barbeiroAtendendo = true;

        imprimirEvento("Barbeiro iniciou atendimento do cliente C" + to_string(clienteAtual));

        pthread_mutex_unlock(&mutexBarbearia);

        int tempoAtendimento = aleatorioEntre(atendimentoMin, atendimentoMax, seed);
        esperarComControle(tempoAtendimento);

        pthread_mutex_lock(&mutexBarbearia);

        if (!tempoEsgotado()) {
            clientesAtendidos++;
            barbeiroAtendendo = false;
            imprimirEvento("Barbeiro concluiu atendimento do cliente C" + to_string(clienteAtual));
        }

        clienteAtual = -1;

        pthread_mutex_unlock(&mutexBarbearia);
    }

    return nullptr;
}

// Thread que gera clientes
void* rotinaGeradorClientes(void* arg) {
    unsigned int seed = time(nullptr) + 500;

    while (simulacaoAtiva && !tempoEsgotado()) {
        int tempoChegada = aleatorioEntre(chegadaMin, chegadaMax, seed);
        esperarComControle(tempoChegada);

        if (!simulacaoAtiva || tempoEsgotado()) {
            break;
        }

        pthread_mutex_lock(&mutexBarbearia);

        totalClientesGerados++;
        int idCliente = totalClientesGerados;

        if ((int)filaClientes.size() < cadeiras) {
            filaClientes.push(idCliente);
            imprimirEvento("Cliente C" + to_string(idCliente) + " chegou e entrou na fila");
            pthread_cond_signal(&condCliente);
        } else {
            clientesDesistentes++;
            imprimirEvento("Cliente C" + to_string(idCliente) + " chegou, mas desistiu por falta de cadeira");
        }

        pthread_mutex_unlock(&mutexBarbearia);
    }

    return nullptr;
}

// Mostra resumo final
void imprimirResumoFinal() {
    cout << endl;
    cout << "================ RESUMO FINAL ================" << endl;
    cout << "Clientes atendidos: " << clientesAtendidos << endl;
    cout << "Clientes desistentes: " << clientesDesistentes << endl;
    cout << "Clientes restantes na fila: " << filaClientes.size() << endl;
    cout << "Total de clientes gerados: " << totalClientesGerados << endl;
    cout << "==============================================" << endl;
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
    cin >> duracao;

    if (cadeiras <= 0 || duracao <= 0 || chegadaMin < 0 || atendimentoMin < 0 ||
        chegadaMax < chegadaMin || atendimentoMax < atendimentoMin) {
        cout << "Parametros invalidos." << endl;
        return 1;
    }

    inicio = chrono::steady_clock::now();

    pthread_create(&threadBarbeiro, nullptr, rotinaBarbeiro, nullptr);
    pthread_create(&threadGerador, nullptr, rotinaGeradorClientes, nullptr);

    while (!tempoEsgotado()) {
        usleep(1000);
    }

    pthread_mutex_lock(&mutexBarbearia);
    simulacaoAtiva = false;
    pthread_cond_broadcast(&condCliente);
    pthread_mutex_unlock(&mutexBarbearia);

    pthread_join(threadGerador, nullptr);
    pthread_join(threadBarbeiro, nullptr);

    imprimirResumoFinal();

    pthread_mutex_destroy(&mutexBarbearia);
    pthread_cond_destroy(&condCliente);

    return 0;
}