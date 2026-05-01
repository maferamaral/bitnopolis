#include "../include/pm.h"
#include "../include/geo.h"
#include "../include/extensible_hash_file.h"

#include <stdio.h>
#include <string.h>

#define PM_MAX_LINE 512
#define PM_CPF_LEN 32
#define PM_NOME_LEN 64
#define PM_NASC_LEN 16
#define PM_CEP_LEN 64
#define PM_COMPL_LEN 64

typedef struct
{
    char cpf[PM_CPF_LEN];
    char nome[PM_NOME_LEN];
    char sobrenome[PM_NOME_LEN];
    char sexo;
    char nasc[PM_NASC_LEN];

    int tem_endereco;

    char cep[PM_CEP_LEN];
    char face;
    int num;
    char compl[PM_COMPL_LEN];

    int vivo;
} PmHabitanteRecord;

static void copiar_string(char *dest, size_t tam, const char *src)
{
    if (dest == NULL || tam == 0)
        return;

    if (src == NULL)
    {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, tam - 1);
    dest[tam - 1] = '\0';
}

size_t pm_habitante_record_size(void)
{
    return sizeof(PmHabitanteRecord);
}

static int existe_quadra(HashExtFile hf_quadras, const char *cep)
{
    int st;
    char out_cep[PM_CEP_LEN];
    double x, y, w, h, sw;
    char fill[32], stroke[32];

    st = geo_buscar_quadra(hf_quadras,
                           cep,
                           out_cep,
                           sizeof(out_cep),
                           &x,
                           &y,
                           &w,
                           &h,
                           &sw,
                           fill,
                           sizeof(fill),
                           stroke,
                           sizeof(stroke));

    return st == GEO_OK;
}

static int processar_p(char *linha, HashExtFile hf_habitantes, int numero_linha)
{
    PmHabitanteRecord h;
    char cpf[PM_CPF_LEN];
    char nome[PM_NOME_LEN];
    char sobrenome[PM_NOME_LEN];
    char sexo;
    char nasc[PM_NASC_LEN];
    HashExtStatus st;

    if (sscanf(linha, "%*s %31s %63s %63s %c %15s",
               cpf, nome, sobrenome, &sexo, nasc) != 5)
    {
        fprintf(stderr, "Erro no .pm linha %d: comando p invalido\n", numero_linha);
        return PM_ERR_PARSE;
    }

    memset(&h, 0, sizeof(h));

    copiar_string(h.cpf, sizeof(h.cpf), cpf);
    copiar_string(h.nome, sizeof(h.nome), nome);
    copiar_string(h.sobrenome, sizeof(h.sobrenome), sobrenome);
    h.sexo = sexo;
    copiar_string(h.nasc, sizeof(h.nasc), nasc);

    h.tem_endereco = 0;
    h.vivo = 1;

    st = hef_insert(hf_habitantes, h.cpf, &h);

    if (st == HEF_ERR_DUPLICATE_KEY)
        st = hef_update(hf_habitantes, h.cpf, &h);

    if (st != HEF_OK)
    {
        fprintf(stderr,
                "Erro no .pm linha %d: falha ao gravar habitante %s (%s)\n",
                numero_linha,
                h.cpf,
                hef_status_str(st));
        return PM_ERR_HASH;
    }

    return PM_OK;
}

static int processar_m(char *linha,
                       HashExtFile hf_habitantes,
                       HashExtFile hf_quadras,
                       int numero_linha)
{
    char cpf[PM_CPF_LEN];
    char cep[PM_CEP_LEN];
    char face;
    int num;
    char compl[PM_COMPL_LEN];

    PmHabitanteRecord h;
    HashExtStatus st;

    if (sscanf(linha, "%*s %31s %63s %c %d %63s",
               cpf, cep, &face, &num, compl) != 5)
    {
        fprintf(stderr, "Erro no .pm linha %d: comando m invalido\n", numero_linha);
        return PM_ERR_PARSE;
    }

    st = hef_get(hf_habitantes, cpf, &h);

    if (st == HEF_ERR_NOT_FOUND)
    {
        fprintf(stderr,
                "Erro no .pm linha %d: morador com CPF %s nao foi cadastrado antes com comando p\n",
                numero_linha,
                cpf);
        return PM_ERR_NOT_FOUND;
    }

    if (st != HEF_OK)
    {
        fprintf(stderr,
                "Erro no .pm linha %d: falha ao buscar habitante %s (%s)\n",
                numero_linha,
                cpf,
                hef_status_str(st));
        return PM_ERR_HASH;
    }

    if (!existe_quadra(hf_quadras, cep))
    {
        fprintf(stderr,
                "Erro no .pm linha %d: CEP %s nao existe no arquivo .geo\n",
                numero_linha,
                cep);
        return PM_ERR_NOT_FOUND;
    }

    h.tem_endereco = 1;
    copiar_string(h.cep, sizeof(h.cep), cep);
    h.face = face;
    h.num = num;
    copiar_string(h.compl, sizeof(h.compl), compl);

    st = hef_update(hf_habitantes, h.cpf, &h);

    if (st != HEF_OK)
    {
        fprintf(stderr,
                "Erro no .pm linha %d: falha ao atualizar endereco de %s (%s)\n",
                numero_linha,
                cpf,
                hef_status_str(st));
        return PM_ERR_HASH;
    }

    return PM_OK;
}

int processar_pm(const char *pm_path,
                 HashExtFile hf_habitantes,
                 HashExtFile hf_quadras)
{
    FILE *f;
    char linha[PM_MAX_LINE];
    char comando[16];
    int numero_linha = 0;
    int status = PM_OK;

    if (pm_path == NULL || hf_habitantes == NULL || hf_quadras == NULL)
        return PM_ERR_INVALID_ARG;

    f = fopen(pm_path, "r");
    if (f == NULL)
        return PM_ERR_IO;

    while (fgets(linha, sizeof(linha), f) != NULL)
    {
        numero_linha++;

        if (sscanf(linha, "%15s", comando) != 1)
            continue;

        if (comando[0] == '#')
            continue;

        if (strcmp(comando, "p") == 0)
        {
            status = processar_p(linha, hf_habitantes, numero_linha);
        }
        else if (strcmp(comando, "m") == 0)
        {
            status = processar_m(linha, hf_habitantes, hf_quadras, numero_linha);
        }
        else
        {
            fprintf(stderr,
                    "Aviso no .pm linha %d: comando ignorado: %s\n",
                    numero_linha,
                    comando);
            status = PM_OK;
        }

        if (status != PM_OK)
            break;
    }

    if (fclose(f) != 0 && status == PM_OK)
        status = PM_ERR_IO;

    return status;
}