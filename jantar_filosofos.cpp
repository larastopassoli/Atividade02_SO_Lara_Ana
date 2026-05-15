#include <iostream>
#include <pthread.h>
#include <vector>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <ctime>

using namespace std;

enum Estado { PENS, FOME, COME }; //cria os estados possíveis

int N;
int duracao;
int pensarMin, pensarMax;
int comerMin, comerMax;

vector<Estado> estados; //guarda o estado (PENS, FOME, COME)
vector<int> refeicoes; //guarda quantas vezes cada um comeu
vector<pthread_t> threads; //guarda as threads dos filósofos (Cada filósofo é uma thread)
vector<pthread_cond_t> condicoes; //serve para fazer o filósofo esperar e acordar ele pra quando puder comer 

// cria um mutex e inicia ele para que as threads não alterem os estado dos filósofos ao mesmo tempo
pthread_mutex_t mutexMonitor = PTHREAD_MUTEX_INITIALIZER;

bool simulacaoAtiva = true; //controla quando o programa para
chrono::steady_clock::time_point inicio; //guarda o inicio da simulação

//verifica se o tempo total da simulação já acabou
bool tempoEsgotado() {
    auto agora = chrono::steady_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(agora - inicio).count();
    return ms >= duracao * 1000;
}

//transforma estado em texto para imprimir 
string estadoTexto(Estado e) {
    if (e == PENS) return "PENS";
    if (e == FOME) return "FOME";
    return "COME";
}

//calcula o tempo decorrido desde o início da simulação
string tempoAtual() {
    auto agora = chrono::steady_clock::now(); //pega o tempo atual
    //calcula quanto tempo desde que começou e converte para milissegundos
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

//retorna o garfo da esquerda do filosofo
int esquerda(int i) {
    return i;
}

// Retorna o garfo da direita 
int direita(int i) {
    return (i + 1) % N;
}

// Retorna o filósofo sentado à esquerda
int vizinhoEsquerda(int i) {
    return (i + N - 1) % N;
}

// Retorna o filósofo sentado à direita
int vizinhoDireita(int i) {
    return (i + 1) % N;
}

//verifica se o filósofo pode comer 
bool podeComer(int i) {
    return estados[i] == FOME && //verifica se o filósofo está com fome
           estados[vizinhoEsquerda(i)] != COME && //verifica se o viziho da esquerda não está comendo
           estados[vizinhoDireita(i)] != COME; //verifica se o vizinho da direira não está comendo
}

//A função imprimirEvento é responsável por exibir cada mudança importante da simulação
void imprimirEvento(int id, Estado antigo, Estado novoEstado) {
    if (tempoEsgotado()) {
        return;
    }

    cout << tempoAtual() << " F" << id << ": "
         << estadoTexto(antigo) << " -> " << estadoTexto(novoEstado) << endl;

    cout << "Garfos: ";
    for (int i = 0; i < N; i++) {
        bool ocupado = false;

        for (int f = 0; f < N; f++) {
            if (estados[f] == COME && (esquerda(f) == i || direita(f) == i)) {
                ocupado = true;
            }
        }

        cout << (ocupado ? "[X]" : "[O]");
    }
    cout << endl;

    cout << "Filósofos: ";
    for (int i = 0; i < N; i++) {
        cout << "F" << i << ":" << estadoTexto(estados[i]);
        if (i < N - 1) cout << " | ";
    }
    cout << endl;

    cout << "Refeições: ";
    for (int i = 0; i < N; i++) {
        cout << "F" << i << ":" << refeicoes[i];
        if (i < N - 1) cout << " | ";
    }
    cout << endl;

    cout << "------------------------------------------------------------" << endl;
}

//Ela gera um número aleatório (tempo de pensar ou comer) entre o mínimo e o máximo
int aleatorioEntre(int minimo, int maximo, unsigned int &seed) {
    return minimo + rand_r(&seed) % (maximo - minimo + 1);
}

//faz uma espera em pequenos pedaços, parando se o tempo da simulação acabar
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

void mudarEstado(int id, Estado novoEstado) { //altera o estado 
    Estado antigo = estados[id]; //guarda o estado antigo 
    estados[id] = novoEstado; //atualiza o estado atual do filósofo
    imprimirEvento(id, antigo, novoEstado); //mostra a mudança de estado
}

void testarFilosofo(int id) { // Verifica se o filósofo pode começar a comer
    if (!simulacaoAtiva || tempoEsgotado()) { //verifica se a função ainda está rodando, se a simulação ou o tempo acabou então ele para a função 
        return;
    }

    if (podeComer(id)) { 
        mudarEstado(id, COME); // Altera o estado do filósofo para comendo
        refeicoes[id]++; // conta a quantidade de refeições do filósofo
        pthread_cond_signal(&condicoes[id]);//acorda a thread do filosofo pois agora ele pode comer 
    }
}

// Tenta pegar os garfos para o filósofo
bool pegarGarfos(int id) {

    // Trava o mutex para acessar os dados compartilhados
    pthread_mutex_lock(&mutexMonitor);

    // Encerra caso a simulação tenha acabado
    if (!simulacaoAtiva || tempoEsgotado()) {
        pthread_mutex_unlock(&mutexMonitor);
        return false;
    }

    // Altera o estado do filósofo para fome
    mudarEstado(id, FOME);

    // Verifica se o filósofo pode começar a comer
    testarFilosofo(id);

    // Espera até conseguir começar a comer
    while (simulacaoAtiva &&
           !tempoEsgotado() &&
           estados[id] != COME) {

        // Faz a thread esperar até ser acordada
        pthread_cond_wait(&condicoes[id], &mutexMonitor);
    }

    // Verifica se o filósofo conseguiu comer
    bool conseguiuComer =
        simulacaoAtiva &&
        !tempoEsgotado() &&
        estados[id] == COME;

    // Libera o mutex
    pthread_mutex_unlock(&mutexMonitor);

    // Retorna se conseguiu pegar os garfos
    return conseguiuComer;
}

void soltarGarfos(int id) { //libera os garfos depois do filósofo terminar de comer 
    pthread_mutex_lock(&mutexMonitor);//trava o mutex pra acessar os dados compartilhados

    mudarEstado(id, PENS);//altera o estado do filósofo para pensando 

    testarFilosofo(vizinhoEsquerda(id));//verifica se o vizinho da esquerda pode comer 
    testarFilosofo(vizinhoDireita(id));//cerifica se o vizinho da direita pode comer 

    pthread_mutex_unlock(&mutexMonitor);//libera o mutex, para que outras threads possam acessar os estados 
}

// Função executada por cada thread filósofo
void* rotinaFilosofo(void* arg) {

    // Recupera o id do filósofo
    int id = *((int*)arg);

    // Libera a memória usada para passar o id
    delete (int*)arg;

    // Cria uma semente para gerar tempos aleatórios
    unsigned int seed = time(nullptr) + id * 100;

    // Repete o ciclo enquanto a simulação estiver ativa
    while (simulacaoAtiva && !tempoEsgotado()) {

        // Simula o tempo em que o filósofo fica pensando
        int tempoPensando = aleatorioEntre(pensarMin, pensarMax, seed);
        esperarComControle(tempoPensando);

        // Encerra se a simulação acabou
        if (!simulacaoAtiva || tempoEsgotado()) break;

        // Tenta pegar os garfos
        bool conseguiuPegar = pegarGarfos(id);

        // Encerra se não conseguiu pegar os garfos
        if (!conseguiuPegar || !simulacaoAtiva || tempoEsgotado()) break;

        // Simula o tempo em que o filósofo fica comendo
        int tempoComendo = aleatorioEntre(comerMin, comerMax, seed);
        esperarComControle(tempoComendo);

        // Libera os garfos após comer
        soltarGarfos(id);
    }

    // Finaliza a thread
    return nullptr;
}

// Lê os parâmetros da simulação de um arquivo
bool lerArquivo(string nomeArquivo) {
    ifstream arquivo(nomeArquivo);

    // Verifica se o arquivo abriu corretamente
    if (!arquivo.is_open()) {
        return false;
    }

    // Lê os valores do arquivo
    arquivo >> N;
    arquivo >> duracao;
    arquivo >> pensarMin;
    arquivo >> pensarMax;
    arquivo >> comerMin;
    arquivo >> comerMax;

    arquivo.close();
    return true;
}

// Mostra quantas vezes cada filósofo comeu
void imprimirResumoFinal() {
    cout << endl;
    cout << " RESUMO FINAL " << endl;

    for (int i = 0; i < N; i++) {
        cout << "Filósofo F" << i << " comeu "
             << refeicoes[i] << " vezes." << endl;
    }

    cout << "----------------------------------------" << endl;
}

int main(int argc, char* argv[]) {

    // Executa usando arquivo de entrada
    if (argc == 2) {

        // Lê os dados do arquivo
        if (!lerArquivo(argv[1])) {
            cout << "Erro ao abrir arquivo de entrada." << endl;
            return 1;
        }

    // Executa usando parâmetros pelo terminal
    } else if (argc == 7) {

        N = atoi(argv[1]);
        duracao = atoi(argv[2]);
        pensarMin = atoi(argv[3]);
        pensarMax = atoi(argv[4]);
        comerMin = atoi(argv[5]);
        comerMax = atoi(argv[6]);

    } else {

        // Mostra como executar o programa
        cout << "Uso com arquivo:" << endl;
        cout << "./filosofos entrada_filosofos.txt" << endl;

        cout << endl;

        cout << "Ou uso com parametros:" << endl;
        cout << "./filosofos N duracao pensarMin pensarMax comerMin comerMax" << endl;

        return 1;
    }

    // Valida os parâmetros informados
    if (N < 3 || duracao <= 0 || pensarMin < 0 || comerMin < 0 ||
        pensarMax < pensarMin || comerMax < comerMin) {

        cout << "Parametros invalidos." << endl;
        return 1;
    }

    // Inicializa os vetores
    estados.resize(N, PENS);
    refeicoes.resize(N, 0);
    threads.resize(N);
    condicoes.resize(N);

    // Inicializa as variáveis de condição
    for (int i = 0; i < N; i++) {
        pthread_cond_init(&condicoes[i], nullptr);
    }

    // Guarda o instante inicial da simulação
    inicio = chrono::steady_clock::now();

    // Cria as threads dos filósofos
    for (int i = 0; i < N; i++) {

        // Cria um id separado para cada thread
        int* id = new int(i);

        // Cria a thread executando a rotina do filósofo
        pthread_create(&threads[i], nullptr, rotinaFilosofo, id);
    }

    // Espera até o tempo da simulação acabar
    while (!tempoEsgotado()) {
        usleep(1000);
    }

    // Trava o mutex para finalizar a simulação
    pthread_mutex_lock(&mutexMonitor);

    // Marca a simulação como encerrada
    simulacaoAtiva = false;

    // Acorda todas as threads que estiverem esperando
    for (int i = 0; i < N; i++) {
        pthread_cond_broadcast(&condicoes[i]);
    }

    // Libera o mutex
    pthread_mutex_unlock(&mutexMonitor);

    // Espera todas as threads terminarem
    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Mostra o resumo final
    imprimirResumoFinal();

    // Destroi as variáveis de condição
    for (int i = 0; i < N; i++) {
        pthread_cond_destroy(&condicoes[i]);
    }

    // Destroi o mutex
    pthread_mutex_destroy(&mutexMonitor);

    return 0;
}

// Compilar:
// g++ jantar_filosofos.cpp -o filosofos -pthread

// Executar:
// ./filosofos entrada_filosofos.txt