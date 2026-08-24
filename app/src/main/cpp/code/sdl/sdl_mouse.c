
#ifdef USE_INTERNAL_SDL_HEADERS
#    include "SDL.h"
#else
#    include <SDL.h>
#endif

#include "../qcommon/qcommon.h"

static SDL_Cursor *cursor = NULL;
static SDL_Surface *cursor_surface = NULL;

void IN_GetMousePosition(int *x, int *y) {
    SDL_GetMouseState(x, y);
}

void IN_FreeCursor(void) {
    if (cursor) {
        SDL_FreeCursor(cursor);
    }
    if (cursor_surface) {
        SDL_FreeSurface(cursor_surface);
    }
}

qboolean IN_IsCursorActive(void)
{
    return SDL_GetRelativeMouseMode() == SDL_TRUE;
}
