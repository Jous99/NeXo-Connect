# NeXo-Connect

Homebrew (`.nro`) para **Nintendo Switch con Atmosphere** que alterna tu consola
entre **NeXo Network** y los servidores oficiales de Nintendo, desde un menú en
la propia consola — sin PC.

> Código **100% original**, escrito desde cero. GPL-3.0-or-later.

---

## Qué hace

Escribe (o borra) el archivo de hosts que Atmosphere lee al arrancar
(`/atmosphere/hosts/default.txt`), redirigiendo los dominios de Nintendo hacia
la IP de tu servidor NeXo. Es exactamente el mismo mecanismo que hasta ahora
hacías copiando el archivo a mano desde el PC — pero automatizado y reversible
desde la consola.

- **[A] Conectar a NeXo Network** — escribe el hosts file con la IP configurada.
- **[B] Volver a Nintendo** — borra el hosts file; la consola vuelve a conectar
  a Nintendo con normalidad.
- **[X] Editar IP del servidor** — teclado en pantalla para poner/cambiar la IP
  (se guarda en `/switch/nexo-connect/server.txt`).
- **[+] Salir**

Tras conectar o desconectar, **reinicia la Switch** para que Atmosphere aplique
los cambios.

---

## ⚠️ Importante — esto NO es suficiente por sí solo

Este homebrew solo gestiona la **redirección de dominios** (hosts). Para que la
Switch acepte el certificado de tu servidor NeXo hace falta, además, tener
instalados los parches que **desactivan la verificación de certificados SSL**
(`exefs_patches/disable_ca_verification` y `disable_browser_ca_verification`),
compatibles con tu versión de firmware. Sin esos parches, la consola rechazará
la conexión aunque el hosts file esté bien.

Esa parte se instala aparte (ver la guía del servidor: `docs/switch-setup.md`
en el repo de NeXo-Server). Una versión futura de NeXo-Connect podría
gestionarlos también.

---

## Compilar

Necesitas el toolchain **devkitPro** con `libnx`. Lo más limpio es usar su
imagen Docker oficial:

```bash
# En la raíz del proyecto:
docker run --rm -v "$PWD:/project" -w /project devkitpro/devkita64 \
  bash -c "make"
```

O, si tienes devkitPro instalado localmente:

```bash
export DEVKITPRO=/opt/devkitpro
make
```

Genera `NeXo-Connect.nro`.

## Instalar en la Switch

1. Copia `NeXo-Connect.nro` a `SD:/switch/` de tu consola.
2. Abre el **menú homebrew** (álbum manteniendo R, o el método que uses).
3. Lanza **NeXo-Connect**.

---

## Estructura

```
NeXo-Connect/
├── source/main.c     # Toda la lógica (menú, hosts, IP, teclado)
├── Makefile          # Build devkitPro para NRO
├── LICENSE           # GPL-3.0
└── README.md
```

La lista de dominios de Nintendo redirigidos está en `source/main.c`
(`kNintendoHosts[]`) y debe mantenerse en sync con
`scripts/atmosphere-hosts.txt` del servidor NeXo.

---

GPL-3.0-or-later · Proyecto educativo · No afiliado con Nintendo
