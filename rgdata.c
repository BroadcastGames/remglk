
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "glk.h"
#include "remglk.h"
#include "rgdata.h"

typedef enum RawType_enum {
    rawtyp_None = 0,
    rawtyp_Int = 1,
    rawtyp_Str = 2,
    rawtyp_List = 3,
    rawtyp_Struct = 4,
    rawtyp_True = 5,
    rawtyp_False = 6,
    rawtyp_Null = 7,
} RawType;

typedef struct data_raw_struct data_raw_t;

struct data_raw_struct {
    RawType type;
    glui32 *key;
    int keylen;

    glsi32 number;
    glui32 *str;
    data_raw_t **list;
    int count;
    int allocsize;
};

static data_raw_t *data_raw_blockread(void);
static data_raw_t *data_raw_blockread_sub(char *termchar);

static char *stringbuf = NULL;
static int stringbuf_size = 0;
static glui32 *ustringbuf = NULL;
static int ustringbuf_size = 0;

void gli_initialize_datainput()
{
    stringbuf_size = 64;
    stringbuf = malloc(stringbuf_size * sizeof(char));
    if (!stringbuf)
        gli_fatal_error("data: Unable to allocate memory for string buffer");

    ustringbuf_size = 64;
    ustringbuf = malloc(ustringbuf_size * sizeof(glui32));
    if (!ustringbuf)
        gli_fatal_error("data: Unable to allocate memory for string buffer");
}

static int parse_hex_digit(char ch)
{
    if (ch == EOF)
        gli_fatal_error("data: Unexpected end of input");
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch + 10 - 'A';
    if (ch >= 'a' && ch <= 'f')
        return ch + 10 - 'a';
    gli_fatal_error("data: Not a hex digit");
    return 0;
}

static void ensure_stringbuf_size(int val)
{
    if (val <= stringbuf_size)
        return;
    stringbuf_size = val*2;
    stringbuf = realloc(stringbuf, stringbuf_size * sizeof(char));
    if (!stringbuf)
        gli_fatal_error("data: Unable to allocate memory for ustring buffer");
}

static void ensure_ustringbuf_size(int val)
{
    if (val <= ustringbuf_size)
        return;
    ustringbuf_size = val*2;
    ustringbuf = realloc(ustringbuf, ustringbuf_size * sizeof(glui32));
    if (!ustringbuf)
        gli_fatal_error("data: Unable to allocate memory for ustring buffer");
}

static void print_string_json_utf8(glui32 *buf, glui32 len, FILE *fl)
{
    int ix;

    fprintf(fl, "\"");
    for (ix=0; ix<len; ix++) {
        glui32 ch = buf[ix];
        if (ch == '\"')
            fprintf(fl, "\\\"");
        else if (ch == '\\')
            fprintf(fl, "\\\\");
        else if (ch == '\n')
            fprintf(fl, "\\n");
        else if (ch == '\t')
            fprintf(fl, "\\t");
        else if (ch < 32)
            fprintf(fl, "\\u%04X", ch);
        else
            gli_putchar_utf8(ch, fl);
    }
    fprintf(fl, "\"");
}

static data_raw_t *data_raw_alloc(RawType type)
{
    data_raw_t *dat = malloc(sizeof(data_raw_t));
    if (!dat)
        gli_fatal_error("data: Unable to allocate memory for data block");

    dat->type = type;
    dat->key = NULL;
    dat->keylen = 0;
    dat->number = 0;
    dat->str = NULL;
    dat->list = NULL;
    dat->count = 0;
    dat->allocsize = 0;
    return dat;
}

static void data_raw_free(data_raw_t *dat) 
{
    if (dat->str)
        free(dat->str);
    if (dat->key)
        free(dat->key);
    if (dat->list) {
        int ix;
        for (ix=0; ix<dat->count; ix++) {
            data_raw_free(dat->list[ix]);
        }
        free(dat->list);
    }
    dat->type = rawtyp_None;
    free(dat);
}

static void data_raw_ensure_size(data_raw_t *dat, int size)
{
    if (size <= dat->allocsize)
        return;

    if (!dat->list) {
        dat->allocsize = (size+1) * 2;
        dat->list = malloc(sizeof(data_raw_t *) * dat->allocsize);
    }
    else {
        dat->allocsize = (size+1) * 2;
        dat->list = realloc(dat->list, sizeof(data_raw_t *) * dat->allocsize);
    }

    if (!dat->list)
        gli_fatal_error("data: Unable to allocate memory for data list");
}

void data_raw_print(data_raw_t *dat)
{
    int ix;

    if (!dat) {
        printf("<?NULL>");
        return;
    }

    switch (dat->type) {
        case rawtyp_Int:
            printf("%ld", (long)dat->number);
            return;
        case rawtyp_True:
            printf("true");
            return;
        case rawtyp_False:
            printf("false");
            return;
        case rawtyp_Null:
            printf("null");
            return;
        case rawtyp_Str:
            print_string_json_utf8(dat->str, dat->count, stdout);
            return;
        case rawtyp_List:
            printf("[ ");
            for (ix=0; ix<dat->count; ix++) {
                data_raw_print(dat->list[ix]);
                if (ix != dat->count-1)
                    printf(", ");
                else
                    printf(" ");
            }
            printf("]");
            return;
        case rawtyp_Struct:
            printf("{ ");
            for (ix=0; ix<dat->count; ix++) {
                data_raw_t *subdat = dat->list[ix];
                print_string_json_utf8(subdat->key, subdat->keylen, stdout);
                printf(": ");
                data_raw_print(subdat);
                if (ix != dat->count-1)
                    printf(", ");
                else
                    printf(" ");
            }
            printf("}");
            return;
        default:
            printf("<?>");
            return;
    }
}

static data_raw_t *data_raw_blockread()
{
    char termchar;

    data_raw_t *dat = data_raw_blockread_sub(&termchar);
    if (!dat)
        gli_fatal_error("data: Unexpected end of data object");

    return dat;
}

static glsi32 data_raw_int_value(data_raw_t *dat)
{
    if (dat->type != rawtyp_Int)
        gli_fatal_error("data: Need int");

    return dat->number;
}

static glui32 *data_raw_str_dup(data_raw_t *dat)
{
    glui32 *str;

    if (dat->type != rawtyp_Str)
        gli_fatal_error("data: Need str");

    if (dat->count == 0) {
        /* Allocate a tiny block, because I never trusted malloc(0) */
        str = (glui32 *)malloc(1 * sizeof(glui32));
        str[0] = 0;
        return str;
    }

    str = (glui32 *)malloc(dat->count * sizeof(glui32));
    memcpy(str, dat->str, dat->count * sizeof(glui32));
    return str;
}

static glui32 data_raw_str_char(data_raw_t *dat)
{
    if (dat->type != rawtyp_Str)
        gli_fatal_error("data: Need str");

    if (dat->count == 0) 
        gli_fatal_error("data: Need nonempty string");

    return dat->str[0];
}

static int data_raw_string_is(data_raw_t *dat, char *key)
{
    char *cx;
    int pos;

    if (dat->type != rawtyp_Str)
        gli_fatal_error("data: Need str");

    for (pos=0, cx=key; *cx && pos<dat->count; pos++, cx++) {
        if (dat->str[pos] != (glui32)(*cx))
            break;
    }

    if (*cx == '\0' && pos == dat->count)
        return TRUE;
    else
        return FALSE;
}

static data_raw_t *data_raw_struct_field(data_raw_t *dat, char *key)
{
    int ix;
    char *cx;
    int pos;

    if (dat->type != rawtyp_Struct)
        gli_fatal_error("data: Need struct");

    for (ix=0; ix<dat->count; ix++) {
        data_raw_t *subdat = dat->list[ix];
        for (pos=0, cx=key; *cx && pos<subdat->keylen; pos++, cx++) {
            if (subdat->key[pos] != (glui32)(*cx))
                break;
        }
        if (*cx == '\0' && pos == subdat->keylen) {
            return subdat;
        }
    }

    return NULL;
}

static data_raw_t *data_raw_blockread_sub(char *termchar)
{
    int ch;

    *termchar = '\0';

    while (isspace(ch = getchar())) { };
    if (ch == EOF)
        gli_fatal_error("data: Unexpected end of input");
    
    if (ch == ']' || ch == '}') {
        *termchar = ch;
        return NULL;
    }

    if (ch >= '0' && ch <= '9') {
        /* This accepts "01", which it really shouldn't, but whatever */
        data_raw_t *dat = data_raw_alloc(rawtyp_Int);
        while (ch >= '0' && ch <= '9') {
            dat->number = 10 * dat->number + (ch-'0');
            ch = getchar();
        }

        if (ch != EOF)
            ungetc(ch, stdin);
        return dat;
    }

    if (ch == '-') {
        data_raw_t *dat = data_raw_alloc(rawtyp_Int);
        ch = getchar();
        if (!(ch >= '0' && ch <= '9'))
            gli_fatal_error("data: minus must be followed by number");

        while (ch >= '0' && ch <= '9') {
            dat->number = 10 * dat->number + (ch-'0');
            ch = getchar();
        }
        dat->number = -dat->number;

        if (ch != EOF)
            ungetc(ch, stdin);
        return dat;
    }

    if (ch == '"') {
        data_raw_t *dat = data_raw_alloc(rawtyp_Str);

        int ix;
        int ucount = 0;
        int count = 0;
        while ((ch = getchar()) != '"') {
            if (ch == EOF)
                gli_fatal_error("data: Unterminated string");
            if (ch >= 0 && ch < 32)
                gli_fatal_error("data: Control character in string");
            if (ch == '\\') {
                ensure_ustringbuf_size(ucount + 2*count + 1);
                ucount += gli_parse_utf8((unsigned char *)stringbuf, count, ustringbuf+ucount, 2*count);
                count = 0;
                ch = getchar();
                if (ch == EOF)
                    gli_fatal_error("data: Unterminated backslash escape");
                if (ch == 'u') {
                    glui32 val = 0;
                    ch = getchar();
                    val = 16*val + parse_hex_digit(ch);
                    ch = getchar();
                    val = 16*val + parse_hex_digit(ch);
                    ch = getchar();
                    val = 16*val + parse_hex_digit(ch);
                    ch = getchar();
                    val = 16*val + parse_hex_digit(ch);
                    ustringbuf[ucount++] = val;
                    continue;
                }
                ensure_stringbuf_size(count+1);
                switch (ch) {
                    case '"':
                        stringbuf[count++] = '"';
                        break;
                    case '/':
                        stringbuf[count++] = '/';
                        break;
                    case '\\':
                        stringbuf[count++] = '\\';
                        break;
                    case 'b':
                        stringbuf[count++] = '\b';
                        break;
                    case 'f':
                        stringbuf[count++] = '\f';
                        break;
                    case 'n':
                        stringbuf[count++] = '\n';
                        break;
                    case 'r':
                        stringbuf[count++] = '\r';
                        break;
                    case 't':
                        stringbuf[count++] = '\t';
                        break;
                    default:
                        gli_fatal_error("data: Unknown backslash code");
                }
            }
            else {
                ensure_stringbuf_size(count+1);
                stringbuf[count++] = ch;
            }
        }

        ensure_ustringbuf_size(ucount + 2*count);
        ucount += gli_parse_utf8((unsigned char *)stringbuf, count, ustringbuf+ucount, 2*count);

        dat->count = ucount;
        dat->str = malloc(ucount * sizeof(glui32));
        for (ix=0; ix<ucount; ix++) {
            dat->str[ix] = ustringbuf[ix];
        }

        return dat;
    }

    if (isalpha(ch)) {
        data_raw_t *dat = NULL;

        int count = 0;
        while (isalnum(ch) || ch == '_') {
            ensure_stringbuf_size(count+1);
            stringbuf[count++] = ch;
            ch = getchar();
        }

        ensure_stringbuf_size(count+1);
        stringbuf[count++] = '\0';

        if (!strcmp(stringbuf, "true"))
            dat = data_raw_alloc(rawtyp_True);
        else if (!strcmp(stringbuf, "false"))
            dat = data_raw_alloc(rawtyp_False);
        else if (!strcmp(stringbuf, "null"))
            dat = data_raw_alloc(rawtyp_Null);
        else
            gli_fatal_error("data: Unrecognized symbol");

        if (ch != EOF)
            ungetc(ch, stdin);
        return dat;
    }

    if (ch == '[') {
        data_raw_t *dat = data_raw_alloc(rawtyp_List);
        int count = 0;
        int commapending = FALSE;
        char term = '\0';

        while (TRUE) {
            data_raw_t *subdat = data_raw_blockread_sub(&term);
            if (!subdat) {
                if (term == ']') {
                    if (commapending)
                        gli_fatal_error("data: List should not end with comma");
                    break;
                }
                gli_fatal_error("data: Mismatched end of list");
            }
            data_raw_ensure_size(dat, count+1);
            dat->list[count++] = subdat;
            commapending = FALSE;

            while (isspace(ch = getchar())) { };
            if (ch == ']')
                break;
            if (ch != ',')
                gli_fatal_error("data: Expected comma in list");
            commapending = TRUE;
        }

        dat->count = count;
        return dat;
    }

    if (ch == '{') {
        data_raw_t *dat = data_raw_alloc(rawtyp_Struct);
        int count = 0;
        int commapending = FALSE;
        char term = '\0';

        while (TRUE) {
            data_raw_t *keydat = data_raw_blockread_sub(&term);
            if (!keydat) {
                if (term == '}') {
                    if (commapending)
                        gli_fatal_error("data: Struct should not end with comma");
                    break;
                }
                gli_fatal_error("data: Mismatched end of struct");
            }

            if (keydat->type != rawtyp_Str)
                gli_fatal_error("data: Struct key must be string");

            while (isspace(ch = getchar())) { };
            
            if (ch != ':')
                gli_fatal_error("data: Expected colon in struct");

            data_raw_t *subdat = data_raw_blockread_sub(&term);
            if (!keydat)
                gli_fatal_error("data: Mismatched end of struct");

            subdat->key = keydat->str;
            subdat->keylen = keydat->count;
            keydat->str = NULL;
            keydat->count = 0;
            data_raw_free(keydat);
            keydat = NULL;

            data_raw_ensure_size(dat, count+1);
            dat->list[count++] = subdat;
            commapending = FALSE;

            while (isspace(ch = getchar())) { };
            if (ch == '}')
                break;
            if (ch != ',')
                gli_fatal_error("data: Expected comma in struct");
            commapending = TRUE;
        }

        dat->count = count;
        return dat;
    }

    gli_fatal_error("data: Invalid character in data");
    return NULL;
}

static data_metrics_t *data_metrics_parse(data_raw_t *rawdata)
{
    data_raw_t *dat;

    data_metrics_t *metrics = (data_metrics_t *)malloc(sizeof(data_metrics_t));
    metrics->width = 0;
    metrics->height = 0;
    metrics->outspacingx = 0;
    metrics->outspacingy = 0;
    metrics->inspacingx = 0;
    metrics->inspacingy = 0;
    metrics->gridcharwidth = 1;
    metrics->gridcharheight = 1;
    metrics->gridmarginx = 0;
    metrics->gridmarginy = 0;
    metrics->buffercharwidth = 1;
    metrics->buffercharheight = 1;
    metrics->buffermarginx = 0;
    metrics->buffermarginy = 0;

    if (rawdata->type != rawtyp_Struct)
        gli_fatal_error("data: Need struct");

    dat = data_raw_struct_field(rawdata, "width");
    if (!dat)
        gli_fatal_error("data: Metrics require width");
    metrics->width = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "height");
    if (!dat)
        gli_fatal_error("data: Metrics require height");
    metrics->height = data_raw_int_value(dat);

    dat = data_raw_struct_field(rawdata, "charwidth");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->gridcharwidth = val;
        metrics->buffercharwidth = val;
    }
    dat = data_raw_struct_field(rawdata, "charheight");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->gridcharheight = val;
        metrics->buffercharheight = val;
    }

    dat = data_raw_struct_field(rawdata, "gridcharwidth");
    if (dat)
        metrics->gridcharwidth = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "gridcharheight");
    if (dat)
        metrics->gridcharheight = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "buffercharwidth");
    if (dat)
        metrics->buffercharwidth = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "buffercharheight");
    if (dat)
        metrics->buffercharheight = data_raw_int_value(dat);

    dat = data_raw_struct_field(rawdata, "margin");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->gridmarginx = val;
        metrics->gridmarginy = val;
        metrics->buffermarginx = val;
        metrics->buffermarginy = val;
    }

    dat = data_raw_struct_field(rawdata, "gridmargin");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->gridmarginx = val;
        metrics->gridmarginy = val;
    }

    dat = data_raw_struct_field(rawdata, "buffermargin");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->buffermarginx = val;
        metrics->buffermarginy = val;
    }

    dat = data_raw_struct_field(rawdata, "marginx");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->gridmarginx = val;
        metrics->buffermarginx = val;
    }

    dat = data_raw_struct_field(rawdata, "marginy");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->gridmarginy = val;
        metrics->buffermarginy = val;
    }

    dat = data_raw_struct_field(rawdata, "gridmarginx");
    if (dat) 
        metrics->gridmarginx = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "gridmarginy");
    if (dat) 
        metrics->gridmarginy = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "buffermarginx");
    if (dat) 
        metrics->buffermarginx = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "buffermarginy");
    if (dat) 
        metrics->buffermarginy = data_raw_int_value(dat);

    dat = data_raw_struct_field(rawdata, "spacing");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->inspacingx = val;
        metrics->inspacingy = val;
        metrics->outspacingx = val;
        metrics->outspacingy = val;
    }

    dat = data_raw_struct_field(rawdata, "inspacing");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->inspacingx = val;
        metrics->inspacingy = val;
    }

    dat = data_raw_struct_field(rawdata, "outspacing");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->outspacingx = val;
        metrics->outspacingy = val;
    }

    dat = data_raw_struct_field(rawdata, "spacingx");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->inspacingx = val;
        metrics->outspacingx = val;
    }

    dat = data_raw_struct_field(rawdata, "spacingy");
    if (dat) {
        glsi32 val = data_raw_int_value(dat);
        metrics->inspacingy = val;
        metrics->outspacingy = val;
    }

    dat = data_raw_struct_field(rawdata, "inspacingx");
    if (dat)
        metrics->inspacingx = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "inspacingy");
    if (dat)
        metrics->inspacingy = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "outspacingx");
    if (dat)
        metrics->outspacingx = data_raw_int_value(dat);
    dat = data_raw_struct_field(rawdata, "outspacingy");
    if (dat)
        metrics->outspacingy = data_raw_int_value(dat);

    if (metrics->gridcharwidth <= 0 || metrics->gridcharheight <= 0
        || metrics->buffercharwidth <= 0 || metrics->buffercharheight <= 0)
        gli_fatal_error("Metrics character size must be positive");

    return metrics;
}

void data_metrics_free(data_metrics_t *metrics)
{
    free(metrics);
}

void data_metrics_print(data_metrics_t *metrics)
{
    /* This displays very verbosely, and not in JSON-readable format.
       That's okay -- it's only used for debugging. */
 
    printf("{\n");   
    printf("  size: %ldx%ld\n", (long)metrics->width, (long)metrics->height);
    printf("  outspacing: %ldx%ld\n", (long)metrics->outspacingx, (long)metrics->outspacingy);
    printf("  inspacing: %ldx%ld\n", (long)metrics->inspacingx, (long)metrics->inspacingy);
    printf("  gridchar: %ldx%ld\n", (long)metrics->gridcharwidth, (long)metrics->gridcharheight);
    printf("  gridmargin: %ldx%ld\n", (long)metrics->gridmarginx, (long)metrics->gridmarginy);
    printf("  bufferchar: %ldx%ld\n", (long)metrics->buffercharwidth, (long)metrics->buffercharheight);
    printf("  buffermargin: %ldx%ld\n", (long)metrics->buffermarginx, (long)metrics->buffermarginy);
    printf("}\n");   
}

void data_input_free(data_input_t *data)
{
    data->dtag = dtag_Unknown;
    if (data->linevalue) {
        free(data->linevalue);
        data->linevalue = NULL;
    }
    if (data->metrics) {
        data_metrics_free(data->metrics);
        data->metrics = NULL;
    }
    free(data);
}

void data_input_print(data_input_t *data)
{
    switch (data->dtag) {
        case dtag_Init:
            printf("{ \"type\": \"init\", \"gen\": %d, \"metrics\":\n",
                data->gen);
            data_metrics_print(data->metrics);
            printf("}\n");
            break;

        default:
            printf("{? unknown dtag %d}\n", data->dtag);
            break;
    }
}

data_input_t *data_input_read()
{
    data_raw_t *dat;

    data_raw_t *rawdata = data_raw_blockread();

    if (rawdata->type != rawtyp_Struct)
        gli_fatal_error("data: Input struct not a struct");

    dat = data_raw_struct_field(rawdata, "type");
    if (!dat)
        gli_fatal_error("data: Input struct has no type");

    data_input_t *input = (data_input_t *)malloc(sizeof(data_input_t));
    input->dtag = dtag_Unknown;
    input->gen = 0;
    input->window = 0;
    input->charvalue = 0;
    input->linevalue = NULL;
    input->linelen = 0;
    input->terminator = 0;
    input->metrics = NULL;

    if (data_raw_string_is(dat, "init")) {
        input->dtag = dtag_Init;

        dat = data_raw_struct_field(rawdata, "gen");
        if (dat)
            input->gen = data_raw_int_value(dat);

        dat = data_raw_struct_field(rawdata, "metrics");
        if (!dat)
            gli_fatal_error("data: Init input struct has no metrics");

        input->metrics = data_metrics_parse(dat);
    }
    else if (data_raw_string_is(dat, "arrange")) {
        input->dtag = dtag_Arrange;

        dat = data_raw_struct_field(rawdata, "gen");
        if (!dat)
            gli_fatal_error("data: Arrange input struct has no gen");
        input->gen = data_raw_int_value(dat);

        dat = data_raw_struct_field(rawdata, "metrics");
        if (!dat)
            gli_fatal_error("data: Arrange input struct has no metrics");

        input->metrics = data_metrics_parse(dat);
    }
    else if (data_raw_string_is(dat, "line")) {
        input->dtag = dtag_Line;

        dat = data_raw_struct_field(rawdata, "gen");
        if (!dat)
            gli_fatal_error("data: Line input struct has no gen");
        input->gen = data_raw_int_value(dat);

        dat = data_raw_struct_field(rawdata, "window");
        if (!dat)
            gli_fatal_error("data: Line input struct has no window");
        input->window = data_raw_int_value(dat);

        dat = data_raw_struct_field(rawdata, "value");
        if (!dat)
            gli_fatal_error("data: Line input struct has no value");
        input->linevalue = data_raw_str_dup(dat);
        input->linelen = dat->count;

        dat = data_raw_struct_field(rawdata, "terminator");
        if (dat)
            input->terminator = data_raw_str_char(dat);
    }
    else if (data_raw_string_is(dat, "char")) {
        input->dtag = dtag_Char;

        dat = data_raw_struct_field(rawdata, "gen");
        if (!dat)
            gli_fatal_error("data: Char input struct has no gen");
        input->gen = data_raw_int_value(dat);

        dat = data_raw_struct_field(rawdata, "window");
        if (!dat)
            gli_fatal_error("data: Char input struct has no window");
        input->window = data_raw_int_value(dat);

        dat = data_raw_struct_field(rawdata, "value");
        if (!dat)
            gli_fatal_error("data: Char input struct has no value");
        input->charvalue = data_raw_str_char(dat);
    }
    else {
        gli_fatal_error("data: Input struct has unknown type");
    }

    /*### partials support */

    return input;
}
