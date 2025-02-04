// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "gfx_es2/glsl_program.h"

#include "UIShader.h"

static GLSLProgram *glslModulate;
static GLSLProgram *glslPlain;

static const char modulate_fs[] =
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"#endif\n"
	"uniform sampler2D sampler0;\n"
	"varying vec2 v_texcoord0;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_FragColor = texture2D(sampler0, v_texcoord0) * v_color;\n"
	"}\n";

static const char modulate_vs[] =
	"attribute vec4 a_position;\n"
	"attribute vec4 a_color;\n"
	"attribute vec2 a_texcoord0;\n"
	"uniform mat4 u_worldviewproj;\n"
	"varying vec2 v_texcoord0;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  v_texcoord0 = a_texcoord0;\n"
	"  v_color = a_color;\n"
	"  gl_Position = u_worldviewproj * a_position;\n"
	"}\n";

static const char plain_fs[] =
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"#endif\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_FragColor = v_color;\n"
	"}\n";

static const char plain_vs[] =
	"attribute vec4 a_position;\n"
	"attribute vec4 a_color;\n"
	"uniform mat4 u_worldviewproj;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  v_color = a_color;\n"
	"  gl_Position = u_worldviewproj * a_position;\n"
	"}\n";

void UIShader_Init() {
	// Compile UI shaders
	glslModulate = glsl_create_source(modulate_vs, modulate_fs);
	glslPlain = glsl_create_source(plain_vs, plain_fs);
}

GLSLProgram *UIShader_Get()
{
	return glslModulate;
}

void UIShader_Shutdown()
{
	glsl_destroy(glslModulate);
	glsl_destroy(glslPlain);
}

void UIShader_Prepare()
{
	// Draw 2D overlay stuff
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);

	uiTexture->Bind(0);
}
