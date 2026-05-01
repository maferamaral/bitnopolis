#ifndef QUADRA_H
#define QUADRA_H

typedef void *Quadra;

Quadra quadra_create(const char *cep,
                     double x, double y, double w, double h,
                     const char *cfill,
                     const char *cstrk,
                     double sw);

void quadra_destroy(Quadra q);

const char *quadra_get_cep(Quadra q);
double quadra_get_x(Quadra q);
double quadra_get_y(Quadra q);
double quadra_get_w(Quadra q);
double quadra_get_h(Quadra q);

const char *quadra_get_fill(Quadra q);
const char *quadra_get_stroke(Quadra q);
double quadra_get_sw(Quadra q);

#endif