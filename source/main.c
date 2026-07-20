// SPDX-FileCopyrightText: Copyright 2026 NeXo Network Project
// SPDX-License-Identifier: GPL-3.0-or-later
//
// NeXo-Connect — Homebrew para alternar una Nintendo Switch (con Atmosphere)
// entre NeXo Network y los servidores oficiales de Nintendo.
//
// Escribe/borra el archivo de hosts que Atmosphere (DNS.mitm) lee al arrancar,
// redirigiendo los dominios de Nintendo hacia tu servidor NeXo. Es reversible:
// "Volver a Nintendo" borra el archivo y la consola vuelve a conectar normal.
//
// Código 100% original, escrito desde cero.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <switch.h>

#define HOSTS_DIR    "/atmosphere/hosts"
#define HOSTS_FILE   "/atmosphere/hosts/default.txt"
#define CONFIG_DIR   "/switch/nexo-connect"
#define CONFIG_FILE  "/switch/nexo-connect/server.txt"
#define DEFAULT_IP   "192.168.1.133"
#define IP_MAXLEN    64

// Dominios de Nintendo que redirigimos hacia el servidor NeXo.
// Mantener en sync con scripts/atmosphere-hosts.txt del servidor.
static const char *kNintendoHosts[] = {
    // Auth chain
    "dauth-lp1.ndas.srv.nintendo.net",
    "aauth-lp1.ndas.srv.nintendo.net",
    "accounts.nintendo.com",
    "api.accounts.nintendo.com",
    "cdn.accounts.nintendo.com",
    // BAAS (token por juego)
    "e97b8a9d672e4ce4845b-sb.baas.nintendo.com",
    "d78dbb08cf698e042d54-sb.baas.nintendo.com",
    "ed9e2f05d286b7b8ceda-sb.baas.nintendo.com",
    "a9e5b722a406f9cee4ea-sb.baas.nintendo.com",
    "e1c218b5657e4e03b6e0-sb.baas.nintendo.com",
    "7fbcac5a1e52b070d5a2-sb.baas.nintendo.com",
    "9dc9e6e4765c77a5d4d1-sb.baas.nintendo.com",
    "f4a0f589eca62d8e8f79-sb.baas.nintendo.com",
    "ead00c7dc4b91df4-sb.baas.nintendo.com",
    "7d3231955ef40ca3-sb.baas.nintendo.com",
    // Amigos y presencia
    "friends.lp1.s.n.srv.nintendo.net",
    "friends-lp1.s.n.srv.nintendo.net",
    // Servidores de juego / NPLN
    "api.lp1.npln.srv.nintendo.net",
    "g9s300c4msl.lp1.s.n.srv.nintendo.net",
    "g9s300c4msl-sb.lp1.s.n.srv.nintendo.net",
    // BCAT
    "bcat-list-lp1.cdn.nintendo.net",
    "bcat-dl-lp1.cdn.nintendo.net",
    "bcat-list-lp1-sb.cdn.nintendo.net",
    // Conectividad / NIFM
    "ctest.cdn.nintendo.net",
    "nasc.nintendowifi.net",
    "pptest.nintendo.net",
    "conntest.nintendowifi.net",
};
static const int kNintendoHostsCount = sizeof(kNintendoHosts) / sizeof(kNintendoHosts[0]);

// ─── Utilidades ──────────────────────────────────────────────────────────────

// mkdir recursivo (estilo "mkdir -p") sobre rutas de la SD.
static void ensure_dir(const char *path) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len && tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

// Lee la IP guardada; si no existe, usa la de por defecto.
static void load_ip(char *out, size_t out_len) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) {
        snprintf(out, out_len, "%s", DEFAULT_IP);
        return;
    }
    if (!fgets(out, (int)out_len, f)) {
        snprintf(out, out_len, "%s", DEFAULT_IP);
        fclose(f);
        return;
    }
    fclose(f);
    // Quitar salto de línea y espacios finales.
    out[strcspn(out, "\r\n")] = '\0';
    if (out[0] == '\0') snprintf(out, out_len, "%s", DEFAULT_IP);
}

// Guarda la IP en la config.
static bool save_ip(const char *ip) {
    ensure_dir(CONFIG_DIR);
    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) return false;
    fprintf(f, "%s\n", ip);
    fclose(f);
    return true;
}

// Escribe el archivo de hosts con todas las redirecciones hacia la IP dada.
static bool write_hosts(const char *ip) {
    ensure_dir(HOSTS_DIR);
    FILE *f = fopen(HOSTS_FILE, "w");
    if (!f) return false;
    fprintf(f, "# Generado por NeXo-Connect — NO editar a mano.\n");
    fprintf(f, "# Redirige los dominios de Nintendo hacia el servidor NeXo.\n\n");
    for (int i = 0; i < kNintendoHostsCount; i++) {
        fprintf(f, "%s %s\n", ip, kNintendoHosts[i]);
    }
    fclose(f);
    return true;
}

// Elimina el archivo de hosts (vuelve a los servidores de Nintendo).
static bool remove_hosts(void) {
    if (remove(HOSTS_FILE) != 0 && errno != ENOENT) return false;
    return true;
}

// ¿Está actualmente activa la conexión a NeXo? (existe el hosts file)
static bool is_connected(void) {
    FILE *f = fopen(HOSTS_FILE, "r");
    if (f) { fclose(f); return true; }
    return false;
}

// Teclado en pantalla para editar la IP. Devuelve true si el usuario confirmó.
static bool edit_ip(char *ip, size_t ip_len) {
    SwkbdConfig kbd;
    Result rc = swkbdCreate(&kbd, 0);
    if (R_FAILED(rc)) return false;

    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, "IP del servidor NeXo (ej: 192.168.1.133)");
    swkbdConfigSetInitialText(&kbd, ip);
    swkbdConfigSetStringLenMax(&kbd, IP_MAXLEN - 1);

    char tmp[IP_MAXLEN];
    rc = swkbdShow(&kbd, tmp, sizeof(tmp));
    swkbdClose(&kbd);

    if (R_FAILED(rc)) return false;      // cancelado
    tmp[strcspn(tmp, "\r\n")] = '\0';
    if (tmp[0] == '\0') return false;    // vacío, no cambiar

    snprintf(ip, ip_len, "%s", tmp);
    return true;
}

// ─── Interfaz ────────────────────────────────────────────────────────────────

static void draw_menu(const char *ip, const char *status_msg) {
    consoleClear();
    printf("\n");
    printf("  \x1b[36m╔══════════════════════════════════════════╗\x1b[0m\n");
    printf("  \x1b[36m║\x1b[0m           \x1b[1mNeXo-Connect\x1b[0m                   \x1b[36m║\x1b[0m\n");
    printf("  \x1b[36m║\x1b[0m   Switch  <->  NeXo Network / Nintendo    \x1b[36m║\x1b[0m\n");
    printf("  \x1b[36m╚══════════════════════════════════════════╝\x1b[0m\n\n");

    if (is_connected())
        printf("  Estado actual:  \x1b[32m● Conectado a NeXo Network\x1b[0m\n");
    else
        printf("  Estado actual:  \x1b[33m● Servidores de Nintendo\x1b[0m\n");

    printf("  Servidor NeXo:  \x1b[1m%s\x1b[0m\n\n", ip);

    printf("  \x1b[32m[A]\x1b[0m  Conectar a NeXo Network\n");
    printf("  \x1b[33m[B]\x1b[0m  Volver a Nintendo\n");
    printf("  \x1b[36m[X]\x1b[0m  Editar IP del servidor\n");
    printf("  \x1b[31m[+]\x1b[0m  Salir\n\n");

    if (status_msg && status_msg[0])
        printf("  %s\n", status_msg);

    printf("\n  Tras conectar/desconectar, \x1b[1mreinicia tu Switch\x1b[0m para aplicar.\n");
    consoleUpdate(NULL);
}

int main(int argc, char **argv) {
    consoleInit(NULL);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    char ip[IP_MAXLEN];
    load_ip(ip, sizeof(ip));

    char status[128] = "";
    draw_menu(ip, status);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus) {
            break;
        }

        if (kDown & HidNpadButton_A) {
            if (write_hosts(ip))
                snprintf(status, sizeof(status), "\x1b[32m✓ Conectado a NeXo. Reinicia la consola.\x1b[0m");
            else
                snprintf(status, sizeof(status), "\x1b[31m✗ Error al escribir el archivo de hosts.\x1b[0m");
            draw_menu(ip, status);
        }

        if (kDown & HidNpadButton_B) {
            if (remove_hosts())
                snprintf(status, sizeof(status), "\x1b[33m✓ Volviendo a Nintendo. Reinicia la consola.\x1b[0m");
            else
                snprintf(status, sizeof(status), "\x1b[31m✗ Error al borrar el archivo de hosts.\x1b[0m");
            draw_menu(ip, status);
        }

        if (kDown & HidNpadButton_X) {
            if (edit_ip(ip, sizeof(ip))) {
                if (save_ip(ip)) {
                    // Si ya estaba conectado, reescribir con la IP nueva.
                    if (is_connected()) write_hosts(ip);
                    snprintf(status, sizeof(status), "\x1b[32m✓ IP guardada: %s\x1b[0m", ip);
                } else {
                    snprintf(status, sizeof(status), "\x1b[31m✗ No se pudo guardar la IP.\x1b[0m");
                }
            }
            // Tras cerrar el teclado, redibujar el menú (el applet swkbd
            // suspende el render de la app y lo restaura al volver; no hay
            // que reinicializar la consola).
            draw_menu(ip, status);
        }
    }

    consoleExit(NULL);
    return 0;
}
