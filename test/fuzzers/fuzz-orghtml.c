#include <stdint.h>
#include <stdlib.h>
#include "org4c-html.h"


static void
process_output(const ORG_CHAR* text, ORG_SIZE size, void* userdata)
{
   /* This is a dummy function because we don't need to generate any output
    * actually. */
   (void) text;
   (void) size;
   (void) userdata;
   return;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    unsigned parser_flags, renderer_flags;

    /* We interpret the 1st 8 bytes as parser flags and renderer flags. */
    if(size < 2 * sizeof(unsigned)) {
        return 0;
    }
    parser_flags = ((unsigned*)data)[0];
    renderer_flags = ((unsigned*)data)[1];
    data += 2 * sizeof(unsigned);
    size -= 2 * sizeof(unsigned);

    org_html((const ORG_CHAR*) data, (ORG_SIZE) size, process_output, NULL, parser_flags, renderer_flags);
    return 0;
}
