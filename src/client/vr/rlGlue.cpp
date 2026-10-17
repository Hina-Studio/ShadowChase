#include "rlGlue.hpp"

#include <raylib.h>
#include <rlgl.h>

namespace slashco_vr {

unsigned int glueLoadFramebuffer() {
    return rlLoadFramebuffer();
}

unsigned int glueLoadDepthTexture(int w, int h) {
    return rlLoadTextureDepth(w, h, true);
}

void glueFramebufferAttach(unsigned int fbo, unsigned int tex, int attachType, int texType,
                           int mip) {
    rlFramebufferAttach(fbo, tex, attachType, texType, mip);
}

bool glueFramebufferComplete(unsigned int fbo) {
    return rlFramebufferComplete(fbo);
}

void glueBindFramebuffer(unsigned int fbo) {
    rlBindFramebuffer(0x8D40u, fbo);
}

void glueUnloadFramebuffer(unsigned int fbo) {
    rlUnloadFramebuffer(fbo);
}

void glueUnloadTexture(unsigned int id) {
    rlUnloadTexture(id);
}

void glueViewport(int x, int y, int w, int h) {
    rlViewport(x, y, w, h);
}

void glueClearColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    rlClearColor(r, g, b, a);
}

void glueClearBuffers() {
    rlClearScreenBuffers();
}

void glueFlushBatch() {
    rlDrawRenderBatchActive();
}

int glueScreenWidth() {
    return GetScreenWidth();
}

int glueScreenHeight() {
    return GetScreenHeight();
}

}  // namespace slashco_vr
