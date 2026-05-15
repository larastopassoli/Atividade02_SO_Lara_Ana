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

// Quantidade de cadeiras disponíveis na sala de espera
int cadeiras;

// Intervalos de tempo para chegada de clientes
int chegadaMin, chegadaMax;

// Intervalos de tempo para atendimento do barbeiro
int atendimentoMin, atendimentoMax;

// Duração total da simulação
int duracao;

// Fila de clientes aguardando atendimento (FIFO)
queue<int> filaClientes;

// Contadores estatísticos da simulação
int clientesAtendidos = 0;
int clientesDesistentes = 0;
int totalClientesGerados = 0;

// Controla se a simulação ainda está ativa
bool simulacaoAtiva = true;

// Indica se o barbeiro está atendendo alguém
bool barbeiroAtendendo = false;

// Armazena o cliente atualmente em atendimento
int clienteAtual = -1;

// Mutex utilizado para evitar condição de corrida
// no acesso às variáveis compartilhadas
pthread_mutex_t mutexBarbearia = PTHREAD_MUTEX_INITIALIZER;

// Variável de condição usada para acordar o barbeiro
// quando um cliente chega
pthread_cond_t condCliente = PTHREAD_COND_INITIALIZER;

// Threads principais da aplicação
pthread_t threadBarbeiro;
pthread_t threadGerador;

// Marca o início da simulação
chrono::steady_clock::time_point inicio;

// Verifica se o tempo limite da simulação foi atingido
bool tempoEsgotado() {
    auto agora = chrono::steady_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(agora - inicio).count();

    return ms >= duracao * 1000;
}

// Formata o tempo no padrão HH:MM:SS.mmm
// para facilitar visualização dos eventos
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

// Gera número aleatório dentro de um intervalo
// Cada thread utiliza uma seed própria
int aleatorioEntre(int minimo, int maximo, unsigned int &seed) {
    return minimo + rand_r(&seed) % (maximo - minimo + 1);
}

// Espera controlada em pequenos intervalos
// Isso permite interromper a execução rapidamente
// quando a simulação acabar
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

// Exibe o estado atual da fila de espera
// '#' representa cadeira ocupada
// '.' representa cadeira livre
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

    // Mostra os IDs dos clientes na fila
    while (!copia.empty()) {
        cout << "C" << copia.front() << " ";
        copia.pop();
    }

    cout << endl;
}

// Centraliza a impressão dos eventos importantes
// da simulação
void imprimirEvento(string evento) {

    if (tempoEsgotado()) {
        return;
    }

    cout << tempoAtual() << " " << evento << endl;

    // Mostra estado atual do barbeiro
    if (barbeiroAtendendo) {
        cout << "Barbeiro: ATENDE C" << clienteAtual << endl;
    } else {
        cout << "Barbeiro: DORME" << endl;
    }

    imprimirFila();

    // Mostra estatísticas em tempo real
    cout << "Contadores: atendidos=" << clientesAtendidos
         << " | desistentes=" << clientesDesistentes
         << " | em_espera=" << filaClientes.size() << endl;

    cout << "------------------------------------------------------------" << endl;
}

// Thread do barbeiro
// Implementa o problema clássico do barbeiro dorminhoco
void* rotinaBarbeiro(void* arg) {

    unsigned int seed = time(nullptr) + 200;

    while (simulacaoAtiva && !tempoEsgotado()) {

        pthread_mutex_lock(&mutexBarbearia);

        // Enquanto não houver clientes,
        // o barbeiro permanece dormindo
        while (filaClientes.empty() && simulacaoAtiva && !tempoEsgotado()) {

            barbeiroAtendendo = false;
            clienteAtual = -1;

            imprimirEvento("Barbeiro está dormindo, pois não há clientes");

            // pthread_cond_wait libera o mutex temporariamente
            // enquanto a thread aguarda sinal
            pthread_cond_wait(&condCliente, &mutexBarbearia);
        }

        if (!simulacaoAtiva || tempoEsgotado()) {
            pthread_mutex_unlock(&mutexBarbearia);
            break;
        }

        // Remove o próximo cliente da fila (FIFO)
        clienteAtual = filaClientes.front();
        filaClientes.pop();

        barbeiroAtendendo = true;

        imprimirEvento("Barbeiro iniciou atendimento do cliente C" + to_string(clienteAtual));

        pthread_mutex_unlock(&mutexBarbearia);

        // Simula tempo de atendimento
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

// Thread responsável por gerar clientes
// em intervalos aleatórios
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

        // Verifica se ainda há cadeiras disponíveis
        if ((int)filaClientes.size() < cadeiras) {

            filaClientes.push(idCliente);

            imprimirEvento("Cliente C" + to_string(idCliente) + " chegou e entrou na fila");

            // Acorda o barbeiro caso esteja dormindo
            pthread_cond_signal(&condCliente);

        } else {

            // Cliente desiste se não houver espaço
            clientesDesistentes++;

            imprimirEvento("Cliente C" + to_string(idCliente) + " chegou, mas desistiu por falta de cadeira");
        }

        pthread_mutex_unlock(&mutexBarbearia);
    }

    return nullptr;
}

// Exibe estatísticas finais da simulação
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

    // Entrada de parâmetros da simulação
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

    // Validação básica dos parâmetros informados
    if (cadeiras <= 0 || duracao <= 0 || chegadaMin < 0 || atendimentoMin < 0 ||
        chegadaMax < chegadaMin || atendimentoMax < atendimentoMin) {

        cout << "Parametros invalidos." << endl;

        return 1;
    }

    // Marca início da execução
    inicio = chrono::steady_clock::now();

    // Criação das threads concorrentes
    pthread_create(&threadBarbeiro, nullptr, rotinaBarbeiro, nullptr);
    pthread_create(&threadGerador, nullptr, rotinaGeradorClientes, nullptr);

    // Mantém programa principal ativo
    // até o fim da simulação
    while (!tempoEsgotado()) {
        usleep(1000);
    }

    // Encerra a simulação de forma segura
    pthread_mutex_lock(&mutexBarbearia);

    simulacaoAtiva = false;

    // Acorda possíveis threads bloqueadas
    pthread_cond_broadcast(&condCliente);

    pthread_mutex_unlock(&mutexBarbearia);

    // Aguarda encerramento das threads
    pthread_join(threadGerador, nullptr);
    pthread_join(threadBarbeiro, nullptr);

    imprimirResumoFinal();

    // Liberação dos recursos de sincronização
    pthread_mutex_destroy(&mutexBarbearia);
    pthread_cond_destroy(&condCliente);

    return 0;
}

//compilar:
//g++ barbeiro_dorminhoco.cpp -o barbeiro -pthread
//executar:
//./barbeiro