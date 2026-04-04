#pragma once
#include <vector>

class SdlLibrary {
public:
	SdlLibrary();

	SdlLibrary(SdlLibrary const&) = delete;
	SdlLibrary& operator=(SdlLibrary const&) = delete;
	SdlLibrary(SdlLibrary&&) = delete;
	SdlLibrary& operator=(SdlLibrary&&) = delete;

	void setRelativeMouseMode(bool enabled);

	[[nodiscard]] static std::vector<char const*> getSurfaceRequiredExtensions();
	void postQuitEvent();

	bool pullEvents();

	~SdlLibrary();
};