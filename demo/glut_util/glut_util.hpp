/*  Berkelium GLUT Embedding
 *  glut_util.hpp
 *
 *  Copyright (c) 2010, Ewen Cheslack-Postava
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _BERKELIUM_GLUT_UTIL_HPP_
#define _BERKELIUM_GLUT_UTIL_HPP_


/* This file contains a set of utility methods useful when embedding Berkelium
 * into a GLUT application. The GLUT demos are mostly just thin wrappers around
 * these methods.
 */

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#if defined(GL_BGRA_EXT) && !defined(GL_BGRA)
#define GL_BGRA GL_BGRA_EXT
#endif

#include "berkelium/Berkelium.hpp"
#include "berkelium/Window.hpp"

#include <cstring>

/** Handles an onPaint call by mapping the results into an OpenGL texture. The
 *  first parameters are the same as Berkelium::WindowDelegate::onPaint.  The
 *  additional parameters and return value are:
 *  \param dest_texture - the OpenGL texture handle for the texture to render
 *                        the results into.
 *  \param dest_texture_width - width of destination texture
 *  \param dest_texture_height - height of destination texture
 *  \param ignore_partial - if true, ignore any partial updates.  This is useful
 *         if you have loaded a new page, but updates for the old page have not
 *         completed yet.
 *  \param scroll_buffer - a temporary workspace used for scroll data.  Must be
 *         at least dest_texture_width * dest_texture_height * 4 bytes large.
 *  \returns true if the texture was updated, false otherwiase
 */
bool mapOnPaintToTexture(
    Berkelium::Window *wini,
    const unsigned char* bitmap_in, const Berkelium::Rect& bitmap_rect,
    int dx, int dy,
    const Berkelium::Rect& scroll_rect,
    unsigned int dest_texture,
    unsigned int dest_texture_width,
    unsigned int dest_texture_height,
    bool ignore_partial,
    char* scroll_buffer) {

    glBindTexture(GL_TEXTURE_2D, dest_texture);

    // If we've reloaded the page and need a full update, ignore updates
    // until a full one comes in.  This handles out of date updates due to
    // delays in event processing.
    if (ignore_partial) {
        if (bitmap_rect.left() != 0 ||
            bitmap_rect.top() != 0 ||
            bitmap_rect.right() != dest_texture_width ||
            bitmap_rect.bottom() != dest_texture_height) {
            return false;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, 3, dest_texture_width, dest_texture_height, 0,
            GL_BGRA, GL_UNSIGNED_BYTE, bitmap_in);
        ignore_partial = false;
        return true;
    }


    // Now, we first handle scrolling. We need to do this first since it
    // requires shifting existing data, some of which will be overwritten by
    // the regular dirty rect update.
    if (dx != 0 || dy != 0) {
        // scroll_rect contains the Rect we need to move
        // First we figure out where the the data is moved to by translating it
        Berkelium::Rect scrolled_rect = scroll_rect.translate(-dx, -dy);
        // Next we figure out where they intersect, giving the scrolled
        // region
        Berkelium::Rect scrolled_shared_rect = scroll_rect.intersect(scrolled_rect);
        // Only do scrolling if they have non-zero intersection
        if (scrolled_shared_rect.width() > 0 && scrolled_shared_rect.height() > 0) {
            // And the scroll is performed by moving shared_rect by (dx,dy)
            Berkelium::Rect shared_rect = scrolled_shared_rect.translate(dx, dy);

            // Copy the data out of the texture
            glGetTexImage(
                GL_TEXTURE_2D, 0,
                GL_BGRA, GL_UNSIGNED_BYTE,
                scroll_buffer
            );

            // Annoyingly, OpenGL doesn't provide convenient primitives, so
            // we manually copy out the region to the beginning of the
            // buffer
            int wid = scrolled_shared_rect.width();
            int hig = scrolled_shared_rect.height();
            for(int jj = 0; jj < hig; jj++) {
                memcpy(
                    scroll_buffer + (jj*wid * 4),
                    scroll_buffer + ((scrolled_shared_rect.top()+jj)*dest_texture_width + scrolled_shared_rect.left()) * 4,
                    wid*4
                );
            }

            // And finally, we push it back into the texture in the right
            // location
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                shared_rect.left(), shared_rect.top(),
                shared_rect.width(), shared_rect.height(),
                GL_BGRA, GL_UNSIGNED_BYTE, scroll_buffer
            );
        }
    }

    // Finally, we perform the main update, just copying the rect that is
    // marked as dirty but not from scrolled data.
    glTexSubImage2D(GL_TEXTURE_2D, 0,
        bitmap_rect.left(), bitmap_rect.top(),
        bitmap_rect.width(), bitmap_rect.height(),
        GL_BGRA, GL_UNSIGNED_BYTE, bitmap_in
    );

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}


/** Maps an input coordinate to a texture coordinate for injection into
 *  Berkelium.
 *  \param glut_size the size of the GLUT window
 *  \param glut_coord the coordinate value received from GLUT
 *  \param tex_size the size of the texture/Berkelium window
 *  \returns the coordinate transformed to the correct value for the texture /
 *           Berkelium window
 */
unsigned int mapGLUTCoordToTexCoord(
    unsigned int glut_coord, unsigned int glut_size,
    unsigned int tex_size) {

    return (glut_coord * tex_size) / glut_size;
}

/** Given modifiers retrieved from GLUT (e.g. glutGetModifiers), convert to a
 *  form that can be passed to Berkelium.
 */
int mapGLUTModsToBerkeliumMods(int modifiers) {
    int wvmods = 0;

    if (modifiers & GLUT_ACTIVE_SHIFT)
        wvmods |= Berkelium::SHIFT_MOD;
    if (modifiers & GLUT_ACTIVE_CTRL)
        wvmods |= Berkelium::CONTROL_MOD;
    if (modifiers & GLUT_ACTIVE_ALT)
        wvmods |= Berkelium::ALT_MOD;

    // Note: GLUT doesn't expose Berkelium::META_MOD

    return wvmods;
}

/** Returns true if the ASCII value is considered a special input to Berkelium
 *  which cannot be handled directly via the textEvent method and must be
 *  handled using keyEvent instead.
 */
unsigned int isASCIISpecialToBerkelium(unsigned char glut_char) {
    unsigned char ASCII_BACKSPACE = 8;
    unsigned char ASCII_TAB       = 9;
    unsigned char ASCII_ESCAPE    = 27;
    unsigned char ASCII_DELETE    = 127;

    return (glut_char == ASCII_BACKSPACE ||
        glut_char == ASCII_TAB ||
        glut_char == ASCII_ESCAPE ||
        glut_char == ASCII_DELETE
    );
}

// A few of the most useful keycodes handled below.
enum VirtKeys {
BK_KEYCODE_PRIOR = 0x21,
BK_KEYCODE_NEXT = 0x22,
BK_KEYCODE_END = 0x23,
BK_KEYCODE_HOME = 0x24,
BK_KEYCODE_INSERT = 0x2D,
};

/** Given an input key from GLUT, convert it to a form that can be passed to
 *  Berkelium.
 */
unsigned int mapGLUTKeyToBerkeliumKey(int glut_key) {
    switch(glut_key) {
#define MAP_VK(X, Y) case GLUT_KEY_##X: return BK_KEYCODE_##Y;
        MAP_VK(INSERT, INSERT);
        MAP_VK(HOME, HOME);
        MAP_VK(END, END);
        MAP_VK(PAGE_UP, PRIOR);
        MAP_VK(PAGE_DOWN, NEXT);
      default: return 0;
    }
}

/** GLTextureWindow handles rendering a window into a GL texture.  Unlike the
 *  utility methods, this takes care of the entire process and cleanup.
 */
class GLTextureWindow : public Berkelium::WindowDelegate {
public:
    GLTextureWindow(unsigned int _w, unsigned int _h)
     : width(_w),
       height(_h),
       needs_full_refresh(true)
    {
        // Create texture to hold rendered view
        glGenTextures(1, &web_texture);
        glBindTexture(GL_TEXTURE_2D, web_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        scroll_buffer = new char[width*height*4];

        bk_window = Berkelium::Window::create();
        bk_window->setDelegate(this);
        bk_window->resize(width, height);
    }

    ~GLTextureWindow() {
        delete scroll_buffer;
        delete bk_window;
    }

    Berkelium::Window* getWindow() {
        return bk_window;
    }

    void clear() {
        // Black out the page
        unsigned char black = 0;
        glBindTexture(GL_TEXTURE_2D, web_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, 3, 1, 1, 0,
            GL_LUMINANCE, GL_UNSIGNED_BYTE, &black);

        needs_full_refresh = true;
    }

    void bind() {
        glBindTexture(GL_TEXTURE_2D, web_texture);
    }

    void release() {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    virtual void onPaint(Berkelium::Window *wini,
        const unsigned char *bitmap_in, const Berkelium::Rect &bitmap_rect,
        int dx, int dy, const Berkelium::Rect &scroll_rect) {

        bool updated = mapOnPaintToTexture(
            wini, bitmap_in, bitmap_rect, dx, dy, scroll_rect,
            web_texture, width, height, needs_full_refresh, scroll_buffer
        );
        if (updated) {
            needs_full_refresh = false;
            glutPostRedisplay();
        }
    }

private:
    // The Berkelium window, i.e. our web page
    Berkelium::Window* bk_window;
    // Width and height of our window.
    unsigned int width, height;
    // Storage for a texture
    unsigned int web_texture;
    // Bool indicating when we need to refresh the entire image
    bool needs_full_refresh;
    // Buffer used to store data for scrolling
    char* scroll_buffer;
};

#endif //_BERKELIUM_GLUT_UTIL_HPP_
