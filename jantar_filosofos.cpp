#include <iostream>
#include <pthread.h>
#include <vector>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdlib>

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

void imprimirEvento(int id, Estado antigo, Estado novoEstado) {
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

int aleatorioEntre(int minimo, int maximo, unsigned int &seed) {
    return minimo + rand_r(&seed) % (maximo - minimo + 1);
}

void mudarEstado(int id, Estado novoEstado) {
    Estado antigo = estados[id];
    estados[id] = novoEstado;
    imprimirEvento(id, antigo, novoEstado);
}

void testarFilosofo(int id) {
    if (podeComer(id)) {
        mudarEstado(id, COME);
        refeicoes[id]++;
        pthread_cond_signal(&condicoes[id]);
    }
}

void pegarGarfos(int id) {
    pthread_mutex_lock(&mutexMonitor);

    mudarEstado(id, FOME);

    testarFilosofo(id);

    while (simulacaoAtiva && estados[id] != COME) {
        pthread_cond_wait(&condicoes[id], &mutexMonitor);
    }

    pthread_mutex_unlock(&mutexMonitor);
}

void soltarGarfos(int id) {
    pthread_mutex_lock(&mutexMonitor);

    mudarEstado(id, PENS);

    testarFilosofo(vizinhoEsquerda(id));
    testarFilosofo(vizinhoDireita(id));

    pthread_mutex_unlock(&mutexMonitor);
}

void* rotinaFilosofo(void* arg) {
    int id = *((int*)arg);
    delete (int*)arg;

    unsigned int seed = time(nullptr) + id * 100;

    while (simulacaoAtiva) {
        int tempoPensando = aleatorioEntre(pensarMin, pensarMax, seed);
        usleep(tempoPensando * 1000);

        if (!simulacaoAtiva) break;

        pegarGarfos(id);

        if (!simulacaoAtiva) break;

        int tempoComendo = aleatorioEntre(comerMin, comerMax, seed);
        usleep(tempoComendo * 1000);

        soltarGarfos(id);
    }

    return nullptr;
}

bool lerArquivo(string nomeArquivo) {
    ifstream arquivo(nomeArquivo);

    if (!arquivo.is_open()) {
        return false;
    }

    arquivo >> N;
    arquivo >> duracao;
    arquivo >> pensarMin;
    arquivo >> pensarMax;
    arquivo >> comerMin;
    arquivo >> comerMax;

    arquivo.close();
    return true;
}

void imprimirResumoFinal() {
    cout << endl;
    cout << "================ RESUMO FINAL ================" << endl;

    for (int i = 0; i < N; i++) {
        cout << "Filósofo F" << i << " comeu " << refeicoes[i] << " vezes." << endl;
    }

    cout << "==============================================" << endl;
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        if (!lerArquivo(argv[1])) {
            cout << "Erro ao abrir arquivo de entrada." << endl;
            return 1;
        }
    } else if (argc == 7) {
        N = atoi(argv[1]);
        duracao = atoi(argv[2]);
        pensarMin = atoi(argv[3]);
        pensarMax = atoi(argv[4]);
        comerMin = atoi(argv[5]);
        comerMax = atoi(argv[6]);
    } else {
        cout << "Uso com arquivo:" << endl;
        cout << "./filosofos entrada_filosofos.txt" << endl;
        cout << endl;
        cout << "Ou uso com parametros:" << endl;
        cout << "./filosofos N duracao pensarMin pensarMax comerMin comerMax" << endl;
        return 1;
    }

    if (N < 3 || duracao <= 0 || pensarMin < 0 || comerMin < 0 ||
        pensarMax < pensarMin || comerMax < comerMin) {
        cout << "Parametros invalidos." << endl;
        return 1;
    }

    estados.resize(N, PENS);
    refeicoes.resize(N, 0);
    threads.resize(N);
    condicoes.resize(N);

    for (int i = 0; i < N; i++) {
        pthread_cond_init(&condicoes[i], nullptr);
    }

    inicio = chrono::steady_clock::now();

    for (int i = 0; i < N; i++) {
        int* id = new int(i);
        pthread_create(&threads[i], nullptr, rotinaFilosofo, id);
    }

    sleep(duracao);

    pthread_mutex_lock(&mutexMonitor);
    simulacaoAtiva = false;

    for (int i = 0; i < N; i++) {
        pthread_cond_broadcast(&condicoes[i]);
    }

    pthread_mutex_unlock(&mutexMonitor);

    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], nullptr);
    }

    imprimirResumoFinal();

    for (int i = 0; i < N; i++) {
        pthread_cond_destroy(&condicoes[i]);
    }

    pthread_mutex_destroy(&mutexMonitor);

    return 0;
}