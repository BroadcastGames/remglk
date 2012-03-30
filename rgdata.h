typedef enum DTag_enum {
    dtag_Unknown = 0,
    dtag_Init = 1,
    dtag_Refresh = 2,
    dtag_Line = 2,
    dtag_Char = 3,
    dtag_Arrange = 4,
    dtag_Hyperlink = 5,
} DTag;

typedef struct gen_list_struct {
    void **list;
    int count;
    int allocsize;
} gen_list_t;

typedef struct data_input_struct data_input_t;
typedef struct data_update_struct data_update_t;
typedef struct data_window_struct data_window_t;

struct data_metrics_struct {
    glui32 width, height;
    glui32 outspacingx, outspacingy;
    glui32 inspacingx, inspacingy;
    glui32 gridcharwidth, gridcharheight;
    glui32 gridmarginx, gridmarginy;
    glui32 buffercharwidth, buffercharheight;
    glui32 buffermarginx, buffermarginy;
};

struct data_input_struct {
    DTag dtag;
    glsi32 gen;
    glui32 window;
    glui32 charvalue;
    glui32 *linevalue;
    glui32 linelen;
    glui32 terminator;
    data_metrics_t *metrics;
};

struct data_update_struct {
    glsi32 gen;
    gen_list_t windows;
    gen_list_t contents;
    gen_list_t inputs;
    int disable;
};

struct data_window_struct {
    glui32 window;
    glui32 type;
    glui32 rock;
    grect_t size;
};

extern void gli_initialize_datainput(void);

extern void gen_list_init(gen_list_t *list);
extern void gen_list_free(gen_list_t *list);
extern void *gen_list_ensure(gen_list_t *list, int num);

extern data_metrics_t *data_metrics_alloc(int width, int height);
extern void data_metrics_free(data_metrics_t *metrics);
extern void data_metrics_print(data_metrics_t *metrics);

extern data_input_t *data_input_read(void);
extern void data_input_free(data_input_t *data);
extern void data_input_print(data_input_t *data);

extern data_update_t *data_update_alloc(void);
extern void data_update_free(data_update_t *data);
extern void data_update_print(data_update_t *data);

extern data_window_t *data_window_alloc(glui32 window, glui32 type, glui32 rock);
extern void data_window_free(data_window_t *data);
