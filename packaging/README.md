# Publicar en Universal Updater (Universal-DB)

Universal Updater toma las apps del repositorio
[Universal-Team/db](https://github.com/Universal-Team/db). El bot de Universal-DB
auto-rellena casi todo (versión, descargas, QR, estrellas, capturas…) leyendo este
repo y sus **GitHub Releases**. Solo hay que entregar un JSON mínimo.

## Requisitos previos

1. Repo público con descripción (lo cumple el README).
2. Una **GitHub Release** cuyo asset sea `pandaflow.3dsx` (binario directo, no zip).
   El CI lo publica solo al empujar un tag `v*`:
   ```sh
   git tag v1.0.0
   git push origin v1.0.0
   ```
3. `meta/icon.png` accesible por URL (se usa como icono).

## Enviar la app

Opción rápida (recomendada, se revisa antes): **Pull Request** a
`Universal-Team/db` añadiendo el archivo `docs/_3ds/pandaflow.json` con el
contenido de [`universal-db.json`](universal-db.json):

```json
{
	"github": "PandaAkiraNakai/PandaFlow",
	"systems": ["3DS"],
	"categories": ["app"],
	"icon": "https://raw.githubusercontent.com/PandaAkiraNakai/PandaFlow/main/meta/icon.png"
}
```

Como el release tiene un `.3dsx` directo, Universal-DB genera el script de
instalación automáticamente (lo deja en `sdmc:/3ds/PandaFlow/`). No hacen falta
scripts manuales.

Alternativa: el formulario web <https://db.universal-team.net/app-request> genera
el JSON leyendo el repo, y luego se sube como Issue/PR.

> Nota: PandaFlow es `.3dsx` (homebrew), no `.cia`. FBI instala `.cia`; para
> `.3dsx` el camino es Universal Updater o copia manual a `sdmc:/3ds/`.
