/*
 * Stubs MINIMOS de API do QEMU, so para checagem de sintaxe/parsing dos
 * arquivos goldfish fora da arvore do QEMU. NAO e uma reimplementacao e
 * nao entra no build real -- vive em tools/stubs/ e e usado por
 * tools/check-syntax.sh.
 *
 * Emula a "era moderna" (QEMU_VERSION_MAJOR nao definido -> GF_ATLEAST
 * sempre verdadeiro, exceto 10.0), entao cobre o caminho novo do compat.h.
 * O caminho antigo e coberto definindo QEMU_VERSION_MAJOR/MINOR na
 * linha de comando.
 */
#ifndef STUB_OSDEP_H
#define STUB_OSDEP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

typedef uint64_t hwaddr;
typedef uint64_t ram_addr_t;
#define HWADDR_PRIx PRIx64

/* glib */
typedef void *gpointer;
typedef char gchar;
#define g_new(type, n)      ((type *)calloc((n), sizeof(type)))
#define g_new0(type, n)     ((type *)calloc((n), sizeof(type)))
#define g_malloc0(n)        calloc(1, (n))
#define g_free(p)           free(p)
#define g_strdup(s)         strdup(s)
void *g_realloc(void *p, size_t n);
#define ARRAY_SIZE(a)       (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define QEMU_BUILD_BUG_ON(x)

char *pstrcpy(char *buf, int buf_size, const char *str);

#endif
