#include "plyEngine/PresentPass.hpp"

#include <glad/gl.h>

#include <cstdio>

namespace ply {

PresentPass::PresentPass(const RenderTargetPlan& plan) {
    setPlan(plan);
}

PresentPass::~PresentPass() {
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
    }
    if (texture_) {
        glDeleteTextures(1, &texture_);
    }
}

void PresentPass::setPlan(const RenderTargetPlan& plan) {
    if (plan.internalWidth <= 0 || plan.internalHeight <= 0) {
        return;
    }
    if (plan.internalWidth == width_ && plan.internalHeight == height_) {
        return;
    }
    allocate(plan.internalWidth, plan.internalHeight);
}

void PresentPass::allocate(int width, int height) {
    // Drop the old surface first: a failed re-allocation must not leave a
    // half-sized texture attached to a complete-sized name.
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (texture_) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    width_ = width;
    height_ = height;

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    // RGBA8, byte-for-byte (see Texture.hpp's colour-space note): the world
    // texture is pixel art at internal resolution, sampled only by the present
    // blit, and the readback compares bytes.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    complete_ = status == GL_FRAMEBUFFER_COMPLETE;
    if (!complete_) {
        std::fprintf(stderr, "plyEngine: present target incomplete (0x%x)\n", status);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PresentPass::begin(ClearColor clear) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(clear.r, clear.g, clear.b, clear.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void PresentPass::end() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PresentPass::present(const RenderTargetPlan& plan, ClearColor letterbox) {
    if (!complete_) {
        return;
    }
    const int outW = plan.outputWidth;
    const int outH = plan.outputHeight;
    if (outW <= 0 || outH <= 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, outW, outH);
    glClearColor(letterbox.r, letterbox.g, letterbox.b, letterbox.a);
    glClear(GL_COLOR_BUFFER_BIT);

    const int k = plan.factor < 1 ? 1 : plan.factor;
    // Clip the destination to the backbuffer, in top-left output px. Only the
    // factor-1 case can clip (makeRenderTargetPlan never centres an image
    // wider than its window above factor 1), but the math is written for any
    // factor and snapped outward to the factor grid so the blitted region
    // keeps exact k x k blocks: the clipped edge shows letterbox colour, never
    // a partially covered pixel.
    const int destR = plan.destination.x + plan.destination.w;
    const int destB = plan.destination.y + plan.destination.h;
    const int clipL = plan.destination.x < 0 ? -plan.destination.x : 0;
    const int clipT = plan.destination.y < 0 ? -plan.destination.y : 0;
    const int clipR = destR > outW ? destR - outW : 0;
    const int clipB = destB > outH ? destB - outH : 0;
    if (clipL + clipR >= plan.destination.w || clipT + clipB >= plan.destination.h) {
        return;  // nothing of the internal surface is visible
    }
    const int sxL = (clipL + k - 1) / k;  // internal px clipped on the left (ceil)
    const int sxT = (clipT + k - 1) / k;
    const int sxR = (clipR + k - 1) / k;
    const int sxB = (clipB + k - 1) / k;

    // Source and destination, still in top-left px. The destination stays on
    // the factor grid relative to plan.destination and the source keeps the
    // exact k:1 ratio, so nearest sampling reproduces whole texel blocks.
    const int sx0 = sxL;
    const int sy0 = sxT;
    const int sx1 = width_ - sxR;
    const int sy1 = height_ - sxB;
    const int dx0 = plan.destination.x + sxL * k;
    const int dy0 = plan.destination.y + sxT * k;
    const int dx1 = destR - sxR * k;
    const int dy1 = destB - sxB * k;

    // GL rectangles are bottom-up; the texture's bottom row is the internal
    // surface's bottom, and it lands at the destination's bottom edge.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
    glBlitFramebuffer(sx0, height_ - sy1, sx1, height_ - sy0, dx0, outH - dy1, dx1, outH - dy0,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

}  // namespace ply
