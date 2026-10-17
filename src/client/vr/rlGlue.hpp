#pragma once

#include <cstdint>

namespace slashco_vr {

unsigned int glueLoadFramebuffer();
unsigned int glueLoadDepthTexture(int w, int h);
void glueFramebufferAttach(unsigned int fbo, unsigned int tex, int attachType, int texType,
                           int mip);
bool glueFramebufferComplete(unsigned int fbo);
void glueBindFramebuffer(unsigned int fbo);
void glueUnloadFramebuffer(unsigned int fbo);
void glueUnloadTexture(unsigned int id);
void glueViewport(int x, int y, int w, int h);
void glueClearColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void glueClearBuffers();
void glueFlushBatch();
int glueScreenWidth();
int glueScreenHeight();

}  // namespace slashco_vr
