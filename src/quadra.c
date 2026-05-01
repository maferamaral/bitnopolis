#include "quadra.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    char cep[32];
    double x, y, w, h;

    char cfill[24];
    char cstrk[24];
    double sw;
} QuadraImpl;

Quadra quadra_create(const char *cep,
                     double x, double y, double w, double h,
                     const char *cfill,
                     const char *cstrk,
                     double sw)
{
    QuadraImpl *q = (QuadraImpl *)malloc(sizeof(QuadraImpl));
    if (!q)
        return NULL;

    strncpy(q->cep, cep, 31);
    q->cep[31] = '\0';

    q->x = x;
    q->y = y;
    q->w = w;
    q->h = h;

    strncpy(q->cfill, cfill, 23);
    q->cfill[23] = '\0';

    strncpy(q->cstrk, cstrk, 23);
    q->cstrk[23] = '\0';

    q->sw = sw;

    return q;
}

void quadra_destroy(Quadra q)
{
    if (!q)
        return;
    free(q);
}

const char *quadra_get_cep(Quadra q)
{
    return ((QuadraImpl *)q)->cep;
}

double quadra_get_x(Quadra q)
{
    return ((QuadraImpl *)q)->x;
}

double quadra_get_y(Quadra q)
{
    return ((QuadraImpl *)q)->y;
}

double quadra_get_w(Quadra q)
{
    return ((QuadraImpl *)q)->w;
}

double quadra_get_h(Quadra q)
{
    return ((QuadraImpl *)q)->h;
}

const char *quadra_get_fill(Quadra q)
{
    return ((QuadraImpl *)q)->cfill;
}

const char *quadra_get_stroke(Quadra q)
{
    return ((QuadraImpl *)q)->cstrk;
}

double quadra_get_sw(Quadra q)
{
    return ((QuadraImpl *)q)->sw;
}