#pragma once
#include <vector>

class SdlLibrary {
public:
	SdlLibrary();

	SdlLibrary(SdlLibrary const&) = delete;
	SdlLibrary& operator=(SdlLibrary const&) = delete;
	SdlLibrary(SdlLibrary&&) = delete;
	SdlLibrary& operator=(SdlLibrary&&) = delete;


	[[nodiscard]] static std::vector<char const*> getSurfaceRequiredExtensions();
	void postQuitEvent();

	bool pullEvents();

	~SdlLibrary();
};