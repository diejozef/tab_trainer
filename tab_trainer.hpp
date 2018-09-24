#pragma once
#include "mem.hpp"
#include "declarations.hpp"

namespace tab_trainer
{
	class feature
	{
	public:
		explicit feature(const char* name, const char* sig, mem::ProcessMemoryManager* mem, uint8_t key) :
			m_name(name),
			m_sig(sig),
			m_mem(mem),
			m_key(key)
		{ }

		virtual void on_key_down(uint8_t vk)  { }

	protected:
		const char* m_name;
		const char* m_sig;
		mem::ProcessMemoryManager* m_mem;
		uint8_t m_key;
	};

	// todo: this is too static?
	template<size_t size>
	class feature_set_value : public feature
	{
	public:
		explicit feature_set_value() : 
			feature(nullptr, nullptr, nullptr, 0),
			m_base(0),
			m_relative_offset(0)
		{ }

		explicit feature_set_value(const char* name, 
			const char* sig, 
			mem::ProcessMemoryManager* mem, 
			uint8_t key, 
			const std::vector<ptrdiff_t>& offsets_to_structure,
			const std::vector<ptrdiff_t>& offsets, 
			const std::vector<int>& mods, 
			ptrdiff_t cap_offset,
			ptrdiff_t sig_relative_offset) :
			feature(name, sig, mem, key),
			m_base(0),
			m_relative_offset(sig_relative_offset),
			m_offsets_to_structure(offsets_to_structure),
			m_offsets(offsets),
			m_modifiers(mods),
			m_cap_offset(cap_offset)
		{ }

		void on_key_down(uint8_t vk)
		{
			if (vk != m_key)
				return;

			if (!m_base)
			{
				m_base = get_nbytes<uint32_t>(m_mem->find_pattern(m_sig, reinterpret_cast<uint8_t*>(0x7FF00000000), true, m_relative_offset), 0);
				if (!m_base)
				{
					printf("[*] can't do that yet!\n");
					return;
				}
			}
			
			auto address = m_base;
			for (const auto& i : m_offsets_to_structure)
				address = m_mem->read<uint32_t>(address + i);

			m_bytearray = m_mem->read<mem::ByteArray<size>>(address);
			const auto cap = m_bytearray.get<int>(m_cap_offset);

			for (const auto& i : m_offsets)
				m_bytearray.set(i, cap * m_modifiers[&i - &m_offsets[0]]);

			m_mem->write(address, m_bytearray);
			printf("[*] %s set!\n", m_name);
		}

	private:
		uint32_t m_base;
		ptrdiff_t m_relative_offset;
		std::vector<ptrdiff_t> m_offsets_to_structure;
		std::vector<ptrdiff_t> m_offsets;
		std::vector<int> m_modifiers;
		ptrdiff_t m_cap_offset;
		mem::ByteArray<size> m_bytearray;
	};

	// bytepatched function
	template<size_t patch_size>
	class feature_func_bp : public feature
	{
	public:
		explicit feature_func_bp() :
			feature(nullptr, nullptr, nullptr, 0),
			m_enable(false),
			m_fn_address(0)
		{ }

		explicit feature_func_bp(const char* name, const char* sig, mem::ProcessMemoryManager* mem, uint8_t key, const std::array<uint8_t, patch_size>& patch) :
			feature(name, sig, mem, key),
			m_enable(false),
			m_fn_address(0),
			m_patch(patch)
		{ }

		void on_key_down(uint8_t vk)
		{
			if (vk != m_key)
				return;

			if (!m_fn_address)
			{
				m_fn_address = reinterpret_cast<uintptr_t>(m_mem->find_pattern(m_sig, reinterpret_cast<uint8_t*>(0x7FF00000000)));
				if (!m_fn_address)
				{
					printf("[*] can't do that yet!\n");
					return;
				}
			}

			TOGGLE(m_enable);
			if (m_enable)
				m_mem->byte_patch(m_fn_address, m_patch, true, &m_backup);
			else
				m_mem->byte_patch(m_fn_address, m_backup);

			printf("[*] %s: %s\n", m_name, m_enable ? "on" : "off");
		}

		void on_exit()
		{
			// remove patch on app exit
			if (m_enable)
				m_mem->byte_patch(m_fn_address, m_backup);
		}

	private:
		bool m_enable;
		uintptr_t m_fn_address;
		std::array<uint8_t, patch_size> m_patch;
		std::array<uint8_t, patch_size> m_backup;
	};

	extern feature_func_bp<6> veterans;
	extern feature_func_bp<12> fast_building;
	extern feature_func_bp<8> fast_research;
	extern feature_set_value<0x200> resources;
}