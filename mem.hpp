#pragma once
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <vector>
#include <array>

namespace mem
{
	template<size_t size>
	class ByteArray
	{
	public:
		template<typename T>
		T get(ptrdiff_t offset)
		{
			return *reinterpret_cast<T*>(uintptr_t(m_bytes.data()) + offset);
		}

		template<typename T>
		void set(ptrdiff_t offset, T value)
		{
			*reinterpret_cast<T*>(uintptr_t(m_bytes.data()) + offset) = value;
		}

	private:
		std::array<uint8_t, size> m_bytes;
	};

	struct ProcessMemoryModule
	{
		ProcessMemoryModule(const char* name, uintptr_t base) :
			m_base(base)
		{
			strcpy_s(m_name, name);
		}

		char m_name[MAX_PATH];
		uintptr_t m_base;
	};

	class ProcessMemoryManager
	{
	public:
		ProcessMemoryManager() :
			m_proc(nullptr)
		{ }

		~ProcessMemoryManager()
		{
			if (m_proc)
				CloseHandle(m_proc);
		}

		bool initialize(const char* process_name)
		{
			auto processes = std::array<DWORD, 1024>();
			auto needed = DWORD(0);
			if (K32EnumProcesses(processes.data(), processes.size(), &needed))
			{
				for (auto i = 0u; i < needed / sizeof(DWORD); i++)
				{
					if (!processes[i])
						continue;

					const auto proc_id = processes[i];
					auto proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, proc_id);
					if (proc)
					{
						auto mods = std::array<HMODULE, 1024>();
						auto mod_needed = DWORD(0);
						if (K32EnumProcessModules(proc, mods.data(), mods.size(), &mod_needed))
						{
							auto mod_name = std::array<char, MAX_PATH>();
							K32GetModuleBaseNameA(proc, mods[0], mod_name.data(), mod_name.size());
							if (!strcmp(mod_name.data(), process_name))
							{
								m_proc = proc;

								for (auto j = 0u; j < needed / sizeof(DWORD); j++)
								{
									if (!mods[j])
										continue;

									K32GetModuleBaseNameA(proc, mods[j], mod_name.data(), mod_name.size());
									m_modules.emplace_back(ProcessMemoryModule(mod_name.data(), reinterpret_cast<uintptr_t>(mods[j])));
								}

								return true;
							}
						}
					}

					CloseHandle(proc);
				}
			}

			return false;
		}

		template<size_t size>
		void byte_patch(uintptr_t address, const std::array<uint8_t, size>& patch, bool make_backup = false, std::array<uint8_t, size>* backup = nullptr)
		{
			if(make_backup)
				ReadProcessMemory(m_proc, reinterpret_cast<void*>(address), backup->data(), size, nullptr);

			WriteProcessMemory(m_proc, reinterpret_cast<void*>(address), patch.data(), size, nullptr);
		}

		// read methods
		template<typename T>
		T read(uintptr_t address)
		{
			T buffer;
			ReadProcessMemory(m_proc, reinterpret_cast<void*>(address), &buffer, sizeof(T), nullptr);
			return buffer;
		}

		template<typename T>
		T read(const char* module_name, ptrdiff_t rva)
		{
			T buffer;
			auto index = get_module_index(module_name);
			ReadProcessMemory(m_proc, reinterpret_cast<void*>(m_modules[index].m_base + rva), &buffer, sizeof(T), nullptr);
			return buffer;
		}

		template<typename T>
		T read(size_t module_index, ptrdiff_t rva)
		{
			T buffer;
			ReadProcessMemory(m_proc, reinterpret_cast<void*>(m_modules[module_index].m_base + rva), &buffer, sizeof(T), nullptr);
			return buffer;
		}

		// write methods
		template<typename T>
		void write(uintptr_t address, T value)
		{
			WriteProcessMemory(m_proc, reinterpret_cast<void*>(address), &value, sizeof(T), nullptr);
		}

		template<typename T>
		void write(const char* module_name, ptrdiff_t rva, T value)
		{
			auto index = get_module_index(module_name);
			WriteProcessMemory(m_proc, reinterpret_cast<void*>(m_modules[index].m_base + rva), &value, sizeof(T), nullptr);
		}

		template<typename T>
		void write(size_t module_index, ptrdiff_t rva, T value)
		{
			WriteProcessMemory(m_proc, reinterpret_cast<void*>(m_modules[module_index].m_base + rva), &value, sizeof(T), nullptr);
		}

		uint8_t* find_pattern(const char* pattern, uint8_t* scan_start = nullptr, bool relative = false, ptrdiff_t relative_offset = 0x0)
		{
			// DarthTon's pattern scan
			static auto search = [](const char* pattern, uint8_t* scan_start, size_t scan_size) -> uint8_t*
			{
				const auto scan_end = scan_start + scan_size;
				auto res = std::search(scan_start, scan_end, pattern, pattern + strlen(pattern), [](uint8_t val1, uint8_t val2) -> bool
				{
					return (val1 == val2 || val2 == 0xCC);
				});

				return (res >= scan_end) ? nullptr : res;
			};

			auto sys_info = SYSTEM_INFO{ 0 };
			GetSystemInfo(&sys_info);
			auto start_addr = std::max(static_cast<void*>(sys_info.lpMinimumApplicationAddress), static_cast<void*>(scan_start));

			auto mbi = MEMORY_BASIC_INFORMATION{ 0 };
			while (VirtualQueryEx(m_proc, start_addr, &mbi, sizeof(mbi)))
			{
				if ((mbi.State & MEM_COMMIT) && (mbi.Protect & PAGE_EXECUTE_READWRITE))
				{
					auto region = new uint8_t[mbi.RegionSize];
					auto bytes_read = size_t(0);
					ReadProcessMemory(m_proc, mbi.BaseAddress, region, mbi.RegionSize, &bytes_read);
					auto result = search(pattern, region, mbi.RegionSize);
					if (result)
					{
						auto offset = result - region;
						if (!relative)
						{
							delete[] region;
							return reinterpret_cast<uint8_t*>(uintptr_t(mbi.BaseAddress) + offset);
						}

						offset += relative_offset;
						ReadProcessMemory(m_proc, reinterpret_cast<void*>(uintptr_t(mbi.BaseAddress) + offset), &result, sizeof(uint8_t*), &bytes_read);
						delete[] region;
						return result;
					}
					
					delete[] region;
				}

				start_addr = reinterpret_cast<void*>(uintptr_t(start_addr) + mbi.RegionSize);
			}

			return nullptr;
		}

		uint8_t* find_pattern(size_t module_index, const char* pattern, bool relative = false, ptrdiff_t relative_offset = 0x0)
		{
			return find_pattern(pattern, reinterpret_cast<uint8_t*>(m_modules[module_index].m_base), relative, relative_offset);
		}

	private:
		size_t get_module_index(const char* module_name)
		{
			auto i = std::find_if(m_modules.begin(), m_modules.end(), [&](const ProcessMemoryModule& m)
			{
				return !strcmp(m.m_name, module_name);
			});

			if (i != m_modules.end())
				return i - m_modules.begin();

			return 0;
		}

	private:
		HANDLE m_proc;
		std::vector<ProcessMemoryModule> m_modules;
	};
}