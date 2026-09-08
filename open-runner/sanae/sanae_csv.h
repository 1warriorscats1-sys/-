/* Original SANAE compatibility code. MIT; see the toolkit LICENSE.md.
 * UTF-8/byte-preserving RFC4180-style CSV, bounded allocations. */
#ifndef SANAE_CSV_H
#define SANAE_CSV_H
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#define SANAE_CSV_MAX_BYTES (16u * 1024u * 1024u)
#define SANAE_CSV_MAX_CELLS 1048576u
#define SANAE_CSV_MAX_COLUMNS 4096u

typedef struct { char *text; size_t row, column; int quoted; } SanaeCsvCell;
typedef struct { SanaeCsvCell *cells; size_t count, rows, columns; } SanaeCsv;

static void sanae_csv_free(SanaeCsv *csv) {
    size_t i;
    for (i = 0; i < csv->count; ++i) free(csv->cells[i].text);
    free(csv->cells); memset(csv, 0, sizeof(*csv));
}

static int sanae_csv_parse(const char *input, SanaeCsv *csv) {
    size_t length, row = 0, column = 0, capacity = 0;
    const char *p;
    char *field;
    memset(csv, 0, sizeof(*csv));
    if (!input || !(length = strlen(input)) || length > SANAE_CSV_MAX_BYTES) return 0;
    p = input;
    if (length >= 3 && memcmp(p, "\xef\xbb\xbf", 3) == 0) p += 3;
    if (!*p) return 0;
    field = (char *)malloc(length + 1);
    if (!field) return 0;
    for (;;) {
        size_t used = 0;
        int quoted = (*p == '"');
        char separator;
        if (quoted) {
            int closed = 0; ++p;
            while (*p) {
                if (*p == '"') {
                    if (p[1] == '"') { field[used++] = '"'; p += 2; }
                    else { ++p; closed = 1; break; }
                } else field[used++] = *p++;
            }
            if (!closed || (*p && *p != ',' && *p != '\r' && *p != '\n')) goto fail;
        } else {
            while (*p && *p != ',' && *p != '\r' && *p != '\n') {
                if (*p == '"') goto fail;
                field[used++] = *p++;
            }
        }
        field[used] = 0;
        if (csv->count == SANAE_CSV_MAX_CELLS || column >= SANAE_CSV_MAX_COLUMNS) goto fail;
        if (csv->count == capacity) {
            size_t next = capacity ? capacity * 2 : 32;
            SanaeCsvCell *grown = (SanaeCsvCell *)realloc(csv->cells, next * sizeof(*grown));
            if (!grown) goto fail;
            csv->cells = grown; capacity = next;
        }
        csv->cells[csv->count].text = (char *)malloc(used + 1);
        if (!csv->cells[csv->count].text) goto fail;
        memcpy(csv->cells[csv->count].text, field, used + 1);
        csv->cells[csv->count].row = row;
        csv->cells[csv->count].column = column;
        csv->cells[csv->count].quoted = quoted;
        ++csv->count; ++column;
        if (column > csv->columns) csv->columns = column;
        csv->rows = row + 1;
        separator = *p;
        if (!separator) break;
        ++p;
        if (separator != ',') {
            if (separator == '\r' && *p == '\n') ++p;
            ++row; column = 0;
            if (!*p) break;
        }
    }
    free(field);
    if (csv->rows > SANAE_CSV_MAX_CELLS / csv->columns) {
        sanae_csv_free(csv); return 0;
    }
    return 1;
fail:
    free(field); sanae_csv_free(csv); return 0;
}
#endif
