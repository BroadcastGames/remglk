/* gtstyle.c: Style formatting hints.
        for RemGlk, remote-procedure-call implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://eblong.com/zarf/glk/
*/

#include <stdio.h>
#include "glk.h"
#include "remglk.h"
#include "rgwin_grid.h"
#include "rgwin_buf.h"

/* In the spirit of doing more in the C code so the JSON rendering layer has to do less */


/* stylehint_NUMHINTS is typically equal to 10
  style_NUMSTYLES is typically equal to 11
  wintype_NUMTYPES is typically equal to 6
   */
#define wintype_NUMTYPES (6)
glui32 stylehint_values[style_NUMSTYLES][wintype_NUMTYPES][stylehint_NUMHINTS];
int stylehint_is_set[style_NUMSTYLES][wintype_NUMTYPES][stylehint_NUMHINTS];


/*
This approach of emitting here is rather primitive and spits a lot to the client.
As the 'opcode' like approach sets foreground color of user1 as one operation,
background color as another.
It may make sense to experiment with waiting until the Glk 'window' is
emitted via JSON to cluster styleset* and emit a new 'style' section.
*/
void glk_stylehint_set(glui32 wintype, glui32 styl, glui32 hint,
    glsi32 val)
{
    if (styl >= style_NUMSTYLES || hint >= stylehint_NUMHINTS)
        return;

    stylehint_values[styl][wintype][hint] = val;
    stylehint_is_set[styl][wintype][hint] = TRUE;

   switch (hint) {
       case stylehint_BackColor:
       case stylehint_TextColor:
           /* First two bytes of Glk color are 00, Android standard is to use FF
               8-char for standard alpha transparency. Just emit 6 chars to follow
               CSS and HTML5 canvas convention for colorhexRGB. */
           /* include extra newline after stanza */
           printf("{ \"type\": \"stylesetcolor\", \"stylehint\": %d, \"target\": \"%s\","
               "\"wintype\": %d, \"styleindex\": %d, \"value\": %d, \"valuehex\": \"%08X\","
               "\"colorhexRGB\": \"#%06X\" }\n\n",
               hint, name_for_style(styl), wintype, styl, val, val, val);
           break;
       default:
           /* include extra newline after stanza */
           printf("{ \"type\": \"stylesetother\", \"stylehint\": %d, \"target\": \"%s\","
               "\"wintype\": %d, \"styleindex\": %d, \"value\": %d, \"valuehex\": \"%08X\" }\n\n",
               hint, name_for_style(styl), wintype, styl, val, val);
           break;
   }
}

void glk_stylehint_clear(glui32 wintype, glui32 styl, glui32 hint)
{
   /* include extra newline after stanza */
   printf("{ \"type\": \"stylesetclear\", \"stylehint\": %d, \"target\": \"%s\","
       "\"wintype\": %d, \"styleindex\": %d }\n\n",
       hint, name_for_style(styl), wintype, styl);
}

glui32 glk_style_distinguish(window_t *win, glui32 styl1, glui32 styl2)
{
    if (!win) {
        gli_strict_warning("style_distinguish: invalid ref");
        return FALSE;
    }

    if (styl1 >= style_NUMSTYLES || styl2 >= style_NUMSTYLES)
        return FALSE;

    /* ### */

    return FALSE;
}

glui32 glk_style_measure(window_t *win, glui32 styl, glui32 hint,
    glui32 *result)
{
    glui32 dummy;

    if (!win) {
        gli_strict_warning("style_measure: invalid ref");
        return FALSE;
    }

    if (styl >= style_NUMSTYLES || hint >= stylehint_NUMHINTS)
        return FALSE;

    if (!result)
        result = &dummy;

    /* ### */

    return FALSE;
}
