/* Compiles the stb implementations exactly once, as C.
   Everything else in the project includes only the headers. */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
