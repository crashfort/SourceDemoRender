#pragma once

namespace SDR::EngineClient
{
	void* GetPtr();

	bool IsConsoleVisible();
	void FlashWindow();
	void ClientCommand(const char* str);
}
