#ifndef MORADOR_H
#define MORADOR_H

typedef void *Morador;

Morador morador_create(const char *cpf,
                       const char *nome,
                       const char *sobrenome,
                       char sexo,
                       const char *nasc);

void morador_destroy(Morador m);

/* dados básicos */
const char *morador_get_cpf(Morador m);
const char *morador_get_nome(Morador m);
const char *morador_get_sobrenome(Morador m);
char morador_get_sexo(Morador m);
const char *morador_get_nasc(Morador m);

/* endereço */
void morador_set_endereco(Morador m,
                          const char *cep,
                          char face,
                          int num,
                          const char *complemento);

int morador_tem_endereco(Morador m);

const char *morador_get_cep(Morador m);
char morador_get_face(Morador m);
int morador_get_num(Morador m);
const char *morador_get_compl(Morador m);

/* ações */
void morador_remove_endereco(Morador m);

#endif
