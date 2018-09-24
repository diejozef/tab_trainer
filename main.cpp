#include "declarations.hpp"
#include "mem.hpp"
#include "tab_trainer.hpp"

using ZXLevelState = mem::ByteArray<0x200>;

constexpr ptrdiff_t offset_wood = 0xE8;
constexpr ptrdiff_t offset_stone = offset_wood + (1 * 0x4);
constexpr ptrdiff_t offset_iron = offset_wood + (2 * 0x4);
constexpr ptrdiff_t offset_oil = offset_wood + (3 * 0x4);
constexpr ptrdiff_t offset_gold = offset_wood + (4 * 0x4);
constexpr ptrdiff_t offset_storage = 0x100;
constexpr ptrdiff_t offset_showmap = 0x196;

LRESULT CALLBACK kb_hook(int code, WPARAM wparam, LPARAM lparam)
{
	if (wparam == WM_KEYDOWN)
	{
		const auto kb = reinterpret_cast<PKBDLLHOOKSTRUCT>(lparam);
		const auto vk = static_cast<uint8_t>(kb->vkCode);

		// handle input
		tab_trainer::veterans.on_key_down(vk);
		tab_trainer::fast_building.on_key_down(vk);
		tab_trainer::fast_research.on_key_down(vk);
		tab_trainer::resources.on_key_down(vk);
	}

	return CallNextHookEx(nullptr, code, wparam, lparam);
}

BOOL CALLBACK con_handler(DWORD ctrl_type)
{
	if (ctrl_type == CTRL_CLOSE_EVENT)
	{
		// handle app exit(restore patches)
		tab_trainer::veterans.on_exit();
		tab_trainer::fast_building.on_exit();
		tab_trainer::fast_research.on_exit();
		return TRUE;
	}

	return FALSE;
}

int main(void)
{
	auto mem = mem::ProcessMemoryManager();
	if (!mem.initialize("TheyAreBillions.exe"))
	{
		printf("[*] couldn't initialize memory manager(process is not running?).\n");
		getchar();
		return 0;
	}

	printf("[*] [F1] cap resources\n");
	printf("[*] [F2] veteran training\n");
	printf("[*] [F3] fast building / training / repair\n");
	printf("[*] [F4] fast research\n");

	tab_trainer::resources = tab_trainer::feature_set_value<0x200>(
		"resources",
		"\x48\x8B\x14\x25\xCC\xCC\xCC\xCC\x48\x85\xD2\x75\x03",
		&mem,
		VK_F1,
		{ 0x0, 0x18 },
		{ offset_gold, offset_wood, offset_stone, offset_iron, offset_oil },
		{ 40, 1, 1, 1, 1 },
		offset_storage,
		0x4);

	// mov al, 01
	// ret
	auto veterans_patch = std::array<uint8_t, 6>{
		0xB0, 0x01,		// mov al, 01
		0xC3,			// ret
		0x90,			// nop
		0x90,			// nop
		0x90			// nop
	};
	tab_trainer::veterans = tab_trainer::feature_func_bp(
		"veteran training", 
		"\x57\x56\x48\x83\xEC\x28\x48\x8B\x0C\x25\xCC\xCC\xCC\xCC\x48\xBA", 
		&mem, 
		VK_F2, 
		veterans_patch);

	// mov eax, 01
	// ret
	auto fast_building_patch = std::array<uint8_t, 12>{
		0xB8, 0x01, 0x00, 0x00, 0x00,	// mov eax, 01
		0xC3,							// ret
		0x90,							// nop
		0x90,							// nop
		0x90,							// nop
		0x90,							// nop
		0x90,							// nop
		0x90							// nop
	};
	tab_trainer::fast_building = tab_trainer::feature_func_bp(
		"fast building", 
		"\x48\x83\xEC\x28\xF3\x0F\x10\x81\x38\x01\x00\x00", 
		&mem, 
		VK_F3,
		fast_building_patch);

	// mov eax, 01
	// ret
	auto fast_research_patch = std::array<uint8_t, 8>{
		0xB8, 0x01, 0x00, 0x00, 0x00,	// mov eax, 01
		0xC3,							// ret
		0x90,							// nop
		0x90							// nop
	};

	tab_trainer::fast_research = tab_trainer::feature_func_bp(
		"fast research",
		"\x48\x83\xEC\x28\x48\x8B\x41\x08\xF3\x0F\x10\x80",
		&mem,
		VK_F4,
		fast_research_patch
	);

	SetConsoleCtrlHandler(con_handler, TRUE);
	SetWindowsHookEx(WH_KEYBOARD_LL, kb_hook, GetModuleHandleA(nullptr), 0);
	while (true)
	{
		MSG msg = { 0 };
		if (GetMessage(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}