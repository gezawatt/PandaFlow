#pragma once
#include <citro2d.h>
#include <stdbool.h>

// Carga una carátula (jpg/png) y la sube como textura. Reemplaza la anterior.
bool coverLoad(const char* path);
void coverClear(void);
bool coverHas(void);

// Dibuja la carátula como cuadrado de lado 'size' en (x,y).
void coverDraw(float x, float y, float depth, float size);
