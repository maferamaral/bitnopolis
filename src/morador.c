#include "morador.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    char cpf[32];
    char nome[64];
    char sobrenome[64];
    char sexo;
    char nasc[16];

    int temEndereco;

    char cep[32];
    char face;
    int num;
    char complemento[64];

} MoradorImpl;

Morador morador_create(const char *cpf,
                       const char *nome,
                       const char *sobrenome,
                       char sexo,
                       const char *nasc)
{
    MoradorImpl *m = (MoradorImpl *)malloc(sizeof(MoradorImpl));
    if (!m)
        return NULL;

    strncpy(m->cpf, cpf, 31);
    m->cpf[31] = '\0';

    strncpy(m->nome, nome, 63);
    m->nome[63] = '\0';

    strncpy(m->sobrenome, sobrenome, 63);
    m->sobrenome[63] = '\0';

    m->sexo = sexo;

    strncpy(m->nasc, nasc, 15);
    m->nasc[15] = '\0';

    m->temEndereco = 0;

    return m;
}

void morador_destroy(Morador m)
{
    if (!m)
        return;
    free(m);
}

/* getters */

const char *morador_get_cpf(Morador m)
{
    return ((MoradorImpl *)m)->cpf;
}

const char *morador_get_nome(Morador m)
{
    return ((MoradorImpl *)m)->nome;
}

const char *morador_get_sobrenome(Morador m)
{
    return ((MoradorImpl *)m)->sobrenome;
}

char morador_get_sexo(Morador m)
{
    return ((MoradorImpl *)m)->sexo;
}

const char *morador_get_nasc(Morador m)
{
    return ((MoradorImpl *)m)->nasc;
}

/* endereço */

void morador_set_endereco(Morador m,
                          const char *cep,
                          char face,
                          int num,
                          const char *complemento)
{
    MoradorImpl *mm = (MoradorImpl *)m;

    strncpy(mm->cep, cep, 31);
    mm->cep[31] = '\0';

    mm->face = face;
    mm->num = num;

    strncpy(mm->complemento, complemento, 63);
    mm->complemento[63] = '\0';

    mm->temEndereco = 1;
}

int morador_tem_endereco(Morador m)
{
    return ((MoradorImpl *)m)->temEndereco;
}

const char *morador_get_cep(Morador m)
{
    return ((MoradorImpl *)m)->cep;
}

char morador_get_face(Morador m)
{
    return ((MoradorImpl *)m)->face;
}

int morador_get_num(Morador m)
{
    return ((MoradorImpl *)m)->num;
}

const char *morador_get_compl(Morador m)
{
    return ((MoradorImpl *)m)->complemento;
}

/* ações */

void morador_remove_endereco(Morador m)
{
    ((MoradorImpl *)m)->temEndereco = 0;
}
