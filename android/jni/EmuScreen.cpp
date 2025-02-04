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

#include "input/input_state.h"
#include "ui/ui.h"

#include "../../Core/Config.h"
#include "../../Core/CoreTiming.h"
#include "../../Core/CoreParameter.h"
#include "../../Core/Core.h"
#include "../../Core/Host.h"
#include "../../Core/System.h"
#include "../../Core/MIPS/MIPS.h"
#include "../../GPU/GLES/TextureCache.h"
#include "../../GPU/GLES/ShaderManager.h"
#include "../../GPU/GPUState.h"
#include "../../Core/HLE/sceCtrl.h"

#include "GamepadEmu.h"
#include "UIShader.h"

#include "MenuScreens.h"
#include "EmuScreen.h"

extern ShaderManager shaderManager;

EmuScreen::EmuScreen(const std::string &filename) : invalid_(true)
{
	std::string fileToStart = filename;
	// This is probably where we should start up the emulated PSP.
	INFO_LOG(BOOT, "Starting up hardware.");

	CoreParameter coreParam;
	coreParam.cpuCore = CPU_INTERPRETER;
	coreParam.gpuCore = GPU_GLES;
	coreParam.enableSound = g_Config.bEnableSound;
	coreParam.fileToStart = fileToStart;
	coreParam.mountIso = "";
	coreParam.startPaused = false;
	coreParam.enableDebugging = false;
	coreParam.printfEmuLog = false;
	coreParam.headLess = false;

	std::string error_string;
	if (PSP_Init(coreParam, &error_string)) {
		invalid_ = false;
	} else {
		invalid_ = true;
		errorMessage_ = error_string;
		ERROR_LOG(BOOT, "%s", errorMessage_.c_str());
		return;
	}

	LayoutGamepad(dp_xres, dp_yres);

	NOTICE_LOG(BOOT, "Loading %s...", fileToStart.c_str());
	coreState = CORE_RUNNING;
}

EmuScreen::~EmuScreen()
{
	if (!invalid_) {
		// If we were invalid, it would already be shutdown.
		host->PrepareShutdown();
		PSP_Shutdown();
	}
}

void EmuScreen::dialogFinished(const Screen *dialog, DialogResult result) {
	if (result == DR_OK) {
		screenManager()->switchScreen(new MenuScreen());
	}
}

void EmuScreen::update(InputState &input)
{
	if (errorMessage_.size()) {
		screenManager()->push(new ErrorScreen(
			"Error loading file",
			errorMessage_));
		errorMessage_ = "";
		return;
	}

	if (invalid_)
		return;
	// First translate touches into pad input.
	UpdateGamepad(input);
	UpdateInputState(&input);

	// Then translate pad input into PSP pad input.

	static const int mapping[12][2] = {
		{PAD_BUTTON_A, CTRL_CROSS},
		{PAD_BUTTON_B, CTRL_SQUARE},
		{PAD_BUTTON_X, CTRL_CIRCLE},
		{PAD_BUTTON_Y, CTRL_TRIANGLE},
		{PAD_BUTTON_UP, CTRL_UP},
		{PAD_BUTTON_DOWN, CTRL_DOWN},
		{PAD_BUTTON_LEFT, CTRL_LEFT},
		{PAD_BUTTON_RIGHT, CTRL_RIGHT},
		{PAD_BUTTON_LBUMPER, CTRL_LTRIGGER},
		{PAD_BUTTON_RBUMPER, CTRL_RTRIGGER},
		{PAD_BUTTON_START, CTRL_START},
		{PAD_BUTTON_BACK, CTRL_SELECT},
	};

	for (int i = 0; i < 12; i++) {
		if (input.pad_buttons_down & mapping[i][0])
			__CtrlButtonDown(mapping[i][1]);
		if (input.pad_buttons_up & mapping[i][0])
			__CtrlButtonUp(mapping[i][1]);
	}

	if (input.pad_buttons_down & (PAD_BUTTON_MENU | PAD_BUTTON_BACK)) {
		screenManager()->push(new InGameMenuScreen());
	}
}

void EmuScreen::render()
{
	if (invalid_)
		return;

	// First attempt at an Android-friendly execution loop.
	// We simply run the CPU for 1/60th of a second each frame. If a swap doesn't happen, not sure what the best thing to do is :P
	// Also if we happen to get half a frame or something, things will be screwed up so this doesn't actually really work.
	//
	// I think we need to allocate FBOs per framebuffer and just blit the displayed one here at the end of the frame.
	// Also - we should add another option to the core that lets us run it until a vblank event happens or the N cycles have passed
	// - then the synchronization would at least not be able to drift off.
	u64 nowTicks = CoreTiming::GetTicks();
	u64 frameTicks = usToCycles(1000000 / 60);
	mipsr4k.RunLoopUntil(nowTicks + frameTicks);  // should really be relative to the last frame but whatever

	//if (hasRendered)
	{
		UIShader_Prepare();

		uiTexture->Bind(0);

		ui_draw2d.Begin(DBMODE_NORMAL);

		// Don't want the gamepad on MacOSX and Linux
// #ifdef ANDROID
		DrawGamepad(ui_draw2d);
// #endif

		DrawWatermark();

		glsl_bind(UIShader_Get());
		ui_draw2d.End();
		ui_draw2d.Flush(UIShader_Get());

		//hasRendered = false;

		// Reapply the graphics state of the PSP
		ReapplyGfxState();
	}

	// Tiled renderers like PowerVR should benefit greatly from this. However - seems I can't call it?
#ifdef ANDROID
	bool hasDiscard = false;  // TODO
	if (hasDiscard) {
		//glDiscardFramebufferEXT(GL_COLOR_EXT | GL_DEPTH_EXT | GL_STENCIL_EXT);
	}
#endif
}

void EmuScreen::deviceLost()
{
	TextureCache_Clear(false);  // This doesn't seem to help?
	shaderManager.ClearCache(false);
}
