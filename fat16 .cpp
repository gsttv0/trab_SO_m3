#include <iostream>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <ctime>
#include <string>
using namespace std;

#pragma pack(1)

struct SetorBoot {
    unsigned char salto[3];
    unsigned char nomeOEM[8];
    unsigned short bytesPorSetor;
    unsigned char setoresPorCluster;
    unsigned short setoresReservados;
    unsigned char numFATs;
    unsigned short entradasRaiz;
    unsigned short totalSetores16;
    unsigned char tipoMidia;
    unsigned short setoresPorFAT;
    unsigned short setoresPorTrilha;
    unsigned short numCabecas;
    unsigned int setoresOcultos;
    unsigned int totalSetores32;
};

struct EntradaDir {
    unsigned char nome[11];
    unsigned char atributos;
    unsigned char reservado;
    unsigned char criacaoDecimos;
    unsigned short horaCriacao;
    unsigned short dataCriacao;
    unsigned short dataAcesso;
    unsigned short clusterAlto;
    unsigned short horaModif;
    unsigned short dataModif;
    unsigned short clusterInicial;
    unsigned int tamanho;
};

FILE *arquivo;
SetorBoot boot;

long inicioFAT;
long inicioRaiz;
long inicioDados;
int bytesPorCluster;
int setoresRaiz;
int totalClusters;

void nomeParaFAT(string nome, char saida[11]) {
    for (int i = 0; i < 11; i++) saida[i] = ' ';

    int ponto = -1;
    for (int i = 0; i < (int)nome.size(); i++) {
        if (nome[i] == '.') { ponto = i; break; }
    }

    string base, ext;
    if (ponto == -1) {
        base = nome;
    } else {
        base = nome.substr(0, ponto);
        ext = nome.substr(ponto + 1);
    }

    for (int i = 0; i < (int)base.size() && i < 8; i++)
        saida[i] = toupper(base[i]);
    for (int i = 0; i < (int)ext.size() && i < 3; i++)
        saida[8 + i] = toupper(ext[i]);
}

string nomeDeFAT(unsigned char nome[11]) {
    string resultado = "";
    for (int i = 0; i < 8; i++)
        if (nome[i] != ' ') resultado += nome[i];

    string ext = "";
    for (int i = 8; i < 11; i++)
        if (nome[i] != ' ') ext += nome[i];

    if (ext != "") resultado += "." + ext;
    return resultado;
}

void decodificaData(unsigned short data, int *dia, int *mes, int *ano) {
    *dia = data & 0x1F;
    *mes = (data >> 5) & 0x0F;
    *ano = ((data >> 9) & 0x7F) + 1980;
}

void decodificaHora(unsigned short hora, int *h, int *m, int *s) {
    *s = (hora & 0x1F) * 2;
    *m = (hora >> 5) & 0x3F;
    *h = (hora >> 11) & 0x1F;
}

unsigned short montaData(int dia, int mes, int ano) {
    return (dia & 0x1F) | ((mes & 0x0F) << 5) | (((ano - 1980) & 0x7F) << 9);
}

unsigned short montaHora(int h, int m, int s) {
    return ((s / 2) & 0x1F) | ((m & 0x3F) << 5) | ((h & 0x1F) << 11);
}

unsigned short lerEntradaFAT(int cluster) {
    unsigned short valor;
    fseek(arquivo, inicioFAT + cluster * 2, SEEK_SET);
    fread(&valor, 2, 1, arquivo);
    return valor;
}

void escreveEntradaFAT(int cluster, unsigned short valor) {
    for (int i = 0; i < boot.numFATs; i++) {
        long posicao = (long)(boot.setoresReservados + i * boot.setoresPorFAT) * boot.bytesPorSetor + cluster * 2;
        fseek(arquivo, posicao, SEEK_SET);
        fwrite(&valor, 2, 1, arquivo);
    }
}

void lerBoot() {
    fseek(arquivo, 0, SEEK_SET);
    fread(&boot, sizeof(SetorBoot), 1, arquivo);

    setoresRaiz = (boot.entradasRaiz * 32 + boot.bytesPorSetor - 1) / boot.bytesPorSetor;
    bytesPorCluster = boot.setoresPorCluster * boot.bytesPorSetor;

    inicioFAT = boot.setoresReservados * boot.bytesPorSetor;
    inicioRaiz = (long)(boot.setoresReservados + boot.numFATs * boot.setoresPorFAT) * boot.bytesPorSetor;
    inicioDados = inicioRaiz + setoresRaiz * boot.bytesPorSetor;

    unsigned int totalSetores = boot.totalSetores16;
    if (totalSetores == 0) totalSetores = boot.totalSetores32;

    unsigned int setoresDeDados = totalSetores - (boot.setoresReservados + boot.numFATs * boot.setoresPorFAT + setoresRaiz);
    totalClusters = setoresDeDados / boot.setoresPorCluster;
}

int procurarArquivo(string nome, EntradaDir *achada) {
    char alvo[11];
    nomeParaFAT(nome, alvo);

    fseek(arquivo, inicioRaiz, SEEK_SET);
    for (int i = 0; i < boot.entradasRaiz; i++) {
        EntradaDir e;
        fread(&e, sizeof(EntradaDir), 1, arquivo);

        if (e.nome[0] == 0x00) break;
        if (e.nome[0] == 0xE5) continue;
        if (e.atributos == 0x0F) continue;

        bool igual = true;
        for (int j = 0; j < 11; j++) {
            if (e.nome[j] != (unsigned char)alvo[j]) { igual = false; break; }
        }
        if (igual) {
            if (achada != NULL) *achada = e;
            return i;
        }
    }
    return -1;
}

void listarDisco() {
    printf("\nArquivos no disco:\n");
    printf("------------------------------\n");

    fseek(arquivo, inicioRaiz, SEEK_SET);
    int qtd = 0;
    for (int i = 0; i < boot.entradasRaiz; i++) {
        EntradaDir e;
        fread(&e, sizeof(EntradaDir), 1, arquivo);

        if (e.nome[0] == 0x00) break;
        if (e.nome[0] == 0xE5) continue;
        if (e.atributos == 0x0F) continue;
        if (e.atributos & 0x08) continue;
        if (e.atributos & 0x10) continue;

        string nome = nomeDeFAT(e.nome);
        printf("%s - %u bytes\n", nome.c_str(), e.tamanho);
        qtd++;
    }

    if (qtd == 0) printf("(nenhum arquivo)\n");
    printf("------------------------------\n");
}

void lerConteudo() {
    string nome;
    printf("Nome do arquivo: ");
    cin >> nome;

    EntradaDir e;
    int indice = procurarArquivo(nome, &e);
    if (indice == -1) { printf("Arquivo nao encontrado.\n"); return; }

    printf("\n----- conteudo de %s -----\n", nome.c_str());

    unsigned int restante = e.tamanho;
    unsigned short cluster = e.clusterInicial;
    char *buffer = new char[bytesPorCluster];

    while (cluster >= 2 && cluster < 0xFFF8 && restante > 0) {
        long posicao = inicioDados + (cluster - 2) * bytesPorCluster;
        fseek(arquivo, posicao, SEEK_SET);
        fread(buffer, 1, bytesPorCluster, arquivo);

        unsigned int qtd = restante;
        if (qtd > (unsigned int)bytesPorCluster) qtd = bytesPorCluster;
        fwrite(buffer, 1, qtd, stdout);
        restante -= qtd;

        cluster = lerEntradaFAT(cluster);
    }

    delete[] buffer;
    printf("\n----- fim -----\n");
}

void mostrarAtributos() {
    string nome;
    printf("Nome do arquivo: ");
    cin >> nome;

    EntradaDir e;
    int indice = procurarArquivo(nome, &e);
    if (indice == -1) { printf("Arquivo nao encontrado.\n"); return; }

    int dia, mes, ano, h, m, s;

    printf("\nAtributos de %s:\n", nome.c_str());
    printf("Tamanho: %u bytes\n", e.tamanho);

    decodificaData(e.dataCriacao, &dia, &mes, &ano);
    decodificaHora(e.horaCriacao, &h, &m, &s);
    printf("Criado em:     %02d/%02d/%04d %02d:%02d:%02d\n", dia, mes, ano, h, m, s);

    decodificaData(e.dataModif, &dia, &mes, &ano);
    decodificaHora(e.horaModif, &h, &m, &s);
    printf("Modificado em: %02d/%02d/%04d %02d:%02d:%02d\n", dia, mes, ano, h, m, s);

    printf("Somente leitura: %s\n", (e.atributos & 0x01) ? "sim" : "nao");
    printf("Oculto: %s\n", (e.atributos & 0x02) ? "sim" : "nao");
    printf("Sistema: %s\n", (e.atributos & 0x04) ? "sim" : "nao");
}

void renomear() {
    string atual, novo;
    printf("Nome atual: ");
    cin >> atual;
    printf("Nome novo: ");
    cin >> novo;

    EntradaDir e;
    int indice = procurarArquivo(atual, &e);
    if (indice == -1) { printf("Arquivo nao encontrado.\n"); return; }

    if (procurarArquivo(novo, NULL) != -1) {
        printf("Ja existe um arquivo com esse nome.\n");
        return;
    }

    char novoNome[11];
    nomeParaFAT(novo, novoNome);
    for (int j = 0; j < 11; j++) e.nome[j] = novoNome[j];

    fseek(arquivo, inicioRaiz + indice * sizeof(EntradaDir), SEEK_SET);
    fwrite(&e, sizeof(EntradaDir), 1, arquivo);
    fflush(arquivo);

    printf("Renomeado com sucesso.\n");
}

void inserir() {
    string caminho, nomeDestino;
    printf("Caminho do arquivo externo: ");
    cin >> caminho;
    printf("Nome no disco (formato 8.3): ");
    cin >> nomeDestino;

    FILE *externo = fopen(caminho.c_str(), "rb");
    if (externo == NULL) { printf("Nao consegui abrir o arquivo externo.\n"); return; }

    fseek(externo, 0, SEEK_END);
    unsigned int tamanho = ftell(externo);
    fseek(externo, 0, SEEK_SET);
    char *dados = new char[tamanho > 0 ? tamanho : 1];
    fread(dados, 1, tamanho, externo);
    fclose(externo);

    if (procurarArquivo(nomeDestino, NULL) != -1) {
        printf("Ja existe um arquivo com esse nome.\n");
        delete[] dados;
        return;
    }

    int clustersNecessarios = 0;
    if (tamanho > 0)
        clustersNecessarios = (tamanho + bytesPorCluster - 1) / bytesPorCluster;

    unsigned short *livres = new unsigned short[clustersNecessarios > 0 ? clustersNecessarios : 1];
    int achados = 0;
    for (int c = 2; c <= totalClusters + 1 && achados < clustersNecessarios; c++) {
        if (lerEntradaFAT(c) == 0x0000) {
            livres[achados] = c;
            achados++;
        }
    }
    if (achados < clustersNecessarios) {
        printf("Nao tem espaco suficiente no disco.\n");
        delete[] dados;
        delete[] livres;
        return;
    }

    int indiceLivre = -1;
    fseek(arquivo, inicioRaiz, SEEK_SET);
    for (int i = 0; i < boot.entradasRaiz; i++) {
        EntradaDir e;
        fread(&e, sizeof(EntradaDir), 1, arquivo);
        if (e.nome[0] == 0x00 || e.nome[0] == 0xE5) { indiceLivre = i; break; }
    }
    if (indiceLivre == -1) {
        printf("O diretorio raiz esta cheio.\n");
        delete[] dados;
        delete[] livres;
        return;
    }

    char *buffer = new char[bytesPorCluster];
    for (int i = 0; i < clustersNecessarios; i++) {
        int c = livres[i];

        if (i == clustersNecessarios - 1)
            escreveEntradaFAT(c, 0xFFFF);
        else
            escreveEntradaFAT(c, livres[i + 1]);

        memset(buffer, 0, bytesPorCluster);
        int inicio = i * bytesPorCluster;
        int pedaco = tamanho - inicio;
        if (pedaco > bytesPorCluster) pedaco = bytesPorCluster;
        for (int k = 0; k < pedaco; k++) buffer[k] = dados[inicio + k];

        long posicao = inicioDados + (c - 2) * bytesPorCluster;
        fseek(arquivo, posicao, SEEK_SET);
        fwrite(buffer, 1, bytesPorCluster, arquivo);
    }
    delete[] buffer;

    EntradaDir nova;
    memset(&nova, 0, sizeof(EntradaDir));

    char nomeFAT[11];
    nomeParaFAT(nomeDestino, nomeFAT);
    for (int j = 0; j < 11; j++) nova.nome[j] = nomeFAT[j];

    nova.atributos = 0x20;
    nova.tamanho = tamanho;
    if (clustersNecessarios > 0)
        nova.clusterInicial = livres[0];
    else
        nova.clusterInicial = 0;

    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);
    unsigned short data = montaData(t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    unsigned short hora = montaHora(t->tm_hour, t->tm_min, t->tm_sec);
    nova.dataCriacao = data;
    nova.horaCriacao = hora;
    nova.dataModif = data;
    nova.horaModif = hora;
    nova.dataAcesso = data;

    fseek(arquivo, inicioRaiz + indiceLivre * sizeof(EntradaDir), SEEK_SET);
    fwrite(&nova, sizeof(EntradaDir), 1, arquivo);
    fflush(arquivo);

    printf("Arquivo \"%s\" criado (%u bytes).\n", nomeDestino.c_str(), tamanho);

    delete[] dados;
    delete[] livres;
}

void apagar() {
    string nome;
    printf("Nome do arquivo: ");
    cin >> nome;

    EntradaDir e;
    int indice = procurarArquivo(nome, &e);
    if (indice == -1) { printf("Arquivo nao encontrado.\n"); return; }

    unsigned short cluster = e.clusterInicial;
    while (cluster >= 2 && cluster < 0xFFF8) {
        unsigned short proximo = lerEntradaFAT(cluster);
        escreveEntradaFAT(cluster, 0x0000);
        cluster = proximo;
    }

    e.nome[0] = 0xE5;
    fseek(arquivo, inicioRaiz + indice * sizeof(EntradaDir), SEEK_SET);
    fwrite(&e, sizeof(EntradaDir), 1, arquivo);
    fflush(arquivo);

    printf("Arquivo removido.\n");
}

int main(int argc, char *argv[]) {
    string caminho;

    if (argc >= 2)
        caminho = argv[1];
    else {
        printf("Caminho da imagem FAT16: ");
        cin >> caminho;
    }

    arquivo = fopen(caminho.c_str(), "r+b");
    if (arquivo == NULL) {
        printf("Erro: nao consegui abrir a imagem.\n");
        return 1;
    }

    lerBoot();
    printf("Imagem aberta.\n");

    int opcao = -1;
    while (opcao != 0) {
        printf("\n===== MENU FAT16 =====\n");
        printf("1 - Listar arquivos do disco\n");
        printf("2 - Mostrar conteudo de um arquivo\n");
        printf("3 - Mostrar atributos de um arquivo\n");
        printf("4 - Renomear um arquivo\n");
        printf("5 - Inserir um arquivo novo\n");
        printf("6 - Apagar um arquivo\n");
        printf("0 - Sair\n");
        printf("Opcao: ");

        if (!(cin >> opcao)) {
            cin.clear();
            cin.ignore(1000, '\n');
            printf("Opcao invalida.\n");
            continue;
        }

        if (opcao == 1) listarDisco();
        else if (opcao == 2) lerConteudo();
        else if (opcao == 3) mostrarAtributos();
        else if (opcao == 4) renomear();
        else if (opcao == 5) inserir();
        else if (opcao == 6) apagar();
        else if (opcao == 0) printf("Saindo...\n");
        else printf("Opcao invalida.\n");
    }

    fclose(arquivo);
    return 0;
}
