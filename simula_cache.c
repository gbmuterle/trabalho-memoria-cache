#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int rotulo;
    int dirty;
    int lru;
} LinhaCache;

int potencia_de_2(int x) {
    if (x < 1) return 0;
    while (x % 2 == 0)
        x = x / 2;
    return x == 1;
}

int qtd_bits(int x) {
    int bits = 0;
    while (x > 1) {
        x = x / 2;
        bits = bits + 1;
    }
    return bits;
}

int procura_linha(LinhaCache *cache, int base, int assoc, int rotulo) {
    int i;
    for (i = base; i < base + assoc; i++) {
        if (cache[i].rotulo == rotulo)
            return i;
    }
    return -1;
}

int escolhe_linha(LinhaCache *cache, int base, int assoc, int aleatoria) {
    int i;
    for (i = base; i < base + assoc; i++) {
        if (cache[i].rotulo == -1)
            return i;
    }
    if (aleatoria) {
        return base + rand() % assoc;
    }
    int linha = base;
    for (i = base + 1; i < base + assoc; i++) {
        if (cache[i].lru < cache[linha].lru)
            linha = i;
    }
    return linha;
}

int main(int argc, char *argv[]) {
    if (argc < 8) {
        printf("uso: %s escrita tam_linha num_linhas assoc hit subst(LRU|A) tempo_mp [arquivo]\n", argv[0]);
        return 1;
    }

    int politica = atoi(argv[1]);
    int tam_linha = atoi(argv[2]);
    int num_linhas = atoi(argv[3]);
    int assoc = atoi(argv[4]);
    double hit = atof(argv[5]);
    double tempo_mp = atof(argv[7]);

    int aleatoria = 0;
    if (strcmp(argv[6], "LRU") != 0)
        aleatoria = 1;

    if (!potencia_de_2(tam_linha) || !potencia_de_2(num_linhas) || !potencia_de_2(assoc) || assoc > num_linhas) {
        printf("tam_linha, num_linhas e assoc devem ser potencias de 2, com assoc <= num_linhas\n");
        return 1;
    }

    int num_conjuntos = num_linhas / assoc;
    int bits_offset = qtd_bits(tam_linha);
    int bits_indice = qtd_bits(num_conjuntos);

    srand(time(NULL));

    LinhaCache *cache = malloc(num_linhas * sizeof(LinhaCache));
    int i;
    for (i = 0; i < num_linhas; i++) {
        cache[i].rotulo = -1;
        cache[i].dirty = 0;
        cache[i].lru = 0;
    }

    FILE *entrada;
    if (argc >= 9)
        entrada = fopen(argv[8], "r");
    else
        entrada = stdin;
    if (entrada == NULL) {
        printf("erro ao abrir o arquivo\n");
        return 1;
    }

    int leituras = 0, escritas = 0;
    int acerto_leitura = 0, falta_leitura = 0;
    int acerto_escrita = 0, falta_escrita = 0;
    int leituras_mp = 0, escritas_mp = 0;
    int tempo = 0;

    unsigned int endereco;
    char op;
    while (fscanf(entrada, "%x %c", &endereco, &op) == 2) {
        int indice = (endereco >> bits_offset) % num_conjuntos;
        int rotulo = endereco >> (bits_offset + bits_indice);
        int base = indice * assoc;
        tempo = tempo + 1;

        int pos = procura_linha(cache, base, assoc, rotulo);

        if (op == 'W') {
            escritas = escritas + 1;
            if (politica == 0) {
                // write-through
                escritas_mp = escritas_mp + 1;
                if (pos >= 0) {
                    acerto_escrita = acerto_escrita + 1;
                    cache[pos].lru = tempo;
                } else {
                    falta_escrita = falta_escrita + 1;
                }
            } else {
                // write-back
                if (pos >= 0) {
                    acerto_escrita = acerto_escrita + 1;
                    cache[pos].dirty = 1;
                    cache[pos].lru = tempo;
                } else {
                    falta_escrita = falta_escrita + 1;
                    int linha = escolhe_linha(cache, base, assoc, aleatoria);
                    if (cache[linha].dirty == 1)
                        escritas_mp = escritas_mp + 1;
                    leituras_mp = leituras_mp + 1;
                    cache[linha].rotulo = rotulo;
                    cache[linha].dirty = 1;
                    cache[linha].lru = tempo;
                }
            }
        } else {
            leituras = leituras + 1;
            if (pos >= 0) {
                acerto_leitura = acerto_leitura + 1;
                cache[pos].lru = tempo;
            } else {
                falta_leitura = falta_leitura + 1;
                int linha = escolhe_linha(cache, base, assoc, aleatoria);
                if (politica == 1 && cache[linha].dirty == 1)
                    escritas_mp = escritas_mp + 1;
                leituras_mp = leituras_mp + 1;
                cache[linha].rotulo = rotulo;
                cache[linha].dirty = 0;
                cache[linha].lru = tempo;
            }
        }
    }
    if (entrada != stdin)
        fclose(entrada);

    if (politica == 1) {
        for (i = 0; i < num_linhas; i++) {
            if (cache[i].dirty == 1)
                escritas_mp = escritas_mp + 1;
        }
    }

    int total = leituras + escritas;
    int acertos = total - falta_leitura - falta_escrita;
    double taxa_leitura = (double) acerto_leitura / leituras;
    double taxa_escrita = (double) acerto_escrita / escritas;
    double taxa_global = (double) acertos / total;
    double tempo_medio = hit + (1.0 - taxa_global) * tempo_mp;

    if (politica == 0)
        printf("Politica de escrita: write-through\n");
    else
        printf("Politica de escrita: write-back\n");
    printf("Tamanho da linha: %d bytes\n", tam_linha);
    printf("Numero de linhas: %d\n", num_linhas);
    printf("Associatividade: %d\n", assoc);
    printf("Numero de conjuntos: %d\n", num_conjuntos);
    printf("Tamanho da cache: %d bytes\n", num_linhas * tam_linha);
    if (aleatoria == 1)
        printf("Politica de substituicao: Aleatoria\n");
    else
        printf("Politica de substituicao: LRU\n");
    printf("Tempo de acerto: %.4f ns\n", hit);
    printf("Tempo da memoria principal: %.4f ns\n", tempo_mp);
    printf("Enderecos de leitura: %d\n", leituras);
    printf("Enderecos de escrita: %d\n", escritas);
    printf("Total de enderecos: %d\n", total);
    printf("Leituras na memoria principal: %d\n", leituras_mp);
    printf("Escritas na memoria principal: %d\n", escritas_mp);
    printf("Taxa de acerto leitura: %.4f (%d de %d)\n", taxa_leitura, acerto_leitura, leituras);
    printf("Taxa de acerto escrita: %.4f (%d de %d)\n", taxa_escrita, acerto_escrita, escritas);
    printf("Taxa de acerto global: %.4f (%d de %d)\n", taxa_global, acertos, total);
    printf("Tempo medio de acesso: %.4f ns\n", tempo_medio);

    free(cache);
    return 0;
}