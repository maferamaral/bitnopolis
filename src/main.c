#include "../include/extensible_hash_file.h"
#include "../include/geo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PATH_MAX_LEN 512
#define NAME_MAX_LEN 256

typedef struct
{
    char dir_entrada[PATH_MAX_LEN];
    char arq_geo[NAME_MAX_LEN];
    char arq_pm[NAME_MAX_LEN];
    char arq_qry[NAME_MAX_LEN];
    char dir_saida[PATH_MAX_LEN];

    int tem_e;
    int tem_pm;
    int tem_qry;
} Parametros;

static void inicializar_parametros(Parametros *p)
{
    strcpy(p->dir_entrada, ".");
    p->arq_geo[0] = '\0';
    p->arq_pm[0] = '\0';
    p->arq_qry[0] = '\0';
    p->dir_saida[0] = '\0';

    p->tem_e = 0;
    p->tem_pm = 0;
    p->tem_qry = 0;
}

static void copiar_string(char *dest, size_t tam, const char *src)
{
    strncpy(dest, src, tam - 1);
    dest[tam - 1] = '\0';
}

static int ler_parametros(int argc, char **argv, Parametros *p)
{
    int i;

    inicializar_parametros(p);

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-e") == 0)
        {
            if (i + 1 >= argc)
                return 0;

            copiar_string(p->dir_entrada, sizeof(p->dir_entrada), argv[++i]);
            p->tem_e = 1;
        }
        else if (strcmp(argv[i], "-f") == 0)
        {
            if (i + 1 >= argc)
                return 0;

            copiar_string(p->arq_geo, sizeof(p->arq_geo), argv[++i]);
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 >= argc)
                return 0;

            copiar_string(p->dir_saida, sizeof(p->dir_saida), argv[++i]);
        }
        else if (strcmp(argv[i], "-q") == 0)
        {
            if (i + 1 >= argc)
                return 0;

            copiar_string(p->arq_qry, sizeof(p->arq_qry), argv[++i]);
            p->tem_qry = 1;
        }
        else if (strcmp(argv[i], "-pm") == 0)
        {
            if (i + 1 >= argc)
                return 0;

            copiar_string(p->arq_pm, sizeof(p->arq_pm), argv[++i]);
            p->tem_pm = 1;
        }
        else
        {
            fprintf(stderr, "Parametro desconhecido: %s\n", argv[i]);
            return 0;
        }
    }

    if (p->arq_geo[0] == '\0')
    {
        fprintf(stderr, "Erro: parametro -f arq.geo obrigatorio.\n");
        return 0;
    }

    if (p->dir_saida[0] == '\0')
    {
        fprintf(stderr, "Erro: parametro -o dir obrigatorio.\n");
        return 0;
    }

    return 1;
}

static void juntar_caminho(char *dest, size_t tam, const char *dir, const char *arq)
{
    size_t len;

    if (dir == NULL || dir[0] == '\0' || strcmp(dir, ".") == 0)
    {
        snprintf(dest, tam, "%s", arq);
        return;
    }

    len = strlen(dir);

    if (dir[len - 1] == '/' || dir[len - 1] == '\\')
        snprintf(dest, tam, "%s%s", dir, arq);
    else
        snprintf(dest, tam, "%s/%s", dir, arq);
}

static void obter_nome_base(char *dest, size_t tam, const char *arq)
{
    const char *inicio;
    char temp[NAME_MAX_LEN];
    char *ponto;

    inicio = strrchr(arq, '/');
    if (inicio == NULL)
        inicio = strrchr(arq, '\\');

    if (inicio != NULL)
        inicio++;
    else
        inicio = arq;

    copiar_string(temp, sizeof(temp), inicio);

    ponto = strrchr(temp, '.');
    if (ponto != NULL)
        *ponto = '\0';

    copiar_string(dest, tam, temp);
}

static void montar_saida_svg_base(char *dest, size_t tam, const Parametros *p)
{
    char base[NAME_MAX_LEN];
    char nome[NAME_MAX_LEN];

    obter_nome_base(base, sizeof(base), p->arq_geo);
    snprintf(nome, sizeof(nome), "%s.svg", base);
    juntar_caminho(dest, tam, p->dir_saida, nome);
}

static void montar_saida_hash(char *dest, size_t tam, const Parametros *p, const char *nome)
{
    juntar_caminho(dest, tam, p->dir_saida, nome);
}

static void imprimir_uso(void)
{
    fprintf(stderr, "Uso:\n");
    fprintf(stderr, "  ted [-e path] -f arq.geo [-pm arq.pm] [-q arq.qry] -o dir\n");
}

int main(int argc, char **argv)
{
    Parametros params;

    char caminho_geo[PATH_MAX_LEN];
    char caminho_pm[PATH_MAX_LEN];
    char caminho_qry[PATH_MAX_LEN];
    char caminho_svg_base[PATH_MAX_LEN];

    char caminho_quadras_hf[PATH_MAX_LEN];
    char caminho_habitantes_hf[PATH_MAX_LEN];

    HashExtFile hf_quadras = NULL;
    HashExtFile hf_habitantes = NULL;

    HashExtStatus st;
    int geo_status;

    if (!ler_parametros(argc, argv, &params))
    {
        imprimir_uso();
        return EXIT_FAILURE;
    }

    juntar_caminho(caminho_geo, sizeof(caminho_geo), params.dir_entrada, params.arq_geo);
    montar_saida_svg_base(caminho_svg_base, sizeof(caminho_svg_base), &params);

    montar_saida_hash(caminho_quadras_hf, sizeof(caminho_quadras_hf), &params, "quadras.hf");
    montar_saida_hash(caminho_habitantes_hf, sizeof(caminho_habitantes_hf), &params, "habitantes.hf");

    st = hef_create(caminho_quadras_hf,
                    4,
                    (uint32_t)geo_quadra_record_size(),
                    1,
                    &hf_quadras);

    if (st != HEF_OK)
    {
        fprintf(stderr, "Erro ao criar hashfile de quadras: %s\n", hef_status_str(st));
        return EXIT_FAILURE;
    }

    /*
     * Por enquanto, o hashfile de habitantes ainda fica criado
     * para a proxima etapa do projeto: leitura do .pm.
     * Depois trocaremos sizeof(int) pelo tamanho real do registro de habitante.
     */
    st = hef_create(caminho_habitantes_hf,
                    4,
                    sizeof(int),
                    1,
                    &hf_habitantes);

    if (st != HEF_OK)
    {
        fprintf(stderr, "Erro ao criar hashfile de habitantes: %s\n", hef_status_str(st));
        hef_close(&hf_quadras);
        return EXIT_FAILURE;
    }

    geo_status = geo_processar_arquivo(caminho_geo, hf_quadras, caminho_svg_base);

    if (geo_status != GEO_OK)
    {
        fprintf(stderr, "Erro ao processar arquivo .geo: codigo %d\n", geo_status);
        hef_close(&hf_habitantes);
        hef_close(&hf_quadras);
        return EXIT_FAILURE;
    }

    if (params.tem_pm)
    {
        juntar_caminho(caminho_pm, sizeof(caminho_pm), params.dir_entrada, params.arq_pm);

        /*
         * Proxima etapa:
         * processar_pm(caminho_pm, hf_habitantes, hf_quadras);
         */
        printf("Arquivo .pm informado: %s\n", caminho_pm);
    }

    if (params.tem_qry)
    {
        juntar_caminho(caminho_qry, sizeof(caminho_qry), params.dir_entrada, params.arq_qry);

        /*
         * Proxima etapa:
         * processar_qry(caminho_qry, hf_quadras, hf_habitantes, ...);
         */
        printf("Arquivo .qry informado: %s\n", caminho_qry);
    }

    st = hef_close(&hf_habitantes);
    if (st != HEF_OK)
    {
        fprintf(stderr, "Erro ao fechar hashfile de habitantes: %s\n", hef_status_str(st));
        hef_close(&hf_quadras);
        return EXIT_FAILURE;
    }

    st = hef_close(&hf_quadras);
    if (st != HEF_OK)
    {
        fprintf(stderr, "Erro ao fechar hashfile de quadras: %s\n", hef_status_str(st));
        return EXIT_FAILURE;
    }

    printf("Processamento concluido.\n");
    printf("SVG gerado: %s\n", caminho_svg_base);
    printf("Hash quadras: %s\n", caminho_quadras_hf);
    printf("Hash habitantes: %s\n", caminho_habitantes_hf);

    return EXIT_SUCCESS;
}