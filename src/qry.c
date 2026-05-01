#include "../include/qry.h"
#include "../include/geo.h"
#include "../include/extensible_hash_file.h"

#include <stdio.h>
#include <string.h>

#define QRY_MAX_LINE 512
#define CPF_LEN 32
#define NOME_LEN 64
#define NASC_LEN 16
#define CEP_LEN 64
#define COMPL_LEN 64

typedef struct
{
    char cpf[CPF_LEN];
    char nome[NOME_LEN];
    char sobrenome[NOME_LEN];
    char sexo;
    char nasc[NASC_LEN];

    int tem_endereco;

    char cep[CEP_LEN];
    char face;
    int num;
    char compl[COMPL_LEN];

    int vivo;
} HabitanteRecord;

typedef struct
{
    FILE *txt;
    int total_habitantes;
    int total_moradores;
    int homens;
    int mulheres;
    int moradores_homens;
    int moradores_mulheres;
    int sem_teto;
    int sem_teto_homens;
    int sem_teto_mulheres;
} CensoCtx;

typedef struct
{
    const char *cep;
    int n;
    int s;
    int l;
    int o;
    int total;
} PqCtx;

typedef struct
{
    const char *cep;
    FILE *txt;
    HashExtFile hf_habitantes;
    int removidos;
} RqCtx;

static int cmd_censo(FILE *txt, HashExtFile hf_habitantes, const char *linha);
static int cmd_pq(FILE *txt,
                  FILE *svg,
                  HashExtFile hf_quadras,
                  HashExtFile hf_habitantes,
                  const char *linha);
static int cmd_rq(FILE *txt,
                  FILE *svg,
                  HashExtFile hf_quadras,
                  HashExtFile hf_habitantes,
                  const char *linha);

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

static int buscar_habitante(HashExtFile hf, const char *cpf, HabitanteRecord *out)
{
    HashExtStatus st = hef_get(hf, cpf, out);

    if (st == HEF_ERR_NOT_FOUND)
        return QRY_ERR_NOT_FOUND;

    if (st != HEF_OK)
        return QRY_ERR_HASH;

    return QRY_OK;
}

static int atualizar_habitante(HashExtFile hf, const HabitanteRecord *h)
{
    HashExtStatus st = hef_update(hf, h->cpf, h);

    if (st != HEF_OK)
        return QRY_ERR_HASH;

    return QRY_OK;
}

static int inserir_habitante(HashExtFile hf, const HabitanteRecord *h)
{
    HashExtStatus st = hef_insert(hf, h->cpf, h);

    if (st == HEF_ERR_DUPLICATE_KEY)
        st = hef_update(hf, h->cpf, h);

    if (st != HEF_OK)
        return QRY_ERR_HASH;

    return QRY_OK;
}

static int buscar_quadra(HashExtFile hf_quadras,
                         const char *cep,
                         double *x,
                         double *y,
                         double *w,
                         double *h)
{
    char out_cep[CEP_LEN];
    double sw;
    char fill[32];
    char stroke[32];

    int st = geo_buscar_quadra(hf_quadras,
                               cep,
                               out_cep,
                               sizeof(out_cep),
                               x,
                               y,
                               w,
                               h,
                               &sw,
                               fill,
                               sizeof(fill),
                               stroke,
                               sizeof(stroke));

    if (st == GEO_ERR_NOT_FOUND)
        return QRY_ERR_NOT_FOUND;

    if (st != GEO_OK)
        return QRY_ERR_HASH;

    return QRY_OK;
}

static void calcular_ponto_endereco(double x,
                                    double y,
                                    double w,
                                    double h,
                                    char face,
                                    int num,
                                    double *px,
                                    double *py)
{
    switch (face)
    {
    case 'N':
    case 'n':
        *px = x + w - num;
        *py = y;
        break;

    case 'S':
    case 's':
        *px = x + w - num;
        *py = y + h;
        break;

    case 'L':
    case 'l':
        *px = x + w;
        *py = y + h - num;
        break;

    case 'O':
    case 'o':
        *px = x;
        *py = y + h - num;
        break;

    default:
        *px = x;
        *py = y;
        break;
    }
}

static void escrever_dados_habitante(FILE *txt, const HabitanteRecord *h)
{
    fprintf(txt, "cpf: %s\n", h->cpf);
    fprintf(txt, "nome: %s %s\n", h->nome, h->sobrenome);
    fprintf(txt, "sexo: %c\n", h->sexo);
    fprintf(txt, "nascimento: %s\n", h->nasc);
    fprintf(txt, "situacao: %s\n", h->vivo ? "vivo" : "falecido");

    if (h->tem_endereco)
    {
        fprintf(txt, "endereco: %s/%c/%d/%s\n",
                h->cep, h->face, h->num, h->compl);
    }
    else
    {
        fprintf(txt, "endereco: sem-teto\n");
    }
}

static void svg_inicio(FILE *svg)
{
    fprintf(svg, "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n");
}

static void svg_fim(FILE *svg)
{
    fprintf(svg, "</svg>\n");
}

static void svg_cruz(FILE *svg, double x, double y)
{
    fprintf(svg,
            "<line x1=\"%.2lf\" y1=\"%.2lf\" x2=\"%.2lf\" y2=\"%.2lf\" stroke=\"red\" stroke-width=\"2\" />\n",
            x - 4, y - 4, x + 4, y + 4);
    fprintf(svg,
            "<line x1=\"%.2lf\" y1=\"%.2lf\" x2=\"%.2lf\" y2=\"%.2lf\" stroke=\"red\" stroke-width=\"2\" />\n",
            x - 4, y + 4, x + 4, y - 4);
}

static void svg_quadrado_mudanca(FILE *svg, double x, double y, const char *cpf)
{
    fprintf(svg,
            "<rect x=\"%.2lf\" y=\"%.2lf\" width=\"10\" height=\"10\" fill=\"red\" stroke=\"black\" />\n",
            x - 5, y - 5);
    fprintf(svg,
            "<text x=\"%.2lf\" y=\"%.2lf\" font-size=\"4\" fill=\"white\">%s</text>\n",
            x - 4, y + 1, cpf);
}

static void svg_circulo_despejo(FILE *svg, double x, double y)
{
    fprintf(svg,
            "<circle cx=\"%.2lf\" cy=\"%.2lf\" r=\"5\" fill=\"black\" />\n",
            x, y);
}

static int cmd_h(FILE *txt, HashExtFile hf_habitantes, const char *linha)
{
    char cpf[CPF_LEN];
    HabitanteRecord h;

    if (sscanf(linha, "%*s %31s", cpf) != 1)
        return QRY_ERR_PARSE;

    fprintf(txt, "[*] %s", linha);

    if (buscar_habitante(hf_habitantes, cpf, &h) != QRY_OK)
    {
        fprintf(txt, "habitante nao encontrado: %s\n", cpf);
        return QRY_OK;
    }

    escrever_dados_habitante(txt, &h);
    return QRY_OK;
}

static int cmd_nasc(FILE *txt, HashExtFile hf_habitantes, const char *linha)
{
    HabitanteRecord h;
    char cpf[CPF_LEN], nome[NOME_LEN], sobrenome[NOME_LEN], nasc[NASC_LEN];
    char sexo;

    if (sscanf(linha, "%*s %31s %63s %63s %c %15s",
               cpf, nome, sobrenome, &sexo, nasc) != 5)
        return QRY_ERR_PARSE;

    memset(&h, 0, sizeof(h));
    copiar_string(h.cpf, sizeof(h.cpf), cpf);
    copiar_string(h.nome, sizeof(h.nome), nome);
    copiar_string(h.sobrenome, sizeof(h.sobrenome), sobrenome);
    h.sexo = sexo;
    copiar_string(h.nasc, sizeof(h.nasc), nasc);
    h.tem_endereco = 0;
    h.vivo = 1;

    fprintf(txt, "[*] %s", linha);
    fprintf(txt, "habitante cadastrado: %s %s (%s)\n", h.nome, h.sobrenome, h.cpf);

    return inserir_habitante(hf_habitantes, &h);
}

static int cmd_mud(FILE *txt,
                   FILE *svg,
                   HashExtFile hf_quadras,
                   HashExtFile hf_habitantes,
                   const char *linha)
{
    char cpf[CPF_LEN], cep[CEP_LEN], compl[COMPL_LEN];
    char face;
    int num;
    HabitanteRecord h;
    double x, y, w, alt, px, py;
    int st;

    if (sscanf(linha, "%*s %31s %63s %c %d %63s",
               cpf, cep, &face, &num, compl) != 5)
        return QRY_ERR_PARSE;

    fprintf(txt, "[*] %s", linha);

    st = buscar_habitante(hf_habitantes, cpf, &h);
    if (st != QRY_OK)
    {
        fprintf(txt, "habitante nao encontrado: %s\n", cpf);
        return QRY_OK;
    }

    st = buscar_quadra(hf_quadras, cep, &x, &y, &w, &alt);
    if (st != QRY_OK)
    {
        fprintf(txt, "quadra nao encontrada: %s\n", cep);
        return QRY_OK;
    }

    h.tem_endereco = 1;
    copiar_string(h.cep, sizeof(h.cep), cep);
    h.face = face;
    h.num = num;
    copiar_string(h.compl, sizeof(h.compl), compl);

    atualizar_habitante(hf_habitantes, &h);

    calcular_ponto_endereco(x, y, w, alt, face, num, &px, &py);
    svg_quadrado_mudanca(svg, px, py, cpf);

    fprintf(txt, "mudanca realizada para %s: %s/%c/%d/%s\n",
            cpf, cep, face, num, compl);

    return QRY_OK;
}

static int cmd_dspj(FILE *txt,
                    FILE *svg,
                    HashExtFile hf_quadras,
                    HashExtFile hf_habitantes,
                    const char *linha)
{
    char cpf[CPF_LEN];
    HabitanteRecord h;
    double x, y, w, alt, px, py;
    int st;

    if (sscanf(linha, "%*s %31s", cpf) != 1)
        return QRY_ERR_PARSE;

    fprintf(txt, "[*] %s", linha);

    st = buscar_habitante(hf_habitantes, cpf, &h);
    if (st != QRY_OK)
    {
        fprintf(txt, "habitante nao encontrado: %s\n", cpf);
        return QRY_OK;
    }

    escrever_dados_habitante(txt, &h);

    if (h.tem_endereco)
    {
        if (buscar_quadra(hf_quadras, h.cep, &x, &y, &w, &alt) == QRY_OK)
        {
            calcular_ponto_endereco(x, y, w, alt, h.face, h.num, &px, &py);
            svg_circulo_despejo(svg, px, py);
        }
    }

    h.tem_endereco = 0;
    atualizar_habitante(hf_habitantes, &h);

    fprintf(txt, "despejo realizado\n");
    return QRY_OK;
}

static int cmd_rip(FILE *txt,
                   FILE *svg,
                   HashExtFile hf_quadras,
                   HashExtFile hf_habitantes,
                   const char *linha)
{
    char cpf[CPF_LEN];
    HabitanteRecord h;
    double x, y, w, alt, px, py;
    int st;

    if (sscanf(linha, "%*s %31s", cpf) != 1)
        return QRY_ERR_PARSE;

    fprintf(txt, "[*] %s", linha);

    st = buscar_habitante(hf_habitantes, cpf, &h);
    if (st != QRY_OK)
    {
        fprintf(txt, "habitante nao encontrado: %s\n", cpf);
        return QRY_OK;
    }

    escrever_dados_habitante(txt, &h);

    if (h.tem_endereco)
    {
        if (buscar_quadra(hf_quadras, h.cep, &x, &y, &w, &alt) == QRY_OK)
        {
            calcular_ponto_endereco(x, y, w, alt, h.face, h.num, &px, &py);
            svg_cruz(svg, px, py);
        }
    }

    h.vivo = 0;
    atualizar_habitante(hf_habitantes, &h);

    fprintf(txt, "obito registrado\n");
    return QRY_OK;
}

int processar_qry(const char *qry_path,
                  HashExtFile hf_quadras,
                  HashExtFile hf_habitantes,
                  const char *txt_path,
                  const char *svg_path)
{
    FILE *qry;
    FILE *txt;
    FILE *svg;
    char linha[QRY_MAX_LINE];
    char comando[32];
    int status = QRY_OK;

    if (qry_path == NULL || hf_quadras == NULL || hf_habitantes == NULL ||
        txt_path == NULL || svg_path == NULL)
        return QRY_ERR_INVALID_ARG;

    qry = fopen(qry_path, "r");
    if (qry == NULL)
        return QRY_ERR_IO;

    txt = fopen(txt_path, "w");
    if (txt == NULL)
    {
        fclose(qry);
        return QRY_ERR_IO;
    }

    svg = fopen(svg_path, "w");
    if (svg == NULL)
    {
        fclose(txt);
        fclose(qry);
        return QRY_ERR_IO;
    }

    svg_inicio(svg);

    while (fgets(linha, sizeof(linha), qry) != NULL)
    {
        if (sscanf(linha, "%31s", comando) != 1)
            continue;

        if (comando[0] == '#')
            continue;

        if (strcmp(comando, "h?") == 0)
            status = cmd_h(txt, hf_habitantes, linha);
        else if (strcmp(comando, "nasc") == 0)
            status = cmd_nasc(txt, hf_habitantes, linha);
        else if (strcmp(comando, "mud") == 0)
            status = cmd_mud(txt, svg, hf_quadras, hf_habitantes, linha);
        else if (strcmp(comando, "dspj") == 0)
            status = cmd_dspj(txt, svg, hf_quadras, hf_habitantes, linha);
        else if (strcmp(comando, "rip") == 0)
            status = cmd_rip(txt, svg, hf_quadras, hf_habitantes, linha);
        else if (strcmp(comando, "censo") == 0)
            status = cmd_censo(txt, hf_habitantes, linha);
        else if (strcmp(comando, "pq") == 0)
            status = cmd_pq(txt, svg, hf_quadras, hf_habitantes, linha);
        else if (strcmp(comando, "rq") == 0)
            status = cmd_rq(txt, svg, hf_quadras, hf_habitantes, linha);
        else
        {
            fprintf(txt, "[*] %s", linha);
            fprintf(txt, "comando desconhecido\n");
            status = QRY_OK;
        }

        if (status != QRY_OK)
            break;
    }

    svg_fim(svg);

    if (fclose(svg) != 0 && status == QRY_OK)
        status = QRY_ERR_IO;

    if (fclose(txt) != 0 && status == QRY_OK)
        status = QRY_ERR_IO;

    if (fclose(qry) != 0 && status == QRY_OK)
        status = QRY_ERR_IO;

    return status;
}

static double porcentagem(int parte, int total)
{
    if (total == 0)
        return 0.0;

    return ((double)parte * 100.0) / (double)total;
}

static int cb_censo(const char *key, const void *value, void *user_data)
{
    CensoCtx *ctx = (CensoCtx *)user_data;
    const HabitanteRecord *h = (const HabitanteRecord *)value;

    (void)key;

    if (!h->vivo)
        return 0;

    ctx->total_habitantes++;

    if (h->sexo == 'M' || h->sexo == 'm')
        ctx->homens++;
    else if (h->sexo == 'F' || h->sexo == 'f')
        ctx->mulheres++;

    if (h->tem_endereco)
    {
        ctx->total_moradores++;

        if (h->sexo == 'M' || h->sexo == 'm')
            ctx->moradores_homens++;
        else if (h->sexo == 'F' || h->sexo == 'f')
            ctx->moradores_mulheres++;
    }
    else
    {
        ctx->sem_teto++;

        if (h->sexo == 'M' || h->sexo == 'm')
            ctx->sem_teto_homens++;
        else if (h->sexo == 'F' || h->sexo == 'f')
            ctx->sem_teto_mulheres++;
    }

    return 0;
}

static int cmd_censo(FILE *txt, HashExtFile hf_habitantes, const char *linha)
{
    CensoCtx ctx;
    HashExtStatus st;

    memset(&ctx, 0, sizeof(ctx));
    ctx.txt = txt;

    fprintf(txt, "[*] %s", linha);

    st = hef_foreach(hf_habitantes, cb_censo, &ctx);
    if (st != HEF_OK)
    {
        fprintf(txt, "erro ao percorrer habitantes: %s\n", hef_status_str(st));
        return QRY_ERR_HASH;
    }

    fprintf(txt, "total de habitantes: %d\n", ctx.total_habitantes);
    fprintf(txt, "total de moradores: %d\n", ctx.total_moradores);
    fprintf(txt, "proporcao moradores/habitantes: %.2lf%%\n",
            porcentagem(ctx.total_moradores, ctx.total_habitantes));

    fprintf(txt, "homens: %d\n", ctx.homens);
    fprintf(txt, "mulheres: %d\n", ctx.mulheres);

    fprintf(txt, "%% habitantes homens: %.2lf%%\n",
            porcentagem(ctx.homens, ctx.total_habitantes));
    fprintf(txt, "%% habitantes mulheres: %.2lf%%\n",
            porcentagem(ctx.mulheres, ctx.total_habitantes));

    fprintf(txt, "%% moradores homens: %.2lf%%\n",
            porcentagem(ctx.moradores_homens, ctx.total_moradores));
    fprintf(txt, "%% moradores mulheres: %.2lf%%\n",
            porcentagem(ctx.moradores_mulheres, ctx.total_moradores));

    fprintf(txt, "total de sem-tetos: %d\n", ctx.sem_teto);
    fprintf(txt, "%% sem-tetos homens: %.2lf%%\n",
            porcentagem(ctx.sem_teto_homens, ctx.sem_teto));
    fprintf(txt, "%% sem-tetos mulheres: %.2lf%%\n",
            porcentagem(ctx.sem_teto_mulheres, ctx.sem_teto));

    return QRY_OK;
}

static int cb_pq(const char *key, const void *value, void *user_data)
{
    PqCtx *ctx = (PqCtx *)user_data;
    const HabitanteRecord *h = (const HabitanteRecord *)value;

    (void)key;

    if (!h->vivo)
        return 0;

    if (!h->tem_endereco)
        return 0;

    if (strcmp(h->cep, ctx->cep) != 0)
        return 0;

    ctx->total++;

    switch (h->face)
    {
    case 'N':
    case 'n':
        ctx->n++;
        break;
    case 'S':
    case 's':
        ctx->s++;
        break;
    case 'L':
    case 'l':
        ctx->l++;
        break;
    case 'O':
    case 'o':
        ctx->o++;
        break;
    default:
        break;
    }

    return 0;
}

static void svg_texto(FILE *svg, double x, double y, int valor)
{
    fprintf(svg,
            "<text x=\"%.2lf\" y=\"%.2lf\" font-size=\"8\" fill=\"blue\">%d</text>\n",
            x, y, valor);
}

static int cmd_pq(FILE *txt,
                  FILE *svg,
                  HashExtFile hf_quadras,
                  HashExtFile hf_habitantes,
                  const char *linha)
{
    char cep[CEP_LEN];
    double x, y, w, h;
    PqCtx ctx;
    HashExtStatus st;

    if (sscanf(linha, "%*s %63s", cep) != 1)
        return QRY_ERR_PARSE;

    fprintf(txt, "[*] %s", linha);

    if (buscar_quadra(hf_quadras, cep, &x, &y, &w, &h) != QRY_OK)
    {
        fprintf(txt, "quadra nao encontrada: %s\n", cep);
        return QRY_OK;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.cep = cep;

    st = hef_foreach(hf_habitantes, cb_pq, &ctx);
    if (st != HEF_OK)
    {
        fprintf(txt, "erro ao percorrer habitantes: %s\n", hef_status_str(st));
        return QRY_ERR_HASH;
    }

    fprintf(txt, "quadra: %s\n", cep);
    fprintf(txt, "face N: %d\n", ctx.n);
    fprintf(txt, "face S: %d\n", ctx.s);
    fprintf(txt, "face L: %d\n", ctx.l);
    fprintf(txt, "face O: %d\n", ctx.o);
    fprintf(txt, "total: %d\n", ctx.total);

    svg_texto(svg, x + w / 2.0, y - 3.0, ctx.n);
    svg_texto(svg, x + w / 2.0, y + h + 10.0, ctx.s);
    svg_texto(svg, x + w + 3.0, y + h / 2.0, ctx.l);
    svg_texto(svg, x - 10.0, y + h / 2.0, ctx.o);
    svg_texto(svg, x + w / 2.0, y + h / 2.0, ctx.total);

    return QRY_OK;
}

static void svg_x_vermelho(FILE *svg, double x, double y)
{
    fprintf(svg,
            "<line x1=\"%.2lf\" y1=\"%.2lf\" x2=\"%.2lf\" y2=\"%.2lf\" stroke=\"red\" stroke-width=\"2\" />\n",
            x - 6, y - 6, x + 6, y + 6);
    fprintf(svg,
            "<line x1=\"%.2lf\" y1=\"%.2lf\" x2=\"%.2lf\" y2=\"%.2lf\" stroke=\"red\" stroke-width=\"2\" />\n",
            x - 6, y + 6, x + 6, y - 6);
}

static int cb_rq(const char *key, const void *value, void *user_data)
{
    RqCtx *ctx = (RqCtx *)user_data;
    HabitanteRecord h;

    (void)key;

    memcpy(&h, value, sizeof(HabitanteRecord));

    if (!h.vivo)
        return 0;

    if (!h.tem_endereco)
        return 0;

    if (strcmp(h.cep, ctx->cep) != 0)
        return 0;

    fprintf(ctx->txt, "%s %s %s\n", h.cpf, h.nome, h.sobrenome);

    h.tem_endereco = 0;
    h.cep[0] = '\0';
    h.face = '\0';
    h.num = 0;
    h.compl[0] = '\0';

    hef_update(ctx->hf_habitantes, h.cpf, &h);
    ctx->removidos++;

    return 0;
}

static int cmd_rq(FILE *txt,
                  FILE *svg,
                  HashExtFile hf_quadras,
                  HashExtFile hf_habitantes,
                  const char *linha)
{
    char cep[CEP_LEN];
    double x, y, w, h;
    RqCtx ctx;
    HashExtStatus st;

    if (sscanf(linha, "%*s %63s", cep) != 1)
        return QRY_ERR_PARSE;

    fprintf(txt, "[*] %s", linha);

    if (buscar_quadra(hf_quadras, cep, &x, &y, &w, &h) != QRY_OK)
    {
        fprintf(txt, "quadra nao encontrada: %s\n", cep);
        return QRY_OK;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.cep = cep;
    ctx.txt = txt;
    ctx.hf_habitantes = hf_habitantes;

    fprintf(txt, "moradores removidos da quadra %s:\n", cep);

    st = hef_foreach(hf_habitantes, cb_rq, &ctx);
    if (st != HEF_OK)
    {
        fprintf(txt, "erro ao percorrer habitantes: %s\n", hef_status_str(st));
        return QRY_ERR_HASH;
    }

    st = hef_remove(hf_quadras, cep, NULL);
    if (st != HEF_OK && st != HEF_ERR_NOT_FOUND)
    {
        fprintf(txt, "erro ao remover quadra: %s\n", hef_status_str(st));
        return QRY_ERR_HASH;
    }

    fprintf(txt, "total de moradores transformados em sem-teto: %d\n", ctx.removidos);

    svg_x_vermelho(svg, x + w, y + h);

    return QRY_OK;
}
